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
    signal contextRequested(string contextId)
    signal configureNodeRequested(string nodeId)
    signal colabRequested(string nodeId)
    signal runStepRequested(string nodeId)
    signal runNextStepRequested(string nodeId)
    signal fixRequested()
    signal artifactUploadRequested(string nodeId)
    signal sourceUploadRequested()

    objectName: "dubbingTaskShelf"
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    Layout.minimumWidth: 220
    Layout.maximumWidth: 280
    Layout.preferredWidth: 248
    Layout.fillHeight: true

    function qmlSmokeFeatureActionsCheck() {
        return resultsButton.visible && settingsButton.visible && modelButton.visible
                && colabButton.visible && uploadButton.visible && handoffButton.visible
                && runButton.visible && backButton.visible && continueButton.visible
                && runButton.width >= 180
                && backButton.width >= 96
                && continueButton.width >= 96
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingSmall
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
                objectName: "dubbingTaskShelfHide"
                iconName: "chevron-left"
                iconOnly: true
                quiet: true
                toolTip: qsTr("Hide task controls")
                onClicked: root.hideRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summaryColumn.implicitHeight + Theme.paddingMedium
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)
            ColumnLayout {
                id: summaryColumn
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: root.stepTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: root.workflowNode ? (root.workflowNode.detail || root.workflowNode.state || qsTr("Ready for action")) : qsTr("Choose a task")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }
        }

        ScrollView {
            id: actionScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: actionScroll.availableWidth
                spacing: Theme.paddingSmall

                Text {
                    Layout.fillWidth: true
                    text: qsTr("FEATURES")
                    color: Theme.textSecondary
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1
                }

                PrimaryButton {
                    id: resultsButton
                    objectName: "dubbingFeatureResults"
                    Layout.fillWidth: true
                    text: qsTr("Results")
                    iconName: "check"
                    quiet: root.dubbing.processing
                    onClicked: root.contextRequested("results")
                }
                PrimaryButton {
                    id: settingsButton
                    objectName: "dubbingFeatureSettings"
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    iconName: "sliders"
                    quiet: true
                    onClicked: root.contextRequested("settings")
                }
                PrimaryButton {
                    id: modelButton
                    objectName: "dubbingFeatureModel"
                    Layout.fillWidth: true
                    text: root.workflowNode && root.workflowNode.configurable === true
                          ? qsTr("Open model") : qsTr("Task settings")
                    iconName: root.workflowNode && root.workflowNode.configurable === true
                               ? "grid" : "sliders"
                    onClicked: root.workflowNode && root.workflowNode.configurable === true
                               ? root.configureNodeRequested(root.displayedStepId)
                               : root.contextRequested("settings")
                }
                PrimaryButton {
                    id: colabButton
                    objectName: "dubbingFeatureColab"
                    Layout.fillWidth: true
                    text: qsTr("Colab")
                    iconName: "cloud"
                    quiet: true
                    onClicked: root.colabRequested(root.displayedStepId)
                }
                PrimaryButton {
                    id: uploadButton
                    objectName: "dubbingFeatureUpload"
                    Layout.fillWidth: true
                    text: qsTr("Upload")
                    iconName: "folder"
                    quiet: true
                    onClicked: root.displayedStepId === "import"
                               ? root.sourceUploadRequested()
                               : root.artifactUploadRequested(root.displayedStepId)
                }
                PrimaryButton {
                    id: handoffButton
                    objectName: "dubbingFeatureHandoff"
                    Layout.fillWidth: true
                    text: qsTr("Data & handoff")
                    iconName: "share"
                    quiet: true
                    onClicked: root.contextRequested("handoff")
                }

                PrimaryButton {
                    id: fixButton
                    Layout.fillWidth: true
                    visible: root.displayedStepId === "translate" || root.displayedStepId === "review-translation"
                    text: qsTr("Review fixes")
                    iconName: "magic"
                    quiet: true
                    onClicked: root.fixRequested()
                }
            }
        }

        PrimaryButton {
            id: runButton
            objectName: "dubbingTaskRunButton"
            Layout.fillWidth: true
            text: root.canRerunStep ? qsTr("Run task again") : qsTr("Run task")
            iconName: root.dubbing.processing ? "activity" : "play"
            loading: root.dubbing.processing
            enabled: !root.dubbing.processing && root.displayedStepId !== ""
            onClicked: root.runStepRequested(root.displayedStepId)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            PrimaryButton {
                id: backButton
                objectName: "dubbingTaskBackButton"
                text: qsTr("Back")
                iconName: "chevron-left"
                quiet: true
                Layout.preferredWidth: 100
                Layout.minimumWidth: 96
                onClicked: root.runNextStepRequested("__previous__")
            }
            PrimaryButton {
                id: continueButton
                objectName: "dubbingTaskContinueButton"
                Layout.fillWidth: true
                text: qsTr("Continue")
                iconName: "chevron-right"
                quiet: true
                enabled: root.nextNodeId !== ""
                onClicked: root.runNextStepRequested(root.nextNodeId)
            }
        }
    }
}
