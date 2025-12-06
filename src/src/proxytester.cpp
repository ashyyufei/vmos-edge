#include "proxytester.h"
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>
#include <QByteArray>
#include <QSslConfiguration>
#include <QSslSocket>

ProxyTester::ProxyTester(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_timeoutTimer(new QTimer(this))
{
    // 设置超时时间为10秒
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(10000);
    
    connect(m_timeoutTimer, &QTimer::timeout, this, &ProxyTester::onRequestTimeout);
}

void ProxyTester::testProxy(const QString &serverAddress, 
                           int port, 
                           const QString &username, 
                           const QString &password, 
                           const QString &protocol,
                           const QString &testUrl)
{
    qDebug() << "开始测试代理连接...";
    qDebug() << "代理地址:" << serverAddress << "端口:" << port << "协议:" << protocol;
    qDebug() << "测试URL:" << testUrl;
    
    emit testProgress("正在设置代理...");
    
    // 清理之前的连接
    cleanup();
    
    // 创建代理对象
    QNetworkProxy proxy;
    setupProxy(proxy, serverAddress, port, username, password, protocol);
    
    // 设置网络管理器使用代理
    m_networkManager->setProxy(proxy);
    
    emit testProgress("正在连接代理服务器...");
    
    // 开始计时
    m_elapsedTimer.start();
    
    // 创建网络请求
    QNetworkRequest request;
    request.setUrl(QUrl(testUrl));
    request.setRawHeader(QByteArray("User-Agent"), QByteArray("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"));
    
    // 如果是 HTTPS 请求，忽略 SSL 证书错误（仅用于测试代理连接）
    if (testUrl.startsWith("https://", Qt::CaseInsensitive)) {
        QSslConfiguration sslConfig = request.sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(sslConfig);
        qDebug() << "已忽略 SSL 证书验证（仅用于测试）";
    }
    
    // 发送请求
    m_currentReply = m_networkManager->get(request);
    
    // 连接信号
    connect(m_currentReply, &QNetworkReply::finished, this, &ProxyTester::onRequestFinished);
    
    // 启动超时定时器
    m_timeoutTimer->start();
    qDebug() << "已发送测试请求，等待响应...";
}

void ProxyTester::setupProxy(QNetworkProxy &proxy, const QString &serverAddress, int port, 
                            const QString &username, const QString &password, const QString &protocol)
{
    // 设置代理类型
    QString protocolLower = protocol.toLower();
    if (protocolLower == "socks5") {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        qDebug() << "设置代理类型: SOCKS5";
    } else if (protocolLower == "http" || protocolLower == "https" || protocolLower == "http-relay") {
        proxy.setType(QNetworkProxy::HttpProxy);
        qDebug() << "设置代理类型: HTTP";
    } else {
        proxy.setType(QNetworkProxy::HttpProxy); // 默认使用HTTP代理
        qDebug() << "设置代理类型: HTTP (默认)";
    }
    
    // 设置代理服务器地址和端口
    proxy.setHostName(serverAddress);
    proxy.setPort(port);
    qDebug() << "代理服务器:" << serverAddress << ":" << port;
    
    // 设置认证信息
    if (!username.isEmpty() && !password.isEmpty()) {
        proxy.setUser(username);
        proxy.setPassword(password);
        qDebug() << "代理认证: 已设置用户名";
    } else {
        qDebug() << "代理认证: 未设置";
    }
}

void ProxyTester::onRequestFinished()
{
    m_timeoutTimer->stop();
    
    if (!m_currentReply) {
        return;
    }
    
    int latency = m_elapsedTimer.elapsed();
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        // 请求成功
        emit testCompleted(true, QString("代理连接成功！延迟: %1ms").arg(latency), latency);
    } else {
        // 请求失败，显示详细错误信息
        QString errorString = m_currentReply->errorString();
        QNetworkReply::NetworkError error = m_currentReply->error();
        
        QString errorMessage;
        switch (error) {
            case QNetworkReply::ProxyConnectionRefusedError:
                errorMessage = QString("代理服务器拒绝连接，请检查代理地址和端口是否正确");
                break;
            case QNetworkReply::ProxyConnectionClosedError:
                errorMessage = QString("代理服务器连接已关闭");
                break;
            case QNetworkReply::ProxyNotFoundError:
                errorMessage = QString("无法找到代理服务器，请检查代理地址是否正确");
                break;
            case QNetworkReply::ProxyAuthenticationRequiredError:
                errorMessage = QString("代理认证失败，请检查用户名和密码是否正确");
                break;
            case QNetworkReply::ProxyTimeoutError:
                errorMessage = QString("代理连接超时，请检查网络连接");
                break;
            case QNetworkReply::ConnectionRefusedError:
                errorMessage = QString("连接被拒绝，请检查代理服务器是否正常运行");
                break;
            case QNetworkReply::TimeoutError:
                errorMessage = QString("连接超时，请检查网络连接和代理设置");
                break;
            case QNetworkReply::HostNotFoundError:
                errorMessage = QString("无法解析代理服务器地址，请检查地址是否正确");
                break;
            default:
                errorMessage = QString("代理连接失败: %1 (错误代码: %2)").arg(errorString).arg(error);
                break;
        }
        
        qDebug() << "代理测试失败:" << errorMessage << "原始错误:" << errorString;
        emit testCompleted(false, errorMessage, latency);
    }
    
    cleanup();
}

void ProxyTester::onRequestTimeout()
{
    if (m_currentReply) {
        m_currentReply->abort();
        int latency = m_elapsedTimer.elapsed();
        emit testCompleted(false, "代理连接超时", latency);
        cleanup();
    }
}

void ProxyTester::cleanup()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    m_timeoutTimer->stop();
    
    // 重置网络管理器代理设置
    m_networkManager->setProxy(QNetworkProxy::NoProxy);
}
