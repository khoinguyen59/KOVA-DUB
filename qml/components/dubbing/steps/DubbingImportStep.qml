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
    implicitHeight: layout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    signal nextStepRequested()

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            LineIcon { name: "folder"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("1. NGUỒN MEDIA (IMPORT)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    text: qsTr("Chọn tệp Video hoặc Audio gốc cần lồng tiếng (hỗ trợ MP4, MKV, MOV, WAV, MP3).")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: fileInfoLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: (root.dubbing.sourceMediaPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.08) : Qt.rgba(1, 1, 1, 0.03)
            border.color: (root.dubbing.sourceMediaPath || "").length > 0 ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.30) : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1

            RowLayout {
                id: fileInfoLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                LineIcon {
                    name: (root.dubbing.sourceMediaPath || "").length > 0 ? "check" : "alert"
                    color: (root.dubbing.sourceMediaPath || "").length > 0 ? Theme.success : Theme.textSecondary
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                Text {
                    id: sourcePathText
                    Layout.fillWidth: true
                    text: (root.dubbing.sourceMediaPath || "").length > 0
                          ? root.dubbing.sourceMediaPath
                          : qsTr("Chưa chọn tệp nguồn. Vui lòng kéo thả hoặc nhấn nút thêm tệp ở khung xem trước.")
                    color: (root.dubbing.sourceMediaPath || "").length > 0 ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                    HoverHandler { id: sourcePathHover }
                    ToolTip.visible: sourcePathHover.hovered && (root.dubbing.sourceMediaPath || "").length > 0
                    ToolTip.text: root.dubbing.sourceMediaPath
                }
            }
        }

        // Action & Navigation row
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            PrimaryButton {
                text: qsTr("Tiếp tục: Bước 2 (Chuẩn Hóa Âm Thanh) ➔")
                iconName: "chevron-right"
                buttonColor: Theme.accent
                enabled: (root.dubbing.sourceMediaPath || "").length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: root.nextStepRequested()
            }
        }
    }
}
