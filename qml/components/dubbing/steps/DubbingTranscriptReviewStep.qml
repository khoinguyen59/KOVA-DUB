import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal openTranscriptEditorRequested()
    signal openAlignmentStudioRequested()
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
            LineIcon { name: "check-circle"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("5. DUYỆT VĂN BẢN GỐC (REVIEW TRANSCRIPT)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Kiểm tra và hiệu chỉnh văn bản nguồn trước khi dịch để đảm bảo bản dịch chính xác nhất.")
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
                    text: root.dubbing.segments.length > 0
                          ? qsTr("Hiện có %1 phân đoạn lời thoại đã sẵn sàng duyệt.").arg(root.dubbing.segments.length)
                          : qsTr("Chưa có phân đoạn nào. Vui lòng chạy bước Nhận dạng (Transcribe) trước.")
                    color: root.dubbing.segments.length > 0 ? Theme.success : Theme.warning
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        text: qsTr("Mở Trình Sửa Phụ Đề")
                        iconName: "edit"
                        enabled: !root.dubbing.processing && root.dubbing.segments.length > 0
                        onClicked: root.openTranscriptEditorRequested()
                    }
                    PrimaryButton {
                        text: qsTr("Mở Alignment Studio")
                        iconName: "alignment"
                        quiet: true
                        enabled: !root.dubbing.processing && root.dubbing.normalizedAudioPath !== "" && root.dubbing.segments.length > 0
                        onClicked: root.openAlignmentStudioRequested()
                    }
                    Item { Layout.fillWidth: true }
                    PrimaryButton {
                        text: qsTr("Tiếp tục sang Dịch thuật")
                        iconName: "chevron-right"
                        enabled: !root.dubbing.processing && root.stepComplete
                        onClicked: root.continueRequested()
                    }
                }
            }
        }
    }
}
