#include "MediaMtxService.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>

MediaMtxService::MediaMtxService(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_rtspUrl("rtsp://localhost:8554")
    , m_httpApiUrl("http://localhost:9997")
    , m_isRunning(false)
{
    m_process = new QProcess(this);
    
    // 连接信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MediaMtxService::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &MediaMtxService::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &MediaMtxService::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &MediaMtxService::onReadyReadStandardError);
}

MediaMtxService::~MediaMtxService()
{
    stop();
}

bool MediaMtxService::start()
{
    if (m_isRunning) {
        qDebug() << "MediaMtxService: Service is already running";
        return true;
    }

    QString mediamtxPath = getMediaMtxPath();
    if (mediamtxPath.isEmpty()) {
        QString error = "MediaMtx executable not found";
        qWarning() << "MediaMtxService:" << error;
        emit serviceError(error);
        return false;
    }

    qDebug() << "MediaMtxService: Starting mediamtx from" << mediamtxPath;

    // 设置工作目录为可执行文件所在目录
    QFileInfo fileInfo(mediamtxPath);
    m_process->setWorkingDirectory(fileInfo.absolutePath());

    // 准备启动参数
    // mediamtx的用法: mediamtx.exe [<confpath>] [flags]
    // 配置文件路径应该作为位置参数传递，而不是使用标志
    QStringList arguments;
    
    // 如果存在配置文件，将配置文件路径作为位置参数传递
    QString configPath = getConfigPath();
    if (!configPath.isEmpty() && QFileInfo::exists(configPath)) {
        // 使用相对路径或绝对路径
        // 如果配置文件在工作目录下，可以使用相对路径
        QDir workingDir(fileInfo.absolutePath());
        QString relativeConfigPath = workingDir.relativeFilePath(configPath);
        if (relativeConfigPath.startsWith("..")) {
            // 如果相对路径包含..，使用绝对路径
            arguments << configPath;
        } else {
            // 使用相对路径
            arguments << relativeConfigPath;
        }
        qDebug() << "MediaMtxService: Using config file:" << configPath;
    }
    // 如果没有指定配置文件，mediamtx会使用默认的mediamtx.yml

    // 启动进程
    m_process->start(mediamtxPath, arguments);

    if (!m_process->waitForStarted(3000)) {
        QString error = QString("Failed to start mediamtx: %1").arg(m_process->errorString());
        qWarning() << "MediaMtxService:" << error;
        emit serviceError(error);
        return false;
    }

    m_isRunning = true;
    qDebug() << "MediaMtxService: mediamtx started successfully";
    qDebug() << "MediaMtxService: RTSP URL:" << m_rtspUrl;
    qDebug() << "MediaMtxService: HTTP API URL:" << m_httpApiUrl;
    
    emit serviceStarted();
    return true;
}

void MediaMtxService::stop()
{
    if (!m_process) {
        return;
    }

    // 检查进程是否还在运行（即使 m_isRunning 为 false，进程可能仍在运行）
    if (m_process->state() == QProcess::NotRunning) {
        m_isRunning = false;
        return;
    }

    qDebug() << "MediaMtxService: Stopping mediamtx service";

    // 断开信号连接，避免在停止过程中触发回调
    disconnect(m_process, nullptr, this, nullptr);

    // 先尝试正常终止（给一个很短的等待时间，避免阻塞UI）
    m_process->terminate();
    
    // 只等待500ms，避免阻塞UI太久
    if (!m_process->waitForFinished(500)) {
        // 如果正常退出失败，立即强制终止
        qDebug() << "MediaMtxService: Process did not terminate gracefully, killing it";
        m_process->kill();
        
        // 只等待200ms，确保kill命令已发送
        m_process->waitForFinished(200);
    }

    m_isRunning = false;
    qDebug() << "MediaMtxService: mediamtx stopped";
    
    // 重新连接信号（以防后续需要重启）
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MediaMtxService::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &MediaMtxService::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &MediaMtxService::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &MediaMtxService::onReadyReadStandardError);
    
    emit serviceStopped();
}

void MediaMtxService::stopQuick()
{
    if (!m_process) {
        return;
    }

    // 检查进程是否还在运行
    if (m_process->state() == QProcess::NotRunning) {
        m_isRunning = false;
        return;
    }

    qDebug() << "MediaMtxService: Quick stopping mediamtx service";

    // 断开信号连接，避免在停止过程中触发回调
    disconnect(m_process, nullptr, this, nullptr);

    // 直接强制终止，不等待
    m_process->kill();
    
    // 不等待进程结束，让系统在程序退出时自动清理
    // 这样可以避免阻塞UI
    m_isRunning = false;
    qDebug() << "MediaMtxService: mediamtx kill signal sent";
}

bool MediaMtxService::isRunning() const
{
    return m_isRunning && m_process && m_process->state() == QProcess::Running;
}

QString MediaMtxService::getRtspUrl() const
{
    return m_rtspUrl;
}

QString MediaMtxService::getHttpApiUrl() const
{
    return m_httpApiUrl;
}

QString MediaMtxService::getMediaMtxPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    
#ifdef Q_OS_WIN
    QString exeName = "mediamtx.exe";
#else
    QString exeName = "mediamtx";
#endif

    // 优先检查应用程序目录下的mediamtx子目录
    QString path = QDir(appDir).filePath(QString("mediamtx/%1").arg(exeName));
    if (QFileInfo::exists(path)) {
        return path;
    }

    // 其次检查应用程序目录（直接）
    path = QDir(appDir).filePath(exeName);
    if (QFileInfo::exists(path)) {
        return path;
    }

    // 最后检查系统PATH
    QProcess findProcess;
#ifdef Q_OS_WIN
    findProcess.start("where", QStringList() << exeName);
#else
    findProcess.start("which", QStringList() << exeName);
#endif
    if (findProcess.waitForFinished(1000) && findProcess.exitCode() == 0) {
        QString systemPath = QString::fromUtf8(findProcess.readAllStandardOutput()).trimmed();
        if (QFileInfo::exists(systemPath)) {
            return systemPath;
        }
    }

    qWarning() << "MediaMtxService: mediamtx executable not found in:" << appDir << "or mediamtx subdirectory";
    return QString();
}

QString MediaMtxService::getConfigPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 优先检查mediamtx子目录下的配置文件
    QStringList configNames = {
        "mediamtx/mediamtx.yml",      // 优先：mediamtx目录下的yml
        "mediamtx/mediamtx.yaml",     // 优先：mediamtx目录下的yaml
        "mediamtx.yml",               // 其次：应用程序目录下的yml
        "mediamtx.yaml"               // 最后：应用程序目录下的yaml
    };
    
    for (const QString &configName : configNames) {
        QString configPath = QDir(appDir).filePath(configName);
        if (QFileInfo::exists(configPath)) {
            return configPath;
        }
    }
    
    return QString();
}

void MediaMtxService::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isRunning = false;
    
    if (exitStatus == QProcess::CrashExit) {
        QString error = QString("mediamtx crashed with exit code %1").arg(exitCode);
        qWarning() << "MediaMtxService:" << error;
        emit serviceError(error);
    } else {
        qDebug() << "MediaMtxService: mediamtx exited with code" << exitCode;
    }
    
    emit serviceStopped();
}

void MediaMtxService::onProcessError(QProcess::ProcessError error)
{
    m_isRunning = false;
    
    QString errorString;
    switch (error) {
    case QProcess::FailedToStart:
        errorString = "Failed to start mediamtx process";
        break;
    case QProcess::Crashed:
        errorString = "mediamtx process crashed";
        break;
    case QProcess::Timedout:
        errorString = "mediamtx process operation timed out";
        break;
    case QProcess::WriteError:
        errorString = "Write error to mediamtx process";
        break;
    case QProcess::ReadError:
        errorString = "Read error from mediamtx process";
        break;
    default:
        errorString = "Unknown error occurred with mediamtx process";
        break;
    }
    
    qWarning() << "MediaMtxService:" << errorString << "-" << m_process->errorString();
    emit serviceError(errorString);
    emit serviceStopped();
}

void MediaMtxService::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();
    if (!output.isEmpty()) {
        qDebug() << "MediaMtxService [stdout]:" << output;
    }
}

void MediaMtxService::onReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromUtf8(data).trimmed();
    if (!output.isEmpty()) {
        qWarning() << "MediaMtxService [stderr]:" << output;
    }
}

