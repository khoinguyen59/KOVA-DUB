import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    property var ocr: AppController.subtitleOcr
    property var runtime: AppController.subtitleOcrRuntime

    signal startOcrRequested()
    signal cancelOcrRequested()

    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Theme.border
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingMedium

        Text {
            text: qsTr("Cấu hình nhận diện Subtitle OCR")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontMedium
            font.weight: Font.Bold
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // Route selector
        Text { text: qsTr("Môi trường xử lý:"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall
            AppButton {
                text: qsTr("Local CPU (Tesseract / Paddle)")
                checkable: true
                checked: root.ocr ? root.ocr.executionRoute !== "colab-gpu" : true
                onClicked: if (root.ocr) root.ocr.executionRoute = "local-cpu"
            }
            AppButton {
                text: qsTr("Colab GPU (PaddleOCR v4)")
                checkable: true
                checked: root.ocr ? root.ocr.executionRoute === "colab-gpu" : false
                onClicked: if (root.ocr) root.ocr.executionRoute = "colab-gpu"
            }
        }

        // Language selector
        Text { text: qsTr("Ngôn ngữ chữ gốc trong video:"); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
        ComboBox {
            Layout.fillWidth: true
            model: [
                { text: qsTr("Tiếng Trung (Giản thể / Phồn thể)"), code: "zh" },
                { text: qsTr("Tiếng Anh"), code: "en" },
                { text: qsTr("Tiếng Việt"), code: "vi" },
                { text: qsTr("Tiếng Nhật"), code: "ja" },
                { text: qsTr("Tiếng Hàn"), code: "ko" }
            ]
            textRole: "text"
            valueRole: "code"
            onActivated: function(index) {
                if (root.ocr) root.ocr.ocrLanguage = model[index].code
            }
        }

        Item { Layout.fillHeight: true }

        // Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            AppButton {
                Layout.fillWidth: true
                text: root.ocr && root.ocr.processing ? qsTr("Hủy Quét") : qsTr("Bắt Đầu Bóc Tách OCR")
                primary: true
                onClicked: {
                    if (root.ocr && root.ocr.processing)
                        root.cancelOcrRequested()
                    else
                        root.startOcrRequested()
                }
            }
        }
    }
}
