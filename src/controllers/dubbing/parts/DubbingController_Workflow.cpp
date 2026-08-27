namespace {

bool readableDubbingArtifact(const QString &path)
{
    const QFileInfo info(path.trimmed());
    return info.isFile() && info.size() > 0;
}

} // namespace

QVariantList DubbingController::workflowNodes() const
{
    const bool hasMedia = readableDubbingArtifact(m_project.sourceMediaPath);
    const bool hasSegments = !m_project.segments.isEmpty();
    bool hasTargets = false;
    bool allTargets = hasSegments;
    bool hasClips = false;
    bool hasConflict = false;
    for (const QVariant &entry : m_project.segments) {
        const QVariantMap segment = entry.toMap();
        hasTargets = hasTargets || !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
        allTargets = allTargets && !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
        hasClips = hasClips || readableDubbingArtifact(
            segment.value(QStringLiteral("clipPath")).toString());
        hasConflict = hasConflict || segment.value(QStringLiteral("timingConflict")).toBool();
    }
    const QVariantMap synthesisSelection = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    const QVariantMap synthesisParameters = synthesisSelection
        .value(QStringLiteral("parameters")).toMap();
    ExecutionProvider synthesisProvider = ExecutionProvider::LocalDev;
    const QString synthesisProviderId = synthesisSelection.value(
        QStringLiteral("executionProvider"), synthesisParameters.value(
        QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString();
    const bool remoteTtsSelected = executionProviderFromId(synthesisProviderId, &synthesisProvider)
        && synthesisProvider != ExecutionProvider::LocalDev;
    const bool ttsReady = remoteTtsSelected || (m_tts && m_tts->isModelLoaded());
    const bool translationReady = !m_project.targetLanguage.trimmed().isEmpty();
    const auto node = [](const QString &id, const QString &title, const QString &state,
                         const QString &detail, const QString &provider = QString()) {
        QVariantMap value{{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
                          {QStringLiteral("state"), state}, {QStringLiteral("detail"), detail},
                          {QStringLiteral("providerName"), provider},
                          {QStringLiteral("providerState"), QStringLiteral("ready")}};
        return QVariant(value);
    };
    QVariantList result;
    const WorkflowGraph graph = DubbingWorkflowDefinition::create();
    for (const WorkflowGraphNode &definition : graph.nodes) {
        QString state = QStringLiteral("blocked");
        QString detail;
        QString provider;
        if (definition.id == QStringLiteral("media-input")) {
            state = hasMedia ? QStringLiteral("ready") : QStringLiteral("missing");
            detail = hasMedia ? QFileInfo(m_project.sourceMediaPath).fileName() : QStringLiteral("Import audio or video");
        } else if (definition.id == QStringLiteral("ingest")) {
            const bool normalized = readableDubbingArtifact(m_project.masterAudioPath)
                && readableDubbingArtifact(m_project.analysisAudioPath);
            state = normalized ? QStringLiteral("completed") : (hasMedia ? QStringLiteral("ready") : QStringLiteral("missing"));
            detail = normalized ? QStringLiteral("Media normalized") : (hasMedia ? QStringLiteral("Ready to normalize") : QStringLiteral("Import source media"));
        } else if (definition.id == QStringLiteral("source-separate")) {
            const bool normalized = readableDubbingArtifact(m_project.masterAudioPath);
            const bool separated = normalized && readableDubbingArtifact(m_project.vocalsAudioPath)
                && readableDubbingArtifact(m_project.backgroundAudioPath);
            state = !hasMedia ? QStringLiteral("missing")
                : !normalized ? QStringLiteral("blocked")
                : (separated ? QStringLiteral("completed") : QStringLiteral("ready"));
            detail = separated ? QStringLiteral("Vocals and Background stems available")
                               : !normalized ? QStringLiteral("Normalize source media before running Isolator")
                               : (hasMedia ? QStringLiteral("Run Isolator to create Vocals and Background stems") : QStringLiteral("Import source media"));
        } else if (definition.id == QStringLiteral("transcribe")) {
            const QString transcriptSource = normalizedTranscriptSource(
                m_project.transcriptConfiguration.value(QStringLiteral("transcriptSource"),
                                                        QStringLiteral("stt")).toString());
            const bool audioReady = readableDubbingArtifact(m_project.vocalsAudioPath)
                || readableDubbingArtifact(m_project.analysisAudioPath)
                || readableDubbingArtifact(m_project.masterAudioPath);
            const QVariantList sttSegments = m_project.transcriptConfiguration.value(
                QStringLiteral("sttSegments")).toList();
            const QVariantList ocrSegments = m_project.transcriptConfiguration.value(
                QStringLiteral("ocrSegments")).toList();
            const QVariantList reconciledSegments = m_project.transcriptConfiguration.value(
                QStringLiteral("reconciledSegments")).toList();
            const bool ocrReady = m_subtitleOcr
                && (m_subtitleOcr->executionRoute() == QStringLiteral("colab-gpu")
                    ? m_subtitleOcr->colabRouteReady() : m_subtitleOcr->localRouteReady());
            const bool sourceReady = transcriptSource == QStringLiteral("ocr") ? hasMedia
                : transcriptSource == QStringLiteral("reconcile")
                    ? (!sttSegments.isEmpty() && !ocrSegments.isEmpty()) : audioReady;
            const bool completed = transcriptSource == QStringLiteral("ocr") ? !ocrSegments.isEmpty()
                : transcriptSource == QStringLiteral("reconcile") ? !reconciledSegments.isEmpty()
                : !sttSegments.isEmpty();
            state = completed ? QStringLiteral("completed")
                : (!sourceReady || (transcriptSource == QStringLiteral("ocr") && !ocrReady)
                   ? QStringLiteral("blocked") : QStringLiteral("ready"));
            detail = completed ? QStringLiteral("%1 segments").arg(
                transcriptSource == QStringLiteral("ocr") ? ocrSegments.size()
                : transcriptSource == QStringLiteral("reconcile") ? reconciledSegments.size()
                : sttSegments.size())
                : transcriptSource == QStringLiteral("ocr")
                    ? QStringLiteral("Subtitle OCR transcript source")
                    : transcriptSource == QStringLiteral("reconcile")
                        ? QStringLiteral("Run or upload STT and OCR first, then reconcile")
                        : QStringLiteral("Speech-to-text source stage");
        } else if (definition.id == QStringLiteral("review-transcript")) {
            state = hasSegments ? QStringLiteral("completed") : QStringLiteral("blocked");
            detail = hasSegments ? QStringLiteral("Transcript available for review") : QStringLiteral("Transcribe source media first");
        } else if (definition.id == QStringLiteral("translate")) {
            const int unresolved = unresolvedTranscriptConflictCount();
            state = unresolved > 0 ? QStringLiteral("blocked")
                : (!translationReady ? QStringLiteral("blocked")
                    : (hasTargets ? QStringLiteral("completed")
                       : (hasSegments ? QStringLiteral("ready") : QStringLiteral("missing"))));
            detail = unresolved > 0
                ? QStringLiteral("Resolve %1 STT/OCR conflict(s) before Translate.").arg(unresolved)
                : (!translationReady ? QStringLiteral("Choose a target language")
                   : (hasTargets ? QStringLiteral("Target text available")
                                 : QStringLiteral("Translate with CrispASR")));
            provider = QStringLiteral("Local translation runtime");
        } else if (definition.id == QStringLiteral("review-translation")) {
            state = hasTargets ? QStringLiteral("completed") : QStringLiteral("blocked");
            detail = hasTargets ? QStringLiteral("Translated transcript available for review") : QStringLiteral("Translate the transcript first");
        } else if (definition.id == QStringLiteral("assign-voices")) {
            const bool voicesReady = hasTargets && !m_project.speakers.isEmpty();
            state = voicesReady ? QStringLiteral("ready") : QStringLiteral("blocked");
            detail = voicesReady ? QStringLiteral("Speaker assignments are ready") : QStringLiteral("Translated transcript and a speaker are required");
        } else if (definition.id == QStringLiteral("synthesize")) {
            const bool cloneVoiceReady = cloneVoiceSelectionValid();
            state = !cloneVoiceReady ? QStringLiteral("blocked")
                : (!ttsReady ? QStringLiteral("missing")
                   : (hasClips ? QStringLiteral("completed")
                      : (allTargets ? QStringLiteral("ready") : QStringLiteral("blocked"))));
            const QString defaultVoice = automaticDefaultFamilyId(
                QStringLiteral("tts"), m_project.dubbingQuality);
            detail = !cloneVoiceReady ? cloneVoiceSelectionError()
                : (ttsReady ? QStringLiteral("TTS model loaded")
                              : (m_project.dubbingQuality == QStringLiteral("custom")
                                     ? QStringLiteral("Choose a TTS model")
                                     : QStringLiteral("Default: %1").arg(defaultVoice)));
            provider = m_project.dubbingQuality == QStringLiteral("custom")
                ? QStringLiteral("No model configured")
                : (defaultVoice == QStringLiteral("vieneu-tts-v2-turbo")
                       ? QStringLiteral("VieNeu-TTS v2 Turbo (default)")
                       : QStringLiteral("OmniVoice (default)"));
        } else if (definition.id == QStringLiteral("fit-timing")) {
            state = !hasClips ? QStringLiteral("blocked") : (hasConflict ? QStringLiteral("blocked") : QStringLiteral("completed"));
            detail = hasConflict ? QStringLiteral("One or more clips exceed the fit tolerance") : QStringLiteral("Fit generated clips to segment timing");
        } else if (definition.id == QStringLiteral("review-conflicts")) {
            state = hasConflict ? QStringLiteral("blocked") : (hasClips ? QStringLiteral("completed") : QStringLiteral("blocked"));
            detail = hasConflict ? QStringLiteral("Review timing conflicts") : QStringLiteral("No timing conflicts pending");
        } else if (definition.id == QStringLiteral("mix")) {
            state = hasClips && !hasConflict ? QStringLiteral("ready") : QStringLiteral("blocked");
            detail = hasClips ? QStringLiteral("Mix generated clips with background audio") : QStringLiteral("Generate segment audio first");
        } else if (definition.id == QStringLiteral("export")) {
            state = !hasClips ? QStringLiteral("missing") : (!previewPath().isEmpty() ? QStringLiteral("completed") : QStringLiteral("ready"));
            detail = !hasClips ? QStringLiteral("Generate translated audio first") : (!previewPath().isEmpty() ? QStringLiteral("Preview rendered") : QStringLiteral("Render the mixed audio"));
        }
        if (workflowWaitingForInput() && definition.id == m_workflowRunner->activeNodeId()) {
            state = QStringLiteral("waiting_for_input");
            detail = QStringLiteral("Review is waiting for your decision");
        } else if (m_workflowRunner && m_workflowRunner->running()
                   && definition.id == m_workflowRunner->activeNodeId()) {
            state = QStringLiteral("running");
            detail = m_workflowMode != QStringLiteral("automatic")
                    || m_automaticStatusText.isEmpty()
                ? QStringLiteral("Node is running") : m_automaticStatusText;
        }
        const auto stageTitleForNode = [](const QString &id) {
            if (id == QStringLiteral("media-input")) return QStringLiteral("Import/Download");
            if (id == QStringLiteral("ingest")) return QStringLiteral("Normalize");
            if (id == QStringLiteral("source-separate")) return QStringLiteral("Isolator");
            if (id == QStringLiteral("transcribe")) return QStringLiteral("Transcribe/STT");
            if (id == QStringLiteral("review-transcript")) return QStringLiteral("Transcribe/STT");
            if (id == QStringLiteral("translate")) return QStringLiteral("Translate");
            if (id == QStringLiteral("review-translation")) return QStringLiteral("Subtitle");
            if (id == QStringLiteral("assign-voices") || id == QStringLiteral("synthesize")) return QStringLiteral("TTS");
            if (id == QStringLiteral("fit-timing") || id == QStringLiteral("review-conflicts")) return QStringLiteral("Alignment");
            if (id == QStringLiteral("mix")) return QStringLiteral("Export/Output");
            return QStringLiteral("Export/Output");
        };
        const QString displayTitle = stageTitleForNode(definition.id);
        QVariantMap item = node(definition.id, displayTitle, state, detail, provider).toMap();
        item.insert(QStringLiteral("displayStageTitle"), displayTitle);
        item.insert(QStringLiteral("parameters"), definition.parameters);
        item.insert(QStringLiteral("typeId"), definition.typeId);
        item.insert(QStringLiteral("typeVersion"), definition.typeVersion);
        if (definition.id == QStringLiteral("source-separate")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("voice-isolation"));
        } else if (definition.id == QStringLiteral("transcribe")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("stt"));
        } else if (definition.id == QStringLiteral("translate")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("translation"));
        } else if (definition.id == QStringLiteral("synthesize")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("tts"));
        }
        if (item.value(QStringLiteral("configurable")).toBool()
            && m_project.dubbingQuality != QStringLiteral("custom")) {
            item.insert(
                QStringLiteral("defaultFamilyId"),
                automaticDefaultFamilyId(
                    item.value(QStringLiteral("capabilityId")).toString(),
                    m_project.dubbingQuality));
        }
        const QVariantMap selected = m_workflowNodeConfigurations.value(definition.id).toMap();
        if (!selected.isEmpty()) {
            item.insert(QStringLiteral("providerName"), selected.value(QStringLiteral("modelName")));
            item.insert(QStringLiteral("selectedFamilyId"), selected.value(QStringLiteral("familyId")));
            item.insert(QStringLiteral("selectedRuntimeId"), selected.value(QStringLiteral("runtimeId")));
            item.insert(QStringLiteral("supportsVoiceCloning"),
                        selected.value(QStringLiteral("supportsVoiceCloning")).toBool());
            const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
            IModelSession *session = AppController::instance() && AppController::instance()->sessionRegistry()
                ? AppController::instance()->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
            item.insert(QStringLiteral("providerState"),
                        session && session->canProcess() ? QStringLiteral("ready") : QStringLiteral("loading"));
            const ModelSessionState modelState = session ? session->state() : ModelSessionState::Unconfigured;
            item.insert(QStringLiteral("modelState"), static_cast<int>(modelState));
            item.insert(QStringLiteral("modelStateText"),
                        modelState == ModelSessionState::Ready ? QStringLiteral("ready")
                        : modelState == ModelSessionState::Loading ? QStringLiteral("loading")
                        : modelState == ModelSessionState::Unloading ? QStringLiteral("unloading")
                        : modelState == ModelSessionState::Processing ? QStringLiteral("processing")
                        : modelState == ModelSessionState::Error ? QStringLiteral("error")
                        : QStringLiteral("unloaded"));
            QVariantMap parameters = item.value(QStringLiteral("parameters")).toMap();
            const QVariantMap customParameters = selected.value(QStringLiteral("parameters")).toMap();
            for (auto it = customParameters.cbegin(); it != customParameters.cend(); ++it)
                parameters.insert(it.key(), it.value());
            item.insert(QStringLiteral("parameters"), parameters);
            const QString providerId = selected.value(
                QStringLiteral("executionProvider"),
                customParameters.value(QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString();
            ExecutionProvider executionProvider = ExecutionProvider::LocalDev;
            if (executionProviderFromId(providerId, &executionProvider)
                && executionProvider != ExecutionProvider::LocalDev) {
                const QString modelId = selected.value(
                    QStringLiteral("modelId"), customParameters.value(QStringLiteral("modelId"))).toString();
                item.insert(QStringLiteral("executionProvider"), executionProviderId(executionProvider));
                item.insert(QStringLiteral("providerName"),
                            modelId.isEmpty() ? executionProviderDisplayName(executionProvider)
                                              : QStringLiteral("%1 · %2").arg(executionProviderDisplayName(executionProvider), modelId));
                item.insert(QStringLiteral("providerState"), QStringLiteral("selected"));
            } else {
                item.insert(QStringLiteral("executionProvider"), QStringLiteral("local-dev"));
            }
            QVariantList runtimeSchema;
            if (capabilityId == QStringLiteral("tts") && m_tts) {
                const QString signature = selected.value(QStringLiteral("configurationSignature")).toString();
                if (!signature.isEmpty() && m_tts->instance(signature))
                    runtimeSchema = m_tts->instance(signature)->schemaForCapability(QStringLiteral("tts"));
                else
                    runtimeSchema = m_tts->schemaForCapability(QStringLiteral("tts"));
            }
            const QVariantMap familyConfig = selected.value(QStringLiteral("familyConfig")).toMap();
            const QVariantMap studioConfig = familyConfig.value(QStringLiteral("studio")).toMap()
                .value(capabilityId).toMap();
            const QVariantList parameterSchema = CapabilitySettingsSchema::merge(
                familyConfig, capabilityId, runtimeSchema);
            item.insert(QStringLiteral("parameterSchema"), parameterSchema);
            item.insert(QStringLiteral("studioConfig"), studioConfig);
        }
        const QVariantMap effectiveParameters = item.value(QStringLiteral("parameters")).toMap();
        const QString configuredProvider = item.value(QStringLiteral("executionProvider"),
            effectiveParameters.value(QStringLiteral("executionProvider"),
                                      QStringLiteral("local-dev"))).toString().trimmed().toLower();
        const QString configuredModel = effectiveParameters.value(QStringLiteral("modelId")).toString().trimmed();
        const auto roleForNode = [](const QString &id) {
            if (id == QStringLiteral("media-input")) return QStringLiteral("Choose the original audio or video used by the project.");
            if (id == QStringLiteral("ingest")) return QStringLiteral("Inspect media and create normalized working audio.");
            if (id == QStringLiteral("source-separate")) return QStringLiteral("Create Vocals and Background stems for the mix.");
            if (id == QStringLiteral("transcribe")) return QStringLiteral("Create timed text from speech, subtitles, or both.");
            if (id == QStringLiteral("review-transcript")) return QStringLiteral("Review the transcript before translation.");
            if (id == QStringLiteral("translate")) return QStringLiteral("Translate reviewed timed text into the target language.");
            if (id == QStringLiteral("review-translation")) return QStringLiteral("Review translated text before speech synthesis.");
            if (id == QStringLiteral("assign-voices")) return QStringLiteral("Choose one TTS voice applied to all segments and speakers.");
            if (id == QStringLiteral("synthesize")) return QStringLiteral("Generate timed speech with the selected TTS voice.");
            if (id == QStringLiteral("fit-timing")) return QStringLiteral("Fit generated clips to the reviewed timing.");
            if (id == QStringLiteral("review-conflicts")) return QStringLiteral("Resolve clips whose timing needs a decision.");
            if (id == QStringLiteral("mix")) return QStringLiteral("Mix generated speech with the background audio.");
            return QStringLiteral("Write the verified dub and subtitles to the chosen output.");
        };
        const bool ocrUsesColab = definition.id == QStringLiteral("transcribe")
            && normalizedTranscriptSource(m_project.transcriptConfiguration.value(
                    QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString())
                   == QStringLiteral("ocr")
            && m_subtitleOcr && m_subtitleOcr->executionRoute() == QStringLiteral("colab-gpu");
        const bool colabHeavy = configuredProvider == QStringLiteral("colab-direct") || ocrUsesColab;
        item.insert(QStringLiteral("roleDescription"), roleForNode(definition.id));
        item.insert(QStringLiteral("showColabRecommendation"), colabHeavy);
        item.insert(QStringLiteral("resourceText"), colabHeavy ? QStringLiteral("Nên dùng Colab")
            : configuredProvider == QStringLiteral("api-gateway") ? QStringLiteral("API Gateway")
            : QStringLiteral("CPU phù hợp"));
        item.insert(QStringLiteral("resourceReason"), colabHeavy
            ? QStringLiteral("The selected Direct Colab route runs its exact model on the temporary GPU worker.")
            : configuredProvider == QStringLiteral("api-gateway")
                ? QStringLiteral("This node sends requests only to the configured API Gateway.")
                : QStringLiteral("This stage is executed locally without a required GPU worker."));
        if (colabHeavy)
            item.insert(QStringLiteral("notebookFile"), DubbingColabModelRoutes::notebookForModel(
                ocrUsesColab ? QStringLiteral("subtitle-ocr") : definition.id,
                ocrUsesColab ? m_subtitleOcr->colabModelId() : configuredModel));
        const bool isReadyOrComplete = (state == QStringLiteral("ready") || state == QStringLiteral("completed"));
        const bool isCompleted = (state == QStringLiteral("completed"));
        item.insert(QStringLiteral("canRun"), isReadyOrComplete);
        item.insert(QStringLiteral("completed"), isCompleted);
        item.insert(QStringLiteral("runReady"), isReadyOrComplete);
        result.append(item);
    }
    return result;
}

QVariantList DubbingController::workflowStages() const
{
    // These are intentionally not new graph nodes.  They are a stable
    // presentation contract over the persisted node ids, which preserves
    // existing project journals, artifacts and rerun/resume behaviour.
    const QVariantList nodes = workflowNodes();
    QHash<QString, QVariantMap> byId;
    for (const QVariant &entry : nodes) {
        const QVariantMap node = entry.toMap();
        byId.insert(node.value(QStringLiteral("id")).toString(), node);
    }

    struct StageDefinition {
        const char *id;
        const char *title;
        const char *actionNodeId;
        const char *description;
        QStringList nodeIds;
    };
    const QList<StageDefinition> definitions{
        {"import", "Import/Download", "media-input",
         "Choose a local media file or download/import an approved URL.",
         {QStringLiteral("media-input")}},
        {"normalize", "Normalize", "ingest",
         "Probe media and create the master and analysis audio used downstream.",
         {QStringLiteral("ingest")}},
        {"isolator", "Isolator", "source-separate",
         "Create real Vocals and Background stems for review and mixing.",
         {QStringLiteral("source-separate")}},
        {"transcribe", "Transcribe/STT", "transcribe",
         "Create and review timed source text from STT, Subtitle OCR, or the reviewed STT + OCR mode.",
         {QStringLiteral("transcribe"), QStringLiteral("review-transcript")}},
        {"alignment-subtitle", "Alignment/Subtitle", "fit-timing",
         "Configure timing resolution and subtitle output without exposing internal timing nodes as separate user stages.",
         {QStringLiteral("fit-timing"), QStringLiteral("review-conflicts")}},
        {"translate", "Translate", "translate",
         "Translate and review the timed target-language text.",
         {QStringLiteral("translate"), QStringLiteral("review-translation")}},
        {"tts", "TTS", "synthesize",
         "Assign a voice and synthesize the translated segments.",
         {QStringLiteral("assign-voices"), QStringLiteral("synthesize")}},
        {"export", "Export/Output", "export",
         "Mix/render the verified dub and export media, subtitles, a package, or a CapCut Draft.",
         {QStringLiteral("mix"), QStringLiteral("export")}}
    };

    const QHash<QString, int> priority{
        {QStringLiteral("completed"), 0}, {QStringLiteral("ready"), 1},
        {QStringLiteral("missing"), 2}, {QStringLiteral("blocked"), 3},
        {QStringLiteral("waiting_for_input"), 4}, {QStringLiteral("running"), 5}
    };
    const QString activeNodeId = currentStepId();
    QVariantList result;
    for (const StageDefinition &definition : definitions) {
        QVariantMap stage;
        QVariantList productionNodes;
        QVariantMap actionNode = byId.value(QString::fromLatin1(definition.actionNodeId));
        QString state = QStringLiteral("completed");
        int statePriority = -1;
        QString detail;
        bool active = false;
        for (const QString &nodeId : definition.nodeIds) {
            const QVariantMap node = byId.value(nodeId);
            if (node.isEmpty()) continue;
            productionNodes.append(nodeId);
            const QString candidate = node.value(QStringLiteral("state")).toString();
            const int candidatePriority = priority.value(candidate, 3);
            if (candidatePriority > statePriority) {
                statePriority = candidatePriority;
                state = candidate;
                detail = node.value(QStringLiteral("detail")).toString();
            }
            active = active || activeNodeId == nodeId;
        }
        // STT and Subtitle OCR are independent Dubbing operations. A saved
        // STT transcript is evidence that the Transcribe/STT task completed,
        // even when the optional STT + OCR reconciliation is still pending.
        // A fresh STT run keeps its visible running state while it replaces an
        // earlier transcript.
        if (QString::fromLatin1(definition.id) == QStringLiteral("transcribe")
            && state != QStringLiteral("running")
            && state != QStringLiteral("waiting_for_input")) {
            const QVariantList savedSttSegments = m_project.transcriptConfiguration.value(
                QStringLiteral("sttSegments")).toList();
            if (!savedSttSegments.isEmpty()) {
                state = QStringLiteral("completed");
                detail = QStringLiteral("STT transcript saved (%1 segments); Subtitle OCR and reconciliation remain independent.")
                    .arg(savedSttSegments.size());
            }
        }
        if (detail.isEmpty()) detail = QString::fromLatin1(definition.description);
        stage.insert(QStringLiteral("id"), QString::fromLatin1(definition.id));
        stage.insert(QStringLiteral("title"), QString::fromLatin1(definition.title));
        stage.insert(QStringLiteral("description"), QString::fromLatin1(definition.description));
        stage.insert(QStringLiteral("actionNodeId"), QString::fromLatin1(definition.actionNodeId));
        stage.insert(QStringLiteral("productionNodeIds"), productionNodes);
        stage.insert(QStringLiteral("state"), state);
        stage.insert(QStringLiteral("detail"), detail);
        stage.insert(QStringLiteral("active"), active);
        stage.insert(QStringLiteral("configurable"), actionNode.value(QStringLiteral("configurable")).toBool());
        stage.insert(QStringLiteral("capabilityId"), actionNode.value(QStringLiteral("capabilityId")));
        stage.insert(QStringLiteral("executionProvider"), actionNode.value(QStringLiteral("executionProvider")));
        stage.insert(QStringLiteral("providerName"), actionNode.value(QStringLiteral("providerName")));
        stage.insert(QStringLiteral("resourceText"), actionNode.value(QStringLiteral("resourceText")));
        stage.insert(QStringLiteral("resourceReason"), actionNode.value(QStringLiteral("resourceReason")));
        stage.insert(QStringLiteral("notebookFile"), actionNode.value(QStringLiteral("notebookFile")));
        result.append(stage);
    }
    return result;
}

bool DubbingController::workflowReady() const
{
    const QVariantMap sttSelection = m_workflowNodeConfigurations
        .value(QStringLiteral("transcribe")).toMap();
    const QVariantMap sttParameters = sttSelection.value(QStringLiteral("parameters")).toMap();
    // The transcript route is independently persisted so a reopened project
    // keeps the exact STT worker selected in the Transcribe/Colab setup UI.
    // Do not fall back to the workflow-template default here: readiness must
    // describe the route that will actually execute the project.
    const QString persistedSttProvider = m_project.transcriptConfiguration.value(
        QStringLiteral("sttExecutionProvider")).toString().trimmed();
    const QString persistedSttModel = m_project.transcriptConfiguration.value(
        QStringLiteral("sttModelId")).toString().trimmed();
    const QString configuredSttProvider = sttSelection.value(
        QStringLiteral("executionProvider"), sttParameters.value(
        QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString();
    const QString configuredSttModel = sttSelection.value(
        QStringLiteral("modelId"), sttParameters.value(QStringLiteral("modelId"))).toString().trimmed();
    const QString sttProviderId = persistedSttProvider.isEmpty()
        ? configuredSttProvider : persistedSttProvider;
    const QString sttModelId = persistedSttModel.isEmpty()
        ? configuredSttModel : persistedSttModel;
    ExecutionProvider sttProvider = ExecutionProvider::LocalDev;
    const bool remoteSttSelected = executionProviderFromId(sttProviderId, &sttProvider)
        && sttProvider != ExecutionProvider::LocalDev
        && !sttModelId.isEmpty()
        && (sttProvider != ExecutionProvider::ColabDirect
            || DubbingColabModelRoutes::supports(
                QStringLiteral("transcribe"), sttModelId));
    const bool sttReady = remoteSttSelected || (AppController::instance() && AppController::instance()->sessionRegistry()
        && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("stt"))
        && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("stt"))->canProcess());
    const QString transcriptSource = normalizedTranscriptSource(
        m_project.transcriptConfiguration.value(
            QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
    const bool ocrReady = m_subtitleOcr
        && (m_subtitleOcr->executionRoute() == QStringLiteral("colab-gpu")
            ? m_subtitleOcr->colabRouteReady() : m_subtitleOcr->localRouteReady());
    const bool transcriptReady = transcriptSource == QStringLiteral("ocr") ? ocrReady
        : transcriptSource == QStringLiteral("reconcile")
            ? (!m_project.transcriptConfiguration.value(QStringLiteral("sttSegments")).toList().isEmpty()
               && !m_project.transcriptConfiguration.value(QStringLiteral("ocrSegments")).toList().isEmpty())
            : sttReady;
    const bool translationConfigured = !m_workflowNodeConfigurations.value(QStringLiteral("translate")).toMap().isEmpty();
    bool translatedArtifactReady = !m_project.segments.isEmpty();
    for (const QVariant &entry : m_project.segments) {
        if (entry.toMap().value(QStringLiteral("targetText")).toString().trimmed().isEmpty()) {
            translatedArtifactReady = false;
            break;
        }
    }
    bool configuredTranslationReady = false;
    if (translationConfigured) {
        const QVariantMap selected = m_workflowNodeConfigurations
                                         .value(QStringLiteral("translate")).toMap();
        const QVariantMap parameters = selected.value(QStringLiteral("parameters")).toMap();
        ExecutionProvider provider = ExecutionProvider::LocalDev;
        const QString providerId = selected.value(
            QStringLiteral("executionProvider"), parameters.value(
            QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString();
        if (executionProviderFromId(providerId, &provider)
            && provider != ExecutionProvider::LocalDev) {
            const QString remoteModel = selected.value(
                QStringLiteral("modelId"),
                parameters.value(QStringLiteral("modelId"))).toString().trimmed();
            configuredTranslationReady = !remoteModel.isEmpty()
                && (provider != ExecutionProvider::ColabDirect
                    || DubbingColabModelRoutes::supports(
                        QStringLiteral("translate"), remoteModel));
        } else {
            StudioConfiguration configuration;
            configuration.capabilityId = QStringLiteral("translation");
            configuration.familyId = selected.value(QStringLiteral("familyId")).toString();
            configuration.runtimeId = selected.value(QStringLiteral("runtimeId")).toString();
            configuration.runtimeVersion = selected.value(QStringLiteral("runtimeVersion")).toString();
            configuration.selectedFiles = selected.value(QStringLiteral("selectedFiles")).toMap();
            configuredTranslationReady = StudioConfigurationResolver::resolve(configuration).isValid;
        }
    }
    const bool translationReady = !translationConfigured || translatedArtifactReady
        || configuredTranslationReady;
    const QVariantMap synthesisSelection = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    const QVariantMap synthesisParameters = synthesisSelection.value(QStringLiteral("parameters")).toMap();
    ExecutionProvider synthesisProvider = ExecutionProvider::LocalDev;
    const QString synthesisProviderId = synthesisSelection.value(
        QStringLiteral("executionProvider"), synthesisParameters.value(
        QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString();
    const QVariantMap savedClonePreset = selectedCloneVoicePreset();
    const QString savedCloneFamily = savedClonePreset.value(
        QStringLiteral("voiceCloneModelId"), savedClonePreset.value(
        QStringLiteral("familyId"))).toString().trimmed().toLower();
    const bool savedCloneDirect = executionProviderFromId(synthesisProviderId, &synthesisProvider)
        && synthesisProvider == ExecutionProvider::ColabDirect
        && !savedCloneFamily.isEmpty()
        && DubbingColabModelRoutes::supports(QStringLiteral("voice-cloning"), savedCloneFamily);
    const bool remoteTtsSelected = savedCloneDirect
        || (executionProviderFromId(synthesisProviderId, &synthesisProvider)
            && synthesisProvider != ExecutionProvider::LocalDev
            && !synthesisSelection.value(QStringLiteral("modelId"), synthesisParameters.value(
                QStringLiteral("modelId"))).toString().trimmed().isEmpty()
            && (synthesisProvider != ExecutionProvider::ColabDirect
                || DubbingColabModelRoutes::supports(
                    QStringLiteral("synthesize"),
                    synthesisSelection.value(QStringLiteral("modelId"),
                        synthesisParameters.value(QStringLiteral("modelId"))).toString())));
    const bool ttsReady = remoteTtsSelected || (m_tts && m_tts->isModelLoaded());
    return workflowGraphValid()
        && QFileInfo(m_project.sourceMediaPath).isFile()
        && !m_project.targetLanguage.trimmed().isEmpty()
        && cloneVoiceSelectionValid()
        && ttsReady
        && transcriptReady
        && !hasUnresolvedTranscriptConflicts()
        && translationReady;
}

bool DubbingController::setWorkflowNodeModel(const QString &nodeId,
                                             const QString &familyId,
                                             const QString &runtimeId,
                                             const QString &runtimeVersion,
                                             const QVariantMap &selectedFiles)
{
    return configureWorkflowNodeModel(nodeId, familyId, runtimeId, runtimeVersion,
                                      selectedFiles, true);
}

bool DubbingController::configureWorkflowNodeModel(const QString &nodeId,
                                                   const QString &familyId,
                                                   const QString &runtimeId,
                                                   const QString &runtimeVersion,
                                                   const QVariantMap &selectedFiles,
                                                   bool loadSession)
{
    QString capabilityId;
    if (nodeId == QStringLiteral("source-separate")) capabilityId = QStringLiteral("voice-isolation");
    else if (nodeId == QStringLiteral("transcribe")) capabilityId = QStringLiteral("stt");
    else if (nodeId == QStringLiteral("translate")) capabilityId = QStringLiteral("translation");
    else if (nodeId == QStringLiteral("synthesize")) capabilityId = QStringLiteral("tts");
    else {
        setError(QStringLiteral("This workflow node does not support model selection."));
        return false;
    }

    AppController *app = AppController::instance();
    if (!app || !app->registry() || !app->sessionRegistry()) return false;
    const QVariantList families = capabilityId == QStringLiteral("stt")
            || capabilityId == QStringLiteral("voice-isolation")
        ? app->registry()->sttFamilies()
        : (capabilityId == QStringLiteral("translation") ? app->registry()->translationFamilies()
                                                           : app->registry()->ttsFamilies());
    QVariantMap family;
    for (const QVariant &entry : families) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == familyId) {
            family = candidate;
            break;
        }
    }
    if (family.isEmpty()) {
        setError(QStringLiteral("The selected model family is not available."));
        return false;
    }

    QVariantMap runtime;
    for (const QVariant &entry : family.value(QStringLiteral("runtimes")).toList()) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == runtimeId) {
            runtime = candidate;
            break;
        }
    }
    if (runtime.isEmpty()) {
        setError(QStringLiteral("The selected runtime is not compatible with this model."));
        return false;
    }

    StudioConfiguration config;
    config.capabilityId = capabilityId;
    config.familyId = familyId;
    config.runtimeId = runtimeId;
    config.runtimeVersion = runtimeVersion.isEmpty()
        ? runtime.value(QStringLiteral("version")).toString() : runtimeVersion;
    for (const QVariant &entry : family.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap file = entry.toMap();
        const QString role = file.value(QStringLiteral("role")).toString();
        config.selectedFiles.insert(role, selectedFiles.value(role, file.value(QStringLiteral("file"))).toString());
    }
    const auto resolved = StudioConfigurationResolver::resolve(config);
    if (!resolved.isValid) {
        setError(QStringLiteral("The selected model files or runtime are not installed."));
        return false;
    }

    const bool supportsVoiceCloning = family.value(QStringLiteral("supportsCloning")).toBool()
        || family.value(QStringLiteral("capabilities")).toStringList().contains(QStringLiteral("voice-cloning"));
    QVariantMap parameters = m_workflowNodeConfigurations.value(nodeId).toMap()
                                 .value(QStringLiteral("parameters")).toMap();
    // Source-window auto selection was intentionally removed.  Clone voice
    // now always comes from the project-level preset selected by the user.
    parameters.remove(QStringLiteral("autoSelectVoiceReference"));
    parameters.remove(QStringLiteral("autoReferenceSourcePath"));

    const bool isOmniVoice = familyId.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive);
    if (nodeId == QStringLiteral("synthesize") && isOmniVoice && supportsVoiceCloning) {
        if (!parameters.contains(QStringLiteral("forceSegmentDuration")))
            parameters.insert(QStringLiteral("forceSegmentDuration"), true);
    }
    if (nodeId == QStringLiteral("synthesize")
        && !parameters.contains(QStringLiteral("lang"))) {
        parameters.insert(QStringLiteral("lang"), m_project.targetLanguage);
    }

    QVariantMap selected{{QStringLiteral("executionProvider"), QStringLiteral("local-dev")},
                         {QStringLiteral("modelId"), familyId},
                         {QStringLiteral("familyId"), familyId},
                         {QStringLiteral("runtimeId"), config.runtimeId},
                         {QStringLiteral("runtimeVersion"), config.runtimeVersion},
                          {QStringLiteral("selectedFiles"), config.selectedFiles},
                          {QStringLiteral("modelName"), family.value(QStringLiteral("title"))},
                          {QStringLiteral("capabilityId"), capabilityId},
                          {QStringLiteral("configurationSignature"), resolved.signature},
                          {QStringLiteral("supportsVoiceCloning"), supportsVoiceCloning},
                          {QStringLiteral("parameters"), parameters},
                          {QStringLiteral("familyConfig"), family},
                          {QStringLiteral("parameterDefinitions"), family.value(QStringLiteral("parameterDefinitions"))},
                          {QStringLiteral("studioConfig"),
                           family.value(QStringLiteral("studio")).toMap().value(capabilityId)}};
    m_workflowNodeConfigurations.insert(nodeId, selected);
    // A route/model selected by the user is a durable preflight contract in
    // every quality mode; a template default must not replace it on reopen.
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    if (loadSession) {
        unloadConflictingDubbingRuntime(app->sessionRegistry(), capabilityId);
        if (IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId)) {
            session->requestLoad(capabilityId, config);
        }
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Workflow node model changed node=%1 family=%2 runtime=%3")
                     .arg(nodeId, familyId, config.runtimeId));
    persistAfterEdit();
    if (nodeId == QStringLiteral("synthesize")) {
        refreshCloneVoicePresets();
        emit cloneVoiceSelectionChanged();
    }
    emit workflowChanged();
    return true;
}

bool DubbingController::loadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    if (selected.isEmpty()) {
        setError(QStringLiteral("Choose a model configuration before loading this node."));
        return false;
    }
    return setWorkflowNodeModel(nodeId, selected.value(QStringLiteral("familyId")).toString(),
                                selected.value(QStringLiteral("runtimeId")).toString(),
                                selected.value(QStringLiteral("runtimeVersion")).toString(),
                                selected.value(QStringLiteral("selectedFiles")).toMap());
}

bool DubbingController::unloadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
    auto *app = AppController::instance();
    auto *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
    if (!session || capabilityId.isEmpty()) return false;
    session->requestUnload(capabilityId);
    emit workflowChanged();
    return true;
}

bool DubbingController::reloadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
    auto *app = AppController::instance();
    auto *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
    if (!session || capabilityId.isEmpty()) return false;
    session->requestReload(capabilityId);
    emit workflowChanged();
    return true;
}

bool DubbingController::setWorkflowNodeParameters(const QString &nodeId, const QVariantMap &parameters)
{
    if (nodeId.isEmpty()) return false;
    if ((nodeId == QStringLiteral("source-separate") || nodeId == QStringLiteral("transcribe")
         || nodeId == QStringLiteral("translate")
         || nodeId == QStringLiteral("synthesize"))
        && parameters.contains(QStringLiteral("executionProvider"))) {
        ExecutionProvider provider = ExecutionProvider::LocalDev;
        if (!executionProviderFromId(parameters.value(QStringLiteral("executionProvider")).toString(), &provider)) {
            setError(QStringLiteral("Unknown remote execution provider."));
            return false;
        }
        if (nodeId == QStringLiteral("source-separate")
            && provider == ExecutionProvider::ApiGateway) {
            setError(QStringLiteral("Source separation supports Local Dev or Colab GPU, not API Gateway."));
            return false;
        }
    }
    QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    QVariantMap current = selected.value(QStringLiteral("parameters")).toMap();
    const QString previousProviderId = selected.value(
        QStringLiteral("executionProvider"), current.value(
        QStringLiteral("executionProvider"), QStringLiteral("local-dev"))).toString().trimmed().toLower();
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        current.insert(it.key(), it.value());
    const QString providerId = current.value(QStringLiteral("executionProvider"),
                                             QStringLiteral("local-dev")).toString().trimmed().toLower();
    ExecutionProvider provider = ExecutionProvider::LocalDev;
    if (!executionProviderFromId(providerId, &provider)) {
        setError(QStringLiteral("Unknown remote execution provider."));
        return false;
    }
    const bool routeChanged = parameters.contains(QStringLiteral("executionProvider"))
        && previousProviderId != providerId;
    if (provider != ExecutionProvider::LocalDev) {
        // Route is a contract. Do not retain local runtime/family metadata at
        // either persistence level: legacy projects kept it at the root while
        // newer selections may also have it in the nested parameters map.
        // Keeping either can make an already-remote stage look local after a
        // model reselect or project reload.
        for (const QString &key : {QStringLiteral("familyId"), QStringLiteral("runtimeId"),
                                   QStringLiteral("runtimeVersion"), QStringLiteral("selectedFiles"),
                                   QStringLiteral("modelName"), QStringLiteral("supportsVoiceCloning"),
                                   QStringLiteral("configurationSignature"),
                                   QStringLiteral("familyConfig"),
                                   QStringLiteral("parameterDefinitions"),
                                   QStringLiteral("studioConfig")}) {
            selected.remove(key);
            current.remove(key);
        }
    }
    if (routeChanged && provider != ExecutionProvider::LocalDev) {
        if (AppController *app = AppController::instance(); app && app->sessionRegistry()) {
            QString capability;
            if (nodeId == QStringLiteral("source-separate")) capability = QStringLiteral("voice-isolation");
            else if (nodeId == QStringLiteral("transcribe")) capability = QStringLiteral("stt");
            else if (nodeId == QStringLiteral("translate")) capability = QStringLiteral("translation");
            else if (nodeId == QStringLiteral("synthesize")) capability = QStringLiteral("tts");
            if (IModelSession *session = app->sessionRegistry()->sessionForCapability(capability)) {
                for (const SessionConfiguration &loaded : session->loadedConfigurations())
                    session->requestUnloadConfiguration(loaded.signature);
            }
            m_automaticDownloadsQueued.remove(capability);
        }
    }
    // A canonical root copy prevents an old Local root selection from winning
    // over the newly selected remote provider when legacy projects reopen.
    selected.insert(QStringLiteral("executionProvider"), providerId);
    const QString modelId = current.value(QStringLiteral("modelId")).toString().trimmed();
    if (provider == ExecutionProvider::ColabDirect && !modelId.isEmpty()) {
        if (!DubbingColabModelRoutes::supports(nodeId, modelId)) {
            setError(QStringLiteral("No exact Colab notebook is mapped for model '%1' on the %2 node.")
                         .arg(modelId, visibleStepForNode(nodeId)));
            return false;
        }
    } else if (provider == ExecutionProvider::ApiGateway && !modelId.isEmpty()) {
        RemoteModelCatalogController *catalog = AppController::instance()
            ? AppController::instance()->remoteModels() : nullptr;
        const bool catalogAvailable = catalog && catalog->gatewayAvailable();
        QString capability;
        if (nodeId == QStringLiteral("source-separate")) capability = QStringLiteral("voice-isolation");
        else if (nodeId == QStringLiteral("transcribe")) capability = QStringLiteral("stt");
        else if (nodeId == QStringLiteral("translate")) capability = QStringLiteral("translation");
        else if (nodeId == QStringLiteral("synthesize")) capability = QStringLiteral("tts");
        if (catalogAvailable && !catalog->isModelSelectable(providerId, modelId, capability)) {
            setError(QStringLiteral("The selected %1 model is unavailable for this node. Refresh that provider's model catalog and choose a compatible model.")
                         .arg(executionProviderDisplayName(provider)));
            return false;
        }
    }
    if (nodeId == QStringLiteral("transcribe")) {
        QString mode = current.value(QStringLiteral("transcriptSource"), QStringLiteral("stt"))
                           .toString().trimmed().toLower();
        mode = normalizedTranscriptSource(mode);
        m_project.transcriptConfiguration.insert(QStringLiteral("transcriptSource"), mode);
        if (current.contains(QStringLiteral("executionProvider"))) {
            m_project.transcriptConfiguration.insert(
                QStringLiteral("sttExecutionProvider"),
                current.value(QStringLiteral("executionProvider")));
        }
        if (current.contains(QStringLiteral("modelId"))) {
            m_project.transcriptConfiguration.insert(QStringLiteral("sttModelId"),
                                                     current.value(QStringLiteral("modelId")));
        }
        const QString fusionPolicy = DubbingTranscriptFusionService::normalizePolicy(
            current.value(QStringLiteral("fusionPolicy"),
                          m_project.transcriptConfiguration.value(
                              QStringLiteral("fusionPolicy"), QStringLiteral("ask"))).toString());
        current.insert(QStringLiteral("fusionPolicy"), fusionPolicy);
        m_project.transcriptConfiguration.insert(QStringLiteral("fusionPolicy"), fusionPolicy);
        for (const QString &key : {QStringLiteral("ocrLanguage"), QStringLiteral("ocrExecutionRoute"),
                                   QStringLiteral("ocrLocalEngineId"), QStringLiteral("ocrLocalEngineVersion"),
                                   QStringLiteral("ocrColabModelId"), QStringLiteral("ocrRoi"),
                                   QStringLiteral("ocrSampleIntervalMs"),
                                   QStringLiteral("ocrMinimumConfidence")}) {
            if (current.contains(key))
                m_project.transcriptConfiguration.insert(key, current.value(key));
        }
        applyStoredSubtitleOcrConfiguration();
    }
    selected.insert(QStringLiteral("modelId"), modelId);
    selected.insert(QStringLiteral("parameters"), current);
    m_workflowNodeConfigurations.insert(nodeId, selected);
    // A route/model selected by the user is a durable preflight contract in
    // every quality mode; a template default must not replace it on reopen.
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    persistAfterEdit();
    if (nodeId == QStringLiteral("synthesize")) {
        refreshCloneVoicePresets();
        emit cloneVoiceSelectionChanged();
    }
    emit workflowChanged();
    return true;
}

QString DubbingController::workflowStatusText() const
{
    if (processing() && m_workflowMode == QStringLiteral("automatic")
        && !m_automaticStatusText.isEmpty()) return m_automaticStatusText;
    if (processing()) {
        return progressAvailable()
            ? QStringLiteral("Running %1 (%2%)").arg(stage()).arg(progress())
            : QStringLiteral("Running %1").arg(stage());
    }
    if (workflowReady()) return QStringLiteral("Workflow configured and ready to run");
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        return customStatusText();
    return QStringLiteral("Configure media, transcript, target text, and a TTS model");
}


bool DubbingController::runWorkflow(const QString &outputPath)
{
    if (m_dubbingEntryGateActive) {
        setError(QStringLiteral("Choose an entry mode before running the Dubbing workflow."));
        return false;
    }
    if (!m_workflowRunner || m_workflowRunner->running()) return false;
    if (workflowRecoveryAvailable()) {
        setError(QStringLiteral("Resume or discard the interrupted workflow before starting a new run."));
        return false;
    }
    if (PathUtils::urlToLocalPath(outputPath).trimmed().isEmpty()) {
        setError(QStringLiteral("Choose an output path before running the full dubbing workflow."));
        return false;
    }
    if (!workflowGraphValid() || !QFileInfo(m_project.sourceMediaPath).isFile()) {
        setError(QStringLiteral("Import source media before running the dubbing workflow."));
        return false;
    }
    if (!cloneVoiceSelectionValid()) {
        setError(cloneVoiceSelectionError());
        return false;
    }
    if (!snapshotSelectedColabStagesForWorkflow()) return false;
    const QVariantMap transcriptConfiguration = effectiveTranscriptConfiguration(true);
    persistAfterEdit();
    WorkflowGraph graph = DubbingWorkflowDefinition::create();
    QVariantMap effectiveDurationControl = m_project.durationControl;
    effectiveDurationControl.insert(
        QStringLiteral("autoRewrite"),
        m_project.dubbingQuality != QStringLiteral("fast")
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
    for (auto &node : graph.nodes) {
        if (node.id == QStringLiteral("translate"))
            node.parameters.insert(QStringLiteral("durationControl"), effectiveDurationControl);
        const QVariantMap modelConfig = m_workflowNodeConfigurations.value(node.id).toMap();
        if (!modelConfig.isEmpty()) {
            node.parameters.insert(QStringLiteral("familyId"), modelConfig.value(QStringLiteral("familyId")));
            node.parameters.insert(QStringLiteral("runtimeId"), modelConfig.value(QStringLiteral("runtimeId")));
            node.parameters.insert(QStringLiteral("runtimeVersion"), modelConfig.value(QStringLiteral("runtimeVersion")));
            node.parameters.insert(QStringLiteral("selectedFiles"), modelConfig.value(QStringLiteral("selectedFiles")));
            const QVariantMap customParameters = modelConfig.value(QStringLiteral("parameters")).toMap();
            for (auto it = customParameters.cbegin(); it != customParameters.cend(); ++it)
                node.parameters.insert(it.key(), it.value());
            node.properties = node.parameters;
        }
        if (node.id == QStringLiteral("media-input")) {
            node.parameters.insert(QStringLiteral("value"), m_project.sourceMediaPath);
            node.properties.insert(QStringLiteral("value"), m_project.sourceMediaPath);
        } else if (node.id == QStringLiteral("transcribe")) {
            const QVariantMap persistedParameters = transcriptConfiguration.value(
                QStringLiteral("parameters")).toMap();
            for (auto it = persistedParameters.cbegin(); it != persistedParameters.cend(); ++it)
                node.parameters.insert(it.key(), it.value());
            node.parameters.insert(QStringLiteral("language"),
                                   m_project.sourceLanguage.trimmed().isEmpty()
                                       ? QStringLiteral("zh") : m_project.sourceLanguage);
            node.parameters.insert(QStringLiteral("ocrSourceMedia"), m_project.sourceMediaPath);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("translate")) {
            node.parameters.insert(QStringLiteral("sourceLanguage"), m_project.sourceLanguage);
            node.parameters.insert(QStringLiteral("targetLanguage"), m_project.targetLanguage);
            node.properties = node.parameters;
        } else if (node.typeId == QStringLiteral("core.review-gate")) {
            node.parameters.insert(QStringLiteral("mode"), QStringLiteral("never"));
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("synthesize")) {
            QVariantMap synthesisSettings = modelConfig.value(QStringLiteral("parameters")).toMap();
            synthesisSettings.insert(QStringLiteral("familyId"),
                                     modelConfig.value(QStringLiteral("familyId")));
            if (!synthesisSettings.contains(QStringLiteral("lang")))
                synthesisSettings.insert(QStringLiteral("lang"), m_project.targetLanguage);
            if (!applySelectedCloneVoiceToSynthesis(&synthesisSettings)) return false;
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.parameters.insert(QStringLiteral("synthesisSettings"), synthesisSettings);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("fit-timing")) {
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("mix")) {
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("export")) {
            node.parameters.insert(QStringLiteral("destination"), PathUtils::urlToLocalPath(outputPath));
            node.properties = node.parameters;
        }
    }
    m_workflowJournal = std::make_unique<WorkflowRunJournal>(
        QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    m_workflowRunner->setJournal(m_workflowJournal.get());
    return m_workflowRunner->run(graph);
}

bool DubbingController::startAutomaticWorkflow(const QString &outputPath)
{
    if (m_dubbingEntryGateActive) {
        setError(QStringLiteral("Choose an entry mode before starting Dubbing."));
        return false;
    }
    if (processing()) return false;
    const QString destination = PathUtils::urlToLocalPath(outputPath).trimmed();
    if (destination.isEmpty()) {
        setError(QStringLiteral("Choose an output path before generating the final dub."));
        return false;
    }
    if (!workflowGraphValid() || !QFileInfo(m_project.sourceMediaPath).isFile()) {
        setError(QStringLiteral("Import source media before generating the final dub."));
        return false;
    }
    const QString currentPreflight = automaticPreflightFingerprint();
    if (m_automaticPreflightFingerprint.isEmpty()
        || m_automaticPreflightFingerprint != currentPreflight) {
        m_automaticPreflightFingerprint.clear();
        setError(QStringLiteral(
            "Review Automatic preflight after changing media, route, model, variant, or Colab worker."));
        emit workflowChanged();
        return false;
    }
    // Approval is single-use. A retry returns to the review screen so the
    // operator always sees current worker health before another full run.
    m_automaticPreflightFingerprint.clear();
    if (m_project.dubbingQuality == QStringLiteral("custom")) {
        const QVariantMap issue = firstCustomSetupIssue();
        if (!issue.isEmpty()) {
            const QString message = issue.value(QStringLiteral("message")).toString();
            setError(message);
            emit workflowSetupRequired(
                issue.value(QStringLiteral("nodeId")).toString(),
                issue.value(QStringLiteral("setupKind")).toString(), message);
            emit workflowChanged();
            return false;
        }
    }
    if (m_project.dubbingQuality == QStringLiteral("custom")) {
        if (!cloneVoiceSelectionValid()) {
            setError(cloneVoiceSelectionError());
            return false;
        }
    } else if (m_project.ttsVoiceId.trimmed().isEmpty()) {
        const QVariantMap synthesis = m_workflowNodeConfigurations.value(
            QStringLiteral("synthesize")).toMap();
        const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
        QString modelId = parameters.value(QStringLiteral("modelId")).toString().trimmed();
        if (modelId.isEmpty())
            modelId = automaticDefaultFamilyId(QStringLiteral("tts"), m_project.dubbingQuality);
        const QString builtInVoice = DubbingColabModelRoutes::defaultVoiceForTtsModel(modelId);
        if (builtInVoice.isEmpty()) {
            setError(QStringLiteral("The selected TTS model has no deterministic built-in voice. Select a voice in TTS settings."));
            return false;
        }
        // Automatic quality selects the documented default for its exact TTS
        // family once, then persists it as a normal TTS voice selection.
        // This is not a random fallback and never replaces a saved voice.
        m_project.ttsVoiceId = QStringLiteral("builtin:") + builtInVoice;
        m_project.cloneVoicePresetId = m_project.ttsVoiceId;
        emit cloneVoiceSelectionChanged();
        persistAfterEdit();
    } else if (!cloneVoiceSelectionValid()) {
        setError(cloneVoiceSelectionError());
        return false;
    }
    clearError();
    setWorkflowMode(QStringLiteral("automatic"));
    setCurrentStep(QStringLiteral("import"));
    m_automaticOutputPath = destination;
    m_automaticSetupActive = true;
    m_automaticEvents.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticDownloadKeys.clear();
    m_automaticConfiguredNodes.clear();
    m_automaticSetupNodeId = QStringLiteral("source-separate");
    setAutomaticStatus(QStringLiteral("Checking required models and runtimes"));
    appendAutomaticEvent(QStringLiteral("Checking required models and runtimes"),
                         QStringLiteral("running"));
    emit processingChanged();
    emit workflowChanged();
    scheduleAutomaticSetupAdvance();
    return true;
}

void DubbingController::pauseAutomaticWorkflow()
{
    if (m_workflowMode != QStringLiteral("automatic") || !processing()) return;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticDownloadKeys.clear();
    m_automaticConfiguredNodes.clear();
    m_automaticSetupNodeId.clear();
    if (m_translationFix) m_translationFix->cancel();
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    if (m_runner) m_runner->cancel();
    setWorkflowMode(QStringLiteral("paused"));
    setAutomaticStatus(QStringLiteral("Paused. Settings are unlocked; Generate resumes the workflow."));
    appendAutomaticEvent(QStringLiteral("Automatic generation paused"),
                         QStringLiteral("paused"), currentStepId());
    emit processingChanged();
    emit workflowChanged();
}

void DubbingController::startStepByStep()
{
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Step-by-step requested current=%1 processing=%2 source=%3 master=%4 background=%5 segments=%6")
                     .arg(m_currentStepId)
                     .arg(processing() ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(m_project.sourceMediaPath, m_project.masterAudioPath,
                          m_project.backgroundAudioPath)
                     .arg(m_project.segments.size()));
    setWorkflowMode(QStringLiteral("step"));
    if (m_project.sourceMediaPath.isEmpty()) {
        // A new project is allowed into the step-by-step workspace, but it is
        // parked at its first valid action (Import).  It must not synthesize a
        // graph or run a stage before the operator supplies media.
        setCurrentStep(QStringLiteral("media-input"));
        clearError();
        return;
    }
    const QString transcriptSource = normalizedTranscriptSource(
        m_project.transcriptConfiguration.value(
            QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
    if (!readableDubbingArtifact(m_project.masterAudioPath)) setCurrentStep(QStringLiteral("ingest"));
    else if ((!readableDubbingArtifact(m_project.vocalsAudioPath)
              || !readableDubbingArtifact(m_project.backgroundAudioPath))
             && transcriptSource == QStringLiteral("stt"))
        setCurrentStep(QStringLiteral("source-separate"));
    else if (m_project.segments.isEmpty()) setCurrentStep(QStringLiteral("transcribe"));
    else {
        bool allTranslated = true;
        bool allGenerated = true;
        for (const QVariant &entry : m_project.segments) {
            const QVariantMap segment = entry.toMap();
            allTranslated = allTranslated && !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
            allGenerated = allGenerated && QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString());
        }
        if (!allTranslated) setCurrentStep(QStringLiteral("translate"));
        else if (!allGenerated) setCurrentStep(QStringLiteral("synthesize"));
        else if (previewPath().isEmpty() || !QFileInfo(previewPath()).isFile()) setCurrentStep(QStringLiteral("mix"));
        else if (exportPath().isEmpty() || !QFileInfo(exportPath()).isFile()) setCurrentStep(QStringLiteral("export"));
        else setCurrentStep(QStringLiteral("completed"));
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Step-by-step resolved next step=%1").arg(m_currentStepId));
}

bool DubbingController::runCurrentStep(const QString &outputPath)
{
    if (m_dubbingEntryGateActive) {
        setError(QStringLiteral("Choose an entry mode before running the Dubbing workflow."));
        return false;
    }
    if (m_workflowMode != QStringLiteral("step")) startStepByStep();
    const QString step = m_currentStepId;
    const QString transcriptSource = step == QStringLiteral("transcribe")
        ? normalizedTranscriptSource(m_project.transcriptConfiguration.value(
              QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString())
        : QString();
    const bool independentOcr = step == QStringLiteral("transcribe")
        && transcriptSource == QStringLiteral("ocr");
    const bool independentStt = step == QStringLiteral("transcribe")
        && transcriptSource == QStringLiteral("stt");
    const bool canRunAlongsideCurrentWork = independentOcr
        ? canRunIndependentSubtitleOcrAlongsideCurrentWork()
        : (independentStt ? canRunIndependentAudioSttAlongsideCurrentWork() : false);
    if (processing() && !canRunAlongsideCurrentWork) {
        if (independentOcr)
            setBusyError(QStringLiteral("Subtitle OCR can run beside STT only; wait for the current non-STT Dubbing task to finish."));
        else if (independentStt)
            setBusyError(QStringLiteral("Speech-to-Text can run beside Subtitle OCR only; wait for the current non-OCR Dubbing task to finish."));
        return false;
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Run current step step=%1 mode=%2 output=%3 project=%4")
                     .arg(step, m_workflowMode, outputPath, m_project.projectPath));
    if (step == QStringLiteral("ingest")) {
        m_runner->startIngest(m_project.sourceMediaPath);
        return m_runner->processing();
    }
    if (step == QStringLiteral("source-separate")) {
        m_runner->startSourceSeparation(
            m_project.masterAudioPath,
            m_workflowNodeConfigurations.value(QStringLiteral("source-separate")).toMap());
        return m_runner->processing();
    }
    if (step == QStringLiteral("transcribe")) {
        if (transcriptSource == QStringLiteral("reconcile"))
            return reconcileTranscriptSources();
        if (transcriptSource == QStringLiteral("ocr"))
            return runSubtitleOcrIndependently();
        transcribeSource();
        return m_runner->processing();
    }
    if (step == QStringLiteral("translate")) {
        translateSource();
        return m_runner->processing();
    }
    if (step == QStringLiteral("synthesize")) {
        generateAudio();
        return m_runner->processing();
    }
    if (step == QStringLiteral("fit-timing")) {
        m_runner->fitTiming(m_project.segments, m_project.projectPath);
        return m_runner->processing();
    }
    if (step == QStringLiteral("mix")) return renderPreview();
    if (step == QStringLiteral("export")) return exportMedia(outputPath);
    return false;
}

bool DubbingController::rerunStep(const QString &stepId, const QString &outputPath)
{
    if (m_dubbingEntryGateActive) {
        setError(QStringLiteral("Choose an entry mode before running the Dubbing workflow."));
        return false;
    }
    const QString requestedStep = stepId.trimmed();
    const bool independentOcr = requestedStep == QStringLiteral("transcribe")
        && normalizedTranscriptSource(m_project.transcriptConfiguration.value(
               QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString())
               == QStringLiteral("ocr");
    const bool independentStt = requestedStep == QStringLiteral("transcribe")
        && !independentOcr;
    const bool canRunAlongsideCurrentWork = independentOcr
        ? canRunIndependentSubtitleOcrAlongsideCurrentWork()
        : (independentStt ? canRunIndependentAudioSttAlongsideCurrentWork() : false);
    if (processing() && !canRunAlongsideCurrentWork) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Run request rejected while busy requestedStep=%1 activeStep=%2 runnerStage=%3 progress=%4")
                            .arg(stepId, m_currentStepId, m_runner->stage())
                            .arg(m_runner->progress()));
        return false;
    }

    const QString step = requestedStep;
    const bool supported = step == QStringLiteral("ingest")
        || step == QStringLiteral("source-separate")
        || step == QStringLiteral("transcribe")
        || step == QStringLiteral("translate")
        || step == QStringLiteral("synthesize")
        || step == QStringLiteral("fit-timing")
        || step == QStringLiteral("mix")
        || step == QStringLiteral("export");
    if (!supported) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Ignoring rerun request for unsupported step=%1").arg(step));
        return false;
    }
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before running this step again."));
        return false;
    }

    clearError();
    m_automaticEvents.clear();
    setWorkflowMode(QStringLiteral("step"));
    setCurrentStep(step);
    setAutomaticStatus(QStringLiteral("Running manual node: %1").arg(visibleStepForNode(step)));
    appendAutomaticEvent(QStringLiteral("Running manual node: %1").arg(visibleStepForNode(step)),
                         QStringLiteral("running"), step);
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Rerun step step=%1 output=%2 project=%3")
                     .arg(step, outputPath, m_project.projectPath));
    return runCurrentStep(outputPath);
}

bool DubbingController::runWorkflowNode(const QString &nodeId, const QString &outputPath)
{
    const QString normalized = nodeId.trimmed().toLower();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("runWorkflowNode requested: nodeId=%1 (normalized=%2)").arg(nodeId, normalized));
    if (normalized == QStringLiteral("normalize") || normalized == QStringLiteral("ingest")
        || normalized == QStringLiteral("step-2")) {
        return rerunStep(QStringLiteral("ingest"), outputPath);
    }
    if (normalized == QStringLiteral("separate") || normalized == QStringLiteral("isolator")
        || normalized == QStringLiteral("source-separate") || normalized == QStringLiteral("step-3")) {
        return rerunStep(QStringLiteral("source-separate"), outputPath);
    }
    if (normalized == QStringLiteral("transcribe") || normalized == QStringLiteral("stt")
        || normalized == QStringLiteral("review-transcript") || normalized == QStringLiteral("step-4")) {
        return rerunStep(QStringLiteral("transcribe"), outputPath);
    }
    if (normalized == QStringLiteral("subtitle-ocr") || normalized == QStringLiteral("ocr")) {
        return runSubtitleOcrIndependently();
    }
    if (normalized == QStringLiteral("translate") || normalized == QStringLiteral("review-translation")
        || normalized == QStringLiteral("step-6")) {
        return rerunStep(QStringLiteral("translate"), outputPath);
    }
    if (normalized == QStringLiteral("synthesize") || normalized == QStringLiteral("tts")
        || normalized == QStringLiteral("assign-voices") || normalized == QStringLiteral("step-7")) {
        return rerunStep(QStringLiteral("synthesize"), outputPath);
    }
    if (normalized == QStringLiteral("fit-timing") || normalized == QStringLiteral("alignment-subtitle")
        || normalized == QStringLiteral("review-conflicts") || normalized == QStringLiteral("step-5")) {
        return rerunStep(QStringLiteral("fit-timing"), outputPath);
    }
    if (normalized == QStringLiteral("mix")) {
        return rerunStep(QStringLiteral("mix"), outputPath);
    }
    if (normalized == QStringLiteral("export") || normalized == QStringLiteral("step-8")) {
        return rerunStep(QStringLiteral("export"), outputPath);
    }
    return rerunStep(nodeId, outputPath);
}

bool DubbingController::approveWorkflowReview(const QVariantMap &artifact)
{
    if (!workflowWaitingForInput()) return false;
    return m_workflowRunner->resume(QVariantMap{{QStringLiteral("action"), QStringLiteral("approve")},
                                                 {QStringLiteral("artifact"), artifact.isEmpty()
                                                     ? m_workflowReviewRequest.value(QStringLiteral("artifact")) : QVariant(artifact)}});
}

bool DubbingController::rejectWorkflowReview(const QString &reason)
{
    if (!workflowWaitingForInput()) return false;
    return m_workflowRunner->resume(QVariantMap{{QStringLiteral("action"), QStringLiteral("reject")},
                                                 {QStringLiteral("reason"), reason}});
}

bool DubbingController::resumeInterruptedWorkflow()
{
    const QString runId = m_workflowRecovery.value(QStringLiteral("runId")).toString();
    if (!m_workflowRunner || runId.isEmpty() || m_workflowRunner->running()) return false;
    if (!m_workflowRunner->resumeInterrupted(runId)) {
        setError(m_workflowRunner->error().isEmpty()
                     ? QStringLiteral("The interrupted workflow could not be resumed.")
                     : m_workflowRunner->error());
        return false;
    }
    // Some lightweight nodes finish synchronously. In that case the completed
    // handler has already looked for any older interrupted run, so do not wipe
    // out the next recovery prompt here.
    if (m_workflowRunner->running()) {
        m_workflowRecovery.clear();
        setWorkflowMode(QStringLiteral("automatic"));
    }
    emit workflowChanged();
    return true;
}

QVariantList DubbingController::colabModelOptionsForNode(const QString &nodeId) const
{
    return DubbingColabModelRoutes::optionsForNode(nodeId);
}

QString DubbingController::defaultColabModelForNode(const QString &nodeId) const
{
    return DubbingColabModelRoutes::defaultModelForNode(nodeId);
}

QString DubbingController::colabNotebookForNode(const QString &nodeId,
                                                const QString &modelId) const
{
    return DubbingColabModelRoutes::notebookForModel(nodeId, modelId);
}

bool DubbingController::selectWorkflowColabModel(const QString &nodeId,
                                                 const QString &modelId)
{
    const QString normalized = modelId.trimmed().toLower();
    if (!DubbingColabModelRoutes::supports(nodeId, normalized)) {
        setError(QStringLiteral("No exact Colab notebook is mapped for model '%1' on the %2 node.")
                     .arg(modelId, visibleStepForNode(nodeId)));
        return false;
    }

    AppController *app = AppController::instance();
    bool selected = false;
    if (nodeId == QStringLiteral("source-separate") && app && app->colabVoiceIsolator())
        selected = app->colabVoiceIsolator()->selectColabModel(normalized);
    else if (nodeId == QStringLiteral("transcribe") && app && app->sttSession())
        selected = app->sttSession()->selectColabModel(normalized);
    else if (nodeId == QStringLiteral("subtitle-ocr") && app && app->subtitleOcr())
        selected = app->subtitleOcr()->setColabModelId(normalized);
    else if (nodeId == QStringLiteral("translate") && app && app->translation())
        selected = app->translation()->selectColabModel(normalized);
    else if (nodeId == QStringLiteral("synthesize") && app && app->colabTts())
        selected = app->colabTts()->selectColabModel(normalized);
    else if (nodeId == QStringLiteral("alignment") && app && app->colabAlignment())
        selected = app->colabAlignment()->selectColabModel(normalized);
    else if (nodeId == QStringLiteral("adaptive-llm"))
        // This worker belongs to the Dubbing project, not to the standalone
        // LLM Chat surface.  Selecting it must not clear or mutate the chat
        // controller's temporary worker/session just because both use the
        // llm-chat capability.
        selected = true;

    if (!selected) {
        setError(QStringLiteral("The selected Colab model could not be activated for %1.")
                     .arg(visibleStepForNode(nodeId)));
        return false;
    }
    // A verification is bound to an exact model. Selecting a different model
    // invalidates only that stage's memory-only setup snapshot; it never
    // repurposes a verified worker or silently changes route.
    m_colabSetupSnapshots.remove(nodeId);
    emit colabSetupChanged();
    if (nodeId == QStringLiteral("alignment")) {
        return setWorkflowNodeParameters(
            QStringLiteral("transcribe"),
            {{QStringLiteral("alignmentModelId"), normalized}});
    }
    if (nodeId == QStringLiteral("subtitle-ocr")) {
        m_project.transcriptConfiguration.insert(QStringLiteral("ocrExecutionRoute"),
                                                 QStringLiteral("colab-gpu"));
        m_project.transcriptConfiguration.insert(QStringLiteral("ocrColabModelId"), normalized);
        persistAfterEdit();
        emit projectChanged();
        emit workflowChanged();
        return true;
    }
    if (nodeId == QStringLiteral("adaptive-llm")) {
        QVariantMap configuration = translationFixConfiguration();
        configuration.insert(QStringLiteral("provider"), QStringLiteral("colab-direct"));
        configuration.insert(QStringLiteral("configured"), true);
        configuration.insert(QStringLiteral("model"), normalized);
        configuration.remove(QStringLiteral("runtimeId"));
        configuration.remove(QStringLiteral("runtimeVersion"));
        configuration.remove(QStringLiteral("selectedFiles"));
        setAdaptiveConfiguration(configuration);
        return true;
    }
    return setWorkflowNodeParameters(
        nodeId,
        {{QStringLiteral("executionProvider"), QStringLiteral("colab-direct")},
         {QStringLiteral("modelId"), normalized}});
}

bool DubbingController::discardInterruptedWorkflow()
{
    const QString runId = m_workflowRecovery.value(QStringLiteral("runId")).toString();
    if (!m_workflowRunner || runId.isEmpty() || !m_workflowRunner->discardInterrupted(runId)) {
        setError(QStringLiteral("The interrupted workflow could not be discarded."));
        return false;
    }
    discoverInterruptedWorkflow();
    emit workflowChanged();
    return true;
}

