import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import ".."
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property string displayedStepId
    required property var workflowNode
    required property string stepTitle
    required property bool canRunStep
    required property bool canRerunStep
    required property bool stepRunReady
    required property string nextNodeId
    required property bool nextNodeReady
    property bool ocrSetupEditable: true

    signal hideRequested()
    signal configureNodeRequested(string nodeId)
    signal runStepRequested(string nodeId)
    signal runNextStepRequested(string nodeId)
    signal fixRequested()
    signal artifactUploadRequested(string nodeId)

    objectName: "dubbingTaskShelf"
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    Layout.minimumWidth: 220
    Layout.maximumWidth: 420
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            LineIcon { name: "workflow"; color: Theme.accentLight; Layout.preferredWidth: 17; Layout.preferredHeight: 17 }
            Text {
                Layout.fillWidth: true
                text: qsTr("TASK CONTROLS")
                color: Theme.textSecondary
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1
            }
            PrimaryButton {
                text: qsTr("Hide")
                iconName: "chevron-left"
                iconOnly: true
                quiet: true
                toolTip: qsTr("Hide task controls and details")
                onClicked: root.hideRequested()
            }
        }

        ScrollView {
            id: taskShelfScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                id: taskShelfContent
                width: taskShelfScroll.availableWidth
                spacing: Theme.paddingSmall

                DubbingNodeSettingsPanel {
                    id: taskShelfNodeSettings
                    objectName: "taskShelfNodeSettings"
                    dubbing: root.dubbing
                    nodeId: root.displayedStepId
                    node: root.workflowNode
                    nodeTitle: root.stepTitle
                    canRun: root.canRunStep
                    canRerun: root.canRerunStep
                    runReady: root.stepRunReady
                    nextNodeId: root.nextNodeId
                    nextReady: root.nextNodeReady
                    compact: true
                    visible: node !== null
                    onConfigureRequested: root.configureNodeRequested(nodeId)
                    onLoadRequested: root.dubbing.loadWorkflowNodeModel(nodeId)
                    onUnloadRequested: root.dubbing.unloadWorkflowNodeModel(nodeId)
                    onReloadRequested: root.dubbing.reloadWorkflowNodeModel(nodeId)
                    onRunRequested: root.runStepRequested(nodeId)
                    onNextRequested: root.runNextStepRequested(nodeId)
                    onFixRequested: root.fixRequested()
                    onArtifactUploadRequested: root.artifactUploadRequested(nodeId)
                }

                Rectangle {
                    id: dubbingTranslationInputPanel
                    objectName: "dubbingTranslationInputPanel"
                    visible: root.displayedStepId === "translate"
                    Layout.fillWidth: true
                    implicitHeight: translationInputLayout.implicitHeight + Theme.paddingSmall * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                    ColumnLayout {
                        id: translationInputLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingSmall
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("AI translation input")
                            color: Theme.textPrimary
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            text: (root.dubbing.transcriptConfiguration.transcriptSource || "stt") === "reconcile"
                                  ? qsTr("Translate uses the reviewed STT/OCR source after conflicts are resolved. The selected translation model converts it to the target language.")
                                  : qsTr("Translate uses the reviewed source transcript from the selected STT or OCR route.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    id: dubbingTranscriptSourcePanel
                    objectName: "dubbingTranscriptSourcePanel"
                    visible: root.displayedStepId === "transcribe"
                    Layout.fillWidth: true
                    implicitHeight: transcriptSourceLayout.implicitHeight + Theme.paddingSmall * 2
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                    ColumnLayout {
                        id: transcriptSourceLayout
                        anchors.fill: parent
                        anchors.margins: Theme.paddingSmall
                        spacing: Theme.paddingSmall
                        Text {
                            text: qsTr("Transcript source")
                            color: Theme.textPrimary
                            font.bold: true
                        }
                        ComboBox {
                            id: dubbingTranscriptSourceMode
                            objectName: "dubbingTranscriptSourceMode"
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "id"
                            model: [
                                { id: "stt", label: qsTr("Chỉ STT") },
                                { id: "ocr", label: qsTr("Chỉ OCR") },
                                { id: "reconcile", label: qsTr("Khớp STT + OCR") }
                            ]
                            currentIndex: {
                                var source = root.dubbing.transcriptConfiguration.transcriptSource || "stt"
                                if (source === "stt+ocr") source = "reconcile"
                                for (var i = 0; i < model.length; ++i)
                                    if (model[i].id === source) return i
                                return 0
                            }
                            enabled: root.ocrSetupEditable
                            onActivated: function(index) {
                                root.dubbing.setWorkflowNodeParameters("transcribe", {
                                    transcriptSource: model[index].id
                                })
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: (root.dubbing.transcriptConfiguration.transcriptSource || "stt") === "ocr"
                                  ? qsTr("Uses Subtitle OCR with the selected execution route.")
                                  : (root.dubbing.transcriptConfiguration.transcriptSource || "stt") === "reconcile"
                                    ? qsTr("Khớp STT và OCR.")
                                    : qsTr("Uses audio STT.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
