import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../base"
import ".."
import "../steps"
import LAStudio

Rectangle {
    id: root

    required property var dubbing
    required property string displayedStepId
    property string actionNodeId: displayedStepId
    required property var workflowNode
    required property string stepTitle
    required property bool canRunStep
    required property bool canRerunStep
    required property bool stepRunReady
    required property string nextNodeId
    required property bool nextNodeReady
    required property var sourceMediaPanel
    property int selectedSegment: -1
    property int activeTab: 0
    onDisplayedStepIdChanged: activeTab = 0
    property bool ocrSetupEditable: true
    property string playingSeparationStem: ""
    property string playingVoiceClipPath: ""
    property int generatedClipCount: 0
    property bool synthesisComplete: false
    property var artifactSpecs: root.dubbing
                                  ? root.dubbing.workflowArtifactSpecsForStage(root.actionNodeId)
                                  : []
    property var acceptedArtifactNodeIds: []
    onActionNodeIdChanged: acceptedArtifactNodeIds = []
    onArtifactSpecsChanged: acceptedArtifactNodeIds = []

    signal configureNodeRequested(string nodeId)
    signal runStepRequested(string nodeId)
    signal runNextStepRequested(string nodeId)
    signal nextStepRequested(string stepId)
    signal previousStepRequested(string stepId)
    signal fixRequested()
    signal fixSegmentRequested(int index)
    signal artifactUploadRequested(string nodeId)
    signal artifactAccepted(string nodeId)
    signal artifactSkipRequested(string nodeId)
    signal openColabSetupRequested(string nodeId)
    signal openOcrColabSetupRequested()
    signal openTranscriptEditorRequested()
    signal openSubtitleEditorRequested()
    signal openAlignmentStudioRequested()
    signal openExportDialogRequested()
    signal playSeparationRequested(string kind, string path)
    signal voiceClipPlaybackRequested(string path)
    signal separationPlaybackStopped()
    signal voiceModelRequested(string nodeId)
    signal segmentSelected(int index)

    function handleArtifactAccepted(nodeId) {
        var accepted = root.acceptedArtifactNodeIds.slice()
        var id = String(nodeId || "")
        if (id !== "" && accepted.indexOf(id) < 0)
            accepted.push(id)
        root.acceptedArtifactNodeIds = accepted
        if (accepted.length >= root.artifactSpecs.length)
            root.artifactAccepted(id)
    }

    function skipArtifactTask() {
        if (root.dubbing && root.dubbing.skipWorkflowTask(root.actionNodeId))
            root.artifactSkipRequested(root.actionNodeId)
    }

    component SegmentTextArea: AppTextArea {
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        wrapMode: Text.Wrap
        padding: Theme.paddingSmall
        Layout.fillWidth: true
        Layout.minimumHeight: 30
        Layout.preferredHeight: Math.max(30, contentHeight + padding * 2)
        implicitHeight: Math.max(30, contentHeight + padding * 2)
    }

    objectName: "dubbingStepReviewPanel"
    color: Theme.surface
    radius: Theme.radiusMedium
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.paddingMedium
        spacing: Theme.paddingSmall

        // Segmented Tab Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: Theme.radiusSmall
            color: Qt.rgba(0, 0, 0, 0.25)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 2
                spacing: 2

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 1
                    color: root.activeTab === 0
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                           : (tabHover0.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                    border.color: root.activeTab === 0 ? Theme.accentLight : "transparent"
                    border.width: 1
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5
                        LineIcon { name: "edit"; color: root.activeTab === 0 ? Theme.accentLight : Theme.textSecondary; Layout.preferredWidth: 13; Layout.preferredHeight: 13 }
                        Text {
                            text: (root.displayedStepId === "transcribe" || root.displayedStepId === "translate")
                                  ? qsTr("Phân Đoạn (%1)").arg(root.dubbing.segments.length)
                                  : qsTr("Kết Quả Bước")
                            color: root.activeTab === 0 ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 10
                            font.bold: root.activeTab === 0
                        }
                    }
                    HoverHandler { id: tabHover0 }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = 0
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 1
                    color: root.activeTab === 1
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                           : (tabHover1.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                    border.color: root.activeTab === 1 ? Theme.accentLight : "transparent"
                    border.width: 1
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5
                        LineIcon { name: "settings"; color: root.activeTab === 1 ? Theme.accentLight : Theme.textSecondary; Layout.preferredWidth: 13; Layout.preferredHeight: 13 }
                        Text {
                            text: qsTr("Cấu Hình Chi Tiết")
                            color: root.activeTab === 1 ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 10
                            font.bold: root.activeTab === 1
                        }
                    }
                    HoverHandler { id: tabHover1 }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = 1
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusSmall - 1
                    color: root.activeTab === 2
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                           : (tabHover2.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
                    border.color: root.activeTab === 2 ? Theme.accentLight : "transparent"
                    border.width: 1
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5
                        LineIcon { name: "upload"; color: root.activeTab === 2 ? Theme.accentLight : Theme.textSecondary; Layout.preferredWidth: 13; Layout.preferredHeight: 13 }
                        Text {
                            text: qsTr("Dữ Liệu & Handoff")
                            color: root.activeTab === 2 ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 10
                            font.bold: root.activeTab === 2
                        }
                    }
                    HoverHandler { id: tabHover2 }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeTab = 2
                    }
                }
            }
        }

        // TAB 1: Advanced Configuration (Clean, No Duplicate Step Cards)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.paddingSmall
            visible: root.activeTab === 1

            DubbingNodeSettingsPanel {
                id: reviewNodeSettings
                objectName: "reviewNodeSettings"
                dubbing: root.dubbing
                nodeId: root.actionNodeId
                node: root.workflowNode
                nodeTitle: root.stepTitle
                canRun: root.canRunStep
                canRerun: root.canRerunStep
                runReady: root.stepRunReady
                nextNodeId: root.nextNodeId
                nextReady: root.nextNodeReady
                visible: node !== null
                compact: true
                Layout.fillWidth: true
                onConfigureRequested: root.configureNodeRequested(root.actionNodeId)
                onLoadRequested: root.dubbing.loadWorkflowNodeModel(root.actionNodeId)
                onUnloadRequested: root.dubbing.unloadWorkflowNodeModel(root.actionNodeId)
                onReloadRequested: root.dubbing.reloadWorkflowNodeModel(root.actionNodeId)
                onRunRequested: root.runStepRequested(root.actionNodeId)
                onNextRequested: root.nextStepRequested(root.actionNodeId)
                onFixRequested: root.fixRequested()
                onArtifactUploadRequested: root.artifactUploadRequested(root.actionNodeId)
            }

            Item { Layout.fillHeight: true }
        }

        // TAB 2: Data & Artifacts
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.paddingSmall
            visible: root.activeTab === 2

            ScrollView {
                id: artifactHandoffScroll
                objectName: "dubbingArtifactUploadPanelReview"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                visible: ["ingest", "normalize", "transcribe", "review-transcript",
                          "fit-timing", "alignment-subtitle", "translate",
                          "review-translation"].indexOf(root.displayedStepId) >= 0

                ColumnLayout {
                    id: artifactHandoffList
                    width: artifactHandoffScroll.availableWidth
                    spacing: Theme.paddingSmall

                    Repeater {
                        model: root.artifactSpecs
                        delegate: DubbingArtifactUploadPanel {
                            objectName: "dubbingArtifactUploadPanelReviewItem"
                            dubbing: root.dubbing
                            nodeId: String(modelData.nodeId || modelData.id || root.actionNodeId)
                            contractSpec: modelData
                            Layout.fillWidth: true
                            onArtifactAccepted: root.handleArtifactAccepted(nodeId)
                        }
                    }

                    Text {
                        visible: root.artifactSpecs.length === 0
                        Layout.fillWidth: true
                        text: qsTr("No upload contract is available for this task.")
                        color: Theme.warning
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        visible: root.artifactSpecs.length > 0
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall
                        Text {
                            Layout.fillWidth: true
                            text: root.artifactSpecs.length > 1 && root.acceptedArtifactNodeIds.length > 0
                                  ? qsTr("Accepted %1 of %2 outputs")
                                      .arg(root.acceptedArtifactNodeIds.length)
                                      .arg(root.artifactSpecs.length)
                                  : qsTr("Upload is optional")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                        }
                        PrimaryButton {
                            objectName: "dubbingArtifactReviewSkipButton"
                            text: qsTr("Skip task & continue")
                            iconName: "chevron-right"
                            enabled: root.dubbing && !root.dubbing.processing
                            toolTip: qsTr("Do not run this task; continue to the next task")
                            onClicked: root.skipArtifactTask()
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        // TAB 0: Main Step Output / Segments List
        ScrollView {
            id: mainStepScroll
            objectName: "dubbingMainStepScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.activeTab === 0
            contentWidth: availableWidth

            ColumnLayout {
                id: mainStepContent
                width: mainStepScroll.availableWidth
                spacing: Theme.paddingSmall

                // Specific Task View for steps
                DubbingImportStep {
                    visible: root.displayedStepId === "import"
                    dubbing: root.dubbing
                    onNextStepRequested: root.nextStepRequested("import")
                }

                DubbingNormalizeStep {
                    visible: root.displayedStepId === "normalize" || root.displayedStepId === "ingest"
                    dubbing: root.dubbing
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onPreviousStepRequested: root.previousStepRequested("normalize")
                    onNextStepRequested: root.nextStepRequested("normalize")
                }

                DubbingSeparateStep {
                    visible: root.displayedStepId === "source-separate" || root.displayedStepId === "isolator"
                    dubbing: root.dubbing
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    playingSeparationStem: root.playingSeparationStem
                    onPreviousStepRequested: root.previousStepRequested("source-separate")
                    onNextStepRequested: root.nextStepRequested("source-separate")
                    onPlaySeparationRequested: function(kind, path) {
                        root.playSeparationRequested(kind, path)
                    }
                }

                DubbingTranscribeStep {
                    visible: root.displayedStepId === "transcribe"
                    dubbing: root.dubbing
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onConfigureRequested: function(nodeId) { root.configureNodeRequested(nodeId) }
                    onOpenColabSetupRequested: function(nodeId) { root.openColabSetupRequested(nodeId) }
                    onArtifactUploadRequested: function(nodeId) { root.artifactUploadRequested(nodeId) }
                    ocrSetupEditable: root.ocrSetupEditable
                    onPreviousStepRequested: root.previousStepRequested("transcribe")
                    onNextStepRequested: root.nextStepRequested("transcribe")
                }

                DubbingTranscriptReviewStep {
                    visible: root.displayedStepId === "review-transcript"
                    dubbing: root.dubbing
                    onPreviousStepRequested: root.previousStepRequested("review-transcript")
                    onOpenTranscriptEditorRequested: root.openTranscriptEditorRequested()
                    onOpenAlignmentStudioRequested: root.openAlignmentStudioRequested()
                    onContinueRequested: root.nextStepRequested("review-transcript")
                }

                DubbingAlignmentStep {
                    visible: root.displayedStepId === "alignment-subtitle"
                             || root.displayedStepId === "fit-timing"
                             || root.displayedStepId === "review-conflicts"
                    dubbing: root.dubbing
                    stepComplete: root.workflowNode
                                  && (root.workflowNode.state === "completed" || root.workflowNode.completed === true)
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onOpenAlignmentStudioRequested: root.openAlignmentStudioRequested()
                    onPreviousStepRequested: root.previousStepRequested("alignment-subtitle")
                    onNextStepRequested: root.nextStepRequested("alignment-subtitle")
                }

                DubbingTranslateStep {
                    visible: root.displayedStepId === "translate"
                    dubbing: root.dubbing
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onPreviousStepRequested: root.previousStepRequested("translate")
                    onNextStepRequested: root.nextStepRequested("translate")
                    onFixRequested: root.fixRequested()
                }

                DubbingTranslationReviewStep {
                    visible: root.displayedStepId === "review-translation"
                    dubbing: root.dubbing
                    onPreviousStepRequested: root.previousStepRequested("review-translation")
                    onOpenSubtitleEditorRequested: root.openSubtitleEditorRequested()
                    onContinueRequested: root.nextStepRequested("review-translation")
                }

                DubbingSynthesizeStep {
                    visible: root.displayedStepId === "synthesize" || root.displayedStepId === "tts"
                    dubbing: root.dubbing
                    sourceMediaPanel: root.sourceMediaPanel
                    playingVoiceClipPath: root.playingVoiceClipPath
                    generatedClipCount: root.generatedClipCount
                    synthesisComplete: root.synthesisComplete
                    onPreviousStepRequested: root.previousStepRequested("synthesize")
                    onNextStepRequested: root.nextStepRequested("synthesize")
                    onVoiceClipPlaybackRequested: function(path) {
                        root.voiceClipPlaybackRequested(path)
                    }
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onVoiceModelRequested: function(nodeId) { root.voiceModelRequested(nodeId) }
                    onSeparationPlaybackStopped: root.separationPlaybackStopped()
                }

                DubbingMixStep {
                    visible: root.displayedStepId === "mix"
                    dubbing: root.dubbing
                    onRunRequested: function(nodeId) { root.runStepRequested(nodeId) }
                    onPreviousStepRequested: root.previousStepRequested("mix")
                    onNextStepRequested: root.nextStepRequested("mix")
                }

                DubbingExportStep {
                    visible: root.displayedStepId === "export"
                    dubbing: root.dubbing
                    onPreviousStepRequested: root.previousStepRequested("export")
                    onOpenExportDialogRequested: root.openExportDialogRequested()
                }

                // Transcribe & Translate Segments Table
                ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.paddingSmall
                visible: (root.displayedStepId === "transcribe" || root.displayedStepId === "translate") && root.dubbing.segments.length > 0

                // Fast conflict resolution banner
                Rectangle {
                    visible: root.dubbing.unresolvedTranscriptConflictCount > 0
                    Layout.fillWidth: true
                    implicitHeight: 38
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
                    border.color: Theme.warning
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.paddingSmall
                        spacing: Theme.paddingSmall
                        LineIcon { name: "alert"; color: Theme.warning; Layout.preferredWidth: 14; Layout.preferredHeight: 14 }
                        Text {
                            text: qsTr("Có %1 xung đột STT/OCR chưa duyệt").arg(root.dubbing.unresolvedTranscriptConflictCount)
                            color: Theme.warning
                            font.bold: true
                            font.pixelSize: Theme.fontSmall
                            Layout.fillWidth: true
                        }
                        PrimaryButton {
                            text: qsTr("Duyệt trong tab Cấu hình")
                            quiet: true
                            onClicked: root.activeTab = 1
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { text: root.stepTitle.toUpperCase(); color: Theme.textPrimary; font.pixelSize: Theme.fontLarge; font.bold: true }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall
                    TextField { Layout.fillWidth: true; placeholderText: qsTr("Tìm kiếm phân đoạn...") }
                    Text { text: qsTr("%1 / %1").arg(root.dubbing.segments.length); color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: Qt.rgba(1, 1, 1, 0.035)
                    radius: Theme.radiusSmall
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.paddingSmall
                        anchors.rightMargin: Theme.paddingSmall
                        spacing: Theme.paddingSmall
                        Text { text: qsTr("THỜI GIAN"); Layout.preferredWidth: 68; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
                        Text { text: qsTr("VĂN BẢN GỐC / DỊCH"); Layout.fillWidth: true; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true }
                        Text { text: qsTr("TRẠNG THÁI"); Layout.preferredWidth: 50; color: Theme.textSecondary; font.pixelSize: 10; font.bold: true; visible: root.width > 380 }
                        Item { Layout.preferredWidth: root.displayedStepId === "translate" ? 64 : 32 }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 5
                    model: root.dubbing.segments
                    delegate: Rectangle {
                        id: segmentDelegate
                        property bool needsTranslationFix:
                            root.displayedStepId === "translate"
                            && (modelData.targetText || "") !== ""
                            && modelData.durationBudget !== undefined
                            && root.dubbing.translationSegmentNeedsFix(index)

                        width: ListView.view.width
                        height: Math.max(98, segmentTextColumn.implicitHeight + Theme.paddingSmall * 2)
                        radius: Theme.radiusSmall
                        color: root.selectedSegment === index ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12) : Qt.rgba(1, 1, 1, 0.025)
                        border.color: root.selectedSegment === index ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55) : Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            onClicked: {
                                root.selectedSegment = index
                                root.segmentSelected(index)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            spacing: Theme.paddingSmall

                            Text {
                                text: "%1–%2".arg(modelData.startMs).arg(modelData.endMs)
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                Layout.preferredWidth: 68
                                elide: Text.ElideRight
                            }

                            ColumnLayout {
                                id: segmentTextColumn
                                Layout.fillWidth: true
                                spacing: 3

                                SegmentTextArea {
                                    text: modelData.sourceText || ""
                                    placeholderText: qsTr("Lời thoại gốc")
                                    onActiveFocusChanged: if (!activeFocus) root.dubbing.updateSegment(index, { sourceText: text })
                                }
                                SegmentTextArea {
                                    text: modelData.targetText || ""
                                    placeholderText: qsTr("Bản dịch mục tiêu")
                                    onActiveFocusChanged: if (!activeFocus) root.dubbing.updateSegment(index, { targetText: text })
                                }

                                Rectangle {
                                    objectName: "dubbingTranscriptConflict-" + index
                                    visible: modelData.fusionStatus === "conflict"
                                    Layout.fillWidth: true
                                    implicitHeight: fusionConflictLayout.implicitHeight + Theme.paddingSmall * 2
                                    radius: Theme.radiusSmall
                                    color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.12)
                                    border.color: Theme.warning
                                    border.width: 1

                                    ColumnLayout {
                                        id: fusionConflictLayout
                                        anchors.fill: parent
                                        anchors.margins: Theme.paddingSmall
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("Xung đột STT/OCR — chọn nguồn đã kiểm tra.")
                                            color: Theme.warning
                                            font.pixelSize: Theme.fontSmall
                                            font.bold: true
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("STT (%1): %2").arg(Number(modelData.sttConfidence || 0).toFixed(2)).arg(modelData.fusionSttText || "")
                                            color: Theme.textSecondary
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("OCR (%1): %2").arg(Number(modelData.ocrConfidence || 0).toFixed(2)).arg(modelData.fusionOcrText || "")
                                            color: Theme.textSecondary
                                            wrapMode: Text.WordWrap
                                        }
                                        RowLayout {
                                            PrimaryButton {
                                                text: qsTr("Xem vị trí")
                                                quiet: true
                                                enabled: !root.dubbing.processing
                                                onClicked: {
                                                    root.selectedSegment = index
                                                    root.segmentSelected(index)
                                                }
                                            }
                                            PrimaryButton {
                                                objectName: "dubbingUseSttConflict-" + index
                                                text: qsTr("Dùng STT")
                                                quiet: true
                                                enabled: !root.dubbing.processing
                                                onClicked: root.dubbing.resolveTranscriptConflict(index, "stt")
                                            }
                                            PrimaryButton {
                                                objectName: "dubbingUseOcrConflict-" + index
                                                text: qsTr("Dùng OCR")
                                                quiet: true
                                                enabled: !root.dubbing.processing
                                                onClicked: root.dubbing.resolveTranscriptConflict(index, "ocr")
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: modelData.durationBudget !== undefined
                                    text: modelData.durationBudget
                                          ? qsTr("Ngân sách %1–%2 âm tiết · hiện tại %3 · %4")
                                                .arg(modelData.durationBudget.minUnits || 0)
                                                .arg(modelData.durationBudget.maxUnits || 0)
                                                .arg(modelData.durationUnits !== undefined ? modelData.durationUnits : "—")
                                                .arg(modelData.durationStatus || qsTr("chờ xử lý"))
                                          : ""
                                    color: modelData.durationStatus === "within-budget" ? Theme.success : Theme.warning
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                text: modelData.state || qsTr("Sẵn sàng")
                                color: modelData.state === "stale" ? Theme.warning : Theme.textSecondary
                                font.pixelSize: 10
                                Layout.preferredWidth: 50
                                visible: root.width > 380
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.preferredWidth: root.displayedStepId === "translate" ? 64 : 32
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Theme.paddingSmall

                                Item {
                                    visible: root.displayedStepId === "translate"
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    Layout.alignment: Qt.AlignVCenter
                                    PrimaryButton {
                                        anchors.fill: parent
                                        visible: (modelData.targetText || "") !== "" && modelData.durationBudget !== undefined
                                        text: ""
                                        iconName: "spark"
                                        iconOnly: true
                                        quiet: true
                                        enabled: !root.dubbing.processing && segmentDelegate.needsTranslationFix
                                        toolTip: qsTr("Tinh chỉnh riêng câu này")
                                        onClicked: root.fixSegmentRequested(index)
                                    }
                                }

                                PrimaryButton {
                                    text: ""
                                    iconName: "trash"
                                    iconOnly: true
                                    quiet: true
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    Layout.alignment: Qt.AlignVCenter
                                    textColor: Theme.danger
                                    toolTip: qsTr("Xóa phân đoạn")
                                    enabled: !root.dubbing.processing
                                    onClicked: root.dubbing.removeSegment(index)
                                }
                            }
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: root.dubbing.segments.length === 0
                        spacing: Theme.paddingSmall
                        LineIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "mic"; color: Theme.accentLight; width: 32; height: 32 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Bản ghi lời thoại sẽ xuất hiện ở đây"); color: Theme.textPrimary; font.pixelSize: Theme.fontMedium; font.bold: true }
                    }
                }

                // Bottom Navigation & Quick Action Strip for Transcribe / Translate
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.paddingSmall

                    PrimaryButton {
                        visible: root.displayedStepId === "translate" && root.dubbing.translationFixCandidateCount > 0
                        text: qsTr("⚡ Tự động sửa bản dịch (%1)").arg(root.dubbing.translationFixCandidateCount)
                        iconName: "spark"
                        buttonColor: Theme.warning
                        quiet: true
                        loading: root.dubbing.translationFixing
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        onClicked: root.fixRequested()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.paddingSmall

                        PrimaryButton {
                            text: qsTr("⬅ Quay lại")
                            iconName: "chevron-left"
                            quiet: true
                            Layout.preferredHeight: 38
                            Layout.preferredWidth: 100
                            onClicked: root.previousStepRequested(root.displayedStepId)
                        }

                        PrimaryButton {
                            text: root.displayedStepId === "transcribe"
                                  ? qsTr("Tiếp tục: Duyệt Lời Thoại ➔")
                                  : qsTr("Tiếp tục: Duyệt Bản Dịch ➔")
                            iconName: "chevron-right"
                            buttonColor: Theme.accent
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            enabled: !root.dubbing.processing && (root.dubbing.segments || []).length > 0
                            onClicked: root.nextStepRequested(root.displayedStepId)
                        }
                    }
                }
                }
            }
        }
    }
}
