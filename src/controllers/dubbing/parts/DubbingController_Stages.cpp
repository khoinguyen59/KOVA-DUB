QVariantMap DubbingController::subtitleConfiguration() const
{
    QVariantMap configuration = m_project.subtitleConfiguration;
    QVariantMap style;
    QString ignored;
    if (!DubbingSubtitleService::normalizeStyle(configuration.value(QStringLiteral("style")).toMap(),
                                                style, &ignored)) {
        style = DubbingSubtitleService::defaultStyle();
    }
    configuration.insert(QStringLiteral("style"), style);
    if (!configuration.contains(QStringLiteral("source")))
        configuration.insert(QStringLiteral("source"), QStringLiteral("segments"));
    QString textSource = configuration.value(QStringLiteral("textSource"),
                                             QStringLiteral("target")).toString().trimmed().toLower();
    if (textSource != QStringLiteral("source") && textSource != QStringLiteral("target"))
        textSource = QStringLiteral("target");
    configuration.insert(QStringLiteral("textSource"), textSource);
    if (!configuration.contains(QStringLiteral("burnIn")))
        configuration.insert(QStringLiteral("burnIn"), false);
    return configuration;
}

QVariantMap DubbingController::timingConfiguration() const
{
    QVariantMap configuration = m_project.timingConfiguration;
    QString mode = configuration.value(QStringLiteral("mode"), QStringLiteral("keep"))
                       .toString().trimmed().toLower();
    if (mode != QStringLiteral("keep") && mode != QStringLiteral("ripple")
        && mode != QStringLiteral("manual")) {
        mode = QStringLiteral("keep");
    }
    configuration.insert(QStringLiteral("mode"), mode);
    configuration.insert(QStringLiteral("minimumGapMs"),
                         qBound(0, configuration.value(QStringLiteral("minimumGapMs"), 80).toInt(),
                                5000));
    return configuration;
}

QVariantList DubbingController::timingConflicts() const
{
    const QVariantMap configuration = timingConfiguration();
    return DubbingTimingService::analyzeSpeechOverlaps(
               m_project.segments, configuration.value(QStringLiteral("minimumGapMs")).toLongLong())
        .value(QStringLiteral("conflicts")).toList();
}

QVariantMap DubbingController::previewTimingResolution(const QString &mode, int minimumGapMs)
{
    if (processing()) {
        setBusyError(QStringLiteral("Wait for the current Dubbing operation before reviewing speech timing."));
        return {};
    }
    const QString normalizedMode = mode.trimmed().toLower();
    if (normalizedMode != QStringLiteral("keep") && normalizedMode != QStringLiteral("ripple")
        && normalizedMode != QStringLiteral("manual")) {
        setError(QStringLiteral("Choose Keep timing, Ripple forward, or Manual timing."));
        return {};
    }
    if (!hasProject()) {
        setError(QStringLiteral("Open a dubbing project before resolving speech timing."));
        return {};
    }

    const qint64 gapMs = qBound(0, minimumGapMs, 5000);
    QVariantMap report;
    if (normalizedMode == QStringLiteral("ripple")) {
        QString error;
        const QVariantList revised = DubbingTimingService::rippleForward(
            m_project.segments, gapMs, &report, &error);
        if (revised.isEmpty() && !m_project.segments.isEmpty()) {
            setError(error);
            return {};
        }
    } else {
        report = DubbingTimingService::analyzeSpeechOverlaps(m_project.segments, gapMs);
        report.insert(QStringLiteral("mode"), normalizedMode);
        report.insert(QStringLiteral("manualReviewRequired"), normalizedMode == QStringLiteral("manual"));
    }
    m_timingResolutionPreview = report;
    clearError();
    emit timingResolutionChanged();
    return report;
}

bool DubbingController::applyTimingResolution(const QString &mode, int minimumGapMs)
{
    if (processing()) {
        setBusyError(QStringLiteral("Wait for the current Dubbing operation before changing speech timing."));
        return false;
    }
    const QString normalizedMode = mode.trimmed().toLower();
    if (normalizedMode != QStringLiteral("keep") && normalizedMode != QStringLiteral("ripple")
        && normalizedMode != QStringLiteral("manual")) {
        setError(QStringLiteral("Choose Keep timing, Ripple forward, or Manual timing."));
        return false;
    }
    const QVariantMap preview = previewTimingResolution(normalizedMode, minimumGapMs);
    if (preview.isEmpty() && !m_project.segments.isEmpty()) return false;

    QVariantMap configuration = timingConfiguration();
    configuration.insert(QStringLiteral("mode"), normalizedMode);
    configuration.insert(QStringLiteral("minimumGapMs"), qBound(0, minimumGapMs, 5000));

    if (normalizedMode == QStringLiteral("ripple")) {
        QString error;
        QVariantMap report;
        const QVariantList revised = DubbingTimingService::rippleForward(
            m_project.segments, configuration.value(QStringLiteral("minimumGapMs")).toLongLong(),
            &report, &error);
        if (revised.isEmpty() && !m_project.segments.isEmpty()) {
            setError(error);
            return false;
        }
        if (report.value(QStringLiteral("blockingConflictCount")).toInt() != 0) {
            setError(QStringLiteral("Ripple preview still contains blocking speech overlaps."));
            return false;
        }
        m_timingUndoSegments = m_project.segments;
        m_project.segments = revised;
        m_timingResolutionPreview = report;
        invalidateTimingOutputs();
        emit segmentsChanged();
        emit workflowChanged();
    } else {
        // Keep and manual modes deliberately leave all timestamps unchanged.
        // Manual mode is a durable explicit-review decision, not an automatic repair.
        m_timingUndoSegments.clear();
    }

    m_project.timingConfiguration = configuration;
    clearError();
    emit projectChanged();
    emit timingResolutionChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::undoTimingResolution()
{
    if (processing()) {
        setBusyError(QStringLiteral("Wait for the current Dubbing operation before undoing speech timing."));
        return false;
    }
    if (m_timingUndoSegments.isEmpty()) {
        setError(QStringLiteral("No ripple timing change is available to undo."));
        return false;
    }
    m_project.segments = m_timingUndoSegments;
    m_timingUndoSegments.clear();
    m_timingResolutionPreview = DubbingTimingService::analyzeSpeechOverlaps(
        m_project.segments,
        timingConfiguration().value(QStringLiteral("minimumGapMs")).toLongLong());
    invalidateTimingOutputs();
    clearError();
    emit segmentsChanged();
    emit projectChanged();
    emit workflowChanged();
    emit timingResolutionChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::setIntentionalTimingOverlap(int segmentIndex, bool enabled)
{
    if (segmentIndex < 0 || segmentIndex >= m_project.segments.size()) {
        setError(QStringLiteral("Choose a valid speech segment before changing overlap intent."));
        return false;
    }
    QVariantMap segment = m_project.segments.at(segmentIndex).toMap();
    segment.insert(QStringLiteral("intentionalOverlap"), enabled);
    m_project.segments[segmentIndex] = segment;
    m_timingResolutionPreview = DubbingTimingService::analyzeSpeechOverlaps(
        m_project.segments,
        timingConfiguration().value(QStringLiteral("minimumGapMs")).toLongLong());
    clearError();
    emit segmentsChanged();
    emit timingResolutionChanged();
    persistAfterEdit();
    return true;
}


void DubbingController::onIngestFinished(bool success, const QVariantMap &manifest)
{
    if (!success) return; // Error is already handled and set on runner
    
    m_project.sourceMediaPath = manifest.value(QStringLiteral("sourcePath")).toString();
    m_project.sourceHash = manifest.value(QStringLiteral("sourceHash")).toString();
    m_project.masterAudioPath = manifest.value(QStringLiteral("masterAudioPath")).toString();
    m_project.analysisAudioPath = manifest.value(QStringLiteral("analysisAudioPath")).toString();
    m_project.vocalsAudioPath = manifest.value(QStringLiteral("vocalsAudioPath")).toString();
    m_project.backgroundAudioPath = manifest.value(QStringLiteral("backgroundAudioPath")).toString();
    m_runner->setBackgroundAudioPath(manifest.value(QStringLiteral("backgroundAudioPath")).toString());
    m_project.sourceDurationMs = manifest.value(QStringLiteral("sourceDurationMs")).toLongLong();
    m_project.sourceSampleRate = manifest.value(QStringLiteral("sourceSampleRate")).toInt();
    m_project.sourceChannels = manifest.value(QStringLiteral("sourceChannels")).toInt();
    m_project.sourceIsVideo = manifest.value(QStringLiteral("sourceIsVideo")).toBool();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Media normalized successfully: source=%1, hash=%2, master=%3, analysis=%4, vocals=%5")
                     .arg(m_project.sourceMediaPath, m_project.sourceHash,
                          m_project.masterAudioPath, m_project.analysisAudioPath,
                          m_project.vocalsAudioPath));
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}


void DubbingController::transcribeSource()
{
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import media before starting transcription."));
        return;
    }
    QVariantMap configuration = effectiveTranscriptConfiguration(true);
    QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
    parameters.insert(QStringLiteral("ocrSourceMedia"), m_project.sourceMediaPath);
    configuration.insert(QStringLiteral("parameters"), parameters);
    const QString mode = normalizedTranscriptSource(
        parameters.value(QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
    if (mode == QStringLiteral("reconcile")) {
        reconcileTranscriptSources();
        return;
    }
    const QString audioPath = !m_project.vocalsAudioPath.isEmpty()
        ? m_project.vocalsAudioPath
        : (!m_project.analysisAudioPath.isEmpty() ? m_project.analysisAudioPath
                                                 : m_project.masterAudioPath);
    const QFileInfo audioInfo(audioPath);
    if (mode != QStringLiteral("ocr") && (audioPath.isEmpty() || !audioInfo.isFile()
                                           || audioInfo.size() <= 0)) {
        setError(QStringLiteral(
            "Normalize the source audio before transcription. A separated vocals stem is preferred, "
            "but the normalized analysis track can be used as a fallback."));
        return;
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Starting dubbing transcription source=%1 language=%2 audio=%3")
                     .arg(mode, m_project.sourceLanguage, audioPath));
    persistAfterEdit();
    m_runner->startTranscription(m_project.sourceLanguage, audioPath, QString(), configuration);
}

bool DubbingController::runSubtitleOcrIndependently()
{
    if (!m_subtitleOcr) {
        setError(QStringLiteral("Subtitle OCR is unavailable in this application session."));
        return false;
    }
    if (m_independentSubtitleOcrActive || m_subtitleOcr->processing()) {
        setBusyError(QStringLiteral("Subtitle OCR is already preparing or running."));
        return false;
    }
    if (!canRunIndependentSubtitleOcrAlongsideCurrentWork()) {
        setBusyError(QStringLiteral("Subtitle OCR can run beside STT only; wait for the current non-STT Dubbing task to finish."));
        return false;
    }
    const QFileInfo source(m_project.sourceMediaPath);
    if (!source.isFile() || source.size() <= 0) {
        setError(QStringLiteral("Import a readable video before running Subtitle OCR."));
        return false;
    }

    applyStoredSubtitleOcrConfiguration();
    m_independentSubtitleOcrActive = true;
    m_independentSubtitleOcrLoadingSource = true;
    m_independentSubtitleOcrSourcePath = source.absoluteFilePath();
    clearError();
    emit subtitleOcrProcessingChanged();
    emit processingChanged();
    emit workflowChanged();

    if (!m_subtitleOcr->loadSource(m_independentSubtitleOcrSourcePath)) {
        const QString message = m_subtitleOcr->error().trimmed().isEmpty()
            ? QStringLiteral("The selected video could not be prepared for Subtitle OCR.")
            : m_subtitleOcr->error();
        m_independentSubtitleOcrActive = false;
        m_independentSubtitleOcrLoadingSource = false;
        m_independentSubtitleOcrSourcePath.clear();
        setError(QStringLiteral("OCR transcript failed: %1").arg(message));
        emit subtitleOcrProcessingChanged();
        emit processingChanged();
        emit workflowChanged();
        return false;
    }
    return true;
}

bool DubbingController::canRunIndependentSubtitleOcrAlongsideCurrentWork() const
{
    if (m_independentSubtitleOcrActive || (m_subtitleOcr && m_subtitleOcr->processing()))
        return false;
    if (m_automaticSetupActive || m_mediaQueueProcessing
        || (m_translationFix && m_translationFix->busy())
        || (m_workflowRunner && m_workflowRunner->running())) {
        return false;
    }
    // The only runner operation which is independent from OCR is audio STT.
    return !m_runner || !m_runner->processing()
        || m_runner->stage() == QStringLiteral("transcribe");
}

bool DubbingController::canRunIndependentAudioSttAlongsideCurrentWork() const
{
    if (m_automaticSetupActive || m_mediaQueueProcessing
        || (m_translationFix && m_translationFix->busy())
        || (m_workflowRunner && m_workflowRunner->running())) {
        return false;
    }
    // The only controller-owned work that can coexist with audio STT is the
    // independent Subtitle OCR pass. A second STT run remains rejected by the
    // production runner itself.
    return subtitleOcrProcessing()
        && (!m_runner || !m_runner->processing());
}

bool DubbingController::reconcileTranscriptSources()
{
    const QVariantList sttSegments = m_project.transcriptConfiguration
                                         .value(QStringLiteral("sttSegments")).toList();
    const QVariantList ocrSegments = m_project.transcriptConfiguration
                                         .value(QStringLiteral("ocrSegments")).toList();
    if (sttSegments.isEmpty() || ocrSegments.isEmpty()) {
        setError(QStringLiteral("Reconcile requires completed independent STT and OCR transcripts. Run or upload the missing source first."));
        return false;
    }

    const QString policy = m_project.transcriptConfiguration
                               .value(QStringLiteral("fusionPolicy"), QStringLiteral("ask")).toString();
    const QVariantList reconciled = DubbingTranscriptFusionService::fuse(sttSegments, ocrSegments, policy);
    if (reconciled.isEmpty()) {
        setError(QStringLiteral("Reconcile produced no usable transcript segments. Keep the STT/OCR results and review their source timing."));
        return false;
    }
    m_project.segments = reconciled;
    m_project.transcriptConfiguration.insert(QStringLiteral("reconciledSegments"), reconciled);
    m_project.transcriptConfiguration.insert(QStringLiteral("lastReconciledPolicy"),
                                              DubbingTranscriptFusionService::normalizePolicy(policy));
    QVariantMap reconciliationOutput;
    reconciliationOutput.insert(QStringLiteral("transcript"), reconciled);
    reconciliationOutput.insert(QStringLiteral("transcriptSource"), QStringLiteral("reconcile"));
    m_stepOutputs.insert(QStringLiteral("review-transcript"), reconciliationOutput);
    m_lastCompletedStepId = QStringLiteral("review-transcript");
    clearError();
    emit segmentsChanged();
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

void DubbingController::translateSource()
{
    int emptySourceCount = 0;
    for (const QVariant &value : std::as_const(m_project.segments)) {
        if (value.toMap().value(QStringLiteral("sourceText")).toString().trimmed().isEmpty())
            ++emptySourceCount;
    }
    const QVariantMap configured =
        m_workflowNodeConfigurations.value(QStringLiteral("translate")).toMap();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Translation requested sourceLanguage=%1 targetLanguage=%2 segments=%3 emptySource=%4 family=%5 runtime=%6 runtimeVersion=%7")
                     .arg(m_project.sourceLanguage, m_project.targetLanguage)
                     .arg(m_project.segments.size())
                     .arg(emptySourceCount)
                     .arg(configured.value(QStringLiteral("familyId")).toString(),
                          configured.value(QStringLiteral("runtimeId")).toString(),
                          configured.value(QStringLiteral("runtimeVersion")).toString()));
    if (m_project.sourceMediaPath.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: source media is empty."));
        setError(QStringLiteral("Import media before translating."));
        return;
    }
    if (m_project.segments.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: transcript has no segments."));
        setError(QStringLiteral("Transcribe the source before translating."));
        return;
    }
    const int unresolvedConflicts = unresolvedTranscriptConflictCount();
    if (unresolvedConflicts > 0) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: %1 STT/OCR conflict(s) still require review.")
                            .arg(unresolvedConflicts));
        setError(QStringLiteral("Resolve %1 STT/OCR conflict(s) before translating. The original STT and OCR evidence has been retained for review.")
                     .arg(unresolvedConflicts));
        return;
    }
    if (m_project.targetLanguage.trimmed().isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: target language is empty."));
        setError(QStringLiteral("Choose a target language before translating."));
        return;
    }
    if (emptySourceCount > 0) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation request contains %1 segment(s) without source text.")
                            .arg(emptySourceCount));
    }
    QVariantMap translationConfig = configured;
    QVariantMap parameters = translationConfig.value(QStringLiteral("parameters")).toMap();
    QVariantMap effectiveDurationControl = m_project.durationControl;
    effectiveDurationControl.insert(
        QStringLiteral("autoRewrite"),
        m_project.dubbingQuality != QStringLiteral("fast")
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
    parameters.insert(QStringLiteral("durationControl"), effectiveDurationControl);
    translationConfig.insert(QStringLiteral("parameters"), parameters);
    m_runner->setTranslationFixConfiguration(translationFixConfiguration());
    m_runner->startTranslation(m_project.sourceLanguage, m_project.targetLanguage, m_project.segments,
                               translationConfig);
}

void DubbingController::generateAudio()
{
    const QVariantMap synthesisConfiguration = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    QVariantMap synthesisSettings = synthesisConfiguration
        .value(QStringLiteral("parameters")).toMap();
    synthesisSettings.insert(QStringLiteral("familyId"),
                             synthesisConfiguration.value(QStringLiteral("familyId")));
    if (!synthesisSettings.contains(QStringLiteral("lang")))
        synthesisSettings.insert(QStringLiteral("lang"), m_project.targetLanguage);
    if (!applySelectedCloneVoiceToSynthesis(&synthesisSettings)) return;
    m_runner->startAudioGeneration(m_project.segments, m_project.projectPath, synthesisSettings);
}

void DubbingController::cancelProcessing()
{
    if (m_mediaQueueProcessing) {
        cancelMediaQueue();
        return;
    }
    const bool wasAutomatic = m_workflowMode == QStringLiteral("automatic")
        || m_automaticSetupActive;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticDownloadKeys.clear();
    m_automaticConfiguredNodes.clear();
    m_automaticSetupNodeId.clear();
    m_pendingExportPath.clear();
    if (m_translationFix) m_translationFix->cancel();
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    m_runner->cancel();
    if (wasAutomatic) {
        setWorkflowMode(QStringLiteral("idle"));
        setAutomaticStatus(QStringLiteral("Automatic generation stopped"));
        appendAutomaticEvent(QStringLiteral("Automatic generation stopped"),
                             QStringLiteral("stopped"), currentStepId());
    }
    emit processingChanged();
    emit workflowChanged();
}

bool DubbingController::fixTranslations(const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return false;
    clearError();
    return m_translationFix->start(
        m_project.sourceLanguage, m_project.targetLanguage,
        m_project.segments, configuration);
}

bool DubbingController::fixTranslationSegment(
    int index, const QVariantMap &configuration)
{
    if (!m_translationFix || processing()
        || index < 0 || index >= m_project.segments.size())
        return false;
    clearError();
    return m_translationFix->start(
        m_project.sourceLanguage, m_project.targetLanguage,
        m_project.segments, configuration, index);
}

bool DubbingController::translationSegmentNeedsFix(int index) const
{
    if (index < 0 || index >= m_project.segments.size()) return false;
    return DubbingTranslationFixService::eligibleSegmentCount(
               {m_project.segments.at(index)}, m_project.targetLanguage)
        == 1;
}

void DubbingController::testTranslationFixConnection(
    const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return;
    m_translationFix->testConnection(configuration);
}

QVariantList DubbingController::translationFixCliModelOptions(
    const QString &cliAgent) const
{
    return DubbingTranslationFixService::cliModelOptions(cliAgent);
}

void DubbingController::cancelTranslationFix()
{
    if (m_translationFix) m_translationFix->cancel();
}

void DubbingController::setAdaptiveConfiguration(const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return;
    const QString previousProvider = m_translationFix->configuration().value(
        QStringLiteral("provider")).toString();
    QVariantMap next = configuration;
    next.insert(QStringLiteral("configured"), true);
    m_translationFix->setConfiguration(next);
    if (previousProvider == QStringLiteral("local")
        && m_translationFix->configuration().value(QStringLiteral("provider")).toString()
               != QStringLiteral("local")) {
        if (AppController *app = AppController::instance(); app && app->sessionRegistry()) {
            if (IModelSession *session = app->sessionRegistry()->sessionForCapability(
                    QStringLiteral("llm-chat"))) {
                for (const SessionConfiguration &loaded : session->loadedConfigurations())
                    session->requestUnloadConfiguration(loaded.signature);
            }
        }
    }
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.customRewriteConfiguration = m_translationFix->configuration();
    if (m_runner) m_runner->setTranslationFixConfiguration(
        m_translationFix->configuration());
    persistAfterEdit();
    emit workflowChanged();
}

bool DubbingController::exportMedia(const QString &path)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (outputPath.isEmpty()) {
        setError(QStringLiteral("Choose an output path."));
        return false;
    }
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before exporting."));
        return false;
    }
    if (previewPath().isEmpty() || !QFileInfo::exists(previewPath())) {
        m_pendingExportPath = outputPath;
        if (!renderPreview()) {
            m_pendingExportPath.clear();
            return false;
        }
        return true;
    }
    return m_runner->startExport(m_project.sourceMediaPath, previewPath(), outputPath,
                                 m_project.segments, subtitleConfiguration());
}

bool DubbingController::exportAudioStem(const QString &stem, const QString &path)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (outputPath.isEmpty()) {
        setError(QStringLiteral("Choose an audio output path."));
        return false;
    }

    QString sourcePath;
    if (stem == QStringLiteral("mix")) sourcePath = previewPath();
    else if (stem == QStringLiteral("vocal")) sourcePath = dubbedVocalPath();
    else if (stem == QStringLiteral("background")) sourcePath = m_project.backgroundAudioPath;
    else {
        setError(QStringLiteral("Unknown audio stem: %1").arg(stem));
        return false;
    }
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        setError(QStringLiteral("The %1 audio stem is not available yet.").arg(stem));
        return false;
    }
    QString error;
    if (!replaceCopy(sourcePath, outputPath, &error)) {
        setError(error);
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::exportSubtitles(const QString &path, bool useTargetText)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (path.isEmpty() || outputPath.isEmpty()) {
        setError(QStringLiteral("Choose a subtitle output path."));
        return false;
    }
    QString error;
    if (!writeDubbingSubtitles(m_project.segments, outputPath, useTargetText, &error)) {
        setError(error);
        return false;
    }
    clearError();
    return true;
}

