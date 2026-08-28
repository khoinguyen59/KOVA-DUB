import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import "../shared"
import "../shared/settings"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property string nodeId
    required property var node
    required property string nodeTitle

    property bool advancedOpen: false
    readonly property var parameterSchema: node && node.parameterSchema ? node.parameterSchema : []
    readonly property var dynamicSettings: node && node.parameters ? node.parameters : ({})
    readonly property var studioConfig: node && node.studioConfig ? node.studioConfig : ({})
    readonly property var basicSchema: splitSchema(parameterSchema, false)
    readonly property var advancedSchema: splitSchema(parameterSchema, true)
    readonly property bool hasLanguageInput: studioConfig && studioConfig.inputs
                                                     ? studioConfig.inputs.indexOf("language") !== -1
                                                     : nodeId === "transcribe"
    readonly property bool isOmniVoice: nodeId === "synthesize"
                                             && node
                                             && String(node.selectedFamilyId || "").toLowerCase().indexOf("omnivoice") !== -1
    // Keep the project TTS voice selector visible before a model/runtime is
    // loaded as well; model setup must not hide a required run configuration.
    readonly property bool ttsVoiceAvailable: nodeId === "synthesize"
    readonly property var cloneTargetModels: [
        { id: "vieneu-tts-v3-turbo", name: "VieNeu-TTS v3 Turbo" },
        { id: "omnivoice", name: "OmniVoice" }
    ]
    readonly property bool isRemoteTranscription: nodeId === "transcribe"
                                                  && String(dynamicSettings.executionProvider || "local-dev").toLowerCase() !== "local-dev"
    readonly property string alignmentModelId:
        String(dynamicSettings.alignmentModelId
               || dubbing.defaultColabModelForNode("alignment"))

    signal closeRequested()
    signal rewriteSetupRequested()
    signal voiceModelRequested(string nodeId)

    Layout.preferredWidth: 332
    Layout.minimumWidth: 290
    Layout.fillHeight: true
    visible: node && node.configurable === true
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    clip: true

    function splitSchema(schema, advanced) {
        var result = []
        for (var i = 0; i < schema.length; ++i) {
            var item = schema[i] || {}
            if (!!item.advanced === advanced) result.push(item)
        }
        return result
    }

    function updateParameter(parameterId, value) {
        var patch = ({})
        patch[parameterId] = value
        root.dubbing.setWorkflowNodeParameters(root.nodeId, patch)
    }

    function isSavedCloneVoice(item) {
        return item && (item.kind === "saved-clone"
                        || (item.id && String(item.id).indexOf("builtin:") !== 0
                            && (item.audioPath || item.referenceAudio)))
    }

    function cloneTargetIndex() {
        var selected = root.node && root.node.parameters
                ? String(root.node.parameters.voiceCloneModelId || "omnivoice").toLowerCase()
                : "omnivoice"
        for (var i = 0; i < root.cloneTargetModels.length; ++i) {
            if (root.cloneTargetModels[i].id === selected
                    || (selected === "vieneu" && root.cloneTargetModels[i].id.indexOf("vieneu-") === 0))
                return i
        }
        return 1
    }

    function selectedTtsVoiceObject() {
        var options = root.dubbing.ttsVoiceOptions || []
        for (var i = 0; i < options.length; ++i) {
            if (options[i].id === root.dubbing.selectedTtsVoiceId) return options[i]
        }
        return null
    }

    function selectCloneTarget(index) {
        var selected = root.selectedTtsVoiceObject()
        if (!root.isSavedCloneVoice(selected) || !root.cloneTargetModels[index]) return
        if (root.dubbing.selectCloneVoicePresetForTarget(selected.id,
                                                         root.cloneTargetModels[index].id))
            root.voiceModelRequested(root.nodeId)
    }

    function updateDurationControl(parameterId, value) {
        var next = Object.assign({}, root.dubbing.durationControl)
        next[parameterId] = value
        root.dubbing.durationControl = next
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            LineIcon {
                name: "sliders"
                color: Theme.accent
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 Settings").arg(root.nodeTitle)
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: root.node && root.node.providerName
                          ? root.node.providerName : qsTr("No model configured")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: root.node && root.node.roleDescription
                    text: root.node ? root.node.roleDescription : ""
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: root.node && root.node.showColabRecommendation === true
                    text: qsTr("Nên dùng Colab")
                    color: Theme.accentLight
                    font.pixelSize: 10
                    font.bold: true
                    ToolTip.visible: colabRecommendationHover.hovered
                    ToolTip.text: root.node ? root.node.resourceReason : ""
                    HoverHandler { id: colabRecommendationHover }
                }
            }

            Button {
                id: closeButton
                implicitWidth: 30
                implicitHeight: 30
                flat: true
                onClicked: root.closeRequested()
                contentItem: LineIcon {
                    name: "chevron-right"
                    color: closeButton.hovered ? Theme.accent : Theme.textSecondary
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                }
                background: Rectangle {
                    radius: 7
                    color: closeButton.hovered ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.025)
                    border.color: closeButton.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                                                      : Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.surfaceAlt }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: Math.max(0, parent.width - Theme.paddingSmall)
                spacing: Theme.paddingMedium

                SettingsSection {
                    title: qsTr("Core")
                    iconName: "file"
                    visible: root.nodeId === "translate" || root.hasLanguageInput

                    LanguageSelector {
                        Layout.fillWidth: true
                        visible: root.nodeId === "translate"
                        family: null
                        labelText: qsTr("Source language")
                        language: root.dubbing.sourceLanguage
                        onLanguageSelected: function(language) { root.dubbing.sourceLanguage = language }
                    }

                    LanguageSelector {
                        Layout.fillWidth: true
                        family: null
                        labelText: root.nodeId === "translate" ? qsTr("Target language") : qsTr("Language")
                        language: root.nodeId === "translate"
                                  ? root.dubbing.targetLanguage
                                  : String(root.dynamicSettings["lang"] !== undefined
                                           ? root.dynamicSettings["lang"]
                                           : (root.nodeId === "synthesize"
                                              ? root.dubbing.targetLanguage : "auto"))
                        onLanguageSelected: function(language) {
                            if (root.nodeId === "translate")
                                root.dubbing.targetLanguage = language
                            else
                                root.updateParameter("lang", language)
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("Model Parameters")
                    iconName: "sliders"
                    visible: root.basicSchema.length > 0

                    ModelParameterControls {
                        enabled: !root.dubbing.processing
                        schema: root.basicSchema
                        dynamicSettings: root.dynamicSettings
                        onParameterChanged: function(parameterId, value) {
                            root.updateParameter(parameterId, value)
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("TTS / Text to Speech")
                    iconName: "volume"
                    visible: root.ttsVoiceAvailable

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Giọng đọc lồng tiếng (Voice)")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ComboBox {
                            id: ttsVoiceSelector
                            Layout.fillWidth: true
                            textRole: "name"
                            model: root.dubbing.ttsVoiceOptions
                            currentIndex: {
                                for (var i = 0; i < model.length; ++i)
                                    if (model[i].id === root.dubbing.selectedTtsVoiceId) return i
                                return -1
                            }
                            enabled: !root.dubbing.processing && model.length > 0
                            onActivated: function(index) {
                                var selected = model[index]
                                if (!selected || !root.dubbing.selectTtsVoice(selected.id))
                                    return
                                if (root.isSavedCloneVoice(selected))
                                    root.voiceModelRequested(root.nodeId)
                            }
                        }

                        PrimaryButton {
                            text: qsTr("Bảng Giọng Nói")
                            iconName: "users"
                            buttonColor: Theme.accent
                            implicitHeight: 38
                            implicitWidth: 130
                            onClicked: dubbingVoiceGalleryDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        visible: root.isSavedCloneVoice(root.selectedTtsVoiceObject())

                        Text {
                            text: qsTr("Clone model")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: root.cloneTargetModels
                            textRole: "name"
                            currentIndex: root.cloneTargetIndex()
                            enabled: !root.dubbing.processing
                            onActivated: function(index) { root.selectCloneTarget(index) }
                        }

                        PrimaryButton {
                            text: qsTr("Open model")
                            iconName: "settings"
                            quiet: true
                            implicitWidth: 104
                            onClicked: root.voiceModelRequested(root.nodeId)
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Choose a voice from the catalog or a saved reference. The selected voice is applied consistently to the dubbing run.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        PrimaryButton {
                            text: qsTr("Làm mới danh sách giọng")
                            iconName: "refresh"
                            quiet: true
                            enabled: !root.dubbing.processing
                            onClicked: root.dubbing.refreshCloneVoicePresets()
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: root.dubbing.ttsVoiceSelectionError !== ""
                        text: root.dubbing.ttsVoiceSelectionError
                        color: Theme.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsSection {
                    title: qsTr("Voice design")
                    iconName: "spark"
                    visible: root.node && root.node.id === "synthesize"

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Saved voice-design presets are not available for Dubbing yet. They contain a design description for a dedicated Voice Design worker, while this node accepts only an exact TTS voice or a saved reference voice. LA Studio will not silently substitute a designed voice or treat an exported WAV as a reusable design preset.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsSection {
                    title: qsTr("Forced alignment")
                    iconName: "clock"
                    visible: root.isRemoteTranscription

                    ToggleRow {
                        text: qsTr("Refine STT timestamps on Colab")
                        checked: root.dynamicSettings.refineAlignmentWithColab === true
                        enabled: !root.dubbing.processing
                        onToggled: {
                            root.updateParameter("refineAlignmentWithColab", checked)
                            if (checked)
                                root.dubbing.selectWorkflowColabModel(
                                    "alignment", root.alignmentModelId)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        visible: root.dynamicSettings.refineAlignmentWithColab === true

                        ComboBox {
                            Layout.fillWidth: true
                            textRole: "displayName"
                            model: root.dubbing.colabModelOptionsForNode("alignment")
                            currentIndex: {
                                for (var i = 0; i < model.length; ++i)
                                    if (model[i].modelId === root.alignmentModelId) return i
                                return -1
                            }
                            enabled: !root.dubbing.processing
                            onActivated: function(index) {
                                root.dubbing.selectWorkflowColabModel(
                                    "alignment", model[index].modelId)
                            }
                        }
                        ColabNotebookLink {
                            notebookFile: root.dubbing.colabNotebookForNode(
                                              "alignment",
                                              root.alignmentModelId)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                text: AppController.colabAlignmentSession.active
                                      ? qsTr("Alignment worker connected")
                                      : qsTr("Alignment worker not connected")
                                color: AppController.colabAlignmentSession.active
                                       ? Theme.success : Theme.warning
                                font.pixelSize: 10
                            }
                            PrimaryButton {
                                text: AppController.colabAlignmentSession.active
                                      ? qsTr("Reconnect") : qsTr("Connect")
                                iconName: "link"
                                quiet: true
                                enabled: !root.dubbing.processing
                                onClicked: alignmentColabDialog.open()
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("This is a separate temporary Colab worker. If disabled, Dubbing keeps the timestamps returned by STT and does not load an alignment model locally.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("Segment duration")
                    iconName: "clock"
                    visible: root.isOmniVoice

                    ToggleRow {
                        text: qsTr("Force exact SRT segment duration")
                        checked: root.dynamicSettings.forceSegmentDuration !== undefined
                                 ? root.dynamicSettings.forceSegmentDuration === true
                                 : root.isOmniVoice
                        enabled: !root.dubbing.processing
                        onToggled: root.updateParameter("forceSegmentDuration", checked)
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Sets OmniVoice output to each segment's exact SRT time slot.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsSection {
                    title: qsTr("Duration-aware dubbing")
                    iconName: "clock"
                    visible: root.nodeId === "translate"

                    ToggleRow {
                        text: qsTr("Measure translated phoneme count")
                        checked: root.dubbing.durationControl.enabled !== false
                        enabled: !root.dubbing.processing
                        onToggled: root.updateDurationControl("enabled", checked)
                    }
                    PrimaryButton {
                        Layout.fillWidth: true
                        visible: root.dubbing.dubbingQuality === "custom"
                                 && root.dubbing.durationControl.autoRewrite !== false
                        text: root.dubbing.adaptiveReady
                              ? qsTr("Change rewrite model") : qsTr("Choose rewrite model")
                        iconName: "spark"
                        quiet: true
                        enabled: !root.dubbing.processing
                        onClicked: root.rewriteSetupRequested()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: qsTr("Lower tolerance"); color: Theme.textSecondary; font.pixelSize: 10 }
                            AppSpinBox {
                                Layout.fillWidth: true
                                from: 0; to: 90; stepSize: 1
                                value: Math.round(Number(root.dubbing.durationControl.lowerToleranceRatio !== undefined ? root.dubbing.durationControl.lowerToleranceRatio : 0.20) * 100)
                                textFromValue: function(value) { return value + "%" }
                                valueFromText: function(text) { return parseInt(text) }
                                editable: true
                                enabled: !root.dubbing.processing && root.dubbing.durationControl.enabled !== false
                                onValueModified: root.updateDurationControl("lowerToleranceRatio", value / 100.0)
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: qsTr("Upper tolerance"); color: Theme.textSecondary; font.pixelSize: 10 }
                            AppSpinBox {
                                Layout.fillWidth: true
                                from: 0; to: 200; stepSize: 1
                                value: Math.round(Number(root.dubbing.durationControl.upperToleranceRatio !== undefined ? root.dubbing.durationControl.upperToleranceRatio : 0.20) * 100)
                                textFromValue: function(value) { return value + "%" }
                                valueFromText: function(text) { return parseInt(text) }
                                editable: true
                                enabled: !root.dubbing.processing && root.dubbing.durationControl.enabled !== false
                                onValueModified: root.updateDurationControl("upperToleranceRatio", value / 100.0)
                            }
                        }
                    }
                    ToggleRow {
                        text: qsTr("Automatically shorten overlong translations with LLM")
                        checked: root.dubbing.durationControl.autoRewrite !== false
                        enabled: !root.dubbing.processing && root.dubbing.durationControl.enabled !== false
                        onToggled: root.updateDurationControl("autoRewrite", checked)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.dubbing.durationControl.autoRewrite !== false
                        Text { Layout.fillWidth: true; text: qsTr("Maximum attempts per segment"); color: Theme.textSecondary; font.pixelSize: 10; wrapMode: Text.WordWrap }
                        AppSpinBox {
                            Layout.preferredWidth: 140
                            from: 1; to: 8
                            value: Number(root.dubbing.durationControl.maxPreTtsIterations !== undefined ? root.dubbing.durationControl.maxPreTtsIterations : 4)
                            enabled: !root.dubbing.processing
                            onValueModified: root.updateDurationControl("maxPreTtsIterations", value)
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The translation model is not given a length constraint. Only translations above the upper phoneme limit are sent to the rewrite LLM.")
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.dubbing.durationControl.autoRewrite !== false
                              ? qsTr("Target language: %1 - shorten overlong segments with LLM").arg(root.dubbing.targetLanguage)
                              : qsTr("Target language: %1 - manual review").arg(root.dubbing.targetLanguage)
                        color: Theme.textSecondary
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                CollapsibleSettingsSection {
                    title: qsTr("Advanced")
                    iconName: "sliders"
                    visible: root.advancedSchema.length > 0
                    expanded: root.advancedOpen
                    onToggled: root.advancedOpen = !root.advancedOpen

                    ModelParameterControls {
                        enabled: !root.dubbing.processing
                        schema: root.advancedSchema
                        dynamicSettings: root.dynamicSettings
                        onParameterChanged: function(parameterId, value) {
                            root.updateParameter(parameterId, value)
                        }
                    }
                }

                SettingsSection {
                    title: qsTr("Model & Runtime")
                    iconName: "cpu"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: root.node && root.node.providerName
                                  ? root.node.providerName : qsTr("Not configured")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.node && root.node.selectedRuntimeId
                                  ? root.node.selectedRuntimeId : qsTr("Runtime not selected")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Model and runtime files are selected from Open model.")
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Dialog {
        id: alignmentColabDialog
        parent: Overlay.overlay
        modal: true
        property bool awaitingVerification: false
        anchors.centerIn: parent
        width: Math.min(520, Math.max(320, (Overlay.overlay ? Overlay.overlay.width : (parent ? parent.width : 520)) - Theme.paddingXL * 2))
        title: qsTr("Forced-alignment Colab worker")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            alignmentWorkerUrl.text = AppController.colabAlignmentSession.workerUrl
            if (!awaitingVerification) {
                alignmentWorkerToken.text = ""
                alignmentWorkerError.message = ""
            }
        }
        onAccepted: {
            if (!root.dubbing.selectWorkflowColabModel(
                    "alignment", root.alignmentModelId)) {
                alignmentWorkerError.message = qsTr("Select an exact alignment model.")
                alignmentColabDialog.open()
                return
            }
            if (!AppController.colabAlignmentSession.connectTemporaryWorker(
                    alignmentWorkerUrl.text.trim(),
                    alignmentWorkerToken.text,
                    "forced-alignment",
                    root.alignmentModelId)) {
                alignmentWorkerError.message =
                    AppController.colabAlignmentSession.lastError
                alignmentColabDialog.open()
                return
            }
            awaitingVerification = true
            alignmentWorkerToken.text = ""
        }
        onRejected: {
            if (!awaitingVerification) return
            awaitingVerification = false
            if (AppController.colabAlignmentSession.checking)
                AppController.colabAlignmentSession.disconnectTemporaryWorker()
        }
        onClosed: {
            if (awaitingVerification && AppController.colabAlignmentSession.checking) {
                Qt.callLater(function() {
                    if (alignmentColabDialog.awaitingVerification
                            && AppController.colabAlignmentSession.checking)
                        alignmentColabDialog.open()
                })
            }
        }
        contentItem: ColumnLayout {
            spacing: Theme.paddingSmall
            ColabNotebookLink {
                notebookFile: root.dubbing.colabNotebookForNode(
                                  "alignment", root.alignmentModelId)
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Exact model: %1").arg(root.alignmentModelId)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WrapAnywhere
            }
            TextField {
                id: alignmentWorkerUrl
                Layout.fillWidth: true
                placeholderText: qsTr("https://…trycloudflare.com")
                selectByMouse: true
            }
            TextField {
                id: alignmentWorkerToken
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Temporary token from Colab")
                selectByMouse: true
            }
            ColabSessionStatus {
                session: AppController.colabAlignmentSession
            }
            ErrorGuidanceInline {
                id: alignmentWorkerError
                Layout.fillWidth: true
                message: ""
                source: "Dubbing alignment worker"
            }
        }
    }

    Connections {
        target: AppController.colabAlignmentSession
        function onVerificationFinished(success, message) {
            if (!alignmentColabDialog.awaitingVerification) return
            alignmentColabDialog.awaitingVerification = false
            if (success) {
                alignmentColabDialog.close()
                return
            }
            alignmentWorkerError.message = message
            if (!alignmentColabDialog.visible) alignmentColabDialog.open()
        }
    }

    VoiceGalleryDialog {
        id: dubbingVoiceGalleryDialog
        parent: Overlay.overlay
        onVoiceSelected: function(audioPath, referenceText, name, familyId, voiceId) {
            root.dubbing.refreshCloneVoicePresets()
            if (voiceId && voiceId.length > 0) {
                if (root.dubbing.selectTtsVoice(voiceId))
                    root.voiceModelRequested(root.nodeId)
                return
            }
            var cleanName = String(name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
            var opts = root.dubbing.ttsVoiceOptions || []
            for (var i = 0; i < opts.length; ++i) {
                var optClean = String(opts[i].name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
                if (opts[i].id === voiceId || opts[i].name === name || optClean === cleanName || opts[i].audioPath === audioPath || opts[i].id === ("builtin:" + name)) {
                    if (root.dubbing.selectTtsVoice(opts[i].id))
                        root.voiceModelRequested(root.nodeId)
                    return
                }
            }
            for (var j = 0; j < opts.length; ++j) {
                if (opts[j].audioPath && audioPath && opts[j].audioPath.indexOf(audioPath) !== -1) {
                    if (root.dubbing.selectTtsVoice(opts[j].id))
                        root.voiceModelRequested(root.nodeId)
                    return
                }
            }
        }
    }
}
