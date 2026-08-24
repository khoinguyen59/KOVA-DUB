import QtQuick
import QtQuick.Layouts
import "../base"
import LAStudio

Item {
    id: root

    required property string stepId
    required property string title
    property string iconName: "check"
    property bool complete: false
    property bool active: false
    signal selected(string stepId)

    implicitWidth: stepRow.implicitWidth + Theme.paddingSmall * 2
    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: root.active ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
               : (stepHover.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent")
        border.color: root.active ? Qt.rgba(Theme.accentLight.r, Theme.accentLight.g, Theme.accentLight.b, 0.45)
                      : (root.complete ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35) : "transparent")
        border.width: 1

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    RowLayout {
        id: stepRow
        anchors.fill: parent
        anchors.leftMargin: Theme.paddingSmall
        anchors.rightMargin: Theme.paddingSmall
        spacing: 8
        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: 12
            color: root.active ? Theme.accent : (root.complete ? Theme.success : Qt.rgba(1, 1, 1, 0.08))
            border.color: root.complete ? Theme.success : (root.active ? Theme.accentLight : Qt.rgba(1, 1, 1, 0.18))
            border.width: 1
            LineIcon {
                anchors.centerIn: parent
                width: 12
                height: 12
                name: root.complete ? "check" : root.iconName
                color: root.complete || root.active ? "#ffffff" : Theme.textSecondary
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.title
            color: root.active ? Theme.textPrimary : (stepHover.hovered ? "#ffffff" : Theme.textSecondary)
            font.pixelSize: Theme.fontSmall
            font.bold: root.active || root.complete
            elide: Text.ElideRight
        }
    }

    TapHandler { onTapped: root.selected(root.stepId) }
    HoverHandler { id: stepHover; cursorShape: Qt.PointingHandCursor }
}

