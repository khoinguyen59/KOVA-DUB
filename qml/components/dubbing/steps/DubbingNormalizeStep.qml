import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

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
            LineIcon { name: "activity"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("2. CHUẨN HÓA ÂM THANH (NORMALIZE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Chuyển đổi âm thanh nguồn sang chuẩn PCM WAV 44.1kHz đồng bộ cho các bước tiếp theo.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: infoLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: root.dubbing.normalizedAudioPath.length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: root.dubbing.normalizedAudioPath.length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1

            RowLayout {
                id: infoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                LineIcon {
                    name: root.dubbing.normalizedAudioPath.length > 0 ? "check" : "refresh-cw"
                    color: root.dubbing.normalizedAudioPath.length > 0 ? Theme.success : Theme.textSecondary
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                Text {
                    Layout.fillWidth: true
                    text: root.dubbing.normalizedAudioPath.length > 0
                          ? qsTr("Tệp âm thanh chuẩn hóa: %1").arg(root.dubbing.normalizedAudioPath)
                          : qsTr("Chưa chuẩn hóa. Nhấn 'Chạy Bước Này' ở bảng điều khiển để tạo tệp âm thanh làm việc.")
                    color: root.dubbing.normalizedAudioPath.length > 0 ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
