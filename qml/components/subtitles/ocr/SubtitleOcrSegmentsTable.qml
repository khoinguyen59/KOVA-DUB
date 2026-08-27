import QtQuick
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    property var ocr: AppController.subtitleOcr
    signal exportSrtRequested()

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
                text: qsTr("Danh sách phụ đề bóc tách (%1)").arg(root.ocr ? root.ocr.segmentCount : 0)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontMedium
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Xuất file .SRT")
                enabled: root.ocr && root.ocr.segmentCount > 0
                onClicked: root.exportSrtRequested()
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.ocr ? root.ocr.segments : []
            spacing: 6

            delegate: Rectangle {
                width: ListView.view.width
                height: 52
                color: Qt.rgba(1, 1, 1, 0.03)
                radius: Theme.radiusSmall
                border.color: Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    Text {
                        text: "#" + (index + 1)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                    }

                    Text {
                        text: modelData.startTime + " -> " + modelData.endTime
                        color: Theme.accentLight
                        font.pixelSize: Theme.fontSmall
                        font.family: "monospace"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.text || ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontMedium
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
