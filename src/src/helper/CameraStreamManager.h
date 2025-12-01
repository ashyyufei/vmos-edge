#ifndef CAMERASTREAMMANAGER_H
#define CAMERASTREAMMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QNetworkInterface>
#include <QHostAddress>

class CameraStreamManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY streamingStateChanged)
    Q_PROPERTY(QString rtspUrl READ rtspUrl NOTIFY rtspUrlChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorOccurred)

public:
    explicit CameraStreamManager(QObject *parent = nullptr);
    ~CameraStreamManager();

    // 开始推流
    Q_INVOKABLE bool startStreaming(const QString &cameraDeviceId, 
                                    const QString &streamName = "camera",
                                    int width = 1280, 
                                    int height = 720,
                                    int fps = 30,
                                    const QString &audioDeviceId = "");  // 音频设备ID，为空则不推音频
    
    // 停止推流
    Q_INVOKABLE void stopStreaming();
    
    // 检查是否正在推流
    bool isStreaming() const;
    
    // 获取RTSP URL
    QString rtspUrl() const;
    
    // 获取错误信息
    QString errorMessage() const;
    
    // 测试播放RTSP流（使用ffplay）
    Q_INVOKABLE void testPlayback(const QString &rtspUrl = "");

signals:
    void streamingStarted();
    void streamingStopped();
    void streamingStateChanged(bool isStreaming);
    void rtspUrlChanged(const QString &url);
    void errorOccurred(const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    // 编码器类型枚举
    enum class EncoderType {
        Software,      // libx264 软件编码
        NVENC,         // NVIDIA 硬件编码
        QSV,           // Intel Quick Sync 硬件编码
        AMF            // AMD 硬件编码
    };
    
    // 获取FFmpeg路径
    QString getFfmpegPath() const;
    
    // 构建FFmpeg命令参数
    QStringList buildFfmpegArgs(const QString &cameraDeviceId,
                                const QString &streamName,
                                int width, int height, int fps,
                                const QString &audioDeviceId = "");
    
    // 检测可用的硬件编码器
    EncoderType detectAvailableEncoder() const;
    
    // 测试编码器是否真正可用
    bool testEncoder(const QString &ffmpegPath, const QString &encoderName) const;
    
    // 根据编码器类型构建视频编码参数
    void addVideoEncoderArgs(QStringList &args, EncoderType encoder, 
                            int width, int height, int fps) const;
    
    // 获取RTSP推流地址
    QString getRtspPushUrl(const QString &streamName) const;
    
    // 获取本机IP地址（用于RTSP推流，让云机可以拉流）
    QString getLocalIpAddress() const;
    
    QProcess *m_process;
    QString m_rtspUrl;
    QString m_errorMessage;
    bool m_isStreaming;
    QString m_currentStreamName;
    mutable EncoderType m_cachedEncoder;  // 缓存检测到的编码器
    mutable bool m_encoderDetected;       // 是否已检测编码器
};

#endif // CAMERASTREAMMANAGER_H

