pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio
import "../base/colabNotebookUrls.js" as ColabNotebookUrls

// One Dubbing-only setup surface for temporary Direct Colab workers.  It is
// intentionally a view over AppController's session objects: URL/token never
// pass through project persistence, Settings, a Gateway client, or this QML
// component after a connection starts.
Dialog {
    id: root

    required property var dubbing
    // When opened from Automatic preflight, only the selected Direct Colab
    // workers are displayed. The general Dubbing settings surface leaves this
    // empty and continues to show all available capability cards.
    property var stageIds: []
    property var draftUrls: ({})
    property var draftTokens: ({})
    property string unifiedWorkerUrl: ""
    property string unifiedToken: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(980, parent ? parent.width - Theme.paddingXL * 2 : 980)
    height: Math.min(760, parent ? parent.height - Theme.paddingXL * 2 : 760)
    modal: true
    padding: 0
    title: ""
    closePolicy: Popup.CloseOnEscape

    function sessionForStage(stageId) {
        if (stageId === "source-separate") return AppController.colabSeparationSession
        if (stageId === "transcribe") return AppController.colabSttSession
        if (stageId === "subtitle-ocr") return AppController.colabSubtitleOcrSession
        if (stageId === "translate") return AppController.colabTranslationSession
        if (stageId === "synthesize") return AppController.colabTtsSession
        if (stageId === "alignment") return AppController.colabAlignmentSession
        return null
    }

    function draftUrl(stageId, session) {
        var value = draftUrls[stageId]
        return value === undefined ? (session ? session.workerUrl : "") : value
    }

    function setDraftUrl(stageId, value) {
        var next = Object.assign({}, draftUrls)
        next[stageId] = value
        draftUrls = next
    }

    function draftToken(stageId) { return draftTokens[stageId] || "" }

    function setDraftToken(stageId, value) {
        var next = Object.assign({}, draftTokens)
        next[stageId] = value
        draftTokens = next
    }

    function isScopedToStages() {
        return root.stageIds && root.stageIds.length > 0
    }

    function includesStage(stageId) {
        return !root.isScopedToStages() || root.stageIds.indexOf(stageId) >= 0
    }

    function includesAnyStage(stageList) {
        if (!root.isScopedToStages()) return true
        for (var i = 0; i < stageList.length; ++i) {
            if (root.stageIds.indexOf(stageList[i]) >= 0) return true
        }
        return false
    }

    function transcriptSourceMode() {
        var source = root.dubbing && root.dubbing.transcriptConfiguration
                ? root.dubbing.transcriptConfiguration.transcriptSource : "stt"
        return source === "stt+ocr" ? "reconcile" : (source || "stt")
    }

    // Reconcile is an explicit two-worker action. It must never fall back to
    // the single Unified URL field: STT and OCR have different capabilities,
    // models and verification sessions.
    function transcriptRoutesRequested() {
        return root.transcriptSourceMode() === "reconcile"
                || (root.isScopedToStages()
                    && root.includesAnyStage(["transcribe", "subtitle-ocr"]))
    }

    function showUnifiedRoutePanel() {
        return !root.transcriptRoutesRequested()
    }

    function showSttRoute() {
        var mode = root.transcriptSourceMode()
        return root.transcriptRoutesRequested()
                && (mode === "stt" || mode === "reconcile"
                    || (root.isScopedToStages() && root.includesStage("transcribe")))
                && (!root.isScopedToStages()
                    || root.includesStage("transcribe")
                    || mode === "reconcile")
    }

    function showOcrRoute() {
        var mode = root.transcriptSourceMode()
        return root.transcriptRoutesRequested()
                && (mode === "ocr" || mode === "reconcile"
                    || (root.isScopedToStages() && root.includesStage("subtitle-ocr")))
                && (!root.isScopedToStages()
                    || root.includesStage("subtitle-ocr")
                    || mode === "reconcile")
    }

    function hasSelectedStageOutsideScope() {
        if (!root.isScopedToStages()) return false
        var stages = dubbing ? (dubbing.colabSetupStages || []) : []
        for (var i = 0; i < stages.length; ++i) {
            if (stages[i].selectedForDirectColab
                    && root.stageIds.indexOf(stages[i].id) < 0)
                return true
        }
        return false
    }

    function checkVisibleStages() {
        if (!root.isScopedToStages()) {
            root.dubbing.validateAllWorkflowColabStages()
            return
        }
        for (var i = 0; i < root.stageIds.length; ++i)
            root.dubbing.checkWorkflowColabStage(root.stageIds[i])
    }

    function qmlSmokeScopedStageCheck(stageId) {
        var previousStageIds = root.stageIds
        root.stageIds = [stageId]
        // Dialog content can remain unpolished while the popup is closed, so
        // the smoke contract checks the same scope predicates used by the
        // bindings instead of reading child Item.visible too early.
        var transcriptVisible = root.includesAnyStage(["transcribe", "subtitle-ocr"])
        var ttsVisible = root.includesStage("synthesize")
        var ocrVisible = root.includesStage("subtitle-ocr")
        var result = transcriptVisible === (stageId === "transcribe" || stageId === "subtitle-ocr")
                && ttsVisible === (stageId === "synthesize")
                && ocrVisible === (stageId === "subtitle-ocr")
        root.stageIds = previousStageIds
        return result
    }

    function selectedDirectColabStageCount() {
        var stages = dubbing ? dubbing.colabSetupStages : []
        var count = 0
        for (var i = 0; i < stages.length; ++i)
            if (stages[i].selectedForDirectColab && root.includesStage(stages[i].id)) ++count
        return count
    }

    function stageForId(stageId) {
        var stages = dubbing ? (dubbing.colabSetupStages || []) : []
        for (var i = 0; i < stages.length; ++i)
            if (stages[i].id === stageId) return stages[i]
        return null
    }

    function stageModelForId(stageId) {
        var stage = root.stageForId(stageId)
        return stage ? (stage.modelId || "") : ""
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium

            LineIcon {
                name: "cloud"
                color: Theme.accentLight
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Dubbing Direct Colab setup")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                }
            }
            Button {
                implicitWidth: 32
                implicitHeight: 32
                onClicked: root.close()
                contentItem: LineIcon { anchors.centerIn: parent; name: "close"; color: Theme.textSecondary; width: 16; height: 16 }
                background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent" }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surfaceAlt }

        Rectangle {
            id: transcriptSourcePanel
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            implicitHeight: transcriptSourceLayout.implicitHeight + Theme.paddingMedium * 2
            visible: root.includesAnyStage(["transcribe", "subtitle-ocr"])
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
            border.width: 1

            ColumnLayout {
                id: transcriptSourceLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Next transcript action")
                    color: Theme.textPrimary
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: colabTranscriptSourceMode
                        objectName: "dubbingColabTranscriptSourceMode"
                        Layout.preferredWidth: 230
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
                        enabled: !root.dubbing.processing
                        onActivated: function(index) {
                            root.dubbing.setWorkflowNodeParameters("transcribe", {
                                transcriptSource: model[index].id
                            })
                        }
                    }
                }
            }
        }

        Rectangle {
            id: unifiedRoutePanel
            Layout.fillWidth: true
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingLarge
            implicitHeight: unifiedLayout.implicitHeight + Theme.paddingMedium * 2
            visible: root.showUnifiedRoutePanel()
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.06)
            border.color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35)
            border.width: 1

            ColumnLayout {
                id: unifiedLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Optional system route: Unified Dubbing Colab")
                    color: Theme.textPrimary
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    PrimaryButton {
                        objectName: "dubbingOpenUnifiedColabButton"
                        text: qsTr("Open Unified Colab")
                        iconName: "cloud"
                        implicitWidth: 180
                        onClicked: Qt.openUrlExternally(
                                       ColabNotebookUrls.forNotebookFile(
                                           "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb"))
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedDirectColabStageCount() > 0
                              ? qsTr("%1 selected").arg(root.selectedDirectColabStageCount())
                              : qsTr("Unified route")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    TextField {
                        id: unifiedUrlField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Unified worker URL (https://…trycloudflare.com)")
                        text: root.unifiedWorkerUrl
                        selectByMouse: true
                        onTextEdited: root.unifiedWorkerUrl = text
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: unifiedUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                    }
                    TextField {
                        id: unifiedTokenField
                        Layout.preferredWidth: 220
                        placeholderText: qsTr("Temporary bearer token")
                        text: root.unifiedToken
                        echoMode: TextInput.Password
                        selectByMouse: true
                        onTextEdited: root.unifiedToken = text
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textSecondary
                        background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: unifiedTokenField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                    }
                    PrimaryButton {
                        text: qsTr("Connect selected stages")
                        iconName: "link"
                        implicitWidth: 180
                        enabled: root.selectedDirectColabStageCount() > 0
                                 && !root.hasSelectedStageOutsideScope()
                                 && unifiedUrlField.text.trim() !== ""
                                 && unifiedTokenField.text !== ""
                                 && !root.dubbing.colabSetupChecking
                        onClicked: {
                            if (root.dubbing.connectUnifiedWorkflowColab(unifiedUrlField.text.trim(), unifiedTokenField.text))
                                root.unifiedToken = ""
                        }
                    }
                }

                GridLayout {
                    id: workerRouteGrid
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: Theme.paddingSmall
                    columnSpacing: Theme.paddingSmall

                    Text {
                        id: ttsRouteLabel
                        visible: root.includesStage("synthesize")
                        text: qsTr("TTS")
                        color: Theme.textSecondary
                        font.bold: true
                        Layout.preferredWidth: 42
                    }
                    RowLayout {
                        id: ttsRouteRow
                        visible: root.includesStage("synthesize")
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        TextField {
                            id: dubbingTtsWorkerUrlField
                            objectName: "dubbingTtsWorkerUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("TTS worker URL")
                            text: root.draftUrl("synthesize", root.sessionForStage("synthesize"))
                            selectByMouse: true
                            onTextEdited: root.setDraftUrl("synthesize", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: dubbingTtsWorkerUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        TextField {
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("TTS token")
                            text: root.draftToken("synthesize")
                            echoMode: TextInput.Password
                            selectByMouse: true
                            onTextEdited: root.setDraftToken("synthesize", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        PrimaryButton {
                            text: qsTr("Connect")
                            iconName: "link"
                            implicitWidth: 104
                            enabled: dubbingTtsWorkerUrlField.text.trim() !== ""
                                     && root.draftToken("synthesize") !== ""
                                     && root.stageModelForId("synthesize") !== ""
                                     && !root.dubbing.colabSetupChecking
                            onClicked: {
                                if (root.dubbing.connectWorkflowColabStage(
                                        "synthesize", root.stageModelForId("synthesize"),
                                        dubbingTtsWorkerUrlField.text.trim(),
                                        root.draftToken("synthesize")))
                                    root.setDraftToken("synthesize", "")
                            }
                        }
                    }

                    Text {
                        id: ocrRouteLabel
                        visible: root.includesStage("subtitle-ocr")
                        text: qsTr("OCR")
                        color: Theme.textSecondary
                        font.bold: true
                        Layout.preferredWidth: 42
                    }
                    RowLayout {
                        id: ocrRouteRow
                        visible: root.includesStage("subtitle-ocr")
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        TextField {
                            id: dubbingOcrWorkerUrlField
                            objectName: "dubbingOcrWorkerUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("OCR worker URL")
                            text: root.draftUrl("subtitle-ocr", root.sessionForStage("subtitle-ocr"))
                            selectByMouse: true
                            onTextEdited: root.setDraftUrl("subtitle-ocr", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: dubbingOcrWorkerUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        TextField {
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("OCR token")
                            text: root.draftToken("subtitle-ocr")
                            echoMode: TextInput.Password
                            selectByMouse: true
                            onTextEdited: root.setDraftToken("subtitle-ocr", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        PrimaryButton {
                            text: qsTr("Connect")
                            iconName: "link"
                            implicitWidth: 104
                            enabled: dubbingOcrWorkerUrlField.text.trim() !== ""
                                     && root.draftToken("subtitle-ocr") !== ""
                                     && root.stageModelForId("subtitle-ocr") !== ""
                                     && !root.dubbing.colabSetupChecking
                            onClicked: {
                                if (root.dubbing.connectWorkflowColabStage(
                                        "subtitle-ocr", root.stageModelForId("subtitle-ocr"),
                                        dubbingOcrWorkerUrlField.text.trim(),
                                        root.draftToken("subtitle-ocr")))
                                    root.setDraftToken("subtitle-ocr", "")
                            }
                        }
                    }
                }
            }
        }

        // STT + OCR use separate workers and separate verification sessions.
        // Keep this card compact so selecting reconciliation does not reserve
        // the large green Unified Dubbing area for a route that is not used.
        Rectangle {
            id: transcriptWorkerRoutesPanel
            Layout.fillWidth: true
            Layout.leftMargin: Theme.paddingLarge
            Layout.rightMargin: Theme.paddingLarge
            implicitHeight: transcriptWorkerRoutesLayout.implicitHeight + Theme.paddingSmall * 2
            visible: root.transcriptRoutesRequested()
            radius: Theme.radiusSmall
            color: Theme.surfaceAlt
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
            border.width: 1

            ColumnLayout {
                id: transcriptWorkerRoutesLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Transcript workers")
                        color: Theme.textPrimary
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    PrimaryButton {
                        text: qsTr("Open STT Colab")
                        iconName: "cloud"
                        quiet: true
                        visible: root.showSttRoute()
                        onClicked: Qt.openUrlExternally(
                                       ColabNotebookUrls.forNotebookFile(
                                           root.dubbing.colabNotebookForNode(
                                               "transcribe", root.stageModelForId("transcribe"))))
                    }
                    PrimaryButton {
                        text: qsTr("Open OCR Colab")
                        iconName: "cloud"
                        quiet: true
                        visible: root.showOcrRoute()
                        onClicked: Qt.openUrlExternally(
                                       ColabNotebookUrls.forNotebookFile(
                                           root.dubbing.colabNotebookForNode(
                                               "subtitle-ocr", root.stageModelForId("subtitle-ocr"))))
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: Theme.paddingSmall
                    columnSpacing: Theme.paddingSmall

                    Text {
                        visible: root.showSttRoute()
                        text: qsTr("STT")
                        color: Theme.textSecondary
                        font.bold: true
                        Layout.preferredWidth: 42
                    }
                    RowLayout {
                        visible: root.showSttRoute()
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        TextField {
                            id: dubbingSttWorkerUrlField
                            objectName: "dubbingSttWorkerUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("STT worker URL")
                            text: root.draftUrl("transcribe", root.sessionForStage("transcribe"))
                            selectByMouse: true
                            onTextEdited: root.setDraftUrl("transcribe", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: dubbingSttWorkerUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        TextField {
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("STT token")
                            text: root.draftToken("transcribe")
                            echoMode: TextInput.Password
                            selectByMouse: true
                            onTextEdited: root.setDraftToken("transcribe", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        PrimaryButton {
                            text: qsTr("Connect")
                            iconName: "link"
                            implicitWidth: 104
                            enabled: dubbingSttWorkerUrlField.text.trim() !== ""
                                     && root.draftToken("transcribe") !== ""
                                     && root.stageModelForId("transcribe") !== ""
                                     && !root.dubbing.colabSetupChecking
                            onClicked: {
                                if (root.dubbing.connectWorkflowColabStage(
                                        "transcribe", root.stageModelForId("transcribe"),
                                        dubbingSttWorkerUrlField.text.trim(),
                                        root.draftToken("transcribe")))
                                    root.setDraftToken("transcribe", "")
                            }
                        }
                    }

                    Text {
                        visible: root.showOcrRoute()
                        text: qsTr("OCR")
                        color: Theme.textSecondary
                        font.bold: true
                        Layout.preferredWidth: 42
                    }
                    RowLayout {
                        visible: root.showOcrRoute()
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        TextField {
                            id: dubbingTranscriptOcrWorkerUrlField
                            objectName: "dubbingTranscriptOcrWorkerUrlField"
                            Layout.fillWidth: true
                            placeholderText: qsTr("OCR worker URL")
                            text: root.draftUrl("subtitle-ocr", root.sessionForStage("subtitle-ocr"))
                            selectByMouse: true
                            onTextEdited: root.setDraftUrl("subtitle-ocr", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: dubbingTranscriptOcrWorkerUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        TextField {
                            Layout.preferredWidth: 180
                            placeholderText: qsTr("OCR token")
                            text: root.draftToken("subtitle-ocr")
                            echoMode: TextInput.Password
                            selectByMouse: true
                            onTextEdited: root.setDraftToken("subtitle-ocr", text)
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textSecondary
                            background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                        }
                        PrimaryButton {
                            text: qsTr("Connect")
                            iconName: "link"
                            implicitWidth: 104
                            enabled: dubbingTranscriptOcrWorkerUrlField.text.trim() !== ""
                                     && root.draftToken("subtitle-ocr") !== ""
                                     && root.stageModelForId("subtitle-ocr") !== ""
                                     && !root.dubbing.colabSetupChecking
                            onClicked: {
                                if (root.dubbing.connectWorkflowColabStage(
                                        "subtitle-ocr", root.stageModelForId("subtitle-ocr"),
                                        dubbingTranscriptOcrWorkerUrlField.text.trim(),
                                        root.draftToken("subtitle-ocr")))
                                    root.setDraftToken("subtitle-ocr", "")
                            }
                        }
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.paddingLarge
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                Repeater {
                    id: stageRepeater
                    // Capture the outer dialog once.  A Repeater delegate has
                    // its own component scope, so resolving the `root` id from
                    // an event handler is not reliable on every Qt build.
                    readonly property var setupDialog: root
                    model: (root.stageIds && root.stageIds.length > 0)
                           ? root.dubbing.colabSetupStages.filter(function(stage) {
                               return root.stageIds.indexOf(stage.id) >= 0
                           })
                           : root.dubbing.colabSetupStages

                    delegate: Rectangle {
                        id: stageCard
                        required property var modelData
                        readonly property var setupDialog: stageRepeater.setupDialog
                        readonly property string stageId: modelData.id || ""
                        readonly property var stageSession: stageRepeater.setupDialog.sessionForStage(stageId)
                        Layout.fillWidth: true
                        implicitHeight: stageLayout.implicitHeight + Theme.paddingMedium * 2
                        radius: Theme.radiusSmall
                        color: Qt.rgba(1, 1, 1, 0.025)
                        border.color: modelData.verified
                                      ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.46)
                                      : (modelData.selectedForDirectColab
                                         ? Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.42)
                                         : Qt.rgba(1, 1, 1, 0.10))
                        border.width: 1

                        ColumnLayout {
                            id: stageLayout
                            anchors.fill: parent
                            anchors.margins: Theme.paddingMedium
                            spacing: Theme.paddingSmall

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall
                                Text {
                                    Layout.fillWidth: true
                                    text: stageCard.modelData.title || stageCard.stageId
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    font.bold: true
                                }
                                Text {
                                    text: stageCard.modelData.verified
                                          ? qsTr("Verified exact worker")
                                          : (stageCard.modelData.selectedForDirectColab
                                             ? qsTr("Direct Colab needs verification")
                                             : qsTr("Optional — current route is not Direct Colab"))
                                    color: stageCard.modelData.verified ? Theme.success
                                           : (stageCard.modelData.selectedForDirectColab ? Theme.warning : Theme.textSecondary)
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: stageCard.stageId === "transcribe" || stageCard.stageId === "subtitle-ocr"
                                text: stageCard.modelData.requiredForCurrentTranscriptAction
                                      ? qsTr("Required")
                                      : qsTr("Optional")
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall
                                Text { text: qsTr("Exact model"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                                ComboBox {
                                    id: modelBox
                                    Layout.preferredWidth: 315
                                    textRole: "displayName"
                                    model: stageCard.setupDialog.dubbing.colabModelOptionsForNode(stageCard.stageId)
                                    currentIndex: {
                                        for (var i = 0; i < model.length; ++i)
                                            if (model[i].modelId === stageCard.modelData.modelId) return i
                                        return -1
                                    }
                                    onActivated: function(index) {
                                        stageCard.setupDialog.dubbing.selectWorkflowColabModel(stageCard.stageId, model[index].modelId)
                                    }
                                    enabled: true
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: stageCard.modelData.capability || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            ColabNotebookLink {
                                notebookFile: stageCard.modelData.notebookFile || ""
                                enabled: true
                                opacity: 1.0
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.paddingSmall
                                TextField {
                                    id: workerUrlField
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Temporary worker URL (https://…trycloudflare.com)")
                                    text: stageCard.setupDialog.draftUrl(stageCard.stageId, stageCard.stageSession)
                                    selectByMouse: true
                                    onTextEdited: stageCard.setupDialog.setDraftUrl(stageCard.stageId, text)
                                    enabled: true
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: workerUrlField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                                }
                                TextField {
                                    id: tokenField
                                    Layout.preferredWidth: 220
                                    placeholderText: qsTr("Temporary bearer token")
                                    text: stageCard.setupDialog.draftToken(stageCard.stageId)
                                    echoMode: TextInput.Password
                                    selectByMouse: true
                                    onTextEdited: stageCard.setupDialog.setDraftToken(stageCard.stageId, text)
                                    enabled: true
                                    color: Theme.textPrimary
                                    placeholderTextColor: Theme.textSecondary
                                    background: Rectangle { radius: Theme.radiusSmall; color: Theme.surface; border.color: tokenField.activeFocus ? Theme.accent : Theme.surfaceAlt; border.width: 1 }
                                }
                                PrimaryButton {
                                    text: stageCard.stageSession && stageCard.stageSession.active ? qsTr("Replace") : qsTr("Connect")
                                    iconName: "link"
                                    implicitWidth: 104
                                    enabled: workerUrlField.text.trim() !== "" && tokenField.text !== "" && !stageCard.setupDialog.dubbing.colabSetupChecking
                                    onClicked: {
                                        if (stageCard.setupDialog.dubbing.connectWorkflowColabStage(stageCard.stageId,
                                                                                   stageCard.modelData.modelId,
                                                                                   workerUrlField.text.trim(), tokenField.text)) {
                                            stageCard.setupDialog.setDraftToken(stageCard.stageId, "")
                                        }
                                    }
                                }
                            }

                            ColabSessionStatus {
                                Layout.fillWidth: true
                                session: stageCard.stageSession
                                showDisconnected: true
                                useExternalActions: true
                                onCheckRequested: {
                                    stageCard.setupDialog.dubbing.checkWorkflowColabStage(stageCard.stageId)
                                }
                                onDisconnectRequested: {
                                    stageCard.setupDialog.dubbing.disconnectWorkflowColabStage(stageCard.stageId)
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: stageCard.modelData.diagnostic || ""
                                color: stageCard.modelData.verified ? Theme.success
                                       : (stageCard.modelData.selectedForDirectColab ? Theme.warning : Theme.textSecondary)
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                text: stageCard.modelData.snapshotValid ? qsTr("Session snapshot valid") : qsTr("No valid session snapshot")
                                color: stageCard.modelData.snapshotValid ? Theme.success : Theme.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surfaceAlt }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.paddingLarge
            spacing: Theme.paddingMedium
            Text {
                Layout.fillWidth: true
                text: root.dubbing.colabSetupSummary === "" ? qsTr("Select model and connect.") : root.dubbing.colabSetupSummary
                color: root.dubbing.colabSetupChecking ? Theme.warning : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
            PrimaryButton {
                text: root.dubbing.colabSetupChecking
                      ? qsTr("Checking…")
                      : (root.isScopedToStages() ? qsTr("Check current stage") : qsTr("Check all selected"))
                iconName: "activity"
                enabled: !root.dubbing.colabSetupChecking
                onClicked: root.checkVisibleStages()
            }
            PrimaryButton { text: qsTr("Close"); quiet: true; onClicked: root.close() }
        }
    }
}
