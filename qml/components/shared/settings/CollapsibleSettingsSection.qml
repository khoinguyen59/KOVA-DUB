import QtQuick
import QtQuick.Layouts
import LAStudio
import "../../base"

Rectangle {
    id: root

    property string title: ""
    property string iconName: "sliders"
    property bool expanded: false
    default property alias content: contentColumn.data

    signal toggled()

    Layout.fillWidth: true
    implicitHeight: mainCol.implicitHeight + Theme.paddingMedium * 2
    radius: 10
    color: "#1d1b2c"
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    clip: true

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        Rectangle {
            id: headerArea
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight + 4
            radius: 6
            color: headerMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: Theme.paddingSmall

                LineIcon {
                    name: root.iconName
                    color: Theme.accentLight
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }

                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    Layout.fillWidth: true
                }

                LineIcon {
                    name: "chevron-right"
                    color: Theme.textSecondary
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    rotation: root.expanded ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
                }
            }

            MouseArea {
                id: headerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.expanded = !root.expanded
                    root.toggled()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.06)
            visible: root.expanded
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            visible: root.expanded
            opacity: root.expanded ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 180 } }
        }
    }
}

