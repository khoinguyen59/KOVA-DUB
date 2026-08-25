import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal openExportDialogRequested()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingLarge * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingLarge
        spacing: Theme.paddingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon { name: "share"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("10. XUẤT BẢN THÀNH PHẨM (EXPORT)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Ghép hoàn chỉnh Video + Giọng Lồng Tiếng + Nhạc Nền + Phụ Đề (Hardsub/Softsub) hoặc xuất Project CapCut.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: exportInfoLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: (root.dubbing.exportPath.length > 0 || root.dubbing.previewPath.length > 0) ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: (root.dubbing.exportPath.length > 0 || root.dubbing.previewPath.length > 0) ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                id: exportInfoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    LineIcon {
                        name: root.dubbing.exportPath.length > 0 ? "check-circle" : "film"
                        color: root.dubbing.exportPath.length > 0 ? Theme.success : Theme.accentLight
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.dubbing.exportPath.length > 0
                              ? qsTr("Tệp thành phẩm: %1").arg(root.dubbing.exportPath)
                              : (root.dubbing.previewPath.length > 0 ? qsTr("Tệp xem trước: %1").arg(root.dubbing.previewPath) : qsTr("Chưa xuất bản video thành phẩm."))
                        color: root.dubbing.exportPath.length > 0 ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    PrimaryButton {
                        text: qsTr("Cấu hình Xuất Video / Dự Án")
                        iconName: "download"
                        enabled: !root.dubbing.processing && root.dubbing.sourceMediaPath.length > 0
                        onClicked: root.openExportDialogRequested()
                    }
                }
            }
        }
    }
}
