void DubbingController::setSourceLanguage(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == m_project.sourceLanguage) return;
    m_project.sourceLanguage = normalized;
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setTargetLanguage(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == m_project.targetLanguage) return;
    m_project.targetLanguage = normalized;

    QVariantMap synthesis = m_workflowNodeConfigurations
                                .value(QStringLiteral("synthesize")).toMap();
    if (!synthesis.isEmpty()) {
        QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
        parameters.insert(QStringLiteral("lang"), normalized);
        synthesis.insert(QStringLiteral("parameters"), parameters);
        m_workflowNodeConfigurations.insert(QStringLiteral("synthesize"), synthesis);
    }

    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setDurationControl(const QVariantMap &value)
{
    m_project.durationControl = value;
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setDubbingQuality(const QString &value)
{
    const QString requested = value.trimmed().toLower();
    const QString normalized =
        requested == QStringLiteral("adaptive") || requested == QStringLiteral("custom")
        ? requested : QStringLiteral("fast");
    if (normalized == m_project.dubbingQuality) return;
    m_project.dubbingQuality = normalized;
    if (normalized != QStringLiteral("custom")) {
        m_workflowNodeConfigurations.clear();
        m_project.workflowNodeConfigurations.clear();
        resetStandardTranslationFixConfiguration();
    } else if (m_translationFix
               && !m_project.customRewriteConfiguration.isEmpty()) {
        m_translationFix->setConfiguration(m_project.customRewriteConfiguration);
        if (m_runner)
            m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
    }
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

bool DubbingController::ensureProject(const QString &path)
{
    if (!path.trimmed().isEmpty()) {
        m_project.projectPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    }
    if (m_project.projectPath.isEmpty()) {
        setError(QStringLiteral("Choose a project file before saving."));
        return false;
    }
    return true;
}

bool DubbingController::newProject(const QString &path)
{
    m_project = DubbingProject();
    m_project.subtitleConfiguration = {{QStringLiteral("source"), QStringLiteral("segments")},
                                       {QStringLiteral("textSource"), QStringLiteral("target")},
                                       {QStringLiteral("burnIn"), false},
                                       {QStringLiteral("style"), DubbingSubtitleService::defaultStyle()}};
    m_project.timingConfiguration = {{QStringLiteral("mode"), QStringLiteral("keep")},
                                     {QStringLiteral("minimumGapMs"), 80}};
    m_project.transcriptConfiguration = {{QStringLiteral("transcriptSource"), QStringLiteral("stt")},
                                         {QStringLiteral("fusionPolicy"), QStringLiteral("prefer-ocr")}};
    m_project.audioMixConfiguration = {{QStringLiteral("originalGainPercent"), 0},
                                       {QStringLiteral("dubbedGainPercent"), 100},
                                       {QStringLiteral("backgroundGainPercent"), 100}};
    m_timingResolutionPreview.clear();
    m_timingUndoSegments.clear();
    m_workflowNodeConfigurations.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("import"));
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    m_workflowReviewStore.reset();
    m_activeReviewId.clear();
    m_workflowReviewRequest.clear();
    m_workflowRecovery.clear();
    if (!path.isEmpty()) {
        if (!ensureProject(path)) return false;
    } else {
        // The app owns project identity. Never reuse one fixed path for an
        // unnamed project: a second project must not overwrite the first one.
        const QString dir = defaultProjectsDirectory();
        const QString baseName = QStringLiteral("Project_%1").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
        QString targetPath = QDir(dir).filePath(baseName + QStringLiteral(".ladub.json"));
        int suffix = 2;
        while (QFileInfo::exists(targetPath)) {
            targetPath = QDir(dir).filePath(
                QStringLiteral("%1__%2.ladub.json").arg(baseName).arg(suffix++));
        }
        m_project.projectPath = targetPath;
    }
    m_project.speakers.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("speaker-1")},
                                          {QStringLiteral("name"), QStringLiteral("Speaker 1")},
                                          {QStringLiteral("voice"), QVariantMap()} });
    requestSourceThumbnail();
    emit projectChanged();
    emit segmentsChanged();
    refreshCloneVoicePresets();
    emit cloneVoiceSelectionChanged();
    emit workflowChanged();
    return saveProject();
}

bool DubbingController::openProject(const QString &path)
{
    DubbingProject loaded;
    QString error;
    if (!DubbingProject::load(PathUtils::urlToLocalPath(path), loaded, &error)) {
        setError(error);
        return false;
    }
    m_project = std::move(loaded);
    applyStoredSubtitleOcrConfiguration();
    if (m_project.subtitleConfiguration.isEmpty()) {
        m_project.subtitleConfiguration = {{QStringLiteral("source"), QStringLiteral("segments")},
                                           {QStringLiteral("textSource"), QStringLiteral("target")},
                                           {QStringLiteral("burnIn"), false},
                                           {QStringLiteral("style"), DubbingSubtitleService::defaultStyle()}};
    }
    if (m_project.timingConfiguration.isEmpty()) {
        m_project.timingConfiguration = {{QStringLiteral("mode"), QStringLiteral("keep")},
                                         {QStringLiteral("minimumGapMs"), 80}};
    }
    if (m_project.transcriptConfiguration.isEmpty()) {
        m_project.transcriptConfiguration = {{QStringLiteral("transcriptSource"), QStringLiteral("stt")},
                                             {QStringLiteral("fusionPolicy"), QStringLiteral("prefer-ocr")}};
    } else if (!m_project.transcriptConfiguration.contains(QStringLiteral("fusionPolicy"))) {
        m_project.transcriptConfiguration.insert(QStringLiteral("fusionPolicy"), QStringLiteral("prefer-ocr"));
    }
    if (m_project.audioMixConfiguration.isEmpty()) {
        m_project.audioMixConfiguration = {{QStringLiteral("originalGainPercent"), 0},
                                           {QStringLiteral("dubbedGainPercent"), 100},
                                           {QStringLiteral("backgroundGainPercent"), 100}};
    }
    m_timingResolutionPreview.clear();
    m_timingUndoSegments.clear();
    m_workflowNodeConfigurations = m_project.workflowNodeConfigurations;
    if (m_translationFix && !m_project.customRewriteConfiguration.isEmpty()) {
        // The field predates Adaptive mode, but it is the versioned project
        // home for the non-secret rewrite-route selection in every mode.
        QVariantMap savedRewrite = m_project.customRewriteConfiguration;
        if (!savedRewrite.contains(QStringLiteral("apiKey"))) {
            savedRewrite.insert(QStringLiteral("apiKey"),
                                m_translationFix->configuration().value(QStringLiteral("apiKey")));
        }
        if (!savedRewrite.contains(QStringLiteral("serverUrl"))) {
            savedRewrite.insert(QStringLiteral("serverUrl"),
                                m_translationFix->configuration().value(QStringLiteral("serverUrl")));
        }
        m_translationFix->setConfiguration(savedRewrite);
        if (m_runner)
            m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
    } else if (m_project.dubbingQuality != QStringLiteral("custom")) {
        resetStandardTranslationFixConfiguration();
    }
    if (m_project.ttsVoiceId.trimmed().isEmpty()) {
        const QVariantMap synthesis = m_workflowNodeConfigurations.value(
            QStringLiteral("synthesize")).toMap();
        const QString modelId = synthesis.value(QStringLiteral("parameters")).toMap()
            .value(QStringLiteral("modelId"), automaticDefaultFamilyId(
                QStringLiteral("tts"), m_project.dubbingQuality)).toString();
        const QString builtInVoice = DubbingColabModelRoutes::defaultVoiceForTtsModel(modelId);
        if (!builtInVoice.isEmpty()) {
            // Migration/default only: persist it on the next project save just
            // like a normal selection, without replacing an existing saved ID.
            m_project.ttsVoiceId = QStringLiteral("builtin:") + builtInVoice;
            m_project.cloneVoicePresetId = m_project.ttsVoiceId;
        }
    }
    m_stepOutputs = m_project.workflowStepOutputs;
    m_lastCompletedStepId = m_project.workflowLastCompletedStepId;
    setWorkflowMode(QStringLiteral("idle"));
    static const QSet<QString> persistedSteps{
        QStringLiteral("import"), QStringLiteral("media-input"),
        QStringLiteral("ingest"), QStringLiteral("source-separate"),
        QStringLiteral("transcribe"), QStringLiteral("translate"),
        QStringLiteral("synthesize"), QStringLiteral("fit-timing"),
        QStringLiteral("mix"), QStringLiteral("export"),
        QStringLiteral("completed")};
    const QString savedStep = m_project.workflowCurrentStepId.trimmed().toLower();
    const QString resumeStep = persistedSteps.contains(savedStep)
        ? savedStep
        : (m_project.sourceMediaPath.isEmpty() ? QStringLiteral("import") : QStringLiteral("ingest"));
    setCurrentStep(resumeStep);
    m_pendingExportPath.clear();
    m_capCutDraftPath = m_project.capCutDraftPath;
    m_capCutDraftWarning.clear();
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    m_workflowReviewStore.reset();
    m_activeReviewId.clear();
    m_workflowReviewRequest.clear();
    m_workflowJournal = std::make_unique<WorkflowRunJournal>(
        QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    m_workflowRunner->setJournal(m_workflowJournal.get());
    discoverInterruptedWorkflow();
    
    // Restore durable output paths instead of replacing them with a guessed
    // preview path or clearing the export path. For schema <= 15, recover the
    // old conventional preview file only when it actually exists; never show
    // a fabricated path as a completed artifact.
    auto outputPath = [this](const QString &stepId,
                             const QStringList &keys) -> QString {
        const QVariantMap output = m_stepOutputs.value(stepId).toMap();
        for (const QString &key : keys) {
            const QString path = output.value(key).toString().trimmed();
            if (!path.isEmpty()) return path;
        }
        return {};
    };
    QString previewPath = m_project.previewAudioPath.trimmed();
    if (previewPath.isEmpty())
        previewPath = outputPath(QStringLiteral("mix"),
                                 {QStringLiteral("audio"), QStringLiteral("path")});
    // A sibling `preview.wav` cannot be attributed safely when several
    // projects share one directory.  Preserve the legacy migration only for
    // a directory with exactly one LA Studio project; all new previews are
    // project-scoped and persisted above.
    if (previewPath.isEmpty()) {
        const QDir projectDirectory(QFileInfo(m_project.projectPath).absolutePath());
        const QStringList projects = projectDirectory.entryList(
            {QStringLiteral("*.ladub.json")}, QDir::Files | QDir::NoSymLinks);
        const QString legacyPreview = projectDirectory.filePath(QStringLiteral("preview.wav"));
        if (projects.size() == 1 && QFileInfo(legacyPreview).isFile())
            previewPath = legacyPreview;
    }
    QString dubbedVocalPath = m_project.dubbedVocalAudioPath.trimmed();
    if (dubbedVocalPath.isEmpty())
        dubbedVocalPath = outputPath(QStringLiteral("fit-timing"),
                                     {QStringLiteral("audio"), QStringLiteral("path")});
    if (dubbedVocalPath.isEmpty())
        dubbedVocalPath = outputPath(QStringLiteral("synthesize"),
                                     {QStringLiteral("audio"), QStringLiteral("path")});
    QString exportPath = m_project.exportMediaPath.trimmed();
    if (exportPath.isEmpty())
        exportPath = outputPath(QStringLiteral("export"),
                                {QStringLiteral("media"), QStringLiteral("path")});
    m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
    m_runner->setSourceVocalsAudioPath(m_project.vocalsAudioPath);
    m_runner->setPreviewPath(previewPath);
    m_runner->setDubbedVocalPath(dubbedVocalPath);
    m_runner->setExportPath(exportPath);
    requestSourceThumbnail();
    
    emit projectChanged();
    emit segmentsChanged();
    refreshCloneVoicePresets();
    emit cloneVoiceSelectionChanged();
    emit workflowChanged();
    return true;
}

void DubbingController::discoverInterruptedWorkflow()
{
    m_workflowRecovery.clear();
    if (!m_workflowJournal) return;
    QString error;
    const QList<WorkflowInterruptedRun> runs = m_workflowJournal->interruptedRuns(&error);
    if (!error.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Cannot inspect interrupted workflow runs: %1").arg(error));
        return;
    }
    if (runs.isEmpty()) return;
    const WorkflowInterruptedRun &run = runs.constFirst();
    m_workflowRecovery = {{QStringLiteral("runId"), run.runId},
                          {QStringLiteral("workflowId"), run.workflowId},
                          {QStringLiteral("workflowVersion"), run.workflowVersion},
                          {QStringLiteral("activeNodeId"), run.activeNodeId},
                          {QStringLiteral("lastEvent"), run.lastEventType},
                          {QStringLiteral("lastUpdated"), run.lastUpdated}};
}

void DubbingController::syncProjectStateForPersistence()
{
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    m_project.workflowCurrentStepId = m_currentStepId;
    m_project.workflowLastCompletedStepId = m_lastCompletedStepId;
    m_project.workflowStepOutputs = m_stepOutputs;
    m_project.capCutDraftPath = m_capCutDraftPath;
    if (m_runner) {
        m_project.previewAudioPath = m_runner->previewPath();
        m_project.dubbedVocalAudioPath = m_runner->dubbedVocalPath();
        m_project.exportMediaPath = m_runner->exportPath();
    }
}

bool DubbingController::persistWorkflowTranscriptArtifact(const QString &nodeId,
                                                          const QVariantList &segments,
                                                          bool useTargetText,
                                                          QString *path)
{
    if (segments.isEmpty() || m_project.projectPath.trimmed().isEmpty()) return false;

    QString artifactId = nodeId.trimmed().toLower();
    QString fileName;
    if (artifactId == QStringLiteral("stt") || artifactId == QStringLiteral("transcribe")) {
        artifactId = QStringLiteral("transcribe");
        fileName = QStringLiteral("transcript.srt");
    } else if (artifactId == QStringLiteral("ocr")
               || artifactId == QStringLiteral("subtitle-ocr")) {
        artifactId = QStringLiteral("subtitle-ocr");
        fileName = QStringLiteral("ocr.srt");
    } else if (artifactId == QStringLiteral("review")
               || artifactId == QStringLiteral("review-transcript")) {
        artifactId = QStringLiteral("review-transcript");
        fileName = QStringLiteral("reviewed-transcript.srt");
    } else if (artifactId == QStringLiteral("translate")) {
        fileName = QStringLiteral("translated.srt");
    } else {
        return false;
    }

    const QString artifactDirectory = dubbingArtifactStageDirectory(m_project.projectPath, artifactId);
    if (!QDir().mkpath(artifactDirectory)) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Cannot create project transcript artifact directory: %1")
                            .arg(artifactDirectory));
        return false;
    }

    const QString artifactPath = QDir(artifactDirectory).filePath(fileName);
    QString error;
    if (!writeDubbingSubtitles(segments, artifactPath, useTargetText, &error)) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Cannot persist %1 transcript artifact: %2")
                            .arg(artifactId, error));
        return false;
    }
    if (path) *path = artifactPath;
    return true;
}

bool DubbingController::saveProject()
{
    if (!ensureProject(QString())) return false;
    // Route/model choices are a project contract for every Dubbing quality.
    // Clearing standard-mode selections here was the reason a reopened project
    // could revive an old Local setup instead of the confirmed Colab route.
    syncProjectStateForPersistence();
    QVariantMap persistedRewrite = translationFixConfiguration();
    // API credentials belong only to the secure settings store. Direct Colab
    // URL/token never enter this map in the first place.
    persistedRewrite.remove(QStringLiteral("apiKey"));
    persistedRewrite.remove(QStringLiteral("serverUrl"));
    m_project.customRewriteConfiguration = persistedRewrite;
    QString error;
    if (!m_project.save(&error)) {
        setError(error);
        return false;
    }
    recordHistoryEntry();
    return true;
}

bool DubbingController::saveProjectAs(const QString &path)
{
    const QString localPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (localPath.isEmpty()) {
        setError(QStringLiteral("Choose a project file before saving."));
        return false;
    }
    const QString previousPath = m_project.projectPath;
    m_project.projectPath = localPath;
    if (saveProject()) {
        requestSourceThumbnail();
        emit projectChanged();
        return true;
    }
    // DubbingProject uses QSaveFile, so this restores the previous durable
    // project identity after a failed atomic save without corrupting it.
    m_project.projectPath = previousPath;
    emit projectChanged();
    return false;
}

QString DubbingController::historyPath() const
{
    // Project history is user data. Keep it under the same overridable data
    // root as projects so smoke/acceptance profiles cannot pollute a real
    // user's recent-project list.
    const QString base = QDir(PathUtils::dataDir()).filePath(QStringLiteral("history"));
    QDir().mkpath(base);
    return base + QStringLiteral("/dubbing_history.json");
}

QString DubbingController::defaultProjectsDirectory() const
{
    if (!qEnvironmentVariable("LASTUDIO_DATA_DIR").trimmed().isEmpty()) {
        const QString isolated = QDir(PathUtils::dataDir()).filePath(QStringLiteral("projects"));
        if (QDir().mkpath(isolated)) return isolated;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidate = QDir(appDir).filePath(QStringLiteral("projects"));
    // Portable installs may be read-only (for example under Program Files).
    // Keep the convenient beside-executable location when it is writable, but
    // never silently lose an auto-created project because that directory could
    // not be written.
    if (QDir().mkpath(candidate) && QFileInfo(candidate).isWritable())
        return candidate;

    const QString fallback = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("projects"));
    if (QDir().mkpath(fallback))
        return fallback;
    return candidate;
}

bool DubbingController::createAutoProject(const QString &customName)
{
    const QString dir = defaultProjectsDirectory();
    QString baseName = customName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Project_%1").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    } else {
        // The app owns project identity. Normalize user input to one safe file
        // stem, then allocate a collision-free path instead of overwriting a
        // previous project that happened to use the same display name.
        if (baseName.endsWith(QStringLiteral(".ladub.json"), Qt::CaseInsensitive)) {
            baseName.chop(QStringLiteral(".ladub.json").size());
        } else if (baseName.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            baseName.chop(QStringLiteral(".json").size());
        }
        baseName.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")),
                         QStringLiteral("_"));
        baseName = baseName.trimmed();
        if (baseName.isEmpty())
            baseName = QStringLiteral("Project");
    }

    QString targetPath = QDir(dir).filePath(baseName + QStringLiteral(".ladub.json"));
    int suffix = 2;
    while (QFileInfo::exists(targetPath)) {
        targetPath = QDir(dir).filePath(
            QStringLiteral("%1__%2.ladub.json").arg(baseName).arg(suffix++));
    }
    return newProject(targetPath);
}

void DubbingController::refreshHistory()
{
    loadHistory();
}

void DubbingController::loadHistory()
{
    QVariantList historyItems;
    QSet<QString> seenPaths;

    // First load from history json
    QFile file(historyPath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isArray()) {
            const QVariantList arr = document.array().toVariantList();
            for (const QVariant &item : arr) {
                const QString path = item.toMap().value(QStringLiteral("projectPath")).toString();
                if (!path.isEmpty() && QFile::exists(path) && !seenPaths.contains(path)) {
                    seenPaths.insert(path);
                    historyItems.append(item);
                }
            }
        }
    }

    // Also scan default projects directory
    const QString defaultDir = defaultProjectsDirectory();
    QDir dir(defaultDir);
    const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.ladub.json"), QDir::Files, QDir::Time);
    for (const QFileInfo &info : entries) {
        const QString absPath = info.absoluteFilePath();
        if (!seenPaths.contains(absPath)) {
            seenPaths.insert(absPath);
            DubbingProject project;
            QString err;
            if (DubbingProject::load(absPath, project, &err)) {
                historyItems.append(QVariantMap{
                    {QStringLiteral("id"), absPath},
                    {QStringLiteral("projectPath"), absPath},
                    {QStringLiteral("projectName"), info.completeBaseName()},
                    {QStringLiteral("sourceMediaPath"), project.sourceMediaPath},
                    {QStringLiteral("sourceName"), QFileInfo(project.sourceMediaPath).fileName()},
                    {QStringLiteral("sourceLanguage"), project.sourceLanguage},
                    {QStringLiteral("targetLanguage"), project.targetLanguage},
                    {QStringLiteral("segmentCount"), project.segments.size()},
                    {QStringLiteral("timestamp"), info.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))}
                });
            }
        }
    }

    m_history = std::move(historyItems);
    emit historyChanged();
}

void DubbingController::recordHistoryEntry()
{
    if (m_project.projectPath.isEmpty()) return;
    const QFileInfo projectInfo(m_project.projectPath);
    QVariantList updated;
    for (const QVariant &value : std::as_const(m_history)) {
        if (value.toMap().value(QStringLiteral("projectPath")).toString()
            != projectInfo.absoluteFilePath())
            updated.append(value);
    }
    updated.prepend(QVariantMap{
        {QStringLiteral("id"), projectInfo.absoluteFilePath()},
        {QStringLiteral("projectPath"), projectInfo.absoluteFilePath()},
        {QStringLiteral("projectName"), projectInfo.completeBaseName()},
        {QStringLiteral("sourceMediaPath"), m_project.sourceMediaPath},
        {QStringLiteral("sourceName"), QFileInfo(m_project.sourceMediaPath).fileName()},
        {QStringLiteral("sourceLanguage"), m_project.sourceLanguage},
        {QStringLiteral("targetLanguage"), m_project.targetLanguage},
        {QStringLiteral("segmentCount"), m_project.segments.size()},
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))}
    });
    while (updated.size() > 30) updated.removeLast();
    QSaveFile file(historyPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument::fromVariant(updated).toJson());
        if (file.commit()) {
            m_history = std::move(updated);
            emit historyChanged();
        }
    }
}

bool DubbingController::deleteHistoryItem(const QString &id)
{
    if (id.isEmpty()) return false;
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history.at(i).toMap().value(QStringLiteral("id")).toString() != id) continue;
        QVariantList updated = m_history;
        updated.removeAt(i);
        QSaveFile file(historyPath());
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument::fromVariant(updated).toJson());
        if (!file.commit()) return false;
        m_history = std::move(updated);
        emit historyChanged();
        return true;
    }
    return false;
}

void DubbingController::clearHistory()
{
    m_history.clear();
    QFile::remove(historyPath());
    emit historyChanged();
}

void DubbingController::closeProject()
{
    m_project = DubbingProject();
    m_workflowNodeConfigurations.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("import"));
    m_runner->cancel();
    requestSourceThumbnail();
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    emit projectChanged();
    emit segmentsChanged();
    refreshCloneVoicePresets();
    emit cloneVoiceSelectionChanged();
    emit workflowChanged();
}

bool DubbingController::importMedia(const QString &pathOrUrl)
{
    const QString input = pathOrUrl.trimmed();
    const QUrl suppliedUrl = QUrl::fromUserInput(input);
    if (suppliedUrl.isValid()
        && (suppliedUrl.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
            || suppliedUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)) {
        setError(QStringLiteral(
            "Public links must be downloaded by the dedicated Colab media worker. "
            "Or download the file yourself and add the local file."));
        return false;
    }
    const QString localPath = PathUtils::urlToLocalPath(input).trimmed();
    const QFileInfo fileInfo(localPath);
    const QString path = fileInfo.absoluteFilePath();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Import media requested: input=\"%1\", local=\"%2\", exists=%3, isFile=%4")
                     .arg(input, localPath)
                     .arg(fileInfo.exists() ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(fileInfo.isFile() ? QStringLiteral("true") : QStringLiteral("false")));
    if (localPath.isEmpty() || !fileInfo.exists() || !fileInfo.isFile()) {
        Logger::error(QStringLiteral("DubbingController"),
                      QStringLiteral("Media import rejected: resolved path does not point to a file: \"%1\"").arg(path));
        setError(QStringLiteral("Media file does not exist: %1").arg(path));
        return false;
    }
    // Import is a durable project operation.  Do not silently manufacture an
    // untitled project here: the application-level project gate must be the
    // first decision before a studio can change media, model routes or output
    // state.  This also keeps API/controller callers aligned with the QML.
    if (!hasProject()) {
        setError(QStringLiteral("Create or open an LA Studio project before adding media."));
        return false;
    }

    // Import is intentionally side-effect free: preview the selected media and
    // reset downstream artifacts. Normalization and source separation only run
    // after the user chooses automatic or step-by-step processing.
    m_project.sourceMediaPath = path;
    m_project.sourceHash.clear();
    m_project.masterAudioPath.clear();
    m_project.analysisAudioPath.clear();
    m_project.vocalsAudioPath.clear();
    m_project.backgroundAudioPath.clear();
    m_project.sourceDurationMs = 0;
    m_project.sourceSampleRate = 0;
    m_project.sourceChannels = 0;
    const QString suffix = fileInfo.suffix().toLower();
    m_project.sourceIsVideo = suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm") || suffix == QStringLiteral("avi");
    m_project.segments.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    m_runner->setBackgroundAudioPath(QString());
    m_runner->setSourceVocalsAudioPath(QString());
    m_runner->setPreviewPath(QString());
    m_runner->setExportPath(QString());
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("ingest"));
    requestSourceThumbnail();
    emit projectChanged();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

void DubbingController::beginDubbingEntry()
{
    // This is called whenever the Dubbing route is loaded.  Do not reset a
    // project here: reopening the tab must preserve its work and only require
    // the operator to choose/confirm how it will be operated.
    m_dubbingEntryGateActive = true;
    m_automaticPreflightFingerprint.clear();
    emit workflowChanged();
}

void DubbingController::reopenDubbingEntryGate()
{
    m_dubbingEntryGateActive = true;
    m_automaticPreflightFingerprint.clear();
    emit workflowChanged();
}

bool DubbingController::chooseDubbingEntryMode(const QString &mode)
{
    const QString selected = mode.trimmed().toLower();
    if (selected != QStringLiteral("automatic") && selected != QStringLiteral("step")) {
        setError(QStringLiteral("Choose Automatic or Step-by-step before using Dubbing."));
        return false;
    }

    // Persist only the operator's choice.  No workflow graph, media, segment,
    // subtitle, generated artifact, or node configuration is changed here.
    if (m_project.workflowEntryMode != selected) {
        m_project.workflowEntryMode = selected;
        persistAfterEdit();
        emit projectChanged();
    }
    m_dubbingEntryGateActive = false;
    m_automaticPreflightFingerprint.clear();
    clearError();
    emit workflowChanged();
    return true;
}

