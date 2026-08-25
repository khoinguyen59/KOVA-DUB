import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import ".."
import LAStudio

ColumnLayout {
    id: root

    required property var dubbing
    required property var sourceMediaPanel
    property bool timelineMinimized: false
    property real timelineHeight: 140
    property real minimumHeight: 72
    property real maximumHeight: 280

    signal segmentSelected(int index)

    Layout.fillWidth: true
    spacing: 0

    // Timeline Resize Handle
    Rectangle {
        id: dubbingTimelineResizeHandle
        objectName: "dubbingTimelineResizeHandle"
        Layout.fillWidth: true
        Layout.preferredHeight: 6
        color: timelineResizeMouse.containsMouse || timelineResizeMouse.drag.active
               ? Theme.accent
               : Qt.rgba(1, 1, 1, 0.08)

        MouseArea {
            id: timelineResizeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeVerCursor
            drag.target: Item {}
            drag.axis: Drag.YAxis
            property real startY: 0
            property real startH: 0

            onPressed: function(mouse) {
                startY = mouse.y
                startH = root.timelineHeight
            }

            onPositionChanged: function(mouse) {
                if (drag.active) {
                    var delta = startY - mouse.y
                    root.timelineHeight = Math.max(root.minimumHeight, Math.min(root.maximumHeight, startH + delta))
                }
            }
        }
    }

    // Timeline Panel
    Rectangle {
        id: dubbingTimelinePanel
        objectName: "dubbingTimelinePanel"
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        Layout.fillWidth: true
        Layout.preferredHeight: root.timelineMinimized ? 36 : root.timelineHeight
        Behavior on Layout.preferredHeight {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.timelineMinimized ? Theme.paddingSmall : Theme.paddingMedium
            spacing: Theme.paddingSmall

            // Timeline Header Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.paddingSmall

                LineIcon {
                    name: "activity"
                    color: Theme.accentLight
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                Text {
                    text: qsTr("DÒNG THỜI GIAN & SÓNG ÂM")
                    color: Theme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 0.5
                }
                Rectangle {
                    implicitWidth: segCountText.implicitWidth + 12
                    implicitHeight: 18
                    radius: 9
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3)
                    Text {
                        id: segCountText
                        anchors.centerIn: parent
                        text: qsTr("%1 phân đoạn").arg(root.dubbing.segments.length)
                        color: Theme.accentLight
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
                Item { Layout.fillWidth: true }

                PrimaryButton {
                    text: root.timelineMinimized ? qsTr("▲ Mở Rộng Sóng Âm") : qsTr("▼ Thu Nhỏ")
                    quiet: true
                    onClicked: root.timelineMinimized = !root.timelineMinimized
                }
            }

            // Expanded Waveform Area
            WaveformView {
                id: dubbingWaveformView
                objectName: "dubbingWaveformView"
                visible: !root.timelineMinimized
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(0, 0, 0, 0.25)
                waveColor: Theme.accent
                playedWaveColor: Theme.accentLight
                showPlaybackProgress: true
            }
        }
    }
}
