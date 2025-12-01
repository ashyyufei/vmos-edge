#include "CameraDeviceScanner.h"
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

CameraDeviceScanner::CameraDeviceScanner(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CameraDeviceScanner::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_devices.size();
}

QVariant CameraDeviceScanner::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_devices.size()) {
        return QVariant();
    }

    const CameraDevice &device = m_devices.at(index.row());
    
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

QHash<int, QByteArray> CameraDeviceScanner::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DeviceIdRole] = "deviceId";
    roles[DeviceNameRole] = "deviceName";
    roles[DescriptionRole] = "description";
    roles[IsAvailableRole] = "isAvailable";
    return roles;
}

void CameraDeviceScanner::scanDevices()
{
    qDebug() << "CameraDeviceScanner: Starting device scan...";
    
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit countChanged();
    
    // 优先使用FFmpeg枚举（跨平台）
    scanDevicesWithFfmpeg();
    
    // 如果FFmpeg方法失败，使用平台特定方法
    if (m_devices.isEmpty()) {
#ifdef Q_OS_WIN
        scanWindowsDevices();
#elif defined(Q_OS_LINUX)
        scanLinuxDevices();
#elif defined(Q_OS_MACOS)
        scanMacDevices();
#endif
    }
    
    if (m_devices.isEmpty()) {
        emit scanError("未找到可用的摄像头设备");
    } else {
        qDebug() << "CameraDeviceScanner: Found" << m_devices.size() << "camera devices";
    }
    
    emit scanFinished();
}

void CameraDeviceScanner::scanDevicesWithFfmpeg()
{
    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        qWarning() << "CameraDeviceScanner: FFmpeg not found, cannot scan devices";
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
    // Linux使用v4l2
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-hide_banner" 
                       << "-f" << "v4l2" 
                       << "-list_formats" << "all" 
                       << "-i" << "/dev/video0");
#elif defined(Q_OS_MACOS)
    // macOS使用avfoundation
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-f" << "avfoundation" 
                       << "-list_devices" << "true" 
                       << "-i" << "");
#endif

    if (!ffmpegProcess.waitForStarted(3000)) {
        qWarning() << "CameraDeviceScanner: Failed to start FFmpeg process:" << ffmpegProcess.errorString();
        return;
    }
    
    if (!ffmpegProcess.waitForFinished(5000)) {
        qWarning() << "CameraDeviceScanner: FFmpeg process timeout";
        ffmpegProcess.kill();
        return;
    }
    
    QString output = QString::fromUtf8(ffmpegProcess.readAllStandardError());
    qDebug() << "CameraDeviceScanner: FFmpeg output:" << output;
    
#ifdef Q_OS_WIN
    parseFfmpegDeviceList(output, "dshow");
#elif defined(Q_OS_LINUX)
    // Linux需要扫描/dev/video*设备
    scanLinuxDevices();
#elif defined(Q_OS_MACOS)
    parseFfmpegDeviceList(output, "avfoundation");
#endif
}

void CameraDeviceScanner::parseFfmpegDeviceList(const QString &output, const QString &format)
{
    beginResetModel();
    
    if (format == "dshow") {
        // Windows DirectShow格式解析
        // 实际输出格式:
        // [dshow @ ...] DirectShow video devices ...
        // [dshow @ ...]  "设备名"
        // [dshow @ ...]     Alternative name "..."
        // [dshow @ ...]  "设备名2"
        
        // 先找到 "DirectShow video devices" 行
        QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        bool inVideoDevicesSection = false;
        
        qDebug() << "CameraDeviceScanner: Parsing DirectShow devices, total lines:" << lines.size();
        
        int index = 0;
        for (const QString &line : lines) {
            // 检查是否进入视频设备区域
            if (line.contains("DirectShow video devices")) {
                inVideoDevicesSection = true;
                continue;
            }
            
            // 如果遇到音频设备区域，停止解析
            if (line.contains("DirectShow audio devices")) {
                break;
            }
            
            // 如果在视频设备区域
            if (inVideoDevicesSection) {
                // 跳过 "Alternative name" 行
                if (line.contains("Alternative name", Qt::CaseInsensitive)) {
                    continue;
                }
                
                // 匹配设备名行：包含 [dshow @ ...] 和引号，但不包含 "Alternative"
                // 格式: [dshow @ 000001f25d4a2d80]  "1080P USB Camera"
                if (line.contains("[dshow") && line.contains("\"") && !line.contains("Alternative", Qt::CaseInsensitive)) {
                    // 提取引号中的设备名
                    int startQuote = line.indexOf('"');
                    int endQuote = line.indexOf('"', startQuote + 1);
                    
                    if (startQuote >= 0 && endQuote > startQuote) {
                        QString deviceName = line.mid(startQuote + 1, endQuote - startQuote - 1);
                        
                        if (!deviceName.isEmpty()) {
                            qDebug() << "CameraDeviceScanner: Found device:" << deviceName;
                            
                            CameraDevice device;
                            device.deviceId = deviceName;  // Windows dshow使用设备名称
                            device.deviceName = deviceName;
                            device.description = QString("摄像头 %1").arg(index + 1);
                            device.isAvailable = true;
                            
                            m_devices.append(device);
                            index++;
                        }
                    }
                }
            }
        }
    } else if (format == "avfoundation") {
        // macOS AVFoundation格式解析
        // 示例输出: "[0] FaceTime HD Camera"
        QRegularExpression re("\\[\\d+\\]\\s+(.+)");
        QRegularExpressionMatchIterator i = re.globalMatch(output);
        
        int index = 0;
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString deviceName = match.captured(1).trimmed();
            
            CameraDevice device;
            device.deviceId = QString::number(index);  // macOS使用索引
            device.deviceName = deviceName;
            device.description = deviceName;
            device.isAvailable = true;
            
            m_devices.append(device);
            index++;
        }
    }
    
    endResetModel();
    emit countChanged();
}

void CameraDeviceScanner::scanWindowsDevices()
{
    // Windows备用方案：可以尝试使用DirectShow API或枚举注册表
    // 这里先使用FFmpeg方法，如果失败可以扩展
    qDebug() << "CameraDeviceScanner: Windows specific scan not implemented, using FFmpeg method";
}

void CameraDeviceScanner::scanLinuxDevices()
{
    beginResetModel();
    
    // Linux: 扫描/dev/video*设备
    for (int i = 0; i < 10; ++i) {
        QString devicePath = QString("/dev/video%1").arg(i);
        QFileInfo fileInfo(devicePath);
        
        if (fileInfo.exists()) {
            // 尝试使用v4l2-ctl获取设备信息（如果可用）
            QProcess v4l2Process;
            v4l2Process.start("v4l2-ctl", QStringList() << "--device" << devicePath << "--info");
            
            QString deviceName = QString("Video Device %1").arg(i);
            if (v4l2Process.waitForFinished(1000)) {
                QString output = QString::fromUtf8(v4l2Process.readAllStandardOutput());
                // 解析设备名称（简化处理）
                QRegularExpression re("Card\\s+name\\s*:\\s*(.+)");
                QRegularExpressionMatch match = re.match(output);
                if (match.hasMatch()) {
                    deviceName = match.captured(1).trimmed();
                }
            }
            
            CameraDevice device;
            device.deviceId = devicePath;
            device.deviceName = deviceName;
            device.description = QString("摄像头 %1").arg(i);
            device.isAvailable = true;
            
            m_devices.append(device);
        }
    }
    
    endResetModel();
    emit countChanged();
}

void CameraDeviceScanner::scanMacDevices()
{
    // macOS备用方案：可以使用AVFoundation API
    qDebug() << "CameraDeviceScanner: macOS specific scan not implemented, using FFmpeg method";
}

QVariantMap CameraDeviceScanner::getDevice(int index) const
{
    if (index < 0 || index >= m_devices.size()) {
        return QVariantMap();
    }
    
    const CameraDevice &device = m_devices.at(index);
    QVariantMap map;
    map["deviceId"] = device.deviceId;
    map["deviceName"] = device.deviceName;
    map["description"] = device.description;
    map["isAvailable"] = device.isAvailable;
    
    return map;
}

void CameraDeviceScanner::queryDeviceCapabilities(int index)
{
    if (index < 0 || index >= m_devices.size()) {
        qWarning() << "CameraDeviceScanner: Invalid device index:" << index;
        return;
    }
    
    qDebug() << "CameraDeviceScanner: Querying capabilities for device" << index;
    queryDeviceCapabilitiesWithFfmpeg(index);
}

void CameraDeviceScanner::queryDeviceCapabilitiesWithFfmpeg(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    
    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        qWarning() << "CameraDeviceScanner: FFmpeg not found, cannot query capabilities";
        return;
    }
    
    const CameraDevice &device = m_devices.at(deviceIndex);
    QProcess ffmpegProcess;
    
#ifdef Q_OS_WIN
    // Windows使用dshow的-list_options参数查询设备能力
    QString deviceSpec = QString("video=%1").arg(device.deviceId);
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-f" << "dshow"
                       << "-list_options" << "true"
                       << "-i" << deviceSpec);
#elif defined(Q_OS_LINUX)
    // Linux使用v4l2的-list_formats参数
    ffmpegProcess.start(ffmpegPath, QStringList() 
                       << "-hide_banner"
                       << "-f" << "v4l2"
                       << "-list_formats" << "all"
                       << "-i" << device.deviceId);
#elif defined(Q_OS_MACOS)
    // macOS使用avfoundation，需要先获取设备索引
    // 注意：macOS的能力查询比较复杂，这里先简化处理
    qDebug() << "CameraDeviceScanner: macOS capabilities query not fully implemented, using defaults";
    setDefaultCapabilities(deviceIndex);
    emit capabilitiesQueried(deviceIndex);
    return;
#endif

    if (!ffmpegProcess.waitForStarted(3000)) {
        qWarning() << "CameraDeviceScanner: Failed to start FFmpeg process for capabilities query:" << ffmpegProcess.errorString();
        // 设置默认值
        setDefaultCapabilities(deviceIndex);
        emit capabilitiesQueried(deviceIndex);
        return;
    }
    
    if (!ffmpegProcess.waitForFinished(5000)) {
        qWarning() << "CameraDeviceScanner: FFmpeg capabilities query timeout";
        ffmpegProcess.kill();
        // 设置默认值
        setDefaultCapabilities(deviceIndex);
        emit capabilitiesQueried(deviceIndex);
        return;
    }
    
    QString output = QString::fromUtf8(ffmpegProcess.readAllStandardError());
    qDebug() << "CameraDeviceScanner: Capabilities output:" << output;
    
#ifdef Q_OS_WIN
    parseDshowCapabilities(output, deviceIndex);
#elif defined(Q_OS_LINUX)
    parseV4l2Capabilities(output, deviceIndex);
#endif
    
    // 确保至少有一些默认值
    if (deviceIndex >= 0 && deviceIndex < m_devices.size()) {
        CameraDevice &device = m_devices[deviceIndex];
        if (device.supportedResolutions.isEmpty() || device.supportedFps.isEmpty()) {
            qWarning() << "CameraDeviceScanner: No capabilities found, using defaults";
            setDefaultCapabilities(deviceIndex);
        }
    }
    
    emit capabilitiesQueried(deviceIndex);
}

void CameraDeviceScanner::parseDshowCapabilities(const QString &output, int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    
    CameraDevice &device = m_devices[deviceIndex];
    device.supportedResolutions.clear();
    device.supportedFps.clear();
    
    // 解析dshow输出格式示例（多种可能的格式）：
    // 格式1: "video_size=640x480" "framerate=30/1"
    // 格式2: video_size=640x480 framerate=30/1
    // 格式3: [dshow @ ...] "video_size=640x480" "framerate=30/1"
    
    QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    
    // 尝试多种正则表达式模式
    QRegularExpression resolutionRe1("\"video_size=(\\d+)x(\\d+)\"");  // 带引号
    QRegularExpression resolutionRe2("video_size=(\\d+)x(\\d+)");      // 不带引号
    QRegularExpression fpsRe1("\"framerate=(\\d+)/(\\d+)\"");          // 带引号
    QRegularExpression fpsRe2("framerate=(\\d+)/(\\d+)");              // 不带引号
    
    QSet<QString> resolutionSet;  // 使用Set去重
    QSet<int> fpsSet;
    
    qDebug() << "CameraDeviceScanner: Parsing capabilities, total lines:" << lines.size();
    
    for (const QString &line : lines) {
        // 解析分辨率 - 尝试多种格式
        QRegularExpressionMatch resMatch = resolutionRe1.match(line);
        if (!resMatch.hasMatch()) {
            resMatch = resolutionRe2.match(line);
        }
        
        if (resMatch.hasMatch()) {
            int width = resMatch.captured(1).toInt();
            int height = resMatch.captured(2).toInt();
            QString resKey = QString("%1x%2").arg(width).arg(height);
            if (!resolutionSet.contains(resKey) && width > 0 && height > 0) {
                Resolution res;
                res.width = width;
                res.height = height;
                device.supportedResolutions.append(res);
                resolutionSet.insert(resKey);
                qDebug() << "CameraDeviceScanner: Found resolution:" << resKey;
            }
        }
        
        // 解析帧率 - 尝试多种格式
        QRegularExpressionMatch fpsMatch = fpsRe1.match(line);
        if (!fpsMatch.hasMatch()) {
            fpsMatch = fpsRe2.match(line);
        }
        
        if (fpsMatch.hasMatch()) {
            int fpsNum = fpsMatch.captured(1).toInt();
            int fpsDen = fpsMatch.captured(2).toInt();
            if (fpsDen > 0) {
                int fps = fpsNum / fpsDen;
                if (fps > 0 && !fpsSet.contains(fps)) {
                    device.supportedFps.append(fps);
                    fpsSet.insert(fps);
                    qDebug() << "CameraDeviceScanner: Found fps:" << fps;
                }
            }
        }
    }
    
    // 如果没有找到能力信息，使用默认值
    if (device.supportedResolutions.isEmpty()) {
        qWarning() << "CameraDeviceScanner: No resolutions found in output, using defaults";
        setDefaultCapabilities(deviceIndex);
    } else {
        // 排序：分辨率从大到小
        std::sort(device.supportedResolutions.begin(), device.supportedResolutions.end(),
                  [](const Resolution &a, const Resolution &b) {
                      return (a.width * a.height) > (b.width * b.height);
                  });
    }
    
    if (device.supportedFps.isEmpty()) {
        qWarning() << "CameraDeviceScanner: No fps found in output, using defaults";
        // 如果分辨率已找到但帧率未找到，只设置帧率默认值
        if (device.supportedResolutions.isEmpty()) {
            setDefaultCapabilities(deviceIndex);
        } else {
            // 只设置帧率默认值
            device.supportedFps.append(30);
            device.supportedFps.append(24);
            device.supportedFps.append(15);
        }
    } else {
        std::sort(device.supportedFps.begin(), device.supportedFps.end(), std::greater<int>());
    }
    
    qDebug() << "CameraDeviceScanner: Final result -" << device.supportedResolutions.size() 
             << "resolutions and" << device.supportedFps.size() << "fps options";
}

void CameraDeviceScanner::parseV4l2Capabilities(const QString &output, int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    
    CameraDevice &device = m_devices[deviceIndex];
    device.supportedResolutions.clear();
    device.supportedFps.clear();
    
    // 解析v4l2输出格式示例：
    // [v4l2 @ 0x...] Raw       :     yuyv422 : 640x480 30.000000 fps
    // [v4l2 @ 0x...] Raw       :     yuyv422 : 1280x720 30.000000 fps
    
    QRegularExpression re("(\\d+)x(\\d+)\\s+([\\d.]+)\\s+fps");
    QRegularExpressionMatchIterator i = re.globalMatch(output);
    
    QSet<QString> resolutionSet;
    QSet<int> fpsSet;
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        int width = match.captured(1).toInt();
        int height = match.captured(2).toInt();
        double fpsDouble = match.captured(3).toDouble();
        int fps = static_cast<int>(fpsDouble);
        
        QString resKey = QString("%1x%2").arg(width).arg(height);
        if (!resolutionSet.contains(resKey)) {
            Resolution res;
            res.width = width;
            res.height = height;
            device.supportedResolutions.append(res);
            resolutionSet.insert(resKey);
        }
        
        if (fps > 0 && !fpsSet.contains(fps)) {
            device.supportedFps.append(fps);
            fpsSet.insert(fps);
        }
    }
    
    // 如果没有找到能力信息，使用默认值
    if (device.supportedResolutions.isEmpty() || device.supportedFps.isEmpty()) {
        setDefaultCapabilities(deviceIndex);
    } else {
        // 排序
        std::sort(device.supportedResolutions.begin(), device.supportedResolutions.end(),
                  [](const Resolution &a, const Resolution &b) {
                      return (a.width * a.height) > (b.width * b.height);
                  });
        std::sort(device.supportedFps.begin(), device.supportedFps.end(), std::greater<int>());
    }
    
    qDebug() << "CameraDeviceScanner: Found" << device.supportedResolutions.size() 
             << "resolutions and" << device.supportedFps.size() << "fps options";
}

void CameraDeviceScanner::setDefaultCapabilities(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_devices.size()) {
        return;
    }
    
    CameraDevice &device = m_devices[deviceIndex];
    
    // 设置默认分辨率（如果为空）
    if (device.supportedResolutions.isEmpty()) {
        Resolution res1, res2, res3;
        res1.width = 1920; res1.height = 1080;
        res2.width = 1280; res2.height = 720;
        res3.width = 640; res3.height = 480;
        device.supportedResolutions.append(res1);
        device.supportedResolutions.append(res2);
        device.supportedResolutions.append(res3);
    }
    
    // 设置默认帧率（如果为空）
    if (device.supportedFps.isEmpty()) {
        device.supportedFps.append(30);
        device.supportedFps.append(24);
        device.supportedFps.append(15);
    }
    
    qDebug() << "CameraDeviceScanner: Set default capabilities for device" << deviceIndex;
}

QVariantList CameraDeviceScanner::getSupportedResolutions(int index) const
{
    QVariantList list;
    if (index < 0 || index >= m_devices.size()) {
        qWarning() << "CameraDeviceScanner: Invalid index for getSupportedResolutions:" << index;
        return list;
    }
    
    const CameraDevice &device = m_devices.at(index);
    qDebug() << "CameraDeviceScanner: getSupportedResolutions for device" << index 
             << "has" << device.supportedResolutions.size() << "resolutions";
    
    for (const Resolution &res : device.supportedResolutions) {
        QVariantMap map;
        map["width"] = res.width;
        map["height"] = res.height;
        map["label"] = res.toString();
        list.append(map);
        qDebug() << "CameraDeviceScanner: Added resolution:" << res.toString();
    }
    
    // 如果列表为空，返回默认值
    if (list.isEmpty()) {
        qWarning() << "CameraDeviceScanner: Resolution list is empty, adding defaults";
        QVariantMap res1, res2, res3;
        res1["width"] = 1920; res1["height"] = 1080; res1["label"] = "1920x1080";
        res2["width"] = 1280; res2["height"] = 720; res2["label"] = "1280x720";
        res3["width"] = 640; res3["height"] = 480; res3["label"] = "640x480";
        list.append(res1);
        list.append(res2);
        list.append(res3);
    }
    
    return list;
}

QVariantList CameraDeviceScanner::getSupportedFps(int index) const
{
    QVariantList list;
    if (index < 0 || index >= m_devices.size()) {
        qWarning() << "CameraDeviceScanner: Invalid index for getSupportedFps:" << index;
        return list;
    }
    
    const CameraDevice &device = m_devices.at(index);
    qDebug() << "CameraDeviceScanner: getSupportedFps for device" << index 
             << "has" << device.supportedFps.size() << "fps options";
    
    for (int fps : device.supportedFps) {
        QVariantMap map;
        map["fps"] = fps;
        map["label"] = QString("%1 fps").arg(fps);
        list.append(map);
        qDebug() << "CameraDeviceScanner: Added fps:" << fps;
    }
    
    // 如果列表为空，返回默认值
    if (list.isEmpty()) {
        qWarning() << "CameraDeviceScanner: FPS list is empty, adding defaults";
        QVariantMap fps1, fps2, fps3;
        fps1["fps"] = 30; fps1["label"] = "30 fps";
        fps2["fps"] = 24; fps2["label"] = "24 fps";
        fps3["fps"] = 15; fps3["label"] = "15 fps";
        list.append(fps1);
        list.append(fps2);
        list.append(fps3);
    }
    
    return list;
}

QString CameraDeviceScanner::getFfmpegPath() const
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
        // 可能有多行输出，取第一行
        systemPath = systemPath.split('\n').first().trimmed();
        if (QFileInfo::exists(systemPath)) {
            return systemPath;
        }
    }
    
    return QString();
}

