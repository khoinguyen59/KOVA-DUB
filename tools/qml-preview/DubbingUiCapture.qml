import QtQuick
import LAStudio

Main {
    id: previewRoot
    function argumentValue(name, fallback) {
        var args = Qt.application.arguments || []
        var index = args.indexOf(name)
        return index >= 0 && index + 1 < args.length ? args[index + 1] : fallback
    }

    readonly property int captureWidth: Math.max(960, parseInt(argumentValue("--width", "1280")))
    readonly property int captureHeight: Math.max(600, parseInt(argumentValue("--height", "720")))
    readonly property string captureSuffix: argumentValue("--suffix", "1280x720")

    width: captureWidth
    height: captureHeight
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

    function captureObject(target, path) {
        var item = target && target.contentItem ? target.contentItem : target
        if (item && item.parent && item.parent.width >= item.width
                && item.parent.height >= item.height)
            item = item.parent
        if (!item || typeof item.grabToImage !== "function")
            return false
        item.grabToImage(function(result) {
            result.saveToFile(path)
        })
        return true
    }

    function outputPath(name) {
        return "C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/ui-demo/dubbing-"
                + name + "-" + previewRoot.captureSuffix + ".png"
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
            previewRoot.capture(previewRoot.outputPath("preview"))
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
                        previewRoot.outputPath("drawer-results")))
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
                        previewRoot.outputPath("drawer-settings")))
                console.warn("Unable to capture Dubbing settings drawer")
            if (previewRoot.qmlPreviewSelectDubbingStep
                    && previewRoot.qmlPreviewSelectDubbingStep("alignment-subtitle"))
                alignmentCaptureTimer.start()
            else
                ocrTimer.start()
        }
    }

    Timer {
        id: alignmentCaptureTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!previewRoot.capture(previewRoot.outputPath("align")))
                console.warn("Unable to capture Dubbing Align state")
            if (previewRoot.qmlPreviewOpenDubbingColab
                    && previewRoot.qmlPreviewOpenDubbingColab())
                colabCaptureTimer.start()
            else
                ocrTimer.start()
        }
    }

    Timer {
        id: colabCaptureTimer
        interval: 850
        repeat: false
        onTriggered: {
            if (!previewRoot.captureObject(
                        previewRoot.qmlPreviewDubbingColabDialog
                            ? previewRoot.qmlPreviewDubbingColabDialog() : null,
                        previewRoot.outputPath("colab")))
                console.warn("Unable to capture Dubbing Colab setup")
            if (previewRoot.qmlPreviewCloseDubbingColab)
                previewRoot.qmlPreviewCloseDubbingColab()
            if (previewRoot.qmlPreviewOpenDubbingHistory
                    && previewRoot.qmlPreviewOpenDubbingHistory())
                historyCaptureTimer.start()
            else
                ocrTimer.start()
        }
    }

    Timer {
        id: historyCaptureTimer
        interval: 450
        repeat: false
        onTriggered: {
            if (!previewRoot.captureObject(
                        previewRoot.qmlPreviewDubbingOverlay
                            ? previewRoot.qmlPreviewDubbingOverlay() : null,
                        previewRoot.outputPath("history")))
                console.warn("Unable to capture Dubbing history overlay")
            if (previewRoot.qmlPreviewCloseDubbingHistory)
                previewRoot.qmlPreviewCloseDubbingHistory()
            ocrTimer.start()
        }
    }

    Timer {
        id: ocrTimer
        interval: 450
        repeat: false
        onTriggered: {
            if (previewRoot.qmlPreviewCloseDubbingContext)
                previewRoot.qmlPreviewCloseDubbingContext()
            if (previewRoot.qmlPreviewSelectDubbingStep)
                previewRoot.qmlPreviewSelectDubbingStep("transcribe")
            if (AppController.dubbing)
                AppController.dubbing.dubbingOcrRoiVisible = true
            ocrCaptureTimer.start()
        }
    }

    Timer {
        id: ocrCaptureTimer
        interval: 700
        repeat: false
        onTriggered: {
            if (!previewRoot.capture(previewRoot.outputPath("transcribe-ocr")))
                console.warn("Unable to capture Dubbing Transcribe/OCR state")
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
