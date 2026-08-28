pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import LAStudio
import "../base"

// History is optional navigation chrome. It must float above the editor so
// opening it never steals width from the video or the persistent right panel.
Popup {
    id: root

    required property var dubbing
    property int panelWidth: 360

    parent: Overlay.overlay
    modal: false
    focus: true
    padding: 0
    width: Math.min(Math.max(300, root.panelWidth), parent ? parent.width - Theme.paddingMedium * 2 : root.panelWidth)
    height: Math.min(560, parent ? parent.height - Theme.paddingXL * 2 : 560)
    x: Theme.paddingMedium
    y: Theme.paddingMedium
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation {
            property: "x"
            from: -root.width
            to: root.x
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "x"
            from: root.x
            to: -root.width
            duration: 140
            easing.type: Easing.InCubic
        }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.color: Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.42)
        border.width: 1
    }

    contentItem: DubbingHistoryPanel {
        id: historyPanel
        dubbing: root.dubbing
        panelWidth: root.width
        expanded: true
        anchors.fill: parent
        onClearRequested: root.clearRequested()
        onDeleteRequested: function(historyId) { root.deleteRequested(historyId) }
        onProjectOpened: root.close()
        onExpandedChanged: if (!expanded) root.close()
    }

    signal clearRequested()
    signal deleteRequested(string historyId)
}
