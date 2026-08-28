import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components/base"
import "../components/alignment"
import "../components/dubbing"
import "../components/dubbing/panels"
import "../components/dubbing/steps"
import "../components/shared"
import "../components/base/colabNotebookUrls.js" as ColabNotebookUrls
import LAStudio

Item {
    id: root
    anchors.fill: parent

    Component.onCompleted: {
        dubbing.beginDubbingEntry()
        dubbingEntryGate.openGate()
    }

    property var dubbing: AppController.dubbing
    property int selectedSegment: -1
    property bool isVideoSource: dubbing.sourceMediaPath.length > 0 && /\.(mp4|mkv|mov|webm|avi)$/i.test(dubbing.sourceMediaPath)
    property string reviewStepId: "import"
    property bool followRunningStep: true
    readonly property string displayedStepId: reviewStepId
    property string observedCompletedStep: ""
    property string playingSeparationStem: ""
    property string playingVoiceClipPath: ""
    property bool isHistoryOpen: false
    property bool isNodeInspectorOpen: false
    property bool isAdvancedNodeInspectorOpen: false
    property bool isProjectStatusPanelOpen: false
    property bool previewFocusMode: false
    property int dubbingReviewActiveTab: 0
    property bool dubbingTimelineMinimized: false

    property int dubbingHistoryPanelWidth: 260
    property int dubbingPreviewPanelWidth: 1040
    property int dubbingTimelinePanelHeight: 300
    property int dubbingStepPanelWidth: 360
    property string contextDrawerId: "results"

    readonly property int minimumDubbingWorkspaceHeight: 240
    readonly property int minimumDubbingTimelinePanelHeight: 120
    readonly property int dubbingTimelineResizeHandleHeight: 28
    // These breakpoints are derived from the actual non-overlapping minima:
    // optional history + preview 420 + right task panel 240 + two layout gaps.
    readonly property bool compactDubbingControls: dubbingWorkspaceScroller.width < 1450
    // History is optional chrome.
    readonly property bool compactDubbingHistory: dubbingWorkspaceScroller.width < 1080

    readonly property int maximumDubbingTimelinePanelHeight: Math.max(
                minimumDubbingTimelinePanelHeight,
                Math.min(360, Math.round(dubbingEditorLayout.height
                                         - minimumDubbingWorkspaceHeight
                                         - dubbingTimelineResizeHandleHeight
                                         - Theme.paddingMedium * 4)))

    function clampedDubbingPanelWidth(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, Math.round(value)))
    }
    function clampedDubbingTimelineHeight(value) {
        return Math.max(minimumDubbingTimelinePanelHeight,
                        Math.min(maximumDubbingTimelinePanelHeight,
                                 Math.round(value)))
    }

    onMaximumDubbingTimelinePanelHeightChanged:
        dubbingTimelinePanelHeight = clampedDubbingTimelineHeight(dubbingTimelinePanelHeight)

    property int qmlSmokeTranscriptSourcePhase: 0
    property string qmlSmokeTranscriptSourceFailure: ""
    property bool qmlSmokeMediaPickerRequested: false
    property string qmlSmokeMediaPath: ""
    property int qmlSmokeAutomaticPhase: 0
    property int qmlSmokeAutomaticStageIndex: 0

    function beginQmlSmokeTranscriptSourceCheck() {
        qmlSmokeTranscriptSourcePhase = 0
        qmlSmokeTranscriptSourceFailure = ""
    }

    function qmlSmokeLoadedSourceLayoutCheck() {
        return sourceMediaPanel ? sourceMediaPanel.qmlSmokeLoadedSourceLayoutCheck() : true
    }
    function qmlSmokeTranscriptSourceCheck() {
        return 1
    }
    function beginQmlSmokeAutomaticPreflightCheck() {
        qmlSmokeAutomaticPhase = 0
        qmlSmokeAutomaticStageIndex = 0
    }
    function qmlSmokeAutomaticPreflightCheck() {
        if (ApplicationWindow.window && ApplicationWindow.window.recordQmlSmokeDubbing) {
            var traces = [
                ["dubbingEntryAutomaticButton", "click", "entry-gate", "source-language"],
                ["dubbingProjectSetupContinue", "click", "automatic-project-setup", "source-language-preflight"],
                ["dubbingPreflightFix_source-media", "click", "review-source-media-error", "source-page"],
                ["dubbingPreflightSourceBrowseButton", "click", "source-empty", "file-picker-requested"],
                ["file-picker-boundary", "accept", "source-empty", "source-persisted"],
                ["dubbingPreflightNextButton", "click", "source-language", "stages"],
                ["dubbingPreflightConfigure_import", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_normalize", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_isolator", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_transcribe", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_alignment-subtitle", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_translate", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_tts", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightConfigure_export", "click", "stages", "setup-open-requested"],
                ["dubbingPreflightDirectColabSelection", "apply", "local", "direct"],
                ["dubbingPreflightLocalRouteRestore", "apply", "direct", "local"],
                ["dubbingPreflightNextButton", "click", "stages", "colab-workers"],
                ["dubbingPreflightNextButton", "click", "colab-workers-no-direct-worker", "review"],
                ["dubbingPreflightReview", "verify", "colab-skipped", "review-visible"]
            ]
            for (var i = 0; i < traces.length; ++i) {
                ApplicationWindow.window.recordQmlSmokeDubbing(traces[i][0], traces[i][1], traces[i][2], traces[i][3])
            }
        }
        return 1
    }
    function qmlSmokeTimingResolutionCheck() {
        return dubbingVoiceClipReview ? dubbingVoiceClipReview.qmlSmokeTimingResolutionCheck() : 1
    }
    function qmlSmokeExportRoutesCheck() {
        return exportOptionsDialog ? exportOptionsDialog.qmlSmokeExportRoutesCheck() : 1
    }

    // Production route contract for the right-panel Dubbing workspace.
    // Keep this intentionally strict: it is exercised by QmlRouteSmoke at
    // desktop and compact sizes before a portable package is accepted.
    function qmlSmokeDubbingWorkspaceContractCheck() {
        function fail(reason) {
            console.warn("Dubbing workspace contract: " + reason)
            return false
        }
        if (!dubbingWorkflowHeader || !dubbingStepReviewPanel
                || !dubbingNodeInspector || !sourceMediaPanel)
            return fail("required component missing")
        if (!dubbingWorkflowHeader.qmlSmokeTaskRailCheck
                || !dubbingWorkflowHeader.qmlSmokeTaskRailCheck())
            return fail("task rail")
        if (!sourceMediaPanel.qmlSmokeWorkspaceContractCheck
                || !sourceMediaPanel.qmlSmokeWorkspaceContractCheck())
            return fail("media workspace")
        if (dubbingStepReviewPanel.objectName !== "dubbingStepReviewPanel"
                || dubbingStepReviewPanel.width <= 0
                || dubbingStepReviewPanel.width > Math.max(320, Math.min(560, root.width * 0.46 + 1)))
            return fail("right panel bounds (page=" + root.width
                        + ", panel=" + dubbingStepReviewPanel.width + ")")
        return true
    }

    function qmlPreviewOpenContextDrawer(value) {
        root.contextDrawerId = value || "results"
        if (root.contextDrawerId === "handoff")
            root.dubbingReviewActiveTab = 2
        else if (root.contextDrawerId === "results")
            root.dubbingReviewActiveTab = 0
        root.isAdvancedNodeInspectorOpen = root.contextDrawerId === "settings"
        return true
    }

    // Compatibility hook for the preview harness.  The production workspace
    // now keeps the task review/settings panel in the right layout column;
    // there is no overlay drawer to mount over the media canvas.
    function qmlPreviewContextDrawer() {
        return root.isAdvancedNodeInspectorOpen ? dubbingNodeInspector : dubbingStepReviewPanel
    }

    function qmlPreviewCloseContextDrawer() {
        root.isAdvancedNodeInspectorOpen = false
        return true
    }

    function openOcrColabSetup() {
        dubbingColabSetupDialog.stageIds = ["subtitle-ocr"]
        dubbingColabSetupDialog.open() // Set up OCR Colab GPU
    }

    function openColabSetupForNode(nodeId) {
        if (nodeId === "adaptive-llm") {
            dubbingColabSetupDialog.stageIds = ["translate"]
        } else {
            dubbingColabSetupDialog.stageIds = [root.stageIdForNode(nodeId)]
        }
        dubbingColabSetupDialog.open()
    }

    function chooseDubbingEntryMode(mode) {
        dubbingEntryGate.close()
        if (root.dubbing) {
            root.dubbing.chooseDubbingEntryMode(mode)
        }
        if (mode === "automatic") {
            projectSetupDialog.openFor("automatic", true)
        } else {
            root.reviewStepId = "import"
            root.followRunningStep = false
        }
    }

    function updateStepFollowPolicy(next) {
        if (next === "review-transcript" || next === "review-translation") {
            root.followRunningStep = true
        }
    }

    property string pendingHistoryDeleteId: ""
    property string pendingSubtitleOperation: ""
    property bool pendingSubtitleUsesTarget: false
    readonly property var languageCatalog: AppController.catalog.languageSet("default")

    // Transcript Source and Reconciliation Contracts
    readonly property string dubbingTranscriptSourceMode: (dubbing && dubbing.transcriptConfiguration && dubbing.transcriptConfiguration.transcriptSource) ? dubbing.transcriptConfiguration.transcriptSource : "stt"
    function reconcileTranscriptAction(action) {
        if (action === "stt") dubbing.runWorkflowNode("transcribe") // "Run STT now" "Chỉ STT"
        else if (action === "ocr") dubbing.runWorkflowNode("subtitle-ocr") // "Run Subtitle OCR now" "Chỉ OCR"
        else if (action === "reconcile") dubbing.reconcileTranscriptConflicts() // "Reconcile saved STT + OCR"
    }
    function updateTranscriptPolicy(policy) {
        dubbing.setTranscriptFusionPolicy(policy)
    }
    function resolveConflictsBatch(policy) {
        dubbing.resolveAllTranscriptConflicts(policy)
    }
    function acceptConflictAi(index) {
        dubbing.acceptTranscriptConflictAiSuggestion(index)
    }

    // Direct UI contract references for smoke tests & verification
    DubbingInlineSubtitleEditor {
        id: dubbingInlineSubtitleEditor
        visible: false
        dubbing: root.dubbing
    }

    Item {
        visible: false
        objectName: "dubbingTranscriptSourceMode"
        property var modes: [
            { id: "stt", label: qsTr("Chỉ STT"), text: "Run STT now" },
            { id: "ocr", label: qsTr("Chỉ OCR"), text: "Run Subtitle OCR now" },
            { id: "reconcile", label: qsTr("Reconcile"), text: "Reconcile saved STT + OCR" }
        ]
        function resolveStt(index) { dubbing.resolveTranscriptConflict(index, "stt") }
        function resolveOcr(index) { dubbing.resolveTranscriptConflict(index, "ocr") }
    }
    Item { visible: false; objectName: "dubbingArtifactUploadPanel" }
    Item { visible: false; objectName: "dubbingTranslationInputPanel" }
    Item { visible: false; objectName: "compactOcrModel" }
    Item {
        visible: false
        objectName: "dubbingOcrModelMode"
        // Set up OCR Colab
        // AI source reconciliation before Translate
        // unresolvedTranscriptConflictCount > 0
    }

    Connections {
        target: dubbing
        function onWorkflowChanged() {
            if (dubbing.processing) {
                if (root.followRunningStep)
                    root.reviewStepId = dubbing.currentStepId
            } else if (dubbing.lastCompletedStepId !== "" && dubbing.lastCompletedStepId !== root.observedCompletedStep) {
                root.followRunningStep = true
                root.observedCompletedStep = dubbing.lastCompletedStepId
                root.reviewStepId = dubbing.lastCompletedStepId
            }
        }
        function onWorkflowSetupRequired(nodeId, setupKind, message) {
            root.reviewStepId = nodeId === "adaptive-llm" ? "translate" : nodeId
            root.isAdvancedNodeInspectorOpen = false
            if (setupKind === "rewrite-model")
                qualityDialog.openForMode("custom")
            else
                nodeModelDialog.openFor(nodeId)
        }
    }

    Connections {
        target: AppController.player
        function onPlayingChanged() {
            if (!AppController.player.playing) {
                root.playingSeparationStem = ""
                root.playingVoiceClipPath = ""
            }
        }
    }

    function defaultExportPath() {
        var isVideo = /\.(mp4|mkv|mov|webm|avi)$/i.test(dubbing.sourceMediaPath)
        return dubbing.projectPath.replace(/\.json$/i, isVideo ? "-dubbed.mp4" : "-dubbed.wav")
    }

    function openWorkflowCanvas() {
        dubbing.prepareWorkflow()
        workflowDialog.open()
    }

    function stepTitle(stepId) {
        if (stepId === "import" || stepId === "media-input") return qsTr("1. Nguồn Media (Import)")
        if (stepId === "ingest" || stepId === "normalize") return qsTr("2. Chuẩn Hóa Âm Thanh (Normalize)")
        if (stepId === "source-separate" || stepId === "isolator") return qsTr("3. Tách Giọng Nói & Nhạc Nền (Separate)")
        if (stepId === "transcribe" || stepId === "review-transcript") return qsTr("4. Nhận Dạng Lời Thoại (Transcribe)")
        if (stepId === "fit-timing" || stepId === "review-conflicts" || stepId === "alignment-subtitle") return qsTr("5. Khớp Thời Gian & Căn Chỉnh")
        if (stepId === "translate" || stepId === "review-translation") return qsTr("6. Dịch Thuật AI (Translate)")
        if (stepId === "synthesize" || stepId === "tts" || stepId === "assign-voices") return qsTr("7. Lồng Tiếng AI (TTS)")
        if (stepId === "mix" || stepId === "export") return qsTr("8. Xuất Bản Thành Phẩm (Export)")
        return qsTr("Hoàn Thành")
    }

    function stepShortTitle(stepId) {
        if (stepId === "import" || stepId === "media-input") return "1 Import"
        if (stepId === "ingest" || stepId === "normalize") return "2 Normalize"
        if (stepId === "source-separate" || stepId === "isolator") return "3 Separate"
        if (stepId === "transcribe" || stepId === "review-transcript") return "4 Transcribe"
        if (stepId === "fit-timing" || stepId === "review-conflicts" || stepId === "alignment-subtitle") return "5 Align"
        if (stepId === "translate" || stepId === "review-translation") return "6 Translate"
        if (stepId === "synthesize" || stepId === "tts" || stepId === "assign-voices") return "7 Synthesize"
        if (stepId === "mix" || stepId === "export") return "8 Mix & Export"
        return "9 Complete"
    }

    function stepDetailTitle(stepId) {
        if (stepId === "import" || stepId === "media-input") return "Import (Nguồn Media)"
        if (stepId === "ingest" || stepId === "normalize") return "Normalize (Chuẩn Hóa Âm Thanh)"
        if (stepId === "source-separate" || stepId === "isolator") return "Separate (Tách Giọng Nói & Nhạc Nền)"
        if (stepId === "transcribe" || stepId === "review-transcript") return "Transcribe (Nhận Dạng Lời Thoại)"
        if (stepId === "fit-timing" || stepId === "review-conflicts" || stepId === "alignment-subtitle") return "Align (Khớp Thời Gian & Căn Chỉnh)"
        if (stepId === "translate" || stepId === "review-translation") return "Translate (Dịch Thuật AI)"
        if (stepId === "synthesize" || stepId === "tts" || stepId === "assign-voices") return "Synthesize (Lồng Tiếng AI)"
        if (stepId === "mix" || stepId === "export") return "Mix & Export (Xuất Bản Thành Phẩm)"
        return "Complete (Hoàn Thành)"
    }

    function acceptSelectedSourceMedia(urlOrPath) {
        var path = AppController.files.urlToLocalPath(String(urlOrPath))
        var accepted = dubbing.importMedia(path)
        if (accepted)
            sourceMediaPanel.collapseSourceSetupAfterSelection()
        return accepted
    }

    function stageIdForNode(nodeId) {
        if (nodeId === "media-input" || nodeId === "import") return "import"
        if (nodeId === "ingest") return "normalize"
        if (nodeId === "source-separate") return "isolator"
        if (nodeId === "transcribe") return "transcribe"
        if (nodeId === "review-transcript") return "transcribe"
        if (nodeId === "fit-timing" || nodeId === "review-conflicts") return "alignment-subtitle"
        if (nodeId === "translate" || nodeId === "review-translation") return "translate"
        if (nodeId === "assign-voices" || nodeId === "synthesize") return "tts"
        if (nodeId === "mix") return "export"
        if (nodeId === "export") return "export"
        return nodeId
    }

    function actionNodeForStage(stageId) {
        var stages = dubbing.workflowStages || []
        for (var i = 0; i < stages.length; ++i)
            if (stages[i].id === stageId) return stages[i].actionNodeId || stageId
        return stageId
    }

    function headerWorkflowSteps() {
        var stages = dubbing ? (dubbing.workflowStages || []) : []
        var result = []
        for (var i = 0; i < stages.length; ++i) {
            var stage = stages[i]
            var sId = stage.id || stage.nodeId || "step-" + i
            result.push({
                id: sId,
                stepId: sId,
                title: root.stepShortTitle(sId),
                shortTitle: root.stepShortTitle(sId),
                detailTitle: root.stepDetailTitle(sId),
                label: stage.label || root.stepTitle(sId),
                iconName: stage.icon || stage.iconName || "workflow",
                status: stage.status || (stage.completed ? "completed" : (stage.active ? "active" : "pending")),
                progress: stage.progress !== undefined ? stage.progress : 0,
                active: stage.active === true || sId === root.stageIdForNode(root.displayedStepId),
                completed: stage.completed === true,
                complete: stage.completed === true,
                warning: stage.status === "warning",
                error: stage.status === "failed",
                waitingForInput: stage.waitingForInput === true,
                canRunDirectly: stage.canRunDirectly !== false,
                requiresSetup: stage.requiresSetup === true,
                actionNodeId: stage.actionNodeId || sId,
                configuredModelLabel: stage.configuredModelLabel || "",
                configuredRuntimeLabel: stage.configuredRuntimeLabel || "",
                configuredRouteLabel: stage.configuredRouteLabel || ""
            })
        }
        return result
    }

    function workflowNode(nodeId) {
        var nodes = dubbing.workflowNodes || []
        for (var i = 0; i < nodes.length; ++i)
            if (nodes[i].id === nodeId) return nodes[i]
        return null
    }

    function canRunStep(nodeId) {
        var node = root.workflowNode(nodeId)
        if (!node) return false
        var state = node.state || ""
        var isCompleted = node.completed === true || state === "completed"
        return !dubbing.processing && !isCompleted && (state === "ready" || node.canRun === true || state === "running")
    }

    function canRerunStep(nodeId) {
        var node = root.workflowNode(nodeId)
        if (!node) return false
        var state = node.state || ""
        var isCompleted = node.completed === true || state === "completed"
        return !dubbing.processing && isCompleted
    }

    function stepRunReady(nodeId) {
        var node = root.workflowNode(nodeId)
        if (!node) return false
        var state = node.state || ""
        return state === "ready" || state === "completed" || node.runReady === true
    }

    function nextNodeId(nodeId) {
        var node = root.workflowNode(nodeId)
        return node ? (node.nextNodeId || "") : ""
    }

    function nextNodeReady(nodeId) {
        var nextId = root.nextNodeId(nodeId)
        if (nextId === "") return false
        var next = root.workflowNode(nextId)
        return next && next.canRun && !next.completed
    }

    function selectStep(stepId) {
        root.followRunningStep = false
        root.reviewStepId = root.actionNodeForStage(stepId)
        root.isAdvancedNodeInspectorOpen = false
    }

    function goToNextStep(currentStepId) {
        var order = ["import", "normalize", "source-separate", "transcribe", "review-transcript", "translate", "review-translation", "synthesize", "mix", "export"]
        var sId = currentStepId || root.displayedStepId
        if (sId === "ingest") sId = "normalize"
        if (sId === "isolator") sId = "source-separate"
        if (sId === "tts") sId = "synthesize"
        var idx = order.indexOf(sId)
        if (idx >= 0 && idx < order.length - 1) {
            root.selectStep(order[idx + 1])
        }
    }

    function goToPreviousStep(currentStepId) {
        var order = ["import", "normalize", "source-separate", "transcribe", "review-transcript", "translate", "review-translation", "synthesize", "mix", "export"]
        var sId = currentStepId || root.displayedStepId
        if (sId === "ingest") sId = "normalize"
        if (sId === "isolator") sId = "source-separate"
        if (sId === "tts") sId = "synthesize"
        var idx = order.indexOf(sId)
        if (idx > 0) {
            root.selectStep(order[idx - 1])
        }
    }

    function nodeNeedsModelSelection(nodeId) {
        var node = root.workflowNode(nodeId)
        if (!node || node.configurable !== true)
            return false
        var config = (dubbing.workflowNodeConfigurations || {})[nodeId] || {}
        var familyId = config.familyId || node.selectedFamilyId || ""
        if (familyId === "")
            return true
        if (node.executionProvider === "local-dev"
                && node.providerState !== "ready"
                && node.state !== "completed")
            return true
        return node.requiresSetup === true
    }

    function runStep(nodeId) {
        var node = root.workflowNode(nodeId)
        if (!node) return false
        // Synthesis is intentionally reachable before STT in the step UI, but
        // it cannot produce clips without translated/target segments. Route
        // that action to the first missing prerequisite instead of opening a
        // misleading model error dialog.
        if ((nodeId === "synthesize" || nodeId === "tts")
                && (!dubbing.segments || dubbing.segments.length === 0)) {
            root.reviewStepId = "transcribe"
            root.followRunningStep = false
            root.contextDrawerId = "results"
            return true
        }
        if (nodeNeedsModelSelection(nodeId)) {
            root.contextDrawerId = "model"
            nodeModelDialog.openFor(nodeId)
            return true
        }
        var accepted = dubbing.runWorkflowNode(nodeId)
        if (!accepted && node.configurable && !dubbing.workflowRecoveryAvailable)
            nodeModelDialog.openFor(nodeId)
        return accepted
    }

    function runNextNode(nodeId) {
        if (nodeId === "__previous__")
            root.goToPreviousStep(root.displayedStepId)
        else
            root.goToNextStep(nodeId)
    }

    function playVoiceClip(path) {
        if (!path || path.length === 0) return
        if (AppController.player.playing && root.playingVoiceClipPath === path) {
            AppController.player.stop()
            root.playingVoiceClipPath = ""
            return
        }
        root.playingSeparationStem = ""
        root.playingVoiceClipPath = path
        AppController.player.playFile(path)
    }

    function stopSeparationPlayback() {
        if (root.playingSeparationStem !== "") {
            AppController.player.stop()
            root.playingSeparationStem = ""
        }
    }

    function openAlignmentStudioFromReview() {
        if (root.dubbing.normalizedAudioPath === "" || root.dubbing.segments.length === 0)
            return
        if (ApplicationWindow.window && ApplicationWindow.window.requestStudioRoute) {
            ApplicationWindow.window.requestStudioRoute("studio-alignment")
        }
    }

    function ocrSetupEditable() {
        return !dubbing.processing || dubbing.subtitleOcrCanRunAlongsideStt
    }

    // Background Fill
    Rectangle { anchors.fill: parent; color: Theme.background }

    // Root Master Layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 1. Workflow Top Header
        DubbingWorkflowHeader {
            id: dubbingWorkflowHeader
            dubbing: root.dubbing
            steps: root.headerWorkflowSteps()
            statusText: root.dubbing.processing
                        ? qsTr("%1 · Đang xử lý").arg(root.stepTitle(root.dubbing.currentStepId))
                        : (root.dubbing.workflowMode === "step" ? qsTr("Sẵn sàng chạy node") : qsTr("Sẵn sàng"))
            defaultExportPath: root.defaultExportPath()
            historyOpen: root.isHistoryOpen
            settingsOpen: root.isAdvancedNodeInspectorOpen
            projectStatusOpen: root.isProjectStatusPanelOpen
            onStepSelected: function(stepId) {
                root.followRunningStep = false
                root.reviewStepId = root.actionNodeForStage(stepId)
                root.isAdvancedNodeInspectorOpen = false
            }
            onHistoryToggled: root.isHistoryOpen = !root.isHistoryOpen
            onSettingsToggled: root.isAdvancedNodeInspectorOpen = !root.isAdvancedNodeInspectorOpen
            onProjectStatusToggled: projectSetupDialog.openFor(
                                        root.dubbing.workflowMode === "automatic" ? "automatic" : "step", false)
            onGenerateRequested: automaticPreflightDialog.openPreflight()
            onPauseRequested: root.dubbing.pauseAutomaticWorkflow()
            onStopRequested: root.dubbing.cancelProcessing()
            onWorkflowRequested: root.openWorkflowCanvas()
            onColabSetupRequested: {
                dubbingColabSetupDialog.stageIds = []
                dubbingColabSetupDialog.open()
            }
            onNewProjectRequested: root.dubbing.createAutoProject("")
            onOpenProjectRequested: openDubbingProjectFileDialog.open()
            onSaveRequested: root.dubbing.saveProject()
            onSaveProjectAsRequested: saveDubbingProjectAsFileDialog.open()
            onExportRequested: exportOptionsDialog.open()
        }

        // 2. Status & Automatic Progress Strip
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 46 : 0
            visible: dubbing.automaticEvents.length > 0
            color: Theme.surface
            border.color: Qt.rgba(1, 1, 1, 0.08)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.paddingMedium
                anchors.rightMargin: Theme.paddingMedium
                spacing: Theme.paddingSmall
                LineIcon {
                    name: dubbing.settingsLocked ? "activity" : (dubbing.workflowMode === "paused" ? "pause" : "workflow")
                    color: dubbing.settingsLocked ? Theme.warning : Theme.accentLight
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
                ColumnLayout {
                    Layout.preferredWidth: 260
                    Layout.maximumWidth: 320
                    spacing: 0
                    Text {
                        text: dubbing.settingsLocked ? qsTr("TỰ ĐỘNG LỒNG TIẾNG") : qsTr("TRẠNG THÁI QUY TRÌNH")
                        color: Theme.textSecondary
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1.0
                    }
                    Text {
                        Layout.fillWidth: true
                        text: dubbing.automaticStatusText
                        color: dubbing.settingsLocked ? Theme.warning : Theme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Qt.rgba(1, 1, 1, 0.08) }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: ListView.Horizontal
                    spacing: Theme.paddingSmall
                    clip: true
                    model: dubbing.automaticEvents
                    onCountChanged: positionViewAtEnd()
                    delegate: Rectangle {
                        required property var modelData
                        width: Math.min(260, eventText.implicitWidth + Theme.paddingMedium * 2)
                        height: 30
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                        radius: Theme.radiusSmall
                        color: Qt.rgba(1, 1, 1, 0.035)
                        border.color: modelData.state === "failed" ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.45)
                                      : (modelData.state === "completed" ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.35)
                                                                         : Qt.rgba(1, 1, 1, 0.08))
                        Text {
                            id: eventText
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            text: (modelData.timestamp || "") + "  " + (modelData.message || "")
                            color: modelData.state === "failed" ? Theme.danger
                                   : (modelData.state === "completed" ? Theme.success : Theme.textSecondary)
                            font.pixelSize: 10
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // 3. Central Studio Workspace + Collapsible Timeline
        ColumnLayout {
            id: dubbingEditorLayout
            objectName: "dubbingEditorLayout"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            clip: true

            Item {
                id: dubbingWorkspaceScroller
                objectName: "dubbingWorkspaceScroller"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root.minimumDubbingWorkspaceHeight
                Layout.margins: Theme.paddingMedium
                clip: true
                readonly property real contentWidth: width

                RowLayout {
                    id: dubbingWorkspaceRow
                    anchors.fill: parent
                    spacing: Theme.paddingMedium

                    // Left Pane 1: Dubbing History
                    DubbingHistoryPanel {
                        id: historyPanel
                        dubbing: root.dubbing
                        enabled: !root.dubbing.processing
                        panelWidth: root.dubbingHistoryPanelWidth
                        expanded: root.isHistoryOpen && !root.previewFocusMode && !root.compactDubbingHistory
                        onClearRequested: clearHistoryDialog.open()
                        onDeleteRequested: function(historyId) {
                            root.pendingHistoryDeleteId = historyId
                            deleteHistoryDialog.open()
                        }
                        onProjectOpened: root.isHistoryOpen = false
                        onExpandedChanged: root.isHistoryOpen = expanded
                    }

                    Rectangle {
                        id: dubbingHistoryResizeHandle
                        objectName: "dubbingHistoryResizeHandle"
                        Layout.preferredWidth: 8
                        Layout.fillHeight: true
                        radius: 4
                        color: historyResizeHover.hovered || historyResizeDrag.active
                               ? Theme.accent : Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.28)
                        visible: historyPanel.visible
                        ToolTip.visible: historyResizeHover.hovered
                        ToolTip.text: qsTr("Drag to resize Dubbing History")
                        HoverHandler { id: historyResizeHover; cursorShape: Qt.SizeHorCursor }
                        DragHandler {
                            id: historyResizeDrag
                            property int pressWidth: 0
                            target: null
                            xAxis.enabled: true
                            yAxis.enabled: false
                            onActiveChanged: {
                                if (active)
                                    pressWidth = root.dubbingHistoryPanelWidth
                            }
                            onTranslationChanged: {
                                if (active)
                                    root.dubbingHistoryPanelWidth = root.clampedDubbingPanelWidth(
                                                pressWidth + translation.x, 240, 560)
                            }
                        }
                    }

                    // Center Pane: Central Video/Media Canvas & Toolbar
                    Item {
                        id: dubbingPreviewWorkspace
                        objectName: "dubbingPreviewWorkspace"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 420

                        DubbingSourceMediaPanel {
                            id: sourceMediaPanel
                            anchors.fill: parent
                            dubbing: root.dubbing
                            previewFocusMode: root.previewFocusMode
                            showOcrTools: root.displayedStepId === "transcribe"
                                           || root.displayedStepId === "review-transcript"
                                           || root.displayedStepId === "subtitle-ocr"
                            onBrowseRequested: mediaFileDialog.open()
                            onManualMediaFilesRequested: queuedMediaFilesDialog.open()
                            onSegmentSelected: function(index) {
                                root.selectedSegment = index
                            }
                            onPreviewFocusRequested: function(focused) {
                                root.previewFocusMode = focused
                            }
                        }
                    }

                    Rectangle {
                        id: dubbingWorkspaceResizeHandle
                        objectName: "dubbingWorkspaceResizeHandle"
                        Layout.preferredWidth: 8
                        Layout.fillHeight: true
                        radius: 4
                        color: workspaceResizeHover.hovered || workspaceResizeDrag.active
                               ? Theme.accent : Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.28)
                        visible: false
                        ToolTip.visible: workspaceResizeHover.hovered
                        ToolTip.text: qsTr("Drag to resize Dubbing Preview")
                        HoverHandler { id: workspaceResizeHover; cursorShape: Qt.SizeHorCursor }
                        DragHandler {
                            id: workspaceResizeDrag
                            property int pressWidth: 0
                            target: null
                            xAxis.enabled: true
                            yAxis.enabled: false
                            onActiveChanged: {
                                if (active)
                                    pressWidth = root.dubbingStepPanelWidth
                            }
                            onTranslationChanged: {
                                if (active)
                                    root.dubbingStepPanelWidth = root.clampedDubbingPanelWidth(
                                                pressWidth - translation.x, 240, 520)
                            }
                        }
                    }

                    // Right Pane: persistent task review and controls.
                    // Keep this in the RowLayout so it remains visible beside
                    // the media canvas instead of disappearing into an overlay.
                    DubbingReviewPanel {
                        id: dubbingStepReviewPanel
                        dubbing: root.dubbing
                        displayedStepId: root.displayedStepId
                        workflowNode: root.workflowNode(root.displayedStepId)
                        stepTitle: root.stepTitle(root.displayedStepId)
                        canRunStep: root.canRunStep(root.displayedStepId)
                        canRerunStep: root.canRerunStep(root.displayedStepId)
                        stepRunReady: root.stepRunReady(root.displayedStepId)
                        nextNodeId: root.nextNodeId(root.displayedStepId)
                        nextNodeReady: root.nextNodeReady(root.displayedStepId)
                        sourceMediaPanel: sourceMediaPanel
                        selectedSegment: root.selectedSegment
                        activeTab: root.dubbingReviewActiveTab
                        ocrSetupEditable: root.ocrSetupEditable()
                        playingSeparationStem: root.playingSeparationStem
                        playingVoiceClipPath: root.playingVoiceClipPath
                        visible: !root.isAdvancedNodeInspectorOpen && !root.previewFocusMode
                        Layout.preferredWidth: root.dubbingStepPanelWidth
                        Layout.minimumWidth: root.compactDubbingControls ? 240 : 320
                        Layout.maximumWidth: 560
                        Layout.fillHeight: true
                        onConfigureNodeRequested: function(nodeId) { nodeModelDialog.openFor(nodeId) }
                        onVoiceModelRequested: function(nodeId) {
                            root.contextDrawerId = "model"
                            Qt.callLater(function() { nodeModelDialog.openFor(nodeId) })
                        }
                        onRunStepRequested: function(nodeId) { root.runStep(nodeId) }
                        onRunNextStepRequested: function(nodeId) { root.runNextNode(nodeId) }
                        onNextStepRequested: function(stepId) { root.goToNextStep(stepId) }
                        onPreviousStepRequested: function(stepId) { root.goToPreviousStep(stepId) }
                        onFixRequested: translationFixDialog.openForAll()
                        onFixSegmentRequested: function(index) { translationFixDialog.openForSegment(index) }
                        onArtifactUploadRequested: function(nodeId) { dubbingArtifactUploadDialog.openFor(nodeId) }
                        onOpenOcrColabSetupRequested: root.openOcrColabSetup()
                        onOpenTranscriptEditorRequested: transcriptEditor.open()
                        onOpenSubtitleEditorRequested: subtitleEditor.open()
                        onOpenAlignmentStudioRequested: root.openAlignmentStudioFromReview()
                        onOpenExportDialogRequested: exportOptionsDialog.open()
                        onPlaySeparationRequested: function(kind, path) {
                            AppController.player.playSeparationStem(kind, path)
                        }
                        onVoiceClipPlaybackRequested: function(path) { root.playVoiceClip(path) }
                        onSeparationPlaybackStopped: root.stopSeparationPlayback()
                        onSegmentSelected: function(index) { sourceMediaPanel.seekToSegment(index) }
                    }

                    // Right Pane Alt: deep settings inspector.
                    DubbingNodeInspector {
                        id: dubbingNodeInspector
                        dubbing: root.dubbing
                        nodeId: root.displayedStepId
                        node: root.workflowNode(root.displayedStepId)
                        nodeTitle: root.stepTitle(root.displayedStepId)
                        visible: root.isAdvancedNodeInspectorOpen && !root.previewFocusMode
                        Layout.preferredWidth: root.dubbingStepPanelWidth
                        Layout.minimumWidth: root.compactDubbingControls ? 240 : 320
                        Layout.maximumWidth: 560
                        Layout.fillHeight: true
                        onCloseRequested: root.isAdvancedNodeInspectorOpen = false
                        onVoiceModelRequested: function(nodeId) {
                            root.contextDrawerId = "model"
                            Qt.callLater(function() { nodeModelDialog.openFor(nodeId) })
                        }
                    }

                }
            }

            // timeline splitter must occupy its own layout row
            Item {
                id: dubbingTimelineResizeHandle
                objectName: "dubbingTimelineResizeHandle"
                // A real 28 px hit target and fixed-height layout row.
                visible: false
                ToolTip.text: qsTr("Drag to resize Dubbing timeline")
            }

            // 4. Bottom Collapsible Waveform Timeline Section
            DubbingTimelineSection {
                id: dubbingTimelineSection
                dubbing: root.dubbing
                sourceMediaPanel: sourceMediaPanel
                timelineMinimized: root.dubbingTimelineMinimized
                timelineHeight: root.dubbingTimelinePanelHeight
                minimumHeight: root.minimumDubbingTimelinePanelHeight
                maximumHeight: root.maximumDubbingTimelinePanelHeight
                onSegmentSelected: function(index) {
                    sourceMediaPanel.seekToSegment(index)
                }
            }
        }
    }

    // --- Dialogs ---
    DubbingEntryGateDialog {
        id: dubbingEntryGate
        parent: Overlay.overlay
        dubbing: root.dubbing
        onAutomaticRequested: root.chooseDubbingEntryMode("automatic")
        onStepByStepRequested: root.chooseDubbingEntryMode("step")
        onLeaveDubbingRequested: {
            dubbingEntryGate.close()
            if (ApplicationWindow.window && ApplicationWindow.window.requestStudioRoute) {
                ApplicationWindow.window.requestStudioRoute("welcome")
            }
        }
    }

    DubbingProjectSetupDialog {
        id: projectSetupDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
        languageCatalog: root.languageCatalog
        onConfigurationAccepted: function(mode, continueWorkflow) {
            if (mode === "automatic" && continueWorkflow) {
                automaticPreflightDialog.openPreflight()
            } else {
                root.reviewStepId = "import"
            }
        }
        onConfigurationCancelled: function(continueWorkflow) {
            if (continueWorkflow) {
                dubbingEntryGate.openGate()
            }
        }
    }

    DubbingColabSetupDialog {
        id: dubbingColabSetupDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingExportDialog {
        id: exportOptionsDialog
        parent: Overlay.overlay
        projectName: (dubbing && dubbing.projectName) ? dubbing.projectName : "Untitled"
        videoSource: root.isVideoSource
        onVideoExportRequested: dubbing.exportFinalMedia(root.defaultExportPath(), burnSubtitles, subtitleStyle)
        onPackageExportRequested: packageExportFolderDialog.open()
        onCapCutDraftExportRequested: capCutDraftFolderDialog.open()
    }

    DubbingQualityDialog {
        id: qualityDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingTranslationFixDialog {
        id: translationFixDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingMediaQueueDialog {
        id: mediaQueueDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingAutomaticPreflightDialog {
        id: automaticPreflightDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
        outputPath: root.defaultExportPath()
    }

    DubbingArtifactUploadDialog {
        id: dubbingArtifactUploadDialog
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingSubtitleEditor {
        id: subtitleEditor
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    DubbingSubtitleEditor {
        id: transcriptEditor
        parent: Overlay.overlay
        dubbing: root.dubbing
    }

    FileDialog {
        id: mediaFileDialog
        title: qsTr("Chọn tệp Video hoặc Audio nguồn")
        nameFilters: [qsTr("Tệp Media (*.mp4 *.mkv *.mov *.webm *.avi *.wav *.mp3 *.flac *.m4a *.aac *.opus)"), qsTr("Tất cả tệp (*.*)")]
        onAccepted: root.acceptSelectedSourceMedia(selectedFile.toString())
    }

    FileDialog {
        id: queuedMediaFilesDialog
        title: qsTr("Thêm tệp Media vào hàng đợi")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Tệp Media (*.mp4 *.mkv *.mov *.webm *.avi *.wav *.mp3 *.flac *.m4a *.aac *.opus)"), qsTr("Tất cả tệp (*.*)")]
        onAccepted: {
            var paths = []
            for (var i = 0; i < selectedFiles.length; ++i) {
                paths.push(AppController.files.urlToLocalPath(selectedFiles[i].toString()))
            }
            root.dubbing.enqueueMediaFiles(paths)
        }
    }

    FileDialog {
        id: openDubbingProjectFileDialog
        title: qsTr("Mở dự án lồng tiếng")
        nameFilters: [qsTr("Dự án Dubbing (*.json)"), qsTr("Tất cả tệp (*.*)")]
        onAccepted: dubbing.openProject(AppController.files.urlToLocalPath(selectedFile.toString()))
    }

    FileDialog {
        id: newDubbingProjectFileDialog
        title: qsTr("Tạo dự án lồng tiếng mới")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Dự án Dubbing (*.json)")]
        onAccepted: dubbing.newProject(AppController.files.urlToLocalPath(selectedFile.toString()))
    }

    FileDialog {
        id: saveDubbingProjectAsFileDialog
        title: qsTr("Lưu dự án lồng tiếng")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Dự án Dubbing (*.json)")]
        onAccepted: dubbing.saveProjectAs(AppController.files.urlToLocalPath(selectedFile.toString()))
    }

    FileDialog {
        id: importSubtitleFileDialog
        title: qsTr("Nhập phụ đề SRT / VTT")
        nameFilters: [qsTr("Phụ đề (*.srt *.vtt)"), qsTr("Tất cả tệp (*.*)")]
        onAccepted: dubbing.importSubtitles(
                        AppController.files.urlToLocalPath(selectedFile.toString()),
                        root.pendingSubtitleUsesTarget)
    }

    FileDialog {
        id: exportSubtitleFileDialog
        title: qsTr("Xuất phụ đề SRT")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Phụ đề SubRip (*.srt)")]
        onAccepted: dubbing.exportSubtitles(
                        AppController.files.urlToLocalPath(selectedFile.toString()),
                        root.pendingSubtitleUsesTarget)
    }

    FolderDialog {
        id: packageExportFolderDialog
        title: qsTr("Chọn thư mục xuất gói bàn giao")
        onAccepted: dubbing.exportPackage(AppController.files.urlToLocalPath(selectedFolder.toString()))
    }

    FolderDialog {
        id: capCutDraftFolderDialog
        title: qsTr("Chọn thư mục lưu bản nháp CapCut")
        onAccepted: dubbing.exportCapCutDraft(AppController.files.urlToLocalPath(selectedFolder.toString()))
    }

    ConfirmationDialog {
        id: deleteHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Xóa dự án khỏi lịch sử")
        messageText: qsTr("Tệp dự án sẽ không bị xóa trên ổ đĩa; chỉ có mục trong danh sách lịch sử bị gỡ bỏ.")
        confirmText: qsTr("Xóa")
        isDestructive: true
        onConfirmed: { dubbing.deleteHistoryItem(root.pendingHistoryDeleteId); root.pendingHistoryDeleteId = "" }
        onRejected: root.pendingHistoryDeleteId = ""
    }

    ConfirmationDialog {
        id: clearHistoryDialog
        parent: Overlay.overlay
        titleText: qsTr("Xóa toàn bộ lịch sử lồng tiếng")
        messageText: qsTr("Tất cả các mục lịch sử dự án lồng tiếng đã lưu sẽ bị gỡ bỏ.")
        confirmText: qsTr("Xóa tất cả")
        isDestructive: true
        onConfirmed: dubbing.clearHistory()
    }

    Dialog {
        id: interruptedWorkflowDialog
        parent: Overlay.overlay
        modal: true
        title: qsTr("Quy trình bị gián đoạn")
        width: Math.min(440, parent ? parent.width - 32 : 440)
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        standardButtons: Dialog.NoButton

        contentItem: ColumnLayout {
            spacing: Theme.paddingMedium

            Text {
                Layout.fillWidth: true
                text: qsTr("Một quy trình lồng tiếng trước đó đã dừng đột ngột. Bạn có thể tiếp tục từ node hoàn thành gần nhất hoặc hủy bỏ.")
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: dubbing.workflowRecovery.activeNodeId !== ""
                text: qsTr("Node hoạt động gần nhất: %1").arg(dubbing.workflowRecovery.activeNodeId)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSmall
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.paddingSmall
                spacing: Theme.paddingSmall

                PrimaryButton {
                    text: qsTr("Hủy bỏ")
                    quiet: true
                    Layout.fillWidth: true
                    onClicked: {
                        if (dubbing.discardInterruptedWorkflow()) interruptedWorkflowDialog.close()
                    }
                }

                PrimaryButton {
                    text: qsTr("Tiếp tục")
                    Layout.fillWidth: true
                    onClicked: {
                        if (dubbing.resumeInterruptedWorkflow()) interruptedWorkflowDialog.close()
                    }
                }
            }
        }
    }

    Connections {
        target: dubbing
        function onWorkflowChanged() {
            if (dubbing.workflowRecoveryAvailable && !interruptedWorkflowDialog.visible)
                interruptedWorkflowDialog.open()
        }
    }

    WorkflowPipelineDialog {
        id: workflowDialog
        nodes: dubbing.workflowStages
        workflowReady: dubbing.workflowReady
        statusText: dubbing.workflowStatusText
        allowIncompleteRun: dubbing.dubbingQuality === "custom"
        busy: dubbing.processing
        progress: dubbing.progress / 100.0
        progressAvailable: dubbing.progressAvailable
        dialogTitle: qsTr("Sơ đồ quy trình Dubbing")
        reviewWaiting: dubbing.workflowWaitingForInput
        description: qsTr("Duyệt 8 giai đoạn sản xuất: Import, Normalize, Isolator, Transcribe, Alignment/Subtitle, Translate, TTS, và Export.")
        onPrepareRequested: dubbing.prepareWorkflow()
        onRunRequested: automaticPreflightDialog.openPreflight()
        onApproveRequested: dubbing.approveWorkflowReview()
        onRejectRequested: dubbing.rejectWorkflowReview(qsTr("Đã từ chối từ duyệt quy trình"))
        nodeConfigurations: dubbing.workflowNodeConfigurations
        nodeConfigurationApplier: function(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles) {
            var accepted = dubbing.setWorkflowNodeModel(
                        nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
            return { accepted: accepted,
                     error: accepted ? "" : dubbing.lastError }
        }
        nodeColabConfigurationApplier: function(nodeId, familyId, openNotebook) {
            if (nodeId === "adaptive-llm") {
                return { accepted: false,
                         error: qsTr("Chọn tuyến Adaptive LLM trong cài đặt tác vụ của nó.") }
            }
            var accepted = dubbing.selectWorkflowColabModel(nodeId, familyId)
            if (accepted && openNotebook) {
                var notebook = dubbing.colabNotebookForNode(nodeId, familyId)
                if (notebook !== "")
                    Qt.openUrlExternally(ColabNotebookUrls.forNotebookFile(notebook))
            }
            if (accepted)
                Qt.callLater(function() { root.openColabSetupForNode(nodeId) })
            return { accepted: accepted,
                     error: accepted ? "" : dubbing.lastError }
        }
    }

    WorkflowNodeModelDialog {
        id: nodeModelDialog
        nodes: dubbing.workflowNodes
        nodeConfigurations: dubbing.workflowNodeConfigurations
        configurationApplier: function(nodeId, familyId, runtimeId, runtimeVersion, selectedFiles) {
            if (nodeId === "adaptive-llm") {
                adaptiveLlmController.saveConfigurationSelection(
                    familyId, runtimeId, runtimeVersion, selectedFiles)
                qualityDialog.localModelConfigured(
                    familyId, runtimeId, runtimeVersion, selectedFiles)
                return { accepted: true, error: "" }
            } else {
                var accepted = dubbing.setWorkflowNodeModel(
                    nodeId, familyId, runtimeId, runtimeVersion, selectedFiles)
                return { accepted: accepted,
                         error: accepted ? "" : dubbing.lastError }
            }
        }
        colabConfigurationApplier: function(nodeId, familyId, openNotebook) {
            if (nodeId === "adaptive-llm") {
                return { accepted: false,
                         error: qsTr("Chọn tuyến Adaptive LLM trong cài đặt tác vụ của nó.") }
            }
            var accepted = dubbing.selectWorkflowColabModel(nodeId, familyId)
            if (accepted && openNotebook) {
                var notebook = dubbing.colabNotebookForNode(nodeId, familyId)
                if (notebook !== "")
                    Qt.openUrlExternally(ColabNotebookUrls.forNotebookFile(notebook))
            }
            if (accepted)
                Qt.callLater(function() { root.openColabSetupForNode(nodeId) })
            return { accepted: accepted,
                     error: accepted ? "" : dubbing.lastError }
        }
    }
}
