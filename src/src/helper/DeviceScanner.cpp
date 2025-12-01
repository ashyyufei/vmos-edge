
#include "DeviceScanner.h"
#include <QNetworkAddressEntry>
#include <QDateTime>
#include <QAbstractSocket>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QMap>

DeviceScanner::DeviceScanner(QObject *parent)
    : QObject(parent),
      m_udpSocket(new QUdpSocket(this)),
      m_scanTimer(new QTimer(this)),
      m_checkTimer(new QTimer(this)),
      m_scanning(false),
      m_lastDeviceFoundTime(0)
{
    // 连接UDP套接字的readyRead信号，当有数据可读时触发
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &DeviceScanner::onReadyRead);
    
    // 连接定时器的timeout信号，当扫描超时时触发
    connect(m_scanTimer, &QTimer::timeout, this, &DeviceScanner::onScanTimeout);
    
    // 连接检查定时器，定期检查待处理的数据包（每30ms检查一次）
    // 参考Python脚本的主动接收方式，确保不遗漏响应
    // 进一步提高检查频率，确保不同网段的设备响应都能及时接收
    connect(m_checkTimer, &QTimer::timeout, this, &DeviceScanner::onCheckPendingDatagrams);
    m_checkTimer->setInterval(30); // 30ms检查一次，进一步提高频率
    m_checkTimer->setSingleShot(false);
}

bool DeviceScanner::scanning() const
{
    return m_scanning;
}

QVariantList DeviceScanner::discoveredDevices() const
{
    return m_discoveredDevices;
}

void DeviceScanner::startDiscovery(int timeout)
{
    if (m_scanning) {
        qDebug() << "DeviceScanner: Already scanning, ignoring startDiscovery request";
        return;
    }

    // 启动计时器
    m_scanElapsedTimer.start();
    
    qWarning() << "======================================== DeviceScanner: START DISCOVERY ========================================";
    qWarning() << "DeviceScanner: timeout:" << timeout << "ms, Start time:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    qDebug() << "DeviceScanner: ========== startDiscovery() called, timeout:" << timeout << "ms";
    qDebug() << "DeviceScanner: Start time:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    emit discoveryStarted();

    // 1. 清理上一次的扫描结果
    m_discoveredDevices.clear();
    m_foundDeviceIds.clear();
    m_pendingIps.clear(); // 确保清理，用于区分扫描模式
    emit discoveredDevicesChanged();

    // 2. 更新状态并绑定端口
    m_scanning = true;
    emit scanningChanged();
    
    // 确保套接字处于未绑定状态，先关闭再绑定
    // UDP套接字的close()是同步的，不需要等待
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "DeviceScanner: Closing existing socket, previous state:" << m_udpSocket->state();
        m_udpSocket->close();
    }
    
    // 绑定到任意IPv4地址以接收响应，使用共享地址选项
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "DeviceScanner: Failed to bind UDP socket:" << m_udpSocket->errorString();
        m_scanning = false;
        emit scanningChanged();
        emit discoveryFinished();
        return;
    }
    
    qDebug() << "DeviceScanner: UDP socket bound successfully, state:" << m_udpSocket->state() 
             << ", local port:" << m_udpSocket->localPort();

    // 让事件循环处理一下，确保绑定完成
    QCoreApplication::processEvents();

    // 3. 遍历所有网段并发送探测包
    QList<QHostAddress> targets = getTargetHosts();
    qDebug() << "DeviceScanner: Found" << targets.size() << "target IPs to scan";
    
    // 统计网段分布
    QMap<QString, int> networkStats;
    for (const QHostAddress &host : targets) {
        QString ipStr = host.toString();
        QString network = ipStr.left(ipStr.lastIndexOf('.'));
        networkStats[network]++;
    }
    qWarning() << "DeviceScanner: Network distribution:";
    for (auto it = networkStats.begin(); it != networkStats.end(); ++it) {
        qWarning() << "DeviceScanner:   " << it.key() << ".x:" << it.value() << "IPs";
    }
    
    QByteArray probeData = "lgcloud";
    int sentCount = 0;
    qint64 sendStartTime = m_scanElapsedTimer.elapsed();
    
    // 分批发送，每批之间处理事件循环，确保数据包真正发送出去
    // 减小批次大小，增加事件处理频率，减少UI卡顿
    const int batchSize = 20; // 每批发送20个，减少批次大小以更频繁地处理事件
    for (int i = 0; i < targets.size(); i += batchSize) {
        int endIndex = qMin(i + batchSize, targets.size());
        for (int j = i; j < endIndex; ++j) {
            const QHostAddress &host = targets[j];
            qint64 bytesWritten = m_udpSocket->writeDatagram(probeData, host, 7678);
            if (bytesWritten > 0) {
                sentCount++;
            } else {
                qWarning() << "DeviceScanner: Failed to send probe to" << host.toString() << ":" << m_udpSocket->errorString();
            }
        }
        // 每批发送后处理事件循环，确保数据包真正发送，同时让UI保持响应
        // 使用ExcludeUserInputEvents避免处理用户输入，但允许其他事件（如绘制）处理
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
    }
    
    qint64 sendEndTime = m_scanElapsedTimer.elapsed();
    qWarning() << "DeviceScanner: Sent" << sentCount << "probe packets to port 7678 in" << (sendEndTime - sendStartTime) << "ms";
    
    // 发送完所有包后，处理事件循环确保数据包发送，但减少阻塞时间
    // 移除长时间的msleep，改用更短的时间间隔，让UI保持响应
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        // 使用更短的延迟，避免长时间阻塞UI
        QThread::msleep(5); // 从20ms减少到5ms，总共只阻塞25ms而不是200ms
    }

    // 4. 启动检查定时器（立即开始检查）
    // 先启动检查定时器，给设备一些响应时间
    m_checkTimer->start();
    
    // 优化：缩短初始等待时间，减少阻塞，让UI保持响应
    // 根据日志，所有设备都在48ms内响应，但我们需要给跨网段设备一些响应时间
    qint64 waitStartTime = m_scanElapsedTimer.elapsed();
    qWarning() << "DeviceScanner: Waiting for initial responses (100ms for quick devices)...";
    int responseCountDuringWait = 0;
    m_lastDeviceFoundTime = waitStartTime; // 初始化最后发现设备的时间
    // 减少等待次数和每次等待时间，总共等待100ms而不是200ms，减少UI卡顿
    for (int i = 0; i < 10; ++i) {
        QThread::msleep(10); // 每次只等待10ms，总共100ms，但更频繁地处理事件
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10); // 处理可能已经到达的响应
        // 每次等待后检查是否有响应到达，主动处理
        if (m_udpSocket->hasPendingDatagrams()) {
            int beforeCount = m_discoveredDevices.size();
            onReadyRead();
            int afterCount = m_discoveredDevices.size();
            if (afterCount > beforeCount) {
                responseCountDuringWait++;
                m_lastDeviceFoundTime = m_scanElapsedTimer.elapsed(); // 更新最后发现时间
                qWarning() << "DeviceScanner: [Wait Phase@" << m_scanElapsedTimer.elapsed() << "ms] Found device, total:" << afterCount;
            }
        }
    }
    qint64 waitEndTime = m_scanElapsedTimer.elapsed();
    qWarning() << "DeviceScanner: Initial wait completed in" << (waitEndTime - waitStartTime) << "ms, received" << responseCountDuringWait << "responses during wait";
    qWarning() << "DeviceScanner: Devices found so far:" << m_discoveredDevices.size();
    
    // 现在启动超时定时器，实际总等待时间 = 200ms + timeout
    // 但通过动态提前退出机制，如果连续500ms无新响应，会提前结束
    m_scanTimer->start(timeout);
    qWarning() << "DeviceScanner: Scan timer started, will timeout in" << timeout << "ms (after initial 200ms wait, total ~" << (timeout + 200) << "ms, may exit early if no new devices)";
}

void DeviceScanner::startDiscoveryWithIps(const QString& ipList, int timeout)
{
    if (m_scanning) {
        return;
    }

    // 启动计时器
    m_scanElapsedTimer.start();

    emit discoveryStarted();

    // 1. 清理上一次的扫描结果
    m_discoveredDevices.clear();
    m_foundDeviceIds.clear();
    m_pendingIps.clear();
    emit discoveredDevicesChanged();

    // 2. 更新状态并绑定端口
    m_scanning = true;
    emit scanningChanged();
    
    // 确保套接字处于未绑定状态，先关闭再绑定
    // UDP套接字的close()是同步的，不需要等待
    if (m_udpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_udpSocket->close();
    }
    
    // 绑定到任意IPv4地址以接收响应，使用共享地址选项
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning() << "DeviceScanner: Failed to bind UDP socket:" << m_udpSocket->errorString();
        m_scanning = false;
        emit scanningChanged();
        emit discoveryFinished();
        return;
    }
    
    qDebug() << "DeviceScanner: UDP socket bound successfully, state:" << m_udpSocket->state();

    // 让事件循环处理一下，确保绑定完成
    QCoreApplication::processEvents();

    // 3. 解析IP列表并发送探测包
    QStringList ips = ipList.split(',', Qt::SkipEmptyParts);
    if (ips.isEmpty()) {
        stopDiscovery();
        return;
    }
    QByteArray probeData = "lgcloud";
    for (const QString &ipString : ips) {
        QString trimmedIp = ipString.trimmed();
        QHostAddress host(trimmedIp);
        if (!host.isNull() && host.protocol() == QAbstractSocket::IPv4Protocol) {
            m_udpSocket->writeDatagram(probeData, host, 7678);
            m_pendingIps.insert(trimmedIp);
        } else {
            qWarning() << "DeviceScanner: Invalid IP address in list:" << ipString;
        }
    }
    
    // 发送完所有包后，处理事件循环确保数据包发送，但减少阻塞时间
    // 移除长时间的msleep，改用更短的时间间隔，让UI保持响应
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        // 使用更短的延迟，避免长时间阻塞UI
        QThread::msleep(5); // 从20ms减少到5ms，总共只阻塞25ms而不是200ms
    }

    // If no valid IPs were found to scan, stop immediately.
    if (m_pendingIps.isEmpty()) {
        stopDiscovery();
        return;
    }

    // 4. 启动检查定时器（立即开始检查）
    // 先启动检查定时器，给设备一些响应时间
    m_checkTimer->start();
    
    // 优化：缩短初始等待时间，减少阻塞，让UI保持响应
    qDebug() << "DeviceScanner: Waiting for initial responses (100ms for quick devices)...";
    m_lastDeviceFoundTime = m_scanElapsedTimer.elapsed(); // 初始化最后发现设备的时间
    // 减少等待次数和每次等待时间，总共等待100ms而不是200ms，减少UI卡顿
    for (int i = 0; i < 10; ++i) {
        QThread::msleep(10); // 每次只等待10ms，总共100ms，但更频繁地处理事件
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10); // 处理可能已经到达的响应
        // 每次等待后检查是否有响应到达，主动处理
        if (m_udpSocket->hasPendingDatagrams()) {
            int beforeCount = m_discoveredDevices.size();
            onReadyRead();
            int afterCount = m_discoveredDevices.size();
            if (afterCount > beforeCount) {
                m_lastDeviceFoundTime = m_scanElapsedTimer.elapsed(); // 更新最后发现时间
            }
        }
    }
    
    // 现在启动超时定时器，实际总等待时间 = 200ms + timeout
    // 但通过动态提前退出机制，如果连续500ms无新响应，会提前结束
    m_scanTimer->start(timeout);
}

void DeviceScanner::stopDiscovery()
{
    if (!m_scanning) {
        return;
    }

    qint64 elapsedMs = m_scanElapsedTimer.elapsed();
    qWarning() << "======================================== DeviceScanner: STOP ========================================";
    qWarning() << "DeviceScanner: Stop called at" << elapsedMs << "ms, time:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    qWarning() << "DeviceScanner: Final device count:" << m_discoveredDevices.size();
    
    m_scanTimer->stop();
    m_checkTimer->stop(); // 停止检查定时器
    
    // 在关闭套接字前，最后检查一次是否有待处理的数据包
    if (m_udpSocket->hasPendingDatagrams()) {
        qDebug() << "DeviceScanner: [Stop] Processing final pending datagrams before stop";
        int beforeCount = m_discoveredDevices.size();
        onReadyRead();
        int afterCount = m_discoveredDevices.size();
        if (afterCount > beforeCount) {
            qDebug() << "DeviceScanner: [Stop] Found" << (afterCount - beforeCount) << "additional devices in final check";
        }
    }
    
    m_pendingIps.clear();
    m_udpSocket->close(); // close()会解绑端口，下次启动需要重新bind
    m_scanning = false;
    emit scanningChanged();
    emit discoveryFinished();
    
    qWarning() << "DeviceScanner: [Stop] Scan stopped, socket closed, total duration:" << elapsedMs << "ms";
    qWarning() << "DeviceScanner: [Stop] Final device list:";
    for (int i = 0; i < m_discoveredDevices.size(); ++i) {
        QVariantMap device = m_discoveredDevices[i].toMap();
        qWarning() << "DeviceScanner:   [" << (i + 1) << "] IP:" << device["ip"].toString() 
                 << "ID:" << device["id"].toString() << "Name:" << device["name"].toString();
    }
    qWarning() << "======================================== DeviceScanner: END ========================================";
}

void DeviceScanner::onScanTimeout()
{
    qint64 elapsedMs = m_scanElapsedTimer.elapsed();
    qWarning() << "======================================== DeviceScanner: TIMEOUT ========================================";
    qWarning() << "DeviceScanner: Timeout reached at" << elapsedMs << "ms, time:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    
    // 在超时前，最后检查一次是否有待处理的数据包
    // 跨网段的设备可能刚好在超时前响应
    if (m_udpSocket->hasPendingDatagrams()) {
        qDebug() << "DeviceScanner: [Timeout] Processing final datagrams before timeout...";
        int beforeCount = m_discoveredDevices.size();
        onReadyRead();
        int afterCount = m_discoveredDevices.size();
        if (afterCount > beforeCount) {
            qDebug() << "DeviceScanner: [Timeout] Found" << (afterCount - beforeCount) << "additional devices in final check";
        }
        
        // 再给一点时间处理可能还有的响应（最多再等100ms），但减少阻塞时间
        for (int i = 0; i < 10; ++i) {
            QThread::msleep(10); // 每次只等待10ms，总共100ms，但更频繁地处理事件
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
            if (m_udpSocket->hasPendingDatagrams()) {
                beforeCount = m_discoveredDevices.size();
                onReadyRead();
                afterCount = m_discoveredDevices.size();
                if (afterCount > beforeCount) {
                    qDebug() << "DeviceScanner: [Timeout] Found" << (afterCount - beforeCount) << "additional devices in extra wait" << (i + 1);
                }
            } else {
                break; // 没有更多响应了，提前退出
            }
        }
    }
    
    qWarning() << "DeviceScanner: [Timeout] Total devices discovered:" << m_discoveredDevices.size();
    qWarning() << "DeviceScanner: [Timeout] Total scan duration:" << elapsedMs << "ms";
    
    if (!m_pendingIps.isEmpty()) {
        qWarning() << "DeviceScanner: [Timeout] Failed IPs (not responded):" << m_pendingIps.values();
        emit discoveryFailed(m_pendingIps.values());
    }
    
    stopDiscovery();
}

void DeviceScanner::onReadyRead()
{
    bool wasUsingSpecificIps = !m_pendingIps.isEmpty(); // 记录是否使用了 startDiscoveryWithIps()
    bool hasPending = m_udpSocket->hasPendingDatagrams();
    qint64 elapsedMs = m_scanElapsedTimer.elapsed();
    
    if (hasPending) {
        qDebug() << "DeviceScanner: [onReadyRead] At" << elapsedMs << "ms - datagrams pending";
    }
    
    int processedCount = 0;
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderIp;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderIp, &senderPort);
        
        QString senderIpStr = senderIp.toString();
        QString response = QString::fromUtf8(datagram).trimmed();
        qint64 responseTime = m_scanElapsedTimer.elapsed();
        
        // 只有当使用 startDiscoveryWithIps() 时，m_pendingIps 才不为空
        // 此时需要从待响应列表中移除该IP
        if (wasUsingSpecificIps) {
            m_pendingIps.remove(senderIpStr);
        }

        // 过滤掉空响应，避免处理无效数据
        if (response.isEmpty()) {
            qDebug() << "DeviceScanner: [Response@" << responseTime << "ms] Empty response from" << senderIpStr << ":" << senderPort;
            continue;
        }
        
        qDebug() << "DeviceScanner: [Response@" << responseTime << "ms] From" << senderIpStr << ":" << senderPort 
                 << "-" << response;
        
        if (response.startsWith("CBS:")) {
            QStringList parts = response.split(':');
            if (parts.size() >= 3) {
                QString deviceId = parts[1];
                
                // 如果设备ID已存在，则忽略，实现去重
                if (m_foundDeviceIds.contains(deviceId)) {
                    qDebug() << "DeviceScanner: [Response@" << responseTime << "ms] Device" << deviceId << "(" << senderIpStr << ") already found, skipping duplicate";
                    continue;
                }

                m_foundDeviceIds.insert(deviceId);

                QVariantMap device;
                device["ip"] = senderIpStr;
                device["id"] = deviceId;
                device["name"] = parts[2];
                device["type"] = parts[0];
                device["last_scan"] = QDateTime::currentDateTime().toString(Qt::ISODate);

                m_discoveredDevices.append(device);
                processedCount++;
                m_lastDeviceFoundTime = responseTime; // 更新最后发现设备的时间
                qWarning() << "DeviceScanner: *** [Device FOUND@" << responseTime << "ms] *** IP:" << senderIpStr 
                         << "ID:" << deviceId << "Name:" << parts[2] 
                         << "Total discovered:" << m_discoveredDevices.size();
                qDebug() << "DeviceScanner: *** [Device FOUND@" << responseTime << "ms] *** IP:" << senderIpStr 
                         << "ID:" << deviceId << "Name:" << parts[2] 
                         << "Total discovered:" << m_discoveredDevices.size();
                
                emit discoveredDevicesChanged();
                emit deviceFound(device);
            } else {
                qWarning() << "DeviceScanner: [Response@" << responseTime << "ms] Invalid CBS response format from" << senderIpStr 
                          << "- parts count:" << parts.size() << "response:" << response;
            }
        } else {
            qDebug() << "DeviceScanner: [Response@" << responseTime << "ms] Response does not start with 'CBS:' from" << senderIpStr << "- response:" << response;
        }
    }
    
    if (processedCount > 0) {
        qDebug() << "DeviceScanner: [onReadyRead] Processed" << processedCount << "new devices, total:" << m_discoveredDevices.size();
    }

    // 只有当使用 startDiscoveryWithIps() 时（wasUsingSpecificIps 为 true），
    // 且所有待响应的IP都已收到响应时（m_pendingIps 现在为空），才提前停止扫描
    // 对于 startDiscovery()，应该等待超时，以便接收所有可能的响应
    if (m_scanning && wasUsingSpecificIps && m_pendingIps.isEmpty()) {
        qDebug() << "DeviceScanner: All expected IPs responded, stopping scan early";
        stopDiscovery();
    }
}

QList<QHostAddress> DeviceScanner::getTargetHosts()
{
    QList<QHostAddress> hosts;
    // 遍历所有网络接口
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        // 只处理活动的、非回环的接口
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        // 遍历接口上的所有IP地址条目
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            // 只处理IPv4
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                quint32 ip = entry.ip().toIPv4Address();
                quint32 netmask = entry.netmask().toIPv4Address();
                quint32 networkAddr = ip & netmask;
                quint32 broadcastAddr = networkAddr | (~netmask);

                // 从网络地址+1到广播地址-1，都是有效的主机地址
                for (quint32 i = networkAddr + 1; i < broadcastAddr; ++i) {
                    hosts.append(QHostAddress(i));
                }
            }
        }
    }
    return hosts;
}

void DeviceScanner::onCheckPendingDatagrams()
{
    // 定期检查是否有待处理的数据包
    // 参考Python脚本的主动接收方式，确保不遗漏响应
    // 提高检查频率，确保不同网段的设备响应都能及时接收
    if (m_scanning) {
        qint64 elapsedMs = m_scanElapsedTimer.elapsed();
        
        // 优先检查是否有待处理的数据包
        bool foundNewDevice = false;
        if (m_udpSocket->hasPendingDatagrams()) {
            int beforeCount = m_discoveredDevices.size();
            onReadyRead();
            int afterCount = m_discoveredDevices.size();
            if (afterCount > beforeCount) {
                foundNewDevice = true;
                qWarning() << "DeviceScanner: [CheckTimer@" << elapsedMs << "ms] Found" << (afterCount - beforeCount) << "new devices via check timer";
            }
        }
        
        // 即使没有待处理的数据包，也处理一次事件循环
        // 确保readyRead信号能及时触发（某些情况下信号可能被延迟）
        // 注意：不要排除SocketNotifiers，否则readyRead信号无法触发
        // 增加处理时间，确保socket事件能被处理
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
        
        // 处理完事件循环后，再次检查（可能事件循环触发了新的数据包）
        if (m_udpSocket->hasPendingDatagrams()) {
            int beforeCount = m_discoveredDevices.size();
            onReadyRead();
            int afterCount = m_discoveredDevices.size();
            if (afterCount > beforeCount) {
                foundNewDevice = true;
                qWarning() << "DeviceScanner: [CheckTimer@" << elapsedMs << "ms] Found" << (afterCount - beforeCount) << "new devices after event processing";
            }
        }
        
        // 动态提前退出机制：如果已经找到至少一个设备，且已经等待了至少300ms，且连续500ms没有新设备响应，则提前结束扫描
        // 这样可以大幅缩短扫描时间，因为大多数设备都在前100ms内响应
        // 注意：只有在至少找到一个设备时才启用提前退出，避免未找到设备时过早退出
        if (!foundNewDevice && m_discoveredDevices.size() > 0 && elapsedMs >= 300) {
            qint64 timeSinceLastDevice = elapsedMs - m_lastDeviceFoundTime;
            if (timeSinceLastDevice >= 500) {
                qWarning() << "DeviceScanner: [CheckTimer@" << elapsedMs << "ms] No new devices for" << timeSinceLastDevice << "ms, stopping early";
                qWarning() << "DeviceScanner: Early exit: waited" << elapsedMs << "ms, last device found at" << m_lastDeviceFoundTime << "ms, total devices:" << m_discoveredDevices.size();
                stopDiscovery();
            }
        }
    }
}

