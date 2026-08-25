import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal openSubtitleEditorRequested()
    signal continueRequested()

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
            LineIcon { name: "edit"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("7. DUYỆT PHỤ ĐỀ DỊCH (REVIEW TRANSLATION)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Kiểm tra lại toàn bộ câu dịch và phụ đề trước khi tiến hành lồng tiếng bằng TTS.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionsLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                id: actionsLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Bản dịch mục tiêu đã sẵn sàng. Bạn có thể mở Trình sửa phụ đề để trau chuốt từng từ ngữ.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        text: qsTr("Mở Trình Sửa Phụ Đề")
                        iconName: "edit"
                        enabled: !root.dubbing.processing && root.dubbing.segments.length > 0
                        onClicked: root.openSubtitleEditorRequested()
                    }
                    Item { Layout.fillWidth: true }
                    PrimaryButton {
                        text: qsTr("Tiếp tục sang Lồng Tiếng (TTS)")
                        iconName: "chevron-right"
                        enabled: !root.dubbing.processing && root.stepComplete
                        onClicked: root.continueRequested()
                    }
                }
            }
        }
    }
}
