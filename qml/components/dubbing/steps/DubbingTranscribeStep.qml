import QtQuick
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool ocrSetupEditable: true
    property bool stepComplete: false

    readonly property var transcriptConfig: root.dubbing
        ? (root.dubbing.transcriptConfiguration || {}) : ({})
    readonly property var sttSegments: root.transcriptConfig.sttSegments || []
    readonly property var ocrSegments: root.transcriptConfig.ocrSegments || []
    readonly property bool sttBusy: root.dubbing.speechToTextProcessing === true
    readonly property bool ocrBusy: root.dubbing.subtitleOcrProcessing === true
    readonly property bool sttSetupEditable: !root.dubbing.processing
        || root.dubbing.sttCanRunAlongsideSubtitleOcr
    readonly property bool sttCanStart: !root.sttBusy
        && (!root.dubbing.processing || root.dubbing.sttCanRunAlongsideSubtitleOcr)
    readonly property bool ocrCanStart: !root.ocrBusy
        && (!root.dubbing.processing || root.dubbing.subtitleOcrCanRunAlongsideStt)
    readonly property bool sttUploadAllowedNow: {
        var aggregateBusy = root.dubbing ? root.dubbing.processing : false
        if (!root.dubbing) return false
        return !aggregateBusy
            || root.dubbing.canImportWorkflowArtifactNow("stt")
    }
    readonly property bool ocrUploadAllowedNow: {
        var aggregateBusy = root.dubbing ? root.dubbing.processing : false
        if (!root.dubbing) return false
        return !aggregateBusy
            || root.dubbing.canImportWorkflowArtifactNow("subtitle-ocr")
    }
    readonly property bool hasAnyTranscript: root.sttSegments.length > 0
        || root.ocrSegments.length > 0
        || ((root.dubbing.segments || []).length > 0)

    signal configureRequested(string nodeId)
    signal openColabSetupRequested(string nodeId)
    signal artifactUploadRequested(string nodeId)
    // Compatibility signal for older page integrations. New actions use the
    // node-scoped signal above so STT and OCR stay independent.
    signal openOcrColabSetupRequested()
    signal runRequested(string nodeId)
    signal nextStepRequested()
    signal previousStepRequested()

    function statusText(count, busy) {
        if (busy) return qsTr("Running")
        if (count > 0) return qsTr("Completed · %1").arg(count)
        return qsTr("Not run")
    }

    function continueToNextStep() {
        // When both independent sources exist, make the reconciliation step
        // explicit in the user action. The service keeps all STT/OCR evidence
        // and uses STT as the deterministic canonical result when cues do not
        // match, so the next task never receives an arbitrary OCR overwrite.
        if (root.sttSegments.length > 0 && root.ocrSegments.length > 0) {
            if (!root.dubbing.reconcileTranscriptSources())
                return
        }
        root.nextStepRequested()
    }

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon {
                name: "mic"
                color: Theme.accentLight
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("4. TRANSCRIBE")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                text: root.hasAnyTranscript ? qsTr("Transcript ready") : qsTr("Choose one or both")
                color: root.hasAnyTranscript ? Theme.success : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
        }

        Rectangle {
            id: dubbingSttCard
            objectName: "dubbingSttCard"
            Layout.fillWidth: true
            implicitHeight: sttLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: root.sttBusy ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.10)
            border.width: root.sttBusy ? 2 : 1

            ColumnLayout {
                id: sttLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("STT · Speech-to-Text")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }
                    Text {
                        text: root.statusText(root.sttSegments.length, root.sttBusy)
                        color: root.sttBusy ? Theme.accentLight
                                            : (root.sttSegments.length > 0 ? Theme.success : Theme.textSecondary)
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.paddingSmall
                    rowSpacing: Theme.paddingSmall

                    PrimaryButton {
                        objectName: "dubbingSttModelButton"
                        text: qsTr("Model")
                        iconName: "settings"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.sttSetupEditable
                        toolTip: qsTr("Choose the Speech-to-Text model")
                        onClicked: root.configureRequested("transcribe")
                    }
                    PrimaryButton {
                        objectName: "dubbingSttColabButton"
                        text: qsTr("Colab")
                        iconName: "cloud"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.sttSetupEditable
                        toolTip: qsTr("Open the Speech-to-Text Colab setup")
                        onClicked: root.openColabSetupRequested("transcribe")
                    }
                    PrimaryButton {
                        objectName: "dubbingSttUploadButton"
                        text: qsTr("Upload")
                        iconName: "folder"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.sttUploadAllowedNow
                        toolTip: qsTr("Upload a saved STT transcript")
                        onClicked: root.artifactUploadRequested("stt")
                    }
                    PrimaryButton {
                        id: dubbingRunSttButton
                        objectName: "dubbingRunSttButton"
                        text: root.sttBusy ? qsTr("Running STT…") : qsTr("Run STT")
                        iconName: root.sttBusy ? "activity" : "play"
                        loading: root.sttBusy
                        Layout.fillWidth: true
                        buttonColor: Theme.accent
                        enabled: root.sttCanStart
                        onClicked: root.runRequested("stt")
                    }
                }
            }
        }

        Rectangle {
            id: dubbingOcrCard
            objectName: "dubbingOcrCard"
            Layout.fillWidth: true
            implicitHeight: ocrLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(0.20, 0.55, 0.95, 0.07)
            border.color: root.ocrBusy ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.10)
            border.width: root.ocrBusy ? 2 : 1

            ColumnLayout {
                id: ocrLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("OCR · Subtitle OCR")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.bold: true
                    }
                    Text {
                        text: root.statusText(root.ocrSegments.length, root.ocrBusy)
                        color: root.ocrBusy ? Theme.accentLight
                                            : (root.ocrSegments.length > 0 ? Theme.success : Theme.textSecondary)
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.paddingSmall
                    rowSpacing: Theme.paddingSmall

                    PrimaryButton {
                        objectName: "dubbingOcrModelButton"
                        text: qsTr("Model")
                        iconName: "settings"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.ocrSetupEditable
                        toolTip: qsTr("Choose the Subtitle OCR model and route")
                        onClicked: root.configureRequested("subtitle-ocr")
                    }
                    PrimaryButton {
                        objectName: "dubbingOcrColabButton"
                        text: qsTr("Colab")
                        iconName: "cloud"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.ocrSetupEditable
                        toolTip: qsTr("Open the Subtitle OCR Colab setup")
                        onClicked: root.openColabSetupRequested("subtitle-ocr")
                    }
                    PrimaryButton {
                        objectName: "dubbingOcrUploadButton"
                        text: qsTr("Upload")
                        iconName: "folder"
                        quiet: true
                        Layout.fillWidth: true
                        enabled: root.ocrUploadAllowedNow
                        toolTip: qsTr("Upload a saved OCR transcript")
                        onClicked: root.artifactUploadRequested("subtitle-ocr")
                    }
                    PrimaryButton {
                        id: dubbingRunOcrButton
                        objectName: "dubbingRunOcrButton"
                        text: root.ocrBusy ? qsTr("Running OCR…") : qsTr("Run OCR")
                        iconName: root.ocrBusy ? "activity" : "play"
                        loading: root.ocrBusy
                        Layout.fillWidth: true
                        buttonColor: Theme.accent
                        enabled: root.ocrCanStart
                        onClicked: root.runRequested("ocr")
                    }
                }
            }
        }

        Rectangle {
            visible: root.sttSegments.length > 0 && root.ocrSegments.length > 0
            Layout.fillWidth: true
            implicitHeight: fusionLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.035)
            border.color: Qt.rgba(1, 1, 1, 0.10)
            border.width: 1

            RowLayout {
                id: fusionLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall
                Text {
                    Layout.fillWidth: true
                    text: qsTr("STT and OCR are both ready")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
                PrimaryButton {
                    objectName: "dubbingReconcileTranscriptButton"
                    text: qsTr("Reconcile")
                    iconName: "merge"
                    quiet: true
                    enabled: !root.dubbing.processing
                    onClicked: root.dubbing.reconcileTranscriptSources()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            PrimaryButton {
                text: qsTr("Back")
                iconName: "chevron-left"
                quiet: true
                Layout.preferredHeight: 38
                Layout.preferredWidth: 100
                onClicked: root.previousStepRequested()
            }

            PrimaryButton {
                objectName: "dubbingTranscribeContinueButton"
                text: root.sttSegments.length > 0 && root.ocrSegments.length > 0
                      ? qsTr("Reconcile & Continue") : qsTr("Continue")
                iconName: "chevron-right"
                buttonColor: Theme.accent
                enabled: !root.dubbing.processing && root.hasAnyTranscript
                Layout.preferredHeight: 38
                Layout.fillWidth: true
                onClicked: root.continueToNextStep()
            }
        }
    }
}
