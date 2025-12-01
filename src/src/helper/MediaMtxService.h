#ifndef MEDIAMTXSERVICE_H
#define MEDIAMTXSERVICE_H

#include <QObject>
#include <QProcess>
#include <QString>

class MediaMtxService : public QObject
{
    Q_OBJECT

public:
    explicit MediaMtxService(QObject *parent = nullptr);
    ~MediaMtxService();

    // 启动mediamtx服务
    bool start();
    
    // 停止mediamtx服务
    void stop();
    
    // 快速停止mediamtx服务（不等待，用于程序退出时）
    void stopQuick();
    
    // 检查服务是否正在运行
    bool isRunning() const;
    
    // 获取RTSP服务地址
    QString getRtspUrl() const;
    
    // 获取HTTP API地址
    QString getHttpApiUrl() const;

signals:
    void serviceStarted();
    void serviceStopped();
    void serviceError(const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();

private:
    // 获取mediamtx可执行文件路径
    QString getMediaMtxPath() const;
    
    // 获取配置文件路径（如果需要）
    QString getConfigPath() const;

    QProcess *m_process;
    QString m_rtspUrl;
    QString m_httpApiUrl;
    bool m_isRunning;
};

#endif // MEDIAMTXSERVICE_H

