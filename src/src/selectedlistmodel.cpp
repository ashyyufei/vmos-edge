#include "selectedlistmodel.h"
#include "treeproxymodel.h"
#include <QDebug>
#include <QMap>
#include <QPair>
#include <QSet>

SelectedListModel::SelectedListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void SelectedListModel::setSourceModel(TreeModel *treeModel)
{
    if (m_sourceModel) {
        disconnect(m_sourceModel, &QAbstractItemModel::modelReset, this, &SelectedListModel::onSourceReset);
        disconnect(m_sourceModel, &QAbstractItemModel::dataChanged, this, &SelectedListModel::onSourceDataChanged);
        disconnect(m_sourceModel, &QAbstractItemModel::rowsInserted, this, &SelectedListModel::onSourceRowsInserted);
        disconnect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &SelectedListModel::onSourceRowsAboutToBeRemoved);
    }

    m_sourceModel = treeModel;

    if (m_sourceModel) {
        connect(m_sourceModel, &QAbstractItemModel::modelReset, this, &SelectedListModel::onSourceReset);
        connect(m_sourceModel, &QAbstractItemModel::dataChanged, this, &SelectedListModel::onSourceDataChanged);
        connect(m_sourceModel, &QAbstractItemModel::rowsInserted, this, &SelectedListModel::onSourceRowsInserted);
        connect(m_sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &SelectedListModel::onSourceRowsAboutToBeRemoved);
        onSourceReset(); // Initial population
    }
}

int SelectedListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_selectedDevices.count();
}

QVariant SelectedListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_selectedDevices.count()) return QVariant();
    const DeviceData &device = m_selectedDevices.at(index.row());
    switch (role) {
        case DeviceRoles::AdbRole: return device.adb;
        case DeviceRoles::CreatedRole: return device.created;
        case DeviceRoles::DataRole: return device.data;
        case DeviceRoles::DisplayNameRole: return device.displayName;
        case DeviceRoles::DnsRole: return device.dns;
        case DeviceRoles::DpiRole: return device.dpi;
        case DeviceRoles::FpsRole: return device.fps;
        case DeviceRoles::GroupIdRole: return device.groupId;
        case DeviceRoles::HeightRole: return device.height;
        case DeviceRoles::HostIdRole: return device.hostId;
        case DeviceRoles::IdRole: return device.id;
        case DeviceRoles::ImageRole: return device.image;
        case DeviceRoles::IpRole: return device.ip;
        case DeviceRoles::MemoryRole: return device.memory;
        case DeviceRoles::NameRole: return device.name;
        case DeviceRoles::ShortIdRole: return device.shortId;
        case DeviceRoles::StateRole: return device.state;
        case DeviceRoles::WidthRole: return device.width;
        case DeviceRoles::CheckedRole: return device.checked;
        case DeviceRoles::SelectedRole: return device.selected;
        case DeviceRoles::RefreshRole: return device.refresh;
        case DeviceRoles::AospVersionRole: return device.aospVersion;
        case DeviceRoles::HostIpRole: return device.hostIp;
        case DeviceRoles::DbIdRole: return device.dbId;
        case DeviceRoles::TcpVideoPortRole: return device.tcpVideoPort;
        case DeviceRoles::TcpAudioPortRole: return device.tcpAudioPort;
        case DeviceRoles::TcpControlPortRole: return device.tcpControlPort;
        case DeviceRoles::MacvlanIpRole: return device.macvlanIp;
        case DeviceRoles::NetworkModeRole: return device.networkMode;
        default: return QVariant();
    }
}

bool SelectedListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_selectedDevices.count())
        return false;

    DeviceData &item = m_selectedDevices[index.row()];

    switch (role) {
    case DeviceRoles::CheckedRole:
        item.checked = value.toBool();
        emit dataChanged(index, index, {role});
        return true;
    case DeviceRoles::SelectedRole:
        item.selected = value.toBool();
        emit dataChanged(index, index, {role});
        return true;
    case DeviceRoles::RefreshRole:
        item.refresh = value.toBool();
        emit dataChanged(index, index, {role});
        return true;
    default:
        // For other roles, propagate the change to the source model
        if (m_sourceModel) {
            QVariantMap changes;
            QHash<int, QByteArray> roles = roleNames();
            if (roles.contains(role)) {
                changes.insert(roles.value(role), value);
                m_sourceModel->modifyDevice(item.dbId, changes);
                return true;
            }
        }
        return false;
    }
}

QHash<int, QByteArray> SelectedListModel::roleNames() const
{
    return m_sourceModel ? m_sourceModel->roleNames() : QHash<int, QByteArray>();
}

void SelectedListModel::setProxyModel(TreeProxyModel *proxyModel)
{
    if (m_proxyModel) {
        disconnect(m_proxyModel, &TreeProxyModel::showRunningOnlyChanged, this, &SelectedListModel::onFilterChanged);
        disconnect(m_proxyModel, &TreeProxyModel::showAllDevicesChanged, this, &SelectedListModel::onFilterChanged);
        disconnect(m_proxyModel, &TreeProxyModel::searchFilterChanged, this, &SelectedListModel::onFilterChanged);
    }

    m_proxyModel = proxyModel;

    if (m_proxyModel) {
        connect(m_proxyModel, &TreeProxyModel::showRunningOnlyChanged, this, &SelectedListModel::onFilterChanged);
        connect(m_proxyModel, &TreeProxyModel::showAllDevicesChanged, this, &SelectedListModel::onFilterChanged);
        connect(m_proxyModel, &TreeProxyModel::searchFilterChanged, this, &SelectedListModel::onFilterChanged);
    }
}

void SelectedListModel::onFilterChanged()
{
    // 当过滤条件改变时，更新所有设备的数据
    updateAllDevicesData();
}

bool SelectedListModel::matchesFilter(const QModelIndex& deviceIndex) const
{
    if (!m_proxyModel) return true;
    
    // 检查搜索过滤
    QString searchFilter = m_proxyModel->searchFilter();
    if (!searchFilter.isEmpty()) {
        QString displayName = m_sourceModel->data(deviceIndex, DeviceRoles::DisplayNameRole).toString();
        QString hostIp = m_sourceModel->data(deviceIndex, DeviceRoles::HostIpRole).toString();
        if (!displayName.contains(searchFilter, Qt::CaseInsensitive) && 
            !hostIp.contains(searchFilter, Qt::CaseInsensitive)) {
            return false;
        }
    }
    
    // 检查状态过滤
    bool showRunningOnly = m_proxyModel->showRunningOnly();
    bool showAllDevices = m_proxyModel->showAllDevices();
    
    if (showRunningOnly && !showAllDevices) {
        QString state = m_sourceModel->data(deviceIndex, DeviceRoles::StateRole).toString();
        if (state != "running") {
            return false;
        }
    }
    
    return true;
}

void SelectedListModel::updateAllDevicesData()
{
    if (!m_sourceModel || !m_proxyModel) return;

    // 保存当前列表中设备的本地状态（checked, selected, refresh）
    QMap<QString, QPair<bool, QPair<bool, bool>>> localStates;
    for (const auto& device : qAsConst(m_selectedDevices)) {
        localStates[device.dbId] = qMakePair(device.checked, qMakePair(device.selected, device.refresh));
    }

    // 重新构建列表：遍历源模型中所有被勾选的设备，只保留符合过滤条件的
    QList<DeviceData> newSelectedDevices;
    
    for (int gi = 0; gi < m_sourceModel->rowCount(QModelIndex()); ++gi) {
        QModelIndex groupIndex = m_sourceModel->index(gi, 0, QModelIndex());
        for (int hi = 0; hi < m_sourceModel->rowCount(groupIndex); ++hi) {
            QModelIndex hostIndex = m_sourceModel->index(hi, 0, groupIndex);
            for (int di = 0; di < m_sourceModel->rowCount(hostIndex); ++di) {
                QModelIndex deviceIndex = m_sourceModel->index(di, 0, hostIndex);
                if (m_sourceModel->data(deviceIndex, DeviceRoles::ItemTypeRole) == TreeModel::TypeDevice) {
                    // 只处理被勾选的设备
                    if (m_sourceModel->data(deviceIndex, DeviceRoles::CheckedRole).toBool()) {
                        // 检查设备是否符合当前的过滤条件
                        if (matchesFilter(deviceIndex)) {
                            DeviceData device = m_sourceModel->data(deviceIndex, DeviceRoles::ItemDataRole).value<DeviceData>();
                            
                            // 恢复本地状态
                            if (localStates.contains(device.dbId)) {
                                auto state = localStates[device.dbId];
                                device.checked = state.first;
                                device.selected = state.second.first;
                                device.refresh = state.second.second;
                            } else {
                                device.checked = true;
                                device.selected = false;
                                device.refresh = false;
                            }
                            
                            newSelectedDevices.append(device);
                        }
                    }
                }
            }
        }
    }

    // 比较新旧列表，找出需要添加、移除和更新的设备
    QSet<QString> oldDbIds;
    for (const auto& device : qAsConst(m_selectedDevices)) {
        oldDbIds.insert(device.dbId);
    }
    
    QSet<QString> newDbIds;
    for (const auto& device : qAsConst(newSelectedDevices)) {
        newDbIds.insert(device.dbId);
    }

    // 移除不再符合条件的设备
    for (int i = m_selectedDevices.count() - 1; i >= 0; --i) {
        if (!newDbIds.contains(m_selectedDevices[i].dbId)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_selectedDevices.removeAt(i);
            endRemoveRows();
        }
    }

    // 添加新符合条件的设备或更新现有设备
    for (const auto& newDevice : qAsConst(newSelectedDevices)) {
        int existingRow = -1;
        for (int i = 0; i < m_selectedDevices.count(); ++i) {
            if (m_selectedDevices[i].dbId == newDevice.dbId) {
                existingRow = i;
                break;
            }
        }

        if (existingRow >= 0) {
            // 更新现有设备的数据
            DeviceData& localDevice = m_selectedDevices[existingRow];
            bool local_checked = localDevice.checked;
            bool local_selected = localDevice.selected;
            bool local_refresh = localDevice.refresh;
            
            localDevice = newDevice;
            localDevice.checked = local_checked;
            localDevice.selected = local_selected;
            localDevice.refresh = local_refresh;
            
            // 通知数据变化
            QVector<int> changedRoles;
            changedRoles.append(StateRole);
            changedRoles.append(DisplayNameRole);
            changedRoles.append(ImageRole);
            changedRoles.append(AdbRole);
            changedRoles.append(DataRole);
            changedRoles.append(DnsRole);
            changedRoles.append(DpiRole);
            changedRoles.append(FpsRole);
            changedRoles.append(HeightRole);
            changedRoles.append(IpRole);
            changedRoles.append(MemoryRole);
            changedRoles.append(NameRole);
            changedRoles.append(ShortIdRole);
            changedRoles.append(WidthRole);
            changedRoles.append(AospVersionRole);
            changedRoles.append(HostIpRole);
            changedRoles.append(TcpVideoPortRole);
            changedRoles.append(TcpAudioPortRole);
            changedRoles.append(TcpControlPortRole);
            changedRoles.append(MacvlanIpRole);
            
            emit dataChanged(index(existingRow, 0), index(existingRow, 0), changedRoles);
        } else {
            // 添加新设备
            int insertRow = m_selectedDevices.count();
            beginInsertRows(QModelIndex(), insertRow, insertRow);
            m_selectedDevices.append(newDevice);
            endInsertRows();
        }
    }
}

void SelectedListModel::onSourceReset()
{
    QSet<QString> previouslyCheckedDbIds;
    for (const auto& device : qAsConst(m_selectedDevices)) {
        if (device.checked) {
            previouslyCheckedDbIds.insert(device.dbId);
        }
    }

    beginResetModel();
    m_selectedDevices.clear();
    if(m_sourceModel) {
        for (int i = 0; i < m_sourceModel->rowCount(QModelIndex()); ++i) { // Groups
            QModelIndex groupIndex = m_sourceModel->index(i, 0, QModelIndex());
            for (int j = 0; j < m_sourceModel->rowCount(groupIndex); ++j) { // Hosts
                QModelIndex hostIndex = m_sourceModel->index(j, 0, groupIndex);
                for (int k = 0; k < m_sourceModel->rowCount(hostIndex); ++k) { // Devices
                    QModelIndex deviceIndex = m_sourceModel->index(k, 0, hostIndex);
                    if (m_sourceModel->data(deviceIndex, DeviceRoles::ItemTypeRole) == TreeModel::TypeDevice) {
                        if (m_sourceModel->data(deviceIndex, DeviceRoles::CheckedRole).toBool()) { // Only include checked items
                            DeviceData device = m_sourceModel->data(deviceIndex, DeviceRoles::ItemDataRole).value<DeviceData>();
                            // 如果源模型中 checked 为 true，默认在 selectedlistmodel 中也设置为 true
                            device.checked = true;
                            device.selected = false;
                            device.refresh = false;
                            m_selectedDevices.append(device);
                        }
                    }
                }
            }
        }
    }
    endResetModel();
}

void SelectedListModel::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
        QModelIndex sourceIndex = topLeft.sibling(i, 0);
        if (m_sourceModel->data(sourceIndex, DeviceRoles::ItemTypeRole) != TreeModel::TypeDevice) continue;

        DeviceData device = m_sourceModel->data(sourceIndex, DeviceRoles::ItemDataRole).value<DeviceData>();
        bool isChecked = (m_sourceModel->data(sourceIndex, DeviceRoles::CheckedRole).toBool());

        int existingRow = -1;
        for(int k=0; k < m_selectedDevices.count(); ++k) {
            if (m_selectedDevices.at(k).dbId == device.dbId) {
                existingRow = k;
                break;
            }
        }

        // 检查设备是否符合过滤条件
        bool matchesFilterCondition = matchesFilter(sourceIndex);

        if (isChecked && existingRow == -1 && matchesFilterCondition) {
            // Add to this model (only if matches filter)
            int insertRow = m_selectedDevices.count();
            beginInsertRows(QModelIndex(), insertRow, insertRow);
            // 如果源模型中 checked 为 true，默认在 selectedlistmodel 中也设置为 true
            device.checked = true;
            device.selected = false;
            device.refresh = false;
            m_selectedDevices.append(device);
            endInsertRows();
        } else if ((!isChecked || !matchesFilterCondition) && existingRow != -1) {
            // Remove from this model if unchecked or doesn't match filter
            beginRemoveRows(QModelIndex(), existingRow, existingRow);
            m_selectedDevices.removeAt(existingRow);
            endRemoveRows();
        } else if (isChecked && existingRow != -1 && matchesFilterCondition) {
            // Data of an already selected item changed in the source model.
            // We need to update our copy, but preserve our local state for checked, selected, refresh.
            DeviceData& localDevice = m_selectedDevices[existingRow];
            bool local_checked = localDevice.checked;
            bool local_selected = localDevice.selected;
            bool local_refresh = localDevice.refresh;

            localDevice = device; // Update with fresh data from source

            localDevice.checked = local_checked; // Restore local state
            localDevice.selected = local_selected;
            localDevice.refresh = local_refresh;
            
            emit dataChanged(index(existingRow, 0), index(existingRow, 0), roles);
        }
    }
}

void SelectedListModel::onSourceRowsInserted(const QModelIndex &parent, int first, int last)
{
    if (!m_sourceModel) return;
    QSet<QString> checkedDbIdsToPreserve = this->property("checkedDbIdsToPreserve").value<QSet<QString>>();

    for (int i = first; i <= last; ++i) {
        QModelIndex sourceIndex = m_sourceModel->index(i, 0, parent);
        if (m_sourceModel->data(sourceIndex, DeviceRoles::ItemTypeRole) == TreeModel::TypeDevice &&
            m_sourceModel->data(sourceIndex, DeviceRoles::CheckedRole).toBool())
        {
            QString deviceDbId = m_sourceModel->data(sourceIndex, DeviceRoles::DbIdRole).toString();
            bool exists = false;
            for(const auto& device : qAsConst(m_selectedDevices)) {
                if (device.dbId == deviceDbId) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                DeviceData device = m_sourceModel->data(sourceIndex, DeviceRoles::ItemDataRole).value<DeviceData>();
                
                // 如果源模型中 checked 为 true，默认在 selectedlistmodel 中也设置为 true
                device.checked = true;

                device.selected = false;
                device.refresh = false;
                int insertRow = m_selectedDevices.count();
                beginInsertRows(QModelIndex(), insertRow, insertRow);
                m_selectedDevices.append(device);
                endInsertRows();
            }
        }
    }
    this->setProperty("checkedDbIdsToPreserve", QVariant()); // Clear the property
}

void SelectedListModel::onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (!m_sourceModel) return;

    QSet<QString> checkedDbIdsToPreserve;
    if(this->property("checkedDbIdsToPreserve").isValid()){
        checkedDbIdsToPreserve = this->property("checkedDbIdsToPreserve").value<QSet<QString>>();
    }

    for (int i = first; i <= last; ++i) {
        QModelIndex sourceIndex = m_sourceModel->index(i, 0, parent);
        if (!sourceIndex.isValid() || m_sourceModel->data(sourceIndex, DeviceRoles::ItemTypeRole) != TreeModel::TypeDevice) {
            continue;
        }
        QString deviceDbId = m_sourceModel->data(sourceIndex, DeviceRoles::DbIdRole).toString();
        for (const auto& device : qAsConst(m_selectedDevices)) {
            if (device.dbId == deviceDbId && device.checked) {
                checkedDbIdsToPreserve.insert(deviceDbId);
                break;
            }
        }
    }
    this->setProperty("checkedDbIdsToPreserve", QVariant::fromValue(checkedDbIdsToPreserve));

    for (int i = first; i <= last; ++i) {
        QModelIndex sourceIndex = m_sourceModel->index(i, 0, parent);
        if (!sourceIndex.isValid() || m_sourceModel->data(sourceIndex, DeviceRoles::ItemTypeRole) != TreeModel::TypeDevice) {
            continue;
        }

        QString deviceDbId = m_sourceModel->data(sourceIndex, DeviceRoles::DbIdRole).toString();

        for (int j = 0; j < m_selectedDevices.count(); ++j) {
            if (m_selectedDevices.at(j).dbId == deviceDbId) {
                beginRemoveRows(QModelIndex(), j, j);
                m_selectedDevices.removeAt(j);
                endRemoveRows();
                break;
            }
        }
    }
}
