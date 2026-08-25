import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    property var queueItems: []

    signal cancelItemRequested(int index)
    signal openFolderRequested(string path)

    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: qsTr("Hàng đợi tải xuống (%1)").arg(root.queueItems.length)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.queueItems
            spacing: 6

            delegate: Rectangle {
                width: ListView.view.width
                height: 60
                color: Qt.rgba(1, 1, 1, 0.03)
                radius: Theme.radiusSmall
                border.color: Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            Layout.fillWidth: true
                            text: modelData.title || modelData.source || qsTr("Đang tải media...")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSmall
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        ProgressBar {
                            Layout.fillWidth: true
                            value: (modelData.progress || 0) / 100.0
                        }
                    }

                    Text {
                        text: (modelData.progress || 0) + "%"
                        color: Theme.accentLight
                        font.pixelSize: Theme.fontSmall
                    }

                    AppButton {
                        text: qsTr("✕")
                        onClicked: root.cancelItemRequested(index)
                    }
                }
            }
        }
    }
}
