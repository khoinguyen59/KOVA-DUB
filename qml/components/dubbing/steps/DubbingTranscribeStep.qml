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
            LineIcon { name: "mic"; color: Theme.accentLight; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: qsTr("4. NHẬN DẠNG LỜI THOẠI & PHỤ ĐỀ (TRANSCRIBE)")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontMedium
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            id: dubbingTranscriptSourceDetailsPanel
            objectName: "dubbingTranscriptSourceDetailsPanel"
            Layout.fillWidth: true
            implicitHeight: transcriptSourceLayout.implicitHeight + Theme.paddingSmall * 2
            radius: Theme.radiusSmall
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
            border.width: 1

            ColumnLayout {
                id: transcriptSourceLayout
                anchors.fill: parent
                anchors.margins: Theme.paddingSmall
                spacing: Theme.paddingSmall

                Text {
                    text: qsTr("Nguồn nhận dạng (Transcript source):")
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                ComboBox {
                    id: dubbingTranscriptSourceModeDetails
                    objectName: "dubbingTranscriptSourceModeDetails"
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "id"
                    model: [
                        { id: "stt", label: qsTr("Chỉ STT (Nhận dạng âm thanh thoại)") },
                        { id: "ocr", label: qsTr("Chỉ OCR (Quét chữ từ phụ đề cứng)") },
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

                Text {
                    text: qsTr("Chính sách khi có xung đột STT/OCR:")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                ComboBox {
                    id: dubbingFusionPolicyMode
                    Layout.fillWidth: true
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

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.dubbing.unresolvedTranscriptConflictCount > 0
                    spacing: Theme.paddingSmall

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Có %1 xung đột STT/OCR cần duyệt trước khi chuyển sang bước Dịch.")
                              .arg(root.dubbing.unresolvedTranscriptConflictCount)
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        PrimaryButton {
                            Layout.fillWidth: true
                            text: qsTr("Dùng STT")
                            quiet: true
                            enabled: !root.dubbing.processing
                            onClicked: root.dubbing.resolveAllTranscriptConflicts("stt")
                        }
                        PrimaryButton {
                            Layout.fillWidth: true
                            text: qsTr("Dùng OCR")
                            quiet: true
                            enabled: !root.dubbing.processing
                            onClicked: root.dubbing.resolveAllTranscriptConflicts("ocr")
                        }
                        PrimaryButton {
                            readonly property var aiAvailability: root.dubbing.transcriptConflictAiAvailability()
                            Layout.fillWidth: true
                            text: qsTr("AI Gợi ý")
                            quiet: true
                            enabled: !root.dubbing.processing && aiAvailability.available
                                     && root.dubbing.unresolvedTranscriptConflictCount > 0
                            onClicked: root.dubbing.requestTranscriptConflictAiSuggestion(-1)
                        }
                    }
                }

                Text {
                    text: qsTr("Cấu hình OCR:")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    ComboBox {
                        id: dubbingOcrRouteMode
                        Layout.fillWidth: true
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
                    PrimaryButton {
                        Layout.preferredWidth: 120
                        text: (root.dubbing.transcriptConfiguration.ocrExecutionRoute || "local-cpu") === "colab-gpu"
                              ? qsTr("Colab GPU") : qsTr("Cài đặt Colab")
                        iconName: "cloud"
                        quiet: true
                        enabled: root.ocrSetupEditable
                        onClicked: root.openOcrColabSetupRequested()
                    }
                }

                ComboBox {
                    id: dubbingOcrModelMode
                    objectName: "dubbingOcrModelMode"
                    Layout.fillWidth: true
                    textRole: "displayName"
                    valueRole: "modelId"
                    visible: (root.dubbing.transcriptConfiguration.ocrExecutionRoute || "local-cpu") === "colab-gpu"
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
            }
        }

        // Action Controls & Run Buttons (Clean 2-row layout with zero horizontal overflow)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.paddingSmall

            // Row 1: Primary Action Button (Full width)
            PrimaryButton {
                id: runTranscribeBtn
                text: (root.dubbing.segments || []).length > 0
                      ? qsTr("⚡ Nhận Dạng Lại Lời Thoại (STT/OCR)")
                      : qsTr("⚡ Chạy Nhận Dạng Lời Thoại & Phụ Đề (STT)")
                iconName: root.dubbing.processing ? "activity" : "play"
                loading: root.dubbing.processing
                enabled: !root.dubbing.processing && ((root.dubbing.vocalsPath || "").length > 0 || (root.dubbing.normalizedAudioPath || "").length > 0 || (root.dubbing.sourceMediaPath || "").length > 0)
                Layout.preferredHeight: 40
                Layout.fillWidth: true
                buttonColor: Theme.accent
                onClicked: root.dubbing.runWorkflowNode("transcribe")
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
                    text: qsTr("Tiếp tục: Duyệt Lời Thoại ➔")
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
