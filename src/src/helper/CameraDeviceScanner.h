#ifndef CAMERADEVICESCANNER_H
#define CAMERADEVICESCANNER_H

#include <QObject>
#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>

struct Resolution {
    int width;
    int height;
    QString toString() const { return QString("%1x%2").arg(width).arg(height); }
};

struct CameraDevice {
    QString deviceId;      // 设备ID（Windows: 设备索引或名称，Linux: /dev/video0等）
    QString deviceName;     // 设备名称
    QString description;    // 设备描述
    bool isAvailable;       // 是否可用
    QList<Resolution> supportedResolutions;  // 支持的分辨率列表
    QList<int> supportedFps;  // 支持的帧率列表
};

class CameraDeviceScanner : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum CameraRoles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        DescriptionRole,
        IsAvailableRole
    };

    explicit CameraDeviceScanner(QObject *parent = nullptr);
    
    // QAbstractListModel接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 扫描摄像头设备
    Q_INVOKABLE void scanDevices();
    
    // 获取设备信息
    Q_INVOKABLE QVariantMap getDevice(int index) const;
    
    // 查询设备能力（分辨率和帧率）
    Q_INVOKABLE void queryDeviceCapabilities(int index);
    
    // 获取设备支持的分辨率列表
    Q_INVOKABLE QVariantList getSupportedResolutions(int index) const;
    
    // 获取设备支持的帧率列表
    Q_INVOKABLE QVariantList getSupportedFps(int index) const;

signals:
    void countChanged();
    void scanFinished();
    void scanError(const QString &error);
    void capabilitiesQueried(int index);  // 设备能力查询完成

private:
    void scanWindowsDevices();  // Windows平台扫描
    void scanLinuxDevices();    // Linux平台扫描
    void scanMacDevices();      // macOS平台扫描
    
    // 使用FFmpeg枚举设备（跨平台方法）
    void scanDevicesWithFfmpeg();
    
    // 解析FFmpeg输出
    void parseFfmpegDeviceList(const QString &output, const QString &format);
    
    // 查询设备能力（使用FFmpeg）
    void queryDeviceCapabilitiesWithFfmpeg(int deviceIndex);
    
    // 解析设备能力输出（Windows dshow）
    void parseDshowCapabilities(const QString &output, int deviceIndex);
    
    // 解析设备能力输出（Linux v4l2）
    void parseV4l2Capabilities(const QString &output, int deviceIndex);
    
    // 设置默认能力值
    void setDefaultCapabilities(int deviceIndex);
    
    // 获取FFmpeg路径
    QString getFfmpegPath() const;
    
    QList<CameraDevice> m_devices;
};

#endif // CAMERADEVICESCANNER_H

