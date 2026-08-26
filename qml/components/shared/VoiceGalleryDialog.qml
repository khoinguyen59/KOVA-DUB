import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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
    signal voiceSelected(string audioPath, string referenceText, string name, string familyId)

    title: ""
    modal: true
    width: Math.min(1080, parent ? parent.width - 32 : 1080)
    height: Math.min(780, parent ? parent.height - 32 : 780)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    property var allVoices: []
    property string activeCategory: "all"
    property string activeGender: "all"
    property string searchFilter: ""

    property var filteredVoices: {
        var _v = root.allVoices
        var _c = root.activeCategory
        var _g = root.activeGender
        var _q = root.searchFilter.trim().toLowerCase()
        return root.calculateFilteredVoices(_v, _c, _g, _q)
    }

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

    function togglePlayVoice(item) {
        if (!item || !item.audioPath) return
        var path = item.audioPath
        
        if (root.currentPlayingPath === path) {
            if (AppController.player.playing && !AppController.player.paused) {
                AppController.player.pause()
            } else if (AppController.player.paused) {
                AppController.player.resume()
            } else {
                AppController.player.playFile(path)
            }
        } else {
            root.currentPlayingPath = path
            AppController.player.playFile(path)
        }
    }

    function isVoicePlaying(item) {
        return item && item.audioPath && root.currentPlayingPath === item.audioPath && AppController.player && AppController.player.playing && !AppController.player.paused
    }

    function isVoicePaused(item) {
        return item && item.audioPath && root.currentPlayingPath === item.audioPath && AppController.player && AppController.player.paused
    }

    function selectVoice(item) {
        if (!item) return
        if (AppController.player && AppController.player.playing) {
            AppController.player.stop()
        }
        root.selectedAudioPath = item.audioPath || ""
        root.selectedReferenceText = item.referenceTranscript || item.referenceText || ""
        root.selectedVoiceName = (item.name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
        root.voiceSelected(root.selectedAudioPath, root.selectedReferenceText, root.selectedVoiceName, item.modelFamily || "")
        root.close()
    }

    function matchesCategory(v, cat) {
        if (!v) return false
        var vName = String(v.name || "").toLowerCase()
        var vAccent = String(v.accent || "").toLowerCase()
        var vFam = String(v.modelFamily || v.familyId || "").toLowerCase()
        var vTags = Array.isArray(v.tags) ? v.tags.join(" ").toLowerCase() : String(v.tags || "").toLowerCase()
        var isCustom = v.isUserPreset === true || v.canDelete === true || v.isBuiltin === false

        if (cat === "all") return true
        if (cat === "custom") return isCustom
        if (cat === "capcut") return vName.indexOf("capcut") !== -1 || vTags.indexOf("capcut") !== -1 || vAccent.indexOf("capcut") !== -1
        if (cat === "vieneu_bac") return vAccent.indexOf("bắc") !== -1 || vTags.indexOf("bắc") !== -1 || vName.indexOf("bắc") !== -1 || vName.indexOf("minh đức") !== -1 || vName.indexOf("ngọc huyền") !== -1 || vName.indexOf("trúc ly") !== -1 || vName.indexOf("ngọc linh") !== -1
        if (cat === "vieneu_trung") return vAccent.indexOf("trung") !== -1 || vTags.indexOf("trung") !== -1 || vName.indexOf("trung") !== -1 || vName.indexOf("quang sơn") !== -1 || vName.indexOf("ngọc trân") !== -1
        if (cat === "vieneu_nam") return (vAccent.indexOf("nam") !== -1 && vAccent.indexOf("bắc") === -1) || (vTags.indexOf("miền nam") !== -1) || vName.indexOf("nam · nam") !== -1 || vName.indexOf("xuân vĩnh") !== -1 || vName.indexOf("thục đoan") !== -1 || vName.indexOf("vĩnh nam") !== -1
        if (cat === "omnivoice") return vFam === "omnivoice" || vTags.indexOf("omnivoice") !== -1 || vName.indexOf("omnivoice") !== -1
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

    function calculateFilteredVoices(voicesList, cat, gender, query) {
        if (!voicesList || voicesList.length === 0) return []
        var res = []
        for (var i = 0; i < voicesList.length; ++i) {
            var v = voicesList[i]
            if (!v || !v.name) continue

            // Category match
            if (!matchesCategory(v, cat)) continue

            // Gender match
            var vGender = String(v.gender || "").toLowerCase()
            if (gender === "male" && vGender !== "male") continue
            if (gender === "female" && vGender !== "female") continue

            // Search filter match
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
        color: "#13121d"
        radius: 16
        border.color: "#302d47"
        border.width: 1
    }

    // Delete confirmation dialog for custom cloned voices
    ConfirmationDialog {
        id: confirmDeleteDialog
        parent: Overlay.overlay
        titleText: qsTr("Xóa Giọng Clone")
        messageText: qsTr("Bạn có chắc chắn muốn xóa giọng '%1' khỏi thư viện giọng clone cá nhân? Hành động này không thể hoàn tác.").arg(root.pendingDeleteName)
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
        // 1. MODAL HEADER
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

                Rectangle {
                    width: 42
                    height: 42
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

                        Rectangle {
                            radius: 10
                            implicitWidth: countBadge.implicitWidth + 12
                            implicitHeight: 20
                            color: "#7c4dff"
                            Text {
                                id: countBadge
                                anchors.centerIn: parent
                                text: qsTr("%1 Giọng").arg(root.allVoices.length)
                                color: "#ffffff"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }

                    Text {
                        text: qsTr("Nghe thử trực tiếp và chọn nhanh hơn 60+ giọng đọc Tiếng Việt & Quốc tế đỉnh cao (CapCut TikTok, VieNeu 3 Miền, OmniVoice).")
                        color: "#aea8d1"
                        font.pixelSize: 12
                    }
                }

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
                                width: 20
                                height: 20
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

            GridView {
                id: voiceGrid
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                cellWidth: Math.floor((width - 32) / Math.max(1, Math.floor((width - 32) / 330)))
                cellHeight: 190
                model: root.filteredVoices
                visible: root.filteredVoices.length > 0

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
                        readonly property bool isVieNeu: (modelData.modelFamily === "vieneu-tts") || (Array.isArray(modelData.tags) && modelData.tags.indexOf("VieNeu-TTS") !== -1)

                        color: isPlayingThis ? "#231f38"
                                             : (cardHover.containsMouse ? "#27243c" : "#1b1929")
                        border.color: isPlayingThis ? "#a27eff"
                                                    : (cardHover.containsMouse ? "#4d4773" : "#2d2a45")
                        border.width: isPlayingThis ? 2 : 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            // 1. Card Top Bar
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    radius: 4
                                    implicitWidth: sourceBadgeText.implicitWidth + 10
                                    implicitHeight: 22
                                    color: cardRect.isCustomVoice ? "#d97706"
                                                                  : (cardRect.isCapcut ? "#ff0055"
                                                                                       : (cardRect.isVieNeu ? "#059669" : "#7c4dff"))

                                    Text {
                                        id: sourceBadgeText
                                        anchors.centerIn: parent
                                        text: cardRect.isCustomVoice ? qsTr("Giọng của tôi")
                                                                     : (cardRect.isCapcut ? "CapCut"
                                                                                          : (cardRect.isVieNeu ? "VieNeu" : "OmniVoice"))
                                        color: "#ffffff"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    radius: 4
                                    implicitWidth: detailBadgeText.implicitWidth + 8
                                    implicitHeight: 22
                                    color: "#252238"
                                    border.color: "#35314f"
                                    border.width: 1

                                    Text {
                                        id: detailBadgeText
                                        anchors.centerIn: parent
                                        text: {
                                            var g = modelData.gender === "female" ? qsTr("Nữ") : (modelData.gender === "male" ? qsTr("Nam") : "")
                                            var acc = modelData.accent || ""
                                            if (g && acc) return g + " · " + acc
                                            if (g) return g
                                            if (acc) return acc
                                            return qsTr("Giọng đọc")
                                        }
                                        color: "#c7c2dc"
                                        font.pixelSize: 10
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // Delete Button (ONLY FOR USER CUSTOM/CLONED VOICES)
                                Rectangle {
                                    visible: cardRect.isCustomVoice
                                    width: 24
                                    height: 24
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

                            // 2. Card Middle
                            Text {
                                Layout.fillWidth: true
                                text: (modelData.name || qsTr("Giọng đọc mẫu")).replace("CapCut: ", "").replace("OmniVoice: ", "")
                                color: cardRect.isPlayingThis ? "#a27eff" : "#ffffff"
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: modelData.referenceTranscript || modelData.description || modelData.referenceText || qsTr("Giọng đọc tự nhiên, sẵn sàng sử dụng cho Voice Cloning và TTS.")
                                color: "#c7c2dc"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            // 3. Card Bottom: Play/Pause Button + Select Button
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    id: playBtn
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    radius: 6
                                    color: cardRect.isPlayingThis ? "#7c4dff"
                                                                 : (playMouse.containsMouse ? "#322f4d" : "#242236")
                                    border.color: cardRect.isPlayingThis ? "#a27eff"
                                                                        : (playMouse.containsMouse ? "#a27eff" : "#3a3658")
                                    border.width: 1

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

                                            Row {
                                                anchors.centerIn: parent
                                                spacing: 2
                                                visible: cardRect.isPlayingThis

                                                Rectangle {
                                                    width: 3; height: 10; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { to: 4; duration: 250 }
                                                        NumberAnimation { to: 14; duration: 300 }
                                                        NumberAnimation { to: 8; duration: 200 }
                                                    }
                                                }
                                                Rectangle {
                                                    width: 3; height: 14; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { to: 14; duration: 200 }
                                                        NumberAnimation { to: 5; duration: 350 }
                                                        NumberAnimation { to: 12; duration: 250 }
                                                    }
                                                }
                                                Rectangle {
                                                    width: 3; height: 8; color: "#ffffff"; radius: 1
                                                    SequentialAnimation on height {
                                                        loops: Animation.Infinite; running: cardRect.isPlayingThis
                                                        NumberAnimation { to: 12; duration: 300 }
                                                        NumberAnimation { to: 4; duration: 200 }
                                                        NumberAnimation { to: 10; duration: 250 }
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

                                PrimaryButton {
                                    text: qsTr("Chọn")
                                    implicitHeight: 34
                                    implicitWidth: 68
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

            // Empty State
            Item {
                anchors.fill: parent
                visible: root.filteredVoices.length === 0

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