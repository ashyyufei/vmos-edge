import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI
import Utils


Item {
    id: root
    property alias model : gridView.model
    property var groupsModel: []
    readonly property int horizontalSpacing: 8
    readonly property int verticalSpacing: 20
    property int itemWidth: 78
    property int itemHeight: 139
    property int viewDirection: 0
    readonly property int bottomSpace: 32

    signal clickMenuItem(string type, var model)
    signal visibleItemsChanged(var items)
    signal itemDestroy(string id)
    signal showContextMenuForItem(var model)
    
    // 启动 scrcpy_server（TCP直连模式需要先启动服务）
    function startScrcpyServerForDevice(hostIp, dbId, tcpVideoPort, tcpAudioPort, tcpControlPort, onSuccess, onError) {
        if (!hostIp || !dbId) {
            if (onError) onError("hostIp 或 dbId 为空")
            return
        }
        
        const url = `http://${hostIp}:18182/container_api/v1/scrcpy`
        console.log("启动 scrcpy_server:", url, "dbId:", dbId, "videoPort:", tcpVideoPort, "audioPort:", tcpAudioPort, "controlPort:", tcpControlPort)
        
        // 根据端口判断是否需要启动对应的流
        const bool_video = tcpVideoPort > 0
        const bool_audio = tcpAudioPort > 0
        const bool_control = tcpControlPort > 0
        
        Network.postJson(url)
        .bind(root)
        .setTimeout(5000)
        .add("db_id", dbId)
        .add("bool_video", bool_video)
        .add("bool_audio", bool_audio)
        .add("bool_control", bool_control)
        .go(startScrcpyServerCallable)
        
        // 保存回调函数
        startScrcpyServerCallable._onSuccess = onSuccess
        startScrcpyServerCallable._onError = onError
        startScrcpyServerCallable._hostIp = hostIp
        startScrcpyServerCallable._dbId = dbId
    }
    
    NetworkCallable {
        id: startScrcpyServerCallable
        property var _onSuccess: null
        property var _onError: null
        property string _hostIp: ""
        property string _dbId: ""
        
        onError: (status, errorString, result, userData) => {
            console.error("启动 scrcpy_server 失败:", status, errorString, result)
            if (_onError) {
                _onError(errorString || "启动 scrcpy_server 失败")
            }
        }
        
        onSuccess: (result, userData) => {
            var res = JSON.parse(result)
            if (res.code === 200) {
                console.log("启动 scrcpy_server 成功:", result)
                if (_onSuccess) {
                    _onSuccess(_hostIp, _dbId)
                }
            } else {
                console.error("启动 scrcpy_server 失败:", res.msg)
                if (_onError) {
                    _onError(res.msg || "启动 scrcpy_server 失败")
                }
            }
        }
    }

    // 添加定时器
    Timer {
        id: updateTimer
        interval: 500
        onTriggered: {
            var items = getVisibleItems()
            visibleItemsChanged(items)
        }
    }

    // 处理滚动和大小改变事件
    function handleViewChange() {
        updateTimer.restart()
    }

    function getVisibleItems() {
        var visibleItems = [];
        if (!gridView.contentItem || !model) {
            return visibleItems;
        }

        for (var i = 0; i < gridView.contentItem.children.length; i++) {
            var delegateItem = gridView.contentItem.children[i];
            var modelIndex = gridView.indexAt(delegateItem.x, delegateItem.y);

            // 将delegateItem的坐标映射到gridView的坐标系
            var topLeftInView = delegateItem.mapToItem(gridView, 0, 0);

            // 检查item是否在可视区域内
            if (topLeftInView.x + delegateItem.width > 0 && topLeftInView.x < gridView.width &&
                    topLeftInView.y + delegateItem.height > 0 && topLeftInView.y < gridView.height) {
                
                if (modelIndex !== -1 && delegateItem.visible) {
                    try {
                        var columnLayout = delegateItem.children[0];
                        if (columnLayout) {
                            visibleItems.push(delegateItem.dbId)
                        }
                    } catch (e) {
                        console.log("Error processing item", i, ":", e);
                    }
                }
            }
        }
        
        return visibleItems;
    }

    function getItemByPadCode(dbId) {
        if (!gridView.contentItem || !model) {
            return null;
        }
        
        // 首先尝试通过 modelIndex 精确匹配
        for (var i = 0; i < gridView.contentItem.children.length; i++) {
            var delegateItem = gridView.contentItem.children[i];
            var modelIndex = gridView.indexAt(delegateItem.x, delegateItem.y);
            
            if (modelIndex !== -1 && delegateItem.dbId === dbId) {
                // 验证这个 delegate 项是否真的对应正确的模型数据
                if (delegateItem.modelData && delegateItem.modelData.dbId === dbId) {
                    return delegateItem;
                }
            }
        }
        
        // 如果精确匹配失败，回退到简单的 dbId 匹配
        for (var i = 0; i < gridView.contentItem.children.length; i++) {
            var delegateItem = gridView.contentItem.children[i];
            var modelIndex = gridView.indexAt(delegateItem.x, delegateItem.y);
            if (modelIndex !== -1 && delegateItem.dbId === dbId) {
                return delegateItem;
            }
        }
        return null;
    }

    function updateScreenshotImage() {
        if (!gridView.contentItem || !model) {
            console.log("GridView contentItem or model not ready for screenshot update.");
            return;
        }

        var visibleCount = 0;
        var totalCount = gridView.contentItem.children.length;
        
        // 只遍历一次，同时进行可见性检查和截图更新
        for (var i = 0; i < totalCount; i++) {
            var delegateItem = gridView.contentItem.children[i];
            var modelIndex = gridView.indexAt(delegateItem.x, delegateItem.y);

            // 快速跳过无效项
            if (modelIndex === -1 || !delegateItem.visible) {
                continue;
            }

            // 将delegateItem的坐标映射到gridView的坐标系
            var topLeftInView = delegateItem.mapToItem(gridView, 0, 0);

            // 检查item是否在可视区域内
            if (topLeftInView.x + delegateItem.width > 0 && topLeftInView.x < gridView.width &&
                    topLeftInView.y + delegateItem.height > 0 && topLeftInView.y < gridView.height) {
                
                visibleCount++;
                
                if (delegateItem.img1 && delegateItem.modelData) {
                    var modelData = delegateItem.modelData;
                    // console.log("Updating screenshot for item", i, "dbId:", delegateItem.dbId, "modelData.name:", modelData?.name, "modelData.hostIp:", modelData?.hostIp);
                    if(modelData.state !== "running"){
                        continue
                    }

                    // 添加时间戳参数避免缓存，确保每个设备获取独立的截图
                    var timestamp = new Date().getTime();
                    var dbId = modelData?.dbId || modelData?.db_id || modelData?.name;
                    var screenshotUrl = `http://${modelData?.hostIp}:18182/container_api/v1/screenshots/${dbId}?t=${timestamp}`;
                    // console.log("===========url: ", dbId, screenshotUrl);
                    
                    // 只有当URL真正改变时才更新，避免重复请求
                    if (delegateItem.img1.imageUrl !== screenshotUrl) {
                        delegateItem.img1.imageUrl = screenshotUrl;
                    }
                } else {
                    // console.log("Item", i, "not ready for screenshot update:", "img1:", !!delegateItem.img1, "modelData:", !!delegateItem.modelData);
                }
            }
        }
        
        // console.log("Updated screenshots for", visibleCount, "visible items out of", totalCount, "total children");
    }

    function selectItemsInRect(x, y, width, height, container) {
        if (!gridView.contentItem) return;
        // var currentModel = model;
        // if (!currentModel) return;

        var selX1 = x;
        var selY1 = y;
        var selX2 = x + width;
        var selY2 = y + height;

        for (var i = 0; i < gridView.contentItem.children.length; ++i) {
            var delegateItem = gridView.contentItem.children[i];
            if (!delegateItem.visible) continue;

            var mappedPoint = delegateItem.mapToItem(container, 0, 0);
            
            var itemX1 = mappedPoint.x;
            var itemY1 = mappedPoint.y;
            var itemX2 = mappedPoint.x + delegateItem.width;
            var itemY2 = mappedPoint.y + delegateItem.height;

            if (selX1 < itemX2 && selX2 > itemX1 && selY1 < itemY2 && selY2 > itemY1) {
                if (delegateItem.modelData) {
                    console.log(`GridTileLayout: Selecting item '${delegateItem.modelData.padName}'. New checked state: ${!delegateItem.modelData.checked}`)
                    delegateItem.modelData.checked = !delegateItem.modelData.checked
                } else {
                    console.log(`GridTileLayout: Skipped item at loop index ${i} because its modelData is undefined.`)
                }
            }
        }
    }

    function isLessThan1Days(milliseconds) {
        var currentDate = new Date();  // 当前时间
        var currentTimestamp = currentDate.getTime();  // 当前时间戳（单位为毫秒）

        // 计算时间戳差值
        var timeDiff = currentTimestamp - milliseconds;  // 毫秒差值

        // 1天的毫秒数（1天 * 24小时 * 60分钟 * 60秒 * 1000毫秒）
        var threeDaysInMilliseconds = 1 * 24 * 60 * 60 * 1000;

        // 判断差值是否小于1天
        if (timeDiff < threeDaysInMilliseconds) {
            return true;  // 差值小于1天
        } else {
            return false;  // 差值不小于1天
        }
    }


    GridView {
        id: gridView
        anchors.fill: parent
        cellWidth: (root.viewDirection == 0 ? root.itemWidth  : root.itemHeight) + root.horizontalSpacing
        cellHeight: (root.viewDirection == 0 ? root.itemHeight : root.itemWidth) + root.bottomSpace + root.verticalSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        // 添加滚动事件监听
        onContentYChanged: handleViewChange()
        onContentXChanged: handleViewChange()

        // 添加大小改变事件监听
        onWidthChanged: handleViewChange()
        onHeightChanged: handleViewChange()

        // 添加视图大小事件监听
        onCellWidthChanged: handleViewChange()
        onCellHeightChanged: handleViewChange()

        // 监听数据改变事件监听
        Connections{
            target: model

            // function onAuthorizeFilterChanged(){
            //     console.log("代理模型触发了过滤")
            //     handleViewChange()
            // }

            // function onSortTypeChanged(){
            //     console.log("代理模型触发了排序")
            //     handleViewChange()
            // }

            function onRowsInserted(parent, first, last) {
                console.log("代理模型触发了新增信号")
                handleViewChange()
            }

            function onRowsRemoved(parent, first, last) {
                console.log("代理模型触发了移除信号")
                handleViewChange()
            }
        }

        delegate: Item {
            id: delegateRoot
            width: root.viewDirection == 0 ? root.itemWidth  : root.itemHeight
            height: (root.viewDirection == 0 ? root.itemHeight : root.itemWidth) + root.bottomSpace
            property string dbId: model?.dbId ?? ""
            property var modelData: model
            property string state: model?.state ?? ""
            property alias img1: img1

            onStateChanged: {
                if(state === "running"){
                    console.log("=================state changed to running")
                    img1.hasVideo = false
                }
            }



            Component.onDestruction: {
                itemDestroy(dbId)
            }

            ColumnLayout {
                anchors.fill: parent

                Item{
                    id: screenshotContainer
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    StackLayout{
                        currentIndex: model.state === "running" ? 0 : 1

                        ScreenshotRenderItem {
                            id: img1
                            Layout.preferredWidth: root.viewDirection == 0 ? root.itemWidth  : root.itemHeight
                            Layout.preferredHeight: root.viewDirection == 0 ? root.itemHeight : root.itemWidth
                            rotation: root.viewDirection == 0 ? 0 : 270
                            imageUrl: `http://${model?.hostIp}:18182/container_api/v1/screenshots/${model?.dbId || model?.db_id || model?.name}?t=${new Date().getTime()}`

                            Image{
                                anchors.fill: parent
                                visible: img1.hasVideo ? false : true
                                source: root.viewDirection == 0 ? ThemeUI.loadRes("pad/android10img.png") : ThemeUI.loadRes("pad/android10img2.png")
                            }
                        }

                        Item{
                            id: nonRunningItem
                            Image{
                                anchors.fill: parent
                                source: root.viewDirection == 0 ? ThemeUI.loadRes("pad/android10img.png") : ThemeUI.loadRes("pad/android10img2.png")

                                Rectangle{
                                    anchors.fill: parent
                                    color: "#80000000"
                                }
                            }

                            Text{
                                anchors.centerIn: parent
                                color: "white"
                                text: AppUtils.getStateStringBystate(model.state)
                            }
                        }
                    }

                    // 悬停效果只覆盖截图区域，不覆盖VCheckBox
                    Rectangle {
                        anchors.fill: parent
                        color: mouseArea.containsMouse ? "#80000000" : "transparent"
                        visible: true
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }

                    MouseArea{
                        id: mouseArea
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        onClicked:
                            (mouse)=> {
                                if(mouse.button === Qt.LeftButton){
                                    if(model.state !== "running"){
                                        return
                                    }
                                    
                                    // 根据配置选择连接模式
                                    const hostIp = model.hostIp || ""
                                    const adb = model.adb || 0
                                    const dbId = model.dbId || model.db_id || model.name || ""
                                    const ip = model.ip || ""
                                    const useDirectTcp = model.networkMode === "macvlan"
                                    const realIP = useDirectTcp ? ip : hostIp;
                                
                                    if (AppConfig.useDirectTcp) {
                                        // TCP直接连接模式：先启动 scrcpy_server，再连接
                                        const tcpVideoPort = useDirectTcp ? 9999 : (model.tcpVideoPort || 0)
                                        const tcpAudioPort = useDirectTcp ? 9998 : (model.tcpAudioPort || 0)
                                        const tcpControlPort = useDirectTcp ? 9997 : (model.tcpControlPort || 0)
                                        
                                        console.log("使用TCP直接连接模式:", hostIp, dbId, "ports:", tcpVideoPort, tcpAudioPort, tcpControlPort)
                                        
                                        console.log("scrcpy_server 启动成功，开始连接设备")
                                        deviceManager.connectDeviceDirectTcp(
                                            dbId,           // serial
                                            realIP,         // host
                                            tcpVideoPort,   // videoPort
                                            tcpAudioPort,   // audioPort
                                            tcpControlPort  // controlPort
                                        )
                                    } else {
                                        // ADB连接模式
                                        const deviceAddress = `${realIP}:${adb}`
                                        console.log("使用ADB连接模式:", deviceAddress)
                                        deviceManager.connectDevice(deviceAddress)
                                    }
                                    
                                    console.log("=================", model.dbId)
                                    FluRouter.navigate("/pad", model, undefined, model.dbId)
                                }else if(mouse.button === Qt.RightButton){
                                    if(model.state === "offline" || modelData.state === "creating"){
                                        return
                                    }

                                    root.showContextMenuForItem(model)
                                }
                            }
                    }
                }

                VCheckBox{
                    id: checkBox
                    Layout.preferredHeight: 27
                    // 不拉伸宽度，作为整体居中
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: (root.viewDirection == 0 ? root.itemWidth : root.itemHeight)
                    // 直接交给 VCheckBox 内部根据实际宽度裁剪
                    text: model.displayName
                    checked: model?.checked ?? false
                    // enabled: !groupControl.eventSync
                    fontSize: 12
                    // 限制文本区域最大宽度为自身 width，VCheckBox 内部会用 width 自适应裁剪
                    maxWidth: (root.viewDirection == 0 ? root.itemWidth : root.itemHeight)
                    onClicked: {
                        if (model.checked !== checked) {
                            model.checked = checked
                        }
                    }
                }
            }
        }
    }
}
