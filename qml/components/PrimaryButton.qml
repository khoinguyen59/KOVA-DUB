import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LAStudio
import "base"

Button {
    id: root

    property color buttonColor: Theme.accent
    property color textColor: "#ffffff"
    property bool loading: false
    property string iconName: ""
    property bool iconOnly: false
    property string toolTip: ""
    property string accessibleName: ""
    property color borderColor: Qt.rgba(1, 1, 1, 0.08)
    property bool quiet: false
    readonly property real requiredContentWidth:
        root.iconOnly ? 38
                      : contentRow.implicitWidth + Theme.paddingMedium * 2

    implicitWidth: root.iconOnly ? 38 : Math.max(100, root.requiredContentWidth)
    implicitHeight: 38
    Layout.minimumWidth: root.requiredContentWidth

    Accessible.role: Accessible.Button
    Accessible.name: root.accessibleName !== "" ? root.accessibleName : root.text
    Accessible.description: root.iconOnly ? root.toolTip : ""

    AppToolTip {
        text: root.toolTip
        visible: root.hovered && root.toolTip !== ""
    }

    contentItem: Item {
        opacity: root.loading ? 0 : 1

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            spacing: 6
            visible: !root.loading

            LineIcon {
                id: buttonIcon
                visible: root.iconName !== ""
                name: root.iconName
                color: root.enabled ? root.textColor : Theme.textSecondary
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
            }

            Text {
                id: buttonLabel
                visible: !root.iconOnly
                text: root.text
                color: root.enabled ? root.textColor : Theme.textSecondary
                font.pixelSize: Theme.fontSmall
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Layout.maximumWidth: root.width - (buttonIcon.visible ? 36 : 24)
            }
        }
    }

    background: Rectangle {
        implicitWidth: root.iconOnly ? 38 : 100
        implicitHeight: 38
        radius: 8
        
        gradient: (!root.quiet && root.enabled) ? primaryGradient : null
        color: {
            if (!root.enabled) return Theme.surfaceAlt
            if (root.quiet) {
                if (root.pressed) return Qt.rgba(1, 1, 1, 0.12)
                if (root.hovered) return Qt.rgba(1, 1, 1, 0.08)
                return Qt.rgba(1, 1, 1, 0.04)
            }
            return root.buttonColor
        }

        Gradient {
            id: primaryGradient
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: root.pressed ? Qt.darker(root.buttonColor, 1.15) : (root.hovered ? Qt.lighter(root.buttonColor, 1.12) : root.buttonColor)
            }
            GradientStop {
                position: 1.0
                color: root.pressed ? Qt.darker(root.buttonColor, 1.25) : (root.hovered ? root.buttonColor : Qt.darker(root.buttonColor, 1.15))
            }
        }

        border.color: root.enabled ? (root.quiet ? root.borderColor : Qt.rgba(255, 255, 255, 0.15)) : "transparent"
        border.width: 1

        // Loading spinner
        BusyIndicator {
            anchors.centerIn: parent
            running: root.loading
            visible: root.loading
            width: 22
            height: 22
            palette.dark: root.textColor
        }
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }
}


