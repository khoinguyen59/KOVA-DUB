import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    property bool ocrSetupEditable: true
    property bool stepComplete: false

    signal openOcrColabSetupRequested()

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
            LineIcon { name: "mic"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("4. NHẬN DẠNG LỜI THOẠI & PHỤ ĐỀ (TRANSCRIBE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                }
                Text {
                    text: qsTr("Chuyển đổi âm thanh thoại sang văn bản có mốc thời gian bằng STT và/hoặc quét phụ đề cứng Subtitle OCR.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Rectangle {
            id: dubbingTranscriptSourceDetailsPanel
            objectName: "dubbingTranscriptSourceDetailsPanel"
            Layout.fillWidth: true
            implicitHeight: transcriptSourceLayout.implicitHeight + Theme.paddingMedium * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
            border.width: 1

            ColumnLayout {
                id: transcriptSourceLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingMedium
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Nguồn nhận dạng (Transcript source)")
                    color: Theme.textPrimary
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: dubbingTranscriptSourceModeDetails
                        objectName: "dubbingTranscriptSourceModeDetails"
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "id"
                        model: [
                            { id: "stt", label: qsTr("Chỉ STT (Nhận dạng âm thanh)") },
                            { id: "ocr", label: qsTr("Chỉ OCR (Quét chữ phụ đề)") },
                            { id: "reconcile", label: qsTr("Khớp STT + OCR (Tự động hợp nhất)") }
                        ]
                        currentIndex: {
                            var source = root.dubbing.transcriptConfiguration.transcriptSource || "stt"
                            if (source === "stt+ocr") source = "reconcile"
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === source) return i
                            return 0
                        }
                        enabled: root.ocrSetupEditable
                        onActivated: function(index) {
                            root.dubbing.setWorkflowNodeParameters("transcribe", {
                                transcriptSource: model[index].id
                            })
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Chính sách xung đột:")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                    }
                    ComboBox {
                        id: dubbingFusionPolicyMode
                        Layout.preferredWidth: 210
                        textRole: "label"
                        valueRole: "id"
                        model: [
                            { id: "ask", label: qsTr("Hỏi khi xung đột") },
                            { id: "prefer-stt", label: qsTr("Ưu tiên STT") },
                            { id: "prefer-ocr", label: qsTr("Ưu tiên OCR") },
                            { id: "ai-suggest", label: qsTr("AI gợi ý") }
                        ]
                        currentIndex: {
                            var policy = root.dubbing.transcriptConfiguration.fusionPolicy || "ask"
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === policy) return i
                            return 0
                        }
                        enabled: root.ocrSetupEditable
                        onActivated: function(index) {
                            root.dubbing.setTranscriptFusionPolicy(model[index].id)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.dubbing.unresolvedTranscriptConflictCount > 0
                    spacing: Theme.paddingSmall
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Có %1 xung đột STT/OCR cần duyệt trước khi chuyển sang bước Dịch.")
                              .arg(root.dubbing.unresolvedTranscriptConflictCount)
                        color: Theme.warning
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    PrimaryButton {
                        text: qsTr("Dùng STT")
                        quiet: true
                        enabled: !root.dubbing.processing
                        onClicked: root.dubbing.resolveAllTranscriptConflicts("stt")
                    }
                    PrimaryButton {
                        text: qsTr("Dùng OCR")
                        quiet: true
                        enabled: !root.dubbing.processing
                        onClicked: root.dubbing.resolveAllTranscriptConflicts("ocr")
                    }
                    PrimaryButton {
                        readonly property var aiAvailability: root.dubbing.transcriptConflictAiAvailability()
                        text: qsTr("AI Gợi ý")
                        quiet: true
                        enabled: !root.dubbing.processing && aiAvailability.available
                                 && root.dubbing.unresolvedTranscriptConflictCount > 0
                        onClicked: root.dubbing.requestTranscriptConflictAiSuggestion(-1)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    Text {
                        text: qsTr("Cấu hình OCR:")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSmall
                    }
                    ComboBox {
                        id: dubbingOcrRouteMode
                        Layout.preferredWidth: 140
                        textRole: "label"
                        model: [
                            { id: "local-cpu", label: qsTr("Local CPU") },
                            { id: "colab-gpu", label: qsTr("Colab GPU") }
                        ]
                        currentIndex: (root.dubbing.transcriptConfiguration.ocrExecutionRoute || "local-cpu") === "colab-gpu" ? 1 : 0
                        enabled: root.ocrSetupEditable
                        onActivated: function(index) {
                            if (model[index].id === "colab-gpu")
                                root.openOcrColabSetupRequested()
                            else
                                root.dubbing.setWorkflowNodeParameters("transcribe", {
                                    "ocrExecutionRoute": "local-cpu"
                                })
                        }
                    }
                    ComboBox {
                        id: dubbingOcrModelMode
                        objectName: "dubbingOcrModelMode"
                        Layout.fillWidth: true
                        textRole: "displayName"
                        valueRole: "modelId"
                        model: root.dubbing.colabModelOptionsForNode("subtitle-ocr")
                        currentIndex: {
                            var selected = root.dubbing.transcriptConfiguration.ocrColabModelId
                                           || root.dubbing.defaultColabModelForNode("subtitle-ocr")
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].modelId === selected) return i
                            return 0
                        }
                        enabled: root.ocrSetupEditable
                        onActivated: function(index) {
                            if (model[index] && model[index].modelId)
                                root.dubbing.selectWorkflowColabModel("subtitle-ocr", model[index].modelId)
                        }
                    }
                    PrimaryButton {
                        text: (root.dubbing.transcriptConfiguration.ocrExecutionRoute || "local-cpu") === "colab-gpu"
                              ? qsTr("Colab OCR GPU") : qsTr("Cài đặt Colab")
                        iconName: "cloud"
                        quiet: true
                        enabled: root.ocrSetupEditable
                        onClicked: root.openOcrColabSetupRequested()
                    }
                }
            }
        }
    }
}
