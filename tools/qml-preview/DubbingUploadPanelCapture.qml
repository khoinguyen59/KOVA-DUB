import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

// Visual evidence harness for the production artifact-upload component.
// It deliberately bypasses Popup composition so the screenshot proves the
// actual file-picker controls without relying on a native popup capture.
ApplicationWindow {
    id: previewRoot

    width: 1280
    height: 720
    visible: true
    title: "LA Studio - Upload artifact preview"
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        Text {
            Layout.fillWidth: true
            text: qsTr("Upload workflow output")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontXLarge
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Artifact input for the next task — Colab is optional")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
        DubbingArtifactUploadPanel {
            id: uploadPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            dubbing: AppController.dubbing
            nodeId: "transcribe"
            contractSpec: ({
                nodeId: "stt",
                title: "STT transcript",
                description: "Choose a saved transcript to use as input for the next task.",
                expectedFiles: ["transcript.srt"],
                allowedExtensions: [".srt", ".vtt", ".ass", ".ssa", ".txt", ".md", ".markdown"],
                multiple: false
            })
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("The file picker validates the declared filename and format before handoff.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSmall
        }
    }

    Timer {
        interval: 5000
        repeat: false
        running: true
        onTriggered: Qt.quit()
    }
}
