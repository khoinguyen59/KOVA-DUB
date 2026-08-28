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
        m_project.projectPath = PathUtils::dataDir() + QStringLiteral("/dubbing/untitled.ladub.json");
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
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(m_project.sourceMediaPath.isEmpty() ? QStringLiteral("import") : QStringLiteral("ingest"));
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    m_workflowReviewStore.reset();
    m_activeReviewId.clear();
    m_workflowReviewRequest.clear();
    m_workflowJournal = std::make_unique<WorkflowRunJournal>(
        QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    m_workflowRunner->setJournal(m_workflowJournal.get());
    discoverInterruptedWorkflow();
    
    // Sync paths to runner
    m_runner->setPreviewPath(QFileInfo(m_project.projectPath).absolutePath() + QStringLiteral("/preview.wav"));
    m_runner->setExportPath(QString());
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

bool DubbingController::saveProject()
{
    if (!ensureProject(QString())) return false;
    // Route/model choices are a project contract for every Dubbing quality.
    // Clearing standard-mode selections here was the reason a reopened project
    // could revive an old Local setup instead of the confirmed Colab route.
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
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
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/history");
    QDir().mkpath(base);
    return base + QStringLiteral("/dubbing_history.json");
}

QString DubbingController::defaultProjectsDirectory() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidate = QDir(appDir).filePath(QStringLiteral("projects"));
    QDir().mkpath(candidate);
    return candidate;
}

bool DubbingController::createAutoProject(const QString &customName)
{
    const QString dir = defaultProjectsDirectory();
    QString baseName = customName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Project_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    }
    const QString targetPath = QDir(dir).filePath(baseName + QStringLiteral(".ladub.json"));
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

