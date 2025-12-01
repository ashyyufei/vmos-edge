#include "devicemanager.h"
#include "scrcpy_observer.h"
#include "grid_observer.h"
#include "../helper/XapkInstaller.h"
#include "../../QtScrcpyCore/src/adb/adbprocessimpl.h"
#include <QCoreApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
    , m_deviceManage(qsc::IDeviceManage::getInstance())
{
    connect(&m_deviceManage, &qsc::IDeviceManage::deviceConnected, this, &DeviceManager::onDeviceConnected);
    connect(&m_deviceManage, &qsc::IDeviceManage::deviceDisconnected, this, &DeviceManager::onDeviceDisconnected);
}

DeviceManager::~DeviceManager()
{
}

void DeviceManager::connectDevice(const QString &serial)
{
    qsc::DeviceParams params;
    params.serial = serial;

    // QtScrcpyCore's CMakeLists copies scrcpy-server to the application directory.
    params.serverLocalPath = QCoreApplication::applicationDirPath() + "/scrcpy-server";

    // FIX: Set a random, non-negative scid to ensure socket names match
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;

    // Set recordPath to user Pictures directory / vmosedge
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    params.recordPath = picturesPath + "/vmosedge/";

    // Set some default parameters
    params.maxSize = 0;
    params.bitRate = 8000000;
    // params.maxFps = 60;
    params.logLevel = "warn";
    params.useReverse = true; // 使用 reverse 模式（更高效），失败时自动降级到 forward
    params.stayAwake = false;
    params.closeScreen = false;
    qInfo() << "Connecting to device:" << serial << "with scid:" << params.scid;
    // 字符串参数 (QString/QByteArray)
    qDebug() << "serial:" << params.serial;
    qDebug() << "recordPath:" << params.recordPath;
    qDebug() << "recordFileFormat:" << params.recordFileFormat;
    qDebug() << "serverLocalPath:" << params.serverLocalPath;
    qDebug() << "serverRemotePath:" << params.serverRemotePath;
    qDebug() << "pushFilePath:" << params.pushFilePath;
    qDebug() << "gameScript:" << params.gameScript;
    qDebug() << "serverVersion:" << params.serverVersion;
    qDebug() << "logLevel:" << params.logLevel;
    qDebug() << "codecOptions:" << params.codecOptions;
    qDebug() << "codecName:" << params.codecName;

    // 数值参数 (int/quint32/qint32)
    qDebug() << "maxSize:" << params.maxSize;
    qDebug() << "bitRate:" << params.bitRate;
    qDebug() << "maxFps:" << params.maxFps;
    qDebug() << "captureOrientationLock:" << params.captureOrientationLock;
    qDebug() << "captureOrientation:" << params.captureOrientation;
    qDebug() << "scid (Session ID):" << params.scid;

    // 布尔值参数 (bool)
    qDebug() << "closeScreen:" << (params.closeScreen ? "true" : "false");
    qDebug() << "useReverse:" << (params.useReverse ? "true" : "false");
    qDebug() << "display:" << (params.display ? "true" : "false");
    qDebug() << "renderExpiredFrames:" << (params.renderExpiredFrames ? "true" : "false");
    qDebug() << "stayAwake:" << (params.stayAwake ? "true" : "false");
    qDebug() << "recordFile:" << (params.recordFile ? "true" : "false");

    qDebug() << "--- [End Parameters Log] ---";
    m_deviceManage.connectDevice(params);
}

void DeviceManager::connectDeviceDirectTcp(const QString &serial, const QString &host, quint16 videoPort, quint16 audioPort, quint16 controlPort)
{
    qsc::DeviceParams params;
    params.serial = serial;

    // 启用TCP直接连接模式
    params.useDirectTcp = true;
    params.tcpHost = host.isEmpty() ? "localhost" : host;
    params.tcpVideoPort = videoPort;
    params.tcpAudioPort = audioPort;
    params.tcpControlPort = controlPort;

    // QtScrcpyCore's CMakeLists copies scrcpy-server to the application directory.
    // 在直接TCP模式下，serverLocalPath可以留空，因为不需要推送server
    params.serverLocalPath = QCoreApplication::applicationDirPath() + "/scrcpy-server";

    // Set a random, non-negative scid
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;

    // Set recordPath to user Pictures directory / vmosedge
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    params.recordPath = picturesPath + "/vmosedge/";

    // Set some default parameters
    params.maxSize = 0;
    params.bitRate = 4000000;
    params.logLevel = "warn";
    params.useReverse = false; // 直接TCP模式不使用reverse
    params.stayAwake = false;
    params.closeScreen = false;

    qInfo() << "Connecting to device via direct TCP:" << serial 
            << "host:" << params.tcpHost 
            << "video port:" << params.tcpVideoPort
            << "control port:" << params.tcpControlPort
            << "audio port:" << params.tcpAudioPort;
    
    qDebug() << "TCP Direct Connect Parameters:";
    qDebug() << "  serial:" << params.serial;
    qDebug() << "  tcpHost:" << params.tcpHost;
    qDebug() << "  tcpVideoPort:" << params.tcpVideoPort;
    qDebug() << "  tcpAudioPort:" << params.tcpAudioPort;
    qDebug() << "  tcpControlPort:" << params.tcpControlPort;
    qDebug() << "  useDirectTcp:" << (params.useDirectTcp ? "true" : "false");

    m_deviceManage.connectDevice(params);
}

void DeviceManager::disconnectDevice(const QString &serial)
{
    qInfo() << "Disconnecting from device:" << serial;
    m_deviceManage.disconnectDevice(serial);
}

bool DeviceManager::hasDevice(const QString &serial) const
{
    return !const_cast<qsc::IDeviceManage&>(m_deviceManage).getDevice(serial).isNull();
}

static inline QPointer<qsc::IDevice> getDev(qsc::IDeviceManage &mgr, const QString &serial) {
    auto dev = mgr.getDevice(serial);
    if (dev.isNull()) {
        // 设备不存在是正常情况（可能正在连接中或连接失败），使用debug级别而不是warning
        // 这样可以避免在正常连接流程中产生大量警告日志
        qDebug() << "Device not found (may be connecting or failed):" << serial;
    }
    return dev;
}

bool DeviceManager::setUserData(const QString &serial, QObject *userData)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev.isNull()) return false;
    dev->setUserData(static_cast<void*>(userData));
    return true;
}

QObject* DeviceManager::getUserData(const QString &serial) const
{
    auto dev = const_cast<qsc::IDeviceManage&>(m_deviceManage).getDevice(serial);
    if (dev.isNull()) return nullptr;
    return static_cast<QObject*>(dev->getUserData());
}

void DeviceManager::goBack(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postGoBack();
}

void DeviceManager::goHome(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postGoHome();
}

void DeviceManager::goMenu(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postGoMenu();
}

void DeviceManager::appSwitch(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postAppSwitch();
}

void DeviceManager::power(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postPower();
}

void DeviceManager::volumeUp(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postVolumeUp();
}

void DeviceManager::volumeDown(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->postVolumeDown();
}

void DeviceManager::textInput(const QString &serial, const QString &text)
{
    auto dev = getDev(m_deviceManage, serial);
    if (!dev) return;
    QString t = text;
    dev->postTextInput(t);
}

void DeviceManager::pushFile(const QString &serial, const QString &file, const QString &devicePath)
{
    // 调用重载方法，不传递adbDeviceAddress，使用device->pushFileRequest
    pushFile(serial, file, devicePath, QString());
}

void DeviceManager::pushFile(const QString &serial, const QString &file, const QString &devicePath, const QString &adbDeviceAddress)
{
    qDebug() << "DeviceManager::pushFile - Called with serial:" << serial 
             << "file:" << file 
             << "devicePath:" << devicePath
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    // 如果提供了adbDeviceAddress，说明是TCP直连模式，需要直接使用ADB命令
    if (!adbDeviceAddress.isEmpty()) {
        qDebug() << "DeviceManager::pushFile - Using TCP direct mode, will connect ADB first";
        // 检查文件是否存在
        QFileInfo fileInfo(file);
        if (!fileInfo.exists()) {
            qWarning() << "DeviceManager::pushFile - File does not exist:" << file;
            return;
        }
        
        // 使用绝对路径，避免路径问题
        QString absoluteFilePath = fileInfo.absoluteFilePath();
        QString originalFileName = fileInfo.fileName();
        
        // Windows上处理中文路径：转换为短路径名（8.3格式）以避免编码问题
#ifdef Q_OS_WIN
        // 尝试获取短路径名（8.3格式），如果失败则使用原路径
        QString shortPath = absoluteFilePath;
        QByteArray pathBytes = absoluteFilePath.toLocal8Bit();
        char shortPathBuffer[MAX_PATH];
        DWORD result = GetShortPathNameA(pathBytes.constData(), shortPathBuffer, MAX_PATH);
        if (result > 0 && result < MAX_PATH) {
            shortPath = QString::fromLocal8Bit(shortPathBuffer);
            qDebug() << "DeviceManager::pushFile - Converted to short path:" << shortPath << "from:" << absoluteFilePath;
        } else {
            qWarning() << "DeviceManager::pushFile - Failed to get short path, using original path";
        }
        absoluteFilePath = shortPath;
#endif
        
        QString finalDevicePath = devicePath;
        if (finalDevicePath.isEmpty()) {
            finalDevicePath = "/sdcard/Download";
        }

        // 修复文件名称改变的问题：
        // 当源文件路径被转换为短路径时，如果目标路径是目录，ADB会使用短文件名作为目标文件名。
        // 因此我们需要明确指定目标文件名。
        // 如果路径以/结尾，或者没有后缀（认为是目录），则追加原始文件名。
        if (finalDevicePath.endsWith("/") || QFileInfo(finalDevicePath).suffix().isEmpty()) {
            if (!finalDevicePath.endsWith("/")) {
                finalDevicePath += "/";
            }
            finalDevicePath += originalFileName;
        }

        // 使用重试机制连接ADB
        connectAdbForPushFileWithRetry(serial, adbDeviceAddress, absoluteFilePath, finalDevicePath, 0);
    } else {
        // adbDeviceAddress为空，检查设备对象是否存在
        qDebug() << "DeviceManager::pushFile - adbDeviceAddress is empty, checking device object for serial:" << serial;
        auto dev = getDev(m_deviceManage, serial);
        if (dev) {
            // 设备对象存在，可能是ADB模式，使用设备对象的pushFileRequest方法
            qDebug() << "DeviceManager::pushFile - Device object found, calling pushFileRequest";
            dev->pushFileRequest(file, devicePath.isEmpty() ? "/sdcard/Download" : devicePath);
        } else {
            qWarning() << "DeviceManager::pushFile - Cannot push file: device object not found for serial:" << serial
                       << "and adbDeviceAddress is empty. In TCP direct mode, please provide adbDeviceAddress (ip:port).";
        }
    }
}

void DeviceManager::installApk(const QString &serial, const QString &apkFile)
{
    // 调用重载方法，不传递adbDeviceAddress，使用device->installApkRequest
    installApk(serial, apkFile, QString());
}

void DeviceManager::installApk(const QString &serial, const QString &apkFile, const QString &adbDeviceAddress)
{
    qDebug() << "DeviceManager::installApk - Called with serial:" << serial 
             << "apkFile:" << apkFile 
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    // 如果提供了adbDeviceAddress，说明是TCP直连模式，需要直接使用ADB命令
    if (!adbDeviceAddress.isEmpty()) {
        qDebug() << "DeviceManager::installApk - Using TCP direct mode, will connect ADB first";
        // 检查文件是否存在
        QFileInfo fileInfo(apkFile);
        if (!fileInfo.exists()) {
            qWarning() << "DeviceManager::installApk - APK file does not exist:" << apkFile;
            return;
        }
        
        // 使用绝对路径，避免路径问题
        QString absoluteApkPath = fileInfo.absoluteFilePath();
        
        // Windows上处理中文路径：转换为短路径名（8.3格式）以避免编码问题
#ifdef Q_OS_WIN
        // 尝试获取短路径名（8.3格式），如果失败则使用原路径
        QString shortPath = absoluteApkPath;
        QByteArray pathBytes = absoluteApkPath.toLocal8Bit();
        char shortPathBuffer[MAX_PATH];
        DWORD result = GetShortPathNameA(pathBytes.constData(), shortPathBuffer, MAX_PATH);
        if (result > 0 && result < MAX_PATH) {
            shortPath = QString::fromLocal8Bit(shortPathBuffer);
            qDebug() << "DeviceManager::installApk - Converted to short path:" << shortPath << "from:" << absoluteApkPath;
        } else {
            qWarning() << "DeviceManager::installApk - Failed to get short path, using original path";
        }
        absoluteApkPath = shortPath;
#endif
        
        // 参考server.cpp的状态机实现，使用重试机制连接ADB
        connectAdbWithRetry(serial, adbDeviceAddress, absoluteApkPath, 0);
    } else {
        // adbDeviceAddress为空，检查设备对象是否存在
        // 注意：在TCP直连模式下，应该使用ip:port作为设备地址，而不是serial（dbId）
        qDebug() << "DeviceManager::installApk - adbDeviceAddress is empty, checking device object for serial:" << serial;
        auto dev = getDev(m_deviceManage, serial);
        if (dev) {
            // 设备对象存在，可能是ADB模式，使用设备对象的installApkRequest方法
            // 注意：这个方法会使用设备的serial（可能是dbId），在TCP直连模式下可能不正确
            qDebug() << "DeviceManager::installApk - Device object found, calling installApkRequest (may fail in TCP direct mode)";
            dev->installApkRequest(apkFile);
        } else {
            qWarning() << "DeviceManager::installApk - Cannot install APK: device object not found for serial:" << serial
                       << "and adbDeviceAddress is empty. In TCP direct mode, please provide adbDeviceAddress (ip:port).";
        }
    }
}

void DeviceManager::installXapk(const QString &serial, const QString &xapkFile)
{
    // 调用重载方法，不传递adbDeviceAddress，使用serial作为ADB设备地址
    installXapk(serial, xapkFile, QString());
}

void DeviceManager::installXapk(const QString &serial, const QString &xapkFile, const QString &adbDeviceAddress)
{
    auto dev = getDev(m_deviceManage, serial);
    if (!dev) {
        qWarning() << "DeviceManager::installXapk - device not found for serial:" << serial;
        return;
    }
    
    // 检查是否已有正在进行的安装
    if (m_xapkInstallers.contains(serial)) {
        qWarning() << "DeviceManager::installXapk - XAPK installation already in progress for serial:" << serial;
        return;
    }
    
    // 确定ADB设备地址：如果提供了adbDeviceAddress则使用它，否则使用serial
    QString finalAdbDeviceAddress = adbDeviceAddress.isEmpty() ? serial : adbDeviceAddress;
    
    // 如果提供了adbDeviceAddress，说明是TCP直连模式，需要先连接ADB
    if (!adbDeviceAddress.isEmpty()) {
        // 使用与APK安装相同的连接逻辑
        connectXapkAdbWithRetry(serial, finalAdbDeviceAddress, xapkFile, dev, 0);
    } else {
        // 直接启动安装（ADB模式，设备已连接）
        startXapkInstallation(serial, xapkFile, dev, finalAdbDeviceAddress);
    }
}

void DeviceManager::startXapkInstallation(const QString &serial, const QString &xapkFile, QPointer<qsc::IDevice> dev, const QString &adbDeviceAddress)
{
    // 创建XAPK安装器
    XapkInstaller* installer = new XapkInstaller(this);
    m_xapkInstallers.insert(serial, installer);
    
    // 连接信号
    connect(installer, &XapkInstaller::finished, this, [this, serial, installer](bool success, const QString& message) {
        qDebug() << "DeviceManager::installXapk - Installation finished for serial:" << serial 
                 << "success:" << success << "message:" << message;
        
        // 清理安装器
        m_xapkInstallers.remove(serial);
        installer->deleteLater();
        
        // 可以在这里发送通知信号（如果需要）
        if (!success) {
            qWarning() << "DeviceManager::installXapk - Installation failed:" << message;
        }
    });
    
    // 获取ADB路径（使用AdbProcessImpl）
    QString adbPath = AdbProcessImpl::getAdbPath();
    
    qDebug() << "DeviceManager::installXapk - Starting XAPK installation, serial:" << serial 
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    // 开始安装
    installer->installXapk(xapkFile, dev, adbPath, adbDeviceAddress);
}

void DeviceManager::setDisplayPower(const QString &serial, bool on)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->setDisplayPower(on);
}

void DeviceManager::expandNotificationPanel(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->expandNotificationPanel();
}

void DeviceManager::collapsePanel(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->collapsePanel();
}

void DeviceManager::requestDeviceClipboard(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->requestDeviceClipboard();
}

void DeviceManager::setDeviceClipboard(const QString &serial, bool pause)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->setDeviceClipboard(pause);
}

void DeviceManager::clipboardPaste(const QString &serial)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->clipboardPaste();
}

void DeviceManager::showTouch(const QString &serial, bool show)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev) dev->showTouch(show);
}

void DeviceManager::onDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size)
{
    if (success) {
        qInfo() << "Device connected:" << deviceName << size;
        emit deviceConnected(serial, deviceName, size);
    } else {
        qWarning() << "Device connect failed:" << serial;
        emit deviceConnectFailed(serial);
    }
}

void DeviceManager::onDeviceDisconnected(const QString &serial)
{
    qInfo() << "Device disconnected:" << serial;
    emit deviceDisconnected(serial);
}

// input helpers
static inline QEvent::Type toMouseType(int type) {
    switch (type) {
    case QEvent::MouseButtonPress: return QEvent::MouseButtonPress;
    case QEvent::MouseButtonRelease: return QEvent::MouseButtonRelease;
    case QEvent::MouseButtonDblClick: return QEvent::MouseButtonDblClick;
    case QEvent::MouseMove: return QEvent::MouseMove;
    default: return QEvent::MouseMove;
    }
}

void DeviceManager::sendMouseEvent(const QString &serial, int type, int x, int y, int button, int buttons, int modifiers,
                                   int frameWidth, int frameHeight, int showWidth, int showHeight)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (dev.isNull()) {
        qWarning() << "DeviceManager::sendMouseEvent - device is null for serial:" << serial;
        return;
    }
    
    QEvent::Type eventType = toMouseType(type);
    QPointF pos(x, y);
    Qt::MouseButton btn = static_cast<Qt::MouseButton>(button);
    Qt::MouseButtons btns = static_cast<Qt::MouseButtons>(buttons);
    Qt::KeyboardModifiers mods = static_cast<Qt::KeyboardModifiers>(modifiers);
    
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    // Qt 6 constructor: (QEvent::Type type, const QPointF &localPos, const QPointF &screenPos, 
    //                   Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
    QMouseEvent ev(eventType, pos, pos, btn, btns, mods);
#else
    // Qt 5 constructor: (QEvent::Type type, const QPointF &localPos, Qt::MouseButton button, 
    //                   Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
    QMouseEvent ev(eventType, pos, btn, btns, mods);
#endif
    
    qDebug() << "DeviceManager::sendMouseEvent - serial:" << serial 
             << "type:" << eventType << "(" << type << ")" << "pos:" << pos 
             << "button:" << btn << "buttons:" << btns
             << "frameSize:" << QSize(frameWidth, frameHeight) << "showSize:" << QSize(showWidth, showHeight);
    
    dev->mouseEvent(&ev, QSize(frameWidth, frameHeight), QSize(showWidth, showHeight));
}

void DeviceManager::sendWheelEvent(const QString &serial, int deltaX, int deltaY, int x, int y, int modifiers,
                                   int frameWidth, int frameHeight, int showWidth, int showHeight)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (dev.isNull()) return;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QPoint pixelDelta(0, 0);
    QPoint angleDelta(deltaX, deltaY);
    QWheelEvent ev(QPointF(x, y), QPointF(x, y), pixelDelta, angleDelta, Qt::NoButton, static_cast<Qt::KeyboardModifiers>(modifiers), Qt::NoScrollPhase, false);
#else
    QWheelEvent ev(QPoint(x, y), deltaY, Qt::NoButton, static_cast<Qt::KeyboardModifiers>(modifiers), Qt::Vertical);
#endif
    dev->wheelEvent(&ev, QSize(frameWidth, frameHeight), QSize(showWidth, showHeight));
}

void DeviceManager::sendKeyEvent(const QString &serial, int type, int key, int modifiers, const QString &text)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (dev.isNull()) return;
    QKeyEvent ev(static_cast<QEvent::Type>(type), key, static_cast<Qt::KeyboardModifiers>(modifiers), text);
    dev->keyEvent(&ev, QSize(), QSize());
}

void DeviceManager::screenshot(const QString &serial)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (!dev.isNull()) dev->screenshot();
}

bool DeviceManager::registerObserver(const QString &serial)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (dev.isNull()) return false;
    if (m_observers.contains(serial)) return true;
    auto ob = QSharedPointer<ScrcpyObserver>::create(this, serial);
    m_observers.insert(serial, ob);
    dev->registerDeviceObserver(ob.data());
    
    // 连接 ScrcpyObserver 的信号到 DeviceManager 的转发方法
    // 这样 QML 可以通过 DeviceManager 的信号接收事件
    connect(ob.data(), &ScrcpyObserver::screenInfo, this, [this, serial](int width, int height) {
        emitScreenInfo(serial, width, height);
    });
    connect(ob.data(), &ScrcpyObserver::fpsUpdated, this, [this, serial](int fps) {
        emitFpsUpdated(serial, fps);
    });
    connect(ob.data(), &ScrcpyObserver::grabCursorChanged, this, [this, serial](bool grab) {
        emitGrabCursorChanged(serial, grab);
    });
    
    return true;
}

void DeviceManager::deRegisterObserver(const QString &serial)
{
    auto dev = m_deviceManage.getDevice(serial);
    if (dev.isNull()) return;
    auto it = m_observers.find(serial);
    if (it != m_observers.end()) {
        dev->deRegisterDeviceObserver(it->data());
        m_observers.erase(it);
    }
}

QObject* DeviceManager::getObserver(const QString &serial) const
{
    auto it = m_observers.find(serial);
    if (it != m_observers.end()) {
        return it->data();  // ScrcpyObserver 现在继承自 QObject，可以直接返回
    }
    return nullptr;
}

bool DeviceManager::registerDeviceObserver(const QString &serial, QObject *observer)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev.isNull()) return false;
    
    // 尝试将 QObject* 转换为 qsc::DeviceObserver*
    // GridObserver 和 ScrcpyObserver 都继承自 QObject 和 qsc::DeviceObserver
    qsc::DeviceObserver* deviceObserver = qobject_cast<GridObserver*>(observer);
    if (!deviceObserver) {
        // 如果不是 GridObserver，尝试 ScrcpyObserver
        deviceObserver = qobject_cast<ScrcpyObserver*>(observer);
    }
    
    if (!deviceObserver) {
        qWarning() << "DeviceManager::registerDeviceObserver - observer is not a valid DeviceObserver";
        return false;
    }
    
    dev->registerDeviceObserver(deviceObserver);
    return true;
}

void DeviceManager::unregisterDeviceObserver(const QString &serial, QObject *observer)
{
    auto dev = getDev(m_deviceManage, serial);
    if (dev.isNull()) return;
    
    // 尝试将 QObject* 转换为 qsc::DeviceObserver*
    qsc::DeviceObserver* deviceObserver = qobject_cast<GridObserver*>(observer);
    if (!deviceObserver) {
        deviceObserver = qobject_cast<ScrcpyObserver*>(observer);
    }
    
    if (!deviceObserver) {
        qWarning() << "DeviceManager::unregisterDeviceObserver - observer is not a valid DeviceObserver";
        return;
    }
    
    dev->deRegisterDeviceObserver(deviceObserver);
}

void DeviceManager::emitNewFrame(const QString &serial, const QImage &frame)
{
    emit newFrame(serial, frame);
}

void DeviceManager::emitFpsUpdated(const QString &serial, int fps)
{
    emit fpsUpdated(serial, fps);
}

void DeviceManager::emitGrabCursorChanged(const QString &serial, bool grab)
{
    emit grabCursorChanged(serial, grab);
}

void DeviceManager::emitScreenInfo(const QString &serial, int width, int height)
{
    emit screenInfo(serial, width, height);
}

// 参考server.cpp的状态机实现，使用重试机制连接ADB
// 直接使用QProcess连接，避免AdbProcess在进程崩溃时的问题
void DeviceManager::connectAdbWithRetry(const QString &serial, const QString &adbDeviceAddress, const QString &apkFile, int retryCount)
{
    qDebug() << "DeviceManager::connectAdbWithRetry - serial:" << serial 
             << "adbDeviceAddress:" << adbDeviceAddress 
             << "retryCount:" << retryCount;
    
    // 设置连接状态
    m_adbConnectStates[serial] = ACS_CONNECTING;
    m_adbConnectRetryCount[serial] = retryCount;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::connectAdbWithRetry - ADB path is empty";
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
        return;
    }
    
    // 直接使用QProcess执行adb connect命令（参考XAPK安装器的方式）
    QProcess* connectProcess = new QProcess(this);
    connectProcess->setProgram(adbPath);
    connectProcess->setArguments(QStringList() << "connect" << adbDeviceAddress);
    
    qDebug() << "DeviceManager::connectAdbWithRetry - Executing:" << adbPath << "connect" << adbDeviceAddress;
    
    connect(connectProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, connectProcess, serial, adbDeviceAddress, apkFile, retryCount](int exitCode, QProcess::ExitStatus exitStatus) {
        
        QString errorOutput = connectProcess->readAllStandardError();
        QString stdOutput = connectProcess->readAllStandardOutput();
        
        bool shouldContinue = false;
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            // 退出码为0，连接成功
            qDebug() << "DeviceManager::connectAdbWithRetry - ADB connected successfully (exit code 0) for serial:" << serial
                     << "output:" << stdOutput;
            shouldContinue = true;
        } else if (exitStatus == QProcess::NormalExit) {
            // 退出码非0，但可能是"already connected"的情况，检查输出
            QString allOutput = (stdOutput + " " + errorOutput).toLower();
            
            // 检查输出中是否包含成功连接的信息
            if (allOutput.contains("connected") || 
                allOutput.contains("already connected") ||
                allOutput.contains("connected to")) {
                qDebug() << "DeviceManager::connectAdbWithRetry - ADB already connected (detected from output) for serial:" << serial
                         << "output:" << stdOutput << "error:" << errorOutput;
                shouldContinue = true;
            } else {
                qWarning() << "DeviceManager::connectAdbWithRetry - ADB connect failed for serial:" << serial 
                           << "adbDeviceAddress:" << adbDeviceAddress
                           << "exitCode:" << exitCode
                           << "error:" << errorOutput
                           << "output:" << stdOutput;
            }
        } else {
            // 进程崩溃或其他错误
            qWarning() << "DeviceManager::connectAdbWithRetry - ADB connect process crashed or failed for serial:" << serial
                       << "adbDeviceAddress:" << adbDeviceAddress
                       << "exitCode:" << exitCode
                       << "exitStatus:" << exitStatus
                       << "error:" << errorOutput
                       << "output:" << stdOutput;
        }
        
        connectProcess->deleteLater();
        
        if (shouldContinue) {
            // 连接成功，验证设备是否真的可用
            verifyAdbConnection(serial, adbDeviceAddress, apkFile);
        } else {
            // 连接失败，检查是否需要重试
            if (retryCount < MAX_RETRY_COUNT) {
                qDebug() << "DeviceManager::connectAdbWithRetry - Retrying ADB connection, retryCount:" << (retryCount + 1);
                // 等待一小段时间后重试（参考server.cpp的重试机制）
                QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, apkFile, retryCount]() {
                    connectAdbWithRetry(serial, adbDeviceAddress, apkFile, retryCount + 1);
                });
            } else {
                qWarning() << "DeviceManager::connectAdbWithRetry - ADB connection failed after" << MAX_RETRY_COUNT << "retries for serial:" << serial
                           << "adbDeviceAddress:" << adbDeviceAddress;
                m_adbConnectStates.remove(serial);
                m_adbConnectRetryCount.remove(serial);
            }
        }
    });
    
    // 启动连接进程
    connectProcess->start();
    if (!connectProcess->waitForStarted(3000)) {
        qWarning() << "DeviceManager::connectAdbWithRetry - Failed to start ADB connect process";
        connectProcess->deleteLater();
        
        // 启动失败也重试
        if (retryCount < MAX_RETRY_COUNT) {
            QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, apkFile, retryCount]() {
                connectAdbWithRetry(serial, adbDeviceAddress, apkFile, retryCount + 1);
            });
        } else {
            m_adbConnectStates.remove(serial);
            m_adbConnectRetryCount.remove(serial);
        }
    }
}

// 验证ADB连接是否真的可用（使用adb devices命令）
void DeviceManager::verifyAdbConnection(const QString &serial, const QString &adbDeviceAddress, const QString &apkFile)
{
    qDebug() << "DeviceManager::verifyAdbConnection - Verifying ADB connection for serial:" << serial
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    m_adbConnectStates[serial] = ACS_VERIFYING;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::verifyAdbConnection - ADB path is empty";
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
        return;
    }
    
    // 使用QProcess执行adb devices命令来验证设备是否真的连接
    QProcess* verifyProcess = new QProcess(this);
    verifyProcess->setProgram(adbPath);
    verifyProcess->setArguments(QStringList() << "devices");
    
    connect(verifyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, verifyProcess, serial, adbDeviceAddress, apkFile](int exitCode, QProcess::ExitStatus exitStatus) {
        
        bool deviceFound = false;
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            QString output = verifyProcess->readAllStandardOutput();
            // 检查输出中是否包含设备地址
            if (output.contains(adbDeviceAddress)) {
                // 进一步检查设备状态是否为"device"（已授权）
                QStringList lines = output.split('\n');
                for (const QString& line : lines) {
                    if (line.contains(adbDeviceAddress) && line.contains("device")) {
                        deviceFound = true;
                        break;
                    }
                }
            }
        }
        
        verifyProcess->deleteLater();
        
        if (deviceFound) {
            qDebug() << "DeviceManager::verifyAdbConnection - Device verified, starting installation for serial:" << serial;
            m_adbConnectStates[serial] = ACS_READY;
            // 等待一小段时间确保设备完全就绪（参考server.cpp的做法）
            QTimer::singleShot(200, this, [this, serial, adbDeviceAddress, apkFile]() {
                startApkInstallationAfterConnect(serial, adbDeviceAddress, apkFile);
                m_adbConnectStates.remove(serial);
                m_adbConnectRetryCount.remove(serial);
            });
        } else {
            qWarning() << "DeviceManager::verifyAdbConnection - Device not found in adb devices list for serial:" << serial
                       << "adbDeviceAddress:" << adbDeviceAddress;
            m_adbConnectStates.remove(serial);
            m_adbConnectRetryCount.remove(serial);
        }
    });
    
    verifyProcess->start();
    if (!verifyProcess->waitForStarted(3000)) {
        qWarning() << "DeviceManager::verifyAdbConnection - Failed to start verify process";
        verifyProcess->deleteLater();
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
    }
}

// 在ADB连接成功后开始安装APK
void DeviceManager::startApkInstallationAfterConnect(const QString &serial, const QString &adbDeviceAddress, const QString &absoluteApkPath)
{
    qDebug() << "DeviceManager::startApkInstallationAfterConnect - Starting APK installation for serial:" << serial
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::startApkInstallationAfterConnect - ADB path is empty, cannot install APK";
        return;
    }
    
    // 构建ADB安装命令
    QStringList adbArgs;
    adbArgs << "-s" << adbDeviceAddress;
    adbArgs << "install" << "-r";
    adbArgs << absoluteApkPath;
    
    qDebug() << "DeviceManager::startApkInstallationAfterConnect - Executing ADB install command:" << adbPath << adbArgs.join(" ");
    
    // 使用QProcess执行安装命令（参考XAPK安装器）
    QProcess* installProcess = new QProcess(this);
    installProcess->setProgram(adbPath);
    installProcess->setArguments(adbArgs);
    
    connect(installProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, installProcess, serial, adbDeviceAddress](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            qDebug() << "DeviceManager::startApkInstallationAfterConnect - APK installed successfully for serial:" << serial;
        } else {
            QString errorOutput = installProcess->readAllStandardError();
            QString stdOutput = installProcess->readAllStandardOutput();
            qWarning() << "DeviceManager::startApkInstallationAfterConnect - APK installation failed for serial:" << serial
                       << "adbDeviceAddress:" << adbDeviceAddress
                       << "exitCode:" << exitCode
                       << "exitStatus:" << exitStatus
                       << "error:" << errorOutput
                       << "output:" << stdOutput;
        }
        installProcess->deleteLater();
    });
    
    // 启动安装进程
    installProcess->start();
    if (!installProcess->waitForStarted(5000)) {
        qWarning() << "DeviceManager::startApkInstallationAfterConnect - Failed to start ADB install process for serial:" << serial;
        installProcess->deleteLater();
    }
}

// XAPK安装的ADB连接（使用与APK安装相同的逻辑）
void DeviceManager::connectXapkAdbWithRetry(const QString &serial, const QString &adbDeviceAddress, const QString &xapkFile, QPointer<qsc::IDevice> dev, int retryCount)
{
    qDebug() << "DeviceManager::connectXapkAdbWithRetry - serial:" << serial 
             << "adbDeviceAddress:" << adbDeviceAddress 
             << "retryCount:" << retryCount;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::connectXapkAdbWithRetry - ADB path is empty";
        return;
    }
    
    // 直接使用QProcess执行adb connect命令
    QProcess* connectProcess = new QProcess(this);
    connectProcess->setProgram(adbPath);
    connectProcess->setArguments(QStringList() << "connect" << adbDeviceAddress);
    
    qDebug() << "DeviceManager::connectXapkAdbWithRetry - Executing:" << adbPath << "connect" << adbDeviceAddress;
    
    connect(connectProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, connectProcess, serial, adbDeviceAddress, xapkFile, dev, retryCount](int exitCode, QProcess::ExitStatus exitStatus) {
        
        QString errorOutput = connectProcess->readAllStandardError();
        QString stdOutput = connectProcess->readAllStandardOutput();
        
        bool shouldContinue = false;
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            qDebug() << "DeviceManager::connectXapkAdbWithRetry - ADB connected successfully (exit code 0) for serial:" << serial;
            shouldContinue = true;
        } else if (exitStatus == QProcess::NormalExit) {
            // 退出码非0，但可能是"already connected"的情况
            QString allOutput = (stdOutput + " " + errorOutput).toLower();
            if (allOutput.contains("connected") || 
                allOutput.contains("already connected") ||
                allOutput.contains("connected to")) {
                qDebug() << "DeviceManager::connectXapkAdbWithRetry - ADB already connected (detected from output) for serial:" << serial;
                shouldContinue = true;
            } else {
                qWarning() << "DeviceManager::connectXapkAdbWithRetry - ADB connect failed for serial:" << serial 
                           << "exitCode:" << exitCode
                           << "error:" << errorOutput
                           << "output:" << stdOutput;
            }
        } else {
            qWarning() << "DeviceManager::connectXapkAdbWithRetry - ADB connect process crashed or failed for serial:" << serial
                       << "exitCode:" << exitCode
                       << "exitStatus:" << exitStatus
                       << "error:" << errorOutput;
        }
        
        connectProcess->deleteLater();
        
        if (shouldContinue) {
            // 连接成功，等待一小段时间后开始安装
            QTimer::singleShot(500, this, [this, serial, xapkFile, dev, adbDeviceAddress]() {
                this->startXapkInstallation(serial, xapkFile, dev, adbDeviceAddress);
            });
        } else {
            // 连接失败，检查是否需要重试
            if (retryCount < MAX_RETRY_COUNT) {
                qDebug() << "DeviceManager::connectXapkAdbWithRetry - Retrying ADB connection, retryCount:" << (retryCount + 1);
                QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, xapkFile, dev, retryCount]() {
                    connectXapkAdbWithRetry(serial, adbDeviceAddress, xapkFile, dev, retryCount + 1);
                });
            } else {
                qWarning() << "DeviceManager::connectXapkAdbWithRetry - ADB connection failed after" << MAX_RETRY_COUNT << "retries for serial:" << serial;
            }
        }
    });
    
    connectProcess->start();
    if (!connectProcess->waitForStarted(3000)) {
        qWarning() << "DeviceManager::connectXapkAdbWithRetry - Failed to start ADB connect process";
        connectProcess->deleteLater();
        
        if (retryCount < MAX_RETRY_COUNT) {
            QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, xapkFile, dev, retryCount]() {
                connectXapkAdbWithRetry(serial, adbDeviceAddress, xapkFile, dev, retryCount + 1);
            });
        }
    }
}

// 文件推送的ADB连接（使用与APK安装相同的逻辑）
void DeviceManager::connectAdbForPushFileWithRetry(const QString &serial, const QString &adbDeviceAddress, const QString &filePath, const QString &devicePath, int retryCount)
{
    qDebug() << "DeviceManager::connectAdbForPushFileWithRetry - serial:" << serial 
             << "adbDeviceAddress:" << adbDeviceAddress 
             << "retryCount:" << retryCount;
    
    // 设置连接状态
    m_adbConnectStates[serial] = ACS_CONNECTING;
    m_adbConnectRetryCount[serial] = retryCount;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::connectAdbForPushFileWithRetry - ADB path is empty";
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
        return;
    }
    
    // 直接使用QProcess执行adb connect命令
    QProcess* connectProcess = new QProcess(this);
    connectProcess->setProgram(adbPath);
    connectProcess->setArguments(QStringList() << "connect" << adbDeviceAddress);
    
    qDebug() << "DeviceManager::connectAdbForPushFileWithRetry - Executing:" << adbPath << "connect" << adbDeviceAddress;
    
    connect(connectProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, connectProcess, serial, adbDeviceAddress, filePath, devicePath, retryCount](int exitCode, QProcess::ExitStatus exitStatus) {
        
        QString errorOutput = connectProcess->readAllStandardError();
        QString stdOutput = connectProcess->readAllStandardOutput();
        
        bool shouldContinue = false;
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            // 退出码为0，连接成功
            qDebug() << "DeviceManager::connectAdbForPushFileWithRetry - ADB connected successfully (exit code 0) for serial:" << serial
                     << "output:" << stdOutput;
            shouldContinue = true;
        } else if (exitStatus == QProcess::NormalExit) {
            // 退出码非0，但可能是"already connected"的情况，检查输出
            QString allOutput = (stdOutput + " " + errorOutput).toLower();
            
            // 检查输出中是否包含成功连接的信息
            if (allOutput.contains("connected") || 
                allOutput.contains("already connected") ||
                allOutput.contains("connected to")) {
                qDebug() << "DeviceManager::connectAdbForPushFileWithRetry - ADB already connected (detected from output) for serial:" << serial
                         << "output:" << stdOutput << "error:" << errorOutput;
                shouldContinue = true;
            } else {
                qWarning() << "DeviceManager::connectAdbForPushFileWithRetry - ADB connect failed for serial:" << serial 
                           << "adbDeviceAddress:" << adbDeviceAddress
                           << "exitCode:" << exitCode
                           << "error:" << errorOutput
                           << "output:" << stdOutput;
            }
        } else {
            // 进程崩溃或其他错误
            qWarning() << "DeviceManager::connectAdbForPushFileWithRetry - ADB connect process crashed or failed for serial:" << serial
                       << "adbDeviceAddress:" << adbDeviceAddress
                       << "exitCode:" << exitCode
                       << "exitStatus:" << exitStatus
                       << "error:" << errorOutput
                       << "output:" << stdOutput;
        }
        
        connectProcess->deleteLater();
        
        if (shouldContinue) {
            // 连接成功，验证设备是否真的可用
            verifyAdbConnectionForPushFile(serial, adbDeviceAddress, filePath, devicePath);
        } else {
            // 连接失败，检查是否需要重试
            if (retryCount < MAX_RETRY_COUNT) {
                qDebug() << "DeviceManager::connectAdbForPushFileWithRetry - Retrying ADB connection, retryCount:" << (retryCount + 1);
                // 等待一小段时间后重试
                QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, filePath, devicePath, retryCount]() {
                    connectAdbForPushFileWithRetry(serial, adbDeviceAddress, filePath, devicePath, retryCount + 1);
                });
            } else {
                qWarning() << "DeviceManager::connectAdbForPushFileWithRetry - ADB connection failed after" << MAX_RETRY_COUNT << "retries for serial:" << serial
                           << "adbDeviceAddress:" << adbDeviceAddress;
                m_adbConnectStates.remove(serial);
                m_adbConnectRetryCount.remove(serial);
            }
        }
    });
    
    // 启动连接进程
    connectProcess->start();
    if (!connectProcess->waitForStarted(3000)) {
        qWarning() << "DeviceManager::connectAdbForPushFileWithRetry - Failed to start ADB connect process";
        connectProcess->deleteLater();
        
        // 启动失败也重试
        if (retryCount < MAX_RETRY_COUNT) {
            QTimer::singleShot(1000, this, [this, serial, adbDeviceAddress, filePath, devicePath, retryCount]() {
                connectAdbForPushFileWithRetry(serial, adbDeviceAddress, filePath, devicePath, retryCount + 1);
            });
        } else {
            m_adbConnectStates.remove(serial);
            m_adbConnectRetryCount.remove(serial);
        }
    }
}

// 验证ADB连接是否真的可用（用于文件推送）
void DeviceManager::verifyAdbConnectionForPushFile(const QString &serial, const QString &adbDeviceAddress, const QString &filePath, const QString &devicePath)
{
    qDebug() << "DeviceManager::verifyAdbConnectionForPushFile - Verifying ADB connection for serial:" << serial
             << "adbDeviceAddress:" << adbDeviceAddress;
    
    m_adbConnectStates[serial] = ACS_VERIFYING;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::verifyAdbConnectionForPushFile - ADB path is empty";
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
        return;
    }
    
    // 使用QProcess执行adb devices命令来验证设备是否真的连接
    QProcess* verifyProcess = new QProcess(this);
    verifyProcess->setProgram(adbPath);
    verifyProcess->setArguments(QStringList() << "devices");
    
    connect(verifyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, verifyProcess, serial, adbDeviceAddress, filePath, devicePath](int exitCode, QProcess::ExitStatus exitStatus) {
        
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            QString output = verifyProcess->readAllStandardOutput();
            // 检查输出中是否包含设备地址
            if (output.contains(adbDeviceAddress)) {
                qDebug() << "DeviceManager::verifyAdbConnectionForPushFile - Device found in adb devices list for serial:" << serial
                         << "adbDeviceAddress:" << adbDeviceAddress;
                // 验证成功，开始推送文件
                startPushFileAfterConnect(serial, adbDeviceAddress, filePath, devicePath);
                m_adbConnectStates.remove(serial);
                m_adbConnectRetryCount.remove(serial);
            } else {
                qWarning() << "DeviceManager::verifyAdbConnectionForPushFile - Device not found in adb devices list for serial:" << serial
                           << "adbDeviceAddress:" << adbDeviceAddress;
                m_adbConnectStates.remove(serial);
                m_adbConnectRetryCount.remove(serial);
            }
        } else {
            qWarning() << "DeviceManager::verifyAdbConnectionForPushFile - Failed to verify ADB connection for serial:" << serial;
            m_adbConnectStates.remove(serial);
            m_adbConnectRetryCount.remove(serial);
        }
        
        verifyProcess->deleteLater();
    });
    
    verifyProcess->start();
    if (!verifyProcess->waitForStarted(3000)) {
        qWarning() << "DeviceManager::verifyAdbConnectionForPushFile - Failed to start verify process";
        verifyProcess->deleteLater();
        m_adbConnectStates.remove(serial);
        m_adbConnectRetryCount.remove(serial);
    }
}

// 在ADB连接成功后开始推送文件
void DeviceManager::startPushFileAfterConnect(const QString &serial, const QString &adbDeviceAddress, const QString &absoluteFilePath, const QString &devicePath)
{
    qDebug() << "DeviceManager::startPushFileAfterConnect - Starting file push for serial:" << serial
             << "adbDeviceAddress:" << adbDeviceAddress
             << "file:" << absoluteFilePath
             << "devicePath:" << devicePath;
    
    QString adbPath = AdbProcessImpl::getAdbPath();
    if (adbPath.isEmpty()) {
        qWarning() << "DeviceManager::startPushFileAfterConnect - ADB path is empty, cannot push file";
        return;
    }
    
    // 构建ADB推送命令
    QStringList adbArgs;
    adbArgs << "-s" << adbDeviceAddress;
    adbArgs << "push" << absoluteFilePath;
    adbArgs << (devicePath.isEmpty() ? "/sdcard/Download" : devicePath);
    
    qDebug() << "DeviceManager::startPushFileAfterConnect - Executing ADB push command:" << adbPath << adbArgs.join(" ");
    
    // 使用QProcess执行推送命令
    QProcess* pushProcess = new QProcess(this);
    pushProcess->setProgram(adbPath);
    pushProcess->setArguments(adbArgs);
    
    connect(pushProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pushProcess, serial, adbDeviceAddress, absoluteFilePath](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            qDebug() << "DeviceManager::startPushFileAfterConnect - File pushed successfully for serial:" << serial
                     << "file:" << absoluteFilePath;
        } else {
            QString errorOutput = pushProcess->readAllStandardError();
            QString stdOutput = pushProcess->readAllStandardOutput();
            qWarning() << "DeviceManager::startPushFileAfterConnect - File push failed for serial:" << serial
                       << "adbDeviceAddress:" << adbDeviceAddress
                       << "file:" << absoluteFilePath
                       << "exitCode:" << exitCode
                       << "exitStatus:" << exitStatus
                       << "error:" << errorOutput
                       << "output:" << stdOutput;
        }
        pushProcess->deleteLater();
    });
    
    // 启动推送进程
    pushProcess->start();
    if (!pushProcess->waitForStarted(5000)) {
        qWarning() << "DeviceManager::startPushFileAfterConnect - Failed to start ADB push process for serial:" << serial;
        pushProcess->deleteLater();
    }
}
