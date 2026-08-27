import QtQuick
import LAStudio

Main {
    id: previewRoot
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    initialPreviewRouteId: "studio-dubbing"

    function findByObjectName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        var children = item.children || []
        for (var i = 0; i < children.length; ++i) {
            var found = findByObjectName(children[i], name)
            if (found)
                return found
        }
        return null
    }

    function capture(path) {
        var items = previewRoot.contentItem ? previewRoot.contentItem.children : []
        for (var i = 0; i < items.length; ++i) {
            if (items[i] && typeof items[i].grabToImage === "function") {
                items[i].grabToImage(function(result) {
                    result.saveToFile(path)
                })
                return true
            }
        }
        return false
    }

    function captureDrawer(path) {
        var drawer = previewRoot.qmlPreviewDubbingDrawer
                ? previewRoot.qmlPreviewDubbingDrawer() : null
        if (!drawer)
            return false
        var target = drawer.contentItem || drawer
        if (typeof target.grabToImage !== "function")
            return false
        target.grabToImage(function(result) {
            result.saveToFile(path)
        })
        return true
    }

    Timer {
        interval: 1600
        running: true
        repeat: false
        onTriggered: {
            previewRoot.capture("C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/ui-demo/dubbing-preview-1280x720.png")
            if (previewRoot.qmlPreviewOpenDubbingContext
                    && previewRoot.qmlPreviewOpenDubbingContext("results")) {
                drawerCaptureTimer.start()
            } else {
                console.warn("No Dubbing context drawer available for preview capture")
            }
        }
    }

    Timer {
        id: drawerCaptureTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!previewRoot.captureDrawer(
                        "C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/ui-demo/dubbing-drawer-results-1280x720.png"))
                console.warn("Unable to capture Dubbing context drawer")
            settingsOpenTimer.start()
        }
    }

    Timer {
        id: settingsOpenTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (previewRoot.qmlPreviewOpenDubbingContext
                    && previewRoot.qmlPreviewOpenDubbingContext("settings"))
                settingsCaptureTimer.start()
            else
                quitTimer.start()
        }
    }

    Timer {
        id: settingsCaptureTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!previewRoot.captureDrawer(
                        "C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/ui-demo/dubbing-drawer-settings-1280x720.png"))
                console.warn("Unable to capture Dubbing settings drawer")
            quitTimer.start()
        }
    }

    Timer {
        id: quitTimer
        interval: 1200
        repeat: false
        onTriggered: Qt.quit()
    }
}
