import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI
import Utils

Item {
    id: root
    implicitWidth: 1000
    implicitHeight: 800

    signal goBack()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32

            FluIconButton {
                iconSource: FluentIcons.ChevronLeft
                display: Button.TextBesideIcon
                iconSize: 13
                text: qsTr("返回")

                onClicked: {
                    root.goBack()
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            radius: 10

            ScrollView {
                anchors.fill: parent
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                ScrollBar.vertical.interactive: false
                ScrollBar.horizontal.interactive: false

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 40
                    anchors.rightMargin: 40
                    spacing: 30

                    FluText {
                        text: qsTr("推流设置")
                        font.pixelSize: 16
                        font.bold: true
                    }

                    // 流类型选择
                    ColumnLayout {
                        Layout.preferredWidth: 280
                        Layout.maximumWidth: 280
                        spacing: 8
                        
                        FluText {
                            text: qsTr("推流类型")
                            font.pixelSize: 14
                        }
                        
                        ButtonGroup {
                            id: streamTypeGroup
                            exclusive: true
                        }
                        
                        Component.onCompleted: {
                            // 如果 streamType 不存在，设置默认值为 2（音视频）
                            var currentType = SettingsHelper.get("streamType", -1)
                            if(currentType === -1) {
                                SettingsHelper.save("streamType", 2)
                            }
                        }
                        
                        Row {
                            spacing: 20
                            
                            VCheckBox {
                                id: videoAudioRadio
                                text: qsTr("音视频")
                                textColor: ThemeUI.blackColor
                                checked: 2 == SettingsHelper.get("streamType", 2)
                                ButtonGroup.group: streamTypeGroup
                                enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                                onClicked: {
                                    SettingsHelper.save("streamType", 2)
                                }
                            }
                            VCheckBox {
                                id: videoOnlyRadio
                                text: qsTr("仅视频")
                                textColor: ThemeUI.blackColor
                                checked: 0 == SettingsHelper.get("streamType", 2)
                                ButtonGroup.group: streamTypeGroup
                                enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                                onClicked: {
                                    SettingsHelper.save("streamType", 0)
                                }
                            }
                            VCheckBox {
                                id: audioOnlyRadio
                                text: qsTr("仅音频")
                                textColor: ThemeUI.blackColor
                                checked: 1 == SettingsHelper.get("streamType", 2)
                                ButtonGroup.group: streamTypeGroup
                                enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                                onClicked: {
                                    SettingsHelper.save("streamType", 1)
                                }
                            }
                        }
                    }

                    // 摄像头选择区域
                    ColumnLayout {
                        Layout.preferredWidth: 280
                        Layout.maximumWidth: 280
                        spacing: 8
                        visible: videoAudioRadio.checked || videoOnlyRadio.checked  // 音视频或仅视频时显示
                        
                        RowLayout{
                            Layout.preferredHeight: 28
                            Layout.fillWidth: true
                            spacing: 8

                            Image {
                                source: "qrc:/res/pad/btn_live_camera1.png"
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            FluText{
                                text: qsTr("摄像头")
                                font.pixelSize: 14
                            }

                            Item{
                                Layout.fillWidth: true
                            }
                            
                            TextButton {
                                text: qsTr("刷新")
                                // visible: cameraDeviceScanner && cameraDeviceScanner.count > 0
                                enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                                Layout.preferredHeight: 24
                                onClicked: {
                                    if(cameraDeviceScanner){
                                        cameraDeviceScanner.scanDevices()
                                    }
                                }
                            }
                        }
                        
                        // 摄像头选择行
                        FluComboBox{
                            id: videoComboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            // visible: cameraDeviceScanner && cameraDeviceScanner.count > 0
                            enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                            model: cameraDeviceScanner
                            textRole: "deviceName"
                            onCurrentIndexChanged: {
                                if(currentIndex >= 0){
                                    SettingsHelper.save("cameraId", currentIndex)
                                }
                            }
                            Component.onCompleted: {
                                if(cameraDeviceScanner && cameraDeviceScanner.count > 0){
                                    const cameraId = SettingsHelper.get("cameraId", 0)
                                    if(cameraId >= cameraDeviceScanner.count){
                                        currentIndex = 0
                                    } else {
                                        currentIndex = cameraId
                                    }
                                }
                            }
                        }
                    }

                    // Item{
                    //     Layout.preferredWidth: 280
                    //     Layout.maximumWidth: 280
                    //     Layout.preferredHeight: 130
                    //     visible: (videoAudioRadio.checked || videoOnlyRadio.checked) && (!cameraDeviceScanner || cameraDeviceScanner.count <= 0)
                    //     Image {
                    //         anchors.fill: parent
                    //         source: "qrc:/res/pad/bk_mic.png"
                    //     }
                    //     ColumnLayout{
                    //         anchors.centerIn: parent
                    //         spacing: 10

                    //         Image{
                    //             source: "qrc:/res/pad/btn_live_camera2.png"
                    //             Layout.alignment: Qt.AlignHCenter
                    //         }

                    //         FluText{
                    //             text: qsTr("未发现摄像头，无法开启")
                    //             Layout.alignment: Qt.AlignHCenter
                    //         }
                            
                    //         TextButton {
                    //             Layout.alignment: Qt.AlignHCenter
                    //             text: qsTr("重新扫描")
                    //             backgroundColor: ThemeUI.primaryColor
                    //             textColor: "white"
                    //             borderRadius: 4
                    //             onClicked: {
                    //                 if(cameraDeviceScanner){
                    //                     cameraDeviceScanner.scanDevices()
                    //                 }
                    //             }
                    //         }
                    //     }
                    // }

                    // 麦克风选择区域
                    ColumnLayout {
                        Layout.preferredWidth: 280
                        Layout.maximumWidth: 280
                        spacing: 8
                        visible: videoAudioRadio.checked || audioOnlyRadio.checked  // 音视频或仅音频时显示
                        
                        RowLayout{
                            Layout.preferredHeight: 28
                            Layout.fillWidth: true
                            spacing: 8

                            Image {
                                source: "qrc:/res/pad/btn_live_mic.png"
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                            }
                            FluText{
                                text: qsTr("麦克风")
                                font.pixelSize: 14
                            }
                            Item{
                                Layout.fillWidth: true
                            }
                            
                            TextButton {
                                text: qsTr("刷新")
                                // visible: audioDeviceScanner && audioDeviceScanner.count > 0
                                enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                                Layout.preferredHeight: 24
                                onClicked: {
                                    if(audioDeviceScanner){
                                        audioDeviceScanner.scanDevices()
                                    }
                                }
                            }
                        }

                        // 麦克风选择行
                        FluComboBox{
                            id: audioComboBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            // visible: audioDeviceScanner && audioDeviceScanner.count > 0
                            enabled: !cameraStreamManager || !cameraStreamManager.isStreaming
                            model: audioDeviceScanner
                            textRole: "deviceName"
                            onCurrentIndexChanged: {
                                if(currentIndex >= 0){
                                    SettingsHelper.save("microphoneId", currentIndex)
                                }
                            }
                            Component.onCompleted: {
                                if(audioDeviceScanner && audioDeviceScanner.count > 0){
                                    const microphoneId = SettingsHelper.get("microphoneId", 0)
                                    if(microphoneId >= audioDeviceScanner.count){
                                        currentIndex = 0
                                    } else {
                                        currentIndex = microphoneId
                                    }
                                }
                            }
                        }
                    }

                    // Item{
                    //     Layout.preferredWidth: 280
                    //     Layout.maximumWidth: 280
                    //     Layout.preferredHeight: 130
                    //     visible: (videoAudioRadio.checked || audioOnlyRadio.checked) && (!audioDeviceScanner || audioDeviceScanner.count <= 0)

                    //     Image {
                    //         anchors.fill: parent
                    //         source: "qrc:/res/pad/bk_mic.png"
                    //     }

                    //     ColumnLayout{
                    //         anchors.centerIn: parent
                    //         spacing: 10

                    //         Image{
                    //             source: "qrc:/res/pad/btn_live_camera2.png"
                    //             Layout.alignment: Qt.AlignHCenter
                    //         }

                    //         FluText{
                    //             text: qsTr("未发现麦克风，无法开启")
                    //             Layout.alignment: Qt.AlignHCenter
                    //         }
                            
                    //         TextButton {
                    //             Layout.alignment: Qt.AlignHCenter
                    //             text: qsTr("重新扫描")
                    //             backgroundColor: ThemeUI.primaryColor
                    //             textColor: "white"
                    //             borderRadius: 4
                    //             onClicked: {
                    //                 if(audioDeviceScanner){
                    //                     audioDeviceScanner.scanDevices()
                    //                 }
                    //             }
                    //         }
                    //     }
                    // }

                    // RTSP地址显示
                    ColumnLayout {
                        Layout.preferredWidth: 280
                        Layout.maximumWidth: 280
                        spacing: 8
                        visible: cameraStreamManager && cameraStreamManager.isStreaming && cameraStreamManager.rtspUrl
                        
                        FluText {
                            text: qsTr("RTSP地址:")
                            font.pixelSize: 13
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: '#939393'
                            radius: 4
                            
                            FluText {
                                anchors.left: parent.left
                                anchors.right: btnCopyRtsp.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                anchors.rightMargin: 8
                                text: cameraStreamManager ? cameraStreamManager.rtspUrl : ""
                                color: "white"
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                            }
                            
                            TextButton {
                                id: btnCopyRtsp
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.rightMargin: 4
                                width: 50
                                height: 26
                                text: qsTr("复制")
                                backgroundColor: "#FF30BF8F"
                                textColor: "white"
                                borderRadius: 4
                                onClicked: {
                                    if(cameraStreamManager && cameraStreamManager.rtspUrl){
                                        FluTools.clipText(cameraStreamManager.rtspUrl)
                                        // 使用简单的消息提示
                                        console.log("RTSP地址已复制到剪贴板")
                                    }
                                }
                            }
                        }
                    }

                    // 推流开关
                    RowLayout {
                        Layout.preferredWidth: 280
                        Layout.maximumWidth: 280
                        Layout.preferredHeight: 40
                        
                        FluText {
                            text: qsTr("开启推流")
                            font.pixelSize: 14
                        }
                        
                        Item {
                            Layout.fillWidth: true
                        }
                        
                        FluToggleSwitch {
                            id: streamToggleSwitch
                            checkColor: ThemeUI.primaryColor
                            checked: cameraStreamManager && cameraStreamManager.isStreaming
                            enabled: {
                                if(videoOnlyRadio.checked) {
                                    // 仅视频：需要摄像头
                                    return cameraDeviceScanner && cameraDeviceScanner.count > 0 && videoComboBox.currentIndex >= 0
                                } else if(audioOnlyRadio.checked) {
                                    // 仅音频：需要麦克风
                                    return audioDeviceScanner && audioDeviceScanner.count > 0 && audioComboBox.currentIndex >= 0
                                } else {
                                    // 音视频：需要摄像头和麦克风
                                    return cameraDeviceScanner && cameraDeviceScanner.count > 0 && videoComboBox.currentIndex >= 0 &&
                                           audioDeviceScanner && audioDeviceScanner.count > 0 && audioComboBox.currentIndex >= 0
                                }
                            }
                            
                            onClicked: {
                                if(checked){
                                    // 开启推流
                                    if(!cameraStreamManager){
                                        console.error("摄像头推流功能未初始化")
                                        checked = false
                                        return
                                    }
                                    
                                    var videoDeviceId = ""
                                    var audioDeviceId = ""
                                    
                                    // 根据流类型获取设备ID
                                    if(videoOnlyRadio.checked || videoAudioRadio.checked) {
                                        // 需要视频
                                        if(!cameraDeviceScanner || videoComboBox.currentIndex < 0){
                                            console.error("请选择有效的摄像头设备")
                                            checked = false
                                            return
                                        }
                                        var device = cameraDeviceScanner.getDevice(videoComboBox.currentIndex)
                                        if(!device || !device.deviceId){
                                            console.error("请选择有效的摄像头设备")
                                            checked = false
                                            return
                                        }
                                        videoDeviceId = device.deviceId
                                    }
                                    
                                    if(audioOnlyRadio.checked || videoAudioRadio.checked) {
                                        // 需要音频
                                        if(!audioDeviceScanner || audioComboBox.currentIndex < 0){
                                            console.error("请选择有效的音频设备")
                                            checked = false
                                            return
                                        }
                                        var audioDevice = audioDeviceScanner.getDevice(audioComboBox.currentIndex)
                                        if(!audioDevice || !audioDevice.deviceId){
                                            console.error("请选择有效的音频设备")
                                            checked = false
                                            return
                                        }
                                        audioDeviceId = audioDevice.deviceId
                                    }
                                    
                                    // 仅音频时，使用空字符串作为视频设备ID
                                    if(audioOnlyRadio.checked) {
                                        videoDeviceId = ""
                                    }
                                    
                                    // 生成流名称
                                    var streamName = "live"
                                    var streamTypeText = videoAudioRadio.checked ? "音视频" : (videoOnlyRadio.checked ? "仅视频" : "仅音频")
                                    console.log("生成的流名称:", streamName)
                                    console.log("流类型:", streamTypeText)
                                    
                                    // 使用摄像头原始分辨率和帧率（传递0表示使用默认/原始值）
                                    var success = cameraStreamManager.startStreaming(
                                        videoDeviceId,
                                        streamName,
                                        0,  // width: 0表示使用摄像头原始分辨率
                                        0,  // height: 0表示使用摄像头原始分辨率
                                        0,  // fps: 0表示使用摄像头原始帧率
                                        audioDeviceId  // 音频设备ID
                                    )
                                    
                                    if(!success){
                                        console.error(cameraStreamManager.errorMessage || "开始推流失败")
                                        checked = false
                                    } else {
                                        console.log("开始推流成功")
                                    }
                                } else {
                                    // 停止推流
                                    if(!cameraStreamManager){
                                        return
                                    }
                                    
                                    cameraStreamManager.stopStreaming()
                                    console.log("已停止推流")
                                }
                            }
                        }
                    }

                    Rectangle {
                        height: 1
                        color: "#e0e0e0"
                        Layout.fillWidth: true
                    }

                    FluText {
                        text: qsTr("云机窗口初始化大小设置（设备按9:16比例自适应调整大小）")
                        font.pixelSize: 16
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        spacing: 20

                        ButtonGroup {
                            id: windowSizeGroup
                            exclusive: true
                            buttons: windowSizeRow.children
                        }

                        Row {
                            id: windowSizeRow
                            spacing: 20

                            VCheckBox {
                                text: qsTr("大窗口（宽480）")
                                textColor: ThemeUI.blackColor
                                checked: 0 == SettingsHelper.get("windowSize", 1)
                                onClicked: {
                                    SettingsHelper.save("windowSize", 0)
                                }
                            }
                            VCheckBox {
                                text: qsTr("中窗口（宽320）")
                                textColor: ThemeUI.blackColor
                                checked: 1 == SettingsHelper.get("windowSize", 1)
                                onClicked: {
                                    SettingsHelper.save("windowSize", 1)
                                }
                            }
                            VCheckBox {
                                text: qsTr("小窗口（宽160）")
                                textColor: ThemeUI.blackColor
                                checked: 2 == SettingsHelper.get("windowSize", 1)
                                onClicked: {
                                    SettingsHelper.save("windowSize", 2)
                                }
                            }
                            VCheckBox {
                                text: qsTr("自定义")
                                textColor: ThemeUI.blackColor
                                checked: 3 == SettingsHelper.get("windowSize", 1)
                                onClicked: {
                                    SettingsHelper.save("windowSize", 3)
                                }
                            }
                        }

                        FluText {
                            text: qsTr("宽")
                        }

                        TextField {
                            id: textFieldWidth
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 26
                            horizontalAlignment: Text.AlignHCenter
                            text: SettingsHelper.get("customWidth", 160)
                            color: "black"
                            background: Rectangle {
                                color: "white"
                                border.width: 1
                                border.color: "#fff5f6fa"
                                radius: 4
                            }
                            validator: IntValidator {
                                top: 9999
                                bottom: 160
                            }
                            onTextChanged: {
                                if (activeFocus && text !== "") {
                                    console.log("width ", text)
                                    textFieldHeight.text = Math.round(Number(text) / (9.0 / 16.0))
                                    SettingsHelper.save("customWidth", text)
                                    SettingsHelper.save("customHeight", textFieldHeight.text)
                                }
                            }
                        }
                        FluText {
                            text: "*"
                            color: "red"
                        }
                        FluText {
                            text: qsTr("高")
                        }
                        TextField {
                            id: textFieldHeight
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 26
                            horizontalAlignment: Text.AlignHCenter
                            text: SettingsHelper.get("customHeight", 284)
                            color: "black"
                            background: Rectangle {
                                color: "white"
                                border.width: 1
                                border.color: "#fff5f6fa"
                                radius: 4
                            }
                            validator: IntValidator {
                                top: 9999
                                bottom: 284
                            }
                            onTextChanged: {
                                if (activeFocus && text !== "") {
                                    console.log("height ", text)
                                    textFieldWidth.text = Math.round(Number(text) * (9.0 / 16.0))
                                    SettingsHelper.save("customWidth", textFieldWidth.text)
                                    SettingsHelper.save("customHeight", text)
                                }
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        height: 1
                        color: "#e0e0e0"
                        Layout.fillWidth: true
                    }

                    FluText {
                        text: qsTr("云机窗口修改大小设置")
                        font.pixelSize: 16
                        font.bold: true
                    }

                    ButtonGroup {
                        id: windowModifyGroup
                        exclusive: true
                        buttons: windowModifyRow.children
                    }

                    Row {
                        id: windowModifyRow
                        spacing: 20

                        VCheckBox {
                            text: qsTr("记录上次")
                            textColor: ThemeUI.blackColor
                            checked: 0 == SettingsHelper.get("windowModify", 1)
                            onClicked: {
                                SettingsHelper.save("windowModify", 0)
                            }
                        }
                        VCheckBox {
                            text: qsTr("保持不变")
                            textColor: ThemeUI.blackColor
                            checked: 1 == SettingsHelper.get("windowModify", 1)
                            onClicked: {
                                SettingsHelper.save("windowModify", 1)
                            }
                        }
                    }

                    Rectangle {
                        height: 1
                        color: "#e0e0e0"
                        Layout.fillWidth: true
                    }

                    FluText {
                        text: qsTr("关闭主面板时")
                        font.pixelSize: 16
                        font.bold: true
                    }

                    ButtonGroup {
                        id: exitAppGroup
                        exclusive: true
                        buttons: exitAppRow.children
                    }

                    Row {
                        id: exitAppRow
                        spacing: 20

                        VCheckBox {
                            text: qsTr("退出程序")
                            textColor: ThemeUI.blackColor
                            checked: 0 == SettingsHelper.get("exitApp", 0)
                            onClicked: {
                                SettingsHelper.save("exitApp", 0)
                            }
                        }
                        VCheckBox {
                            text: qsTr("最小化托盘")
                            textColor: ThemeUI.blackColor
                            checked: 1 == SettingsHelper.get("exitApp", 0)
                            onClicked: {
                                SettingsHelper.save("exitApp", 1)
                            }
                        }
                    }

                    Item{
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }

    // 监听摄像头推流状态变化
    Connections {
        target: cameraStreamManager
        function onStreamingStarted() {
            console.log("摄像头推流已开始，RTSP URL:", cameraStreamManager.rtspUrl)
            // 更新开关状态
            if(streamToggleSwitch){
                streamToggleSwitch.checked = true
            }
        }
        function onStreamingStopped() {
            console.log("摄像头推流已停止")
            // 更新开关状态
            if(streamToggleSwitch){
                streamToggleSwitch.checked = false
            }
        }
        function onErrorOccurred(error) {
            console.error("摄像头推流错误:", error)
            // 推流错误时关闭开关
            if(streamToggleSwitch){
                streamToggleSwitch.checked = false
            }
        }
    }
    
    // 监听摄像头扫描完成
    Connections {
        target: cameraDeviceScanner
        function onScanFinished() {
            console.log("摄像头扫描完成，找到", cameraDeviceScanner.count, "个设备")
            if(cameraDeviceScanner.count > 0 && videoComboBox.currentIndex < 0){
                const cameraId = SettingsHelper.get("cameraId", 0)
                if(cameraId >= cameraDeviceScanner.count){
                    videoComboBox.currentIndex = 0
                } else {
                    videoComboBox.currentIndex = cameraId
                }
                // 扫描完成后，立即查询当前选中设备的能力
                if(videoComboBox.currentIndex >= 0){
                    Qt.callLater(function() {
                        cameraDeviceScanner.queryDeviceCapabilities(videoComboBox.currentIndex)
                    })
                }
            }
        }
        function onScanError(error) {
            console.error("摄像头扫描错误:", error)
        }
        function onCapabilitiesQueried(index) {
            console.log("设备能力查询完成，设备索引:", index, "（将使用摄像头原始分辨率和帧率）")
        }
    }
    
    // 监听音频设备扫描完成
    Connections {
        target: audioDeviceScanner
        function onScanFinished() {
            console.log("音频设备扫描完成，找到", audioDeviceScanner.count, "个设备")
            if(audioDeviceScanner.count > 0 && audioComboBox.currentIndex < 0){
                const microphoneId = SettingsHelper.get("microphoneId", 0)
                if(microphoneId >= audioDeviceScanner.count){
                    audioComboBox.currentIndex = 0
                } else {
                    audioComboBox.currentIndex = microphoneId
                }
            }
        }
        function onScanError(error) {
            console.error("音频设备扫描错误:", error)
        }
    }

    Component.onCompleted: {
        // 初始化时扫描设备
        Qt.callLater(function() {
            if(cameraDeviceScanner){
                cameraDeviceScanner.scanDevices()
            }
            if(audioDeviceScanner){
                audioDeviceScanner.scanDevices()
            }
        })
    }
}

