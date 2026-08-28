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
    property var acceptedArtifactNodeIds: []
    signal artifactAccepted(string nodeId)
    signal skipRequested(string nodeId)

    parent: Overlay.overlay
    modal: true
    title: qsTr("Upload workflow output")
    width: Math.min(760, Overlay.overlay.width - Theme.paddingXL * 2)
    height: Math.min(650, Overlay.overlay.height - Theme.paddingXL * 2)
    anchors.centerIn: parent
    function openFor(nodeId) {
        requestedNodeId = nodeId || ""
        specs = dubbing ? dubbing.workflowArtifactSpecsForStage(requestedNodeId) : []
        acceptedArtifactNodeIds = []
        open()
    }

    function handleArtifactAccepted(nodeId) {
        var accepted = root.acceptedArtifactNodeIds.slice()
        var id = String(nodeId || "")
        if (id !== "" && accepted.indexOf(id) < 0)
            accepted.push(id)
        root.acceptedArtifactNodeIds = accepted
        if (accepted.length >= root.specs.length)
            root.artifactAccepted(id)
    }

    function canSkipTask() {
        var id = String(root.requestedNodeId || "").toLowerCase()
        return id !== "" && id !== "import" && id !== "media-input" && root.specs.length > 0
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
                        nodeId: String(modelData.nodeId || modelData.id || root.requestedNodeId)
                        contractSpec: modelData
                        Layout.fillWidth: true
                        onArtifactAccepted: root.handleArtifactAccepted(nodeId)
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
                Text {
                    objectName: "dubbingArtifactUploadProgress"
                    visible: root.specs.length > 1 && root.acceptedArtifactNodeIds.length > 0
                    Layout.fillWidth: true
                    text: qsTr("Accepted %1 of %2 outputs. Choose the remaining file(s) to continue.")
                          .arg(root.acceptedArtifactNodeIds.length).arg(root.specs.length)
                    color: Theme.success
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 62
        color: Theme.surfaceAlt
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall
            Text {
                Layout.fillWidth: true
                text: root.canSkipTask()
                      ? qsTr("Upload is optional; skip this task to continue.")
                      : qsTr("Choose a declared file or close this window.")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            PrimaryButton {
                objectName: "dubbingArtifactSkipButton"
                text: qsTr("Skip task & continue")
                iconName: "chevron-right"
                enabled: root.canSkipTask() && root.dubbing && !root.dubbing.processing
                toolTip: qsTr("Do not run this task; continue to the next task")
                onClicked: {
                    if (root.dubbing.skipWorkflowTask(root.requestedNodeId)) {
                        root.skipRequested(root.requestedNodeId)
                        root.close()
                    }
                }
            }
            PrimaryButton {
                objectName: "dubbingArtifactUploadCloseButton"
                text: qsTr("Close")
                quiet: true
                onClicked: root.close()
            }
        }
    }
}
