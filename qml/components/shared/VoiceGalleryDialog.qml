import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "../base"

Dialog {
    id: root

    property string familyId: ""
    property string selectedAudioPath: ""
    property string selectedReferenceText: ""
    property string selectedVoiceName: ""
    property string currentPlayingPath: ""

    // Delete management for custom clone voices
    property string pendingDeleteId: ""
    property string pendingDeleteName: ""

    // Signal emitted when user selects a voice
    signal voiceSelected(string audioPath, string referenceText, string name, string familyId, string voiceId)

    title: ""
    modal: true
    width: Math.min(1120, parent ? parent.width - 32 : 1120)
    height: Math.min(800, parent ? parent.height - 32 : 800)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    property var allVoices: []
    property string activeCategory: "all"
    property string activeGender: "all"
    property string searchFilter: ""

    // Fully reactive filtered list
    property var filteredVoices: calculateFilteredVoices(root.allVoices, root.activeCategory, root.activeGender, root.searchFilter)

    Component.onCompleted: {
        refreshVoices()
    }

    onOpened: {
        refreshVoices()
    }

    onClosed: {
        if (AppController.player && AppController.player.playing) {
            AppController.player.stop()
        }
        currentPlayingPath = ""
    }

    Connections {
        target: AppController.voiceClonePresets
        function onPresetsChanged(famId) {
            root.refreshVoices()
        }
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) {
                root.currentPlayingPath = ""
            }
        }
        function onPlaybackFinished() {
            root.currentPlayingPath = ""
        }
    }

    function refreshVoices() {
        var list = []
        if (AppController.voiceClonePresets) {
            list = AppController.voiceClonePresets.allPresets() || []
        }
        root.allVoices = list
    }

    function resolveAudioPath(item) {
        if (!item) return ""
        var path = item.audioPath || item.referenceAudio || item.refAudio || ""
        if (AppController.files && AppController.files.urlToLocalPath) {
            path = AppController.files.urlToLocalPath(path)
        }
        return path
    }

    function togglePlayVoice(item) {
        if (!item) return
        var path = resolveAudioPath(item)
        if (!path || path.length === 0) return
        
        if (root.currentPlayingPath === path) {
            if (AppController.player && AppController.player.playing && !AppController.player.paused) {
                AppController.player.pause()
            } else if (AppController.player && AppController.player.paused) {
                AppController.player.resume()
            } else if (AppController.player) {
                AppController.player.playFile(path)
            }
        } else {
            if (AppController.player && AppController.player.playing) {
                AppController.player.stop()
            }
            root.currentPlayingPath = path
            if (AppController.player) {
                AppController.player.playFile(path)
            }
        }
    }

    function isVoicePlaying(item) {
        var path = resolveAudioPath(item)
        return path && root.currentPlayingPath === path && AppController.player && AppController.player.playing && !AppController.player.paused
    }

    function isVoicePaused(item) {
        var path = resolveAudioPath(item)
        return path && root.currentPlayingPath === path && AppController.player && AppController.player.paused
    }

    function selectVoice(item) {
        if (!item) return
        if (AppController.player && AppController.player.playing) {
            AppController.player.stop()
        }
        root.selectedAudioPath = resolveAudioPath(item)
        root.selectedReferenceText = item.referenceTranscript || item.referenceText || ""
        root.selectedVoiceName = (item.name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
        root.voiceSelected(root.selectedAudioPath, root.selectedReferenceText,
                           item.name || root.selectedVoiceName,
                           item.sourceModelFamily || item.modelFamily || item.familyId || "",
                           item.id || "")
        root.close()
    }

    function hasVoiceTarget(v, target) {
        if (!v) return false
        var requested = String(target || "").toLowerCase()
        var targets = v.voiceModelTargets
        if (!targets || typeof targets.length !== "number") return false
        for (var i = 0; i < targets.length; ++i) {
            var value = String(targets[i] || "").toLowerCase()
            if (value === requested
                    || (requested === "vieneu" && value.indexOf("vieneu-tts") === 0)) {
                return true
            }
        }
        return false
    }

    function containsAny(value, candidates) {
        var text = String(value || "").toLowerCase()
        for (var i = 0; i < candidates.length; ++i) {
            if (text.indexOf(candidates[i]) !== -1) return true
        }
        return false
    }

    function matchesCategory(v, cat) {
        if (!v) return false
        var vName = String(v.name || "").toLowerCase()
        var vAccent = String(v.accent || "").toLowerCase()
        var vFam = String(v.modelFamily || v.familyId || "").toLowerCase()
        var vCat = String(v.category || "").toLowerCase()
        var vTags = Array.isArray(v.tags) ? v.tags.join(" ").toLowerCase() : String(v.tags || "").toLowerCase()
        var isCustom = v.isUserPreset === true || v.canDelete === true || v.isBuiltin === false
        var isVieNeu = hasVoiceTarget(v, "vieneu")
                || vCat === "vieneu" || vFam === "vieneu-tts" || vFam.indexOf("vieneu-tts-") === 0
                || vTags.indexOf("vieneu") !== -1
        var regionText = vAccent + " " + vTags + " " + vName

        if (cat === "all") return true
        if (cat === "custom") return isCustom
        if (cat === "capcut") return vCat === "capcut" || vName.indexOf("capcut") !== -1 || vTags.indexOf("capcut") !== -1
        if (cat === "vieneu_bac") return isVieNeu && containsAny(regionText, ["bắc", "bac", "north"])
        if (cat === "vieneu_trung") return isVieNeu && containsAny(regionText, ["trung", "central", "middle"])
        if (cat === "vieneu_nam") return isVieNeu && containsAny(regionText, ["nam", "south"])
        // The target badge is authoritative for migrated and user-created
        // records. Legacy source-family checks remain for old JSON records.
        if (cat === "omnivoice") return hasVoiceTarget(v, "omnivoice")
                || vCat === "omnivoice" || vFam === "omnivoice"
                || vTags.indexOf("omnivoice") !== -1 || vName.indexOf("omnivoice") !== -1
        return false
    }

    function getCountForCategory(cat) {
        if (!root.allVoices) return 0
        if (cat === "all") return root.allVoices.length
        var count = 0
        for (var i = 0; i < root.allVoices.length; ++i) {
            if (matchesCategory(root.allVoices[i], cat)) {
                count++
            }
        }
        return count
    }

    function calculateFilteredVoices(voicesList, cat, gender, queryRaw) {
        if (!voicesList || voicesList.length === 0) return []
        var query = (queryRaw || "").trim().toLowerCase()
        var res = []
        for (var i = 0; i < voicesList.length; ++i) {
            var v = voicesList[i]
            if (!v || !v.name) continue

            // Category match
            if (!matchesCategory(v, cat)) continue

            // Gender match
            var vGender = String(v.gender || "").toLowerCase()
            if (gender !== "all") {
                if (gender === "male" && vGender !== "male") continue
                if (gender === "female" && vGender !== "female") continue
            }

            // Search query match
            if (query !== "") {
                var vName = String(v.name || "").toLowerCase()
                var vDesc = String(v.description || "").toLowerCase()
                var vAccent = String(v.accent || "").toLowerCase()
                var vTags = Array.isArray(v.tags) ? v.tags.join(" ").toLowerCase() : String(v.tags || "").toLowerCase()
                var vTranscript = String(v.referenceTranscript || v.referenceText || "").toLowerCase()

                if (vName.indexOf(query) === -1 && 
                    vDesc.indexOf(query) === -1 && 
                    vAccent.indexOf(query) === -1 && 
                    vTags.indexOf(query) === -1 && 
                    vTranscript.indexOf(query) === -1) {
                    continue
                }
            }

            res.push(v)
        }
        return res
    }

    // Modal Background
    background: Rectangle {
        color: "#12111d"
        radius: 16
        border.color: "#302d47"
        border.width: 1
    }

    // Delete confirmation dialog for custom cloned voices
    ConfirmationDialog {
        id: confirmDeleteDialog
        parent: Overlay.overlay
        titleText: qsTr("Xóa Giọng Clone")
        messageText: qsTr("Bạn có chắc chắn muốn xóa giọng '%1' khỏi thư viện cá nhân? Hành động này không thể hoàn tác.").arg(root.pendingDeleteName)
        confirmText: qsTr("Xóa vĩnh viễn")
        isDestructive: true
        onConfirmed: {
            if (root.pendingDeleteId !== "") {
                AppController.voiceClonePresets.deletePreset(root.pendingDeleteId)
                root.pendingDeleteId = ""
                root.pendingDeleteName = ""
                root.refreshVoices()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ==========================================
        // 1. MODAL HEADER (Top Title Bar)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            color: "#181726"
            radius: 16

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#28253d"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                // Header Icon Glow Box
                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 10
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#7c4dff" }
                        GradientStop { position: 1.0; color: "#5b21b6" }
                    }
                    border.color: "#a27eff"
                    border.width: 1

                    LineIcon {
                        anchors.centerIn: parent
                        name: "users"
                        color: "#ffffff"
                        implicitWidth: 22
                        implicitHeight: 22
                    }
                }

                // Title & Subtitle
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        spacing: 8
                        Text {
                            text: qsTr("Bảng Chọn Giọng Đọc (Voice Gallery)")
                            color: "#ffffff"
                            font.pixelSize: 18
                            font.bold: true
                        }

                    }

                    Text {
                        text: qsTr("Preview a catalog voice or saved reference, then choose it for this dubbing run.")
                        color: "#aea8d1"
                        font.pixelSize: 12
                    }
                }

                // Close Button
                PrimaryButton {
                    text: qsTr("Đóng")
                    quiet: true
                    implicitWidth: 84
                    implicitHeight: 36
                    onClicked: root.close()
                }
            }
        }

        // ==========================================
        // 2. SEARCH & FILTER SHELF
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: filterColumn.implicitHeight + 24
            color: "#151422"

            ColumnLayout {
                id: filterColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // Row 1: Search Bar & Gender Switcher
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // Custom Search Input Box
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 8
                        color: "#211f33"
                        border.color: searchInput.activeFocus ? "#a27eff" : "#322f4d"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            LineIcon {
                                name: "search"
                                color: "#8d87b3"
                                implicitWidth: 16
                                implicitHeight: 16
                            }

                            TextField {
                                id: searchInput
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: root.searchFilter
                                placeholderText: qsTr("Tìm kiếm tên giọng, vùng miền (Bắc/Trung/Nam), cảm xúc, phong cách...")
                                color: "#ffffff"
                                placeholderTextColor: "#8d87b3"
                                font.pixelSize: 13
                                background: Item {}
                                verticalAlignment: Text.AlignVCenter
                                onTextChanged: root.searchFilter = text
                            }

                            Rectangle {
                                visible: root.searchFilter !== ""
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                radius: 10
                                color: "#302d47"
                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: "#aea8d1"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        searchInput.text = ""
                                        root.searchFilter = ""
                                    }
                                }
                            }
                        }
                    }

                    // Gender Filter Switcher (Segmented Control)
                    Rectangle {
                        implicitHeight: 40
                        implicitWidth: genderRow.implicitWidth + 8
                        radius: 8
                        color: "#211f33"
                        border.color: "#322f4d"
                        border.width: 1

                        RowLayout {
                            id: genderRow
                            anchors.centerIn: parent
                            spacing: 4

                            Repeater {
                                model: [
                                    { gId: "all", label: qsTr("Tất cả") },
                                    { gId: "male", label: qsTr("👨 Nam") },
                                    { gId: "female", label: qsTr("👩 Nữ") }
                                ]
                                delegate: Rectangle {
                                    readonly property bool selected: root.activeGender === modelData.gId
                                    implicitWidth: gTabLabel.implicitWidth + 16
                                    implicitHeight: 32
                                    radius: 6
                                    color: selected ? "#7c4dff" : (gTabMouse.containsMouse ? "#2f2c4a" : "transparent")
                                    border.color: selected ? "#a27eff" : "transparent"
                                    border.width: 1

                                    Text {
                                        id: gTabLabel
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: selected ? "#ffffff" : "#c7c2dc"
                                        font.pixelSize: 12
                                        font.bold: selected
                                    }
                                    MouseArea {
                                        id: gTabMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.activeGender = modelData.gId
                                    }
                                }
                            }
                        }
                    }
                }

                // Row 2: Category Filter Chips
                Flickable {
                    Layout.fillWidth: true
                    implicitHeight: 34
                    contentWidth: categoryRow.implicitWidth
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    RowLayout {
                        id: categoryRow
                        spacing: 8

                        Repeater {
                            model: [
                                { catId: "all", label: qsTr("Tất cả") },
                                { catId: "capcut", label: qsTr("🔥 CapCut TikTok") },
                                { catId: "vieneu_bac", label: qsTr("🇻🇳 Miền Bắc") },
                                { catId: "vieneu_trung", label: qsTr("🇻🇳 Miền Trung") },
                                { catId: "vieneu_nam", label: qsTr("🇻🇳 Miền Nam") },
                                { catId: "omnivoice", label: qsTr("🎙️ OmniVoice AI") },
                                { catId: "custom", label: qsTr("⭐ Giọng của tôi") }
                            ]
                            delegate: Rectangle {
                                readonly property bool selected: root.activeCategory === modelData.catId
                                readonly property int count: root.getCountForCategory(modelData.catId)

                                implicitWidth: chipRow.implicitWidth + 20
                                implicitHeight: 32
                                radius: 16
                                color: selected ? Qt.rgba(0.49, 0.30, 1.0, 0.25)
                                                : (chipMouse.containsMouse ? "#27243d" : "#1d1b2e")
                                border.color: selected ? "#a27eff" : (chipMouse.containsMouse ? "#403d5e" : "#2d2a45")
                                border.width: selected ? 2 : 1

                                RowLayout {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Text {
                                        text: modelData.label
                                        color: selected ? "#ffffff" : "#c7c2dc"
                                        font.pixelSize: 12
                                        font.bold: selected
                                    }

                                    Rectangle {
                                        radius: 8
                                        implicitWidth: chipCount.implicitWidth + 8
                                        implicitHeight: 16
                                        color: selected ? "#7c4dff" : "#2e2b46"
                                        Text {
                                            id: chipCount
                                            anchors.centerIn: parent
                                            text: String(count)
                                            color: selected ? "#ffffff" : "#aea8d1"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }

                                MouseArea {
                                    id: chipMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.activeCategory = modelData.catId
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#28253d" }

        // ==========================================
        // 3. MAIN BENTO GRID VIEW
        // ==========================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // GridView displaying all cards
            GridView {
                id: voiceGrid
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                cellWidth: Math.floor((width - 32) / Math.max(1, Math.floor((width - 32) / 340)))
                cellHeight: 215
                model: root.filteredVoices
                visible: root.filteredVoices && root.filteredVoices.length > 0

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Item {
                    width: voiceGrid.cellWidth
                    height: voiceGrid.cellHeight

                    Rectangle {
                        id: cardRect
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: 12

                        readonly property bool isPlayingThis: root.isVoicePlaying(modelData)
                        readonly property bool isPausedThis: root.isVoicePaused(modelData)
                        readonly property bool isCustomVoice: modelData.canDelete === true || modelData.isUserPreset === true || modelData.isBuiltin === false
                        readonly property bool isCapcut: (modelData.name || "").toLowerCase().indexOf("capcut") !== -1 || (Array.isArray(modelData.tags) && modelData.tags.indexOf("CapCut") !== -1)
                        readonly property bool isVieNeu: root.hasVoiceTarget(modelData, "vieneu")
                                                         || String(modelData.modelFamily || "").toLowerCase().indexOf("vieneu-tts") === 0
                                                         || String(modelData.category || "").toLowerCase() === "vieneu"
                                                         || (Array.isArray(modelData.tags) && modelData.tags.indexOf("VieNeu-TTS") !== -1)

                        color: isPlayingThis ? "#231f38"
                                             : (cardHover.containsMouse ? "#27243c" : "#1b1929")
                        border.color: isPlayingThis ? "#a27eff"
                                                    : (cardHover.containsMouse ? "#4d4773" : "#2d2a45")
                        border.width: isPlayingThis ? 2 : 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            // 1. Card Top Bar: AVATAR ICON + NAME & BADGES + DELETE BUTTON
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                // AVATAR ICON CIRCLE
                                Rectangle {
                                    Layout.preferredWidth: 44
                                    Layout.preferredHeight: 44
                                    radius: 22
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0.0
                                            color: cardRect.isCustomVoice ? "#f59e0b"
                                                                          : (cardRect.isCapcut ? "#ff0055"
                                                                                               : (cardRect.isVieNeu ? "#059669" : "#7c4dff"))
                                        }
                                        GradientStop {
                                            position: 1.0
                                            color: cardRect.isCustomVoice ? "#b45309"
                                                                          : (cardRect.isCapcut ? "#be123c"
                                                                                               : (cardRect.isVieNeu ? "#065f46" : "#4c1d95"))
                                        }
                                    }
                                    border.color: Qt.rgba(1, 1, 1, 0.25)
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            var n = (modelData.name || "").toLowerCase()
                                            if (cardRect.isCustomVoice) return "⭐"
                                            if (n.indexOf("bé") !== -1 || n.indexOf("mới lớn") !== -1) return "👶"
                                            if (n.indexOf("robot") !== -1) return "🤖"
                                            if (n.indexOf("quacks") !== -1) return "🦆"
                                            if (n.indexOf("villain") !== -1 || n.indexOf("crusty") !== -1) return "🎭"
                                            if (modelData.gender === "female") return "👩"
                                            if (modelData.gender === "male") return "👨"
                                            return "🎙️"
                                        }
                                        font.pixelSize: 20
                                    }
                                }

                                // Voice Name and Tag Pills
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.name || qsTr("Giọng đọc mẫu")).replace("CapCut: ", "").replace("OmniVoice: ", "")
                                            color: cardRect.isPlayingThis ? "#a27eff" : "#ffffff"
                                            font.pixelSize: 14
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        // DELETE BUTTON (ONLY FOR CUSTOM/CLONED VOICES)
                                        Rectangle {
                                            visible: cardRect.isCustomVoice
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            radius: 4
                                            color: trashHover.containsMouse ? "#7f1d1d" : "#381720"
                                            border.color: trashHover.containsMouse ? "#ef4444" : "#5c202a"
                                            border.width: 1

                                            LineIcon {
                                                anchors.centerIn: parent
                                                name: "trash"
                                                color: "#f87171"
                                                implicitWidth: 14
                                                implicitHeight: 14
                                            }

                                            MouseArea {
                                                id: trashHover
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    root.pendingDeleteId = modelData.id || ""
                                                    root.pendingDeleteName = modelData.name || qsTr("Giọng clone")
                                                    confirmDeleteDialog.open()
                                                }
                                            }
                                        }
                                    }

                                    // Source & Gender/Accent Pills
                                    RowLayout {
                                        spacing: 4

                                        Rectangle {
                                            radius: 3
                                            implicitWidth: bTxt.implicitWidth + 8
                                            implicitHeight: 18
                                            color: cardRect.isCustomVoice ? "#d97706"
                                                                          : (cardRect.isCapcut ? "#e11d48"
                                                                                               : (cardRect.isVieNeu ? "#059669" : "#6d28d9"))
                                            Text {
                                                id: bTxt
                                                anchors.centerIn: parent
                                                text: cardRect.isCustomVoice ? "Cá nhân"
                                                                             : (cardRect.isCapcut ? "CapCut"
                                                                                                  : (cardRect.isVieNeu ? "VieNeu" : "OmniVoice"))
                                                color: "#ffffff"
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        Rectangle {
                                            radius: 3
                                            implicitWidth: vieNeuTargetText.implicitWidth + 8
                                            implicitHeight: 18
                                            color: "#065f46"
                                            border.color: "#34d399"
                                            border.width: 1
                                            Text {
                                                id: vieNeuTargetText
                                                anchors.centerIn: parent
                                                text: "VieNeu"
                                                color: "#d1fae5"
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        Rectangle {
                                            radius: 3
                                            implicitWidth: omniVoiceTargetText.implicitWidth + 8
                                            implicitHeight: 18
                                            color: "#4c1d95"
                                            border.color: "#a78bfa"
                                            border.width: 1
                                            Text {
                                                id: omniVoiceTargetText
                                                anchors.centerIn: parent
                                                text: "OmniVoice"
                                                color: "#ede9fe"
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        Rectangle {
                                            radius: 3
                                            implicitWidth: gTxt.implicitWidth + 8
                                            implicitHeight: 18
                                            color: "#252238"
                                            border.color: "#35314f"
                                            border.width: 1

                                            Text {
                                                id: gTxt
                                                anchors.centerIn: parent
                                                text: {
                                                    var g = modelData.gender === "female" ? "Nữ" : (modelData.gender === "male" ? "Nam" : "")
                                                    var a = modelData.accent || ""
                                                    if (g && a) return g + " · " + a
                                                    if (g) return g
                                                    return a || "Voice"
                                                }
                                                color: "#c7c2dc"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }

                            // 2. Card Middle: Transcript Preview Text
                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: modelData.referenceTranscript || modelData.referenceText || modelData.description || qsTr("Giọng đọc tự nhiên, sẵn sàng sử dụng cho Voice Cloning và TTS.")
                                color: "#aea8d1"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            // 3. Card Bottom: PLAY/PAUSE BUTTON + SELECT BUTTON
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                // PLAY / PAUSE BUTTON
                                Rectangle {
                                    id: playBtnRect
                                    Layout.fillWidth: true
                                    implicitHeight: 36
                                    radius: 6
                                    color: cardRect.isPlayingThis ? "#7c4dff"
                                                                 : (playMouse.containsMouse ? "#322f4d" : "#242236")
                                    border.color: cardRect.isPlayingThis ? "#c084fc"
                                                                        : (playMouse.containsMouse ? "#a27eff" : "#3a3658")
                                    border.width: cardRect.isPlayingThis ? 1.5 : 1

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Item {
                                            implicitWidth: 16
                                            implicitHeight: 16

                                            LineIcon {
                                                anchors.centerIn: parent
                                                visible: !cardRect.isPlayingThis
                                                name: "play"
                                                color: cardRect.isPlayingThis ? "#ffffff" : "#a27eff"
                                                implicitWidth: 14
                                                implicitHeight: 14
                                            }

                                            // 3 Animated Equalizer Bars when Playing
                                            Row {
                                                anchors.centerIn: parent
                                                spacing: 2
                                                visible: cardRect.isPlayingThis

                                                Rectangle {
                                                    width: 3; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { from: 10; to: 4; duration: 250 }
                                                        NumberAnimation { to: 14; duration: 300 }
                                                        NumberAnimation { to: 8; duration: 200 }
                                                        NumberAnimation { to: 10; duration: 200 }
                                                    }
                                                }
                                                Rectangle {
                                                    width: 3; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { from: 14; to: 5; duration: 200 }
                                                        NumberAnimation { to: 14; duration: 350 }
                                                        NumberAnimation { to: 12; duration: 250 }
                                                        NumberAnimation { to: 14; duration: 200 }
                                                    }
                                                }
                                                Rectangle {
                                                    width: 3; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { from: 8; to: 12; duration: 300 }
                                                        NumberAnimation { to: 4; duration: 200 }
                                                        NumberAnimation { to: 10; duration: 250 }
                                                        NumberAnimation { to: 8; duration: 200 }
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            text: cardRect.isPlayingThis ? qsTr("Tạm dừng") : (cardRect.isPausedThis ? qsTr("Tiếp tục") : qsTr("Nghe thử"))
                                            color: "#ffffff"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: playMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.togglePlayVoice(modelData)
                                    }
                                }

                                // SELECT BUTTON
                                PrimaryButton {
                                    text: qsTr("Chọn")
                                    implicitHeight: 36
                                    implicitWidth: 70
                                    buttonColor: "#7c4dff"
                                    onClicked: root.selectVoice(modelData)
                                }
                            }
                        }

                        MouseArea {
                            id: cardHover
                            anchors.fill: parent
                            hoverEnabled: true
                            z: -1
                            onDoubleClicked: root.selectVoice(modelData)
                        }
                    }
                }
            }

            // Empty State Display
            Item {
                anchors.fill: parent
                visible: !root.filteredVoices || root.filteredVoices.length === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    LineIcon {
                        Layout.alignment: Qt.AlignHCenter
                        name: "volume"
                        color: "#5b567a"
                        implicitWidth: 48
                        implicitHeight: 48
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Không tìm thấy giọng đọc phù hợp")
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Thử thay đổi từ khóa tìm kiếm hoặc chọn danh mục khác.")
                        color: "#aea8d1"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
