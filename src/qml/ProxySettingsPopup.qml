import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import FluentUI
import Utils

FluPopup {
    id: root
    implicitWidth: 420
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    property var modelData: null
    property string cloudMachineName: ""
    
    signal proxySettingsResult(bool success, var settings)
    
    property var proxyProtocols: [
        "HTTP",
        "HTTPS",
        "SOCKS5"
    ]
    
    // 当前显示的页面：0-设置页面，1-信息页面
    property int currentPage: 0
    
    // 代理信息
    property string proxyInfo: ""
    property string proxyStatus: ""
    property string domainResolutionMode: ""
    property string proxyIp: ""
    property string proxyLocation: ""
    
    function validateServerAddress(address) {
        if (!address || address.trim() === "") {
            showError(qsTr("服务器地址不能为空"))
            return false
        }
        if (!AppUtils.isValidIp(address) && !AppUtils.isValidDomain(address)) {
            showError(qsTr("无效的服务器地址: ") + address, 3000);
            return false;
        }
        return true
    }
    
    function validatePort(port) {
        var portNum = parseInt(port)
        if (isNaN(portNum) || portNum < 1 || portNum > 65535) {
            showError(qsTr("请输入正确的端口号(1-65535)"))
            return false
        }
        return true
    }

    function validateAccount(account) {
        // if (!account || account.trim() === "") {
        //     showError(qsTr("账号不能为空"))
        //     return false
        // }
        return true
    }
    
    function validatePassword(password) {
        // if (!password || password.trim() === "") {
        //     showError(qsTr("密码不能为空"))
        //     return false
        // }
        return true
    }
    
    function testNetwork() {
        // 验证输入参数
        if (!validateServerAddress(serverAddressInput.text)) {
            return
        }
        
        if (!validatePort(portInput.text)) {
            return
        }
        
        if (!validateAccount(accountInput.text)) {
            return
        }
        
        if (!validatePassword(passwordInput.text)) {
            return
        }
        
        showLoading(qsTr("正在检测代理连接..."))
        
        // 获取协议类型
        var protocol = "socks5"
        if("HTTP" === root.proxyProtocols[protocolComboBox.currentIndex] || "HTTPS" === root.proxyProtocols[protocolComboBox.currentIndex]){
            protocol = "http"
        }
        // // 获取协议类型
        // var protocol = root.proxyProtocols[protocolComboBox.currentIndex].toLowerCase()

        // 调用C++网络检测
        proxyTester.testProxy(
                    serverAddressInput.text.trim(),
                    parseInt(portInput.text),
                    accountInput.text.trim(),
                    passwordInput.text,
                    protocol,
                    "https://www.baidu.com"
                    )
    }

    onAboutToShow: {
        // 先清空输入框
        serverAddressInput.text = ""
        portInput.text = ""
        accountInput.text = ""
        passwordInput.text = ""
        
        // 重置开关状态
        udpDisabledSwitch.checked = true
        dnsOverProxyDisabledSwitch.checked = true
        
        // 重置到设置页面
        root.currentPage = 0

        // 获取代理信息
        reqGetDeviceProxy(modelData.hostIp, modelData.dbId)
    }
    

    // 页面1：代理设置页面
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            Layout.leftMargin: 20
            Layout.rightMargin: 10

            FluText {
                text: qsTr("设置代理（云机名称：%1）").arg(root.modelData?.displayName ?? "")
                font.bold: true
                font.pixelSize: 16
            }

            Item { Layout.fillWidth: true }

            FluImageButton {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                normalImage: "qrc:/res/common/btn_close_normal.png"
                hoveredImage: "qrc:/res/common/btn_close_normal.png"
                pushedImage: "qrc:/res/common/btn_close_normal.png"
                onClicked: root.close()
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPage


            ColumnLayout {
                spacing: 0
                
                ColumnLayout{
                    Layout.margins: 20

                    // 代理协议
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("代理协议")
                            font.bold: true
                        }

                        FluComboBox {
                            id: protocolComboBox
                            Layout.fillWidth: true
                            model: root.proxyProtocols
                            currentIndex: 2 // 默认选择SOCKS5
                        }
                    }

                    // 服务器地址
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("服务器地址")
                            font.bold: true
                        }

                        FluTextBox {
                            id: serverAddressInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("请输入服务器地址")
                        }
                    }

                    // 服务端口
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("服务端口")
                            font.bold: true
                        }

                        FluTextBox {
                            id: portInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("请输入正确的端口")
                            validator: IntValidator {
                                bottom: 1
                                top: 65535
                            }
                        }
                    }

                    // 账号选择
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("账号")
                            font.bold: true
                        }

                        FluTextBox {
                            id: accountInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("请输入账号")
                        }
                    }

                    // 密码
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("密码")
                            font.bold: true
                        }

                        FluTextBox {
                            id: passwordInput
                            Layout.fillWidth: true
                            placeholderText: qsTr("请输入密码")
                            echoMode: TextInput.Password
                        }
                    }

                    // 是否禁用DNS走代理
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluText {
                                text: qsTr("代理DNS")
                                font.bold: true
                            }

                            Image {
                                id: macvlanIcon
                                source: "qrc:/res/pad/help.svg"
                                scale: 0.8

                                FluTooltip {
                                    parent: macvlanIcon
                                    visible: macvlanMouseArea.containsMouse
                                    text: qsTr("开启代理DNS需要确保您的代理IP支持DNS解析，\n否则云手机将无法联网；关闭代理DNS可能会导致DNS泄露。")
                                    delay: 500
                                    timeout: 3000
                                }

                                MouseArea {
                                    id: macvlanMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }
                            }

                            FluToggleSwitch{
                                id: dnsOverProxyDisabledSwitch
                                checkColor: ThemeUI.primaryColor
                            }

                            FluText{
                                text: qsTr("注意：如果开启后云手机无网络，请关闭代理DNS。")
                                color: ThemeUI.primaryColor
                                font.pixelSize: 10
                            }
                        }

                    }

                    // 是否禁用UDP
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: qsTr("开启UDP")
                            font.bold: true
                        }

                        Image {
                            id: udpIcon
                            source: "qrc:/res/pad/help.svg"
                            scale: 0.8

                            FluTooltip {
                                parent: udpIcon
                                visible: udpMouseArea.containsMouse
                                text: qsTr("启用 UDP 通道传输")
                                delay: 500
                                timeout: 3000
                            }

                            MouseArea {
                                id: udpMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        FluToggleSwitch{
                            id: udpDisabledSwitch
                            checkColor: ThemeUI.primaryColor
                        }
                    }

                    Item { Layout.preferredHeight: 10 }

                    // 网络检测链接
                    FluText {
                        text: qsTr("检查代理")
                        color: ThemeUI.primaryColor
                        font.underline: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                testNetwork()
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 操作按钮
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        Layout.rightMargin: 20
                        spacing: 10

                        Item { Layout.fillWidth: true }

                        FluButton {
                            text: qsTr("取消")
                            onClicked: root.close()
                        }

                        FluFilledButton {
                            text: qsTr("确定")
                            normalColor: ThemeUI.primaryColor
                            onClicked: {
                                // 验证输入
                                if (!validateServerAddress(serverAddressInput.text)) {
                                    return
                                }

                                if (!validatePort(portInput.text)) {
                                    return
                                }

                                if (!validateAccount(accountInput.text)) {
                                    return
                                }

                                if (!validatePassword(passwordInput.text)) {
                                    return
                                }

                                // // 协议
                                // var protocol = root.proxyProtocols[protocolComboBox.currentIndex].toLowerCase()
                                // 协议
                                var protocol = "socks5"
                                if("HTTP" == root.proxyProtocols[protocolComboBox.currentIndex] || "HTTPS" == root.proxyProtocols[protocolComboBox.currentIndex]){
                                    protocol = "http-relay"
                                }

                                // 构建设置对象
                                var settings = {
                                    protocol: protocol,
                                    serverAddress: serverAddressInput.text.trim(),
                                    port: parseInt(portInput.text),
                                    account: accountInput.text.trim(),
                                    password: passwordInput.text.trim(),
                                    udpDisabled: !udpDisabledSwitch.checked,
                                    dnsOverProxyDisabled: !dnsOverProxyDisabledSwitch.checked
                                }

                                reqSetDeviceProxy(root.modelData.hostIp, root.modelData.dbId, settings.serverAddress, settings.port, settings.account, settings.password, "", settings.protocol, "", settings.dnsOverProxyDisabled, settings.udpDisabled)
                            }
                        }
                    }
                }
            }
            

            // 页面2：代理信息页面
            ColumnLayout {
                spacing: 0

                // 代理信息内容
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 20
                    spacing: 20

                    // S5地址
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluText {
                            text: qsTr("代理地址")
                            Layout.preferredWidth: 120
                            font.bold: true
                        }

                        FluTextBox {
                            Layout.fillWidth: true
                            text: root.proxyInfo
                            readOnly: true
                            background: Rectangle {
                                color: "#F5F5F5"
                                border.color: "#E0E0E0"
                                border.width: 1
                                radius: 4
                            }
                        }
                    }

                    // 代理IP
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluText {
                            text: qsTr("代理IP")
                            Layout.preferredWidth: 120
                            font.bold: true
                        }

                        FluTextBox {
                            Layout.fillWidth: true
                            text: root.proxyIp || ""
                            readOnly: true
                            background: Rectangle {
                                color: "#F5F5F5"
                                border.color: "#E0E0E0"
                                border.width: 1
                                radius: 4
                            }
                        }
                    }

                    // 状态
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluText {
                            text: qsTr("状态")
                            Layout.preferredWidth: 120
                            font.bold: true
                        }

                        FluTextBox {
                            Layout.fillWidth: true
                            text: root.proxyStatus
                            readOnly: true
                            background: Rectangle {
                                color: "#F5F5F5"
                                border.color: "#E0E0E0"
                                border.width: 1
                                radius: 4
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    // 操作按钮
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        Layout.rightMargin: 20
                        spacing: 10

                        Item { Layout.fillWidth: true }

                        FluButton {
                            text: qsTr("取消")
                            onClicked: root.close()
                        }

                        FluFilledButton {
                            text: qsTr("关闭代理")
                            normalColor: ThemeUI.primaryColor
                            onClicked: {
                                reqCloseDeviceProxy(root.modelData.hostIp, root.modelData.dbId)
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 网络检测组件
    ProxyTester {
        id: proxyTester
        
        onTestCompleted: function(success, message, latency) {
            hideLoading()
            if (success) {
                showSuccess(message, 3000)
            } else {
                showError(message)
            }
        }
        
        onTestProgress: function(message) {
            // 可以在这里显示进度信息
            console.log("代理检测进度:", message)
        }
    }

    NetworkCallable {
        id: getDeviceProxy
        onStart: {
            showLoading(qsTr("查询代理信息..."))
        }
        onFinish: {
            hideLoading()
        }
        onError:
            (status, errorString, result, userData) => {
                console.debug(status + ";" + errorString + ";" + result)
                showError(errorString)
            }
        onSuccess:
            (result, userData) => {
                try {
                    const res = JSON.parse(result)
                    if(res.code == 200){
                        // 检查是否有代理配置信息
                        if(res.data && res.data.proxy_config) {
                            // 有代理配置信息，切换到信息页面
                            const proxyConfig = res.data.proxy_config
                            
                            // 构建代理信息字符串
                            root.proxyInfo = `${proxyConfig.proxyType}://${proxyConfig.ip}:${proxyConfig.port}`
                            root.proxyStatus = qsTr("已启动")
                            root.domainResolutionMode = qsTr("服务端域名解析 (默认)")
                            
                            // 设置代理IP和位置信息
                            root.proxyIp = proxyConfig.ip
                            // root.proxyLocation = `${proxyConfig.city}, ${proxyConfig.region}, ${proxyConfig.country}`
                            
                            // 填充输入框（用于编辑）
                            serverAddressInput.text = proxyConfig.ip
                            portInput.text = proxyConfig.port
                            if(proxyConfig.account) {
                                accountInput.text = proxyConfig.account
                            }
                            if(proxyConfig.password) {
                                passwordInput.text = proxyConfig.password
                            }
                            
                            // 设置开关状态
                            // if(proxyConfig.udpDisabled !== undefined) {
                            //     udpDisabledSwitch.checked = proxyConfig.udpDisabled
                            // }
                            // if(proxyConfig.dnsOverProxyDisabled !== undefined) {
                            //     dnsOverProxyDisabledSwitch.checked = proxyConfig.dnsOverProxyDisabled
                            // }
                            
                            // 切换到代理信息页面
                            root.currentPage = 1
                        } else {
                            // 没有代理配置信息，显示设置页面
                            root.currentPage = 0
                        }
                    }else{
                        showError(res.msg, 3000)
                    }
                } catch (e) {
                    console.warn("无法将行解析为JSON:", result, e)
                }
            }
    }

    // 获取代理
    function reqGetDeviceProxy(ip, dbId){
        Network.get(`http://${ip}:18182/android_api/v1` + "/proxy_get/" + dbId)
        .setUserData(ip)
        .bind(root)
        .go(getDeviceProxy)
    }

    NetworkCallable {
        id: setDeviceProxy
        onStart: {
            showLoading(qsTr("正在设置代理..."))
        }
        onFinish: {
            hideLoading()
        }
        onError:
            (status, errorString, result, userData) => {
                console.debug(status + ";" + errorString + ";" + result)
                showError(errorString)
                // root.createResult(false)
                // root.close()
            }
        onSuccess:
            (result, userData) => {
                try {
                    const res = JSON.parse(result)
                    if(res.code == 200){
                        // 设置代理成功，显示成功消息
                        showSuccess(qsTr("代理设置成功"))
                        
                        // 调用查询代理接口获取最新信息
                        reqGetDeviceProxy(root.modelData.hostIp, root.modelData.dbId)
                    }else{
                        showError(res.msg, 3000)
                    }
                } catch (e) {
                    console.warn("无法将行解析为JSON:", result, e)
                }
            }
    }

    // 设置代理
    function reqSetDeviceProxy(hostIp, dbId, ip, port, account, password, bypassDomainList = "", proxyName = "", proxyType = "", dnsOverProxyDisabled = false, udpDisabled = false){
        Network.postJson(`http://${hostIp}:18182/android_api/v1` + "/proxy_set/" + dbId)
        .add("ip", ip)
        .add("port", port)
        .add("account", account)
        .add("password", password)
        .addList("bypassDomainList", [])
        .add("proxyName", proxyName)
        .add("proxyType", proxyType)
        .add("dnsOverProxyDisabled", dnsOverProxyDisabled)
        .add("udpDisabled", udpDisabled)
        .setUserData(hostIp)
        .bind(root)
        .go(setDeviceProxy)
    }

    NetworkCallable {
        id: closeDeviceProxy
        onStart: {
            showLoading(qsTr("正在关闭代理..."))
        }
        onFinish: {
            hideLoading()
        }
        onError:
            (status, errorString, result, userData) => {
                console.debug(status + ";" + errorString + ";" + result)
                showError(errorString)
            }
        onSuccess:
            (result, userData) => {
                try {
                    const res = JSON.parse(result)
                    if(res.code == 200){
                        // 关闭代理成功，显示成功消息
                        showSuccess(qsTr("关闭代理成功"))

                        // 调用查询代理接口获取最新状态
                        reqGetDeviceProxy(root.modelData.hostIp, root.modelData.dbId)
                    }else{
                        showError(res.msg, 3000)
                    }
                } catch (e) {
                    console.warn("无法将行解析为JSON:", result, e)
                }
            }
    }

    // 取消代理
    function reqCloseDeviceProxy(hostIp, dbId){
        Network.get(`http://${hostIp}:18182/android_api/v1` + "/proxy_stop/" + dbId)
        .setUserData(hostIp)
        .bind(root)
        .go(closeDeviceProxy)
    }
}
