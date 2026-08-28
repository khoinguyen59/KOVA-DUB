pragma Singleton
import QtQuick

QtObject {
    id: root

    readonly property QtObject player: QtObject {
        property bool playing: false
        property bool paused: false
        property int playbackDurationMs: 0
        property int playbackPositionMs: 0
        property string currentPath: ""
        signal playbackFinished()
        signal positionChanged()

        function playFile(path) {
            if (!path || String(path).length === 0)
                return
            currentPath = String(path)
            playbackDurationMs = 12000
            playbackPositionMs = 0
            paused = false
            playing = true
        }
        function playSeparationStem(kind, path) { playFile(path) }
        function stop() { playing = false; paused = false; playbackPositionMs = 0 }
        function pause() { if (playing) { paused = true; playing = false } }
        function resume() { if (paused) { paused = false; playing = true } }
        function seekTo(positionMs) { playbackPositionMs = Math.max(0, positionMs || 0); positionChanged() }
    }

    readonly property QtObject files: QtObject {
        function urlToLocalPath(value) {
            var text = String(value || "")
            return text.indexOf("file:/") === 0 ? text.replace(/^file:\/+/i, "") : text
        }
    }

    readonly property QtObject catalog: QtObject {
        function languageSet() {
            return [
                { code: "vi", id: "vi", name: qsTr("Vietnamese") },
                { code: "en", id: "en", name: qsTr("English") },
                { code: "ja", id: "ja", name: qsTr("Japanese") }
            ]
        }
    }

    readonly property QtObject settings: QtObject {
        property bool remoteFirstMode: false
        property int windowWidth: 1280
        property int windowHeight: 720
        property int windowX: 80
        property int windowY: 60
        property bool windowMaximized: false
        property bool updateCheckConsentAsked: true
        property bool onboardingComplete: true
        property string modelsPath: "C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudio/models"
        property string gatewayUrl: ""
        property bool gatewayApiKeyConfigured: false
        function modelsPathAvailableBytes() { return 0 }
        function externalMediaToolsAvailable() { return true }
        function saveWindowPlacement() {}
        function setGatewayApiKey() { gatewayApiKeyConfigured = true }
    }

    readonly property QtObject models: QtObject {
        property int version: 1
        property string modelsRoot: "C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudio/models"
        function installedAnchorModels() { return [] }
    }

    readonly property QtObject downloads: QtObject {
        readonly property var allDownloads: []
        function clearCompleted() {}
        function removeDownload() {}
    }

    readonly property QtObject logs: QtObject {
        property bool loading: false
        property string logContent: qsTr("Preview log — production QML loaded; no backend worker is running.")
        function requestLoadLogs() {}
        function readSanitizedLogFile() { return logContent }
        function clearLogFile() { logContent = "" }
    }

    readonly property QtObject updates: QtObject {
        property bool downloading: false
        property bool downloaded: false
        property bool updateAvailable: false
        property real downloadProgress: 0
        property string latestVersion: ""
        property string releaseUrl: ""
        property string errorMessage: ""
        function downloadUpdate() {}
        function installDownloadedUpdate() {}
    }

    readonly property QtObject downloadInstall: QtObject {
        signal installStatesChanged()
        function enqueueRecommendedSetup() { return true }
        function enqueueModelFile() { return true }
        function enqueueRuntime() { return true }
    }

    readonly property QtObject runtimes: QtObject {
        readonly property var allRuntimes: []
        function runtimeVersions() { return [] }
    }

    readonly property QtObject subtitleOcr: QtObject {
        property string cropPreviewUrl: ""
    }

    readonly property QtObject colabAlignmentSession: QtObject {
        property bool active: false
        property bool checking: false
        property string workerUrl: ""
        property string lastError: ""
        function connectTemporaryWorker() { active = true; return true }
        function disconnectTemporaryWorker() { active = false }
    }

    readonly property QtObject colabAlignment: QtObject {
        property bool colabActive: false
        property bool colabConnected: false
        property string model: "mms-forced-aligner-onnx"
        property string colabNotebookFile: ""
        function installedAnchorModels() { return [] }
        function connectColab() { colabConnected = true; colabActive = true; return true }
        function useLocal() { colabActive = false }
        function useColab() { colabActive = true }
    }

    readonly property QtObject voiceClonePresets: QtObject {
        function allPresets() { return [] }
        function presetsForFamily() { return [] }
        function addPreset() {}
        function updatePreset() {}
        function deletePreset() {}
    }

    readonly property QtObject voiceDesignPresets: QtObject {
        function presetsForFamily() { return [] }
        function addPreset() {}
        function updatePreset() {}
        function deletePreset() {}
    }

    readonly property QtObject dubbing: QtObject {
        id: dubbing

        property bool hasProject: true
        property string projectPath: "C:/Users/Nguyen Trong Khoi/Downloads/LAStudio-preview.json"
        property string projectName: "LA Studio Preview"
        // Use the checked-in live-walkthrough fixture so the preview verifies
        // the real VideoOutput/thumbnail path instead of rendering an empty
        // black canvas with a fabricated source path.
        property string sourceMediaPath: "C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/dubbing-live-test/dubbing_live_walkthrough.mp4"
        property string sourceMediaUrl: "file:///C:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/out/dubbing-live-test/dubbing_live_walkthrough.mp4"
        // Use the extracted first-frame fixture when the preview is still
        // waiting for Qt Multimedia. Production obtains this URL from the
        // asynchronous FFmpeg thumbnail cache in DubbingController.
        property string sourceThumbnailUrl: "file:///C:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/out/dubbing-live-test/dubbing_live_walkthrough-thumb.jpg"
        property string playbackMediaUrl: ""
        property string normalizedAudioPath: "C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudio/cache/analysis.wav"
        property string vocalsPath: "C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudio/cache/vocals.wav"
        property string backgroundPath: "C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudio/cache/background.wav"
        property string dubbedVocalPath: ""
        property string previewPath: ""
        property string exportPath: ""
        property string capCutDraftPath: ""
        property string capCutDraftWarning: ""
        property string sourceLanguage: "zh"
        property string targetLanguage: "vi"
        property var durationControl: ({ mode: "fit", minGapMs: 80 })
        property var speakers: []
        property var segments: [
            { startMs: 0, endMs: 3200, sourceText: qsTr("Preview transcript segment"), targetText: qsTr("Preview translation"), speaker: "Speaker 1" }
        ]
        property bool processing: false
        property string stage: ""
        property int progress: 0
        property bool progressAvailable: false
        property string lastError: ""

        property var mediaQueueItems: []
        property bool mediaQueueDownloading: false
        property bool mediaQueueProcessing: false
        property string mediaQueueStatus: ""
        property int mediaQueueProgress: 0
        property bool mediaDownloadCookieFileConfigured: false

        property var workflowNodes: [
            { id: "import", label: qsTr("Nguồn Media"), title: qsTr("Nguồn Media (Import)"), icon: "folder", state: "completed", completed: true, canRun: true, runReady: true, nextNodeId: "normalize", configurable: false },
            { id: "normalize", label: qsTr("Chuẩn Hóa Âm Thanh"), title: qsTr("Chuẩn Hóa Âm Thanh (Normalize)"), icon: "waveform", state: "completed", completed: true, canRun: true, runReady: true, nextNodeId: "transcribe", configurable: true, capabilityId: "normalize" },
            { id: "source-separate", label: qsTr("Tách Giọng & Nhạc Nền"), title: qsTr("Tách Giọng Nói & Nhạc Nền (Separate)"), icon: "separate", state: "ready", completed: false, canRun: true, runReady: true, nextNodeId: "transcribe", configurable: true, capabilityId: "isolation", optional: true },
            { id: "transcribe", label: qsTr("Nhận Dạng Lời Thoại"), title: qsTr("Nhận Dạng Lời Thoại (Transcribe)"), icon: "mic", state: "pending", completed: false, canRun: false, runReady: false, nextNodeId: "translate", configurable: true, capabilityId: "stt" },
            { id: "translate", label: qsTr("Dịch Thuật AI"), title: qsTr("Dịch Thuật AI (Translate)"), icon: "translate", state: "pending", completed: false, canRun: false, runReady: false, nextNodeId: "tts", configurable: true, capabilityId: "translation" },
            { id: "tts", label: qsTr("Lồng Tiếng AI"), title: qsTr("Lồng Tiếng AI (TTS)"), icon: "voice", state: "pending", completed: false, canRun: false, runReady: false, nextNodeId: "alignment-subtitle", configurable: true, capabilityId: "tts" },
            { id: "alignment-subtitle", label: qsTr("Khớp Chữ & Căn Chỉnh"), title: qsTr("Khớp Chữ & Căn Chỉnh (Align)"), icon: "align", state: "pending", completed: false, canRun: false, runReady: false, nextNodeId: "export", configurable: true, capabilityId: "alignment" },
            { id: "export", label: qsTr("Xuất Bản Thành Phẩm"), title: qsTr("Xuất Bản Thành Phẩm (Export)"), icon: "export", state: "pending", completed: false, canRun: false, runReady: false, nextNodeId: "", configurable: false }
        ]
        property var workflowStages: [
            { id: "import", label: qsTr("1. Nguồn Media"), icon: "folder", status: "completed", completed: true, active: false, actionNodeId: "import", canRunDirectly: true },
            { id: "normalize", label: qsTr("2. Chuẩn Hóa Âm Thanh"), icon: "waveform", status: "completed", completed: true, active: false, actionNodeId: "normalize", canRunDirectly: true },
            { id: "isolator", label: qsTr("3. Tách Giọng Nói & Nhạc Nền"), icon: "separate", status: "ready", completed: false, active: true, actionNodeId: "source-separate", canRunDirectly: true },
            { id: "transcribe", label: qsTr("4. Nhận Dạng Lời Thoại"), icon: "mic", status: "pending", completed: false, active: false, actionNodeId: "transcribe", canRunDirectly: false },
            { id: "translate", label: qsTr("5. Dịch Thuật AI"), icon: "translate", status: "pending", completed: false, active: false, actionNodeId: "translate", canRunDirectly: false },
            { id: "tts", label: qsTr("6. Lồng Tiếng AI"), icon: "voice", status: "pending", completed: false, active: false, actionNodeId: "synthesize", canRunDirectly: false },
            { id: "alignment-subtitle", label: qsTr("7. Khớp Chữ & Căn Chỉnh"), icon: "align", status: "pending", completed: false, active: false, actionNodeId: "fit-timing", canRunDirectly: false },
            { id: "export", label: qsTr("8. Xuất Bản Thành Phẩm"), icon: "export", status: "pending", completed: false, active: false, actionNodeId: "export", canRunDirectly: false }
        ]
        property var workflowNodeConfigurations: ({})
        property var transcriptConfiguration: ({ transcriptSource: "stt", fusionPolicy: "prefer-stt" })
        property var audioMixConfiguration: ({ originalGainPercent: 0, dubbedGainPercent: 100 })
        property int unresolvedTranscriptConflictCount: 0
        property bool subtitleOcrProcessing: false
        property bool sttCanRunAlongsideSubtitleOcr: true
        property bool subtitleOcrCanRunAlongsideStt: true
        property var dubbingOcrRoi: ({ x: 0.10, y: 0.72, width: 0.80, height: 0.22 })
        property bool dubbingOcrRoiVisible: false
        property var subtitleConfiguration: ({ textSource: "target", burnIn: true, style: ({}) })
        property var timingConfiguration: ({ mode: "fit", minimumGapMs: 80 })
        property var timingConflicts: []
        property var timingResolutionPreview: ({})
        property bool timingUndoAvailable: false
        property bool workflowReady: true
        property string workflowStatusText: qsTr("Preview workflow ready")
        property string workflowId: "dubbing-standard"
        property int workflowVersion: 1
        property bool workflowGraphValid: true
        property string workflowRunId: ""
        property string workflowNodeRunId: ""
        property bool workflowWaitingForInput: false
        property var workflowReviewRequest: ({})
        property bool workflowRecoveryAvailable: false
        property var workflowRecovery: ({ activeNodeId: "" })
        property string workflowMode: "step"
        property bool dubbingEntryGateActive: false
        property string savedDubbingEntryMode: "step"
        property string currentStepId: "source-separate"
        property var currentStepOutput: ({})
        property string lastCompletedStepId: "normalize"
        property var history: []
        property bool translationFixing: false
        property int translationFixProgress: 0
        property string translationFixStatus: ""
        property var translationFixConfiguration: ({ provider: "lmstudio" })
        property int translationFixCandidateCount: 0
        property string dubbingQuality: "adaptive"
        property string adaptiveProvider: ""
        property bool adaptiveReady: true
        property string adaptiveStatusText: qsTr("Ready")
        property bool customReady: false
        property string customStatusText: qsTr("Not configured")
        property bool settingsLocked: false
        property bool automaticSetupActive: false
        property string automaticStatusText: qsTr("Sẵn sàng chạy node")
        property var automaticEvents: []
        property var automaticPreflight: ({})
        property var ttsVoiceOptions: []
        property string selectedTtsVoiceId: ""
        property bool ttsVoiceSelectionValid: true
        property string ttsVoiceSelectionError: ""
        property var colabSetupStages: [
            { id: "source-separate", title: "Isolator", modelId: "sherpa-onnx-spleeter-2stems-fp16", capability: "voice-isolation", notebookFile: "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb", selectedForDirectColab: true, verified: false, requiredForCurrentTranscriptAction: false, diagnostic: "" },
            { id: "transcribe", title: "STT", modelId: "whisper.cpp", capability: "stt", notebookFile: "LA_STUDIO_STT_WHISPER_GPU.ipynb", selectedForDirectColab: true, verified: false, requiredForCurrentTranscriptAction: true, diagnostic: "" },
            { id: "subtitle-ocr", title: "Subtitle OCR", modelId: "paddleocr-v5", capability: "subtitle-ocr", notebookFile: "LA_STUDIO_SUBTITLE_OCR_PP_OCRV5_GPU.ipynb", selectedForDirectColab: false, verified: false, requiredForCurrentTranscriptAction: false, diagnostic: "" },
            { id: "translate", title: "Translation", modelId: "hy-mt2-1.8b", capability: "translation", notebookFile: "LA_STUDIO_TRANSLATION_HY_MT2_1_8B_GPU.ipynb", selectedForDirectColab: false, verified: false, requiredForCurrentTranscriptAction: false, diagnostic: "" },
            { id: "synthesize", title: "TTS", modelId: "omnivoice", capability: "tts", notebookFile: "LA_STUDIO_TTS_OMNIVOICE_GPU.ipynb", selectedForDirectColab: true, verified: false, requiredForCurrentTranscriptAction: false, diagnostic: "" },
            { id: "alignment", title: "Alignment", modelId: "mms-forced-aligner-onnx", capability: "alignment", notebookFile: "LA_STUDIO_ALIGNMENT_MMS_ONNX_GPU.ipynb", selectedForDirectColab: false, verified: false, requiredForCurrentTranscriptAction: false, diagnostic: "" }
        ]
        property bool colabSetupChecking: false
        property string colabSetupSummary: ""

        signal projectChanged()
        signal previewChanged()
        signal errorChanged()
        signal exportChanged()
        signal mediaQueueChanged()
        signal workflowChanged()
        signal translationFixChanged()
        signal cloneVoiceSelectionChanged()
        signal colabSetupChanged()
        signal timingResolutionChanged()

        function beginDubbingEntry() {}
        function chooseDubbingEntryMode(mode) { savedDubbingEntryMode = mode || "step"; workflowMode = savedDubbingEntryMode; return true }
        function reopenDubbingEntryGate() { dubbingEntryGateActive = true }
        function prepareWorkflow() { workflowReady = true; workflowChanged() }
        function runWorkflowNode(nodeId) { currentStepId = nodeId || currentStepId; workflowChanged(); return true }
        function runWorkflow() { return true }
        function approveAutomaticPreflight() { return true }
        function startAutomaticWorkflow() { return true }
        function pauseAutomaticWorkflow() { processing = false; workflowMode = "paused"; processingChanged() }
        function startStepByStep() { workflowMode = "step" }
        function runCurrentStep() { return runWorkflowNode(currentStepId) }
        function rerunStep(stepId) { return runWorkflowNode(stepId) }
        function approveWorkflowReview() { workflowWaitingForInput = false; return true }
        function rejectWorkflowReview() { workflowWaitingForInput = false; return true }
        function resumeInterruptedWorkflow() { return true }
        function discardInterruptedWorkflow() { workflowRecoveryAvailable = false; return true }
        function createAutoProject() { hasProject = true; projectChanged(); return true }
        function newProject(path) { hasProject = true; if (path) projectPath = path; projectChanged(); return true }
        function openProject(path) { hasProject = true; if (path) projectPath = path; projectChanged(); return true }
        function saveProject() { return true }
        function saveProjectAs(path) { if (path) projectPath = path; projectChanged(); return true }
        function closeProject() { hasProject = false; projectChanged() }
        function importMedia(path) { if (path) sourceMediaPath = path; hasProject = true; projectChanged(); return true }
        function refreshHistory() {}
        function deleteHistoryItem() { return true }
        function clearHistory() {}
        function enqueueMediaLinks() { return 0 }
        function setMediaDownloadCookieFile() { mediaDownloadCookieFileConfigured = true; return true }
        function clearMediaDownloadCookieFile() { mediaDownloadCookieFileConfigured = false }
        function enqueueMediaFiles() { return 0 }
        function setMediaQueueItemSelected() { return true }
        function retryMediaQueueItem() { return true }
        function removeMediaQueueItem() { return true }
        function clearCompletedMediaQueue() {}
        function startMediaQueue() { return true }
        function cancelMediaQueue() { mediaQueueDownloading = false; mediaQueueProcessing = false }
        function transcribeSource() { return true }
        function runSpeechToTextIndependently() { return true }
        function runSubtitleOcrIndependently() { return true }
        function reconcileTranscriptSources() { return true }
        function setAudioMixLevels(originalPercent, dubbedPercent) {
            audioMixConfiguration = { originalGainPercent: originalPercent, dubbedGainPercent: dubbedPercent }
            return true
        }
        function translateSource() { return true }
        function generateAudio() { return true }
        function cancelProcessing() { processing = false; processingChanged() }
        function renderPreview(path) { previewPath = path || previewPath; previewChanged(); return true }
        function exportMedia(path) { exportPath = path || exportPath; exportChanged(); return true }
        function exportFinalMedia() { return true }
        function exportAudioStem() { return true }
        function exportSubtitles() { return true }
        function importSubtitles() { return true }
        function workflowArtifactSpec(nodeId) {
            var id = String(nodeId || "transcribe")
            if (id === "source-separate" || id === "isolator" || id === "separate")
                return { nodeId: "source-separate", title: qsTr("Voice isolation"), description: qsTr("Preview artifact handoff"), expectedFiles: ["vocals.flac", "background.flac"], allowedExtensions: [".flac"], multiple: true }
            if (id === "translate")
                return { nodeId: "translate", title: qsTr("Translated subtitles"), description: qsTr("Preview artifact handoff"), expectedFiles: ["translated.srt"], allowedExtensions: [".srt", ".vtt", ".ass", ".ssa", ".txt", ".md", ".markdown"], multiple: false }
            return { nodeId: id === "ocr" ? "ocr" : "stt", title: id === "ocr" ? qsTr("Subtitle OCR transcript") : qsTr("STT transcript"), description: qsTr("Preview artifact handoff"), expectedFiles: [id === "ocr" ? "ocr.srt" : "transcript.srt"], allowedExtensions: [".srt", ".vtt", ".ass", ".ssa", ".txt", ".md", ".markdown"], multiple: false }
        }
        function workflowArtifactSpecsForStage(nodeId) { return [workflowArtifactSpec(nodeId)] }
        function canOverrideRunningWorkflowArtifact() { return false }
        function workflowArtifactHandoffStatus() { return ({}) }
        function importWorkflowArtifactFiles() { return true }
        function setSubtitleStyle() { return true }
        function setSubtitleTextSource() { return true }
        function setSubtitleBurnIn() { return true }
        function previewTimingResolution() { return ({}) }
        function applyTimingResolution() { return true }
        function undoTimingResolution() { return true }
        function setIntentionalTimingOverlap() { return true }
        function exportPackage() { return true }
        function exportCapCutDraft() { return true }
        function replaceTranscriptSegments() { return true }
        function resolveTranscriptConflict() { return true }
        function resolveAllTranscriptConflicts() { return true }
        function setTranscriptFusionPolicy() { return true }
        function transcriptConflictAiAvailability() { return ({ available: false }) }
        function requestTranscriptConflictAiSuggestion() { return false }
        function acceptTranscriptConflictAiSuggestion() { return true }
        function rejectTranscriptConflictAiSuggestion() { return true }
        function addSegment() {}
        function updateSegment() {}
        function removeSegment() {}
        function addSpeaker() {}
        function setSpeakerVoice() {}
        function clearError() { lastError = "" }
        function resetStandardWorkflowNodeModels() {}
        function defaultWorkflowModelFamily() { return "preview" }
        function setWorkflowNodeModel() { return true }
        function loadWorkflowNodeModel() { return true }
        function unloadWorkflowNodeModel() { return true }
        function reloadWorkflowNodeModel() { return true }
        function setWorkflowNodeParameters() { return true }
        function setDubbingOcrRoi() { return true }
        function presetDubbingOcrLowerRegion() { return true }
        function resetDubbingOcrRoi() { return true }
        function previewDubbingOcrCrop() { return true }
        function colabModelOptionsForNode(nodeId) {
            for (var i = 0; i < colabSetupStages.length; ++i) {
                if (colabSetupStages[i].id === nodeId)
                    return [{ modelId: colabSetupStages[i].modelId, displayName: colabSetupStages[i].modelId }]
            }
            return []
        }
        function defaultColabModelForNode() { return "preview" }
        function colabNotebookForNode() { return "" }
        function selectWorkflowColabModel() { return true }
        function connectUnifiedWorkflowColab() { return true }
        function connectWorkflowColabStage() { return true }
        function checkWorkflowColabStage() { return true }
        function disconnectWorkflowColabStage() { return true }
        function validateAllWorkflowColabStages() { return true }
        function fixTranslations() { return true }
        function fixTranslationSegment() { return true }
        function translationSegmentNeedsFix() { return false }
        function cancelTranslationFix() { translationFixing = false }
        function selectTtsVoice(id) { selectedTtsVoiceId = id || ""; return true }
        function refreshCloneVoicePresets() {}
        function defaultExportPath() { return projectPath.replace(/\.json$/i, "-dubbed.mp4") }
        function openCapCutDraft() { return true }
    }

    readonly property QtObject workflows: QtObject {
        property int sessionCount: 0
        property int runningCount: 0
        readonly property var activeSessions: []
        readonly property var activeWorkflows: []
        signal openRequested(string routeId)
        function openStudioRoute() {}
        function openWorkflow() {}
        function stopWorkflow() {}
    }

    property var currentError: ({})
    property int pendingErrorCount: 0
    property string errorMessage: ""
    property var errorNotifications: []
    function explainError(message, source) {
        var text = String(message || "")
        return {
            title: source ? String(source) : qsTr("Tác vụ chưa hoàn tất"),
            summary: text || qsTr("Không có lỗi đang chờ xử lý."),
            guidance: qsTr("Đây là preview giao diện; không có backend hay worker nào được khởi chạy."),
            actionLabel: "",
            actionRoute: ""
        }
    }
    function clearError() { currentError = ({}); errorMessage = ""; pendingErrorCount = 0 }
    function copyToClipboard() {}
    function createProblemReport() { return "" }
}
