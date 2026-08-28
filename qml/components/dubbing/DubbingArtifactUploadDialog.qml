import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

// A task-level manual handoff. It is intentionally opened from the persistent
// right task panel: each workflow task owns its declared Colab output contract
// and the controller still validates every path.
Dialog {
    id: root

    required property var dubbing
    property string requestedNodeId: ""
    property var specs: []

    parent: Overlay.overlay
    modal: true
    title: qsTr("Upload workflow output")
    width: Math.min(760, Overlay.overlay.width - Theme.paddingXL * 2)
    height: Math.min(650, Overlay.overlay.height - Theme.paddingXL * 2)
    anchors.centerIn: parent
    standardButtons: Dialog.Close

    function openFor(nodeId) {
        requestedNodeId = nodeId || ""
        specs = dubbing ? dubbing.workflowArtifactSpecsForStage(requestedNodeId) : []
        open()
    }

    function contractFiles() {
        var result = []
        for (var i = 0; i < root.specs.length; ++i) {
            var files = root.specs[i].expectedFiles || []
            for (var j = 0; j < files.length; ++j) {
                if (result.indexOf(files[j]) < 0)
                    result.push(files[j])
            }
        }
        return result.join(", ")
    }

    function contractFormats() {
        var result = []
        for (var i = 0; i < root.specs.length; ++i) {
            var extensions = root.specs[i].allowedExtensions || []
            for (var j = 0; j < extensions.length; ++j) {
                if (result.indexOf(extensions[j]) < 0)
                    result.push(extensions[j])
            }
        }
        return result.join(", ")
    }

    onOpened: specs = dubbing ? dubbing.workflowArtifactSpecsForStage(requestedNodeId) : []

    contentItem: ColumnLayout {
        spacing: Theme.paddingSmall
        Text {
            Layout.fillWidth: true
            text: qsTr("Colab is optional. Choose the exact output file(s) already saved on this computer; the app validates the name and format before continuing.")
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSmall
        }
        Text {
            objectName: "dubbingUploadFormatSummary"
            visible: root.specs.length > 0
            Layout.fillWidth: true
            text: qsTr("Required file(s): %1\nAllowed format(s): %2")
                  .arg(root.contractFiles()).arg(root.contractFormats())
            color: Theme.accentLight
            font.bold: true
            wrapMode: Text.WordWrap
        }
        ScrollView {
            id: artifactScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: artifactScroll.availableWidth
                spacing: Theme.paddingMedium
                Repeater {
                    model: root.specs
                    delegate: DubbingArtifactUploadPanel {
                        dubbing: root.dubbing
                        nodeId: modelData.nodeId || ""
                        Layout.fillWidth: true
                    }
                }
                Text {
                    visible: root.specs.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("No upload contract was found for task '%1'. Return to a result-producing task and press Upload there.").arg(root.requestedNodeId)
                    color: Theme.warning
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
