import QtQuick
import QtQuick.Layouts
import "../../base"
import "../../shared"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property string playingSeparationStem: ""
    property bool stepComplete: false

    signal playSeparationRequested(string kind, string path)
    signal runRequested(string nodeId)
    signal nextStepRequested()
    signal previousStepRequested()

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
            LineIcon { name: "layers"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("3. TÁCH GIỌNG NÓI & NHẠC NỀN (SEPARATE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        VoiceSeparationOutput {
            Layout.fillWidth: true
            compact: true
            showActions: true
            showPlaybackControls: true
            showExportButton: false
            showWaveforms: false
            vocalsPath: root.dubbing.vocalsPath
            backgroundPath: root.dubbing.backgroundPath
            playingStem: root.playingSeparationStem
            onPlayRequested: function(kind, path) {
                root.playSeparationRequested(kind, path)
            }
        }

        // Action Controls & Run Buttons (Clean 2-row layout with zero horizontal overflow)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (Full width)
            PrimaryButton {
                id: runSeparateBtn
                text: (root.dubbing.vocalsPath || "").length > 0 && (root.dubbing.backgroundPath || "").length > 0
                      ? qsTr("⚡ Tách Lại Giọng & Nhạc Nền")
                      : qsTr("⚡ Chạy Tách Giọng Nói & Nhạc Nền (Isolator)")
                iconName: root.dubbing.processing ? "activity" : "play"
                loading: root.dubbing.processing
                enabled: !root.dubbing.processing && (root.dubbing.normalizedAudioPath || "").length > 0
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                buttonColor: Theme.accent
                onClicked: root.runRequested("source-separate")
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
                    visible: !((root.dubbing.vocalsPath || "").length > 0 && (root.dubbing.backgroundPath || "").length > 0)
                    text: qsTr("Bỏ qua bước này ➔")
                    iconName: "chevron-right"
                    quiet: true
                    enabled: !root.dubbing.processing
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }

                PrimaryButton {
                    visible: (root.dubbing.vocalsPath || "").length > 0 && (root.dubbing.backgroundPath || "").length > 0
                    text: qsTr("Tiếp tục: Nhận Dạng ➔")
                    iconName: "chevron-right"
                    buttonColor: Theme.accent
                    enabled: !root.dubbing.processing
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }
            }
        }
    }
}
