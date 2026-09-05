int DubbingController::mediaQueueIndex(const QString &itemId) const
{
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        if (m_mediaQueueItems.at(index).toMap().value(QStringLiteral("id")).toString() == itemId)
            return index;
    }
    return -1;
}

void DubbingController::replaceMediaQueueItem(int index, const QVariantMap &item)
{
    if (index < 0 || index >= m_mediaQueueItems.size()) return;
    m_mediaQueueItems[index] = item;
    emit mediaQueueChanged();
}

bool DubbingController::mediaQueueDownloading() const
{
    if ((m_remoteMediaImport && m_remoteMediaImport->active())
        || !m_activeMediaQueueDownloadId.isEmpty()) {
        return true;
    }
    // Keep the queue busy across the event-loop handoff between two links.
    // Otherwise a user could start a processing batch in the tiny gap after
    // item N completes but before item N+1 has been started.
    for (const QVariant &value : m_mediaQueueItems) {
        const QString state = value.toMap().value(QStringLiteral("downloadState")).toString();
        if (state == QStringLiteral("queued") || state == QStringLiteral("downloading")) return true;
    }
    return false;
}

bool DubbingController::mediaDownloadCookieFileConfigured() const
{
    return m_remoteMediaImport && m_remoteMediaImport->hasCookieFilePath();
}

int DubbingController::mediaQueueProgress() const
{
    int queuedJobs = 0;
    int progressTotal = 0;
    for (const QVariant &value : m_mediaQueueItems) {
        const QVariantMap item = value.toMap();
        const QString state = item.value(QStringLiteral("processState")).toString();
        if (state != QStringLiteral("queued") && state != QStringLiteral("running")
            && state != QStringLiteral("completed") && state != QStringLiteral("failed")
            && state != QStringLiteral("cancelled")) {
            continue;
        }
        ++queuedJobs;
        if (state == QStringLiteral("completed") || state == QStringLiteral("failed")
            || state == QStringLiteral("cancelled")) {
            progressTotal += 100;
        } else {
            progressTotal += qBound(0, item.value(QStringLiteral("progress")).toInt(), 99);
        }
    }
    return queuedJobs > 0 ? qRound(static_cast<qreal>(progressTotal) / queuedJobs) : 0;
}

QVariantMap DubbingController::normalizedMediaQueueTasks(const QVariantMap &tasks, QString *error) const
{
    QVariantMap normalized;
    const QString operation = tasks.value(QStringLiteral("operation")).toString().trimmed().toLower();
    if (!operation.isEmpty()) {
        static const QSet<QString> supportedOperations{
            QStringLiteral("import"), QStringLiteral("isolate"),
            QStringLiteral("transcribe"), QStringLiteral("translate"),
            QStringLiteral("voice"), QStringLiteral("export")};
        if (!supportedOperations.contains(operation)) {
            if (error) *error = QStringLiteral("Choose a valid downloaded-media action.");
            return {};
        }
        normalized.insert(QStringLiteral("operation"), operation);
        // A selected action is intentionally one production action across the
        // selected files.  It must not silently enqueue later actions.
        normalized.insert(QStringLiteral("executionMode"), QStringLiteral("per-media"));
        normalized.insert(QStringLiteral("audioFormat"), QStringLiteral("wav"));
        return normalized;
    }
    const bool isolate = tasks.value(QStringLiteral("isolate")).toBool();
    const bool voice = tasks.value(QStringLiteral("voice")).toBool();
    const bool translate = tasks.value(QStringLiteral("translate")).toBool() || voice;
    const bool transcribe = tasks.value(QStringLiteral("transcribe")).toBool() || translate;
    if (!isolate && !transcribe && !translate && !voice) {
        if (error) *error = QStringLiteral("Choose at least one batch task: isolate, STT, translate, or voice.");
        return {};
    }
    normalized.insert(QStringLiteral("isolate"), isolate);
    normalized.insert(QStringLiteral("transcribe"), transcribe);
    normalized.insert(QStringLiteral("translate"), translate);
    normalized.insert(QStringLiteral("voice"), voice);
    const QString executionMode = tasks.value(QStringLiteral("executionMode"),
                                               QStringLiteral("per-media")).toString();
    if (executionMode != QStringLiteral("per-media")
        && executionMode != QStringLiteral("stage-by-stage")) {
        if (error) *error = QStringLiteral("Choose a valid batch execution order.");
        return {};
    }
    normalized.insert(QStringLiteral("executionMode"), executionMode);
    normalized.insert(QStringLiteral("audioFormat"), QStringLiteral("wav"));
    return normalized;
}

QStringList DubbingController::mediaQueueStagePlan() const
{
    const QString operation = m_mediaQueueTasks.value(QStringLiteral("operation")).toString();
    if (operation == QStringLiteral("import")) return {QStringLiteral("ingest")};
    if (operation == QStringLiteral("isolate")) return {QStringLiteral("source-separate")};
    if (operation == QStringLiteral("transcribe")) return {QStringLiteral("transcribe")};
    if (operation == QStringLiteral("translate")) return {QStringLiteral("translate")};
    if (operation == QStringLiteral("voice"))
        return {QStringLiteral("synthesize"), QStringLiteral("mix")};
    if (operation == QStringLiteral("export")) return {QStringLiteral("export")};

    QStringList stages{QStringLiteral("ingest")};
    if (m_mediaQueueTasks.value(QStringLiteral("isolate")).toBool())
        stages.append(QStringLiteral("source-separate"));
    if (m_mediaQueueTasks.value(QStringLiteral("transcribe")).toBool())
        stages.append(QStringLiteral("transcribe"));
    if (m_mediaQueueTasks.value(QStringLiteral("translate")).toBool())
        stages.append(QStringLiteral("translate"));
    if (m_mediaQueueTasks.value(QStringLiteral("voice")).toBool()) {
        stages.append(QStringLiteral("synthesize"));
        stages.append(QStringLiteral("mix"));
    }
    return stages;
}

bool DubbingController::mediaQueueOperationRequiresSavedProject() const
{
    const QString operation = m_mediaQueueTasks.value(QStringLiteral("operation")).toString();
    return !operation.isEmpty() && operation != QStringLiteral("import");
}

bool DubbingController::loadMediaQueueProject(const QVariantMap &item, DubbingProject *project,
                                               QString *error) const
{
    if (!project) return false;
    const QString operation = m_mediaQueueTasks.value(QStringLiteral("operation")).toString();
    const QVariantMap outputs = item.value(QStringLiteral("outputs")).toMap();
    const QString savedProjectPath = outputs.value(QStringLiteral("project")).toString();
    if (savedProjectPath.isEmpty() || !QFileInfo(savedProjectPath).isFile()) {
        if (error) {
            *error = QStringLiteral("Run Import/Normalize for this media before %1. Its saved project is unavailable.")
                         .arg(visibleStepForNode(operation));
        }
        return false;
    }
    if (!DubbingProject::load(savedProjectPath, *project, error)) return false;
    if (project->sourceMediaPath.isEmpty() || !QFileInfo(project->sourceMediaPath).isFile()) {
        if (error) {
            *error = QStringLiteral("The saved project for this media no longer has a usable source file. Run Import/Normalize again.");
        }
        return false;
    }
    // Keep a stable working project outside the public output folder.  The
    // final save is copied atomically back to that folder after this action.
    project->projectPath = newMediaQueueProject(item).projectPath;
    project->workflowNodeConfigurations = m_mediaQueueOriginalNodeConfigurations;
    return true;
}

int DubbingController::enqueueMediaLinks(const QString &urls)
{
    if (!m_remoteMediaImport) {
        setError(QStringLiteral("The local media downloader is unavailable."));
        return 0;
    }
    int added = 0;
    QStringList rejected;
    const QStringList candidates = extractedSharedMediaUrls(urls);
    for (const QString &candidate : candidates) {
        const QString source = candidate.trimmed();
        if (source.isEmpty()) continue;
        const QUrl url = QUrl::fromUserInput(source);
        const QString scheme = url.scheme().toLower();
        if (!url.isValid() || scheme != QStringLiteral("https") || url.host().isEmpty()
            || !url.userInfo().isEmpty()) {
            rejected.append(source.left(96));
            continue;
        }
        QVariantMap item;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.insert(QStringLiteral("id"), id);
        // sourceUrl is short-lived memory only. It is erased the moment the
        // downloader resolves the staged file and is never saved into a
        // project, settings, history record, or output manifest.
        item.insert(QStringLiteral("sourceUrl"), url.toString());
        item.insert(QStringLiteral("displayName"), url.host().isEmpty()
                    ? QStringLiteral("Queued media") : url.host());
        item.insert(QStringLiteral("localPath"), QString());
        item.insert(QStringLiteral("sourceMode"), QStringLiteral("local-download"));
        item.insert(QStringLiteral("downloadState"), QStringLiteral("queued"));
        item.insert(QStringLiteral("processState"), QStringLiteral("not-ready"));
        item.insert(QStringLiteral("status"), QStringLiteral("Waiting for local public-media download"));
        item.insert(QStringLiteral("selected"), false);
        item.insert(QStringLiteral("progress"), 0);
        item.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        m_mediaQueueItems.append(item);
        ++added;
    }
    if (added > 0) {
        m_mediaQueueStatus = QStringLiteral("%1 link(s) queued for local download").arg(added);
        emit mediaQueueChanged();
        startNextQueuedMediaDownload();
    }
    if (!rejected.isEmpty()) {
        setError(QStringLiteral("Only valid public HTTPS links can be queued. Rejected %1 line(s).").arg(rejected.size()));
    } else if (added > 0) {
        clearError();
    }
    return added;
}

bool DubbingController::setMediaDownloadCookieFile(const QString &path)
{
    if (!m_remoteMediaImport) {
        setError(QStringLiteral("The local media downloader is unavailable."));
        return false;
    }
    QString error;
    if (!m_remoteMediaImport->setCookieFilePath(PathUtils::urlToLocalPath(path), &error)) {
        setError(error.isEmpty() ? QStringLiteral("The selected Douyin cookie file cannot be used.") : error);
        return false;
    }
    m_mediaQueueStatus = QStringLiteral("A private temporary copy of the selected Douyin cookies will be used for the next public-page resolve only.");
    clearError();
    emit mediaQueueChanged();
    return true;
}

void DubbingController::clearMediaDownloadCookieFile()
{
    if (m_remoteMediaImport) m_remoteMediaImport->clearCookieFilePath();
    emit mediaQueueChanged();
}

int DubbingController::enqueueMediaFiles(const QVariantList &paths)
{
    int added = 0;
    for (const QVariant &value : paths) {
        const QString localPath = QFileInfo(PathUtils::urlToLocalPath(value.toString())).absoluteFilePath();
        if (!QFileInfo(localPath).isFile()) continue;
        QVariantMap item;
        item.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        item.insert(QStringLiteral("displayName"), QFileInfo(localPath).fileName());
        item.insert(QStringLiteral("localPath"), localPath);
        item.insert(QStringLiteral("sourceMode"), QStringLiteral("manual-upload"));
        item.insert(QStringLiteral("downloadState"), QStringLiteral("downloaded"));
        item.insert(QStringLiteral("processState"), QStringLiteral("ready"));
        item.insert(QStringLiteral("status"), QStringLiteral("Manual file ready for selected batch tasks"));
        item.insert(QStringLiteral("selected"), true);
        item.insert(QStringLiteral("progress"), 0);
        item.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        m_mediaQueueItems.append(item);
        ++added;
    }
    if (added > 0) {
        m_mediaQueueStatus = QStringLiteral("%1 local media file(s) ready for batch processing").arg(added);
        emit mediaQueueChanged();
        clearError();
    }
    return added;
}

bool DubbingController::setMediaQueueItemSelected(const QString &itemId, bool selected)
{
    const int index = mediaQueueIndex(itemId);
    if (index < 0) return false;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    if (item.value(QStringLiteral("downloadState")).toString() != QStringLiteral("downloaded")) return false;
    if (m_mediaQueueProcessing && item.value(QStringLiteral("processState")).toString() == QStringLiteral("running")) return false;
    item.insert(QStringLiteral("selected"), selected);
    replaceMediaQueueItem(index, item);
    return true;
}

bool DubbingController::retryMediaQueueItem(const QString &itemId)
{
    if (mediaQueueDownloading() || mediaQueueProcessing() || processing()) {
        setBusyError(QStringLiteral("Wait for the current download or batch operation to finish before retrying."));
        return false;
    }
    const int index = mediaQueueIndex(itemId);
    if (index < 0) return false;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    const QString state = item.value(QStringLiteral("downloadState")).toString();
    if (state != QStringLiteral("failed")) {
        setError(QStringLiteral("Only a failed download that still has its source link can be retried."));
        return false;
    }
    if (item.value(QStringLiteral("sourceUrl")).toString().trimmed().isEmpty()) {
        setError(QStringLiteral("The original link is no longer available. Add the link again to retry it."));
        return false;
    }
    item.insert(QStringLiteral("downloadState"), QStringLiteral("queued"));
    item.insert(QStringLiteral("processState"), QStringLiteral("not-ready"));
    item.insert(QStringLiteral("status"), QStringLiteral("Waiting to retry in the local downloader"));
    item.insert(QStringLiteral("selected"), false);
    item.insert(QStringLiteral("progress"), 0);
    replaceMediaQueueItem(index, item);
    m_mediaQueueStatus = QStringLiteral("Retrying queued media locally");
    clearError();
    startNextQueuedMediaDownload();
    return true;
}

bool DubbingController::removeMediaQueueItem(const QString &itemId)
{
    const int index = mediaQueueIndex(itemId);
    if (index < 0) return false;
    const QVariantMap item = m_mediaQueueItems.at(index).toMap();
    if (item.value(QStringLiteral("id")).toString() == m_activeMediaQueueDownloadId
        || item.value(QStringLiteral("id")).toString() == m_activeMediaQueueItemId) {
        setError(QStringLiteral("Cancel the active batch operation before removing that item."));
        return false;
    }
    m_mediaQueueItems.removeAt(index);
    emit mediaQueueChanged();
    return true;
}

void DubbingController::clearCompletedMediaQueue()
{
    if (mediaQueueDownloading() || m_mediaQueueProcessing) {
        setBusyError(QStringLiteral("Cancel or wait for the active batch operation before clearing completed items."));
        return;
    }
    QVariantList retained;
    for (const QVariant &value : std::as_const(m_mediaQueueItems)) {
        const QString state = value.toMap().value(QStringLiteral("processState")).toString();
        const QString downloadState = value.toMap().value(QStringLiteral("downloadState")).toString();
        if (state != QStringLiteral("completed") && state != QStringLiteral("failed")
            && state != QStringLiteral("cancelled")
            ) {
            retained.append(value);
        }
    }
    m_mediaQueueItems = retained;
    m_mediaQueueStatus = m_mediaQueueItems.isEmpty()
        ? QStringLiteral("Queue cleared") : QStringLiteral("Completed batch items cleared");
    emit mediaQueueChanged();
}

void DubbingController::startNextQueuedMediaDownload()
{
    if (m_mediaQueueCancelling) return;
    if (!m_remoteMediaImport || m_remoteMediaImport->active()
        || !m_activeMediaQueueDownloadId.isEmpty()) return;
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        if (item.value(QStringLiteral("downloadState")).toString() != QStringLiteral("queued")) continue;
        const QUrl url = QUrl::fromUserInput(item.value(QStringLiteral("sourceUrl")).toString());
        if (!url.isValid() || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
            || url.host().isEmpty() || !url.userInfo().isEmpty()) {
            item.insert(QStringLiteral("downloadState"), QStringLiteral("failed"));
            item.insert(QStringLiteral("processState"), QStringLiteral("not-ready"));
            item.insert(QStringLiteral("status"), QStringLiteral("Invalid queued media URL"));
            replaceMediaQueueItem(index, item);
            continue;
        }
        m_activeMediaQueueDownloadId = item.value(QStringLiteral("id")).toString();
        item.insert(QStringLiteral("downloadState"), QStringLiteral("downloading"));
        item.insert(QStringLiteral("status"), QStringLiteral("Starting local public-media download"));
        replaceMediaQueueItem(index, item);
        m_mediaQueueStatus = QStringLiteral("Downloading queued media %1 locally").arg(index + 1);
        emit mediaQueueChanged();
        if (!m_remoteMediaImport->download(url)) {
            onBatchMediaDownloadFinished(false, QString(),
                QStringLiteral("The queued media download could not be started."));
        }
        return;
    }
    if (!m_mediaQueueProcessing) {
        int downloaded = 0;
        int failed = 0;
        for (const QVariant &value : std::as_const(m_mediaQueueItems)) {
            const QString state = value.toMap().value(QStringLiteral("downloadState")).toString();
            if (state == QStringLiteral("downloaded")) ++downloaded;
            else if (state == QStringLiteral("failed")) ++failed;
        }
        m_mediaQueueStatus = failed > 0
            ? QStringLiteral("Download queue finished: %1 downloaded, %2 failed. Only downloaded items can be selected.")
                  .arg(downloaded).arg(failed)
            : QStringLiteral("All queued links have finished downloading");
        emit mediaQueueChanged();
    }
}

void DubbingController::onBatchMediaDownloadFinished(bool success, const QString &localPath,
                                                      const QString &error)
{
    const int index = mediaQueueIndex(m_activeMediaQueueDownloadId);
    const bool cancelled = m_mediaQueueCancelling;
    m_activeMediaQueueDownloadId.clear();
    // Cookies are a one-shot opt-in: never reuse them for a later queued URL.
    if (m_remoteMediaImport) m_remoteMediaImport->clearCookieFilePath();
    if (index >= 0) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        item.insert(QStringLiteral("receivedBytes"), 0);
        item.insert(QStringLiteral("totalBytes"), -1);
        if (cancelled) {
            item.remove(QStringLiteral("sourceUrl"));
            item.insert(QStringLiteral("downloadState"), QStringLiteral("cancelled"));
            item.insert(QStringLiteral("processState"), QStringLiteral("cancelled"));
            item.insert(QStringLiteral("status"), QStringLiteral("Download cancelled"));
            item.insert(QStringLiteral("selected"), false);
        } else if (success && QFileInfo(localPath).isFile()) {
            item.remove(QStringLiteral("sourceUrl"));
            item.insert(QStringLiteral("displayName"), QFileInfo(localPath).fileName());
            item.insert(QStringLiteral("localPath"), QFileInfo(localPath).absoluteFilePath());
            item.insert(QStringLiteral("downloadState"), QStringLiteral("downloaded"));
            item.insert(QStringLiteral("processState"), QStringLiteral("ready"));
            item.insert(QStringLiteral("status"), QStringLiteral("Downloaded — select for batch processing"));
            item.insert(QStringLiteral("selected"), true);
        } else {
            const QString safeError = error.trimmed().isEmpty()
                ? QStringLiteral("Media download failed") : error.trimmed();
            item.insert(QStringLiteral("downloadState"), QStringLiteral("failed"));
            item.insert(QStringLiteral("processState"), QStringLiteral("not-ready"));
            item.insert(QStringLiteral("status"), safeError);
            item.insert(QStringLiteral("selected"), false);
        }
        replaceMediaQueueItem(index, item);
    }
    m_mediaQueueStatus = cancelled ? QStringLiteral("Download queue cancelled") : (success
        ? QStringLiteral("Downloaded queued media")
        : QStringLiteral("Queued media download failed"));
    emit mediaQueueChanged();
    if (cancelled) {
        m_mediaQueueCancelling = false;
        emit mediaQueueChanged();
        return;
    }
    QTimer::singleShot(0, this, &DubbingController::startNextQueuedMediaDownload);
}

DubbingProject DubbingController::newMediaQueueProject(const QVariantMap &item) const
{
    DubbingProject project;
    const QString root = QDir(PathUtils::dataDir()).filePath(QStringLiteral("dubbing/batch-projects"));
    const QString base = QFileInfo(item.value(QStringLiteral("localPath")).toString()).completeBaseName()
        .replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    const QString safeBase = base.isEmpty() ? QStringLiteral("media") : base.left(80);
    project.projectPath = QDir(root).filePath(
        QStringLiteral("%1-%2.ladub.json").arg(safeBase, item.value(QStringLiteral("id")).toString().left(12)));
    project.sourceLanguage = m_mediaQueueOriginalProject.sourceLanguage;
    project.targetLanguage = m_mediaQueueOriginalProject.targetLanguage;
    project.dubbingQuality = m_mediaQueueOriginalProject.dubbingQuality;
    project.ttsVoiceId = m_mediaQueueOriginalProject.ttsVoiceId;
    project.cloneVoicePresetId = project.ttsVoiceId;
    project.durationControl = m_mediaQueueOriginalProject.durationControl;
    project.workflowNodeConfigurations = m_mediaQueueOriginalNodeConfigurations;
    project.transcriptConfiguration = m_mediaQueueOriginalProject.transcriptConfiguration;
    project.subtitleConfiguration = m_mediaQueueOriginalProject.subtitleConfiguration;
    project.timingConfiguration = m_mediaQueueOriginalProject.timingConfiguration;
    project.customRewriteConfiguration = m_mediaQueueOriginalProject.customRewriteConfiguration;
    project.speakers = m_mediaQueueOriginalProject.speakers;
    project.workflowEntryMode = QStringLiteral("step");
    return project;
}

QString DubbingController::mediaQueueOutputDirectory(const QString &itemId) const
{
    return QDir(PathUtils::dataDir()).filePath(QStringLiteral("dubbing/batch-output/%1").arg(itemId));
}

void DubbingController::recordMediaQueueOutput(const QString &key, const QString &path)
{
    const int index = mediaQueueIndex(m_activeMediaQueueItemId);
    if (index < 0) return;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    QVariantMap outputs = item.value(QStringLiteral("outputs")).toMap();
    outputs.insert(key, path);
    item.insert(QStringLiteral("outputs"), outputs);
    item.insert(QStringLiteral("outputDirectory"), mediaQueueOutputDirectory(m_activeMediaQueueItemId));
    replaceMediaQueueItem(index, item);
}

bool DubbingController::writeMediaQueueSubtitles(const QString &key, bool useTargetText)
{
    const QString fileName = useTargetText ? QStringLiteral("translated.srt") : QStringLiteral("source.srt");
    const QString outputPath = QDir(mediaQueueOutputDirectory(m_activeMediaQueueItemId)).filePath(fileName);
    QString error;
    if (!writeDubbingSubtitles(m_project.segments, outputPath, useTargetText, &error)) {
        completeCurrentMediaQueueItem(false, error.isEmpty()
            ? QStringLiteral("Could not write the batch subtitle output.") : error);
        return false;
    }
    recordMediaQueueOutput(key, outputPath);
    return true;
}

bool DubbingController::startMediaQueue(const QVariantMap &tasks)
{
    if (mediaQueueDownloading() || processing()) {
        setBusyError(QStringLiteral("Wait for active import, download, or Dubbing work before starting the media batch."));
        return false;
    }
    QString error;
    const QVariantMap normalizedTasks = normalizedMediaQueueTasks(tasks, &error);
    if (normalizedTasks.isEmpty()) {
        setError(error);
        return false;
    }
    const bool reuseSavedProject = normalizedTasks.contains(QStringLiteral("operation"))
        && normalizedTasks.value(QStringLiteral("operation")).toString() != QStringLiteral("import");
    int selectedReady = 0;
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        if (!item.value(QStringLiteral("selected")).toBool()
            || item.value(QStringLiteral("downloadState")).toString() != QStringLiteral("downloaded")
            || !QFileInfo(item.value(QStringLiteral("localPath")).toString()).isFile()) {
            continue;
        }
        item.insert(QStringLiteral("processState"), QStringLiteral("queued"));
        item.insert(QStringLiteral("status"), normalizedTasks.value(QStringLiteral("executionMode")).toString()
                    == QStringLiteral("stage-by-stage")
                    ? QStringLiteral("Waiting for stage-by-stage processing")
                    : QStringLiteral("Waiting for end-to-end processing"));
        item.insert(QStringLiteral("progress"), 0);
        item.insert(QStringLiteral("completedStages"), QVariantList{});
        item.insert(QStringLiteral("nextStageIndex"), 0);
        item.insert(QStringLiteral("executionMode"), normalizedTasks.value(QStringLiteral("executionMode")));
        // A later Translate/TTS/Export selection must keep the exact artifacts
        // and project written by an earlier selected action.  Clearing them
        // here would force the user to repeat work or choose every task up
        // front, which is precisely what the media library avoids.
        if (!reuseSavedProject) item.insert(QStringLiteral("outputs"), QVariantMap{});
        item.remove(QStringLiteral("error"));
        replaceMediaQueueItem(index, item);
        ++selectedReady;
    }
    if (selectedReady == 0) {
        setError(QStringLiteral("Select at least one successfully downloaded media item for the batch."));
        return false;
    }
    m_mediaQueueOriginalProject = m_project;
    m_mediaQueueOriginalNodeConfigurations = m_workflowNodeConfigurations;
    m_mediaQueueOriginalPreviewPath = m_runner->previewPath();
    m_mediaQueueOriginalExportPath = m_runner->exportPath();
    m_mediaQueueOriginalProjectCaptured = true;
    m_mediaQueueTasks = normalizedTasks;
    m_mediaQueueExecutionMode = normalizedTasks.value(QStringLiteral("executionMode")).toString();
    m_mediaQueueStagePlan = mediaQueueStagePlan();
    m_mediaQueueStagePlanIndex = 0;
    m_mediaQueueProjects.clear();
    m_mediaQueueProcessing = true;
    m_mediaQueueCancelling = false;
    m_mediaQueueStatus = QStringLiteral("Preparing %1 selected media item(s)").arg(selectedReady);
    clearError();
    emit mediaQueueChanged();
    emit processingChanged();
    QTimer::singleShot(0, this, &DubbingController::startNextMediaQueueItem);
    return true;
}

void DubbingController::startNextMediaQueueItem()
{
    if (!m_mediaQueueProcessing) return;
    if (m_mediaQueueCancelling) {
        finishMediaQueueRun(QStringLiteral("Batch cancelled"));
        return;
    }
    if (m_mediaQueueExecutionMode == QStringLiteral("stage-by-stage")) {
        startNextMediaQueueStageItem();
        return;
    }
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        if (item.value(QStringLiteral("processState")).toString() != QStringLiteral("queued")) continue;
        const QString localPath = item.value(QStringLiteral("localPath")).toString();
        if (!QFileInfo(localPath).isFile()) {
            item.insert(QStringLiteral("processState"), QStringLiteral("failed"));
            item.insert(QStringLiteral("status"), QStringLiteral("Downloaded staging file is no longer available"));
            item.insert(QStringLiteral("error"), item.value(QStringLiteral("status")));
            replaceMediaQueueItem(index, item);
            continue;
        }
        m_activeMediaQueueItemId = item.value(QStringLiteral("id")).toString();
        if (mediaQueueOperationRequiresSavedProject()) {
            DubbingProject restoredProject;
            QString projectError;
            if (!loadMediaQueueProject(item, &restoredProject, &projectError)) {
                item.insert(QStringLiteral("processState"), QStringLiteral("failed"));
                item.insert(QStringLiteral("stage"), QStringLiteral("failed"));
                item.insert(QStringLiteral("status"), projectError);
                item.insert(QStringLiteral("error"), projectError);
                item.insert(QStringLiteral("progress"), 100);
                replaceMediaQueueItem(index, item);
                m_activeMediaQueueItemId.clear();
                continue;
            }
            m_project = std::move(restoredProject);
        } else {
            m_project = newMediaQueueProject(item);
        }
        m_workflowNodeConfigurations = m_project.workflowNodeConfigurations;
        m_stepOutputs.clear();
        m_lastCompletedStepId.clear();
        m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
        m_runner->setSourceVocalsAudioPath(m_project.vocalsAudioPath);
        m_runner->setPreviewPath(QString());
        m_runner->setExportPath(QString());
        item.insert(QStringLiteral("processState"), QStringLiteral("running"));
        item.insert(QStringLiteral("status"), QStringLiteral("Starting import and media validation"));
        item.insert(QStringLiteral("stage"), QStringLiteral("ingest"));
        item.insert(QStringLiteral("progress"), 0);
        item.insert(QStringLiteral("outputDirectory"), mediaQueueOutputDirectory(m_activeMediaQueueItemId));
        replaceMediaQueueItem(index, item);
        if (!QDir().mkpath(mediaQueueOutputDirectory(m_activeMediaQueueItemId))) {
            completeCurrentMediaQueueItem(false, QStringLiteral("Cannot create the batch output directory."));
            return;
        }
        m_mediaQueueStatus = QStringLiteral("Processing %1").arg(item.value(QStringLiteral("displayName")).toString());
        emit projectChanged();
        emit workflowChanged();
        emit processingChanged();
        startMediaQueueStage(QStringLiteral("ingest"));
        return;
    }
    finishMediaQueueRun(QStringLiteral("Selected media batch finished"));
}

void DubbingController::startNextMediaQueueStageItem()
{
    if (!m_mediaQueueProcessing || m_mediaQueueExecutionMode != QStringLiteral("stage-by-stage")) return;
    if (m_mediaQueueCancelling) {
        finishMediaQueueRun(QStringLiteral("Batch cancelled"));
        return;
    }
    if (m_mediaQueueStagePlanIndex >= m_mediaQueueStagePlan.size()) {
        finishMediaQueueRun(QStringLiteral("Selected media batch finished"));
        return;
    }

    const QString stage = m_mediaQueueStagePlan.at(m_mediaQueueStagePlanIndex);
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        if (item.value(QStringLiteral("processState")).toString() != QStringLiteral("queued")
            || item.value(QStringLiteral("nextStageIndex")).toInt() != m_mediaQueueStagePlanIndex) {
            continue;
        }
        const QString localPath = item.value(QStringLiteral("localPath")).toString();
        if (!QFileInfo(localPath).isFile()) {
            item.insert(QStringLiteral("processState"), QStringLiteral("failed"));
            item.insert(QStringLiteral("status"), QStringLiteral("Downloaded staging file is no longer available"));
            item.insert(QStringLiteral("error"), item.value(QStringLiteral("status")));
            item.insert(QStringLiteral("progress"), 100);
            replaceMediaQueueItem(index, item);
            continue;
        }

        m_activeMediaQueueItemId = item.value(QStringLiteral("id")).toString();
        if (mediaQueueOperationRequiresSavedProject()) {
            DubbingProject restoredProject;
            QString projectError;
            if (!loadMediaQueueProject(item, &restoredProject, &projectError)) {
                item.insert(QStringLiteral("processState"), QStringLiteral("failed"));
                item.insert(QStringLiteral("stage"), QStringLiteral("failed"));
                item.insert(QStringLiteral("status"), projectError);
                item.insert(QStringLiteral("error"), projectError);
                item.insert(QStringLiteral("progress"), 100);
                replaceMediaQueueItem(index, item);
                m_activeMediaQueueItemId.clear();
                continue;
            }
            m_project = std::move(restoredProject);
        } else {
            m_project = m_mediaQueueProjects.contains(m_activeMediaQueueItemId)
                ? m_mediaQueueProjects.value(m_activeMediaQueueItemId) : newMediaQueueProject(item);
        }
        m_workflowNodeConfigurations = m_project.workflowNodeConfigurations;
        m_stepOutputs.clear();
        m_lastCompletedStepId.clear();
        m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
        m_runner->setSourceVocalsAudioPath(m_project.vocalsAudioPath);
        m_runner->setPreviewPath(QString());
        m_runner->setExportPath(QString());
        item.insert(QStringLiteral("processState"), QStringLiteral("running"));
        item.insert(QStringLiteral("stage"), stage);
        item.insert(QStringLiteral("status"), QStringLiteral("Starting %1 (stage %2/%3)")
                    .arg(visibleStepForNode(stage))
                    .arg(m_mediaQueueStagePlanIndex + 1)
                    .arg(m_mediaQueueStagePlan.size()));
        item.insert(QStringLiteral("progress"), qRound(100.0 * m_mediaQueueStagePlanIndex
                                                         / m_mediaQueueStagePlan.size()));
        item.insert(QStringLiteral("outputDirectory"), mediaQueueOutputDirectory(m_activeMediaQueueItemId));
        replaceMediaQueueItem(index, item);
        if (!QDir().mkpath(mediaQueueOutputDirectory(m_activeMediaQueueItemId))) {
            completeCurrentMediaQueueItem(false, QStringLiteral("Cannot create the batch output directory."));
            return;
        }
        m_mediaQueueStatus = QStringLiteral("Running %1 for %2 (stage %3/%4)")
            .arg(visibleStepForNode(stage), item.value(QStringLiteral("displayName")).toString())
            .arg(m_mediaQueueStagePlanIndex + 1)
            .arg(m_mediaQueueStagePlan.size());
        emit projectChanged();
        emit workflowChanged();
        emit processingChanged();
        startMediaQueueStage(stage);
        return;
    }

    ++m_mediaQueueStagePlanIndex;
    if (m_mediaQueueStagePlanIndex < m_mediaQueueStagePlan.size()) {
        m_mediaQueueStatus = QStringLiteral("Completed %1 for selected media; continuing with %2")
            .arg(visibleStepForNode(stage), visibleStepForNode(m_mediaQueueStagePlan.at(m_mediaQueueStagePlanIndex)));
        emit mediaQueueChanged();
        QTimer::singleShot(0, this, &DubbingController::startNextMediaQueueStageItem);
    } else {
        finishMediaQueueRun(QStringLiteral("Selected media batch finished"));
    }
}

void DubbingController::startMediaQueueStage(const QString &stage)
{
    if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
    const int index = mediaQueueIndex(m_activeMediaQueueItemId);
    if (index < 0) return;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    item.insert(QStringLiteral("stage"), stage);
    item.insert(QStringLiteral("status"), QStringLiteral("Running %1").arg(visibleStepForNode(stage)));
    const int stageIndex = m_mediaQueueStagePlan.indexOf(stage);
    item.insert(QStringLiteral("progress"), m_mediaQueueExecutionMode == QStringLiteral("stage-by-stage")
                && stageIndex >= 0 && !m_mediaQueueStagePlan.isEmpty()
                ? qRound(100.0 * stageIndex / m_mediaQueueStagePlan.size()) : 0);
    replaceMediaQueueItem(index, item);
    m_mediaQueueStage = stage;
    m_runner->clearError();
    emit mediaQueueChanged();
    emit processingChanged();

    if (stage == QStringLiteral("ingest")) {
        m_runner->startIngest(m_project.sourceMediaPath.isEmpty()
            ? item.value(QStringLiteral("localPath")).toString() : m_project.sourceMediaPath);
    } else if (stage == QStringLiteral("source-separate")) {
        const QString audioPath = m_project.masterAudioPath;
        if (audioPath.isEmpty()) {
            completeCurrentMediaQueueItem(false, QStringLiteral("Media validation did not produce a master audio path."));
            return;
        }
        m_runner->startSourceSeparation(audioPath,
            m_workflowNodeConfigurations.value(QStringLiteral("source-separate")).toMap());
    } else if (stage == QStringLiteral("transcribe")) {
        transcribeSource();
    } else if (stage == QStringLiteral("translate")) {
        translateSource();
    } else if (stage == QStringLiteral("synthesize")) {
        generateAudio();
    } else if (stage == QStringLiteral("mix")) {
        if (!m_runner->renderPreview(m_project.segments, m_project.projectPath)) {
            completeCurrentMediaQueueItem(false, QStringLiteral("Could not start WAV mix rendering for this batch item."));
            return;
        }
    } else if (stage == QStringLiteral("export")) {
        const QVariantMap outputs = item.value(QStringLiteral("outputs")).toMap();
        const QString renderedAudio = outputs.value(QStringLiteral("voiceWav")).toString();
        if (!QFileInfo(renderedAudio).isFile()) {
            completeCurrentMediaQueueItem(false,
                QStringLiteral("Run TTS for this media before Export/Output. Its generated voice WAV is unavailable."));
            return;
        }
        const QString suffix = m_project.sourceIsVideo ? QStringLiteral("mp4") : QStringLiteral("wav");
        const QString outputPath = QDir(mediaQueueOutputDirectory(m_activeMediaQueueItemId))
            .filePath(QStringLiteral("dubbed-output.%1").arg(suffix));
        if (!m_runner->startExport(m_project.sourceMediaPath, renderedAudio, outputPath,
                                   m_project.segments, subtitleConfiguration())) {
            completeCurrentMediaQueueItem(false,
                QStringLiteral("Could not start Export/Output for this batch item."));
            return;
        }
    }

    if (stage != QStringLiteral("mix") && stage != QStringLiteral("export") && !m_runner->processing()) {
        const QString message = m_runner->lastError().trimmed().isEmpty()
            ? QStringLiteral("The %1 batch stage did not start.").arg(visibleStepForNode(stage))
            : m_runner->lastError();
        completeCurrentMediaQueueItem(false, message);
    }
}

void DubbingController::updateMediaQueueProgressFromRunner()
{
    if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
    const int index = mediaQueueIndex(m_activeMediaQueueItemId);
    if (index < 0) return;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    const QStringList stages = m_mediaQueueStagePlan.isEmpty() ? mediaQueueStagePlan() : m_mediaQueueStagePlan;
    if (stages.isEmpty()) return;
    const int stageIndex = qMax(0, stages.indexOf(m_mediaQueueStage));
    const int runnerProgress = qBound(0, m_runner->progress(), 100);
    const int progress = qBound(0, qRound((stageIndex * 100.0 + runnerProgress) / stages.size()), 99);
    if (item.value(QStringLiteral("progress")).toInt() != progress) {
        item.insert(QStringLiteral("progress"), progress);
        replaceMediaQueueItem(index, item);
    }
}

void DubbingController::completeCurrentMediaQueueStage(const QString &stage)
{
    const int index = mediaQueueIndex(m_activeMediaQueueItemId);
    if (index < 0) return;
    const int completedStageIndex = m_mediaQueueStagePlan.indexOf(stage);
    if (completedStageIndex < 0 || completedStageIndex != m_mediaQueueStagePlanIndex) {
        completeCurrentMediaQueueItem(false, QStringLiteral("The batch stage order became inconsistent."));
        return;
    }

    m_mediaQueueProjects.insert(m_activeMediaQueueItemId, m_project);
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    const int nextStageIndex = completedStageIndex + 1;
    if (nextStageIndex >= m_mediaQueueStagePlan.size()) {
        completeCurrentMediaQueueItem(true);
        return;
    }

    item.insert(QStringLiteral("processState"), QStringLiteral("queued"));
    item.insert(QStringLiteral("nextStageIndex"), nextStageIndex);
    item.insert(QStringLiteral("stage"), QStringLiteral("waiting"));
    item.insert(QStringLiteral("progress"), qRound(100.0 * nextStageIndex / m_mediaQueueStagePlan.size()));
    item.insert(QStringLiteral("status"), QStringLiteral("Completed %1; waiting for %2 across the selected queue")
                .arg(visibleStepForNode(stage), visibleStepForNode(m_mediaQueueStagePlan.at(nextStageIndex))));
    replaceMediaQueueItem(index, item);
    m_activeMediaQueueItemId.clear();
    m_mediaQueueStage.clear();
    emit mediaQueueChanged();
    QTimer::singleShot(0, this, &DubbingController::startNextMediaQueueItem);
}

void DubbingController::completeCurrentMediaQueueItem(bool success, const QString &message)
{
    const int index = mediaQueueIndex(m_activeMediaQueueItemId);
    if (index < 0) return;
    QVariantMap item = m_mediaQueueItems.at(index).toMap();
    QString finalMessage = message.trimmed();
    if (success) {
        QString saveError;
        syncProjectStateForPersistence();
        if (!m_project.save(&saveError)) {
            success = false;
            finalMessage = saveError;
        } else {
            const QString projectCopy = QDir(mediaQueueOutputDirectory(m_activeMediaQueueItemId))
                .filePath(QStringLiteral("project.ladub.json"));
            if (!replaceCopy(m_project.projectPath, projectCopy, &saveError)) {
                success = false;
                finalMessage = saveError;
            } else {
                recordMediaQueueOutput(QStringLiteral("project"), projectCopy);
            }
        }
    }
    item = m_mediaQueueItems.at(index).toMap();
    item.insert(QStringLiteral("processState"), success ? QStringLiteral("completed") : QStringLiteral("failed"));
    item.insert(QStringLiteral("progress"), 100);
    item.insert(QStringLiteral("stage"), success ? QStringLiteral("completed") : QStringLiteral("failed"));
    item.insert(QStringLiteral("status"), success ? QStringLiteral("Completed")
                : (finalMessage.isEmpty() ? QStringLiteral("Failed") : finalMessage));
    if (!success) item.insert(QStringLiteral("error"), item.value(QStringLiteral("status")));
    replaceMediaQueueItem(index, item);
    m_mediaQueueStatus = success ? QStringLiteral("Completed %1").arg(item.value(QStringLiteral("displayName")).toString())
                                 : QStringLiteral("Failed %1").arg(item.value(QStringLiteral("displayName")).toString());
    m_activeMediaQueueItemId.clear();
    m_mediaQueueStage.clear();
    m_mediaQueueProjects.remove(item.value(QStringLiteral("id")).toString());
    emit mediaQueueChanged();
    QTimer::singleShot(0, this, &DubbingController::startNextMediaQueueItem);
}

void DubbingController::finishMediaQueueRun(const QString &message)
{
    // A one-item Import/Normalize run is not merely a background batch: it is
    // the user's source-selection action for the active project.  The former
    // unconditional restore below discarded the successfully ingested project
    // and left DubbingPage on its original, empty project ("No media").
    // Multi-item library work intentionally continues to restore the project,
    // because there is no unambiguous single item to activate.
    const bool promotedSingleImport = !m_mediaQueueCancelling
        && promoteSingleImportedMediaQueueProject();
    m_mediaQueueProcessing = false;
    m_mediaQueueCancelling = false;
    m_activeMediaQueueItemId.clear();
    m_mediaQueueStage.clear();
    m_mediaQueueStagePlan.clear();
    m_mediaQueueStagePlanIndex = 0;
    m_mediaQueueProjects.clear();
    if (m_mediaQueueOriginalProjectCaptured && !promotedSingleImport) {
        m_project = m_mediaQueueOriginalProject;
        m_workflowNodeConfigurations = m_mediaQueueOriginalNodeConfigurations;
        m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
        m_runner->setSourceVocalsAudioPath(m_project.vocalsAudioPath);
        m_runner->setPreviewPath(m_mediaQueueOriginalPreviewPath);
        m_runner->setExportPath(m_mediaQueueOriginalExportPath);
    }
    m_mediaQueueOriginalProjectCaptured = false;
    m_mediaQueueStatus = message.isEmpty() ? QStringLiteral("Media batch finished") : message;
    emit projectChanged();
    emit segmentsChanged();
    emit workflowChanged();
    emit mediaQueueChanged();
    emit processingChanged();
}

bool DubbingController::promoteSingleImportedMediaQueueProject()
{
    if (m_mediaQueueTasks.value(QStringLiteral("operation")).toString() != QStringLiteral("import"))
        return false;

    QVariantMap importedItem;
    int completedSelectedItems = 0;
    for (const QVariant &value : std::as_const(m_mediaQueueItems)) {
        const QVariantMap item = value.toMap();
        if (!item.value(QStringLiteral("selected")).toBool()
            || item.value(QStringLiteral("downloadState")).toString() != QStringLiteral("downloaded")
            || item.value(QStringLiteral("processState")).toString() != QStringLiteral("completed")) {
            continue;
        }
        importedItem = item;
        ++completedSelectedItems;
    }
    if (completedSelectedItems != 1) return false;

    DubbingProject importedProject;
    QString loadError;
    if (!loadMediaQueueProject(importedItem, &importedProject, &loadError)) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Imported media could not become the active project: %1")
                            .arg(loadError));
        return false;
    }

    // Retain the path selected at the project gate.  The per-item copy remains
    // in batch-output for later library actions, while Save/Resume continues
    // to use the project the user actually opened.
    if (!m_mediaQueueOriginalProject.projectPath.trimmed().isEmpty())
        importedProject.projectPath = m_mediaQueueOriginalProject.projectPath;
    m_project = std::move(importedProject);
    m_workflowNodeConfigurations = m_project.workflowNodeConfigurations;
    m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
    m_runner->setSourceVocalsAudioPath(m_project.vocalsAudioPath);
    m_runner->setPreviewPath(QString());
    m_runner->setExportPath(QString());
    m_currentStepId = QStringLiteral("import");
    m_lastCompletedStepId = QStringLiteral("ingest");

    QString saveError;
    syncProjectStateForPersistence();
    if (!m_project.save(&saveError)) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Imported media became active but could not be saved: %1")
                            .arg(saveError));
    }
    return true;
}

void DubbingController::cancelMediaQueue()
{
    if (mediaQueueDownloading()) {
        m_mediaQueueCancelling = true;
        for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
            QVariantMap item = m_mediaQueueItems.at(index).toMap();
            const QString downloadState = item.value(QStringLiteral("downloadState")).toString();
            if (downloadState == QStringLiteral("queued")) {
                item.remove(QStringLiteral("sourceUrl"));
                item.insert(QStringLiteral("downloadState"), QStringLiteral("cancelled"));
                item.insert(QStringLiteral("processState"), QStringLiteral("cancelled"));
                item.insert(QStringLiteral("status"), QStringLiteral("Cancelled before download"));
                item.insert(QStringLiteral("selected"), false);
                replaceMediaQueueItem(index, item);
            }
        }
        m_mediaQueueStatus = QStringLiteral("Cancelling download queue");
        emit mediaQueueChanged();
        if (m_remoteMediaImport && m_remoteMediaImport->active()) {
            m_remoteMediaImport->cancel();
        } else {
            m_activeMediaQueueDownloadId.clear();
            m_mediaQueueCancelling = false;
            m_mediaQueueStatus = QStringLiteral("Download queue cancelled");
            emit mediaQueueChanged();
        }
        return;
    }
    if (!m_mediaQueueProcessing) return;
    m_mediaQueueCancelling = true;
    for (int index = 0; index < m_mediaQueueItems.size(); ++index) {
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        if (item.value(QStringLiteral("processState")).toString() == QStringLiteral("queued")) {
            item.insert(QStringLiteral("processState"), QStringLiteral("cancelled"));
            item.insert(QStringLiteral("status"), QStringLiteral("Cancelled before processing"));
            replaceMediaQueueItem(index, item);
        }
    }
    if (m_runner && m_runner->processing()) m_runner->cancel();
    const int activeIndex = mediaQueueIndex(m_activeMediaQueueItemId);
    if (activeIndex >= 0) {
        QVariantMap activeItem = m_mediaQueueItems.at(activeIndex).toMap();
        activeItem.insert(QStringLiteral("processState"), QStringLiteral("cancelled"));
        activeItem.insert(QStringLiteral("status"), QStringLiteral("Cancelled during ")
                          + activeItem.value(QStringLiteral("stage")).toString());
        activeItem.insert(QStringLiteral("progress"), qBound(0, activeItem.value(QStringLiteral("progress")).toInt(), 99));
        replaceMediaQueueItem(activeIndex, activeItem);
    }
    if (!m_runner || !m_runner->processing()) finishMediaQueueRun(QStringLiteral("Batch cancelled"));
}
