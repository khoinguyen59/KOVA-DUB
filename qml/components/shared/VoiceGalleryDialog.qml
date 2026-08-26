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

    // Signal emitted when user selects a voice
    signal voiceSelected(string audioPath, string referenceText, string name, string familyId)

    title: ""
    modal: true
    width: Math.min(1060, parent ? parent.width - 32 : 1060)
    height: Math.min(760, parent ? parent.height - 32 : 760)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    padding: 0

    property var allVoices: []
    property string activeCategory: "all"  // "all", "capcut", "vieneu_bac", "vieneu_trung", "vieneu_nam", "omnivoice", "custom"
    property string activeGender: "all"    // "all", "male", "female"
    property string searchFilter: ""

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
            // Already active on this track -> toggle pause / resume
            if (AppController.player.playing && !AppController.player.paused) {
                AppController.player.pause()
            } else if (AppController.player.paused) {
                AppController.player.resume()
            } else {
                AppController.player.playFile(path)
            }
        } else {
            // Play new track
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
        root.selectedReferenceText = item.referenceText || ""
        root.selectedVoiceName = item.name || ""
        root.voiceSelected(item.audioPath || "", item.referenceText || "", item.name || "", item.modelFamily || "")
        root.close()
    }

    function getFilteredVoices() {
        var result = []
        var query = root.searchFilter.trim().toLowerCase()
        var cat = root.activeCategory
        var gender = root.activeGender

        for (var i = 0; i < root.allVoices.length; ++i) {
            var v = root.allVoices[i]
            if (!v || !v.name) continue

            var vName = String(v.name || "").toLowerCase()
            var vDesc = String(v.description || "").toLowerCase()
            var vAccent = String(v.accent || "").toLowerCase()
            var vFam = String(v.modelFamily || "").toLowerCase()
            var vTags = (v.tags || []).join(" ").toLowerCase()
            var vGender = String(v.gender || "").toLowerCase()
            var vTranscript = String(v.referenceTranscript || v.referenceText || "").toLowerCase()

            // Category matching
            var matchCat = true
            if (cat === "capcut") {
                matchCat = vName.indexOf("capcut") !== -1 || vTags.indexOf("capcut") !== -1 || vAccent.indexOf("capcut") !== -1
            } else if (cat === "vieneu_bac") {
                matchCat = vName.indexOf("bắc") !== -1 || vAccent.indexOf("bắc") !== -1 || vTags.indexOf("miền bắc") !== -1 || vName.indexOf("minh đức") !== -1 || vName.indexOf("đoan trang") !== -1 || vName.indexOf("trúc ly") !== -1
            } else if (cat === "vieneu_trung") {
                matchCat = vName.indexOf("trung") !== -1 || vAccent.indexOf("trung") !== -1 || vTags.indexOf("miền trung") !== -1 || vName.indexOf("quang sơn") !== -1 || vName.indexOf("ngọc trân") !== -1
            } else if (cat === "vieneu_nam") {
                matchCat = vName.indexOf("nam") !== -1 && vName.indexOf("bắc") === -1 && vName.indexOf("trung") === -1 || vAccent.indexOf("nam") !== -1 || vTags.indexOf("miền nam") !== -1 || vName.indexOf("xuân vĩnh") !== -1 || vName.indexOf("thái sơn") !== -1 || vName.indexOf("thục đoan") !== -1
            } else if (cat === "omnivoice") {
                matchCat = vFam === "omnivoice" || vTags.indexOf("omnivoice") !== -1
            } else if (cat === "custom") {
                matchCat = v.isUserPreset === true || v.source === "user"
            }

            if (!matchCat) continue

            // Gender matching
            if (gender !== "all") {
                if (gender === "male" && vGender !== "male") continue
                if (gender === "female" && vGender !== "female") continue
            }

            // Search query matching
            if (query !== "") {
                var matchSearch = vName.indexOf(query) !== -1 ||
                                  vDesc.indexOf(query) !== -1 ||
                                  vAccent.indexOf(query) !== -1 ||
                                  vTags.indexOf(query) !== -1 ||
                                  vTranscript.indexOf(query) !== -1
                if (!matchSearch) continue
            }

            result.push(v)
        }
        return result
    }

    background: Rectangle {
        color: "#161522"
        radius: 14
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Dialog Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Qt.rgba(1, 1, 1, 0.025)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                spacing: Theme.paddingMedium

                Rectangle {
                    width: 38
                    height: 38
                    radius: 8
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                    border.color: Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.4)
                    border.width: 1

                    LineIcon {
                        anchors.centerIn: parent
                        name: "users"
                        color: Theme.accentLight
                        width: 20
                        height: 20
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        spacing: Theme.paddingSmall
                        Text {
                            text: qsTr("Bảng Chọn Giọng Đọc (Voice Gallery)")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontLarge
                            font.bold: true
                        }
                        Rectangle {
                            radius: 10
                            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                            implicitWidth: countText.implicitWidth + 14
                            implicitHeight: 20
                            Text {
                                id: countText
                                anchors.centerIn: parent
                                text: qsTr("%1 Giọng").arg(root.allVoices.length)
                                color: Theme.accentLight
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Text {
                        text: qsTr("Nghe thử trực tiếp và chọn nhanh các giọng đọc CapCut, VieNeu 3 Miền, OmniVoice.")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }
                }

                PrimaryButton {
                    text: qsTr("Đóng")
                    quiet: true
                    implicitWidth: 80
                    implicitHeight: 34
                    onClicked: root.close()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        // Filter and Search Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#1a1928"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingLarge
                anchors.rightMargin: Theme.paddingLarge
                spacing: Theme.paddingMedium

                // Search Box
                TextField {
                    id: searchField
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 38
                    placeholderText: qsTr("🔍 Tìm tên giọng, vùng miền...")
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    selectByMouse: true
                    text: root.searchFilter
                    onTextChanged: root.searchFilter = text

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: "#12111d"
                        border.color: searchField.activeFocus ? Theme.accent : Qt.rgba(1, 1, 1, 0.12)
                        border.width: 1
                    }
                }

                // Category Filter Chips
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                    RowLayout {
                        spacing: 6

                        component FilterChip: Rectangle {
                            property string catId: ""
                            property string label: ""
                            property bool selected: root.activeCategory === catId
                            implicitWidth: chipLabel.implicitWidth + 20
                            implicitHeight: 32
                            radius: 16
                            color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25)
                                            : (chipMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04))
                            border.color: selected ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.1)
                            border.width: selected ? 1.5 : 1

                            Text {
                                id: chipLabel
                                anchors.centerIn: parent
                                text: parent.label
                                color: parent.selected ? Theme.accentLight : Theme.textPrimary
                                font.pixelSize: Theme.fontSmall
                                font.bold: parent.selected
                            }

                            MouseArea {
                                id: chipMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.activeCategory = parent.catId
                            }
                        }

                        FilterChip { catId: "all"; label: qsTr("Tất cả (%1)").arg(root.allVoices.length) }
                        FilterChip { catId: "capcut"; label: qsTr("🔥 CapCut TikTok (22)") }
                        FilterChip { catId: "vieneu_bac"; label: qsTr("Miền Bắc") }
                        FilterChip { catId: "vieneu_trung"; label: qsTr("Miền Trung") }
                        FilterChip { catId: "vieneu_nam"; label: qsTr("Miền Nam") }
                        FilterChip { catId: "omnivoice"; label: qsTr("OmniVoice Studio") }
                    }
                }

                // Gender Filter Switcher
                RowLayout {
                    spacing: 4
                    component GenderButton: Rectangle {
                        property string gId: ""
                        property string label: ""
                        property bool selected: root.activeGender === gId
                        implicitWidth: gLabel.implicitWidth + 14
                        implicitHeight: 30
                        radius: 6
                        color: selected ? Theme.accent : Qt.rgba(1, 1, 1, 0.05)
                        Text {
                            id: gLabel
                            anchors.centerIn: parent
                            text: parent.label
                            color: parent.selected ? "white" : Theme.textSecondary
                            font.pixelSize: 11
                            font.bold: parent.selected
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeGender = parent.gId
                        }
                    }
                    GenderButton { gId: "all"; label: qsTr("Cả hai") }
                    GenderButton { gId: "male"; label: qsTr("Nam") }
                    GenderButton { gId: "female"; label: qsTr("Nữ") }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(1, 1, 1, 0.06) }

        // Main Grid View of Voice Cards
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            property var displayedVoices: root.getFilteredVoices()

            GridView {
                id: voiceGrid
                anchors.fill: parent
                anchors.margins: Theme.paddingLarge
                cellWidth: Math.floor((voiceGrid.width - Theme.paddingLarge * 2) / Math.max(1, Math.floor((voiceGrid.width - Theme.paddingLarge * 2) / 300)))
                cellHeight: 168
                model: parent.displayedVoices

                delegate: Item {
                    width: voiceGrid.cellWidth
                    height: voiceGrid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 6
                        radius: 10
                        color: isPlayingThis ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                                             : (cardMouse.containsMouse ? "#242236" : "#1d1b2b")
                        border.color: isPlayingThis ? Theme.accentLight
                                                    : (cardMouse.containsMouse ? Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.4) : Qt.rgba(1, 1, 1, 0.08))
                        border.width: isPlayingThis ? 2 : 1

                        readonly property bool isPlayingThis: root.isVoicePlaying(modelData)
                        readonly property bool isPausedThis: root.isVoicePaused(modelData)
                        readonly property bool isCapcut: (modelData.name || "").toLowerCase().indexOf("capcut") !== -1 || (modelData.tags || []).indexOf("CapCut") !== -1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            // Top Row: Badges & Accent
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    radius: 4
                                    implicitWidth: badgeText.implicitWidth + 8
                                    implicitHeight: 20
                                    color: isCapcut ? "#ff0055" : (modelData.modelFamily === "omnivoice" ? "#8833ff" : "#00aa77")
                                    Text {
                                        id: badgeText
                                        anchors.centerIn: parent
                                        text: isCapcut ? "CapCut" : (modelData.modelFamily === "omnivoice" ? "OmniVoice" : "VieNeu")
                                        color: "white"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    radius: 4
                                    implicitWidth: genText.implicitWidth + 8
                                    implicitHeight: 20
                                    color: Qt.rgba(1, 1, 1, 0.07)
                                    Text {
                                        id: genText
                                        anchors.centerIn: parent
                                        text: modelData.gender === "female" ? qsTr("Nữ") : (modelData.gender === "male" ? qsTr("Nam") : qsTr("Giọng"))
                                        color: Theme.textSecondary
                                        font.pixelSize: 10
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: modelData.accent || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 100
                                }
                            }

                            // Middle: Voice Name & Description
                            Text {
                                Layout.fillWidth: true
                                text: (modelData.name || "").replace("CapCut: ", "").replace("OmniVoice: ", "")
                                color: isPlayingThis ? Theme.accentLight : Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: modelData.referenceTranscript || modelData.description || modelData.referenceText || ""
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            // Bottom: Play/Pause Button & Select Button
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                // PLAY / PAUSE BUTTON
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    radius: 6
                                    color: isPlayingThis ? Theme.accent : Qt.rgba(1, 1, 1, 0.06)
                                    border.color: isPlayingThis ? Theme.accentLight : (playMouse.containsMouse ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.12))
                                    border.width: 1

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text {
                                            text: isPlayingThis ? "⏸" : (isPausedThis ? "▶" : "▶")
                                            color: isPlayingThis ? "white" : Theme.accentLight
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        Text {
                                            text: isPlayingThis ? qsTr("Tạm dừng") : (isPausedThis ? qsTr("Tiếp tục") : qsTr("Nghe thử"))
                                            color: isPlayingThis ? "white" : Theme.textPrimary
                                            font.pixelSize: 11
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

                                // CHỌN GIỌNG BUTTON
                                PrimaryButton {
                                    text: qsTr("Chọn")
                                    implicitHeight: 34
                                    implicitWidth: 64
                                    onClicked: root.selectVoice(modelData)
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            z: -1
                            onDoubleClicked: root.selectVoice(modelData)
                        }
                    }
                }
            }
        }
    }
}
