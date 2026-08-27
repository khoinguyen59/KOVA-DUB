QVariantMap DubbingController::automaticPreflight() const
{
    QVariantList issues;
    auto addIssue = [&issues](const QString &id, const QString &message,
                              int page = 1, const QString &focus = QString()) {
        issues.append(QVariantMap{{QStringLiteral("id"), id},
                                  {QStringLiteral("message"), message},
                                  {QStringLiteral("page"), page},
                                  {QStringLiteral("focus"), focus}});
    };

    const bool hasMedia = !m_project.sourceMediaPath.trimmed().isEmpty()
        && QFileInfo(m_project.sourceMediaPath).isFile();
    if (!hasMedia)
        addIssue(QStringLiteral("source-media"),
                 QStringLiteral("Import source media before starting Automatic dubbing."), 0,
                 QStringLiteral("source-media"));
    // sourceLanguage/targetLanguage are the project's single source of truth.
    // The wizard never keeps a parallel, display-only copy of them.
    if (m_project.sourceLanguage.trimmed().isEmpty())
        addIssue(QStringLiteral("source-language"),
                 QStringLiteral("Choose the spoken/source language for Transcribe and Translate."), 0,
                 QStringLiteral("source-language"));
    if (m_project.targetLanguage.trimmed().isEmpty())
        addIssue(QStringLiteral("target-language"),
                 QStringLiteral("Choose the output language for Translate and TTS."), 0,
                 QStringLiteral("target-language"));
    if (!workflowGraphValid())
        addIssue(QStringLiteral("workflow-graph"),
                 QStringLiteral("The default Dubbing workflow definition is invalid."));

    if (!m_project.ttsVoiceId.trimmed().isEmpty() && !cloneVoiceSelectionValid()) {
        addIssue(QStringLiteral("tts-voice"), cloneVoiceSelectionError());
    }

    const bool adaptiveRewriteRequired = m_project.dubbingQuality == QStringLiteral("adaptive")
        || (m_project.dubbingQuality == QStringLiteral("custom")
            && m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
    if (adaptiveRewriteRequired && !adaptiveReady()) {
        const QString message = adaptiveProvider() == QStringLiteral("colab-direct")
            ? QStringLiteral("Connect and check the exact Direct Colab Adaptive LLM worker for Translate.")
            : QStringLiteral("Configure the Adaptive rewrite LLM for Translate. Automatic will not download or use a local fallback.");
        addIssue(QStringLiteral("adaptive-llm"), message,
                 adaptiveProvider() == QStringLiteral("colab-direct") ? 2 : 1,
                 QStringLiteral("translate"));
    }

    // Sessions are keyed by production node id. The aggregate stage loop
    // below maps each selected worker to one presentation stage exactly once.
    QHash<QString, QVariantMap> directWorkers;
    for (const QVariant &value : colabSetupStages()) {
        const QVariantMap stage = value.toMap();
        if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()
            || !stageRequiredForCurrentTranscriptAction(
                stage.value(QStringLiteral("id")).toString())) {
            continue;
        }
        directWorkers.insert(stage.value(QStringLiteral("id")).toString(), stage);
    }

    QVariantList aggregateStages;
    QVariantList selectedWorkers;
    for (const QVariant &value : workflowStages()) {
        const QVariantMap stage = value.toMap();
        const QString stageId = stage.value(QStringLiteral("id")).toString();
        const QString nodeId = stage.value(QStringLiteral("actionNodeId")).toString();
        QVariantMap configuration = m_workflowNodeConfigurations.value(nodeId).toMap();
        const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
        const QString providerId = configuration.value(
            QStringLiteral("executionProvider"), parameters.value(
            QStringLiteral("executionProvider"))).toString().trimmed().toLower();
        const QString route = providerId == QStringLiteral("colab-direct")
            ? QStringLiteral("Direct Colab")
            : providerId == QStringLiteral("api-gateway")
                ? QStringLiteral("API Gateway")
                : providerId == QStringLiteral("local-dev") ? QStringLiteral("Local")
                : QStringLiteral("Not selected");
        const bool requiresSourceLanguage = nodeId == QStringLiteral("transcribe")
            || nodeId == QStringLiteral("review-transcript")
            || nodeId == QStringLiteral("translate");
        const bool requiresTargetLanguage = nodeId == QStringLiteral("translate")
            || nodeId == QStringLiteral("synthesize");
        QString languageSummary;
        if (nodeId == QStringLiteral("translate"))
            languageSummary = QStringLiteral("%1 -> %2").arg(m_project.sourceLanguage, m_project.targetLanguage);
        else if (requiresSourceLanguage)
            languageSummary = m_project.sourceLanguage;
        else if (requiresTargetLanguage)
            languageSummary = m_project.targetLanguage;
        QString setupAction = QStringLiteral("none");
        QString setupHint = QStringLiteral("No configuration required");
        if (nodeId == QStringLiteral("media-input")) {
            setupAction = QStringLiteral("source");
            setupHint = QStringLiteral("Choose source media on page 1");
        } else if (nodeId == QStringLiteral("source-separate")
                   || nodeId == QStringLiteral("transcribe")
                   || nodeId == QStringLiteral("translate")
                   || nodeId == QStringLiteral("synthesize")) {
            setupAction = QStringLiteral("node-model");
            setupHint = QStringLiteral("Choose route and model");
        } else if (nodeId == QStringLiteral("fit-timing")) {
            setupAction = QStringLiteral("alignment");
            setupHint = QStringLiteral("Configure timing resolution");
        } else if (nodeId == QStringLiteral("export")) {
            setupAction = QStringLiteral("export");
            setupHint = QStringLiteral("Configure output/export options");
        } else if (nodeId == QStringLiteral("ingest")) {
            setupAction = QStringLiteral("normalize");
            setupHint = QStringLiteral("Automatic local preprocessing; no model required");
        }

        const bool modelStage = nodeId == QStringLiteral("source-separate")
            || nodeId == QStringLiteral("transcribe") || nodeId == QStringLiteral("translate")
            || nodeId == QStringLiteral("synthesize");
        QString preflightState = QStringLiteral("ready");
        QString preflightStateLabel = QStringLiteral("Ready");
        if (nodeId == QStringLiteral("media-input") && !hasMedia) {
            preflightState = QStringLiteral("needs-input");
            preflightStateLabel = QStringLiteral("Needs input");
        } else if (stage.value(QStringLiteral("state")).toString() == QStringLiteral("missing")
                   || (stage.value(QStringLiteral("state")).toString() == QStringLiteral("blocked")
                       && !modelStage)) {
            preflightState = QStringLiteral("blocked-previous");
            preflightStateLabel = QStringLiteral("Blocked by previous stage");
        }
        const QString modelId = configuration.value(
            QStringLiteral("modelId"), parameters.value(QStringLiteral("modelId"))).toString().trimmed();
        // A saved clone is a durable project resource, not a regular TTS
        // voice.  Direct Colab synthesis therefore verifies its exact
        // Voice-Cloning family and session; the independent normal-TTS model
        // remains available for ordinary built-in voices only.
        const QVariantMap savedClonePreset = nodeId == QStringLiteral("synthesize")
            ? selectedCloneVoicePreset() : QVariantMap{};
        const QString savedCloneFamily = savedClonePreset.value(
            QStringLiteral("familyId")).toString().trimmed().toLower();
        const bool usesSavedCloneVoice = nodeId == QStringLiteral("synthesize")
            && !savedCloneFamily.isEmpty()
            && DubbingColabModelRoutes::supports(QStringLiteral("voice-cloning"),
                                                  savedCloneFamily);
        const QString effectiveNodeId = usesSavedCloneVoice
            ? QStringLiteral("voice-cloning") : nodeId;
        const QString effectiveModelId = usesSavedCloneVoice
            ? savedCloneFamily : modelId;
        if (modelStage && (configuration.isEmpty() || providerId.isEmpty()
                           || effectiveModelId.isEmpty())) {
            preflightState = hasMedia ? QStringLiteral("needs-setup")
                                      : QStringLiteral("blocked-previous");
            preflightStateLabel = hasMedia ? QStringLiteral("Needs setup")
                                            : QStringLiteral("Blocked by source media");
            addIssue(stageId, QStringLiteral("Configure %1 with a route and an exact model/runtime.")
                .arg(stage.value(QStringLiteral("title")).toString()));
        } else if (modelStage && providerId == QStringLiteral("colab-direct")) {
            const QVariantMap worker = directWorkers.value(nodeId);
            ColabSession *session = usesSavedCloneVoice
                ? colabSessionForStage(QStringLiteral("voice-cloning"))
                : colabSessionForStage(nodeId);
            QString routeError;
            const bool verified = session && session->hasVerifiedRoute(
                colabCapabilityForStage(effectiveNodeId), effectiveModelId, &routeError);
            if (!DubbingColabModelRoutes::supports(effectiveNodeId, effectiveModelId)
                || !verified) {
                preflightState = QStringLiteral("needs-worker");
                preflightStateLabel = QStringLiteral("Direct Colab needs check");
                addIssue(QStringLiteral("colab-") + nodeId,
                         usesSavedCloneVoice
                             ? QStringLiteral("Connect and check the exact Direct Colab Voice Cloning worker for the saved voice (%1).")
                                   .arg(effectiveModelId)
                             : QStringLiteral("Connect and check the exact Direct Colab worker for %1 (%2).")
                                   .arg(stage.value(QStringLiteral("title")).toString(),
                                        effectiveModelId), 2);
            }
            if (nodeId == QStringLiteral("synthesize") && !usesSavedCloneVoice) {
                const QString ttsLanguage = parameters.value(QStringLiteral("lang"),
                    m_project.targetLanguage).toString().trimmed();
                if (!DubbingColabModelRoutes::supportsTtsLanguage(modelId, ttsLanguage)) {
                    preflightState = QStringLiteral("needs-setup");
                    preflightStateLabel = QStringLiteral("TTS language incompatible");
                    addIssue(QStringLiteral("tts-language"),
                             DubbingColabModelRoutes::ttsLanguageCompatibilityError(
                                 modelId, ttsLanguage), 1, QStringLiteral("synthesize"));
                }
            }
            // Setup cards remain attached to their actual ordinary Dubbing
            // stage.  A saved clone has its own Voice Cloning session rather
            // than pretending the generic TTS worker is the selected route.
            if (!usesSavedCloneVoice && !worker.isEmpty()) {
                QVariantMap workerCard = worker;
                workerCard.insert(QStringLiteral("parentStageId"), stageId);
                workerCard.insert(QStringLiteral("parentStageTitle"), stage.value(QStringLiteral("title")));
                selectedWorkers.append(workerCard);
            }
        } else if (modelStage && providerId == QStringLiteral("api-gateway")) {
            const bool gatewayConfigured = m_settings && !m_settings->gatewayUrl().trimmed().isEmpty()
                && m_settings->gatewayApiKeyConfigured();
            if (!gatewayConfigured) {
                preflightState = QStringLiteral("needs-setup");
                preflightStateLabel = QStringLiteral("API Gateway needs setup");
                addIssue(stageId + QStringLiteral("-gateway"),
                         QStringLiteral("Configure API Gateway credentials before using %1.")
                             .arg(stage.value(QStringLiteral("title")).toString()));
            }
        } else if (modelStage && providerId == QStringLiteral("local-dev")) {
            StudioConfiguration localConfiguration;
            localConfiguration.capabilityId = stage.value(QStringLiteral("capabilityId")).toString();
            localConfiguration.familyId = configuration.value(QStringLiteral("familyId")).toString();
            localConfiguration.runtimeId = configuration.value(QStringLiteral("runtimeId")).toString();
            localConfiguration.runtimeVersion = configuration.value(QStringLiteral("runtimeVersion")).toString();
            localConfiguration.selectedFiles = configuration.value(QStringLiteral("selectedFiles")).toMap();
            if (!StudioConfigurationResolver::resolve(localConfiguration).isValid) {
                preflightState = QStringLiteral("needs-setup");
                preflightStateLabel = QStringLiteral("Local runtime/model needs setup");
                addIssue(stageId + QStringLiteral("-local"),
                         QStringLiteral("Choose an installed local runtime and model for %1.")
                             .arg(stage.value(QStringLiteral("title")).toString()));
            }
        }

        QString configurationSummary = modelStage
            ? (effectiveModelId.isEmpty()
                ? QStringLiteral("No route and exact model/runtime have been confirmed.")
                : usesSavedCloneVoice
                    ? QStringLiteral("%1 / saved clone voice: %2 (Voice Cloning worker)")
                          .arg(route, effectiveModelId)
                    : QStringLiteral("%1 / %2").arg(route, effectiveModelId))
            : setupHint;
        if (nodeId == QStringLiteral("ingest")) {
            const QString sourceShape = m_project.sourceSampleRate > 0 && m_project.sourceChannels > 0
                ? QStringLiteral("source %1 Hz / %2 channel(s)")
                      .arg(m_project.sourceSampleRate).arg(m_project.sourceChannels)
                : QStringLiteral("source format will be probed at ingest");
            configurationSummary = QStringLiteral("Automatic local preprocessing; %1; master and analysis WAV outputs; no model required.")
                .arg(sourceShape);
        } else if (nodeId == QStringLiteral("fit-timing")) {
            const QVariantMap timing = timingConfiguration();
            configurationSummary = QStringLiteral("Timing: %1; minimum gap %2 ms.")
                .arg(timing.value(QStringLiteral("mode")).toString())
                .arg(timing.value(QStringLiteral("minimumGapMs")).toInt());
        } else if (stageId == QStringLiteral("export")) {
            configurationSummary = QStringLiteral("Render mix with background; subtitle burn-in: %1.")
                .arg(subtitleConfiguration().value(QStringLiteral("burnIn")).toBool()
                         ? QStringLiteral("enabled") : QStringLiteral("disabled"));
        }
        const bool adaptiveSetupRequired = nodeId == QStringLiteral("translate")
            && adaptiveRewriteRequired && !adaptiveReady();
        if (adaptiveSetupRequired && preflightState == QStringLiteral("ready")) {
            preflightState = adaptiveProvider() == QStringLiteral("colab-direct")
                ? QStringLiteral("needs-worker") : QStringLiteral("needs-setup");
            preflightStateLabel = adaptiveProvider() == QStringLiteral("colab-direct")
                ? QStringLiteral("Adaptive LLM needs check") : QStringLiteral("Adaptive LLM needs setup");
        }
        if (nodeId == QStringLiteral("translate")) {
            configurationSummary += QStringLiteral("\nAdaptive rewrite LLM: %1.")
                .arg(adaptiveReady() ? adaptiveStatusText() : QStringLiteral("not ready"));
        }

        // Direct Colab notebooks currently expose one immutable GPU
        // configuration. Keep that exact variant on the presentation stage
        // as well as on the worker card: otherwise the stage list can say
        // only "model" while verification is bound to model + variant.
        QString variant = parameters.value(QStringLiteral("variant")).toString().trimmed();
        if (providerId == QStringLiteral("colab-direct")) {
            variant = usesSavedCloneVoice
                ? QStringLiteral("fixed")
                : directWorkers.value(nodeId).value(QStringLiteral("variant")).toString().trimmed();
            if (variant.isEmpty()) variant = QStringLiteral("fixed");
        }

        aggregateStages.append(QVariantMap{
            {QStringLiteral("id"), stageId},
            {QStringLiteral("title"), stage.value(QStringLiteral("title"), stageId)},
            {QStringLiteral("actionNodeId"), nodeId},
            {QStringLiteral("productionNodeIds"), stage.value(QStringLiteral("productionNodeIds"))},
            {QStringLiteral("executionProvider"), providerId},
            {QStringLiteral("route"), route},
            {QStringLiteral("modelId"), effectiveModelId},
            {QStringLiteral("variant"), variant},
            {QStringLiteral("requiresLanguage"), requiresSourceLanguage || requiresTargetLanguage},
            {QStringLiteral("languageSummary"), languageSummary},
            {QStringLiteral("state"), stage.value(QStringLiteral("state"))},
            {QStringLiteral("detail"), stage.value(QStringLiteral("detail"), configurationSummary)},
            {QStringLiteral("setupAction"), setupAction},
            {QStringLiteral("setupHint"), setupHint},
            {QStringLiteral("configurationSummary"), configurationSummary},
            {QStringLiteral("modelRequired"), modelStage},
            {QStringLiteral("effectiveFormat"), nodeId == QStringLiteral("ingest") ? configurationSummary : QString()},
            {QStringLiteral("preflightState"), preflightState},
            {QStringLiteral("preflightStateLabel"), preflightStateLabel},
            {QStringLiteral("adaptiveSetupRequired"), adaptiveSetupRequired}
        });
    }

    if (directWorkers.contains(QStringLiteral("adaptive-llm"))) {
        QVariantMap adaptiveWorker = directWorkers.value(QStringLiteral("adaptive-llm"));
        adaptiveWorker.insert(QStringLiteral("parentStageId"), QStringLiteral("translate"));
        adaptiveWorker.insert(QStringLiteral("parentStageTitle"), QStringLiteral("Translate"));
        selectedWorkers.append(adaptiveWorker);
    }

    return QVariantMap{
        {QStringLiteral("ready"), issues.isEmpty()},
        {QStringLiteral("issues"), issues},
        {QStringLiteral("stages"), aggregateStages},
        {QStringLiteral("selectedWorkers"), selectedWorkers},
        {QStringLiteral("sourceMediaPath"), m_project.sourceMediaPath},
        {QStringLiteral("sourceLanguage"), m_project.sourceLanguage},
        {QStringLiteral("targetLanguage"), m_project.targetLanguage},
        {QStringLiteral("transcriptSource"), m_project.transcriptConfiguration.value(
            QStringLiteral("transcriptSource"), QStringLiteral("stt"))},
        {QStringLiteral("fingerprint"), automaticPreflightFingerprint()}
    };
}

QString DubbingController::automaticPreflightFingerprint() const
{
    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("sourceMedia"), m_project.sourceMediaPath);
    snapshot.insert(QStringLiteral("sourceLanguage"), m_project.sourceLanguage);
    snapshot.insert(QStringLiteral("targetLanguage"), m_project.targetLanguage);
    snapshot.insert(QStringLiteral("transcript"), m_project.transcriptConfiguration);
    snapshot.insert(QStringLiteral("quality"), m_project.dubbingQuality);
    snapshot.insert(QStringLiteral("adaptiveLlm"), translationFixConfiguration());
    snapshot.insert(QStringLiteral("ttsVoice"), m_project.ttsVoiceId);
    snapshot.insert(QStringLiteral("nodes"), m_workflowNodeConfigurations);

    QVariantList workers;
    for (const QVariant &value : colabSetupStages()) {
        const QVariantMap stage = value.toMap();
        if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()
            || !stageRequiredForCurrentTranscriptAction(
                stage.value(QStringLiteral("id")).toString())) {
            continue;
        }
        const QString stageId = stage.value(QStringLiteral("id")).toString();
        ColabSession *session = colabSessionForStage(stageId);
        workers.append(QVariantMap{
            {QStringLiteral("id"), stageId},
            {QStringLiteral("capability"), stage.value(QStringLiteral("capability"))},
            {QStringLiteral("model"), stage.value(QStringLiteral("modelId"))},
            {QStringLiteral("verified"), stage.value(QStringLiteral("verified"))},
            {QStringLiteral("workerUrl"), session ? session->workerUrl() : QString()},
            {QStringLiteral("variant"), session ? session->expectedVariant() : QString()},
            {QStringLiteral("verifiedAt"), session ? session->verifiedAt() : QString()}
        });
    }
    snapshot.insert(QStringLiteral("workers"), workers);
    return QString::fromUtf8(QJsonDocument::fromVariant(snapshot).toJson(QJsonDocument::Compact));
}

QSet<QString> DubbingController::activeDownloadKeys() const
{
    QSet<QString> keys;
    const auto *app = AppController::instance();
    const QVariantList downloads = app && app->downloads()
        ? app->downloads()->activeDownloads() : QVariantList();
    for (const QVariant &entry : downloads) {
        const QVariantMap download = entry.toMap();
        const QString identifier = download.value(QStringLiteral("identifier")).toString();
        const QString filename = download.value(QStringLiteral("filename")).toString();
        if (!identifier.isEmpty() && !filename.isEmpty())
            keys.insert(identifier + QStringLiteral("::") + filename);
    }
    return keys;
}

void DubbingController::captureNewAutomaticDownloads(const QSet<QString> &before)
{
    const QSet<QString> current = activeDownloadKeys();
    for (const QString &key : current) {
        if (!before.contains(key))
            m_automaticDownloadKeys.insert(key);
    }
}

QVariantList DubbingController::automaticSetupDownloads() const
{
    QVariantList scoped;
    if (m_automaticDownloadKeys.isEmpty()) return scoped;
    const auto *app = AppController::instance();
    const QVariantList active = app && app->downloads()
        ? app->downloads()->activeDownloads() : QVariantList();
    for (const QVariant &entry : active) {
        const QVariantMap download = entry.toMap();
        const QString key = download.value(QStringLiteral("identifier")).toString()
            + QStringLiteral("::") + download.value(QStringLiteral("filename")).toString();
        if (m_automaticDownloadKeys.contains(key))
            scoped.append(download);
    }
    return scoped;
}

bool DubbingController::approveAutomaticPreflight()
{
    const QVariantMap preflight = automaticPreflight();
    if (!preflight.value(QStringLiteral("ready")).toBool()) {
        const QVariantList issues = preflight.value(QStringLiteral("issues")).toList();
        const QString detail = issues.isEmpty() ? QStringLiteral("Automatic preflight is blocked.")
            : issues.constFirst().toMap().value(QStringLiteral("message")).toString();
        setError(detail);
        return false;
    }
    m_automaticPreflightFingerprint = preflight.value(QStringLiteral("fingerprint")).toString();
    clearError();
    emit workflowChanged();
    return true;
}

QString DubbingController::workflowId() const
{
    return QString::fromLatin1(DubbingWorkflowDefinition::Id);
}

int DubbingController::workflowVersion() const
{
    return DubbingWorkflowDefinition::Version;
}

bool DubbingController::workflowGraphValid() const
{
    if (!m_workflowRegistry) return false;
    return WorkflowGraphRunner(m_workflowRegistry).validate(DubbingWorkflowDefinition::create()).isEmpty();
}

QString DubbingController::workflowRunId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) return m_workflowRunner->runId();
    return m_runner ? m_runner->runId() : QString();
}

QString DubbingController::workflowNodeRunId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) return m_workflowRunner->nodeRunId();
    return m_runner ? m_runner->nodeRunId() : QString();
}

bool DubbingController::workflowWaitingForInput() const
{
    return m_workflowRunner && m_workflowRunner->waitingForInput();
}

QVariantMap DubbingController::workflowReviewRequest() const
{
    return m_workflowReviewRequest;
}

QString DubbingController::currentStepId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) {
        return visibleStepForNode(m_workflowRunner->activeNodeId());
    }
    return m_currentStepId;
}

QVariantMap DubbingController::currentStepOutput() const
{
    return stepOutput(currentStepId());
}

QVariantMap DubbingController::stepOutput(const QString &stepId) const
{
    return m_stepOutputs.value(stepId).toMap();
}

void DubbingController::setWorkflowMode(const QString &mode)
{
    if (m_workflowMode == mode) return;
    m_workflowMode = mode;
    emit workflowChanged();
}

void DubbingController::setCurrentStep(const QString &stepId)
{
    if (m_currentStepId == stepId) return;
    m_currentStepId = stepId;
    emit workflowChanged();
}

void DubbingController::advanceManualStep(const QString &completedStepId)
{
    static const QHash<QString, QString> next{{QStringLiteral("ingest"), QStringLiteral("source-separate")},
                                              {QStringLiteral("source-separate"), QStringLiteral("transcribe")},
                                              {QStringLiteral("transcribe"), QStringLiteral("translate")},
                                              {QStringLiteral("translate"), QStringLiteral("synthesize")},
                                              {QStringLiteral("synthesize"), QStringLiteral("mix")},
                                              {QStringLiteral("mix"), QStringLiteral("export")},
                                              {QStringLiteral("export"), QStringLiteral("completed")}};
    if (next.contains(completedStepId)) setCurrentStep(next.value(completedStepId));
}

void DubbingController::prepareWorkflow()
{
    if (!workflowGraphValid()) {
        setError(QStringLiteral("The default dubbing workflow definition is invalid."));
        return;
    }
    emit workflowChanged();
}

QString DubbingController::defaultWorkflowModelFamily(const QString &nodeId) const
{
    QString capabilityId;
    if (nodeId == QStringLiteral("source-separate"))
        capabilityId = QStringLiteral("voice-isolation");
    else if (nodeId == QStringLiteral("transcribe"))
        capabilityId = QStringLiteral("stt");
    else if (nodeId == QStringLiteral("translate"))
        capabilityId = QStringLiteral("translation");
    else if (nodeId == QStringLiteral("synthesize"))
        capabilityId = QStringLiteral("tts");
    return automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality);
}

void DubbingController::resetStandardWorkflowNodeModels()
{
    if (m_project.dubbingQuality == QStringLiteral("custom")) return;
    m_workflowNodeConfigurations.clear();
    m_project.workflowNodeConfigurations.clear();
    resetStandardTranslationFixConfiguration();
    emit workflowChanged();
}

void DubbingController::resetStandardTranslationFixConfiguration()
{
    if (!m_translationFix || m_translationFix->busy()) return;
    if (m_settings && m_settings->remoteFirstMode()) {
        configureRemoteRewriteFromGateway();
        return;
    }
    m_translationFix->setConfiguration({
        {QStringLiteral("provider"), QStringLiteral("lmstudio")},
        {QStringLiteral("configured"), false},
        {QStringLiteral("serverUrl"), QStringLiteral("http://127.0.0.1:1234")},
        {QStringLiteral("model"), QStringLiteral("qwen3.5-2b")},
        {QStringLiteral("maxAttempts"),
         m_project.durationControl.value(QStringLiteral("maxPreTtsIterations"), 4)},
        {QStringLiteral("temperature"), 0.35}
    });
    if (m_runner)
        m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
}

void DubbingController::configureRemoteRewriteFromGateway()
{
    if (!m_translationFix || m_translationFix->busy() || !m_settings) return;
    const QString model = m_settings->gatewayLlmModel().trimmed();
    const QString url = m_settings->gatewayUrl().trimmed();
    m_translationFix->setConfiguration({
        {QStringLiteral("provider"), QStringLiteral("api")},
        {QStringLiteral("configured"),
         !url.isEmpty() && !model.isEmpty() && m_settings->gatewayApiKeyConfigured()},
        {QStringLiteral("serverUrl"), url},
        {QStringLiteral("apiKey"), m_settings->gatewayApiKey()},
        {QStringLiteral("model"), model},
        {QStringLiteral("maxAttempts"),
         m_project.durationControl.value(QStringLiteral("maxPreTtsIterations"), 4)},
        {QStringLiteral("temperature"), 0.35}
    });
    if (m_runner)
        m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
}

QString DubbingController::visibleStepForNode(const QString &nodeId)
{
    if (nodeId == QStringLiteral("media-input")) return QStringLiteral("import");
    if (nodeId == QStringLiteral("review-transcript")) return QStringLiteral("transcribe");
    if (nodeId == QStringLiteral("review-translation")) return QStringLiteral("translate");
    if (nodeId == QStringLiteral("fit-timing") || nodeId == QStringLiteral("review-conflicts"))
        return QStringLiteral("alignment-subtitle");
    if (nodeId == QStringLiteral("assign-voices")) return QStringLiteral("synthesize");
    if (nodeId == QStringLiteral("mix")) return QStringLiteral("export");
    return nodeId;
}

void DubbingController::setAutomaticStatus(const QString &message)
{
    if (m_automaticStatusText == message) return;
    m_automaticStatusText = message;
    emit workflowChanged();
}

void DubbingController::appendAutomaticEvent(const QString &message,
                                             const QString &state,
                                             const QString &nodeId)
{
    if (message.trimmed().isEmpty()) return;
    if (!m_automaticEvents.isEmpty()) {
        const QVariantMap last = m_automaticEvents.constLast().toMap();
        if (last.value(QStringLiteral("message")).toString() == message
            && last.value(QStringLiteral("state")).toString() == state)
            return;
    }
    m_automaticEvents.append(QVariantMap{
        {QStringLiteral("message"), message},
        {QStringLiteral("state"), state},
        {QStringLiteral("nodeId"), nodeId},
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}
    });
    while (m_automaticEvents.size() > 40) m_automaticEvents.removeFirst();
    emit workflowChanged();
}

CapabilityFamilyModel *DubbingController::automaticModel(const QString &capabilityId)
{
    AppController *app = AppController::instance();
    if (!app) return nullptr;
    std::unique_ptr<CapabilityFamilyModel> *holder = nullptr;
    if (capabilityId == QStringLiteral("stt")) holder = &m_automaticSttModel;
    else if (capabilityId == QStringLiteral("voice-isolation"))
        holder = &m_automaticVoiceIsolationModel;
    else if (capabilityId == QStringLiteral("translation")) holder = &m_automaticTranslationModel;
    else if (capabilityId == QStringLiteral("tts")) holder = &m_automaticTtsModel;
    else if (capabilityId == QStringLiteral("llm-chat")) holder = &m_automaticLlmModel;
    if (!holder) return nullptr;
    if (!*holder) {
        *holder = std::make_unique<CapabilityFamilyModel>(
            m_models, m_runtimes, app->registry(), app->settings(), this);
        (*holder)->setCapability(capabilityId);
    }
    (*holder)->refresh();
    return holder->get();
}

bool DubbingController::ensureAutomaticModel(const QString &nodeId,
                                             const QString &capabilityId,
                                             bool loadSession)
{
    AppController *app = AppController::instance();
    if (!app || !app->downloadInstall() || !app->sessionRegistry()) {
        finishAutomaticSetupFailure(
            QStringLiteral("Automatic model setup is unavailable for %1.").arg(capabilityId));
        return false;
    }

    QVariantMap configuration = m_workflowNodeConfigurations.value(nodeId).toMap();
    m_automaticSetupNodeId = nodeId;
    const QVariantMap configuredParameters = configuration.value(QStringLiteral("parameters")).toMap();
    const QString providerId = configuration.value(
        QStringLiteral("executionProvider"), configuredParameters.value(
            QStringLiteral("executionProvider"), QStringLiteral("local-dev")))
        .toString().trimmed().toLower();
    ExecutionProvider provider = ExecutionProvider::LocalDev;
    if (!executionProviderFromId(providerId, &provider)) {
        finishAutomaticSetupFailure(
            QStringLiteral("Unknown execution provider for %1.").arg(visibleStepForNode(nodeId)));
        return false;
    }
    const QString configuredModel = configuration.value(
        QStringLiteral("modelId"), configuredParameters.value(QStringLiteral("modelId")))
        .toString().trimmed().toLower();
    const QVariantMap savedClonePreset = nodeId == QStringLiteral("synthesize")
        ? selectedCloneVoicePreset() : QVariantMap{};
    const QString savedCloneFamily = savedClonePreset.value(
        QStringLiteral("familyId")).toString().trimmed().toLower();
    const bool usesSavedCloneVoice = nodeId == QStringLiteral("synthesize")
        && !savedCloneFamily.isEmpty()
        && DubbingColabModelRoutes::supports(QStringLiteral("voice-cloning"),
                                              savedCloneFamily);
    const QString effectiveNodeId = usesSavedCloneVoice
        ? QStringLiteral("voice-cloning") : nodeId;
    const QString effectiveModel = usesSavedCloneVoice
        ? savedCloneFamily : configuredModel;

    // The Automatic wizard has already captured an explicit provider for each
    // model node.  Do not use a global remote-first preference to decide this
    // boundary: selecting Direct Colab or API Gateway must never enqueue a
    // local model/runtime download as a hidden fallback.
    if (provider == ExecutionProvider::ColabDirect) {
        const QString capability = colabCapabilityForStage(effectiveNodeId);
        ColabSession *session = colabSessionForStage(effectiveNodeId);
        QString routeError;
        if (effectiveModel.isEmpty()
            || !DubbingColabModelRoutes::supports(effectiveNodeId, effectiveModel)
            || !session || !session->hasVerifiedRoute(capability, effectiveModel, &routeError)) {
            const QString detail = routeError.trimmed().isEmpty()
                ? usesSavedCloneVoice
                    ? QStringLiteral("Connect and check the exact Direct Colab Voice Cloning worker for the saved voice.")
                    : QStringLiteral("Connect and check the exact Direct Colab worker in Automatic setup.")
                : routeError;
            finishAutomaticSetupFailure(
                QStringLiteral("Direct Colab is not ready for %1: %2")
                    .arg(visibleStepForNode(nodeId), detail));
            return false;
        }
        const QString workerLabel = usesSavedCloneVoice
            ? QStringLiteral("Voice Cloning worker for saved voice %1").arg(effectiveModel)
            : QStringLiteral("worker for %1").arg(visibleStepForNode(nodeId));
        setAutomaticStatus(QStringLiteral("Using verified Direct Colab %1").arg(workerLabel));
        appendAutomaticEvent(QStringLiteral("Direct Colab worker ready: %1 (%2)")
                                 .arg(workerLabel, effectiveModel),
                             QStringLiteral("completed"), nodeId);
        return true;
    }
    if (provider == ExecutionProvider::ApiGateway) {
        Settings *settings = m_settings ? m_settings : app->settings();
        if (configuredModel.isEmpty() || !settings || settings->gatewayUrl().trimmed().isEmpty()
            || !settings->gatewayApiKeyConfigured()) {
            finishAutomaticSetupFailure(
                QStringLiteral("API Gateway is not ready for %1. Configure its URL, key, and exact model in Automatic setup.")
                    .arg(visibleStepForNode(nodeId)));
            return false;
        }
        setAutomaticStatus(QStringLiteral("Using configured API Gateway for %1")
                               .arg(visibleStepForNode(nodeId)));
        appendAutomaticEvent(QStringLiteral("API Gateway ready for %1 (%2)")
                                 .arg(visibleStepForNode(nodeId), configuredModel),
                             QStringLiteral("completed"), nodeId);
        return true;
    }

    CapabilityFamilyModel *model = automaticModel(capabilityId);
    if (!model) {
        finishAutomaticSetupFailure(
            QStringLiteral("Automatic model setup is unavailable for %1.").arg(capabilityId));
        return false;
    }
    QVariantMap recommendation;
    if (configuration.isEmpty()) {
        recommendation = model->configurationForFamily(
            automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality));
        if (recommendation.isEmpty()) {
            finishAutomaticSetupFailure(
                QStringLiteral("The required default model %1 is not available for %2.")
                    .arg(automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality),
                         capabilityId));
            return false;
        }
    } else {
        recommendation = {
            {QStringLiteral("familyId"), configuration.value(QStringLiteral("familyId"))},
            {QStringLiteral("runtimeId"), configuration.value(QStringLiteral("runtimeId"))},
            {QStringLiteral("runtimeVersion"), configuration.value(QStringLiteral("runtimeVersion"))},
            {QStringLiteral("selectedFiles"), configuration.value(QStringLiteral("selectedFiles"))}
        };
        model->setInitialSelectedFiles(recommendation.value(QStringLiteral("familyId")).toString(),
                                       recommendation.value(QStringLiteral("selectedFiles")).toMap());
        model->setSelectedFamilyId(recommendation.value(QStringLiteral("familyId")).toString());
        model->refresh();
    }

    const QString familyId = recommendation.value(QStringLiteral("familyId")).toString();
    QVariantMap familyItem = model->itemForFamily(familyId);
    if (familyItem.isEmpty()) {
        appendAutomaticEvent(
            QStringLiteral("Discarded stale %1 model setting: %2").arg(capabilityId, familyId),
            QStringLiteral("warning"), nodeId);
        m_workflowNodeConfigurations.remove(nodeId);
        m_automaticConfiguredNodes.remove(nodeId);
        scheduleAutomaticSetupAdvance();
        return false;
    }

    // A saved workflow may reference a model variant that has since been
    // removed from the compatibility catalog. Replace only those stale roles
    // with the currently supported selection so automatic runs can download
    // and use the compatible file without requiring manual reconfiguration.
    QVariantMap recommendedFiles = recommendation.value(QStringLiteral("selectedFiles")).toMap();
    const QVariantMap supportedFiles = familyItem.value(QStringLiteral("selectedFiles")).toMap();
    for (const QVariant &requirementValue : familyItem.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap requirement = requirementValue.toMap();
        const QString role = requirement.value(QStringLiteral("role")).toString();
        const QString selectedFile = recommendedFiles.value(role).toString();
        const QVariantList candidates = requirement.value(QStringLiteral("candidates")).toList();
        const QString defaultFile = requirement.value(QStringLiteral("file")).toString();
        const bool supported = candidates.isEmpty()
            ? selectedFile == defaultFile : candidates.contains(selectedFile);
        if (selectedFile.isEmpty() || !supported)
            recommendedFiles.insert(role, supportedFiles.value(role, defaultFile));
    }
    recommendation.insert(QStringLiteral("selectedFiles"), recommendedFiles);

    if (!familyItem.value(QStringLiteral("ready")).toBool()) {
        if (!m_automaticDownloadsQueued.contains(capabilityId)) {
            const QSet<QString> downloadsBefore = activeDownloadKeys();
            if (!app->downloadInstall()->enqueueRecommendedSetup(familyItem)) {
                finishAutomaticSetupFailure(
                    QStringLiteral("Could not start the %1 model download.").arg(capabilityId));
                return false;
            }
            captureNewAutomaticDownloads(downloadsBefore);
            m_automaticDownloadsQueued.insert(capabilityId);
            appendAutomaticEvent(
                QStringLiteral("Downloading the default %1 model and runtime").arg(capabilityId),
                QStringLiteral("downloading"), nodeId);
        }
        setAutomaticStatus(
            QStringLiteral("Preparing %1 model: %2").arg(capabilityId,
                familyItem.value(QStringLiteral("displayName"), familyId).toString()));
        return false;
    }

    m_automaticDownloadsQueued.remove(capabilityId);
    const QString runtimeId = recommendation.value(
        QStringLiteral("runtimeId"), familyItem.value(QStringLiteral("selectedRuntimeId"))).toString();
    const QString runtimeVersion = recommendation.value(
        QStringLiteral("runtimeVersion"), familyItem.value(QStringLiteral("selectedRuntimeVersion"))).toString();
    const QVariantMap selectedFiles = recommendation.value(
        QStringLiteral("selectedFiles"), familyItem.value(QStringLiteral("selectedFiles"))).toMap();

    if (!loadSession) {
        StudioConfiguration selected;
        selected.capabilityId = capabilityId;
        selected.familyId = familyId;
        selected.runtimeId = runtimeId;
        selected.runtimeVersion = runtimeVersion;
        selected.selectedFiles = selectedFiles;
        if (!StudioConfigurationResolver::resolve(selected).isValid) return false;
        if (configuration.isEmpty()) {
            if (!configureWorkflowNodeModel(nodeId, familyId, runtimeId,
                                            runtimeVersion, selectedFiles, false))
                return false;
            m_automaticConfiguredNodes.insert(nodeId);
        }
        return true;
    }

    IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
    if (session && session->canProcess()) return true;
    if (session && (session->state() == ModelSessionState::Loading
                    || session->state() == ModelSessionState::Processing)) {
        setAutomaticStatus(QStringLiteral("Loading %1 model into memory").arg(capabilityId));
        return false;
    }
    setAutomaticStatus(QStringLiteral("Loading %1 model into memory").arg(capabilityId));
    const bool automaticallySelected = configuration.isEmpty();
    if (automaticallySelected) m_automaticConfiguredNodes.insert(nodeId);
    const bool configured = configureWorkflowNodeModel(
        nodeId, familyId, runtimeId, runtimeVersion, selectedFiles, true);
    if (!configured && automaticallySelected) m_automaticConfiguredNodes.remove(nodeId);
    return configured && session && session->canProcess();
}

bool DubbingController::ensureAutomaticAdaptiveModel()
{
    m_automaticSetupNodeId = QStringLiteral("translate");
    if (m_project.dubbingQuality == QStringLiteral("custom")) {
        const bool rewriteEnabled =
            m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool();
        if (!rewriteEnabled || adaptiveReady()) return true;
        finishAutomaticSetupFailure(
            QStringLiteral("The Translate node requires a rewrite model for Custom dubbing."));
        return false;
    }
    if (m_project.dubbingQuality != QStringLiteral("adaptive")) return true;
    if (adaptiveReady()) return true;
    finishAutomaticSetupFailure(
        QStringLiteral("The selected Adaptive LLM route is not ready. Configure and verify it in Automatic preflight; no local fallback will be downloaded."));
    return false;
}

void DubbingController::scheduleAutomaticSetupAdvance()
{
    if (!m_automaticSetupActive || m_automaticAdvanceScheduled) return;
    m_automaticAdvanceScheduled = true;
    QTimer::singleShot(100, this, [this]() {
        m_automaticAdvanceScheduled = false;
        advanceAutomaticSetup();
    });
}

void DubbingController::prepareAutomaticVoiceRuntime()
{
    if (m_workflowMode != QStringLiteral("automatic")
        || !m_workflowRunner || !m_workflowRunner->running())
        return;
    const QVariantMap synthesis = m_workflowNodeConfigurations
                                      .value(QStringLiteral("synthesize")).toMap();
    if (configuredSynthesisProvider(synthesis) != ExecutionProvider::LocalDev) {
        const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
        const QString model = synthesis.value(
            QStringLiteral("modelId"), parameters.value(QStringLiteral("modelId"))).toString();
        setAutomaticStatus(QStringLiteral("Using selected remote TTS route%1")
                               .arg(model.trimmed().isEmpty()
                                        ? QString() : QStringLiteral(" (%1)").arg(model)));
        appendAutomaticEvent(QStringLiteral("Remote TTS route remains selected"),
                             QStringLiteral("completed"), QStringLiteral("synthesize"));
        return;
    }
    AppController *app = AppController::instance();
    if (!app || !app->sessionRegistry()) return;
    for (const QString &capabilityId : {QStringLiteral("stt"),
                                        QStringLiteral("translation")}) {
        IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
        if (session && (session->state() == ModelSessionState::Loading
                        || session->state() == ModelSessionState::Processing
                        || session->state() == ModelSessionState::Unloading
                        || !session->loadedConfigurations().isEmpty())) {
            QTimer::singleShot(100, this, &DubbingController::prepareAutomaticVoiceRuntime);
            return;
        }
    }
    IModelSession *tts = app->sessionRegistry()->sessionForCapability(QStringLiteral("tts"));
    if (tts && (tts->canProcess() || tts->state() == ModelSessionState::Loading)) return;
    setAutomaticStatus(QStringLiteral("Loading the selected model for the Voice node"));
    appendAutomaticEvent(QStringLiteral("Loading the selected voice generation model"),
                         QStringLiteral("loading"), QStringLiteral("synthesize"));
    if (!loadWorkflowNodeModel(QStringLiteral("synthesize"))) {
        setError(QStringLiteral("Could not load the selected model for the Voice node."));
        if (m_workflowRunner->running()) m_workflowRunner->cancel();
    }
}

void DubbingController::finishAutomaticSetupFailure(const QString &message)
{
    if (!m_automaticSetupActive) return;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticDownloadKeys.clear();
    m_automaticConfiguredNodes.clear();
    m_automaticSetupNodeId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setError(message);
    setAutomaticStatus(message);
    appendAutomaticEvent(message, QStringLiteral("failed"));
    emit processingChanged();
    emit workflowChanged();
}

void DubbingController::advanceAutomaticSetup()
{
    if (!m_automaticSetupActive) return;
    if (m_automaticSetupNodeId.isEmpty())
        m_automaticSetupNodeId = QStringLiteral("source-separate");
    // Remote-first automatic runs use the graph's explicit per-node routes.
    // They never probe, load, or download a local model as a fallback. A
    // missing Gateway model or Colab worker therefore fails at the selected
    // node with its own provider-specific error.
    if (auto *app = AppController::instance(); app && app->settings()
        && app->settings()->remoteFirstMode()) {
        configureRemoteRewriteFromGateway();
        const QString outputPath = m_automaticOutputPath;
        m_automaticSetupActive = false;
        m_automaticDownloadsQueued.clear();
        m_automaticDownloadKeys.clear();
        m_automaticConfiguredNodes.clear();
        m_automaticSetupNodeId.clear();
        setAutomaticStatus(QStringLiteral("Starting independent remote workflow routes."));
        appendAutomaticEvent(QStringLiteral("Using configured API Gateway and direct Colab routes"),
                             QStringLiteral("completed"));
        emit processingChanged();
        emit workflowChanged();
        if (m_runner) m_runner->setTranslationFixConfiguration(translationFixConfiguration());
        setCurrentStep(QStringLiteral("ingest"));
        if (!runWorkflow(outputPath)) {
            setWorkflowMode(QStringLiteral("idle"));
            setAutomaticStatus(lastError());
            appendAutomaticEvent(lastError(), QStringLiteral("failed"));
        }
        return;
    }
    if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
        bool waitingForRelease = false;
        for (const QString &capabilityId : {QStringLiteral("tts"),
                                            QStringLiteral("translation"),
                                            QStringLiteral("llm-chat")}) {
            IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
            if (!session) continue;
            const QList<SessionConfiguration> loaded = session->loadedConfigurations();
            for (const SessionConfiguration &configuration : loaded)
                session->requestUnloadConfiguration(configuration.signature);
            waitingForRelease = waitingForRelease || !loaded.isEmpty()
                || session->state() == ModelSessionState::Loading
                || session->state() == ModelSessionState::Processing
                || session->state() == ModelSessionState::Unloading;
        }
        if (waitingForRelease) {
            setAutomaticStatus(QStringLiteral("Releasing previously loaded native runtimes"));
            scheduleAutomaticSetupAdvance();
            return;
        }
    }
    if (!ensureAutomaticModel(QStringLiteral("source-separate"),
                              QStringLiteral("voice-isolation"), false)) return;
    appendAutomaticEvent(QStringLiteral("Voice isolation model is ready"),
                         QStringLiteral("completed"), QStringLiteral("source-separate"));
    const QString transcriptSource = normalizedTranscriptSource(m_project.transcriptConfiguration.value(
        QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
    if (transcriptSource == QStringLiteral("stt")) {
        if (!ensureAutomaticModel(QStringLiteral("transcribe"), QStringLiteral("stt"), true)) return;
        appendAutomaticEvent(QStringLiteral("Speech-to-text model is ready"),
                             QStringLiteral("completed"), QStringLiteral("transcribe"));
    } else {
        appendAutomaticEvent(QStringLiteral("STT setup skipped for OCR-only transcript"),
                             QStringLiteral("completed"), QStringLiteral("transcribe"));
    }
    if (!ensureAutomaticModel(QStringLiteral("translate"), QStringLiteral("translation"), false)) return;
    appendAutomaticEvent(QStringLiteral("Translation model is ready"),
                         QStringLiteral("completed"), QStringLiteral("translate"));
    if (!ensureAutomaticModel(QStringLiteral("synthesize"), QStringLiteral("tts"), false)) return;
    appendAutomaticEvent(QStringLiteral("Voice generation model is configured"),
                         QStringLiteral("completed"), QStringLiteral("synthesize"));
    if (!ensureAutomaticAdaptiveModel()) return;

    const QString outputPath = m_automaticOutputPath;
    m_automaticSetupActive = false;
    m_automaticDownloadsQueued.clear();
    m_automaticDownloadKeys.clear();
    m_automaticConfiguredNodes.clear();
    m_automaticSetupNodeId.clear();
    setAutomaticStatus(QStringLiteral("Models ready. Starting the dubbing workflow."));
    appendAutomaticEvent(QStringLiteral("All required models are ready"),
                         QStringLiteral("completed"));
    emit processingChanged();
    emit workflowChanged();
    if (m_runner) m_runner->setTranslationFixConfiguration(translationFixConfiguration());
    setCurrentStep(QStringLiteral("ingest"));
    if (!runWorkflow(outputPath)) {
        setWorkflowMode(QStringLiteral("idle"));
        setAutomaticStatus(lastError());
        appendAutomaticEvent(lastError(), QStringLiteral("failed"));
    }
}
