QString DubbingController::colabCapabilityForStage(const QString &stageId)
{
    if (stageId == QStringLiteral("source-separate")) return QStringLiteral("voice-isolation");
    if (stageId == QStringLiteral("transcribe")) return QStringLiteral("stt");
    if (stageId == QStringLiteral("subtitle-ocr")) return QStringLiteral("subtitle-ocr");
    if (stageId == QStringLiteral("translate")) return QStringLiteral("translation");
    if (stageId == QStringLiteral("synthesize")) return QStringLiteral("tts");
    if (stageId == QStringLiteral("voice-cloning")) return QStringLiteral("voice-cloning");
    if (stageId == QStringLiteral("alignment")) return QStringLiteral("forced-alignment");
    if (stageId == QStringLiteral("adaptive-llm")) return QStringLiteral("llm-chat");
    return {};
}

QStringList extractedSharedMediaUrls(const QString &pastedText)
{
    // Copy/share text from Douyin, TikTok and similar services contains a
    // short code and descriptive text around the real public URL.  Only pass
    // the URL to the resolver; share text is neither sent nor persisted.
    // Keep this deliberately ASCII-only.  URL extraction happens before URI
    // parsing and QRegularExpression's Windows PCRE build does not accept the
    // JavaScript-style Unicode escapes that were previously used here.  The
    // copied Douyin form separates its URL with whitespace, while trailing
    // ASCII share punctuation is stripped below.
    static const QRegularExpression urlPattern(
        QStringLiteral(R"((https?://[^\s<>"']+))"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList urls;
    QRegularExpressionMatchIterator matches = urlPattern.globalMatch(pastedText);
    while (matches.hasNext()) {
        QString url = matches.next().captured(1).trimmed();
        while (!url.isEmpty() && QStringLiteral(".,;:!?)]}").contains(url.back()))
            url.chop(1);
        if (!url.isEmpty()) urls.append(url);
    }
    if (!urls.isEmpty()) return urls;

    // Preserve the previous direct-input behavior when no explicit HTTP(S)
    // URL was found, so a concise valid URL still reaches the normal validator.
    return pastedText.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
}

ExecutionProvider configuredSynthesisProvider(const QVariantMap &configuration)
{
    const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
    ExecutionProvider provider = ExecutionProvider::LocalDev;
    executionProviderFromId(configuration.value(
        QStringLiteral("executionProvider"),
        parameters.value(QStringLiteral("executionProvider"),
                         QStringLiteral("local-dev"))).toString(), &provider);
    return provider;
}

ColabSession *DubbingController::colabSessionForStage(const QString &stageId) const
{
    AppController *app = AppController::instance();
    if (!app) return nullptr;
    if (stageId == QStringLiteral("source-separate")) return app->colabSeparationSession();
    if (stageId == QStringLiteral("transcribe")) return app->colabSttSession();
    if (stageId == QStringLiteral("subtitle-ocr")) return app->colabSubtitleOcrSession();
    if (stageId == QStringLiteral("translate")) return app->colabTranslationSession();
    if (stageId == QStringLiteral("synthesize")) return app->colabTtsSession();
    if (stageId == QStringLiteral("voice-cloning")) return app->colabVoiceCloneSession();
    if (stageId == QStringLiteral("alignment")) return app->colabAlignmentSession();
    if (stageId == QStringLiteral("adaptive-llm")) return app->colabChatSession();
    return nullptr;
}

QString DubbingController::selectedColabModelForStage(const QString &stageId) const
{
    if (stageId == QStringLiteral("adaptive-llm")) {
        const QString configured = translationFixConfiguration().value(
            QStringLiteral("model")).toString().trimmed().toLower();
        return configured.isEmpty()
            ? DubbingColabModelRoutes::defaultModelForNode(stageId) : configured;
    }
    if (stageId == QStringLiteral("subtitle-ocr")) {
        const QString configured = m_project.transcriptConfiguration.value(
            QStringLiteral("ocrColabModelId")).toString().trimmed().toLower();
        return configured.isEmpty()
            ? DubbingColabModelRoutes::defaultModelForNode(stageId)
            : configured;
    }
    if (stageId == QStringLiteral("transcribe")) {
        const QString persisted = m_project.transcriptConfiguration.value(
            QStringLiteral("sttModelId")).toString().trimmed().toLower();
        if (!persisted.isEmpty()) return persisted;
    }
    const QString nodeId = stageId == QStringLiteral("alignment") ? QStringLiteral("alignment") : stageId;
    const QVariantMap configuration = m_workflowNodeConfigurations.value(
        stageId == QStringLiteral("alignment") ? QStringLiteral("transcribe") : nodeId).toMap();
    const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
    QString model;
    if (stageId == QStringLiteral("alignment"))
        model = parameters.value(QStringLiteral("alignmentModelId")).toString();
    else
        model = parameters.value(QStringLiteral("modelId")).toString();
    if (model.trimmed().isEmpty()) model = DubbingColabModelRoutes::defaultModelForNode(nodeId);
    return model.trimmed().toLower();
}

bool DubbingController::stageUsesDirectColab(const QString &stageId) const
{
    if (stageId == QStringLiteral("adaptive-llm")) {
        const bool rewriteRequired = m_project.dubbingQuality == QStringLiteral("adaptive")
            || (m_project.dubbingQuality == QStringLiteral("custom")
                && m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
                && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
        return rewriteRequired && adaptiveProvider() == QStringLiteral("colab-direct");
    }
    if (stageId == QStringLiteral("subtitle-ocr")) {
        const QString persistedRoute = m_project.transcriptConfiguration.value(
            QStringLiteral("ocrExecutionRoute")).toString().trimmed().toLower();
        const bool routeSelected = persistedRoute == QStringLiteral("colab-gpu")
            || (m_subtitleOcr && m_subtitleOcr->executionRoute() == QStringLiteral("colab-gpu"));
        return routeSelected;
    }
    const QString configurationNode = stageId == QStringLiteral("alignment") ? QStringLiteral("transcribe") : stageId;
    const QVariantMap configuration = m_workflowNodeConfigurations.value(configurationNode).toMap();
    const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
    const QString persistedProvider = stageId == QStringLiteral("transcribe")
        ? m_project.transcriptConfiguration.value(QStringLiteral("sttExecutionProvider"))
              .toString().trimmed().toLower()
        : QString();
    const QString provider = persistedProvider.isEmpty()
        ? configuration.value(QStringLiteral("executionProvider"),
              parameters.value(QStringLiteral("executionProvider"))).toString().trimmed().toLower()
        : persistedProvider;
    if (stageId == QStringLiteral("alignment"))
        return parameters.value(QStringLiteral("refineAlignmentWithColab")).toBool();
    return provider == QStringLiteral("colab-direct");
}

bool DubbingController::stageRequiredForCurrentTranscriptAction(const QString &stageId) const
{
    const QString transcriptSource = normalizedTranscriptSource(
        m_project.transcriptConfiguration.value(
            QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
    if (stageId == QStringLiteral("transcribe"))
        return transcriptSource == QStringLiteral("stt");
    if (stageId == QStringLiteral("subtitle-ocr"))
        return transcriptSource == QStringLiteral("ocr");
    return true;
}

bool DubbingController::snapshotSelectedColabStagesForWorkflow()
{
    // A Direct Colab route may be configured from either the global Dubbing panel
    // or its feature-specific panel. In both cases, capture only the verified
    // model identifier immediately before a workflow begins. URLs and tokens stay
    // exclusively in ColabSession's process-memory state.
    for (const QVariant &entry : colabSetupStages()) {
        const QVariantMap stage = entry.toMap();
        if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()
            || !stageRequiredForCurrentTranscriptAction(
                stage.value(QStringLiteral("id")).toString())) {
            continue;
        }

        const QString stageId = stage.value(QStringLiteral("id")).toString();
        const QString capability = stage.value(QStringLiteral("capability")).toString();
        const QString model = stage.value(QStringLiteral("modelId")).toString();
        ColabSession *session = colabSessionForStage(stageId);
        QString routeError;
        if (!session || !session->hasVerifiedRoute(capability, model, &routeError)) {
            m_colabSetupSnapshots.remove(stageId);
            const QString detail = routeError.trimmed().isEmpty()
                ? QStringLiteral("Connect and check its exact model in Colab setup.")
                : routeError;
            setError(QStringLiteral("Direct Colab is not ready for %1: %2")
                         .arg(stage.value(QStringLiteral("title")).toString(), detail));
            emit colabSetupChanged();
            return false;
        }
        m_colabSetupSnapshots.insert(stageId, model);
    }
    emit colabSetupChanged();
    return true;
}

void DubbingController::observeColabSession(const QString &stageId, ColabSession *session)
{
    if (m_colabSetupConnections.contains(stageId)) {
        QObject::disconnect(m_colabSetupConnections.take(stageId));
    }
    if (!session) return;
    m_colabSetupConnections.insert(stageId, connect(
        session, &ColabSession::verificationFinished, this,
        [this, stageId](bool success, const QString &) {
            refreshColabSetupSnapshot(stageId, success);
        }));
}

void DubbingController::refreshColabSetupSnapshot(const QString &stageId, bool verified)
{
    ColabSession *session = colabSessionForStage(stageId);
    const QString capability = colabCapabilityForStage(stageId);
    const QString model = selectedColabModelForStage(stageId);
    QString routeError;
    const bool valid = verified && session
        && session->hasVerifiedRoute(capability, model, &routeError);
    if (valid) {
        m_colabSetupSnapshots.insert(stageId, model);
    } else {
        m_colabSetupSnapshots.remove(stageId);
    }
    m_colabSetupPendingChecks.remove(stageId);
    if (m_colabSetupPendingChecks.isEmpty()) {
        int selected = 0;
        int verifiedCount = 0;
        for (const QVariant &entry : colabSetupStages()) {
            const QVariantMap stage = entry.toMap();
            if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()) continue;
            ++selected;
            verifiedCount += stage.value(QStringLiteral("verified")).toBool() ? 1 : 0;
        }
        m_colabSetupSummary = selected == 0
            ? QStringLiteral("No workflow stage is currently set to Direct Colab.")
            : QStringLiteral("%1 of %2 selected Direct Colab stage(s) verified.")
                  .arg(verifiedCount).arg(selected);
    }
    emit colabSetupChanged();
    emit workflowChanged();
}

QVariantList DubbingController::colabSetupStages() const
{
    QList<QPair<QString, QString>> definitions{
        {QStringLiteral("source-separate"), QStringLiteral("Isolator (Vocals/Background)")},
        {QStringLiteral("transcribe"), QStringLiteral("Transcribe/STT")},
        {QStringLiteral("subtitle-ocr"), QStringLiteral("Subtitle OCR (Transcribe)")},
        {QStringLiteral("translate"), QStringLiteral("Translation")},
        {QStringLiteral("synthesize"), QStringLiteral("TTS / Text to Speech")},
        {QStringLiteral("alignment"), QStringLiteral("Alignment")},
    };
    if (stageUsesDirectColab(QStringLiteral("adaptive-llm"))) {
        definitions.append({QStringLiteral("adaptive-llm"),
                            QStringLiteral("Translate (Adaptive LLM)")});
    }
    QVariantList result;
    for (const auto &definition : definitions) {
        const QString stageId = definition.first;
        const QString capability = colabCapabilityForStage(stageId);
        const QString model = selectedColabModelForStage(stageId);
        ColabSession *session = colabSessionForStage(stageId);
        QString diagnostic;
        const bool verified = session && session->hasVerifiedRoute(capability, model, &diagnostic);
        if (diagnostic.isEmpty() && session)
            diagnostic = session->verificationMessage().isEmpty()
                ? session->lastError() : session->verificationMessage();
        if (diagnostic.isEmpty())
            diagnostic = QStringLiteral("Not connected for this exact model.");
        result.append(QVariantMap{
            {QStringLiteral("id"), stageId},
            {QStringLiteral("title"), definition.second},
            {QStringLiteral("capability"), capability},
            {QStringLiteral("modelId"), model},
            // Current exact-model notebooks each expose one immutable GPU
            // configuration.  Make that explicit in every Dubbing surface;
            // it is not a Local CPU model-file variant.
            {QStringLiteral("variant"), session && !session->expectedVariant().isEmpty()
                ? session->expectedVariant() : QStringLiteral("fixed")},
            {QStringLiteral("notebookFile"), DubbingColabModelRoutes::notebookForModel(stageId, model)},
            {QStringLiteral("selectedForDirectColab"), stageUsesDirectColab(stageId)},
            {QStringLiteral("requiredForCurrentTranscriptAction"),
                stageRequiredForCurrentTranscriptAction(stageId)},
            {QStringLiteral("active"), session && session->isActive()},
            {QStringLiteral("checking"), session && session->isChecking()},
            {QStringLiteral("verified"), verified},
            {QStringLiteral("snapshotValid"), m_colabSetupSnapshots.value(stageId) == model && verified},
            {QStringLiteral("diagnostic"), diagnostic}
        });
    }
    return result;
}

bool DubbingController::connectWorkflowColabStage(const QString &stageId, const QString &modelId,
                                                   const QString &workerUrl, const QString &bearerToken)
{
    const QString normalizedStage = stageId.trimmed().toLower();
    const QString normalizedModel = modelId.trimmed().toLower();
    const QString capability = colabCapabilityForStage(normalizedStage);
    ColabSession *session = colabSessionForStage(normalizedStage);
    if (capability.isEmpty() || !session || normalizedModel.isEmpty()) {
        setError(QStringLiteral("This Dubbing Colab stage is unavailable."));
        return false;
    }
    if (!selectWorkflowColabModel(normalizedStage, normalizedModel)) return false;
    m_colabSetupSnapshots.remove(normalizedStage);
    if (!session->connectTemporaryWorker(workerUrl, bearerToken, capability, normalizedModel)) {
        setError(session->lastError().isEmpty()
                     ? QStringLiteral("Could not start the Direct Colab verification.")
                     : session->lastError());
        emit colabSetupChanged();
        return false;
    }
    m_colabSetupPendingChecks.insert(normalizedStage);
    m_colabSetupSummary = QStringLiteral("Checking %1 / %2 on Direct Colab.")
        .arg(capability, normalizedModel);
    emit colabSetupChanged();
    return true;
}

bool DubbingController::connectUnifiedWorkflowColab(const QString &workerUrl,
                                                     const QString &bearerToken)
{
#if defined(LASTUDIO_UNIT_TESTS)
    // Tests exercise the complete per-stage handshake against a loopback
    // coordinator. Production always requires the public HTTPS tunnel.
    constexpr bool allowInsecureLoopbackForTests = true;
#else
    constexpr bool allowInsecureLoopbackForTests = false;
#endif
    const RemoteEndpointValidation base = validateRemoteEndpoint(
        workerUrl, RemoteEndpointKind::ColabWorker, allowInsecureLoopbackForTests);
    const QString token = bearerToken.trimmed();
    if (!base.isValid()) {
        setError(base.error);
        return false;
    }
    if (token.isEmpty()) {
        setError(QStringLiteral("Unified Colab worker bearer token is required."));
        return false;
    }

    struct SelectedStage {
        QString id;
        QString capability;
        QString model;
        ColabSession *session = nullptr;
    };
    QList<SelectedStage> selected;
    for (const QVariant &entry : colabSetupStages()) {
        const QVariantMap stage = entry.toMap();
        if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()) continue;
        const QString id = stage.value(QStringLiteral("id")).toString();
        const QString capability = stage.value(QStringLiteral("capability")).toString();
        const QString model = stage.value(QStringLiteral("modelId")).toString();
        ColabSession *session = colabSessionForStage(id);
        if (!session || capability.isEmpty() || model.isEmpty()
            || !DubbingColabModelRoutes::supports(id, model)) {
            setError(QStringLiteral("Unified Colab cannot configure the selected %1 stage.")
                         .arg(stage.value(QStringLiteral("title")).toString()));
            return false;
        }
        selected.append({id, capability, model, session});
    }
    if (selected.isEmpty()) {
        setError(QStringLiteral("Select Direct Colab for at least one Dubbing stage before using Unified Colab."));
        return false;
    }

    // All validation happened before mutating a session. From here each
    // session gets a derived route with the same short-lived token. Neither
    // m_project nor Settings receives the URL/token.
    // Register every expected reply before submitting the first asynchronous
    // verification request.  A fast coordinator on localhost/a LAN can reply
    // before the loop reaches its final stage; registering incrementally used
    // to let the UI briefly report a completed setup with stages still being
    // submitted.
    m_colabSetupPendingChecks.clear();
    for (const SelectedStage &stage : selected)
        m_colabSetupPendingChecks.insert(stage.id);
    QList<SelectedStage> configured;
    for (const SelectedStage &stage : selected) {
        m_colabSetupSnapshots.remove(stage.id);
        const QString endpoint = unifiedColabStageUrl(base.normalizedUrl,
                                                       stage.capability, stage.model);
        QString connectionError;
        if (!stage.session->beginVerifiedSession(endpoint, token,
                                                 stage.capability, stage.model,
                                                 &connectionError,
                                                 allowInsecureLoopbackForTests)) {
            // beginVerifiedSession may have accepted the endpoint far enough
            // to create a reply connection before a later validation error.
            // Do not leave that half-configured stage active when the unified
            // transaction is rolled back.
            stage.session->disconnectTemporaryWorker();
            for (const SelectedStage &previous : configured) {
                previous.session->disconnectTemporaryWorker();
                m_colabSetupSnapshots.remove(previous.id);
            }
            m_colabSetupPendingChecks.clear();
            setError(connectionError.isEmpty()
                         ? QStringLiteral("Could not start Unified Colab verification.")
                         : connectionError);
            emit colabSetupChanged();
            return false;
        }
        configured.append(stage);
        m_colabSetupPendingChecks.insert(stage.id);
    }
    m_colabSetupSummary = QStringLiteral("Checking %1 selected Direct Colab stage(s) through Unified Colab.")
        .arg(selected.size());
    emit colabSetupChanged();
    return true;
}

bool DubbingController::checkWorkflowColabStage(const QString &stageId)
{
    const QString normalizedStage = stageId.trimmed().toLower();
    ColabSession *session = colabSessionForStage(normalizedStage);
    if (!session || !session->isActive()) {
        setError(QStringLiteral("Connect the %1 Direct Colab worker before checking it.")
                     .arg(colabCapabilityForStage(normalizedStage)));
        return false;
    }
    m_colabSetupSnapshots.remove(normalizedStage);
    if (!session->checkConnection()) {
        setError(session->lastError().isEmpty()
                     ? QStringLiteral("Could not start the Direct Colab connection check.")
                     : session->lastError());
        return false;
    }
    m_colabSetupPendingChecks.insert(normalizedStage);
    m_colabSetupSummary = QStringLiteral("Rechecking %1.").arg(colabCapabilityForStage(normalizedStage));
    emit colabSetupChanged();
    return true;
}

void DubbingController::disconnectWorkflowColabStage(const QString &stageId)
{
    const QString normalizedStage = stageId.trimmed().toLower();
    if (ColabSession *session = colabSessionForStage(normalizedStage))
        session->disconnectTemporaryWorker();
    m_colabSetupSnapshots.remove(normalizedStage);
    m_colabSetupPendingChecks.remove(normalizedStage);
    m_colabSetupSummary = QStringLiteral("Disconnected %1 Direct Colab setup.")
        .arg(colabCapabilityForStage(normalizedStage));
    emit colabSetupChanged();
    emit workflowChanged();
}

bool DubbingController::validateAllWorkflowColabStages()
{
    m_colabSetupPendingChecks.clear();
    QStringList unavailable;
    int requested = 0;
    for (const QVariant &entry : colabSetupStages()) {
        const QVariantMap stage = entry.toMap();
        if (!stage.value(QStringLiteral("selectedForDirectColab")).toBool()) continue;
        const QString stageId = stage.value(QStringLiteral("id")).toString();
        ColabSession *session = colabSessionForStage(stageId);
        ++requested;
        m_colabSetupSnapshots.remove(stageId);
        if (!session || !session->isActive() || !session->checkConnection()) {
            unavailable.append(stage.value(QStringLiteral("title")).toString());
            continue;
        }
        m_colabSetupPendingChecks.insert(stageId);
    }
    if (requested == 0) {
        m_colabSetupSummary = QStringLiteral("No workflow stage is currently set to Direct Colab.");
        emit colabSetupChanged();
        return true;
    }
    if (!unavailable.isEmpty()) {
        m_colabSetupSummary = QStringLiteral("Could not check: %1.").arg(unavailable.join(QStringLiteral(", ")));
        emit colabSetupChanged();
        return false;
    }
    m_colabSetupSummary = QStringLiteral("Checking %1 Direct Colab stage(s).").arg(requested);
    emit colabSetupChanged();
    return true;
}

void DubbingController::setVoiceClonePresetService(VoiceClonePresetService *service)
{
    if (m_voiceClonePresetsService == service) return;
    QObject::disconnect(m_cloneVoicePresetsConnection);
    m_voiceClonePresetsService = service;
    if (m_voiceClonePresetsService) {
        m_cloneVoicePresetsConnection = connect(
            m_voiceClonePresetsService, &VoiceClonePresetService::presetsChanged,
            this, [this](const QString &) { refreshCloneVoicePresets(); });
    }
    refreshCloneVoicePresets();
    emit cloneVoiceSelectionChanged();
    emit workflowChanged();
}
