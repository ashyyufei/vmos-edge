#ifndef AUDIODEVICESCANNER_H
#define AUDIODEVICESCANNER_H

#include <QObject>
#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>

struct AudioDevice {
    QString deviceId;      // 设备ID
    QString deviceName;    // 设备名称
    QString description;   // 设备描述
    bool isAvailable;      // 是否可用
};

class AudioDeviceScanner : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum AudioRoles {
        DeviceIdRole = Qt::UserRole + 1,
        DeviceNameRole,
        DescriptionRole,
        IsAvailableRole
    };

    explicit AudioDeviceScanner(QObject *parent = nullptr);
    
    // QAbstractListModel接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 扫描音频设备
    Q_INVOKABLE void scanDevices();
    
    // 获取设备信息
    Q_INVOKABLE QVariantMap getDevice(int index) const;

signals:
    void countChanged();
    void scanFinished();
    void scanError(const QString &error);

private:
    // 使用FFmpeg枚举音频设备
    void scanDevicesWithFfmpeg();
    
    // 解析FFmpeg输出
    void parseFfmpegDeviceList(const QString &output, const QString &format);
    
    // 获取FFmpeg路径
    QString getFfmpegPath() const;
    
    QList<AudioDevice> m_devices;
};

#endif // AUDIODEVICESCANNER_H

