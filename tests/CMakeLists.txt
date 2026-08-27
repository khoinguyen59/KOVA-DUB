find_package(Qt6 REQUIRED COMPONENTS Core Test Concurrent Sql Network Multimedia Quick)
find_package(CURL CONFIG REQUIRED)

set(TEST_SOURCES
    main.cpp
    subtitles/OcrE2ERunner.cpp
    subtitles/OcrE2ERunner.h
    dubbing/LiveRealWorkflowRunner.cpp
    dubbing/LiveRealWorkflowRunner.h
    core/test_FileAccessService.cpp
    core/test_ModelsPathMigration.cpp
    core/test_History.cpp
    core/test_DownloadInstallService.cpp
    core/test_AudioPreviewService.cpp
    core/test_ModelsAndRuntimes.cpp
    core/test_SttSession.cpp
    tts/test_GatewayTtsRunner.cpp
    tts/test_GatewayTtsRunner.h
    colab/test_ColabTtsRunner.cpp
    colab/test_ColabTtsRunner.h
    colab/test_ColabVoiceCloneRunner.cpp
    colab/test_ColabVoiceCloneRunner.h
    colab/test_ColabVoiceDesignRunner.cpp
    colab/test_ColabVoiceDesignRunner.h
    colab/test_ColabAlignmentRunner.cpp
    colab/test_ColabAlignmentRunner.h
    colab/test_ColabSeparationRunner.cpp
    colab/test_ColabSeparationRunner.h
    colab/test_ColabTranslationRunner.cpp
    colab/test_ColabTranslationRunner.h
    colab/test_ColabSubtitleOcrRunner.cpp
    colab/test_ColabSubtitleOcrRunner.h
    colab/test_ColabChatRunner.cpp
    colab/test_ColabChatRunner.h
    tts/test_SubtitleVoice.cpp
    subtitles/test_SubtitleOcrPipeline.cpp
    subtitles/test_SubtitleOcrController.cpp
    subtitles/test_SubtitleOcrRuntimeService.cpp
    tts/test_TtsTextPreprocessor.cpp
    tts/test_TtsRequestValidator.cpp
    core/test_LlmChatEngine.cpp
    core/test_RemoteExecution.cpp
    core/test_StudioCapabilities.cpp
    dubbing/test_AlignmentTranscriptMatcher.cpp
    dubbing/test_AlignmentWorkflow.cpp
    dubbing/test_DubbingProject.cpp
    dubbing/test_TranslationProject.cpp
    dubbing/test_WorkflowGraph.cpp
    core/test_SourceSeparation.cpp
    core/test_RuntimeHostProtocol.cpp
    core/test_MediaIngestService.cpp
    core/test_MediaToolService.cpp
    core/test_HardwareManager.cpp
    core/test_AppErrorCatalog.cpp
    mocks/SttEngine.cpp
    mocks/SttEngineInstance.cpp
    mocks/TtsEngine.cpp
    mocks/TtsEngineInstance.cpp
    mocks/AudioRecorder.cpp
    mocks/AudioPlayer.cpp
    mocks/HFHubClient.cpp
    mocks/DownloadManager.cpp
    mocks/AppController.cpp
    tts/test_SubtitleVoice.h
    subtitles/test_SubtitleOcrPipeline.h
    subtitles/test_SubtitleOcrController.h
    subtitles/test_SubtitleOcrRuntimeService.h

    ../src/api/ApiServerService.cpp

    ../src/controllers/shared/FileAccessService.cpp
    ../src/controllers/shared/AppUpdateService.cpp
    ../src/controllers/shared/ExampleManager.cpp
    ../src/controllers/models/DownloadInstallService.cpp
    ../src/controllers/models/RemoteModelCatalogController.cpp
    ../src/controllers/alignment/AlignmentExecutionService.cpp
    ../src/controllers/alignment/ColabAlignmentController.cpp
    ../src/controllers/separation/ColabVoiceIsolatorController.cpp
    ../src/controllers/separation/VoiceCloneReferenceIsolatorController.cpp
    ../src/dubbing/fusion/AlignmentRefinementService.cpp
    ../src/dubbing/fusion/DubbingSegmentNormalizer.cpp
    ../src/controllers/alignment/AlignmentTranscriptMatcher.cpp
    ../src/alignment/CrispAlignmentInterface.cpp
    ../src/translation/interfaces/CrispTranslationInterface.cpp
    ../src/translation/interfaces/LlamaTranslationInterface.cpp
    ../src/llm/LlmChatEngine.cpp
    ../src/llm/ColabChatRunner.cpp
    ../src/controllers/llm/LlmChatModelSession.cpp
    ../src/controllers/llm/LlmChatController.cpp
    ../src/controllers/models/ModelsPathMigrator.cpp
    ../src/controllers/models/ModelsPathMigrationService.cpp
    ../src/controllers/shared/HistoryRepository.cpp
    ../src/controllers/shared/HistoryService.cpp
    ../src/controllers/shared/AudioPreviewService.cpp
    ../src/controllers/app/StudioActions.cpp
    ../src/controllers/app/AppErrorCatalog.cpp
    ../src/controllers/shared/VoiceClonePresetService.cpp
    ../src/controllers/shared/VoiceDesignPresetService.cpp
    ../src/controllers/alignment/AlignmentModelSession.cpp
    ../src/controllers/translation/TranslationModelSession.cpp
    ../src/translation/services/TranslationProject.cpp
    ../src/translation/engine/TranslationEngine.cpp
    ../src/translation/engine/TranslationEngineInstance.cpp
    ../src/translation/engine/TranslationWorker.cpp
    ../src/translation/runners/GatewayTranslationRunner.cpp
    ../src/translation/runners/ColabTranslationRunner.cpp
    ../src/subtitles/ocr/ColabSubtitleOcrRunner.cpp
    ../src/translation/backends/TranslationBackendFactory.cpp
    ../src/translation/backends/crisp/CrispTranslationBackend.cpp
    ../src/translation/backends/llama/LlamaTranslationBackend.cpp
    ../src/translation/services/TranslationService.cpp
    ../src/controllers/translation/TranslationController.cpp
    ../src/controllers/separation/VoiceIsolatorModelSession.cpp
    ../src/controllers/models/ModelLifecycleController.cpp
    ../src/controllers/stt/SttModelSession.cpp
    ../src/controllers/tts/TtsSharedModelSession.cpp
    ../src/controllers/models/ModelSessionRegistry.cpp
    ../src/controllers/models/StudioConfigurationResolver.cpp
    ../src/controllers/models/CapabilitySettingsSchema.cpp
    ../src/controllers/stt/SttSessionController.cpp
    ../src/controllers/tts/GatewayTtsController.cpp
    ../src/controllers/tts/ColabTtsController.cpp
    ../src/controllers/tts/ColabVoiceCloneController.cpp
    ../src/controllers/tts/ColabVoiceDesignController.cpp
    ../src/controllers/tts/SubtitleVoiceController.cpp
    ../src/controllers/subtitles/SubtitleOcrController.cpp
    ../src/subtitles/cues/SrtTimelineParser.cpp
    ../src/subtitles/ocr/SubtitleOcrPipeline.cpp
    ../src/subtitles/locators/SubtitleOcrRuntimeLocator.cpp
    ../src/subtitles/locators/PaddleOcrRuntimeLocator.cpp
    ../src/subtitles/ocr/SubtitleOcrRuntimeService.cpp
    ../src/tts/pipeline/SubtitleSmartFitPlanner.cpp
    ../src/tts/processing/TextNormalizationPolicy.cpp
    ../src/tts/processing/TtsTextPreprocessor.cpp
    ../src/tts/processing/TtsRequestValidator.cpp
    ../src/tts/pipeline/TimedSpeechPipeline.cpp
    ../src/audio/AudioTimelineRenderer.cpp
    ../src/controllers/stt/SttAudioDecoder.cpp
    ../src/dubbing/project/DubbingProject.cpp
    ../src/dubbing/fusion/DubbingTranscriptFusionService.cpp
    ../src/dubbing/exporters/DubbingSubtitleService.cpp
    ../src/dubbing/exporters/CapCutDraftExporter.cpp
    ../src/dubbing/timing/EspeakNgPhonemizer.cpp
    ../src/dubbing/timing/DubbingDuration.cpp
    ../src/dubbing/audio/DubbingVoiceReferenceSelector.cpp
    ../src/dubbing/audio/DubbingVoiceReferenceSelector.h
    ../src/dubbing/timing/DubbingTimingProfile.cpp
    ../src/dubbing/timing/DubbingTimingService.cpp
    ../src/controllers/dubbing/DubbingController.cpp
    ../src/dubbing/audio/AudioTimelineMixer.cpp
    ../src/controllers/dubbing/DubbingJobRunner.cpp
    ../src/controllers/dubbing/DubbingRunCoordinator.cpp
    ../src/controllers/dubbing/SourceSeparationConfigurationResolver.cpp
    ../src/controllers/dubbing/DubbingTranscriptionJob.cpp
    ../src/controllers/dubbing/DubbingSynthesisJob.cpp
    ../src/controllers/dubbing/DubbingExportJob.cpp
    ../src/controllers/dubbing/DubbingTranslationJob.cpp
    ../src/controllers/dubbing/DubbingTranslationFixService.cpp
    ../src/dubbing/audio/DubbingTranslationService.cpp
    ../src/dubbing/media/MediaToolService.cpp
    ../src/dubbing/media/AtomicMediaCommit.cpp
    ../src/dubbing/media/MediaIngestService.cpp
    ../src/dubbing/media/DouyinBrowserSessionService.cpp
    ../src/dubbing/media/RemoteMediaImportService.cpp
    ../src/separation/io/SeparationAudioIO.cpp
    ../src/separation/engine/SourceSeparationService.cpp
    ../src/separation/engine/SeparationWorker.cpp
    ../src/separation/backends/SeparationBackendFactory.cpp
    ../src/separation/backends/sherpa/SherpaOnnxRuntime.cpp
    ../src/separation/backends/sherpa/SherpaOnnxSeparationBackend.cpp
    ../src/controllers/separation/VoiceIsolatorController.cpp
    ../src/controllers/app/WorkflowActivityManager.cpp
    ../src/workflows/registry/WorkflowTypes.cpp
    ../src/workflows/session/WorkflowSession.cpp
    ../src/workflows/graph/WorkflowGraph.cpp
    ../src/dubbing/workflow/DubbingWorkflowDefinition.cpp
    ../src/workflows/session/WorkflowArtifact.cpp
    ../src/workflows/session/WorkflowRun.cpp
    ../src/workflows/graph/WorkflowRunJournal.cpp
    ../src/workflows/session/WorkflowTranscript.cpp
    ../src/dubbing/workflow/DubbingWorkflowNodes.cpp
    ../src/dubbing/workflow/DubbingWorkflowAdapter.cpp
    ../src/workflows/registry/NodeRegistry.cpp
    ../src/workflows/graph/WorkflowGraphRunner.cpp
    ../src/alignment/AlignmentWorkflowResolver.cpp
    ../src/core/utils/LogViewService.cpp
    ../src/core/storage/CacheLifecycleService.cpp
    ../src/core/storage/PathUtils.cpp
    ../src/core/services/MediaRuntimeLocator.cpp
    ../src/core/utils/Logger.cpp
    ../src/core/hardware/InferenceThreadPolicy.cpp
    ../src/inference/WorkerThreadHost.cpp
    ../src/core/models/CapabilityFamilyModel.cpp
    ../src/core/hardware/HardwareManager.cpp
    ../src/core/services/StudioCapabilityRegistry.cpp
    ../src/core/storage/StudioSelectionRepository.cpp
    ../src/core/storage/Settings.cpp
    ../src/core/storage/SecureCredentialStore.cpp
    ../src/remote/gateway/ExecutionProvider.cpp
    ../src/remote/colab/ColabSession.cpp
    ../src/remote/gateway/GatewayClient.cpp
    ../src/remote/colab/ColabWorkerClient.cpp
    ../src/remote/gateway/GatewayModelCatalog.cpp
    ../src/remote/colab/ColabCapabilityCatalog.cpp
    ../src/stt/runners/ColabSttRunner.cpp
    ../src/stt/runners/GatewaySttRunner.cpp
    ../src/tts/runners/GatewayTtsRunner.cpp
    ../src/tts/runners/ColabTtsRunner.cpp
    ../src/tts/runners/ColabVoiceCloneRunner.cpp
    ../src/tts/runners/ColabVoiceDesignRunner.cpp
    ../src/alignment/ColabAlignmentRunner.cpp
    ../src/separation/runners/ColabSeparationRunner.cpp
    ../src/core/services/LocalizationManager.cpp
    ../src/core/models/ModelManager.cpp
    ../src/core/models/CatalogManager.cpp
    ../src/core/models/RegistryManager.cpp
    ../src/core/models/RuntimeManager.cpp
    ../src/core/utils/QmlLogger.cpp
    ../src/audio/io/WavIO.cpp
    ../src/audio/io/AudioFileDecoder.cpp
    ../src/runtimehost/protocol/RuntimeHostProtocol.cpp
    ../src/runtimehost/service/RuntimeHostClient.cpp
    ../src/runtimehost/protocol/RuntimeHostSharedBuffer.cpp
    ../src/runtimehost/service/RuntimeHostManager.cpp
    ../src/runtimehost/hosted_backends/HostedWhisperBackend.cpp
    ../src/runtimehost/hosted_backends/HostedLlamaTranslationBackend.cpp
    ../src/audio/io/WaveformProvider.cpp

    ../src/core/models/DownloadManager.h
    ../src/dubbing/exporters/DubbingSubtitleService.h
    ../src/core/services/MediaRuntimeLocator.h
    ../src/subtitles/ocr/SubtitleOcrPipeline.h
    ../src/subtitles/locators/SubtitleOcrRuntimeLocator.h
    ../src/subtitles/locators/PaddleOcrRuntimeLocator.h
    ../src/subtitles/ocr/SubtitleOcrRuntimeService.h
    ../src/controllers/subtitles/SubtitleOcrController.h
    ../src/api/ApiServerService.h
    ../src/stt/engine/SttEngine.h
    ../src/stt/engine/SttEngineInstance.h
    ../src/tts/engine/TtsEngine.h
    ../src/tts/engine/TtsEngineInstance.h
    ../src/tts/processing/TtsRequestValidator.h
    ../src/audio/player/AudioRecorder.h
    ../src/audio/player/AudioPlayer.h
    ../src/audio/io/AudioFileDecoder.h
    ../src/runtimehost/protocol/RuntimeHostProtocol.h
    ../src/core/models/HFHubClient.h
    ../src/remote/gateway/GatewayClient.h
    ../src/remote/colab/ColabWorkerClient.h
    ../src/remote/gateway/GatewayModelCatalog.h
    ../src/remote/colab/ColabCapabilityCatalog.h
    ../src/stt/runners/ColabSttRunner.h
    ../src/stt/runners/GatewaySttRunner.h
    ../src/tts/runners/GatewayTtsRunner.h
    ../src/tts/runners/ColabTtsRunner.h
    ../src/tts/runners/ColabVoiceCloneRunner.h
    ../src/tts/runners/ColabVoiceDesignRunner.h
    ../src/alignment/ColabAlignmentRunner.h
    ../src/separation/runners/ColabSeparationRunner.h
    ../src/controllers/tts/GatewayTtsController.h
    ../src/controllers/tts/ColabTtsController.h
    ../src/controllers/tts/ColabVoiceCloneController.h
    ../src/controllers/tts/ColabVoiceDesignController.h
    ../src/controllers/models/RemoteModelCatalogController.h
    ../src/controllers/alignment/ColabAlignmentController.h
    ../src/controllers/separation/ColabVoiceIsolatorController.h
    ../src/controllers/separation/VoiceCloneReferenceIsolatorController.h
    ../src/controllers/app/AppController.h
    ../src/controllers/app/AppErrorCatalog.h
    ../src/controllers/shared/AppUpdateService.h
    ../src/controllers/shared/ExampleManager.h
    ../src/controllers/app/IStudioAction.h
    ../src/controllers/app/StudioActions.h
    ../src/controllers/models/IModelSession.h
    ../src/inference/WorkerThreadHost.h
    ../src/controllers/models/ModelLifecycleController.h
    ../src/controllers/stt/SttModelSession.h
    ../src/controllers/tts/TtsSharedModelSession.h
    ../src/controllers/alignment/AlignmentModelSession.h
    ../src/controllers/translation/TranslationModelSession.h
    ../src/translation/services/TranslationProject.h
    ../src/translation/engine/TranslationEngine.h
    ../src/translation/engine/TranslationEngineInstance.h
    ../src/translation/engine/TranslationWorker.h
    ../src/translation/runners/GatewayTranslationRunner.h
    ../src/translation/runners/ColabTranslationRunner.h
    ../src/subtitles/ocr/ColabSubtitleOcrRunner.h
    ../src/translation/backends/TranslationBackend.h
    ../src/translation/backends/TranslationBackendFactory.h
    ../src/translation/backends/crisp/CrispTranslationBackend.h
    ../src/translation/backends/llama/LlamaTranslationBackend.h
    ../src/translation/services/TranslationService.h
    ../src/controllers/translation/TranslationController.h
    ../src/controllers/separation/VoiceIsolatorModelSession.h
    ../src/controllers/models/ModelSessionRegistry.h
    ../src/llm/LlmChatEngine.h
    ../src/llm/ColabChatRunner.h
    ../src/controllers/llm/LlmChatModelSession.h
    ../src/controllers/llm/LlmChatController.h
    ../src/controllers/app/WorkflowActivityManager.h
    ../src/workflows/registry/WorkflowTypes.h
    ../src/workflows/registry/IWorkflowResolver.h
    ../src/workflows/session/WorkflowSession.h
    ../src/workflows/graph/WorkflowGraph.h
    ../src/dubbing/workflow/DubbingWorkflowDefinition.h
    ../src/workflows/session/WorkflowArtifact.h
    ../src/workflows/session/WorkflowRun.h
    ../src/workflows/graph/WorkflowRunJournal.h
    ../src/workflows/session/WorkflowTranscript.h
    ../src/dubbing/workflow/DubbingWorkflowNodes.h
    ../src/workflows/registry/WorkflowExecutionAdapter.h
    ../src/dubbing/workflow/DubbingWorkflowAdapter.h
    ../src/workflows/registry/NodeRegistry.h
    ../src/workflows/graph/WorkflowGraphRunner.h
    ../src/alignment/AlignmentWorkflowResolver.h
    ../include/runtimes/CrispTranslationInterface.h
    ../src/separation/io/SeparationTypes.h
    ../src/separation/io/SeparationAudioIO.h
    ../src/separation/engine/SourceSeparationService.h
    ../src/separation/engine/SeparationWorker.h
    ../src/separation/backends/SeparationBackend.h
    ../src/separation/backends/SeparationBackendFactory.h
    ../src/separation/backends/sherpa/SherpaOnnxRuntime.h
    ../src/separation/backends/sherpa/SherpaOnnxSeparationBackend.h
    core/test_SourceSeparation.h
    core/test_RuntimeHostProtocol.h
    core/test_MediaIngestService.h
    core/test_MediaToolService.h
    core/test_LlmChatEngine.h
    core/test_RemoteExecution.h
    core/test_HardwareManager.h
    core/test_AppErrorCatalog.h
    dubbing/test_TranslationProject.h
    ../src/controllers/separation/VoiceIsolatorController.h
)

add_executable(LAStudioUnitTests ${TEST_SOURCES})

add_custom_command(TARGET LAStudioUnitTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:LAStudioUnitTests>/data"
    VERBATIM
)

target_compile_definitions(LAStudioUnitTests PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    LASTUDIO_UNIT_TESTS=1
    LASTUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_include_directories(LAStudioUnitTests PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/colab
    ${CMAKE_CURRENT_SOURCE_DIR}/dubbing
    ${CMAKE_CURRENT_SOURCE_DIR}/subtitles
    ${CMAKE_CURRENT_SOURCE_DIR}/tts
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${LLAMA_CPP_SOURCE_DIR}/include
    ${LLAMA_CPP_SOURCE_DIR}/ggml/include
)

target_link_libraries(LAStudioUnitTests PRIVATE
    vietnorm::vietnorm
    Qt6::Core
    Qt6::Test
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Network
    Qt6::Multimedia
    Qt6::Quick
    CURL::libcurl
    dxgi
)

if(WIN32)
    target_link_libraries(LAStudioUnitTests PRIVATE shell32 wintrust crypt32)
endif()

get_target_property(QT6_CORE_DLL Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT QT6_CORE_DLL)
    get_target_property(QT6_CORE_DLL Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(QT_BIN_DIR "${QT6_CORE_DLL}" DIRECTORY)
get_filename_component(QT_ROOT_DIR "${QT_BIN_DIR}" DIRECTORY)
set(QT_PLUGIN_DIR "${QT_ROOT_DIR}/plugins")
set(LASTUDIO_SUBTITLE_OCR_TEST_MEDIA_RUNTIME_ROOT
    "${CMAKE_BINARY_DIR}/subtitle-ocr-frame-runtime/media-tools")

set(LASTUDIO_TEST_SUITES
    TestFileAccessService
    TestModelsPathMigration
    TestHistory
    TestDownloadInstallService
    TestAudioPreviewService
    TestModelsAndRuntimes
    TestSttSession
    TestStudioCapabilities
    TestAlignmentTranscriptMatcher
    TestAlignmentWorkflow
    TestDubbingProject
    TestTranslationProject
    TestWorkflowGraph
    TestSourceSeparation
    TestSubtitleVoice
    TestSubtitleOcrPipeline
    TestSubtitleOcrController
    TestSubtitleOcrRuntimeService
    TestRuntimeHostProtocol
    TestMediaIngestService
    TestMediaToolService
    TestHardwareManager
    TestAppErrorCatalog
    TestTtsTextPreprocessor
    TestTtsRequestValidator
    TestLlmChatEngine
    TestRemoteExecution
    TestGatewayTtsRunner
    TestColabTtsRunner
    TestColabVoiceCloneRunner
    TestColabVoiceDesignRunner
    TestColabAlignmentRunner
    TestColabSeparationRunner
    TestColabTranslationRunner
    TestColabChatRunner
)

foreach(LASTUDIO_TEST_SUITE IN LISTS LASTUDIO_TEST_SUITES)
    add_test(NAME ${LASTUDIO_TEST_SUITE} COMMAND LAStudioUnitTests)
    set_tests_properties(${LASTUDIO_TEST_SUITE} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        ENVIRONMENT "LASTUDIO_TEST_SUITE=${LASTUDIO_TEST_SUITE};LASTUDIO_TEST_MEDIA_RUNTIME_ROOT=${LASTUDIO_SUBTITLE_OCR_TEST_MEDIA_RUNTIME_ROOT};QT_PLUGIN_PATH=${QT_PLUGIN_DIR};QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}"
    )
endforeach()

if(WIN32)
    find_program(POWERSHELL_EXECUTABLE NAMES powershell.exe powershell)
    if(POWERSHELL_EXECUTABLE)
        add_test(NAME PrepareSubtitleOcrFrameRuntime
            COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/subtitles/prepare_subtitle_ocr_frame_runtime.ps1"
                -RepositoryRoot "${CMAKE_SOURCE_DIR}"
                -DeployRoot "${CMAKE_BINARY_DIR}/subtitle-ocr-frame-runtime")
        set_tests_properties(PrepareSubtitleOcrFrameRuntime PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            FIXTURES_SETUP SubtitleOcrFrameRuntime
            TIMEOUT 180)
        set_tests_properties(TestSubtitleOcrController PROPERTIES
            FIXTURES_REQUIRED SubtitleOcrFrameRuntime)

        add_test(NAME RemoteLivePreflightContract
            COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/core/test_RemoteLivePreflightContract.ps1"
                -RepositoryRoot "${CMAKE_SOURCE_DIR}")
        set_tests_properties(RemoteLivePreflightContract PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            TIMEOUT 30)
    endif()
endif()

add_test(NAME QmlRouteSmoke COMMAND $<TARGET_FILE:LAStudio>)
set_tests_properties(QmlRouteSmoke PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    ENVIRONMENT "LASTUDIO_DATA_DIR=${CMAKE_BINARY_DIR}/qml-smoke-data;LASTUDIO_QML_SMOKE=1;QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT_PLUGIN_DIR}"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_BIN_DIR}"
    TIMEOUT 75
)

if(WIN32)
    # main.cpp intentionally removes PATH from the DLL search order. Deploy
    # the Qt runtime beside the smoke executable so this test verifies the
    # same trusted-layout rule used by the portable package, rather than
    # weakening the production DLL-search policy just for tests.
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt.exe windeployqt
        HINTS "${QT_BIN_DIR}" REQUIRED)
    add_test(NAME PrepareQmlRouteSmokeRuntime
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --verbose 0
            --qmldir "${CMAKE_SOURCE_DIR}/qml" --no-translations
            --compiler-runtime "$<TARGET_FILE:LAStudio>")
    set_tests_properties(PrepareQmlRouteSmokeRuntime PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        FIXTURES_SETUP QmlRouteSmokeRuntime
        TIMEOUT 60)
    set_tests_properties(QmlRouteSmoke PROPERTIES
        FIXTURES_REQUIRED QmlRouteSmokeRuntime)
endif()
