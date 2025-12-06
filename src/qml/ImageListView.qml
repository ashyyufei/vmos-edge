import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI
import Utils
import Qt5Compat.GraphicalEffects
import Qt.labs.platform

Item {
    id: root
    implicitWidth: 1000
    implicitHeight: 800

    readonly property var itemWidth: [220.00, 220.00, 100.00, 80.00, 120.00, 150.00]
    readonly property int itemTotalWidth: itemWidth.reduce((acc, cur) => acc + cur, 0)
    property string currentImagePath: ""
    readonly property string defaultImagePath: FluTools.toLocalPath(StandardPaths.writableLocation(StandardPaths.AppLocalDataLocation) + "/images")
    property var filteredModel: []
    property string filterText: ""
    property string filterVersion: ""
    property int sortType: 0 // 0: 导入时间（最晚优先）
    property var availableVersions: []

    Component.onCompleted: {
        root.currentImagePath = SettingsHelper.get("imagesPath", "")
        if(!root.currentImagePath){
            root.currentImagePath = FluTools.toLocalPath(StandardPaths.writableLocation(StandardPaths.AppLocalDataLocation) + "/images")
            SettingsHelper.save("imagesPath", root.currentImagePath)
        }
        updateAvailableVersions()
        updateFilteredModel()
    }

    function updateAvailableVersions() {
        var versions = new Set()
        for (var i = 0; i < imagesModel.rowCount(); i++) {
            var index = imagesModel.index(i, 0)
            var version = imagesModel.data(index, ImagesModel.VersionRole).toString()
            if (version) {
                versions.add(version)
            }
        }
        
        // 转换为数组并排序
        var versionArray = Array.from(versions)
        versionArray.sort()
        
        // 添加"全部"选项
        root.availableVersions = [qsTr("全部Android版本")].concat(versionArray)
    }

    // 将带单位的文件大小字符串统一转换为字节数，支持 B/KB/MB/GB/TB
    function sizeToBytes(sizeStr) {
        if (!sizeStr)
            return 0
        var s = sizeStr.toString().trim().toUpperCase()
        var num = parseFloat(s)
        if (isNaN(num))
            return 0
        if (s.indexOf("TB") !== -1) return num * 1024 * 1024 * 1024 * 1024
        if (s.indexOf("GB") !== -1) return num * 1024 * 1024 * 1024
        if (s.indexOf("MB") !== -1) return num * 1024 * 1024
        if (s.indexOf("KB") !== -1) return num * 1024
        return num // 默认为字节
    }

    function updateFilteredModel() {
        var result = []
        for (var i = 0; i < imagesModel.rowCount(); i++) {
            var index = imagesModel.index(i, 0)
            var item = {
                name: imagesModel.data(index, ImagesModel.NameRole).toString(),
                fileName: imagesModel.data(index, ImagesModel.FileNameRole).toString(),
                version: imagesModel.data(index, ImagesModel.VersionRole).toString(),
                path: imagesModel.data(index, ImagesModel.PathRole).toString(),
                fileSize: imagesModel.data(index, ImagesModel.FileSizeRole).toString(),
                createTime: imagesModel.data(index, ImagesModel.CreateTimeRole).toString(),
                originalIndex: i
            }
            
            // 应用搜索过滤
            if (root.filterText !== "") {
                if (!item.fileName.toLowerCase().includes(root.filterText.toLowerCase())) {
                    continue
                }
            }
            
            // 应用版本过滤
            if (root.filterVersion !== "") {
                if (item.version !== root.filterVersion) {
                    continue
                }
            }
            
            result.push(item)
        }
        
        // 应用排序
        if (root.sortType === 0) {
            // 导入时间（最晚优先）
            result.sort((a, b) => b.createTime.localeCompare(a.createTime))
        } else if (root.sortType === 1) {
            // 导入时间（最早优先）
            result.sort((a, b) => a.createTime.localeCompare(b.createTime))
        } else if (root.sortType === 2) {
            // 文件大小（从大到小）
            result.sort((a, b) => sizeToBytes(b.fileSize) - sizeToBytes(a.fileSize))
        } else if (root.sortType === 3) {
            // 文件大小（从小到大）
            result.sort((a, b) => sizeToBytes(a.fileSize) - sizeToBytes(b.fileSize))
        } else if (root.sortType === 4) {
            // 名称（A-Z）
            result.sort((a, b) => a.name.localeCompare(b.name))
        } else if (root.sortType === 5) {
            // 名称（Z-A）
            result.sort((a, b) => b.name.localeCompare(a.name))
        }
        
        root.filteredModel = result
    }

    signal openDetail(string hostIp)
    signal openBatchMenu(var hostList, var button)
    signal openReset(string hostIp)
    signal openReboot(string hostIp)
    signal openClean(string hostIp)
    signal openImportImagePopup()
    signal deleteImage(string imageName, int imageIndex)
    signal changeImagePath(string newPath)

    FolderDialog {
        id: folderDialog
        title: qsTr("选择镜像存储目录")
        onAccepted: {
            const selectedPath = FluTools.toLocalPath(folder)
            root.changeImagePath(selectedPath)
        }
    }


    ColumnLayout{
        anchors.fill: parent
        anchors.margins: 10



        Rectangle{
            Layout.fillWidth: true
            Layout.preferredHeight: 150


            ColumnLayout{
                anchors.fill: parent
                anchors.topMargin: 6
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.bottomMargin: 20

                RowLayout{
                    Layout.preferredHeight: 32
                    Layout.fillWidth: true


                    Image {
                        source: "qrc:/res/images/icon_manager.svg"
                    }

                    FluText{
                        text: qsTr("镜像管理")
                        color: ThemeUI.primaryColor
                        font.pixelSize: 16
                    }

                    Item{
                        Layout.fillWidth: true
                    }

                    FluIconButton{
                        Layout.preferredHeight: 32
                        iconSource: FluentIcons.Cloud
                        iconColor: "white"
                        iconSize: 15
                        display: AbstractButton.TextBesideIcon
                        textColor: "white"
                        normalColor: ThemeUI.primaryColor
                        hoverColor: Qt.darker(normalColor, 1.1)
                        pressedColor: Qt.darker(normalColor, 1.1)
                        text: qsTr("官方镜像下载")
                        onClicked: {
                            Qt.openUrlExternally("https://help.vmosedge.com/zh/productupdates/image-release-history.html")
                        }
                    }

                    FluIconButton{
                        Layout.preferredHeight: 32
                        iconSource: FluentIcons.Download
                        iconColor: "white"
                        iconSize: 14
                        display: AbstractButton.TextBesideIcon
                        textColor: "white"
                        normalColor: ThemeUI.primaryColor
                        hoverColor: Qt.darker(normalColor, 1.1)
                        pressedColor: Qt.darker(normalColor, 1.1)
                        text: qsTr("导入镜像")
                        onClicked: {
                            root.openImportImagePopup()
                        }
                    }
                }

                Rectangle{
                    Layout.preferredHeight: 52
                    Layout.fillWidth: true
                    radius: 6
                    color: "#F8F9FA"

                    RowLayout{
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20

                        // Image{
                        //     source: "qrc:/res/images/icon_path.svg"
                        // }

                        Image {
                            source: "qrc:/res/images/icon_dir.svg"
                        }

                        FluText{
                            text: qsTr("镜像存储路径：")
                        }

                        FluText{
                            id: imagesPathText
                            text: root.currentImagePath
                        }

                        Item{
                            Layout.fillWidth: true
                        }

                        Rectangle{
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: 86
                            color: "#E7F0FF"
                            radius: 16
                            FluText{
                                anchors.centerIn: parent
                                text: qsTr("更改路径")
                                color: ThemeUI.primaryColor
                            }

                            MouseArea{
                                anchors.fill: parent
                                onClicked: {
                                    folderDialog.open()
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.leftMargin: 8
                    visible: root.currentImagePath === root.defaultImagePath
                    FluIcon {
                        iconSource: FluentIcons.Info
                        iconSize: 14
                        color: "#E6A23C"
                    }
                    FluText {
                        Layout.fillWidth: true
                        textFormat: Text.RichText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        text: qsTr("当前镜像默认存储在 C盘；建议首次使用时调整到磁盘空间更大的盘符（如 D盘）")
                        onLinkActivated: (link) => {}
                    }
                }
            }
        }

        Rectangle{
            Layout.fillHeight: true
            Layout.fillWidth: true

            ColumnLayout{
                anchors.fill: parent

                RowLayout{
                    Layout.preferredHeight: 32
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20

                    Image {
                        source: "qrc:/res/images/icon_list.svg"
                    }

                    FluText{
                        text: qsTr("镜像列表")
                        color: ThemeUI.primaryColor
                        font.pixelSize: 16
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                RowLayout{
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20

                    SearchTextField {
                        id: filterTextField
                        Layout.preferredHeight: 32
                        Layout.preferredWidth: 280
                        placeholderText: qsTr("输入镜像名称")
                        maximumLength: 32
                        onSearchTextChanged: function(text) {
                            root.filterText = text
                            root.updateFilteredModel()
                        }
                    }

                    FluComboBox{
                        id: versionComboBox
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        model: root.availableVersions
                        onCurrentIndexChanged: {
                            if(currentIndex === 0 || currentIndex === -1){
                                root.filterVersion = ""
                            } else {
                                root.filterVersion = root.availableVersions[currentIndex]
                            }
                            root.updateFilteredModel()
                        }
                    }

                    FluComboBox{
                        id: sortComboBox
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        model: [qsTr("导入时间（最晚优先）"), qsTr("导入时间（最早优先）"), qsTr("文件大小（从大到小）"), qsTr("文件大小（从小到大）"), qsTr("名称（AZ)"), qsTr("名称（ZA)")]
                        onCurrentIndexChanged: {
                            root.sortType = currentIndex
                            root.updateFilteredModel()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }


                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#F8F9FA"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20;
                        anchors.rightMargin: 10;
                        spacing: 5


                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[0] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("镜像名称");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }

                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[1] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("镜像版本");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }
                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[2] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("Android版本");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }
                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[3] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("文件大小");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }
                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[4] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("导入时间");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }
                        Item{
                            Layout.fillHeight: true
                            Layout.preferredWidth: listView.width * itemWidth[5] / itemTotalWidth
                            // border.width: 1
                            FluText {
                                text: qsTr("操作");
                                anchors.verticalCenter: parent.verticalCenter
                                font.bold: true
                            }
                        }
                    }
                }

                ListView {
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: root.filteredModel
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Rectangle {
                        width: listView.width
                        height: 50
                        color: "transparent"

                        // 顶层覆盖层
                        Rectangle {
                            anchors.fill: parent
                            color: mouseArea.containsMouse ? "#26000000" : "transparent"
                            z: 999
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 10
                            spacing: 5

                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[0] / itemTotalWidth
                                // border.width: 1
                                FluText {
                                    text: modelData?.fileName ?? ""
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    elide: Text.ElideRight
                                    maximumLineCount: 1

                                    MouseArea{
                                        anchors.fill: parent

                                        onClicked: {
                                            FluTools.clipText(modelData?.fileName ?? "")
                                            showSuccess(qsTr("复制成功"))
                                        }
                                    }
                                }
                            }
                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[1] / itemTotalWidth
                                // border.width: 1

                                FluText {
                                    text: modelData?.name ?? ""
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    elide: Text.ElideRight
                                    maximumLineCount: 1

                                    MouseArea{
                                        anchors.fill: parent

                                        onClicked: {
                                            FluTools.clipText(modelData?.name ?? "")
                                            showSuccess(qsTr("复制成功"))
                                        }
                                    }
                                }
                            }
                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[2] / itemTotalWidth
                                // border.width: 1
                                Rectangle{
                                    width: 78
                                    height: 28
                                    color: "#E7F0FF"
                                    anchors.verticalCenter: parent.verticalCenter
                                    radius: 14

                                    FluText {
                                        anchors.centerIn: parent
                                        text: modelData?.version ?? ""
                                        font.pixelSize: 12
                                        color: ThemeUI.primaryColor
                                    }
                                }
                            }
                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[3] / itemTotalWidth
                                // border.width: 1

                                FluText {
                                    text: modelData?.fileSize ?? "0"
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }
                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[4] / itemTotalWidth
                                // border.width: 1
                                FluText {
                                    text: modelData?.createTime ?? ""
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }
                            }
                            Item{
                                Layout.fillHeight: true
                                Layout.preferredWidth: listView.width * itemWidth[5] / itemTotalWidth
                                // border.width: 1
                                RowLayout{
                                    anchors.verticalCenter: parent.verticalCenter

                                    FluIconButton{
                                        iconSource: FluentIcons.Delete
                                        iconSize: 12
                                        iconColor: "white"
                                        text: qsTr("删除")
                                        textColor: "white"
                                        display: Button.TextBesideIcon
                                        normalColor: "#f06969"
                                        hoverColor: "#f06969"

                                        onClicked: {
                                            root.deleteImage(modelData?.name ?? "", modelData?.originalIndex ?? 0)
                                        }
                                    }
                                }
                            }

                        }

                        Rectangle{
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: "#EBEBEB"
                        }
                    }
                }
            }
        }
    }
}
