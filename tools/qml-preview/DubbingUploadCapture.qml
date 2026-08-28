import QtQuick
import LAStudio

// Captures the production Upload artifact dialog with the local preview shim.
// The QML tree comes from ../../qml; only backend data is replaced by the
// preview singleton, so this is a UI evidence harness rather than a second UI.
Main {
    id: previewRoot

    width: 1280
    height: 720
    visible: true
    minimumWidth: 960
    minimumHeight: 600
    initialPreviewRouteId: "studio-dubbing"

    function outputPath(name) {
        return "C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/ui-demo/"
                + name + ".png"
    }

    function captureObject(target, path) {
        var item = target && target.contentItem ? target.contentItem : target
        if (!item || typeof item.grabToImage !== "function")
            return false
        item.grabToImage(function(result) { result.saveToFile(path) })
        return true
    }

    Timer {
        interval: 1800
        repeat: false
        running: true
        onTriggered: {
            if (!previewRoot.qmlPreviewOpenDubbingArtifactUpload("transcribe")) {
                console.warn("Unable to open production Upload artifact dialog")
                Qt.quit()
                return
            }
            captureTimer.start()
        }
    }

    Timer {
        id: captureTimer
        interval: 700
        repeat: false
        onTriggered: {
            var captured = previewRoot.captureObject(
                        previewRoot.qmlPreviewDubbingArtifactUploadDialog(),
                        previewRoot.outputPath("dubbing-upload-dialog-1280x720"))
            if (!captured)
                console.warn("Unable to capture production Upload artifact dialog")
            // Keep the real popup open long enough for an OS-level window
            // capture. QQuickPopup is rendered in a separate popup layer and
            // grabToImage() on contentItem alone cannot prove its pixels.
            closeDialogTimer.start()
            finishTimer.start()
        }
    }

    Timer {
        id: closeDialogTimer
        interval: 8000
        repeat: false
        onTriggered: previewRoot.qmlPreviewCloseDubbingArtifactUpload()
    }

    Timer {
        id: finishTimer
        interval: 10000
        repeat: false
        onTriggered: Qt.quit()
    }
}
