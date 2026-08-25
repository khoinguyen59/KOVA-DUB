import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../base"
import LAStudio

Rectangle {
    id: root

    property var modelItem: null
    property bool open: modelItem !== null

    signal closeRequested()
    signal downloadRequested(var item)
    signal deleteRequested(var item)

    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    radius: Theme.radiusMedium

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: root.modelItem ? root.modelItem.name : qsTr("Chi Tiết Mô Hình")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            AppButton {
                text: qsTr("✕")
                onClicked: root.closeRequested()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: Theme.paddingMedium

                Text {
                    Layout.fillWidth: true
                    text: root.modelItem ? root.modelItem.description : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontMedium
                    wrapMode: Text.WordWrap
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }

                Text {
                    text: qsTr("Thông Số Kỹ Thuật:")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("• Kích thước tệp: %1").arg(root.modelItem ? root.modelItem.sizeString : "N/A")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    text: qsTr("• Runtime hỗ trợ: %1").arg(root.modelItem ? root.modelItem.runtime : "llama.cpp / ONNX")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    text: qsTr("• Bộ nhớ VRAM khuyến nghị: %1").arg(root.modelItem ? root.modelItem.vram : "4 GB+")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingMedium

            AppButton {
                Layout.fillWidth: true
                text: root.modelItem && root.modelItem.installed ? qsTr("Gỡ Cài Đặt") : qsTr("Tải & Cài Đặt Ngay")
                primary: root.modelItem ? !root.modelItem.installed : true
                onClicked: {
                    if (root.modelItem) {
                        if (root.modelItem.installed)
                            root.deleteRequested(root.modelItem)
                        else
                            root.downloadRequested(root.modelItem)
                    }
                }
            }
        }
    }
}
