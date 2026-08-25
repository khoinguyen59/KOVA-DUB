import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    property var models: []
    property var selectedModel: null

    signal modelClicked(var modelItem)
    signal downloadClicked(var modelItem)

    color: "transparent"

    GridView {
        id: gridView
        anchors.fill: parent
        cellWidth: 320
        cellHeight: 180
        clip: true
        model: root.models

        delegate: Rectangle {
            width: gridView.cellWidth - 16
            height: gridView.cellHeight - 16
            color: root.selectedModel === modelData ? Qt.rgba(0.2, 0.6, 1.0, 0.12) : Theme.surface
            radius: Theme.radiusMedium
            border.color: root.selectedModel === modelData ? Theme.accent : Theme.border
            border.width: root.selectedModel === modelData ? 2 : 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name || qsTr("Model AI")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        implicitWidth: statusTxt.implicitWidth + 14
                        height: 22
                        radius: 11
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                        Text {
                            id: statusTxt
                            anchors.centerIn: parent
                            text: qsTr("Cloud GPU")
                            color: Theme.accentLight
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: modelData.description || ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Colab GPU Worker")
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSmall
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Chi Tiết")
                        primary: true
                        onClicked: root.modelClicked(modelData)
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: root.modelClicked(modelData)
            }
        }
    }
}
