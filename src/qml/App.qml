import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform
import FluentUI

FluLauncher {
    id: app
    Component.onCompleted: {
        // 创建截图目录
        const screenshotPath = StandardPaths.writableLocation(StandardPaths.PicturesLocation) + `/${AppConfig.projectName}`
        Utils.createDirectory(FluTools.toLocalPath(screenshotPath))
        console.log("本地截图目录", screenshotPath)

        const logPath = StandardPaths.writableLocation(StandardPaths.AppLocalDataLocation) + "/logs"
        console.log("log path", logPath)
        ReportHelper.init(AppConfig.reportHost, AppConfig.projectName, AppConfig.channel, AppConfig.versionName, AppConfig.versionCode)

        Network.openLog = true

        FluApp.init(app)
        FluApp.useSystemAppBar = false
        FluApp.windowIcon = AppConfig.projectIcon

        FluTheme.darkMode = FluThemeType.Light
        FluRouter.routes = {
            "/main":"qrc:/qml/MainWindow.qml",
            "/pad": "qrc:/qml/PadWindow.qml",
        }
        FluRouter.navigate("/main")
    }
    Component.onDestruction: {
    }
}
