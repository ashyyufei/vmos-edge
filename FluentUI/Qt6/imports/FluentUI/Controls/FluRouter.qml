pragma Singleton

import QtQuick
import FluentUI

QtObject {
    property var routes : ({})
    property var windows: []

    function addWindow(window){
        if(!window.transientParent){
            windows.push(window)
        }
    }

    function removeWindow(win) {
        if(!win.transientParent){
            var index = windows.indexOf(win)
            if (index !== -1) {
                windows.splice(index, 1)
                win.deleteLater()
            }
        }
    }

    function exit(retCode){
        for(var i =0 ;i< windows.length; i++){
            var win = windows[i]
            win.deleteLater()
        }
        windows = []
        Qt.exit(retCode)
    }

    // 检查窗口是否存在
    function hasWindow(route) {
        for(var i = 0; i < windows.length; i++) {
            if(windows[i]._route === route) {
                return true
            }
        }
        return false
    }

    // 关闭所有窗口
    function closeAllWindows(route) {
        windows = windows.filter(function(win) {
            if(win._route === route) {
                win.deleteLater()
                return false
            }
            return true
        })
    }

    // 检查指定 fingerprint 的窗口是否存在
    function hasWindowByFingerprint(fingerprint) {
        if (fingerprint === undefined || fingerprint === null || fingerprint === "") {
            return false
        }
        for(var i = 0; i < windows.length; i++) {
            if(windows[i]._fingerprint === fingerprint) {
                return true
            }
        }
        return false
    }

    // 关闭指定 fingerprint 的窗口
    function closeWindowByFingerprint(fingerprint) {
        if (fingerprint === undefined || fingerprint === null || fingerprint === "") {
            return false
        }
        var found = false
        windows = windows.filter(function(win) {
            if(win._fingerprint === fingerprint) {
                win.close()
                win.deleteLater()
                found = true
                return false
            }
            return true
        })
        return found
    }

    // 排列窗口
    function arrangeWindows(route, screenInfo) {
        if (!screenInfo) {
            return;
        }

        // virtualX/virtualY give the screen's top-left in the virtual desktop.
        var screenX = screenInfo.virtualX;
        var screenY = screenInfo.virtualY;
        var screenWidth = screenInfo.width;

        var windowsToArrange = windows.filter(function(win) { return win._route === route; });
        if (windowsToArrange.length === 0) return;

        var currentX = screenX;
        var currentY = screenY;
        var currentRowHeight = 0;

        for (var i = 0; i < windowsToArrange.length; i++) {
            var win = windowsToArrange[i];

            if (currentX > screenX && currentX + win.width > screenX + screenWidth) {
                currentY += currentRowHeight;
                currentX = screenX;
                currentRowHeight = 0;
            }

            win.x = currentX;
            win.y = currentY;

            currentX += win.width;
            currentRowHeight = Math.max(currentRowHeight, win.height);

            win.show();
            win.raise();
            win.requestActivate();
        }
    }


    function navigate(route, argument={}, windowRegister = undefined, fingerprint = undefined){
        if(!routes.hasOwnProperty(route)){
            console.error("Not Found Route",route)
            return
        }

        // 2. 首先尝试按指纹查找窗口
        var targetWindow = undefined;
        if (fingerprint !== undefined) {
            for(var i = 0; i < windows.length; i++){
                var item = windows[i];
                console.log("check fingerprint", item._fingerprint)
                if(item._fingerprint === fingerprint){
                    targetWindow = item;
                    console.log("find sameple window ", fingerprint)
                    break;
                }
            }

            // 3. 如果找到了指纹匹配的窗口
            if (targetWindow) {
                // 激活并更新参数，然后函数结束
                // targetWindow.argument = argument;
                targetWindow.show();
                targetWindow.raise();
                targetWindow.requestActivate();
                return;
            }
        }

        var windowComponent = Qt.createComponent(routes[route])
        if (windowComponent.status !== Component.Ready) {
            console.error(windowComponent.errorString())
            return
        }

        var properties = {}
        properties._route = route
        if(fingerprint){
            properties._fingerprint = fingerprint // 将指纹参数也传递给新窗口的属性
        }
        if(windowRegister){
            properties._windowRegister = windowRegister
        }
        properties.argument = argument

        var win = undefined
        for(var i = 0 ;i< windows.length; i++){
            var item = windows[i]
            if(route === item._route){
                win = item
                break
            }
        }

        if(win){
            var launchMode = win.launchMode
            if(launchMode === 1){
                win.argument = argument
                win.show()
                win.raise()
                win.requestActivate()
                return
            }else if(launchMode === 2){
                win.close()
            }
        }

        win = windowComponent.createObject(null,properties)
        if(windowRegister){
            windowRegister._to = win
        }
    }
}
