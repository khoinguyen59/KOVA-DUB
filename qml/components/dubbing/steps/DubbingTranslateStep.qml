import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool stepComplete: false

    signal openFixDialogRequested()

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
            LineIcon { name: "globe"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("6. DỊCH THUẬT AI (TRANSLATE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Dịch lời thoại sang ngôn ngữ đích có kiểm soát độ dài âm tiết (Phoneme Duration Budget) để khớp khẩu hình.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle {
            id: dubbingTranslationInputPanel
            objectName: "dubbingTranslationInputPanel"
            Layout.fillWidth: true
            implicitHeight: translationInputLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
            border.width: 1

            ColumnLayout {
                id: translationInputLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Nguồn đầu vào dịch:")
                    color: Theme.textPrimary
                    font.bold: true
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
                    }
                    PrimaryButton {
                        text: qsTr("Tự động tinh chỉnh AI")
                        iconName: "spark"
                        quiet: true
                        loading: root.dubbing.translationFixing
                        onClicked: root.openFixDialogRequested()
                    }
                }
            }
        }
    }
}
