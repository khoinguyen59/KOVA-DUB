pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import "../../shared"
import ".."
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property var sourceMediaPanel
    property string playingVoiceClipPath: ""
    property int generatedClipCount: 0
    property bool synthesisComplete: false
    property bool stepComplete: false

    signal voiceClipPlaybackRequested(string path)
    signal separationPlaybackStopped()
    signal runRequested(string nodeId)
    signal voiceModelRequested(string nodeId)
    signal nextStepRequested()
    signal previousStepRequested()

    // 3 Mode Switcher: "gallery" (Presets/Clones), "colab" (Colab GPU), "gateway" (API Gateway)
    property string activeTtsMode: "gallery"
    property string currentSamplePlayingPath: ""
    readonly property var cloneTargetModels: [
        { id: "vieneu-tts-v3-turbo", name: "VieNeu-TTS v3 Turbo" },
        { id: "omnivoice", name: "OmniVoice" }
    ]

    Layout.fillWidth: true
    Layout.fillHeight: true
    implicitHeight: mainLayout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    function getSelectedVoiceObject() {
        var opts = root.dubbing.ttsVoiceOptions || []
        var curId = root.dubbing.selectedTtsVoiceId || ""
        for (var i = 0; i < opts.length; ++i) {
            if (opts[i].id === curId) return opts[i]
        }
        if (opts.length > 0) return opts[0]
        return null
    }

    function resolveVoiceAudioPath(item) {
        if (!item) return ""
        var path = item.audioPath || item.referenceAudio || item.refAudio || ""
        if (AppController.files && AppController.files.urlToLocalPath) {
            path = AppController.files.urlToLocalPath(path)
        }
        return path
    }

    function togglePlaySample(item) {
        if (!item) return
        var path = resolveVoiceAudioPath(item)
        if (!path || path.length === 0) return

        if (root.currentSamplePlayingPath === path && AppController.player && AppController.player.playing) {
            AppController.player.stop()
            root.currentSamplePlayingPath = ""
        } else {
            if (AppController.player && AppController.player.playing) {
                AppController.player.stop()
            }
            root.currentSamplePlayingPath = path
            if (AppController.player) {
                AppController.player.playFile(path)
            }
        }
    }

    function selectVoiceAndMaybeOpenModel(itemOrId, sourceFamily) {
        var voiceId = typeof itemOrId === "string"
                ? itemOrId : (itemOrId ? itemOrId.id : "")
        if (!voiceId || !root.dubbing.selectTtsVoice(voiceId))
            return false

        var item = typeof itemOrId === "string" ? root.getSelectedVoiceObject() : itemOrId
        var isSavedClone = item && (item.kind === "saved-clone"
                || (item.id && String(item.id).indexOf("builtin:") !== 0
                    && (item.audioPath || item.referenceAudio)))
        // Every saved reference is target-independent. Open the exact model
        // picker for it instead of routing from the source family that created
        // the preset.
        if (isSavedClone)
            root.voiceModelRequested("synthesize")
        return true
    }

    function selectedCloneTargetIndex() {
        var item = root.getSelectedVoiceObject()
        var selected = item && item.voiceCloneModelId ? String(item.voiceCloneModelId).toLowerCase() : "omnivoice"
        for (var i = 0; i < root.cloneTargetModels.length; ++i) {
            if (root.cloneTargetModels[i].id === selected
                    || (selected === "vieneu" && root.cloneTargetModels[i].id.indexOf("vieneu-") === 0))
                return i
        }
        return 1
    }

    function selectCloneTarget(index) {
        var item = root.getSelectedVoiceObject()
        if (!item || item.kind !== "saved-clone" || !root.cloneTargetModels[index]) return
        if (root.dubbing.selectCloneVoicePresetForTarget(item.id, root.cloneTargetModels[index].id))
            root.voiceModelRequested("synthesize")
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (AppController.player && !AppController.player.playing) {
                root.currentSamplePlayingPath = ""
            }
        }
        function onPlaybackFinished() {
            root.currentSamplePlayingPath = ""
        }
    }

    ScrollView {
        id: synthesisScrollView
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: contentHeight > height
                                   ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

    ColumnLayout {
        id: mainLayout
        x: Theme.paddingMedium
        width: Math.max(0, synthesisScrollView.availableWidth - Theme.paddingMedium * 2)
        spacing: Theme.paddingSmall

        // ==========================================
        // 1. STEP HEADER
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon { name: "volume-2"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("6. LỒNG TIẾNG AI (TTS & VOICE CLONE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ==========================================
        // 2. TTS ENGINE & SOURCE MODE SWITCHER TABS
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.04)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 4

                // Tab 1: Thư Viện Giọng
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 2
                    color: root.activeTtsMode === "gallery" ? Theme.accent : "transparent"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        LineIcon {
                            name: "users"
                            color: root.activeTtsMode === "gallery" ? "#ffffff" : Theme.textSecondary
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                        }
                        Text {
                            text: qsTr("Voice library")
                            color: root.activeTtsMode === "gallery" ? "#ffffff" : Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.bold: root.activeTtsMode === "gallery"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTtsMode = "gallery"
                    }
                }

                // Tab 2: Colab GPU
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 2
                    color: root.activeTtsMode === "colab" ? Theme.accent : "transparent"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        LineIcon {
                            name: "cloud"
                            color: root.activeTtsMode === "colab" ? "#ffffff" : Theme.textSecondary
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                        }
                        Text {
                            text: qsTr("Colab GPU")
                            color: root.activeTtsMode === "colab" ? "#ffffff" : Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.bold: root.activeTtsMode === "colab"
                        }
                        Rectangle {
                            visible: AppController.colabTts && AppController.colabTts.colabActive
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: Theme.success
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTtsMode = "colab"
                    }
                }

                // Tab 3: API Gateway
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 2
                    color: root.activeTtsMode === "gateway" ? Theme.accent : "transparent"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        LineIcon {
                            name: "globe"
                            color: root.activeTtsMode === "gateway" ? "#ffffff" : Theme.textSecondary
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                        }
                        Text {
                            text: qsTr("API Gateway")
                            color: root.activeTtsMode === "gateway" ? "#ffffff" : Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.bold: root.activeTtsMode === "gateway"
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTtsMode = "gateway"
                    }
                }
            }
        }

        // ==========================================
        // 3. TAB CONTENT: VOICE GALLERY / PRESETS
        // ==========================================
        Rectangle {
            visible: root.activeTtsMode === "gallery"
            Layout.fillWidth: true
            implicitHeight: galleryContentLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
            border.width: 1

            ColumnLayout {
                id: galleryContentLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                // Row: Selector + "Bảng Giọng Nói" Button
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    ComboBox {
                        id: ttsVoiceSelector
                        objectName: "dubbingTtsVoiceSelector"
                        Layout.fillWidth: true
                        textRole: "name"
                        model: root.dubbing.ttsVoiceOptions
                        currentIndex: {
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === root.dubbing.selectedTtsVoiceId) return i
                            return 0
                        }
                        enabled: !root.dubbing.processing && model.length > 0
                        onActivated: function(index) {
                            var opts = root.dubbing.ttsVoiceOptions || []
                            if (opts[index] && opts[index].id) {
                                root.selectVoiceAndMaybeOpenModel(opts[index])
                            }
                        }
                    }

                    PrimaryButton {
                        text: qsTr("Bảng Giọng Nói")
                        iconName: "users"
                        buttonColor: Theme.accent
                        Layout.preferredHeight: 38
                        Layout.preferredWidth: 140
                        onClicked: dubbingVoiceGalleryDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    visible: root.getSelectedVoiceObject()
                             && root.getSelectedVoiceObject().kind === "saved-clone"

                    Text {
                        text: qsTr("Clone model")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }

                    ComboBox {
                        id: dubbingCloneTargetSelector
                        Layout.fillWidth: true
                        model: root.cloneTargetModels
                        textRole: "name"
                        currentIndex: root.selectedCloneTargetIndex()
                        enabled: !root.dubbing.processing
                        onActivated: function(index) { root.selectCloneTarget(index) }
                    }

                    PrimaryButton {
                        text: qsTr("Open model")
                        iconName: "settings"
                        quiet: true
                        implicitWidth: 112
                        onClicked: root.voiceModelRequested("synthesize")
                    }
                }

                // Selected Voice Showcase Card
                Rectangle {
                    id: voiceShowcaseCard
                    Layout.fillWidth: true
                    implicitHeight: voiceShowcaseLayout.implicitHeight + 16
                    radius: Theme.radiusSmall
                    color: Qt.rgba(0, 0, 0, 0.25)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                    border.width: 1

                    property var voiceData: root.getSelectedVoiceObject()
                    property bool isCustom: voiceData ? (voiceData.isCustomVoice === true || voiceData.category === "custom" || String(voiceData.id || "").indexOf("custom") !== -1) : false
                    property bool isCapcut: voiceData ? (String(voiceData.name || "").indexOf("CapCut") !== -1 || voiceData.category === "capcut") : false
                    property bool isVieNeu: voiceData ? (String(voiceData.name || "").indexOf("VieNeu") !== -1 || voiceData.category === "vieneu") : false
                    property string audioSamplePath: root.resolveVoiceAudioPath(voiceData)
                    property bool isPlayingThisSample: root.currentSamplePlayingPath !== "" && root.currentSamplePlayingPath === audioSamplePath

                    RowLayout {
                        id: voiceShowcaseLayout
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        // Avatar Circle
                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            radius: 22
                            gradient: Gradient {
                                GradientStop {
                                    position: 0.0
                                    color: voiceShowcaseCard.isCustom ? "#f59e0b" : (voiceShowcaseCard.isCapcut ? "#ff0055" : (voiceShowcaseCard.isVieNeu ? "#059669" : "#7c4dff"))
                                }
                                GradientStop {
                                    position: 1.0
                                    color: voiceShowcaseCard.isCustom ? "#b45309" : (voiceShowcaseCard.isCapcut ? "#be123c" : (voiceShowcaseCard.isVieNeu ? "#065f46" : "#4c1d95"))
                                }
                            }
                            border.color: Qt.rgba(1, 1, 1, 0.3)
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var vd = voiceShowcaseCard.voiceData
                                    if (!vd) return "🎙️"
                                    var n = String(vd.name || "").toLowerCase()
                                    if (voiceShowcaseCard.isCustom) return "⭐"
                                    if (n.indexOf("bé") !== -1 || n.indexOf("mới lớn") !== -1) return "👶"
                                    if (n.indexOf("robot") !== -1) return "🤖"
                                    if (vd.gender === "female") return "👩"
                                    if (vd.gender === "male") return "👨"
                                    return "🎙️"
                                }
                                font.pixelSize: 20
                            }
                        }

                        // Voice Info Column
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text {
                                    text: {
                                        var vd = voiceShowcaseCard.voiceData
                                        if (!vd) return qsTr("Chưa chọn giọng")
                                        return String(vd.name || qsTr("Giọng đọc đã chọn")).replace("CapCut: ", "").replace("OmniVoice: ", "")
                                    }
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontMedium
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                // Category Pill
                                Rectangle {
                                    Layout.preferredHeight: 18
                                    implicitWidth: catLabel.implicitWidth + 10
                                    radius: 9
                                    color: voiceShowcaseCard.isCustom ? Qt.rgba(0.96, 0.62, 0.04, 0.25)
                                           : (voiceShowcaseCard.isCapcut ? Qt.rgba(1, 0, 0.33, 0.25)
                                           : (voiceShowcaseCard.isVieNeu ? Qt.rgba(0.02, 0.59, 0.41, 0.25) : Qt.rgba(0.49, 0.3, 1, 0.25)))
                                    border.color: voiceShowcaseCard.isCustom ? "#f59e0b" : (voiceShowcaseCard.isCapcut ? "#ff0055" : (voiceShowcaseCard.isVieNeu ? "#059669" : "#7c4dff"))
                                    border.width: 1

                                    Text {
                                        id: catLabel
                                        anchors.centerIn: parent
                                        text: voiceShowcaseCard.isCustom ? qsTr("Giọng Clone") : (voiceShowcaseCard.isCapcut ? "CapCut" : (voiceShowcaseCard.isVieNeu ? "VieNeu" : "OmniVoice"))
                                        color: "#ffffff"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    var vd = voiceShowcaseCard.voiceData
                                    if (!vd) return ""
                                    var t = vd.referenceTranscript || vd.referenceText || vd.description || ""
                                    return t !== "" ? ("\"" + t + "\"") : qsTr("Sẵn sàng tổng hợp giọng nói")
                                }
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                font.italic: true
                                elide: Text.ElideRight
                            }
                        }

                        // Play/Pause Sample Audio Button
                        PrimaryButton {
                            visible: voiceShowcaseCard.audioSamplePath !== ""
                            text: voiceShowcaseCard.isPlayingThisSample ? qsTr("Dừng") : qsTr("Nghe thử mẫu")
                            iconName: voiceShowcaseCard.isPlayingThisSample ? "pause" : "play"
                            buttonColor: voiceShowcaseCard.isPlayingThisSample ? Theme.warning : Theme.accent
                            quiet: !voiceShowcaseCard.isPlayingThisSample
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 120
                            onClicked: root.togglePlaySample(voiceShowcaseCard.voiceData)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    PrimaryButton {
                        text: qsTr("Làm mới danh sách")
                        iconName: "refresh"
                        quiet: true
                        enabled: !root.dubbing.processing
                        onClicked: root.dubbing.refreshCloneVoicePresets()
                    }
                }
            }
        }

        // ==========================================
        // 4. TAB CONTENT: COLAB GPU
        // ==========================================
        Rectangle {
            visible: root.activeTtsMode === "colab"
            Layout.fillWidth: true
            implicitHeight: colabContentLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
            border.width: 1

            ColumnLayout {
                id: colabContentLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Cấu hình Colab GPU cho bước Lồng Tiếng (TTS):")
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: Theme.fontSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    Text {
                        text: qsTr("Trạng thái worker Colab:")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: AppController.colabTts && AppController.colabTts.colabActive ? Theme.success : Theme.warning
                    }
                    Text {
                        text: AppController.colabTts && AppController.colabTts.colabActive
                              ? qsTr("Đang kết nối (Sẵn sàng GPU)")
                              : qsTr("Chưa kết nối Colab worker")
                        color: AppController.colabTts && AppController.colabTts.colabActive ? Theme.success : Theme.warning
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Mở cấu hình Colab")
                        iconName: "cloud"
                        quiet: true
                        // Keep model selection and Colab setup on the same path
                        // as the task shelf and the workflow-node inspector.
                        onClicked: root.voiceModelRequested("synthesize")
                    }
                }
            }
        }

        // ==========================================
        // 5. TAB CONTENT: API GATEWAY
        // ==========================================
        Rectangle {
            visible: root.activeTtsMode === "gateway"
            Layout.fillWidth: true
            implicitHeight: gatewayContentLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.06)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
            border.width: 1

            ColumnLayout {
                id: gatewayContentLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("API Gateway TTS:")
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        // ==========================================
        // 6. PRIMARY ACTION / RUN SYNTHESIS BUTTON
        // ==========================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (Full width)
            PrimaryButton {
                id: runSynthesizeBtn
                text: root.generatedClipCount > 0
                      ? qsTr("⚡ Lồng Tiếng Lại Toàn Bộ (%1 phân đoạn)").arg((root.dubbing.segments || []).length)
                      : qsTr("⚡ Bắt Đầu Lồng Tiếng TTS (Run Synthesize)")
                iconName: root.dubbing.processing ? "activity" : "volume-2"
                loading: root.dubbing.processing
                enabled: !root.dubbing.processing
                Layout.preferredHeight: 44
                Layout.fillWidth: true
                buttonColor: Theme.accent
                onClicked: root.runRequested("synthesize")
            }

            // Row 2: Navigation Buttons (Quay lại & Tiếp tục)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                PrimaryButton {
                    text: qsTr("⬅ Quay lại")
                    iconName: "chevron-left"
                    quiet: true
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 100
                    onClicked: root.previousStepRequested()
                }

                PrimaryButton {
                    text: qsTr("Tiếp tục: Trộn Âm Thanh ➔")
                    iconName: "chevron-right"
                    buttonColor: Theme.accent
                    enabled: !root.dubbing.processing && (root.generatedClipCount > 0 || root.synthesisComplete)
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }
            }
        }

        // ==========================================
        // 7. GENERATED CLIPS & TIMING REVIEW
        // ==========================================
        DubbingVoiceClipReview {
            id: voiceClipReview
            dubbing: root.dubbing
            sourceMediaPanel: root.sourceMediaPanel
            playingVoiceClipPath: root.playingVoiceClipPath
            generatedClipCount: root.generatedClipCount
            synthesisComplete: root.synthesisComplete
            onVoiceClipPlaybackRequested: function(path) { root.voiceClipPlaybackRequested(path) }
            onSeparationPlaybackStopped: root.separationPlaybackStopped()
        }
    }
    }

    // Modal Voice Gallery Dialog with the current preset list + custom cloned voices
    VoiceGalleryDialog {
        id: dubbingVoiceGalleryDialog
        parent: Overlay.overlay
        onVoiceSelected: function(audioPath, referenceText, name, familyId, voiceId) {
            dubbingVoiceGalleryDialog.close()
            root.dubbing.refreshCloneVoicePresets()
            if (voiceId && voiceId.length > 0) {
                root.selectVoiceAndMaybeOpenModel(voiceId, familyId)
                return
            }
            var cleanName = String(name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
            var opts = root.dubbing.ttsVoiceOptions || []
            for (var i = 0; i < opts.length; ++i) {
                var optClean = String(opts[i].name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
                if (opts[i].id === voiceId || opts[i].name === name || optClean === cleanName || opts[i].audioPath === audioPath || opts[i].id === ("builtin:" + name)) {
                    root.selectVoiceAndMaybeOpenModel(opts[i])
                    return
                }
            }
            for (var j = 0; j < opts.length; ++j) {
                if (opts[j].audioPath && audioPath && opts[j].audioPath.indexOf(audioPath) !== -1) {
                    root.selectVoiceAndMaybeOpenModel(opts[j])
                    return
                }
            }
        }
    }
}
