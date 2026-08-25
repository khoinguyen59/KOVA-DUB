cmake_minimum_required(VERSION 3.21)

include(cmake/RuntimeAbi.cmake)

# This is the single source of truth for the application version. Release
# automation may override it with -DLASTUDIO_VERSION=<major.minor.release.build>.
# Internal builds use four single-digit fields. Carry at 9 rather than extending
# one field: 0.0.0.1 ... 0.0.0.9, then 0.0.1.0.
set(LASTUDIO_VERSION "0.0.8.1" CACHE STRING "LA Studio application version")
if(NOT LASTUDIO_VERSION MATCHES "^[0-9]\\.[0-9]\\.[0-9]\\.[0-9]$")
    message(FATAL_ERROR "LASTUDIO_VERSION must use four single digits (0-9), for example 0.0.1.0; got '${LASTUDIO_VERSION}'")
endif()
set(LASTUDIO_RELEASE_SUFFIX "" CACHE STRING
    "Optional SemVer prerelease suffix (for example, -beta.1)")
if(NOT LASTUDIO_RELEASE_SUFFIX MATCHES "^(-(alpha|beta|rc)\\.[1-9][0-9]*)?$")
    message(FATAL_ERROR
        "LASTUDIO_RELEASE_SUFFIX must be empty or -alpha.N, -beta.N, or -rc.N; got '${LASTUDIO_RELEASE_SUFFIX}'")
endif()

project(LAStudio VERSION ${LASTUDIO_VERSION} LANGUAGES CXX)

set(LLAMA_CPP_SOURCE_DIR "" CACHE PATH
    "llama.cpp checkout used only for public C API headers at build time")
if(NOT LLAMA_CPP_SOURCE_DIR)
    set(_llama_cpp_candidates
        "$ENV{LLAMA_CPP_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/.deps/llama.cpp"
        "${CMAKE_SOURCE_DIR}/../llama.cpp"
    )
    foreach(_candidate IN LISTS _llama_cpp_candidates)
        if(EXISTS "${_candidate}/include/llama.h" AND EXISTS "${_candidate}/ggml/include/ggml.h")
            set(LLAMA_CPP_SOURCE_DIR "${_candidate}" CACHE PATH
                "llama.cpp checkout used only for public C API headers at build time" FORCE)
            break()
        endif()
    endforeach()
    unset(_llama_cpp_candidates)
    unset(_candidate)
endif()
if(NOT EXISTS "${LLAMA_CPP_SOURCE_DIR}/include/llama.h" OR
   NOT EXISTS "${LLAMA_CPP_SOURCE_DIR}/ggml/include/ggml.h")
    message(FATAL_ERROR
        "LLAMA_CPP_SOURCE_DIR must point to a llama.cpp ${LASTUDIO_LLAMA_CPP_REF} checkout compatible with the packaged runtime. "
        "Pass -DLLAMA_CPP_SOURCE_DIR=<path> or set the LLAMA_CPP_SOURCE_DIR environment variable.")
endif()

set(LASTUDIO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GLOB_RECURSE LASTUDIO_EXAMPLE_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "examples/*.json"
    "examples/*.md"
)
file(GLOB LASTUDIO_LANGUAGE_SET_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}"
    "data/language-sets/*.json"
)
file(MAKE_DIRECTORY "${LASTUDIO_GENERATED_DIR}/lastudio")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/AppVersion.h.in"
    "${LASTUDIO_GENERATED_DIR}/lastudio/AppVersion.h"
    @ONLY
)
option(LASTUDIO_PORTABLE_INTERNAL_LAYOUT
    "Install an internal portable package with executables at its root" OFF)
set(LASTUDIO_EXECUTABLE_BASENAME "LA-Studio-${LASTUDIO_VERSION}")
set(LASTUDIO_EXECUTABLE_FILENAME "${LASTUDIO_EXECUTABLE_BASENAME}.exe")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/RuntimeAbi.h.in"
    "${LASTUDIO_GENERATED_DIR}/lastudio/RuntimeAbi.h"
    @ONLY
)

include(CTest)
include(cmake/CompilerOptions.cmake)
message(STATUS "MSVC value: ${MSVC}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")

include(cmake/Dependencies.cmake)

# Vietnamese text normalization is kept as an isolated target so it can be
# extracted into a standalone repository/package later.
add_subdirectory(src/textnorm)

# Require per-directory qmldir handling for Qt QML module sources.
qt_policy(SET QTP0004 NEW)

set(LASTUDIO_SOURCES
    src/main.cpp
    src/core/utils/Logger.cpp
    src/core/utils/CrashHandler.cpp
    src/core/storage/PathUtils.cpp
    src/core/services/MediaRuntimeLocator.cpp
    src/core/utils/VoiceCloningUtils.cpp
    src/core/hardware/InferenceThreadPolicy.cpp
    src/inference/WorkerThreadHost.cpp
    src/runtimehost/service/RuntimeHostClient.cpp
    src/runtimehost/protocol/RuntimeHostProtocol.cpp
    src/runtimehost/protocol/RuntimeHostSharedBuffer.cpp
    src/runtimehost/service/RuntimeHostManager.cpp
    src/runtimehost/hosted_backends/HostedOmnivoiceBackend.cpp
    src/runtimehost/hosted_backends/HostedWhisperBackend.cpp
    src/runtimehost/hosted_backends/HostedLlamaTranslationBackend.cpp
    src/translation/engine/TranslationEngine.cpp
    src/translation/engine/TranslationEngineInstance.cpp
    src/translation/engine/TranslationWorker.cpp
    src/translation/runners/GatewayTranslationRunner.cpp
    src/translation/runners/ColabTranslationRunner.cpp
    src/subtitles/ocr/ColabSubtitleOcrRunner.cpp
    src/translation/backends/TranslationBackendFactory.cpp
    src/translation/backends/crisp/CrispTranslationBackend.cpp
    src/translation/backends/llama/LlamaTranslationBackend.cpp
    src/core/storage/Settings.cpp
    src/core/storage/SecureCredentialStore.cpp
    src/remote/gateway/ExecutionProvider.cpp
    src/remote/colab/ColabSession.cpp
    src/remote/gateway/GatewayClient.cpp
    src/remote/colab/ColabWorkerClient.cpp
    src/remote/gateway/GatewayModelCatalog.cpp
    src/remote/colab/ColabCapabilityCatalog.cpp
    src/api/ApiServerService.cpp
    src/core/models/HFHubClient.cpp
    src/core/models/DownloadManager.cpp
    src/core/models/ModelManager.cpp
    src/core/models/CatalogManager.cpp
    src/core/models/RegistryManager.cpp
    src/core/models/RuntimeManager.cpp
    src/core/hardware/HardwareManager.cpp
    src/core/utils/QmlLogger.cpp
    src/core/services/StudioCapabilityRegistry.cpp
    src/core/storage/StudioSelectionRepository.cpp
    src/core/models/CapabilityFamilyModel.cpp
    src/core/utils/LogViewService.cpp
    src/core/storage/CacheLifecycleService.cpp

    src/audio/io/WavIO.cpp
    src/audio/io/AudioFileDecoder.cpp
    src/audio/player/AudioRecorder.cpp
    src/audio/player/AudioPlayer.cpp
    src/audio/io/WaveformProvider.cpp
    src/stt/engine/SttEngine.cpp
    src/stt/engine/SttEngineInstance.cpp
    src/stt/engine/SttWorker.cpp
    src/stt/runners/ColabSttRunner.cpp
    src/stt/runners/GatewaySttRunner.cpp
    src/stt/backends/SttBackendFactory.cpp
    src/stt/backends/whisper/WhisperSttBackend.cpp
    src/stt/backends/qwen/QwenSttBackend.cpp
    src/stt/backends/nemotron/NemotronSttBackend.cpp
    src/tts/engine/TtsEngine.cpp
    src/tts/engine/TtsEngineInstance.cpp
    src/tts/engine/TtsWorker.cpp
    src/tts/runners/GatewayTtsRunner.cpp
    src/tts/runners/ColabTtsRunner.cpp
    src/tts/runners/ColabVoiceCloneRunner.cpp
    src/tts/runners/ColabVoiceDesignRunner.cpp
    src/alignment/ColabAlignmentRunner.cpp
    src/separation/runners/ColabSeparationRunner.cpp
    src/tts/processing/TextNormalizationPolicy.cpp
    src/tts/processing/TtsTextPreprocessor.cpp
    src/tts/processing/TtsRequestValidator.cpp
    src/tts/backends/TtsBackendFactory.cpp
    src/tts/backends/kokoro/KokoroBackend.cpp
    src/tts/backends/kokoro/KokoroVietnameseBackend.cpp
    src/tts/backends/qwen/Qwen3Backend.cpp
    src/tts/backends/voxcpm/VoxCpm2Backend.cpp
    src/tts/backends/vibevoice/VibevoiceBackend.cpp
    src/tts/backends/vieneu/VieneuTtsBackend.cpp
    src/tts/backends/omnivoice/OmnivoiceBackend.cpp
    src/controllers/app/AppController.cpp
    src/controllers/shared/AudioPreviewService.cpp
    src/controllers/shared/HistoryService.cpp
    src/controllers/shared/HistoryRepository.cpp
    src/controllers/shared/VoiceClonePresetService.cpp
    src/controllers/shared/VoiceDesignPresetService.cpp
    src/controllers/models/DownloadInstallService.cpp
    src/controllers/models/RemoteModelCatalogController.cpp
    src/controllers/alignment/AlignmentExecutionService.cpp
    src/controllers/alignment/ColabAlignmentController.cpp
    src/controllers/separation/ColabVoiceIsolatorController.cpp
    src/controllers/separation/VoiceCloneReferenceIsolatorController.cpp
    src/dubbing/fusion/AlignmentRefinementService.cpp
    src/dubbing/fusion/DubbingSegmentNormalizer.cpp
    src/controllers/alignment/AlignmentTranscriptMatcher.cpp
    src/alignment/CrispAlignmentInterface.cpp
    src/translation/interfaces/CrispTranslationInterface.cpp
    src/translation/interfaces/LlamaTranslationInterface.cpp
    src/llm/LlmChatEngine.cpp
    src/llm/ColabChatRunner.cpp
    src/controllers/llm/LlmChatModelSession.cpp
    src/controllers/llm/LlmChatController.cpp
    src/controllers/models/ModelsPathMigrator.cpp
    src/controllers/models/ModelsPathMigrationService.cpp
    src/controllers/shared/FileAccessService.cpp
    src/controllers/shared/AppUpdateService.cpp
    src/controllers/shared/ExampleManager.cpp
    src/controllers/alignment/AlignmentModelSession.cpp
    src/controllers/translation/TranslationModelSession.cpp
    src/translation/services/TranslationProject.cpp
    src/translation/services/TranslationService.cpp
    src/controllers/translation/TranslationController.cpp
    src/controllers/separation/VoiceIsolatorModelSession.cpp
    src/controllers/app/StudioActions.cpp
    src/controllers/models/ModelLifecycleController.cpp
    src/controllers/stt/SttModelSession.cpp
    src/controllers/tts/TtsSharedModelSession.cpp
    src/controllers/models/ModelSessionRegistry.cpp
    src/controllers/app/StudioSessionViewModel.cpp
    src/controllers/models/StudioConfigurationResolver.cpp
    src/controllers/models/CapabilitySettingsSchema.cpp
    src/controllers/stt/SttSessionController.cpp
    src/controllers/tts/GatewayTtsController.cpp
    src/controllers/tts/ColabTtsController.cpp
    src/controllers/tts/ColabVoiceCloneController.cpp
    src/controllers/tts/ColabVoiceDesignController.cpp
    src/controllers/tts/SubtitleVoiceController.cpp
    src/controllers/stt/SttAudioDecoder.cpp
    src/dubbing/project/DubbingProject.cpp
    src/dubbing/fusion/DubbingTranscriptFusionService.cpp
    src/dubbing/exporters/DubbingSubtitleService.cpp
    src/dubbing/exporters/CapCutDraftExporter.cpp
    src/dubbing/timing/EspeakNgPhonemizer.cpp
    src/dubbing/timing/DubbingDuration.cpp
    src/dubbing/audio/DubbingVoiceReferenceSelector.cpp
    src/dubbing/timing/DubbingTimingProfile.cpp
    src/dubbing/timing/DubbingTimingService.cpp
    src/controllers/dubbing/DubbingController.cpp
    src/dubbing/audio/AudioTimelineMixer.cpp
    src/subtitles/cues/SrtTimelineParser.cpp
    src/subtitles/ocr/SubtitleOcrPipeline.cpp
    src/subtitles/locators/SubtitleOcrRuntimeLocator.cpp
    src/subtitles/locators/PaddleOcrRuntimeLocator.cpp
    src/subtitles/ocr/SubtitleOcrRuntimeService.cpp
    src/controllers/subtitles/SubtitleOcrController.cpp
    src/tts/pipeline/SubtitleSmartFitPlanner.cpp
    src/tts/pipeline/TimedSpeechPipeline.cpp
    src/audio/AudioTimelineRenderer.cpp
    src/controllers/dubbing/DubbingJobRunner.cpp
    src/controllers/dubbing/DubbingRunCoordinator.cpp
    src/controllers/dubbing/SourceSeparationConfigurationResolver.cpp
    src/controllers/dubbing/DubbingTranscriptionJob.cpp
    src/controllers/dubbing/DubbingSynthesisJob.cpp
    src/controllers/dubbing/DubbingExportJob.cpp
    src/controllers/dubbing/DubbingTranslationJob.cpp
    src/controllers/dubbing/DubbingTranslationFixService.cpp
    src/controllers/dubbing/services/DubbingProjectLifecycleService.cpp
    src/controllers/dubbing/services/DubbingTranscriptService.cpp
    src/controllers/dubbing/services/DubbingColabCoordinatorService.cpp
    src/controllers/dubbing/services/DubbingMediaQueueManager.cpp
    src/controllers/subtitles/services/SubtitleOcrRoiService.cpp
    src/controllers/subtitles/services/SubtitleOcrExportService.cpp
    src/controllers/models/services/ModelDownloadWorkerService.cpp
    src/separation/io/SeparationAudioIO.cpp
    src/separation/engine/SourceSeparationService.cpp
    src/separation/engine/SeparationWorker.cpp
    src/separation/backends/SeparationBackendFactory.cpp
    src/separation/backends/sherpa/SherpaOnnxRuntime.cpp
    src/separation/backends/sherpa/SherpaOnnxSeparationBackend.cpp
    src/controllers/separation/VoiceIsolatorController.cpp
    src/dubbing/audio/DubbingTranslationService.cpp
    src/dubbing/media/MediaToolService.cpp
    src/dubbing/media/AtomicMediaCommit.cpp
    src/dubbing/media/MediaIngestService.cpp
    src/dubbing/media/RemoteMediaImportService.cpp
    src/dubbing/media/DouyinBrowserSessionService.cpp
    src/controllers/app/WorkflowActivityManager.cpp
    src/workflows/registry/WorkflowTypes.cpp
    src/workflows/session/WorkflowSession.cpp
    src/workflows/graph/WorkflowGraph.cpp
    src/dubbing/workflow/DubbingWorkflowDefinition.cpp
    src/workflows/session/WorkflowArtifact.cpp
    src/workflows/session/WorkflowRun.cpp
    src/workflows/graph/WorkflowRunJournal.cpp
    src/workflows/session/WorkflowTranscript.cpp
    src/dubbing/workflow/DubbingWorkflowNodes.cpp
    src/dubbing/workflow/DubbingWorkflowAdapter.cpp
    src/workflows/registry/NodeRegistry.cpp
    src/workflows/graph/WorkflowGraphRunner.cpp
    src/alignment/AlignmentWorkflowResolver.cpp
    src/core/services/LocalizationManager.cpp
)

set(LASTUDIO_HEADERS
    src/core/utils/Logger.h
    src/core/utils/CrashHandler.h
    src/core/storage/PathUtils.h
    src/core/services/MediaRuntimeLocator.h
    src/core/utils/VoiceCloningUtils.h
    src/core/hardware/InferenceThreadPolicy.h
    src/inference/InferenceTypes.h
    src/inference/InstanceRegistry.h
    src/inference/InferenceCancellation.h
    src/inference/WorkerThreadHost.h
    src/runtimehost/service/RuntimeHostClient.h
    src/runtimehost/protocol/RuntimeHostSharedBuffer.h
    src/runtimehost/service/RuntimeHostManager.h
    src/runtimehost/hosted_backends/HostedOmnivoiceBackend.h
    src/runtimehost/hosted_backends/HostedWhisperBackend.h
    src/runtimehost/hosted_backends/HostedLlamaTranslationBackend.h
    src/core/storage/Settings.h
    src/core/storage/SecureCredentialStore.h
    src/remote/gateway/ExecutionProvider.h
    src/remote/colab/ColabSession.h
    src/remote/gateway/GatewayClient.h
    src/remote/colab/ColabWorkerClient.h
    src/remote/gateway/GatewayModelCatalog.h
    src/remote/colab/ColabCapabilityCatalog.h
    src/core/models/HFHubClient.h
    src/core/models/DownloadManager.h
    src/core/models/ModelManager.h
    src/core/models/CatalogManager.h
    src/core/models/RegistryManager.h
    include/runtimes/CrispKokoroInterface.h
    include/runtimes/KokoroVietnameseInterface.h
    include/runtimes/CrispVoxCpm2Interface.h
    include/runtimes/CrispVibeVoiceInterface.h
    include/runtimes/VieneuTtsInterface.h
    include/runtimes/CrispQwen3SttInterface.h
    include/runtimes/CrispNemotronSttInterface.h
    src/core/models/RuntimeManager.h
    src/core/hardware/HardwareManager.h
    src/core/utils/QmlLogger.h
    src/core/services/StudioCapabilityDescriptor.h
    src/core/services/StudioCapabilityRegistry.h
    src/core/storage/StudioSelectionRepository.h
    src/core/models/CapabilityFamilyModel.h
    src/core/utils/LogViewService.h
    src/core/storage/CacheLifecycleService.h
    src/api/ApiServerService.h

    src/audio/io/WavIO.h
    src/audio/io/AudioFileDecoder.h
    src/audio/player/AudioRecorder.h
    src/audio/player/AudioPlayer.h
    src/audio/io/WaveformProvider.h
    src/stt/engine/SttEngine.h
    src/stt/engine/SttEngineInstance.h
    src/stt/engine/SttWorker.h
    src/stt/runners/ColabSttRunner.h
    src/stt/runners/GatewaySttRunner.h
    src/stt/backends/SttBackend.h
    src/stt/backends/SttBackendFactory.h
    src/stt/backends/whisper/WhisperSttBackend.h
    src/stt/backends/qwen/QwenSttBackend.h
    src/stt/backends/nemotron/NemotronSttBackend.h
    src/tts/engine/TtsEngine.h
    src/tts/engine/TtsEngineInstance.h
    src/tts/engine/TtsWorker.h
    src/tts/runners/GatewayTtsRunner.h
    src/tts/runners/ColabTtsRunner.h
    src/tts/runners/ColabVoiceCloneRunner.h
    src/tts/runners/ColabVoiceDesignRunner.h
    src/alignment/ColabAlignmentRunner.h
    src/separation/runners/ColabSeparationRunner.h
    src/tts/processing/TextNormalizationPolicy.h
    src/tts/processing/TtsTextPreprocessor.h
    src/tts/processing/TtsRequestValidator.h
    src/tts/backends/TtsBackend.h
    src/tts/backends/TtsBackendFactory.h
    src/tts/backends/kokoro/KokoroBackend.h
    src/tts/backends/kokoro/KokoroVietnameseBackend.h
    src/tts/backends/qwen/Qwen3Backend.h
    src/tts/backends/voxcpm/VoxCpm2Backend.h
    src/tts/backends/vibevoice/VibevoiceBackend.h
    src/tts/backends/vieneu/VieneuTtsBackend.h
    src/tts/backends/omnivoice/OmnivoiceBackend.h
    src/controllers/app/AppController.h
    src/controllers/shared/AudioPreviewService.h
    src/controllers/shared/HistoryService.h
    src/controllers/shared/HistoryRepository.h
    src/controllers/shared/VoiceClonePresetService.h
    src/controllers/shared/VoiceDesignPresetService.h
    src/controllers/models/DownloadInstallService.h
    src/controllers/models/RemoteModelCatalogController.h
    src/controllers/alignment/AlignmentExecutionService.h
    src/controllers/alignment/ColabAlignmentController.h
    src/controllers/separation/ColabVoiceIsolatorController.h
    src/controllers/separation/VoiceCloneReferenceIsolatorController.h
    src/dubbing/fusion/AlignmentRefinementService.h
    src/dubbing/fusion/DubbingSegmentNormalizer.h
    src/controllers/alignment/AlignmentTranscriptMatcher.h
    include/runtimes/CrispAlignmentInterface.h
    include/runtimes/CrispTranslationInterface.h
    include/runtimes/LlamaTranslationInterface.h
    src/llm/LlmChatEngine.h
    src/llm/ColabChatRunner.h
    src/controllers/llm/LlmChatModelSession.h
    src/controllers/llm/LlmChatController.h
    src/controllers/models/ModelsPathMigrator.h
    src/controllers/models/ModelsPathMigrationService.h
    src/controllers/shared/FileAccessService.h
    src/controllers/shared/AppUpdateService.h
    src/controllers/shared/ExampleManager.h
    src/controllers/alignment/AlignmentModelSession.h
    src/controllers/translation/TranslationModelSession.h
    src/translation/services/TranslationProject.h
    src/translation/engine/TranslationEngine.h
    src/translation/engine/TranslationEngineInstance.h
    src/translation/engine/TranslationWorker.h
    src/translation/runners/GatewayTranslationRunner.h
    src/translation/runners/ColabTranslationRunner.h
    src/subtitles/ocr/ColabSubtitleOcrRunner.h
    src/translation/backends/TranslationBackend.h
    src/translation/backends/TranslationBackendFactory.h
    src/translation/backends/crisp/CrispTranslationBackend.h
    src/translation/backends/llama/LlamaTranslationBackend.h
    src/translation/services/TranslationService.h
    src/controllers/translation/TranslationController.h
    src/controllers/separation/VoiceIsolatorModelSession.h
    src/controllers/app/IStudioAction.h
    src/controllers/app/StudioActions.h
    src/controllers/models/IModelSession.h
    src/controllers/models/ModelLifecycleController.h
    src/controllers/stt/SttModelSession.h
    src/controllers/tts/TtsSharedModelSession.h
    src/controllers/models/ModelSessionRegistry.h
    src/controllers/app/StudioSessionViewModel.h
    src/controllers/models/StudioConfigurationResolver.h
    src/controllers/models/CapabilitySettingsSchema.h
    src/controllers/stt/SttSessionController.h
    src/controllers/tts/GatewayTtsController.h
    src/controllers/tts/ColabTtsController.h
    src/controllers/tts/ColabVoiceCloneController.h
    src/controllers/tts/ColabVoiceDesignController.h
    src/controllers/tts/SubtitleVoiceController.h
    src/controllers/stt/SttAudioDecoder.h
    src/dubbing/project/DubbingProject.h
    src/dubbing/exporters/DubbingSubtitleService.h
    src/dubbing/exporters/CapCutDraftExporter.h
    src/dubbing/timing/EspeakNgPhonemizer.h
    src/dubbing/timing/DubbingTimingService.h
    src/controllers/dubbing/DubbingController.h
    src/dubbing/audio/AudioTimelineMixer.h
    src/subtitles/cues/TimedTextCue.h
    src/subtitles/cues/SrtTimelineParser.h
    src/subtitles/ocr/SubtitleOcrPipeline.h
    src/subtitles/locators/SubtitleOcrRuntimeLocator.h
    src/subtitles/locators/PaddleOcrRuntimeLocator.h
    src/subtitles/ocr/SubtitleOcrRuntimeService.h
    src/controllers/subtitles/SubtitleOcrController.h
    src/tts/pipeline/SubtitleSmartFitPlanner.h
    src/tts/pipeline/TimedSpeechPipeline.h
    src/audio/AudioTimelineRenderer.h
    src/controllers/dubbing/DubbingJobRunner.h
    src/controllers/dubbing/DubbingRunCoordinator.h
    src/controllers/dubbing/SourceSeparationConfigurationResolver.h
    src/controllers/dubbing/DubbingTranscriptionJob.h
    src/controllers/dubbing/DubbingSynthesisJob.h
    src/controllers/dubbing/DubbingExportJob.h
    src/controllers/dubbing/DubbingTranslationJob.h
    src/controllers/dubbing/DubbingTranslationFixService.h
    src/controllers/dubbing/services/DubbingProjectLifecycleService.h
    src/controllers/dubbing/services/DubbingTranscriptService.h
    src/controllers/dubbing/services/DubbingColabCoordinatorService.h
    src/controllers/dubbing/services/DubbingMediaQueueManager.h
    src/controllers/subtitles/services/SubtitleOcrRoiService.h
    src/controllers/subtitles/services/SubtitleOcrExportService.h
    src/controllers/models/services/ModelDownloadWorkerService.h
    src/separation/io/SeparationTypes.h
    src/separation/io/SeparationAudioIO.h
    src/separation/engine/SourceSeparationService.h
    src/separation/engine/SeparationWorker.h
    src/separation/backends/SeparationBackend.h
    src/separation/backends/SeparationBackendFactory.h
    src/separation/backends/sherpa/SherpaOnnxRuntime.h
    src/separation/backends/sherpa/SherpaOnnxSeparationBackend.h
    src/controllers/separation/VoiceIsolatorController.h
    src/dubbing/audio/DubbingTranslationService.h
    src/dubbing/media/MediaToolService.h
    src/dubbing/media/AtomicMediaCommit.h
    src/dubbing/media/MediaIngestService.h
    src/dubbing/media/RemoteMediaImportService.h
    src/dubbing/media/DouyinBrowserSessionService.h
    src/controllers/app/WorkflowActivityManager.h
    src/workflows/registry/WorkflowTypes.h
    src/workflows/registry/IWorkflowResolver.h
    src/workflows/session/WorkflowSession.h
    src/workflows/graph/WorkflowGraph.h
    src/dubbing/workflow/DubbingWorkflowDefinition.h
    src/workflows/session/WorkflowArtifact.h
    src/workflows/session/WorkflowRun.h
    src/workflows/graph/WorkflowRunJournal.h
    src/workflows/session/WorkflowTranscript.h
    src/dubbing/workflow/DubbingWorkflowNodes.h
    src/workflows/registry/WorkflowExecutionAdapter.h
    src/dubbing/workflow/DubbingWorkflowAdapter.h
    src/workflows/registry/NodeRegistry.h
    src/workflows/graph/WorkflowGraphRunner.h
    src/alignment/AlignmentWorkflowResolver.h
    src/core/services/LocalizationManager.h
)

qt_add_executable(LAStudio ${LASTUDIO_SOURCES} ${LASTUDIO_HEADERS})

# Calling this at the top-level is safe because LAStudio is already known.
# It also avoids the deferred Qt Linguist invocation serializing an absolute
# source path with spaces into unquoted CMake code on Windows.
qt_add_translations(LAStudio IMMEDIATE_CALL
    TS_FILES
        i18n/lastudio_en.ts
        i18n/lastudio_vi.ts
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/components/shared/StudioRouteRegistry.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(LAStudio
    URI LAStudio
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/pages/DeveloperPage.qml
        qml/pages/ModelsPage.qml
        qml/pages/MyModelsPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/AlignmentPage.qml
        qml/pages/TranslationPage.qml
        qml/pages/SttPage.qml
        qml/pages/TtsPage.qml
        qml/pages/LlmPage.qml
        qml/pages/VoiceCloningPage.qml
        qml/pages/VoiceDesignPage.qml
        qml/pages/VoiceIsolatorPage.qml
        qml/pages/DubbingPage.qml
        qml/pages/MediaDownloadPage.qml
        qml/pages/SubtitleOcrPage.qml
        qml/pages/WelcomePage.qml
        qml/pages/settings/GeneralSettingsTab.qml
        qml/pages/settings/HardwareSettingsTab.qml
        qml/pages/settings/AboutLicensesTab.qml
        qml/pages/settings/RemoteInferenceTab.qml

        qml/components/Sidebar.qml
        qml/components/AppTitleBar.qml
        qml/components/WaveformView.qml
        qml/components/RuntimeVersionManagerDialog.qml
        qml/components/base/AppComboBox.qml
        qml/components/base/AppSpinBox.qml
        qml/components/base/AppContextMenu.qml
        qml/components/base/ConfirmationDialog.qml
        qml/components/base/ModelDependenciesDialog.qml
        qml/components/base/AppTabButton.qml
        qml/components/base/AppTextArea.qml
        qml/components/base/AppToolTip.qml
        qml/components/base/LineIcon.qml
        qml/components/base/colabNotebookUrls.js
        qml/components/base/ColabNotebookLink.qml
        qml/components/base/ColabSessionStatus.qml
        qml/components/PrimaryButton.qml
        qml/components/SearchBar.qml
        qml/components/ModelListItem.qml
        qml/components/ModelDetailsView.qml
        qml/components/DownloadProgressBar.qml
        qml/components/DownloadsPopup.qml
        qml/components/WorkflowPopup.qml
        qml/components/CommunityDialog.qml
        qml/components/BottomLogPanel.qml
        qml/components/alignment/AlignmentStudioView.qml
        qml/components/translation/TranslationStudioView.qml
        qml/components/alignment/AlignmentSectionHeader.qml
        qml/components/alignment/AlignmentOptionField.qml
        qml/components/alignment/AlignmentStatusStrip.qml
        qml/components/alignment/AlignmentSetupPanel.qml
        qml/components/alignment/AlignmentViewModeButton.qml
        qml/components/alignment/AlignmentPlayerButton.qml
        qml/components/alignment/AlignmentResultMetric.qml
        qml/components/dubbing/DubbingExportDialog.qml
        qml/components/dubbing/DubbingSubtitleEditor.qml
        qml/components/dubbing/DubbingInlineSubtitleEditor.qml
        qml/components/dubbing/DubbingColabSetupDialog.qml
        qml/components/dubbing/DubbingEntryGateDialog.qml
        qml/components/dubbing/DubbingProjectSetupDialog.qml
        qml/components/dubbing/DubbingAutomaticPreflightDialog.qml
        qml/components/dubbing/DubbingTranslationFixDialog.qml
        qml/components/dubbing/DubbingQualityDialog.qml
        qml/components/dubbing/DubbingHistoryPanel.qml
        qml/components/dubbing/DubbingProjectStatusPanel.qml
        qml/components/dubbing/DubbingNodeSettingsPanel.qml
        qml/components/dubbing/DubbingArtifactUploadPanel.qml
        qml/components/dubbing/DubbingArtifactUploadDialog.qml
        qml/components/dubbing/DubbingNodeInspector.qml
        qml/components/dubbing/DubbingWorkflowStep.qml
        qml/components/dubbing/DubbingWorkflowHeader.qml
        qml/components/dubbing/ColabMediaAcquisitionPanel.qml
        qml/components/dubbing/DubbingSourceMediaPanel.qml
        qml/components/dubbing/DubbingMediaQueueDialog.qml
        qml/components/dubbing/DubbingVoiceClipReview.qml
        qml/components/dubbing/panels/DubbingTaskShelf.qml
        qml/components/dubbing/panels/DubbingReviewPanel.qml
        qml/components/dubbing/panels/DubbingTimelineSection.qml
        qml/components/dubbing/steps/DubbingImportStep.qml
        qml/components/dubbing/steps/DubbingNormalizeStep.qml
        qml/components/dubbing/steps/DubbingSeparateStep.qml
        qml/components/dubbing/steps/DubbingTranscribeStep.qml
        qml/components/dubbing/steps/DubbingTranscriptReviewStep.qml
        qml/components/dubbing/steps/DubbingTranslateStep.qml
        qml/components/dubbing/steps/DubbingTranslationReviewStep.qml
        qml/components/dubbing/steps/DubbingSynthesizeStep.qml
        qml/components/dubbing/steps/DubbingMixStep.qml
        qml/components/dubbing/steps/DubbingExportStep.qml
        qml/components/subtitles/ocr/SubtitleOcrPreviewCanvas.qml
        qml/components/subtitles/ocr/SubtitleOcrConfigPanel.qml
        qml/components/subtitles/ocr/SubtitleOcrSegmentsTable.qml
        qml/components/models/ModelCatalogHeader.qml
        qml/components/models/ModelCardGrid.qml
        qml/components/models/ModelDetailsDrawer.qml
        qml/components/media/MediaDownloadInputSection.qml
        qml/components/media/MediaDownloadQueueList.qml
        qml/components/shared/WorkflowPipelineDialog.qml
        qml/components/shared/WorkflowNodeModelDialog.qml
        qml/components/shared/ProjectSelectionGate.qml
        qml/components/shared/MediaControlsAutoHide.qml
        qml/components/voicecloning/VoiceSettingsPanel.qml
        qml/components/voicecloning/VoiceCloningStudioView.qml
        qml/components/voicecloning/ReferenceInputBox.qml
        qml/components/voicecloning/InputSourceTabs.qml
        qml/components/shared/StudioHeaderBar.qml
        qml/components/shared/GeneratedAudioOutput.qml
        qml/components/shared/VoiceSeparationOutput.qml
        qml/components/shared/LoadedModelDialog.qml
        qml/components/shared/ExamplePickerDialog.qml
        qml/components/shared/CapabilityGallery.qml
        qml/components/shared/StudioContext.qml
        qml/components/shared/StudioShell.qml
        qml/components/shared/StudioPageFrame.qml
        qml/components/shared/WorkflowStudioShell.qml
        qml/components/shared/StudioRouteRegistry.qml
        qml/components/shared/VoiceLibraryDialog.qml
        qml/components/shared/PromptTagPicker.qml
        qml/components/shared/AudioInputSourcePicker.qml
        qml/components/shared/MediaInputSourcePicker.qml
        qml/components/shared/settings/CollapsibleSettingsSection.qml
        qml/components/shared/settings/LanguageSelector.qml
        qml/components/shared/settings/ModelParameterControls.qml
        qml/components/shared/settings/SettingsSection.qml
        qml/components/shared/settings/ParameterSlider.qml
        qml/components/shared/settings/ToggleRow.qml
        qml/components/shared/settings/FieldLabel.qml
        qml/components/shared/settings/SettingsCard.qml
        qml/components/shared/settings/SettingRow.qml
        qml/components/tts/TtsHistoryPanel.qml
        qml/components/tts/TtsSettingsPanel.qml
        qml/components/tts/TtsStudioView.qml
        qml/components/llm/LlmChatStudioView.qml
        qml/components/tts/SrtVoiceView.qml
        qml/components/voiceisolator/VoiceIsolatorHistoryPanel.qml
        qml/components/voiceisolator/VoiceIsolatorStudioView.qml
        qml/components/voicedesign/VoiceDesignHistoryPanel.qml
        qml/components/voicedesign/VoiceDesignSettingsPanel.qml
        qml/components/voicedesign/VoiceDesignStudioView.qml
        qml/components/voicedesign/VoicePresetPanel.qml
        qml/components/stt/SttHistoryPanel.qml
        qml/components/stt/SttInputSection.qml
        qml/components/stt/SttTranscriptionView.qml
        qml/components/stt/SttStudioView.qml
        qml/components/stt/SttSettingsPanel.qml
    RESOURCES
        data/catalog.json
        data/registry_schema.sql
        ${LASTUDIO_LANGUAGE_SET_FILES}
        data/vieneu/voices_v3_turbo.json
        ${LASTUDIO_EXAMPLE_FILES}
        icons/app_icon.ico
        icons/app_icon_16.png
        icons/app_icon_32.png
        icons/app_icon_48.png
        icons/app_icon_128.png
        icons/app_icon_256.png
    RESOURCE_PREFIX /
)

# LA Studio ships against the shared Qt runtime.  The CMake import scanner is
# only needed to link static QML plugins, while package.ps1 uses windeployqt
# with the application's qml directory to deploy the dynamic modules.  Do not
# invoke qmlimportscanner during configure/build: on Windows it can otherwise
# stall packaging without contributing any deployed dependency information.
set_property(TARGET LAStudio PROPERTY QT_QML_MODULE_NO_IMPORT_SCAN TRUE)

target_include_directories(LAStudio PRIVATE
    ${LASTUDIO_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/alignment
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/api/parts
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/audio/io
    ${CMAKE_SOURCE_DIR}/src/audio/player
    ${CMAKE_SOURCE_DIR}/src/controllers
    ${CMAKE_SOURCE_DIR}/src/controllers/alignment
    ${CMAKE_SOURCE_DIR}/src/controllers/app
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing/services
    ${CMAKE_SOURCE_DIR}/src/controllers/llm
    ${CMAKE_SOURCE_DIR}/src/controllers/models
    ${CMAKE_SOURCE_DIR}/src/controllers/models/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/models/services
    ${CMAKE_SOURCE_DIR}/src/controllers/separation
    ${CMAKE_SOURCE_DIR}/src/controllers/shared
    ${CMAKE_SOURCE_DIR}/src/controllers/stt
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles/services
    ${CMAKE_SOURCE_DIR}/src/controllers/translation
    ${CMAKE_SOURCE_DIR}/src/controllers/tts
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/hardware
    ${CMAKE_SOURCE_DIR}/src/core/models
    ${CMAKE_SOURCE_DIR}/src/core/parts
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/storage
    ${CMAKE_SOURCE_DIR}/src/core/utils
    ${CMAKE_SOURCE_DIR}/src/dubbing
    ${CMAKE_SOURCE_DIR}/src/dubbing/audio
    ${CMAKE_SOURCE_DIR}/src/dubbing/exporters
    ${CMAKE_SOURCE_DIR}/src/dubbing/fusion
    ${CMAKE_SOURCE_DIR}/src/dubbing/media
    ${CMAKE_SOURCE_DIR}/src/dubbing/project
    ${CMAKE_SOURCE_DIR}/src/dubbing/timing
    ${CMAKE_SOURCE_DIR}/src/dubbing/workflow
    ${CMAKE_SOURCE_DIR}/src/inference
    ${CMAKE_SOURCE_DIR}/src/llm
    ${CMAKE_SOURCE_DIR}/src/remote
    ${CMAKE_SOURCE_DIR}/src/remote/colab
    ${CMAKE_SOURCE_DIR}/src/remote/gateway
    ${CMAKE_SOURCE_DIR}/src/runtimehost
    ${CMAKE_SOURCE_DIR}/src/runtimehost/hosted_backends
    ${CMAKE_SOURCE_DIR}/src/runtimehost/protocol
    ${CMAKE_SOURCE_DIR}/src/runtimehost/service
    ${CMAKE_SOURCE_DIR}/src/separation
    ${CMAKE_SOURCE_DIR}/src/separation/backends
    ${CMAKE_SOURCE_DIR}/src/separation/backends/sherpa
    ${CMAKE_SOURCE_DIR}/src/separation/engine
    ${CMAKE_SOURCE_DIR}/src/separation/io
    ${CMAKE_SOURCE_DIR}/src/separation/runners
    ${CMAKE_SOURCE_DIR}/src/stt
    ${CMAKE_SOURCE_DIR}/src/stt/backends
    ${CMAKE_SOURCE_DIR}/src/stt/backends/nemotron
    ${CMAKE_SOURCE_DIR}/src/stt/backends/qwen
    ${CMAKE_SOURCE_DIR}/src/stt/backends/whisper
    ${CMAKE_SOURCE_DIR}/src/stt/engine
    ${CMAKE_SOURCE_DIR}/src/stt/runners
    ${CMAKE_SOURCE_DIR}/src/subtitles
    ${CMAKE_SOURCE_DIR}/src/subtitles/cues
    ${CMAKE_SOURCE_DIR}/src/subtitles/locators
    ${CMAKE_SOURCE_DIR}/src/subtitles/ocr
    ${CMAKE_SOURCE_DIR}/src/textnorm
    ${CMAKE_SOURCE_DIR}/src/textnorm/data
    ${CMAKE_SOURCE_DIR}/src/textnorm/include
    ${CMAKE_SOURCE_DIR}/src/textnorm/include/vietnorm
    ${CMAKE_SOURCE_DIR}/src/textnorm/src
    ${CMAKE_SOURCE_DIR}/src/textnorm/tests
    ${CMAKE_SOURCE_DIR}/src/translation
    ${CMAKE_SOURCE_DIR}/src/translation/backends
    ${CMAKE_SOURCE_DIR}/src/translation/backends/crisp
    ${CMAKE_SOURCE_DIR}/src/translation/backends/llama
    ${CMAKE_SOURCE_DIR}/src/translation/engine
    ${CMAKE_SOURCE_DIR}/src/translation/interfaces
    ${CMAKE_SOURCE_DIR}/src/translation/runners
    ${CMAKE_SOURCE_DIR}/src/translation/services
    ${CMAKE_SOURCE_DIR}/src/tts
    ${CMAKE_SOURCE_DIR}/src/tts/backends
    ${CMAKE_SOURCE_DIR}/src/tts/backends/kokoro
    ${CMAKE_SOURCE_DIR}/src/tts/backends/omnivoice
    ${CMAKE_SOURCE_DIR}/src/tts/backends/qwen
    ${CMAKE_SOURCE_DIR}/src/tts/backends/vibevoice
    ${CMAKE_SOURCE_DIR}/src/tts/backends/vieneu
    ${CMAKE_SOURCE_DIR}/src/tts/backends/voxcpm
    ${CMAKE_SOURCE_DIR}/src/tts/engine
    ${CMAKE_SOURCE_DIR}/src/tts/pipeline
    ${CMAKE_SOURCE_DIR}/src/tts/processing
    ${CMAKE_SOURCE_DIR}/src/tts/runners
    ${CMAKE_SOURCE_DIR}/src/workflows
    ${CMAKE_SOURCE_DIR}/src/workflows/graph
    ${CMAKE_SOURCE_DIR}/src/workflows/registry
    ${CMAKE_SOURCE_DIR}/src/workflows/session
    ${CMAKE_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/ggml/include
)

target_compile_definitions(LAStudio PRIVATE
    LASTUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(LAStudio PRIVATE
    vietnorm::vietnorm
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Network
    Qt6::Concurrent
    Qt6::Sql
    CURL::libcurl
    ZLIB::ZLIB
)

if(WIN32)
    target_link_libraries(LAStudio PRIVATE dbghelp)
    target_link_libraries(LAStudio PRIVATE shell32 wintrust crypt32)

    # windeployqt deploys Qt but it does not copy runtime DLLs imported from
    # vcpkg.  LAStudio links libcurl and zlib directly, so a normal developer
    # build (including the QML smoke executable) must place those DLLs beside
    # the EXE as well.  Without this the loader exits with 0xc0000135 before
    # main(), which hides all application diagnostics.
    add_custom_command(TARGET LAStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:CURL::libcurl>
            $<TARGET_FILE_DIR:LAStudio>
        VERBATIM)

    # FindZLIB exposes its Windows import library (zlib.lib), not the
    # accompanying zlib1.dll, through TARGET_FILE.  Derive the vcpkg prefix
    # from the import-library directory and stage the actual loader input.
    set(LASTUDIO_ZLIB_IMPORT_LIBRARY "${ZLIB_LIBRARY_RELEASE}")
    if(NOT LASTUDIO_ZLIB_IMPORT_LIBRARY OR NOT EXISTS "${LASTUDIO_ZLIB_IMPORT_LIBRARY}")
        set(LASTUDIO_ZLIB_IMPORT_LIBRARY "${ZLIB_LIBRARY}")
    endif()
    get_filename_component(LASTUDIO_ZLIB_LIB_DIR
        "${LASTUDIO_ZLIB_IMPORT_LIBRARY}" DIRECTORY)
    get_filename_component(LASTUDIO_ZLIB_PREFIX
        "${LASTUDIO_ZLIB_LIB_DIR}" DIRECTORY)
    set(LASTUDIO_ZLIB_RUNTIME_DLL
        "${LASTUDIO_ZLIB_PREFIX}/bin/zlib1.dll")
    if(NOT EXISTS "${LASTUDIO_ZLIB_RUNTIME_DLL}")
        message(FATAL_ERROR
            "Windows build requires the zlib runtime DLL, but it was not found at "
            "${LASTUDIO_ZLIB_RUNTIME_DLL}. Check the vcpkg x64-windows installation.")
    endif()
    add_custom_command(TARGET LAStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LASTUDIO_ZLIB_RUNTIME_DLL}"
            $<TARGET_FILE_DIR:LAStudio>
        VERBATIM)
endif()

# Unit tests build.
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Runtime hosts load native model DLLs in their own Windows process so
# incompatible dependencies such as ggml.dll never share one module table
# with LA Studio or another runtime family.
add_executable(LAStudioRuntimeHost
    src/runtimehost/main.cpp
    src/runtimehost/protocol/RuntimeHostProtocol.cpp
    src/runtimehost/protocol/RuntimeHostSharedBuffer.cpp
    src/runtimehost/service/RuntimeHostServer.cpp
    src/runtimehost/service/RuntimeHostWorker.cpp
    src/runtimehost/service/RuntimeHostAdapter.cpp
    src/tts/backends/omnivoice/OmnivoiceBackend.cpp
    src/stt/backends/whisper/WhisperSttBackend.cpp
    src/translation/backends/llama/LlamaTranslationBackend.cpp
    src/translation/interfaces/LlamaTranslationInterface.cpp
    src/audio/io/WavIO.cpp
    src/audio/io/AudioFileDecoder.cpp
    src/core/utils/Logger.cpp
    src/core/storage/PathUtils.cpp
    src/core/services/MediaRuntimeLocator.cpp
    src/core/hardware/InferenceThreadPolicy.cpp
    src/runtimehost/protocol/RuntimeHostProtocol.h
    src/runtimehost/protocol/RuntimeHostSharedBuffer.h
    src/runtimehost/service/RuntimeHostServer.h
    src/runtimehost/service/RuntimeHostWorker.h
    src/runtimehost/service/RuntimeHostAdapter.h
)
target_include_directories(LAStudioRuntimeHost PRIVATE
    ${LASTUDIO_GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/alignment
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_SOURCE_DIR}/src/api/parts
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/audio/io
    ${CMAKE_SOURCE_DIR}/src/audio/player
    ${CMAKE_SOURCE_DIR}/src/controllers
    ${CMAKE_SOURCE_DIR}/src/controllers/alignment
    ${CMAKE_SOURCE_DIR}/src/controllers/app
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/dubbing/services
    ${CMAKE_SOURCE_DIR}/src/controllers/llm
    ${CMAKE_SOURCE_DIR}/src/controllers/models
    ${CMAKE_SOURCE_DIR}/src/controllers/models/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/models/services
    ${CMAKE_SOURCE_DIR}/src/controllers/separation
    ${CMAKE_SOURCE_DIR}/src/controllers/shared
    ${CMAKE_SOURCE_DIR}/src/controllers/stt
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles/parts
    ${CMAKE_SOURCE_DIR}/src/controllers/subtitles/services
    ${CMAKE_SOURCE_DIR}/src/controllers/translation
    ${CMAKE_SOURCE_DIR}/src/controllers/tts
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/hardware
    ${CMAKE_SOURCE_DIR}/src/core/models
    ${CMAKE_SOURCE_DIR}/src/core/parts
    ${CMAKE_SOURCE_DIR}/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/storage
    ${CMAKE_SOURCE_DIR}/src/core/utils
    ${CMAKE_SOURCE_DIR}/src/dubbing
    ${CMAKE_SOURCE_DIR}/src/dubbing/audio
    ${CMAKE_SOURCE_DIR}/src/dubbing/exporters
    ${CMAKE_SOURCE_DIR}/src/dubbing/fusion
    ${CMAKE_SOURCE_DIR}/src/dubbing/media
    ${CMAKE_SOURCE_DIR}/src/dubbing/project
    ${CMAKE_SOURCE_DIR}/src/dubbing/timing
    ${CMAKE_SOURCE_DIR}/src/dubbing/workflow
    ${CMAKE_SOURCE_DIR}/src/inference
    ${CMAKE_SOURCE_DIR}/src/llm
    ${CMAKE_SOURCE_DIR}/src/remote
    ${CMAKE_SOURCE_DIR}/src/remote/colab
    ${CMAKE_SOURCE_DIR}/src/remote/gateway
    ${CMAKE_SOURCE_DIR}/src/runtimehost
    ${CMAKE_SOURCE_DIR}/src/runtimehost/hosted_backends
    ${CMAKE_SOURCE_DIR}/src/runtimehost/protocol
    ${CMAKE_SOURCE_DIR}/src/runtimehost/service
    ${CMAKE_SOURCE_DIR}/src/separation
    ${CMAKE_SOURCE_DIR}/src/separation/backends
    ${CMAKE_SOURCE_DIR}/src/separation/backends/sherpa
    ${CMAKE_SOURCE_DIR}/src/separation/engine
    ${CMAKE_SOURCE_DIR}/src/separation/io
    ${CMAKE_SOURCE_DIR}/src/separation/runners
    ${CMAKE_SOURCE_DIR}/src/stt
    ${CMAKE_SOURCE_DIR}/src/stt/backends
    ${CMAKE_SOURCE_DIR}/src/stt/backends/nemotron
    ${CMAKE_SOURCE_DIR}/src/stt/backends/qwen
    ${CMAKE_SOURCE_DIR}/src/stt/backends/whisper
    ${CMAKE_SOURCE_DIR}/src/stt/engine
    ${CMAKE_SOURCE_DIR}/src/stt/runners
    ${CMAKE_SOURCE_DIR}/src/subtitles
    ${CMAKE_SOURCE_DIR}/src/subtitles/cues
    ${CMAKE_SOURCE_DIR}/src/subtitles/locators
    ${CMAKE_SOURCE_DIR}/src/subtitles/ocr
    ${CMAKE_SOURCE_DIR}/src/textnorm
    ${CMAKE_SOURCE_DIR}/src/textnorm/data
    ${CMAKE_SOURCE_DIR}/src/textnorm/include
    ${CMAKE_SOURCE_DIR}/src/textnorm/include/vietnorm
    ${CMAKE_SOURCE_DIR}/src/textnorm/src
    ${CMAKE_SOURCE_DIR}/src/textnorm/tests
    ${CMAKE_SOURCE_DIR}/src/translation
    ${CMAKE_SOURCE_DIR}/src/translation/backends
    ${CMAKE_SOURCE_DIR}/src/translation/backends/crisp
    ${CMAKE_SOURCE_DIR}/src/translation/backends/llama
    ${CMAKE_SOURCE_DIR}/src/translation/engine
    ${CMAKE_SOURCE_DIR}/src/translation/interfaces
    ${CMAKE_SOURCE_DIR}/src/translation/runners
    ${CMAKE_SOURCE_DIR}/src/translation/services
    ${CMAKE_SOURCE_DIR}/src/tts
    ${CMAKE_SOURCE_DIR}/src/tts/backends
    ${CMAKE_SOURCE_DIR}/src/tts/backends/kokoro
    ${CMAKE_SOURCE_DIR}/src/tts/backends/omnivoice
    ${CMAKE_SOURCE_DIR}/src/tts/backends/qwen
    ${CMAKE_SOURCE_DIR}/src/tts/backends/vibevoice
    ${CMAKE_SOURCE_DIR}/src/tts/backends/vieneu
    ${CMAKE_SOURCE_DIR}/src/tts/backends/voxcpm
    ${CMAKE_SOURCE_DIR}/src/tts/engine
    ${CMAKE_SOURCE_DIR}/src/tts/pipeline
    ${CMAKE_SOURCE_DIR}/src/tts/processing
    ${CMAKE_SOURCE_DIR}/src/tts/runners
    ${CMAKE_SOURCE_DIR}/src/workflows
    ${CMAKE_SOURCE_DIR}/src/workflows/graph
    ${CMAKE_SOURCE_DIR}/src/workflows/registry
    ${CMAKE_SOURCE_DIR}/src/workflows/session
    ${CMAKE_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/ggml/include
)
target_link_libraries(LAStudioRuntimeHost PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
)
target_compile_definitions(LAStudioRuntimeHost PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(BUILD_TESTING)
    # QML route smoke must exercise the application compiled from the current
    # source, not a possibly stale executable left by a previous package run.
    add_dependencies(LAStudioUnitTests LAStudio LAStudioRuntimeHost)
endif()

if(WIN32)
    target_compile_definitions(LAStudio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(LAStudio PRIVATE dxgi)
endif()



# Omnivoice is loaded dynamically at runtime via OmnivoiceInterface

set_target_properties(LAStudio PROPERTIES
    OUTPUT_NAME "${LASTUDIO_EXECUTABLE_BASENAME}"
    MACOSX_BUNDLE TRUE
)

if(WIN32)
    string(REPLACE "." "," LASTUDIO_VERSION_COMMA "${LASTUDIO_VERSION}")
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/icons/app_icon.ico" LASTUDIO_ICON_FILE)
    set(LASTUDIO_FILE_DESCRIPTION "LA Studio")
    set(LASTUDIO_INTERNAL_NAME "LAStudio")
    set(LASTUDIO_ORIGINAL_FILENAME "${LASTUDIO_EXECUTABLE_FILENAME}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/versioninfo.rc.in"
        "${LASTUDIO_GENERATED_DIR}/LAStudioVersionInfo.rc"
        @ONLY
    )
    set(LASTUDIO_FILE_DESCRIPTION "LA Studio Runtime Host")
    set(LASTUDIO_INTERNAL_NAME "LAStudioRuntimeHost")
    set(LASTUDIO_ORIGINAL_FILENAME "LAStudioRuntimeHost.exe")
    configure_file(
        "${CMAKE_SOURCE_DIR}/resources/versioninfo.rc.in"
        "${LASTUDIO_GENERATED_DIR}/LAStudioRuntimeHostVersionInfo.rc"
        @ONLY
    )

    # Keep a console in Debug builds for dev logging, but hide it in all non-Debug builds.
    set_target_properties(LAStudio PROPERTIES
        WIN32_EXECUTABLE $<IF:$<CONFIG:Debug>,FALSE,TRUE>
    )

    target_sources(LAStudio PRIVATE "${LASTUDIO_GENERATED_DIR}/LAStudioVersionInfo.rc")
    target_sources(LAStudioRuntimeHost PRIVATE "${LASTUDIO_GENERATED_DIR}/LAStudioRuntimeHostVersionInfo.rc")
endif()

# --- Installation Rules ---
include(GNUInstallDirs)

if(WIN32)
    # Keep installer metadata synchronized with the executable at configure time.
    file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" LASTUDIO_INSTALLER_STAGE_DIR)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/out" LASTUDIO_INSTALLER_OUTPUT_DIR)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/icons/app_icon.ico" LASTUDIO_INSTALLER_ICON_FILE)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/LICENSE" LASTUDIO_LICENSE_FILE)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/resources/SOURCE-OFFER.txt" LASTUDIO_SOURCE_OFFER_FILE)
    set(LASTUDIO_INSTALLER_APPLICATION_EXE "${LASTUDIO_EXECUTABLE_FILENAME}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/scripts/installer.iss.in"
        "${CMAKE_BINARY_DIR}/installer.iss"
        @ONLY
    )
endif()

# Install the executable
set(LASTUDIO_RUNTIME_INSTALL_DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(LASTUDIO_PORTABLE_INTERNAL_LAYOUT)
    set(LASTUDIO_RUNTIME_INSTALL_DESTINATION ".")
endif()

install(TARGETS LAStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${LASTUDIO_RUNTIME_INSTALL_DESTINATION}
)

install(TARGETS LAStudioRuntimeHost
    RUNTIME DESTINATION ${LASTUDIO_RUNTIME_INSTALL_DESTINATION}
)

install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION .)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME "AGPL-3.0.txt")
install(FILES "${CMAKE_SOURCE_DIR}/resources/SOURCE-OFFER.txt" DESTINATION licenses)
install(FILES "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" DESTINATION .)
install(FILES "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" DESTINATION licenses)

# Ship the direct Colab workers and their concise operating guide with desktop
# packages. They are support artifacts only: the application never embeds a
# Gateway key or uses these notebooks as a Gateway relay.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/notebooks/"
    DESTINATION "docs/colab-notebooks"
    FILES_MATCHING PATTERN "*.ipynb"
)

# Notebooks that load a reviewed worker template by immutable Git revision
# must still carry that template in the portable support bundle. This keeps
# the shipped notebook/documentation auditable even though a Colab session
# downloads the exact pinned revision at execution time.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/notebooks/workers/"
    DESTINATION "docs/colab-notebooks/workers"
    FILES_MATCHING PATTERN "*.py"
)
install(FILES "${CMAKE_SOURCE_DIR}/docs/REMOTE_WORKERS.md" DESTINATION "docs")

# Note: Runtime DLLs (Whisper, etc.) and windeployqt will be handled 
# by the CI/CD script or a post-install step.

