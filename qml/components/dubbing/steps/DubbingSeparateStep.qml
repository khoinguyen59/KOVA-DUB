import QtQuick
import QtQuick.Controls
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

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingLarge * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

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
                }
                Text {
                    text: qsTr("Tách riêng lời thoại gốc (Vocals) để nhận dạng và giữ lại nhạc nền (BGM) để ghép thành phẩm.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        VoiceSeparationOutput {
            Layout.fillWidth: true
            compact: false
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
    }
}
