#include "deviceproxymodel.h"
#include <QCollator>
#include <QDebug>
#include "structs.h"

DeviceProxyModel::DeviceProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    collator.setLocale(QLocale::system());  // 使用系统语言
    collator.setNumericMode(true);          // 开启数字感知：名字(2) < 名字(11)
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    connect(this, &DeviceProxyModel::rowsInserted, this, &DeviceProxyModel::checkedCountChanged);
    connect(this, &DeviceProxyModel::rowsInserted, this, &DeviceProxyModel::isSelectAllChanged);
    connect(this, &DeviceProxyModel::rowsRemoved, this, &DeviceProxyModel::checkedCountChanged);
    connect(this, &DeviceProxyModel::rowsRemoved, this, &DeviceProxyModel::isSelectAllChanged);
    connect(this, &DeviceProxyModel::modelReset, this, &DeviceProxyModel::checkedCountChanged);
    connect(this, &DeviceProxyModel::modelReset, this, &DeviceProxyModel::isSelectAllChanged);

    connect(this, &DeviceProxyModel::dataChanged, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles){
        if (roles.contains(DeviceRoles::CheckedRole) || roles.isEmpty()) { // If 'checked' role changed or roles are not specified (e.g. general change)
            emit checkedCountChanged();
            emit isSelectAllChanged();
        }
    });

    connect(this, &DeviceProxyModel::layoutChanged, this, &DeviceProxyModel::checkedCountChanged);
    connect(this, &DeviceProxyModel::layoutChanged, this, &DeviceProxyModel::isSelectAllChanged);
}

QString DeviceProxyModel::filterString() const
{
    return m_filterString;
}

void DeviceProxyModel::setFilterString(const QString &newFilterString)
{
    if (m_filterString == newFilterString)
        return;
    m_filterString = newFilterString.trimmed();
    emit filterStringChanged();
    invalidateFilter();
}

bool DeviceProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterString.isEmpty()) {
        return true;
    }

    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QString name = sourceModel()->data(index, DeviceRoles::DisplayNameRole).toString();
    QString ip = sourceModel()->data(index, DeviceRoles::HostIpRole).toString();

    return (name.contains(m_filterString, Qt::CaseInsensitive) ||
            ip.contains(m_filterString, Qt::CaseInsensitive));
}

bool DeviceProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    QVariant leftData, rightData;

    leftData = sourceModel()->data(left, DeviceRoles::DisplayNameRole);
    rightData = sourceModel()->data(right, DeviceRoles::DisplayNameRole);

    return collator.compare(leftData.toString(), rightData.toString()) < 0;
}

QVariantList DeviceProxyModel::getAllPadCodeList() const{
    QVariantList jsonArray;

    // for (int i = 0; i < rowCount(); ++i) {
    //     QModelIndex index = this->index(i, 0);
    //     int status = this->data(index, DeviceRoles::StatusRole).toInt();
    //     int cvmStatus = this->data(index, DeviceRoles::CvmStatusRole).toInt();
    //     if (status == 1 && (cvmStatus == 100 || cvmStatus == 101)) {
    //         QVariantMap padInfo;
    //         padInfo.insert(this->roleNames().value(DeviceRoles::EquipmentIdRole), this->data(index, DeviceRoles::EquipmentIdRole).toLongLong());
    //         padInfo.insert(this->roleNames().value(DeviceRoles::PadCodeRole), this->data(index, DeviceRoles::PadCodeRole).toString());
    //         padInfo.insert(this->roleNames().value(DeviceRoles::StatusRole), this->data(index, DeviceRoles::StatusRole).toInt());
    //         padInfo.insert(this->roleNames().value(DeviceRoles::CvmStatusRole), this->data(index, DeviceRoles::CvmStatusRole).toInt());
    //         padInfo.insert(this->roleNames().value(DeviceRoles::SupplierTypeRole), this->data(index, DeviceRoles::SupplierTypeRole).toString());
    //         jsonArray.append(padInfo);
    //     }
    // }
    return jsonArray;
}

QVariantList DeviceProxyModel::getCheckedIDList() const{
    QVariantList jsonArray;
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex index = this->index(i, 0);
        if (this->data(index, DeviceRoles::CheckedRole).toBool()) {
            jsonArray.append(this->data(index, DeviceRoles::IdRole).toString());
        }
    }
    return jsonArray;
}

QVariantList DeviceProxyModel::getPadList() const
{
    QVariantList jsonArray;

    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex index = this->index(i, 0);
        if (this->data(index, DeviceRoles::CheckedRole).toBool()) {
            QVariantMap padInfo;
            padInfo.insert(this->roleNames().value(DeviceRoles::ShortIdRole), this->data(index, DeviceRoles::ShortIdRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::NameRole), this->data(index, DeviceRoles::NameRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::DisplayNameRole), this->data(index, DeviceRoles::DisplayNameRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::DbIdRole), this->data(index, DeviceRoles::DbIdRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::IdRole), this->data(index, DeviceRoles::IdRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::HostIpRole), this->data(index, DeviceRoles::HostIpRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::HostIdRole), this->data(index, DeviceRoles::HostIdRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::StateRole), this->data(index, DeviceRoles::StateRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::GroupIdRole), this->data(index, DeviceRoles::GroupIdRole).toInt());
            padInfo.insert(this->roleNames().value(DeviceRoles::IpRole), this->data(index, DeviceRoles::IpRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::AdbRole), this->data(index, DeviceRoles::AdbRole).toInt());
            padInfo.insert(this->roleNames().value(DeviceRoles::TcpVideoPortRole), this->data(index, DeviceRoles::TcpVideoPortRole).toInt());
            padInfo.insert(this->roleNames().value(DeviceRoles::TcpAudioPortRole), this->data(index, DeviceRoles::TcpAudioPortRole).toInt());
            padInfo.insert(this->roleNames().value(DeviceRoles::TcpControlPortRole), this->data(index, DeviceRoles::TcpControlPortRole).toInt());
            padInfo.insert(this->roleNames().value(DeviceRoles::NetworkModeRole), this->data(index, DeviceRoles::NetworkModeRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::LocaleRole), this->data(index, DeviceRoles::LocaleRole).toString());
            padInfo.insert(this->roleNames().value(DeviceRoles::TimeZoneRole), this->data(index, DeviceRoles::TimeZoneRole).toString());

            jsonArray.append(padInfo);
        }
    }
    return jsonArray;
}

void DeviceProxyModel::selectAll(bool checked){
    QAbstractItemModel *source = sourceModel();
    if (!source) return;

    // 遍历代理模型（过滤后的顺序），由调用方决定作用范围
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        QModelIndex srcIndex = mapToSource(proxyIndex);
        source->setData(srcIndex, checked, DeviceRoles::CheckedRole);
    }

    emit isSelectAllChanged();
}

void DeviceProxyModel::invertSelection(){

    QAbstractItemModel *source = sourceModel();
    if (!source) return;

    // 遍历代理模型并反转
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        QModelIndex srcIndex = mapToSource(proxyIndex);
        bool currentChecked = source->data(srcIndex, DeviceRoles::CheckedRole).toBool();
        source->setData(srcIndex, !currentChecked, DeviceRoles::CheckedRole);
    }

    emit isSelectAllChanged();
}

void DeviceProxyModel::multiSelect(int count)
{
    QAbstractItemModel *source = sourceModel();
    if (!source) return;

    // 按代理顺序选中前 count 项
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        QModelIndex srcIndex = mapToSource(proxyIndex);
        source->setData(srcIndex, i < count, DeviceRoles::CheckedRole);
    }

    emit isSelectAllChanged();
}

int DeviceProxyModel::checkedCount() const{
    int count = 0;
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        if (this->data(proxyIndex, DeviceRoles::CheckedRole).toBool()) {
            ++count;
        }
    }
    return count;
}

bool DeviceProxyModel::isSelectAll() const{
    int count = 0;
    int total = 0;
    for (int i = 0; i < rowCount(); ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        if (this->data(proxyIndex, DeviceRoles::CheckedRole).toBool()) {
            ++count;
        }
        ++total;
    }
    return total > 0 && total == count;
}

// 区间选择：仅影响代理模型在 [start, end) 的行
void DeviceProxyModel::selectRange(int start, int end, bool checked)
{
    QAbstractItemModel *source = sourceModel();
    if (!source) return;
    if (start < 0) start = 0;
    if (end > rowCount()) end = rowCount();

    for (int i = start; i < end; ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        QModelIndex srcIndex = mapToSource(proxyIndex);
        source->setData(srcIndex, checked, DeviceRoles::CheckedRole);
    }
    emit isSelectAllChanged();
}

void DeviceProxyModel::invertRange(int start, int end)
{
    QAbstractItemModel *source = sourceModel();
    if (!source) return;
    if (start < 0) start = 0;
    if (end > rowCount()) end = rowCount();

    for (int i = start; i < end; ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        QModelIndex srcIndex = mapToSource(proxyIndex);
        bool currentChecked = source->data(srcIndex, DeviceRoles::CheckedRole).toBool();
        source->setData(srcIndex, !currentChecked, DeviceRoles::CheckedRole);
    }
    emit isSelectAllChanged();
}

int DeviceProxyModel::checkedCountInRange(int start, int end) const
{
    if (start < 0) start = 0;
    if (end > rowCount()) end = rowCount();
    int count = 0;
    for (int i = start; i < end; ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        if (this->data(proxyIndex, DeviceRoles::CheckedRole).toBool()) {
            ++count;
        }
    }
    return count;
}

bool DeviceProxyModel::isAllCheckedInRange(int start, int end) const
{
    if (start < 0) start = 0;
    if (end > rowCount()) end = rowCount();
    if (end <= start) return false;
    for (int i = start; i < end; ++i) {
        QModelIndex proxyIndex = this->index(i, 0);
        if (!this->data(proxyIndex, DeviceRoles::CheckedRole).toBool()) {
            return false;
        }
    }
    return true;
}
