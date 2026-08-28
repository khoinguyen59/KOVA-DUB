import QtQuick
import QtQuick.Controls
import LAStudio

// Opens the production artifact-upload Dialog without the Dubbing entry gate.
// This is used only for visual QA of the popup itself; no alternate upload UI
// or backend path is introduced.
ApplicationWindow {
    id: previewRoot

    width: 1280
    height: 720
    visible: true
    title: "LA Studio - Upload artifact dialog preview"
    color: Theme.background

    DubbingArtifactUploadDialog {
        id: uploadDialog
        parent: previewRoot.contentItem
        dubbing: AppController.dubbing
        width: Math.min(760, previewRoot.width - Theme.paddingXL * 2)
        height: Math.min(650, previewRoot.height - Theme.paddingXL * 2)
        anchors.centerIn: parent
        background: Rectangle {
            color: Theme.surface
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42)
            border.width: 1
        }
    }

    Timer {
        interval: 700
        repeat: false
        running: true
        onTriggered: uploadDialog.openFor("transcribe")
    }

    Timer {
        interval: 5000
        repeat: false
        running: true
        onTriggered: Qt.quit()
    }
}
