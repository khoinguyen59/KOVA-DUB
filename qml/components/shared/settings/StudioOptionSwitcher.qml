import QtQuick
import QtQuick.Layouts
import LAStudio
import "../../base"

Rectangle {
    id: root

    property var options: [] // Array of { id: string, label: string, icon: string }
    property string activeId: ""
    signal optionSelected(string id)

    implicitHeight: 40
    Layout.fillWidth: true
    radius: 10
    color: "#161524"
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 4

        Repeater {
            model: root.options

            delegate: Rectangle {
                id: optionBtn
                readonly property bool isSelected: root.activeId === modelData.id
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 7
                
                gradient: isSelected ? activeGradient : null
                color: isSelected ? "transparent" : (optMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                border.color: isSelected ? Qt.rgba(255, 255, 255, 0.15) : "transparent"
                border.width: isSelected ? 1 : 0

                Gradient {
                    id: activeGradient
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#8d5fff" }
                    GradientStop { position: 1.0; color: "#6b39e8" }
                }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    LineIcon {
                        visible: modelData.icon !== undefined && modelData.icon !== ""
                        name: modelData.icon || ""
                        color: optionBtn.isSelected ? "#ffffff" : (optMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                    }

                    Text {
                        text: modelData.label
                        color: optionBtn.isSelected ? "#ffffff" : (optMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                        font.pixelSize: Theme.fontSmall
                        font.bold: optionBtn.isSelected
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: optMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.activeId = modelData.id
                        root.optionSelected(modelData.id)
                    }
                }

                Behavior on color { ColorAnimation { duration: 150 } }
            }
        }
    }
}
