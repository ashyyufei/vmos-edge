#include "CameraStreamManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QNetworkInterface>
#include <QHostAddress>

CameraStreamManager::CameraStreamManager(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_rtspUrl("")
    , m_errorMessage("")
    , m_isStreaming(false)
    , m_currentStreamName("")
    , m_cachedEncoder(EncoderType::Software)
    , m_encoderDetected(false)
{
    m_process = new QProcess(this);
    
    // 连接信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CameraStreamManager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &CameraStreamManager::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &CameraStreamManager::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &CameraStreamManager::onReadyReadStandardError);
}

CameraStreamManager::~CameraStreamManager()
{
    stopStreaming();
}

bool CameraStreamManager::startStreaming(const QString &cameraDeviceId, 
                                         const QString &streamName,
                                         int width, 
                                         int height,
                                         int fps,
                                         const QString &audioDeviceId)
{
    if (m_isStreaming) {
        qWarning() << "CameraStreamManager: Already streaming, stop first";
        m_errorMessage = "已经在推流中，请先停止当前推流";
        emit errorOccurred(m_errorMessage);
        return false;
    }

    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        m_errorMessage = "未找到FFmpeg可执行文件";
        qWarning() << "CameraStreamManager:" << m_errorMessage;
        emit errorOccurred(m_errorMessage);
        return false;
    }

    // 允许视频设备ID为空（仅音频推流）
    bool videoOnly = !cameraDeviceId.isEmpty() && audioDeviceId.isEmpty();
    bool audioOnly = cameraDeviceId.isEmpty() && !audioDeviceId.isEmpty();
    bool videoAudio = !cameraDeviceId.isEmpty() && !audioDeviceId.isEmpty();
    
    if (cameraDeviceId.isEmpty() && audioDeviceId.isEmpty()) {
        m_errorMessage = "视频和音频设备不能同时为空";
        qWarning() << "CameraStreamManager:" << m_errorMessage;
        emit errorOccurred(m_errorMessage);
        return false;
    }

    QString streamType = videoOnly ? "仅视频" : (audioOnly ? "仅音频" : "音视频");
    qDebug() << "CameraStreamManager: 推流类型:" << streamType;
    if (!cameraDeviceId.isEmpty()) {
        qDebug() << "CameraStreamManager: 视频设备:" << cameraDeviceId;
        qDebug() << "CameraStreamManager: 分辨率:" << width << "x" << height << "@" << fps << "fps";
    }
    if (!audioDeviceId.isEmpty()) {
        qDebug() << "CameraStreamManager: 音频设备:" << audioDeviceId;
    }
    qDebug() << "CameraStreamManager: 流名称:" << streamName;

    // 构建FFmpeg命令
    QStringList args = buildFfmpegArgs(cameraDeviceId, streamName, width, height, fps, audioDeviceId);
    
    // 日志输出：为每个参数添加引号（如果包含空格）
    QStringList logArgs;
    for (const QString &arg : args) {
        if (arg.contains(" ") && !arg.startsWith("\"") && !arg.startsWith("'")) {
            logArgs << QString("\"%1\"").arg(arg);
        } else {
            logArgs << arg;
        }
    }
    qDebug() << "CameraStreamManager: FFmpeg command:" << ffmpegPath << logArgs.join(" ");

    // 设置RTSP URL
    m_rtspUrl = getRtspPushUrl(streamName);
    m_currentStreamName = streamName;
    m_errorMessage = "";
    
    // 启动FFmpeg进程
    m_process->start(ffmpegPath, args);
    
    if (!m_process->waitForStarted(3000)) {
        m_errorMessage = QString("启动FFmpeg失败: %1").arg(m_process->errorString());
        qWarning() << "CameraStreamManager:" << m_errorMessage;
        emit errorOccurred(m_errorMessage);
        return false;
    }

    // 等待一小段时间，检查进程是否立即退出（表示设备错误）
    QThread::msleep(500);
    if (m_process->state() == QProcess::NotRunning) {
        // 进程已经退出，读取错误信息
        QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
        qWarning() << "CameraStreamManager: FFmpeg exited immediately, error:" << errorOutput;
        
        // 检查是否是摄像头被占用
        QString lowerError = errorOutput.toLower();
        if (lowerError.contains("device already in use", Qt::CaseInsensitive) ||
            lowerError.contains("could not run graph", Qt::CaseInsensitive) ||
            lowerError.contains("i/o error", Qt::CaseInsensitive)) {
            m_errorMessage = "摄像头被其他应用占用，请关闭其他使用摄像头的程序（如相机应用、视频会议软件等）后重试";
        } else {
            // 尝试提取更具体的错误信息
            QString errorLine = errorOutput;
            // 查找包含"error"或"failed"的行
            QStringList lines = errorOutput.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QString lowerLine = line.toLower();
                if (lowerLine.contains("error") || lowerLine.contains("failed") || 
                    lowerLine.contains("cannot") || lowerLine.contains("i/o")) {
                    errorLine = line.trimmed();
                    break;
                }
            }
            m_errorMessage = QString("摄像头打开失败: %1").arg(errorLine);
        }
        
        emit errorOccurred(m_errorMessage);
        emit streamingStateChanged(false);
        emit streamingStopped();
        return false;
    }

    m_isStreaming = true;
    emit streamingStateChanged(true);
    emit streamingStarted();
    emit rtspUrlChanged(m_rtspUrl);
    
    qDebug() << "CameraStreamManager: Stream started successfully";
    qDebug() << "CameraStreamManager: RTSP URL:" << m_rtspUrl;
    
    return true;
}

void CameraStreamManager::stopStreaming()
{
    if (!m_isStreaming || !m_process) {
        return;
    }

    qDebug() << "CameraStreamManager: Stopping stream";

    // 尝试正常终止
    if (m_process->state() != QProcess::NotRunning) {
        // 发送'q'到FFmpeg标准输入以优雅退出
        m_process->write("q\n");
        
        if (!m_process->waitForFinished(3000)) {
            // 如果优雅退出失败，强制终止
            qWarning() << "CameraStreamManager: Process did not terminate gracefully, killing it";
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    m_isStreaming = false;
    m_rtspUrl = "";
    m_currentStreamName = "";
    
    emit streamingStateChanged(false);
    emit streamingStopped();
    emit rtspUrlChanged("");
    
    qDebug() << "CameraStreamManager: Stream stopped";
}

bool CameraStreamManager::isStreaming() const
{
    return m_isStreaming && m_process && m_process->state() == QProcess::Running;
}

QString CameraStreamManager::rtspUrl() const
{
    return m_rtspUrl;
}

QString CameraStreamManager::errorMessage() const
{
    return m_errorMessage;
}

QStringList CameraStreamManager::buildFfmpegArgs(const QString &cameraDeviceId,
                                                 const QString &streamName,
                                                 int width, int height, int fps,
                                                 const QString &audioDeviceId)
{
    QStringList args;
    
    // 默认参数：软件编码使用较低配置，硬件编码可用更高配置
    // 如果未指定(0)，使用保守的默认值以确保软件编码流畅
    int targetWidth = (width > 0) ? width : 960;   // 降至960（从1280）
    int targetHeight = (height > 0) ? height : 540;  // 降至540（从720）
    int targetFps = (fps > 0) ? fps : 20;  // 降至20（从25）
    
    // 检测最佳编码器
    EncoderType encoder = detectAvailableEncoder();
    QString encoderName;
    switch (encoder) {
        case EncoderType::NVENC: encoderName = "NVENC硬件编码"; break;
        case EncoderType::QSV: encoderName = "QSV硬件编码"; break;
        case EncoderType::AMF: encoderName = "AMF硬件编码"; break;
        default: encoderName = "软件编码"; break;
    }
    qDebug() << "CameraStreamManager: 使用编码器:" << encoderName;
    
#ifdef Q_OS_WIN
    // ==================== Windows dshow 输入配置 ====================
    // 支持三种模式：仅视频、仅音频、音视频
    QString inputSpec;
    if (!cameraDeviceId.isEmpty() && !audioDeviceId.isEmpty()) {
        // 音视频
        inputSpec = QString("video=%1:audio=%2").arg(cameraDeviceId, audioDeviceId);
    } else if (!cameraDeviceId.isEmpty()) {
        // 仅视频
        inputSpec = QString("video=%1").arg(cameraDeviceId);
    } else {
        // 仅音频
        inputSpec = QString("audio=%1").arg(audioDeviceId);
    }
    
    // 全局选项（必须在-i之前）
    args << "-loglevel" << "warning"  // 减少日志输出
         << "-hide_banner";  // 隐藏版本信息
    
    // 输入格式选项
    args << "-f" << "dshow";
    
    // 不指定输入分辨率和帧率，使用摄像头默认值（避免不支持的分辨率）
    // 后续通过滤镜缩放和fps控制
    
    // 输入缓冲区和性能选项
    QString bufferSize = (encoder == EncoderType::Software) ? "50M" : "20M";
    args << "-rtbufsize" << bufferSize  // 软件编码用50M，硬件用20M
         << "-probesize" << "5M"
         << "-analyzeduration" << "1M";
    
    // 输入源
    args << "-i" << inputSpec;
    
    // 仅音频模式：跳过视频处理
    if (cameraDeviceId.isEmpty()) {
        qDebug() << "CameraStreamManager: 仅音频模式，跳过视频编码";
        
        // 禁用视频
        args << "-vn";
        
        // 音频编码
        args << "-c:a" << "aac"
             << "-b:a" << "96k"
             << "-ar" << "44100"
             << "-ac" << "2"
             << "-strict" << "experimental";
        
        // 输出
        args << "-f" << "rtsp"
             << "-rtsp_transport" << "tcp"
             << getRtspPushUrl(streamName);
        
        return args;
    }
    
    // ==================== 通用处理选项 ====================
    // 时间戳和同步选项：使用CFR模式确保时间戳正确
    args << "-vsync" << "cfr";  // 恒定帧率，确保时间戳正确
    
    // 视频滤镜：快速缩放+帧率+SAR
    QStringList vfFilters;
    // 使用fps滤镜确保帧率稳定和时间戳正确
    vfFilters << QString("fps=%1").arg(targetFps);
    // 使用最快的缩放算法
    vfFilters << QString("scale=%1:%2:flags=fast_bilinear").arg(targetWidth).arg(targetHeight);
    vfFilters << "setsar=1";
    
    args << "-vf" << vfFilters.join(",");
    
    // ==================== 视频编码选项 ====================
    addVideoEncoderArgs(args, encoder, targetWidth, targetHeight, targetFps);
    
    // ==================== 音频编码选项 ====================
    if (!audioDeviceId.isEmpty()) {
        // 软件编码时降低音频质量以减轻CPU负担
        QString audioBitrate = (encoder == EncoderType::Software) ? "64k" : "96k";
        args << "-c:a" << "aac"
             << "-b:a" << audioBitrate  // 软件编码64k，硬件96k
             << "-ar" << "44100"  // 采样率44.1kHz
             << "-ac" << "2"  // 立体声
             << "-strict" << "experimental";
    } else {
        args << "-an";  // 无音频
    }
    
    // ==================== 输出格式选项 ====================
    args << "-f" << "rtsp"
         << "-rtsp_transport" << "tcp"  // 使用TCP传输（更可靠）
         << getRtspPushUrl(streamName);
    
#elif defined(Q_OS_LINUX)
    // Linux使用v4l2
    // 直接使用摄像头原始分辨率和帧率
    args << "-f" << "v4l2"
         << "-input_format" << "mjpeg"  // 尝试使用MJPEG格式（如果支持）
         << "-fflags" << "nobuffer"  // 输入端禁用缓冲
         << "-flags" << "low_delay"  // 输入低延迟标志
         << "-i" << cameraDeviceId;  // 例如: /dev/video0
    
    // 使用系统时钟作为时间戳，提高同步精度和流畅度
    args << "-use_wallclock_as_timestamps" << "1"
         << "-fflags" << "+genpts";  // 生成PTS，提高播放流畅度
    
    // 优化视频滤镜：合并scale、fps和setsar参数
    QStringList vfFilters;
    if (fps > 0) {
        vfFilters << QString("fps=%1").arg(fps);  // 在滤镜中设置帧率，更稳定
    }
    if (width > 0 && height > 0) {
        vfFilters << QString("scale=%1:%2").arg(width).arg(height);
    }
    vfFilters << "setsar=1";  // 设置SAR为1:1，避免比例问题
    
    if (!vfFilters.isEmpty()) {
        args << "-vf" << vfFilters.join(",");
    }
    
    // 使用默认帧率30来计算GOP（如果fps为0）
    int actualFps = fps > 0 ? fps : 30;
    
    // 计算合适的GOP大小（2秒一个关键帧，提高流畅度）
    int gopSize = actualFps * 2;
    
    args << "-c:v" << "libx264"
         << "-preset" << "ultrafast"  // 使用ultrafast以获得最低延迟
         << "-tune" << "zerolatency"
         << "-pix_fmt" << "yuv420p"
         << "-g" << QString::number(gopSize)  // GOP大小，2秒一个关键帧
         << "-keyint_min" << QString::number(actualFps)
         << "-b:v" << "5000k"  // 进一步提高视频比特率以提高清晰度
         << "-maxrate" << "5000k"
         << "-bufsize" << "10000k"  // 增加缓冲区以减少卡顿
         << "-x264-params" << QString("keyint=%1:min-keyint=%1:scenecut=0:threads=auto:bframes=0").arg(gopSize)  // 禁用B帧以减少延迟
         << "-flags" << "+global_header"
         << "-strict" << "experimental"
         << "-rtsp_transport" << "tcp"
         << "-muxdelay" << "0"  // 最小化mux延迟
         << "-f" << "rtsp"
         << getRtspPushUrl(streamName);
#elif defined(Q_OS_MACOS)
    // macOS使用avfoundation
    // 直接使用摄像头原始分辨率和帧率
    args << "-f" << "avfoundation"
         << "-fflags" << "nobuffer"  // 输入端禁用缓冲
         << "-flags" << "low_delay"  // 输入低延迟标志
         << "-i" << QString("%1:none").arg(cameraDeviceId);
    
    // 使用系统时钟作为时间戳，提高同步精度和流畅度
    args << "-use_wallclock_as_timestamps" << "1"
         << "-fflags" << "+genpts";  // 生成PTS，提高播放流畅度
    
    // 如果指定了分辨率和帧率（非0），则使用指定值；否则使用摄像头原始值
    if (fps > 0) {
        args << "-framerate" << QString::number(fps);
    }
    
    if (width > 0 && height > 0) {
        args << "-video_size" << QString("%1x%2").arg(width).arg(height);
    }
    
    // 优化视频滤镜：添加setsar参数
    QStringList vfFilters;
    vfFilters << "setsar=1";  // 设置SAR为1:1，避免比例问题
    
    if (!vfFilters.isEmpty()) {
        args << "-vf" << vfFilters.join(",");
    }
    
    // 使用默认帧率30来计算GOP（如果fps为0）
    int actualFps = fps > 0 ? fps : 30;
    
    // 计算合适的GOP大小（2秒一个关键帧，提高流畅度）
    int gopSize = actualFps * 2;
    
    args << "-c:v" << "libx264"
         << "-preset" << "ultrafast"  // 使用ultrafast以获得最低延迟
         << "-tune" << "zerolatency"
         << "-pix_fmt" << "yuv420p"
         << "-g" << QString::number(gopSize)  // GOP大小，2秒一个关键帧
         << "-keyint_min" << QString::number(actualFps)
         << "-b:v" << "5000k"  // 进一步提高视频比特率以提高清晰度
         << "-maxrate" << "5000k"
         << "-bufsize" << "10000k"  // 增加缓冲区以减少卡顿
         << "-x264-params" << QString("keyint=%1:min-keyint=%1:scenecut=0:threads=auto:bframes=0").arg(gopSize)  // 禁用B帧以减少延迟
         << "-flags" << "+global_header"
         << "-strict" << "experimental"
         << "-rtsp_transport" << "tcp"
         << "-muxdelay" << "0"  // 最小化mux延迟
         << "-f" << "rtsp"
         << getRtspPushUrl(streamName);
#endif

    return args;
}

// 检测可用的硬件编码器
CameraStreamManager::EncoderType CameraStreamManager::detectAvailableEncoder() const
{
    // 如果已经检测过，直接返回缓存结果
    if (m_encoderDetected) {
        return m_cachedEncoder;
    }
    
    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        m_cachedEncoder = EncoderType::Software;
        m_encoderDetected = true;
        return m_cachedEncoder;
    }
    
    // 策略：先测试硬件编码器是否真正可用，而不是仅检测FFmpeg是否支持
    // 这样可以避免硬件不存在时初始化失败
    
    // 测试 NVENC（NVIDIA）
    if (testEncoder(ffmpegPath, "h264_nvenc")) {
        qDebug() << "CameraStreamManager: 检测到可用的NVIDIA硬件编码器";
        m_cachedEncoder = EncoderType::NVENC;
        m_encoderDetected = true;
        return m_cachedEncoder;
    }
    
    // 测试 QSV（Intel）
    if (testEncoder(ffmpegPath, "h264_qsv")) {
        qDebug() << "CameraStreamManager: 检测到可用的Intel QSV硬件编码器";
        m_cachedEncoder = EncoderType::QSV;
        m_encoderDetected = true;
        return m_cachedEncoder;
    }
    
    // 测试 AMF（AMD）
    if (testEncoder(ffmpegPath, "h264_amf")) {
        qDebug() << "CameraStreamManager: 检测到可用的AMD硬件编码器";
        m_cachedEncoder = EncoderType::AMF;
        m_encoderDetected = true;
        return m_cachedEncoder;
    }
    
    // 回退到软件编码
    qDebug() << "CameraStreamManager: 未检测到可用的硬件编码器，使用软件编码";
    m_cachedEncoder = EncoderType::Software;
    m_encoderDetected = true;
    return m_cachedEncoder;
}

// 测试编码器是否真正可用（不仅是FFmpeg支持，还要硬件存在）
bool CameraStreamManager::testEncoder(const QString &ffmpegPath, const QString &encoderName) const
{
    // 使用一个简单的测试命令：尝试初始化编码器
    // ffmpeg -f lavfi -i nullsrc=s=128x128:d=0.1 -c:v encoderName -f null -
    QProcess process;
    QStringList args;
    args << "-f" << "lavfi"
         << "-i" << "nullsrc=s=128x128:d=0.1"
         << "-c:v" << encoderName
         << "-f" << "null"
         << "-";
    
    process.start(ffmpegPath, args);
    
    // 等待进程完成，最多2秒
    if (!process.waitForFinished(2000)) {
        process.kill();
        return false;
    }
    
    // 检查退出代码和错误输出
    if (process.exitCode() == 0) {
        return true;
    }
    
    // 检查错误信息
    QString errorOutput = QString::fromUtf8(process.readAllStandardError());
    
    // 这些错误表示编码器不可用
    if (errorOutput.contains("Cannot load", Qt::CaseInsensitive) ||
        errorOutput.contains("not available", Qt::CaseInsensitive) ||
        errorOutput.contains("Unknown encoder", Qt::CaseInsensitive) ||
        errorOutput.contains("No NVENC capable devices found", Qt::CaseInsensitive) ||
        errorOutput.contains("cannot open", Qt::CaseInsensitive) ||
        errorOutput.contains("not found", Qt::CaseInsensitive) ||
        errorOutput.contains("does not support", Qt::CaseInsensitive) ||
        errorOutput.contains("failed to open", Qt::CaseInsensitive) ||
        errorOutput.contains("DLL", Qt::CaseInsensitive) ||
        errorOutput.contains("Failed loading", Qt::CaseInsensitive)) {
        qDebug() << "CameraStreamManager:" << encoderName << "不可用:" << errorOutput.split('\n').first();
        return false;
    }
    
    // 没有明显错误，认为可用
    return true;
}

// 根据编码器类型添加视频编码参数
void CameraStreamManager::addVideoEncoderArgs(QStringList &args, EncoderType encoder, 
                                               int width, int height, int fps) const
{
    // 计算码率：根据分辨率自适应，软件编码使用更低码率
    int bitrate;
    bool isSoftware = (encoder == EncoderType::Software);
    
    if (width * height >= 1920 * 1080) {
        bitrate = isSoftware ? 2000 : 3500;  // 1080P: 软件2M / 硬件3.5M
    } else if (width * height >= 1280 * 720) {
        bitrate = isSoftware ? 1500 : 2500;  // 720P: 软件1.5M / 硬件2.5M
    } else {
        bitrate = isSoftware ? 1000 : 1500;  // 低分辨率: 软件1M / 硬件1.5M
    }
    
    int gopSize = fps;  // 1秒一个关键帧
    
    switch (encoder) {
        case EncoderType::NVENC:
            // NVIDIA硬件编码：高性能低延迟
            // 使用兼容旧版FFmpeg的参数
            args << "-c:v" << "h264_nvenc"
                 << "-preset" << "llhp"  // 低延迟高性能（兼容旧版，新版用p1）
                 << "-tune" << "ll"  // 低延迟调优（兼容旧版，新版用ull）
                 << "-rc" << "cbr"  // 恒定码率
                 << "-cbr" << "1"  // 启用CBR模式
                 << "-b:v" << QString::number(bitrate) + "k"
                 << "-maxrate" << QString::number(bitrate) + "k"
                 << "-bufsize" << QString::number(bitrate * 2) + "k"
                 << "-g" << QString::number(gopSize)
                 << "-bf" << "0"  // 无B帧
                 << "-zerolatency" << "1"  // 零延迟模式
                 << "-delay" << "0"  // 无延迟
                 << "-pix_fmt" << "yuv420p";
            break;
            
        case EncoderType::QSV:
            // Intel QSV硬件编码（兼容旧版FFmpeg）
            args << "-c:v" << "h264_qsv"
                 << "-preset" << "veryfast"  // 快速预设
                 << "-global_quality" << "23"  // 质量23（较好）
                 << "-b:v" << QString::number(bitrate) + "k"
                 << "-maxrate" << QString::number(bitrate) + "k"
                 << "-bufsize" << QString::number(bitrate * 2) + "k"
                 << "-g" << QString::number(gopSize)
                 << "-bf" << "0";
            break;
            
        case EncoderType::AMF:
            // AMD AMF硬件编码
            args << "-c:v" << "h264_amf"
                 << "-quality" << "speed"  // 速度优先
                 << "-rc" << "cbr"  // 恒定码率
                 << "-b:v" << QString::number(bitrate) + "k"
                 << "-maxrate" << QString::number(bitrate) + "k"
                 << "-bufsize" << QString::number(bitrate * 2) + "k"
                 << "-g" << QString::number(gopSize)
                 << "-bf" << "0"
                 << "-pix_fmt" << "yuv420p";
            break;
            
        default:  // Software
            // libx264软件编码：极致优化参数
            args << "-c:v" << "libx264"
                 << "-preset" << "ultrafast"  // 最快编码速度
                 << "-tune" << "zerolatency"  // 零延迟调优
                 << "-crf" << "30"  // 进一步降低质量换速度（从28到30）
                 << "-maxrate" << QString::number(bitrate) + "k"
                 << "-bufsize" << QString::number(bitrate * 2) + "k"
                 << "-g" << QString::number(gopSize)
                 << "-pix_fmt" << "yuv420p";
            break;
    }
    
    // 通用选项
    args << "-flags" << "+global_header";  // RTSP需要全局头
}

QString CameraStreamManager::getLocalIpAddress() const
{
    // 获取本机的第一个非回环IPv4地址
    // 优先选择192.168.x.x或10.x.x.x等内网地址
    QString preferredIp;
    QString fallbackIp;
    
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        // 只处理活动的、非回环的接口
        if (!(iface.flags() & QNetworkInterface::IsUp) || 
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        // 遍历接口上的所有IP地址条目
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            // 只处理IPv4
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QString ip = entry.ip().toString();
                
                // 优先选择内网地址（192.168.x.x, 10.x.x.x, 172.16-31.x.x）
                bool isPrivateIp = false;
                if (ip.startsWith("192.168.") || ip.startsWith("10.")) {
                    isPrivateIp = true;
                } else if (ip.startsWith("172.")) {
                    QStringList parts = ip.split('.');
                    if (parts.size() >= 2) {
                        bool ok;
                        int secondOctet = parts[1].toInt(&ok);
                        if (ok && secondOctet >= 16 && secondOctet <= 31) {
                            isPrivateIp = true;
                        }
                    }
                }
                
                if (isPrivateIp) {
                    if (preferredIp.isEmpty()) {
                        preferredIp = ip;
                    }
                } else if (fallbackIp.isEmpty()) {
                    // 其他IPv4地址作为备选
                    fallbackIp = ip;
                }
            }
        }
    }
    
    // 优先返回内网地址，如果没有则返回其他IPv4地址
    QString result = !preferredIp.isEmpty() ? preferredIp : fallbackIp;
    
    if (result.isEmpty()) {
        qWarning() << "CameraStreamManager: 无法获取本机IP地址，使用localhost";
        return "localhost";
    }
    
    qDebug() << "CameraStreamManager: 检测到本机IP地址:" << result;
    return result;
}

QString CameraStreamManager::getRtspPushUrl(const QString &streamName) const
{
    // MediaMtx默认RTSP端口8554
    // 使用本机IP地址而不是localhost，以便云机可以拉流
    QString localIp = getLocalIpAddress();
    // 推流地址格式: rtsp://192.168.x.x:8554/streamName
    // 注意：如果MediaMtx配置了认证，可能需要添加用户名密码
    // 格式: rtsp://username:password@192.168.x.x:8554/streamName
    return QString("rtsp://%1:8554/%2").arg(localIp, streamName);
}

QString CameraStreamManager::getFfmpegPath() const
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

void CameraStreamManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool wasStreaming = m_isStreaming;
    m_isStreaming = false;
    
    // 如果已经有更具体的错误信息（从stderr解析的），就不覆盖它
    bool hasSpecificError = !m_errorMessage.isEmpty() && 
                           (m_errorMessage.contains("摄像头被") || m_errorMessage.contains("推流错误"));
    
    if (exitStatus == QProcess::CrashExit) {
        if (!hasSpecificError) {
            m_errorMessage = QString("FFmpeg进程崩溃，退出代码: %1").arg(exitCode);
        }
        qWarning() << "CameraStreamManager:" << m_errorMessage;
        emit errorOccurred(m_errorMessage);
    } else if (exitCode != 0) {
        if (!hasSpecificError) {
            // 根据退出代码提供更具体的错误信息
            QString errorDetail;
            if (exitCode == 1) {
                errorDetail = "可能是摄像头设备问题或配置错误";
            } else {
                errorDetail = QString("退出代码: %1").arg(exitCode);
            }
            m_errorMessage = QString("FFmpeg进程异常退出，%1").arg(errorDetail);
        }
        qWarning() << "CameraStreamManager:" << m_errorMessage;
        emit errorOccurred(m_errorMessage);
    } else {
        qDebug() << "CameraStreamManager: FFmpeg exited normally with code" << exitCode;
        // 正常退出时清除错误信息
        m_errorMessage = "";
    }
    
    if (wasStreaming) {
        emit streamingStateChanged(false);
        emit streamingStopped();
        emit rtspUrlChanged("");
    }
}

void CameraStreamManager::onProcessError(QProcess::ProcessError error)
{
    m_isStreaming = false;
    
    QString errorString;
    switch (error) {
    case QProcess::FailedToStart:
        errorString = "启动FFmpeg进程失败";
        break;
    case QProcess::Crashed:
        errorString = "FFmpeg进程崩溃";
        break;
    case QProcess::Timedout:
        errorString = "FFmpeg进程操作超时";
        break;
    case QProcess::WriteError:
        errorString = "写入FFmpeg进程失败";
        break;
    case QProcess::ReadError:
        errorString = "读取FFmpeg进程失败";
        break;
    default:
        errorString = "FFmpeg进程未知错误";
        break;
    }
    
    m_errorMessage = QString("%1: %2").arg(errorString, m_process->errorString());
    qWarning() << "CameraStreamManager:" << m_errorMessage;
    emit errorOccurred(m_errorMessage);
    emit streamingStateChanged(false);
    emit streamingStopped();
}

void CameraStreamManager::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();
    if (!output.isEmpty()) {
        qDebug() << "CameraStreamManager [stdout]:" << output;
    }
}

void CameraStreamManager::onReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    QString output = QString::fromUtf8(data).trimmed();
    if (!output.isEmpty()) {
        // FFmpeg通常将信息输出到stderr
        qDebug() << "CameraStreamManager [stderr]:" << output;
        
        // 检查是否有错误信息
        QString lowerOutput = output.toLower();
        
        // 检查摄像头被占用错误
        if (lowerOutput.contains("device already in use") || 
            lowerOutput.contains("could not run graph") ||
            lowerOutput.contains("i/o error")) {
            m_errorMessage = "摄像头被其他应用占用，请关闭其他使用摄像头的程序后重试";
            qWarning() << "CameraStreamManager: Camera device in use";
            emit errorOccurred(m_errorMessage);
        }
        // 检查其他常见错误
        else if (lowerOutput.contains("error", Qt::CaseInsensitive) || 
                 lowerOutput.contains("failed", Qt::CaseInsensitive) ||
                 lowerOutput.contains("cannot", Qt::CaseInsensitive) ||
                 lowerOutput.contains("no such file", Qt::CaseInsensitive)) {
            // 提取关键错误信息
            QString errorMsg = output;
            // 尝试提取更具体的错误信息
            if (output.contains(":")) {
                int colonIndex = output.lastIndexOf(":");
                if (colonIndex > 0 && colonIndex < output.length() - 1) {
                    errorMsg = output.mid(colonIndex + 1).trimmed();
                }
            }
            
            // 如果错误信息太长，截取前200个字符
            if (errorMsg.length() > 200) {
                errorMsg = errorMsg.left(200) + "...";
            }
            
            m_errorMessage = QString("推流错误: %1").arg(errorMsg);
            emit errorOccurred(m_errorMessage);
        }
    }
}

void CameraStreamManager::testPlayback(const QString &rtspUrl)
{
    QString url = rtspUrl.isEmpty() ? m_rtspUrl : rtspUrl;
    
    if (url.isEmpty()) {
        qWarning() << "CameraStreamManager: No RTSP URL provided for test playback";
        return;
    }
    
    // 获取ffplay路径（通常和ffmpeg在同一目录）
    QString ffmpegPath = getFfmpegPath();
    if (ffmpegPath.isEmpty()) {
        qWarning() << "CameraStreamManager: FFmpeg not found, cannot test playback";
        return;
    }
    
    // 构建ffplay路径
    QString ffplayPath = ffmpegPath;
    ffplayPath.replace("ffmpeg.exe", "ffplay.exe");
    ffplayPath.replace("ffmpeg", "ffplay");
    
    // 检查ffplay是否存在
    if (!QFileInfo::exists(ffplayPath)) {
        qWarning() << "CameraStreamManager: FFplay not found at" << ffplayPath;
        qWarning() << "CameraStreamManager: Please ensure ffplay is in the same directory as ffmpeg";
        return;
    }
    
    // 构建ffplay命令参数
    QStringList args;
    args << "-rtsp_transport" << "tcp"  // 使用TCP传输
         << "-i" << url;                 // RTSP URL
    
    qDebug() << "CameraStreamManager: Starting ffplay for test playback:" << ffplayPath << args.join(" ");
    
    // 使用startDetached启动ffplay（独立进程，不阻塞）
    bool started = QProcess::startDetached(ffplayPath, args);
    if (!started) {
        qWarning() << "CameraStreamManager: Failed to start ffplay for test playback";
    } else {
        qDebug() << "CameraStreamManager: FFplay started successfully for URL:" << url;
    }
}

