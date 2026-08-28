import QtQuick
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal fixRequested()
    signal nextStepRequested()
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
            LineIcon { name: "globe"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("6. DỊCH THUẬT AI (TRANSLATE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            id: dubbingTranslationInputPanel
            objectName: "dubbingTranslationInputPanel"
            Layout.fillWidth: true
            implicitHeight: translationInputLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
            border.width: 1

            ColumnLayout {
                id: translationInputLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Nguồn đầu vào dịch:")
                    color: Theme.textPrimary
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: (root.dubbing.transcriptConfiguration.transcriptSource || "stt") === "reconcile"
                          ? qsTr("Dịch sử dụng nguồn STT + OCR đã qua duyệt. Mô hình dịch sẽ tự động canh chỉnh theo độ dài câu gốc.")
                          : qsTr("Dịch sử dụng bản ghi lời thoại từ nguồn đã chọn (STT hoặc OCR).")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.dubbing.translationFixCandidateCount > 0
                    spacing: Theme.paddingSmall

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Có %1 câu dịch vượt quá thời lượng âm tiết đề xuất.").arg(root.dubbing.translationFixCandidateCount)
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                    PrimaryButton {
                        text: qsTr("Tự động tinh chỉnh AI")
                        iconName: "spark"
                        quiet: true
                        loading: root.dubbing.translationFixing
                        onClicked: root.fixRequested()
                    }
                }
            }
        }

        // Warning banner if no segments are transcribed yet
        Rectangle {
            visible: root.dubbing.segments.length === 0
            Layout.fillWidth: true
            implicitHeight: noSegmentsCol.implicitHeight + 16
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.12)
            border.color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.40)
            border.width: 1

            RowLayout {
                id: noSegmentsCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                LineIcon { name: "alert-triangle"; color: Theme.warning; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Dự án chưa có phân đoạn lời thoại. Vui lòng chạy Bước 4 (Nhận dạng lời thoại & Phụ đề) trước khi bắt đầu dịch.")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Action Controls & Run Buttons (Clean 2-row layout with zero horizontal overflow)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (Full width)
            PrimaryButton {
                id: runTranslateBtn
                text: (root.dubbing.segments || []).length > 0 && root.dubbing.segments[0] && root.dubbing.segments[0].targetText
                      ? qsTr("⚡ Dịch Lại Toàn Bộ Lời Thoại (%1 phân đoạn)").arg(root.dubbing.segments.length)
                      : qsTr("⚡ Chạy Dịch Thuật AI (Translate)")
                iconName: root.dubbing.processing ? "activity" : "globe"
                loading: root.dubbing.processing
                enabled: !root.dubbing.processing && (root.dubbing.segments || []).length > 0
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                buttonColor: Theme.accent
                onClicked: root.dubbing.runWorkflowNode("translate")
            }

            // Row 2: Navigation Buttons (Quay lại & Tiếp tục)
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
                    text: qsTr("Tiếp tục: Duyệt Bản Dịch ➔")
                    iconName: "chevron-right"
                    buttonColor: Theme.accent
                    enabled: !root.dubbing.processing && (root.dubbing.segments || []).length > 0
                    Layout.preferredHeight: 38
                    Layout.fillWidth: true
                    onClicked: root.nextStepRequested()
                }
            }
        }
    }
}
