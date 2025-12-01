#ifndef STRUCTS_H
#define STRUCTS_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>
#include <QDateTime>


enum DeviceRoles {
    ItemTypeRole = Qt::UserRole + 1,
    ItemDataRole,

    // Roles for DeviceData, GroupData, and HostData fields
    AdbRole,
    AospVersionRole,
    CheckedRole,
    CreatedRole,
    DataRole,
    DbIdRole,
    DisplayNameRole,
    DnsRole,
    DpiRole,
    FpsRole,
    GroupIdRole,
    GroupNameRole,
    GroupPadCountRole,
    HeightRole,
    HostIdRole,
    HostIpRole,
    HostNameRole,
    HostPadCountRole,
    IdRole,
    ImageRole,
    IpRole,
    MemoryRole,
    NameRole,
    RefreshRole,
    SelectedRole,
    ShortIdRole,
    StateRole,
    WidthRole,
    UpdateTimeRole,
    TcpVideoPortRole,
    TcpAudioPortRole,
    TcpControlPortRole,
    MacvlanIpRole,
    NetworkModeRole,
    TimeZoneRole,
    LocaleRole
};

// 设备数据结构体
struct DeviceData {
    int groupId;                      // 分组ID
    QString hostId;                   // 主机ID
    int adb;
    QString data;
    QString dbId;                     // 数据库ID，设备整个生命周期中保持不变
    QString dns;
    QString dpi;
    QString fps;
    QString height;
    QString id;
    QString image;
    QString ip;
    int memory;
    QString name;
    QString displayName;
    QString shortId;
    QString state;
    QString created;
    QString width;
    QString aospVersion;
    QString hostIp;
    bool checked;                     // 是否勾选
    bool selected;                    // 是否选定
    bool refresh;                     // 是否需要重连
    int tcpVideoPort;                 // TCP视频流端口
    int tcpAudioPort;                 // TCP音频流端口
    int tcpControlPort;               // TCP控制流端口
    QString macvlanIp;               // Macvlan IP地址
    QString networkMode;              // 网络模式: "macvlan" (直连) 或 "bridge" (桥接)
    QString timezone;                 // 时区
    QString locale;                 // 语言
};

inline bool operator==(const DeviceData& a, const DeviceData& b) {
    return a.groupId == b.groupId &&
           a.hostId == b.hostId &&
           a.adb == b.adb &&
           a.data == b.data &&
           a.dbId == b.dbId &&
           a.dns == b.dns &&
           a.dpi == b.dpi &&
           a.fps == b.fps &&
           a.height == b.height &&
           a.id == b.id &&
           a.image == b.image &&
           a.ip == b.ip &&
           a.memory == b.memory &&
           a.name == b.name &&
           a.displayName == b.displayName &&
           a.shortId == b.shortId &&
           a.state == b.state &&
           a.created == b.created &&
           a.width == b.width &&
           a.aospVersion == b.aospVersion &&
           a.hostIp == b.hostIp &&
           a.checked == b.checked &&
           a.selected == b.selected &&
           a.tcpVideoPort == b.tcpVideoPort &&
           a.tcpAudioPort == b.tcpAudioPort &&
           a.tcpControlPort == b.tcpControlPort &&
           a.macvlanIp == b.macvlanIp &&
           a.networkMode == b.networkMode &&
           a.timezone == b.timezone &&
           a.locale == b.locale;
}

Q_DECLARE_METATYPE(DeviceData)

struct GroupData
{
    int groupId;
    QString groupName;
    int groupPadCount;
};

inline bool operator==(const GroupData& a, const GroupData& b) {
    return a.groupId == b.groupId &&
           a.groupName == b.groupName &&
           a.groupPadCount == b.groupPadCount;
}

Q_DECLARE_METATYPE(GroupData)

struct HostData
{
    int groupId;
    QString hostId;
    QString hostName;
    QString ip;
    int hostPadCount;
    QString updateTime;
    QString state;
    bool selected;
};

inline bool operator==(const HostData& a, const HostData& b) {
    return a.groupId == b.groupId &&
           a.hostId == b.hostId &&
           a.hostName == b.hostName &&
           a.ip == b.ip &&
           a.hostPadCount == b.hostPadCount &&
           a.updateTime == b.updateTime &&
           a.state == b.state &&
           a.selected == b.selected;
}

Q_DECLARE_METATYPE(HostData)

#endif // STRUCTS_H
