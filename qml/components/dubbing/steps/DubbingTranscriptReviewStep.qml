import QtQuick
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
    signal previousStepRequested()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + Theme.paddingMedium * 2
    radius: Theme.radiusMedium
    color: Qt.rgba(Theme.surfaceLevel2.r, Theme.surfaceLevel2.g, Theme.surfaceLevel2.b, 0.60)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

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
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionsLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            ColumnLayout {
                id: actionsLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                Text {
                    Layout.fillWidth: true
                    text: (root.dubbing.segments || []).length > 0
                          ? qsTr("Hiện có %1 phân đoạn lời thoại đã sẵn sàng duyệt.").arg((root.dubbing.segments || []).length)
                          : qsTr("Chưa có phân đoạn nào. Vui lòng chạy bước Nhận dạng (Transcribe) trước.")
                    color: (root.dubbing.segments || []).length > 0 ? Theme.success : Theme.warning
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                // Main Tool Actions
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        text: qsTr("Mở Trình Sửa Lời Thoại")
                        iconName: "edit"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        enabled: !root.dubbing.processing && (root.dubbing.segments || []).length > 0
                        onClicked: root.openTranscriptEditorRequested()
                    }

                    PrimaryButton {
                        text: qsTr("Alignment Studio")
                        iconName: "alignment"
                        quiet: true
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 38
                        enabled: !root.dubbing.processing && (root.dubbing.normalizedAudioPath || "") !== "" && (root.dubbing.segments || []).length > 0
                        onClicked: root.openAlignmentStudioRequested()
                    }
                }

                // Navigation Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        text: qsTr("⬅ Quay lại")
                        iconName: "chevron-left"
                        quiet: true
                        Layout.preferredHeight: 38
                        Layout.preferredWidth: 100
                        onClicked: root.previousStepRequested()
                    }

                    PrimaryButton {
                        text: qsTr("Tiếp tục sang Dịch thuật ➔")
                        iconName: "chevron-right"
                        buttonColor: Theme.accent
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        enabled: !root.dubbing.processing && (root.dubbing.segments || []).length > 0
                        onClicked: root.continueRequested()
                    }
                }
            }
        }
    }
}
