import QtQuick
import QtQuick.Layouts
import LAStudio
import "../../base"

Rectangle {
    id: root
    property string title: ""
    property string iconName: "sliders"
    default property alias content: contentLayout.data

    Layout.fillWidth: true
    implicitHeight: mainColumn.implicitHeight + Theme.paddingMedium * 2
    radius: 10
    color: "#1d1b2c"
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: mainColumn
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            id: headerRow
            Layout.fillWidth: true
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
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(1, 1, 1, 0.06)
            visible: contentLayout.children.length > 0
        }

        ColumnLayout {
            id: contentLayout
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
        }
    }
}

