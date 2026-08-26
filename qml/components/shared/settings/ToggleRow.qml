import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio

CheckBox {
    id: toggle

    // Keep toggles discoverable on keyboard-only and high-DPI workflows.  A
    // description is optional, but when supplied it makes a setting's effect
    // explicit instead of presenting a bare, ambiguous label.
    property string description: ""

    Layout.fillWidth: true
    implicitHeight: Math.max(36, content.implicitHeight + Theme.paddingSmall)
    focusPolicy: Qt.StrongFocus

    leftPadding: 0
    rightPadding: 48 // 38px indicator width + 10px buffer to prevent text overlap
    topPadding: Math.round(Theme.paddingSmall / 2)
    bottomPadding: Math.round(Theme.paddingSmall / 2)

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 20
        x: toggle.width - width
        y: Math.round((toggle.height - height) / 2)
        radius: 10
        color: toggle.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.22) : Qt.rgba(1, 1, 1, 0.04)
        border.color: toggle.checked ? Qt.rgba(0.49, 0.30, 1.0, 0.85) : Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Rectangle {
            width: 14
            height: 14
            radius: 7
            x: toggle.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            color: toggle.checked ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.42)

            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }

    contentItem: Column {
        id: content
        spacing: 2

        Text {
            width: content.width
            text: toggle.text
            color: toggle.enabled ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            width: content.width
            visible: toggle.description !== ""
            text: toggle.description
            color: Theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
