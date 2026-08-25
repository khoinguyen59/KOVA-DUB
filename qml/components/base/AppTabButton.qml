import QtQuick
import QtQuick.Layouts
import LAStudio

Rectangle {
    id: root

    property string text: "Tab"
    property string icon: ""
    property string iconName: ""
    property bool selected: false
    property bool enabled: true

    signal clicked()

    implicitWidth: Math.max(76, contentRow.implicitWidth + Theme.paddingMedium * 2)
    implicitHeight: 34
    radius: 7
    color: {
        if (selected) return Qt.rgba(0.49, 0.30, 1.0, 0.22)
        if (hoverHandler.hovered && root.enabled) return Qt.rgba(1, 1, 1, 0.06)
        return "transparent"
    }
    border.color: selected ? Qt.rgba(0.64, 0.49, 1.0, 0.65) : Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    opacity: enabled ? 1.0 : 0.55

    RowLayout {
        id: contentRow
        anchors.centerIn: parent
        spacing: 6

        LineIcon {
            id: tabIcon
            name: root.iconName
            color: root.selected ? Theme.accentLight : (hoverHandler.hovered ? Theme.textPrimary : Theme.textSecondary)
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            visible: root.iconName !== ""
        }

        Text {
            id: tabEmoji
            text: root.icon
            font.pixelSize: Theme.fontMedium
            visible: root.icon !== "" && root.iconName === ""
        }

        Text {
            id: tabText
            text: root.text
            color: selected ? "#ffffff" : (hoverHandler.hovered ? Theme.textPrimary : Theme.textSecondary)
            font.pixelSize: Theme.fontSmall
            font.bold: selected
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: {
            if (root.enabled) {
                root.clicked()
            }
        }
    }

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }
}
