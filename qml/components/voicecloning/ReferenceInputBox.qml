import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LAStudio
import "../base"
import "../shared"
import "../shared/settings"

// Component for reference voice input with transcript
ColumnLayout {
    id: root

    property string audioPath: ""
    property string referenceText: ""
    property bool isPlaying: false
    property bool showTips: true
    property bool showHeader: true
    property bool locked: false
    property bool requiresExactTranscript: false
    property string transcriptHint: ""
    property string familyId: ""
    property var savedVoices: []
    property int selectedSavedVoiceIndex: -1
    // Keep the durable preset identity separate from its editable fields.
    // This lets the Colab controller invalidate a temporary profile whenever
    // the user switches a saved reference.
    property string selectedSavedVoiceId: ""
    // This name becomes the user-facing reusable voice name in TTS when a
    // Direct Colab clone is created from a new reference.  It is intentionally
    // placed next to the reference rather than hidden in the remote settings.
    property string reusableVoiceName: ""
    property bool loadingSavedVoice: false

    signal audioCleared()
    signal playClicked()
    signal stopClicked()

    onAudioPathChanged: {
        if (!root.loadingSavedVoice)
            root.selectedSavedVoiceId = ""
        AppController.preview.requestWavSamples(root.audioPath)
    }

    onReferenceTextChanged: {
        if (!root.loadingSavedVoice)
            root.selectedSavedVoiceId = ""
        if (refTextEdit.text !== root.referenceText)
            refTextEdit.text = root.referenceText
    }

    onFamilyIdChanged: {
        root.selectedSavedVoiceId = ""
        reloadSavedVoices(true)
    }
    Component.onCompleted: reloadSavedVoices(true)

    Connections {
        target: AppController.voiceClonePresets
        function onPresetsChanged(familyId) {
            if (String(familyId || "").trim().toLowerCase()
                    === root.familyId.trim().toLowerCase())
                root.reloadSavedVoices(false)
        }
    }

    function reloadSavedVoices(clearSelection) {
        var previousId = clearSelection === true ? "" : root.selectedSavedVoiceId
        var voices = []
        if (AppController.voiceClonePresets) {
            voices = root.familyId !== ""
                     ? AppController.voiceClonePresets.presetsForFamily(root.familyId)
                     : AppController.voiceClonePresets.allPresets()
            if ((!voices || voices.length === 0) && root.familyId !== "") {
                voices = AppController.voiceClonePresets.allPresets()
            }
        }
        root.savedVoices = voices || []
        root.selectedSavedVoiceIndex = -1
        root.selectedSavedVoiceId = ""
        for (var i = 0; i < (voices ? voices.length : 0); ++i) {
            if (voices[i].valid && voices[i].id === previousId) {
                root.selectedSavedVoiceIndex = i
                root.selectedSavedVoiceId = previousId
                return
            }
        }
    }

    function defaultVoiceName() {
        if (root.audioPath !== "")
            return VoiceCloningUtils.fileNameFromPath(root.audioPath)
        return qsTr("Reference voice")
    }

    function loadSavedVoice(index) {
        if (root.locked || index < 0 || index >= root.savedVoices.length) return
        var voice = root.savedVoices[index]
        if (!voice.valid) return
        root.loadingSavedVoice = true
        root.selectedSavedVoiceIndex = index
        root.selectedSavedVoiceId = voice.id || ""
        root.reusableVoiceName = voice.name || ""
        root.audioPath = voice.audioPath || ""
        root.referenceText = voice.referenceText || ""
        root.loadingSavedVoice = false
    }

    function saveCurrentVoice() {
        if (root.locked || root.audioPath === "") return
        libraryDialog.initialMode = "reference"
        libraryDialog.familyId = root.familyId
        libraryDialog.open()
        Qt.callLater(libraryDialog.applyCurrentReference)
    }

    function manageVoices() {
        libraryDialog.initialMode = "reference"
        libraryDialog.familyId = root.familyId
        libraryDialog.open()
    }
    spacing: Theme.paddingLarge
    Layout.fillHeight: false

    // Header
    property string referenceMode: "custom" // "custom", "saved"

    Text {
        visible: root.showHeader
        text: qsTr("Reference Voice")
        color: Theme.textPrimary
        font.pixelSize: Theme.fontMedium
        font.bold: true
    }

    StudioOptionSwitcher {
        Layout.fillWidth: true
        activeId: root.referenceMode
        options: [
            { id: "custom", label: qsTr("Upload / Record"), icon: "mic" },
            { id: "saved", label: qsTr("Saved Voices") + (root.savedVoices.length > 0 ? " (" + root.savedVoices.length + ")" : ""), icon: "users" }
        ]
        onOptionSelected: function(id) { root.referenceMode = id }
    }

    // Saved Voice Profile Section
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: savedCol.implicitHeight + Theme.paddingMedium * 2
        radius: 10
        color: "#1d1b2c"
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        visible: root.referenceMode === "saved"

        ColumnLayout {
            id: savedCol
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingSmall

            Text {
                text: qsTr("Select from previously saved clone voices:")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                AppComboBox {
                    id: savedVoiceCombo
                    Layout.fillWidth: true
                    model: root.savedVoices
                    textRole: "name"
                    secondaryTextRole: "originalAudioName"
                    currentIndex: root.selectedSavedVoiceIndex
                    enabled: !root.locked && root.savedVoices.length > 0
                    onActivated: function(index) { root.loadSavedVoice(index) }
                }

                PrimaryButton {
                    text: qsTr("Bảng Giọng Nói")
                    iconName: "users"
                    buttonColor: Theme.accent
                    implicitHeight: 38
                    implicitWidth: 140
                    onClicked: voiceGalleryDialog.open()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                visible: root.savedVoices.length > 0

                Text {
                    text: qsTr("Targets")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }

                Rectangle {
                    radius: 3
                    implicitWidth: 52
                    implicitHeight: 18
                    color: "#065f46"
                    border.color: "#34d399"
                    Text {
                        anchors.centerIn: parent
                        text: "VieNeu"
                        color: "#d1fae5"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Rectangle {
                    radius: 3
                    implicitWidth: 72
                    implicitHeight: 18
                    color: "#4c1d95"
                    border.color: "#a78bfa"
                    Text {
                        anchors.centerIn: parent
                        text: "OmniVoice"
                        color: "#ede9fe"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                PrimaryButton {
                    text: qsTr("Save Current")
                    quiet: true
                    implicitHeight: 34
                    implicitWidth: 110
                    enabled: !root.locked && root.audioPath !== ""
                    onClicked: root.saveCurrentVoice()
                }

                PrimaryButton {
                    text: qsTr("Manage Presets")
                    iconName: "settings"
                    quiet: true
                    implicitHeight: 34
                    implicitWidth: 125
                    onClicked: root.manageVoices()
                }
            }
        }
    }

    // Custom Upload / Record Box
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: root.audioPath === "" ? 170 : customCol.implicitHeight + Theme.paddingMedium * 2
        color: "#1d1b2c"
        radius: 10
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        visible: root.referenceMode === "custom" || root.audioPath !== ""

        ColumnLayout {
            id: customCol
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: Theme.paddingMedium
            visible: root.audioPath === ""

            InputSourceTabs {
                id: inputTabs
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: !root.locked
                recordingSampleRate: root.familyId === "vieneu-tts-v3-turbo" ? 48000 : 24000
                recommendedAudioHint: root.familyId === "vieneu-tts-v3-turbo"
                                      ? "48kHz mono WAV recommended for VieNeu v3 native cloning"
                                      : "24kHz mono WAV recommended"

                onAudioLoaded: (path) => {
                    if (root.locked) return
                    root.audioPath = path
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Voice name for TTS reuse")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }

                TextField {
                    id: reusableVoiceNameField
                    Layout.fillWidth: true
                    text: root.reusableVoiceName
                    placeholderText: qsTr("e.g. Hoài Vũ — Vietnamese")
                    enabled: !root.locked
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    selectByMouse: true
                    onTextChanged: {
                        if (root.reusableVoiceName !== text)
                            root.reusableVoiceName = text
                    }
                    background: Rectangle {
                        radius: 6
                        color: Qt.rgba(1, 1, 1, 0.04)
                        border.color: reusableVoiceNameField.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1
                    }
                }
            }
        }

        // Display loaded audio
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            visible: root.audioPath !== ""
            spacing: Theme.paddingMedium

            Text {
                text: "Loaded: " + VoiceCloningUtils.fileNameFromPath(root.audioPath)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            WaveformView {
                id: refWaveform
                Layout.fillWidth: true
                Layout.fillHeight: true
                samples: (root.audioPath !== "" && AppController.preview.wavSamplesSourcePath === root.audioPath) ? AppController.preview.wavSamples : []
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.paddingMedium

                PrimaryButton {
                    text: "Change / Record"
                    quiet: true
                    implicitHeight: 32
                    enabled: !root.locked
                    onClicked: {
                        if (root.isPlaying)
                            root.stopClicked()
                        root.audioPath = ""
                        root.audioCleared()
                    }
                }

                PrimaryButton {
                    text: root.isPlaying ? "Stop" : "Play"
                    buttonColor: root.isPlaying ? Theme.danger : Theme.success
                    implicitHeight: 32
                    onClicked: {
                        if (root.isPlaying) {
                            root.stopClicked()
                        } else {
                            root.playClicked()
                        }
                    }
                }
            }
        }
    }

    // Reference Transcript
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon {
                name: "file"
                color: Theme.textSecondary
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
            Text {
                text: root.requiresExactTranscript ? qsTr("Reference Transcript") : qsTr("Reference Transcript (optional)")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                visible: root.requiresExactTranscript
                text: "*"
                color: Theme.danger
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }
            PrimaryButton {
                text: qsTr("Import .txt")
                iconName: "folder"
                quiet: true
                implicitHeight: 28
                implicitWidth: 95
                enabled: !root.locked
                onClicked: txtFileDialogLoader.active = true
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.requiresExactTranscript
                  ? qsTr("Required: type the exact words spoken in the reference audio.")
                  : (root.transcriptHint !== "" ? root.transcriptHint
                                                : qsTr("Optional: improves voice similarity when provided."))
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: Theme.surface
            border.color: root.requiresExactTranscript && root.audioPath !== ""
                          && root.referenceText.trim() === "" ? Theme.danger : Theme.surfaceAlt
            border.width: 1
            radius: Theme.radiusSmall

            AppTextArea {
                id: refTextEdit
                anchors.fill: parent
                anchors.margins: 4
                placeholderText: "Type what is spoken in the reference audio here..."
                font.pixelSize: Theme.fontSmall
                background: Rectangle { color: "transparent" }
                readOnly: root.locked

                onTextChanged: root.referenceText = text
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.requiresExactTranscript && root.audioPath !== ""
                     && root.referenceText.trim() === ""
            text: "Reference Transcript is required for this route/model."
            color: Theme.danger
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }

    // Tips
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: tipsCol.implicitHeight + Theme.paddingMedium * 2
        color: Qt.rgba(0.49, 0.30, 1.0, 0.05)
        radius: Theme.radiusSmall
        border.color: Qt.rgba(0.49, 0.30, 1.0, 0.1)
        border.width: 1
        visible: root.showTips

        ColumnLayout {
            id: tipsCol
            anchors.fill: parent
            anchors.margins: Theme.paddingMedium
            spacing: 4

            RowLayout {
                LineIcon {
                    name: "info"
                    color: Theme.accent
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }
                Text {
                    text: "Tips for better cloning"
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }
            }

            Text {
                text: "• Clean audio without background noise\n• 5-15 seconds of clear speech\n• Natural tone and prosody"
                color: Theme.textSecondary
                font.pixelSize: 11
                lineHeight: 1.3
                Layout.fillWidth: true
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }

    VoiceLibraryDialog {
        id: libraryDialog
        parent: Overlay.overlay
        familyId: root.familyId
        currentReferenceAudioPath: root.audioPath
        currentReferenceText: root.referenceText
        onReferenceVoiceSelected: function(audioPath, referenceText, name) {
            if (root.locked) return
            root.audioPath = audioPath
            root.referenceText = referenceText
        }
    }

    VoiceGalleryDialog {
        id: voiceGalleryDialog
        parent: Overlay.overlay
        familyId: root.familyId
        onVoiceSelected: function(audioPath, referenceText, name, familyId) {
            if (root.locked) return
            root.loadingSavedVoice = true
            root.audioPath = audioPath
            root.referenceText = referenceText
            root.reusableVoiceName = name

            for (var i = 0; i < root.savedVoices.length; ++i) {
                if (root.savedVoices[i].name === name || root.savedVoices[i].audioPath === audioPath) {
                    root.selectedSavedVoiceIndex = i
                    root.selectedSavedVoiceId = root.savedVoices[i].id || ""
                    break
                }
            }
            root.loadingSavedVoice = false
        }
    }
    Loader {
        id: txtFileDialogLoader
        active: false
        sourceComponent: txtFileDialogComponent
    }

    Component {
        id: txtFileDialogComponent
        FileDialog {
            title: "Select Reference Transcript"
            nameFilters: ["Text files (*.txt)", "All files (*)"]

            Component.onCompleted: open()

            onAccepted: {
                if (root.locked) {
                    txtFileDialogLoader.active = false
                    return
                }
                var path = AppController.files.urlToLocalPath(selectedFile.toString())
                var content = AppController.files.readTextFile(path)
                refTextEdit.text = content
                txtFileDialogLoader.active = false
            }
            onRejected: txtFileDialogLoader.active = false
        }
    }
}
