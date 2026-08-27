import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Drawer {
    id: root

    objectName: "dubbingContextDrawer"
    property string contextId: "results"
    default property alias contentData: contextHost.data

    edge: Qt.RightEdge
    modal: false
    interactive: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(520, Math.max(320, parent ? parent.width * 0.30 : 420))
    height: parent ? parent.height : 0
    padding: 0

    function openContext(value) {
        root.contextId = value || "results"
        root.open()
    }

    background: Rectangle {
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.surfaceAlt
            border.color: Qt.rgba(1, 1, 1, 0.08)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingSmall
                spacing: Theme.paddingSmall
                Text {
                    Layout.fillWidth: true
                    text: root.contextId === "settings" ? qsTr("Task settings")
                          : root.contextId === "model" ? qsTr("Model & runtime")
                          : root.contextId === "handoff" ? qsTr("Data & handoff")
                          : qsTr("Task results")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    elide: Text.ElideRight
                }
                PrimaryButton {
                    objectName: "dubbingContextDrawerClose"
                    iconName: "close"
                    iconOnly: true
                    quiet: true
                    toolTip: qsTr("Close task panel")
                    onClicked: root.close()
                }
            }
        }

        Item {
            id: contextHost
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }
    }
}
