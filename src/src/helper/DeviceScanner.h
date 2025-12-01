
#ifndef DEVICESCANNER_H
#define DEVICESCANNER_H

#include <QObject>
#include <QVariantList>
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkInterface>
#include <QSet>
#include <QElapsedTimer>

class DeviceScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList discoveredDevices READ discoveredDevices NOTIFY discoveredDevicesChanged)

public:
    explicit DeviceScanner(QObject *parent = nullptr);

    bool scanning() const;
    QVariantList discoveredDevices() const;

    Q_INVOKABLE void startDiscovery(int timeout = 3000);
    Q_INVOKABLE void startDiscoveryWithIps(const QString& ipList, int timeout = 3000);
    Q_INVOKABLE void stopDiscovery();

signals:
    void scanningChanged();
    void discoveredDevicesChanged();
    void deviceFound(const QVariantMap &device);
    void discoveryStarted();
    void discoveryFinished();
    void discoveryFailed(const QStringList &failedIps);

private slots:
    void onReadyRead();
    void onScanTimeout();
    void onCheckPendingDatagrams(); // 定期检查待处理的数据包

private:
    QList<QHostAddress> getTargetHosts();

    QUdpSocket *m_udpSocket;
    QTimer *m_scanTimer;
    QTimer *m_checkTimer; // 用于定期检查待处理的数据包
    bool m_scanning;
    QVariantList m_discoveredDevices;
    QSet<QString> m_foundDeviceIds; // 用于设备去重
    QSet<QString> m_pendingIps;     // 用于追踪待响应的IP
    QElapsedTimer m_scanElapsedTimer; // 用于记录扫描耗时
    qint64 m_lastDeviceFoundTime;   // 最后一次发现设备的时间（毫秒）
};

#endif // DEVICESCANNER_H
