#include "AudioDeviceScanner.h"
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

AudioDeviceScanner::AudioDeviceScanner(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AudioDeviceScanner::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_devices.size();
}

QVariant AudioDeviceScanner::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_devices.size())
        return QVariant();
    
    const AudioDevice &device = m_devices[index.row()];
    
    switch (role) {
    case DeviceIdRole:
        return device.deviceId;
    case DeviceNameRole:
        return device.deviceName;
    case DescriptionRole:
        return device.description;
    case IsAvailableRole:
        return device.isAvailable;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AudioDeviceScanner::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DeviceIdRole] = "deviceId";
    roles[DeviceNameRole] = "deviceName";
    roles[DescriptionRole] = "description";
    roles[IsAvailableRole] = "isAvailable";
    return roles;
}

void AudioDeviceScanner::scanDevices()
{
    qDebug() << "AudioDeviceScanner: Starting audio device scan...";
    
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit countChanged();
    
    // 使用FFmpeg枚举音频设备
    scanDevicesWithFfmpeg();
    
    if (m_devices.isEmpty()) {
        emit scanError("未找到可用的音频设备");
    } else {
        qDebug() << "AudioDeviceScanner: Found" << m_devices.size() << "audio devices";
    }
    
    emit scanFinished();
}

void AudioDeviceScanner::scanDevicesWithFfmpeg()
{
    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        qWarning() << "AudioDeviceScanner: FFmpeg not found, cannot scan devices";
        return;
    }
    
    QProcess ffmpegProcess;
    
#ifdef Q_OS_WIN
    // Windows使用dshow
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-list_devices" << "true" 
                       << "-f" << "dshow" 
                       << "-i" << "dummy");
#elif defined(Q_OS_LINUX)
    // Linux使用alsa或pulse
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-hide_banner" 
                       << "-f" << "alsa" 
                       << "-list_devices" << "true" 
                       << "-i" << "dummy");
#elif defined(Q_OS_MACOS)
    // macOS使用avfoundation
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-f" << "avfoundation" 
                       << "-list_devices" << "true" 
                       << "-i" << "");
#endif

    if (!ffmpegProcess.waitForStarted(3000)) {
        qWarning() << "AudioDeviceScanner: Failed to start FFmpeg process:" << ffmpegProcess.errorString();
        return;
    }
    
    if (!ffmpegProcess.waitForFinished(5000)) {
        qWarning() << "AudioDeviceScanner: FFmpeg process timeout";
        ffmpegProcess.kill();
        return;
    }
    
    QString output = QString::fromUtf8(ffmpegProcess.readAllStandardError());
    qDebug() << "AudioDeviceScanner: FFmpeg output:" << output;
    
#ifdef Q_OS_WIN
    parseFfmpegDeviceList(output, "dshow");
#elif defined(Q_OS_LINUX)
    parseFfmpegDeviceList(output, "alsa");
#elif defined(Q_OS_MACOS)
    parseFfmpegDeviceList(output, "avfoundation");
#endif
}

void AudioDeviceScanner::parseFfmpegDeviceList(const QString &output, const QString &format)
{
    beginResetModel();
    
    if (format == "dshow") {
        QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        bool inAudioDevicesSection = false;
        
        qDebug() << "AudioDeviceScanner: Parsing DirectShow audio devices, total lines:" << lines.size();
        
        int index = 0;
        for (const QString &line : lines) {
            if (line.contains("DirectShow audio devices")) {
                inAudioDevicesSection = true;
                continue;
            }
            if (line.contains("DirectShow video devices")) {
                // 跳过视频设备部分
                continue;
            }
            if (inAudioDevicesSection) {
                if (line.contains("Alternative name", Qt::CaseInsensitive)) {
                    continue;
                }
                if (line.contains("[dshow") && line.contains("\"") && !line.contains("Alternative", Qt::CaseInsensitive)) {
                    int startQuote = line.indexOf('"');
                    int endQuote = line.indexOf('"', startQuote + 1);
                    
                    if (startQuote >= 0 && endQuote > startQuote) {
                        QString deviceName = line.mid(startQuote + 1, endQuote - startQuote - 1);
                        
                        if (!deviceName.isEmpty()) {
                            qDebug() << "AudioDeviceScanner: Found audio device:" << deviceName;
                            
                            AudioDevice device;
                            device.deviceId = deviceName;
                            device.deviceName = deviceName;
                            device.description = QString("麦克风 %1").arg(index + 1);
                            device.isAvailable = true;
                            
                            m_devices.append(device);
                            index++;
                        }
                    }
                }
            }
        }
    } else if (format == "avfoundation") {
        // macOS avfoundation格式解析
        QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        bool inAudioSection = false;
        
        for (const QString &line : lines) {
            if (line.contains("AVFoundation audio devices")) {
                inAudioSection = true;
                continue;
            }
            if (line.contains("AVFoundation video devices")) {
                inAudioSection = false;
                continue;
            }
            if (inAudioSection && line.contains("[")) {
                QRegularExpression re(R"(\[\s*(\d+)\]\s*(.+))");
                QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    QString deviceId = match.captured(1);
                    QString deviceName = match.captured(2).trimmed();
                    
                    AudioDevice device;
                    device.deviceId = deviceId;
                    device.deviceName = deviceName;
                    device.description = deviceName;
                    device.isAvailable = true;
                    
                    m_devices.append(device);
                }
            }
        }
    } else if (format == "alsa") {
        // Linux alsa格式解析
        QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.contains("card") && line.contains(":")) {
                QRegularExpression re(R"(card\s+(\d+):\s+\[(.+)\],\s+device\s+(\d+):\s+(.+))");
                QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    QString cardId = match.captured(1);
                    QString cardName = match.captured(2);
                    QString deviceId = match.captured(3);
                    QString deviceName = match.captured(4);
                    
                    AudioDevice device;
                    device.deviceId = QString("hw:%1,%2").arg(cardId, deviceId);
                    device.deviceName = QString("%1 - %2").arg(cardName, deviceName);
                    device.description = device.deviceName;
                    device.isAvailable = true;
                    
                    m_devices.append(device);
                }
            }
        }
    }
    
    endResetModel();
    emit countChanged();
}

QVariantMap AudioDeviceScanner::getDevice(int index) const
{
    QVariantMap deviceMap;
    if (index >= 0 && index < m_devices.size()) {
        const AudioDevice &device = m_devices[index];
        deviceMap["deviceId"] = device.deviceId;
        deviceMap["deviceName"] = device.deviceName;
        deviceMap["description"] = device.description;
        deviceMap["isAvailable"] = device.isAvailable;
    }
    return deviceMap;
}

QString AudioDeviceScanner::getFfmpegPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    
#ifdef Q_OS_WIN
    QString exeName = "ffmpeg.exe";
#else
    QString exeName = "ffmpeg";
#endif

    // 优先检查应用程序目录
    QString path = QDir(appDir).filePath(exeName);
    if (QFileInfo::exists(path)) {
        return path;
    }
    
    // 检查系统PATH
    QProcess findProcess;
#ifdef Q_OS_WIN
    findProcess.start("where", QStringList() << exeName);
#else
    findProcess.start("which", QStringList() << exeName);
#endif
    if (findProcess.waitForFinished(1000) && findProcess.exitCode() == 0) {
        QString systemPath = QString::fromUtf8(findProcess.readAllStandardOutput()).trimmed();
        systemPath = systemPath.split('\n').first().trimmed();
        if (QFileInfo::exists(systemPath)) {
            return systemPath;
        }
    }
    
    return QString();
}

