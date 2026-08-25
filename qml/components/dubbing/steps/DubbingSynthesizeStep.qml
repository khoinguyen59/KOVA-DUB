import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
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
            LineIcon { name: "volume-2"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("8. LỒNG TIẾNG TTS (SYNTHESIZE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Tạo giọng đọc AI hoặc Clone Voice cho từng phân đoạn thoại với tốc độ khớp chính xác thời lượng.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        DubbingVoiceClipReview {
            id: voiceClipReview
            dubbing: root.dubbing
            sourceMediaPanel: root.sourceMediaPanel
            playingVoiceClipPath: root.playingVoiceClipPath
            generatedClipCount: root.generatedClipCount
            synthesisComplete: root.synthesisComplete
            onVoiceClipPlaybackRequested: root.voiceClipPlaybackRequested(path)
            onSeparationPlaybackStopped: root.separationPlaybackStopped()
        }
    }
}
