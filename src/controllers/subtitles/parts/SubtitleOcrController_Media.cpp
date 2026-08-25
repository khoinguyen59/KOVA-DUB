bool SubtitleOcrController::loadSource(const QString &path)
{
    if (m_processing) return false;
    const QString localPath = PathUtils::urlToLocalPath(path);
    const QFileInfo info(localPath);
    if (!info.isFile() || info.size() <= 0) {
        setError(QStringLiteral("Choose a readable video file before running Subtitle OCR."));
        return false;
    }
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    if (!media.hasFfprobe()) {
        setError(QStringLiteral("FFprobe is required to inspect the video before Subtitle OCR."));
        return false;
    }
    m_pendingSourcePath = info.absoluteFilePath();
    m_cancelRequested = false;
    m_lastFailedOperation = Operation::None;
    m_samples.clear();
    emit frameRetryChanged();
    clearDiagnostics();
    appendDiagnostic(QStringLiteral("probe-start"),
                     QStringLiteral("source=%1; ffprobe=%2")
                         .arg(m_pendingSourcePath, media.ffprobe));
    setError({});
    setProcessing(true);
    setPhase(QStringLiteral("probing"));
    setProgress(0, false);
    startProcess(Operation::Probe, media.ffprobe,
                 {QStringLiteral("-v"), QStringLiteral("error"),
                  QStringLiteral("-select_streams"), QStringLiteral("v:0"),
                  QStringLiteral("-show_entries"),
                  QStringLiteral("stream=width,height,sample_aspect_ratio,display_aspect_ratio:stream_tags=rotate:stream_side_data=rotation:format=duration"),
                  QStringLiteral("-of"), QStringLiteral("json"), m_pendingSourcePath});
    return true;
}

bool SubtitleOcrController::useDownloadedMedia(const QString &path)
{
    if (m_processing || m_sourceImporting) return false;
    setSourceImportState(false);
    const bool accepted = loadSource(path);
    if (!accepted) {
        m_sourceImportError = m_error;
        emit sourceImportChanged();
    }
    return accepted;
}

bool SubtitleOcrController::importSourceLink(const QString &url)
{
    Q_UNUSED(url);
    // Subtitle OCR accepts local media only.  A public link must first be
    // resolved by the dedicated local downloader and then selected from the
    // local media library; OCR never activates a desktop downloader fallback.
    setSourceImportState(false, {}, QStringLiteral(
        "Download the public link locally, then choose the completed file for Subtitle OCR."));
    return false;
}

void SubtitleOcrController::cancelSourceImport()
{
    if (!m_sourceImporting) return;
    setSourceImportState(false, {}, QStringLiteral("The local media download was canceled from its media library."));
}

bool SubtitleOcrController::retrySourceImport()
{
    setSourceImportState(false, {}, QStringLiteral(
        "Retry the public link with the local downloader, then choose the completed file."));
    return false;
}

void SubtitleOcrController::setSourceImportState(bool importing, const QString &status,
                                                 const QString &error)
{
    const qint64 received = importing ? m_sourceImportReceivedBytes : 0;
    const qint64 total = importing ? m_sourceImportTotalBytes : -1;
    if (m_sourceImporting == importing && m_sourceImportStatus == status
        && m_sourceImportError == error && m_sourceImportReceivedBytes == received
        && m_sourceImportTotalBytes == total) {
        return;
    }
    m_sourceImporting = importing;
    m_sourceImportStatus = status;
    m_sourceImportError = error;
    m_sourceImportReceivedBytes = received;
    m_sourceImportTotalBytes = total;
    emit sourceImportChanged();
}

void SubtitleOcrController::completeProbe(const QByteArray &output)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    const QJsonObject root = document.object();
    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    const QJsonObject stream = streams.isEmpty() ? QJsonObject() : streams.at(0).toObject();
    const int width = stream.value(QStringLiteral("width")).toInt();
    const int height = stream.value(QStringLiteral("height")).toInt();
    int rotation = 0;
    for (const QJsonValue &sideDataValue : stream.value(QStringLiteral("side_data_list")).toArray()) {
        const QJsonObject sideData = sideDataValue.toObject();
        if (sideData.contains(QStringLiteral("rotation"))) {
            rotation = normalizedRotation(qRound(sideData.value(QStringLiteral("rotation")).toDouble()));
            break;
        }
    }
    if (rotation == 0) {
        bool rotationOk = false;
        const int taggedRotation = stream.value(QStringLiteral("tags")).toObject()
            .value(QStringLiteral("rotate")).toString().toInt(&rotationOk);
        if (rotationOk) rotation = normalizedRotation(taggedRotation);
    }
    bool durationOk = false;
    const double durationSeconds = root.value(QStringLiteral("format")).toObject()
        .value(QStringLiteral("duration")).toVariant().toDouble(&durationOk);
    if (parseError.error != QJsonParseError::NoError || width <= 0 || height <= 0
        || !durationOk || durationSeconds <= 0.0) {
        fail(QStringLiteral("The selected file has no readable video stream for Subtitle OCR."));
        return;
    }
    cleanWorkspace();
    m_sourcePath = m_pendingSourcePath;
    m_pendingSourcePath.clear();
    m_rotationDegrees = rotation;
    const bool transposed = rotation == 90 || rotation == 270;
    m_frameWidth = transposed ? height : width;
    m_frameHeight = transposed ? width : height;
    m_sampleAspectRatio = stream.value(QStringLiteral("sample_aspect_ratio")).toString();
    m_displayAspectRatio = stream.value(QStringLiteral("display_aspect_ratio")).toString();
    double displayAspectRatio = 0.0;
    if (parseAspectRatio(m_displayAspectRatio, &displayAspectRatio)) {
        if (transposed) displayAspectRatio = 1.0 / displayAspectRatio;
        m_sourceWidth = qMax(1, qRound(m_frameHeight * displayAspectRatio));
    } else {
        m_sourceWidth = m_frameWidth;
    }
    m_sourceHeight = m_frameHeight;
    m_durationMs = qRound64(durationSeconds * 1000.0);
    appendDiagnostic(QStringLiteral("probe-complete"),
                     QStringLiteral("source=%1; frame=%2x%3; display=%4x%5; rotation=%6; SAR=%7; DAR=%8; durationMs=%9")
                         .arg(m_sourcePath).arg(m_frameWidth).arg(m_frameHeight)
                         .arg(m_sourceWidth).arg(m_sourceHeight).arg(m_rotationDegrees)
                         .arg(m_sampleAspectRatio.isEmpty() ? QStringLiteral("unknown") : m_sampleAspectRatio)
                         .arg(m_displayAspectRatio.isEmpty() ? QStringLiteral("unknown") : m_displayAspectRatio)
                         .arg(m_durationMs));
    m_segments.clear();
    resetRunStatistics();
    setResultStatus(QStringLiteral("ready"));
    setError({});
    setProgress(0, false);
    setProcessing(false);
    setPhase(QStringLiteral("ready"));
    if (m_sourceImportStatus == QStringLiteral("Inspecting staged media")) {
        setSourceImportState(false);
    }
    emit sourceChanged();
    emit segmentsChanged();
}


void SubtitleOcrController::beginCacheLookup()
{
    if (!m_processing || m_cancelRequested) {
        completeCancellation();
        return;
    }
    setPhase(QStringLiteral("fingerprinting-source"));
    m_sourceFingerprintWatcher.setFuture(QtConcurrent::run(sha256File, m_sourcePath));
}

QString SubtitleOcrController::cacheKeyMaterial() const
{
    const QFileInfo runtime(runtimePath());
    const PaddleOcrRuntimeResolution paddle = usesPaddleLocalEngine()
        ? PaddleOcrRuntimeLocator::resolve() : PaddleOcrRuntimeResolution{};
    const QFileInfo paddleManifest(paddle.manifestPath);
    const QString paddleManifestHash = paddle.manifestPath.isEmpty()
        ? QString() : sha256File(paddle.manifestPath);
    return QStringLiteral("schema=%1\nsource=%2\nfingerprint=%3\nsize=%4\nroi=%5\ninterval=%6\nconfidence=%7\nlanguage=%8\nroute=%9\nengine=%10\nengineVersion=%11\nruntimePath=%12\nengineSize=%13\nengineModified=%14\npaddleManifest=%15\npaddleManifestSha256=%16\npreprocess=scale3-gray-v1\nbenchmarkLimit=%17")
        .arg(kSubtitleOcrCacheVersion)
        .arg(QFileInfo(m_sourcePath).canonicalFilePath(), m_sourceFingerprint)
        .arg(QFileInfo(m_sourcePath).size())
        .arg(normalizedRoiText(m_roi))
        .arg(m_sampleIntervalMs)
        .arg(m_minimumConfidence, 0, 'f', 6)
        .arg(m_ocrLanguage, m_executionRoute,
             m_executionRoute == QStringLiteral("local-cpu") ? m_localEngineId : m_colabModelId)
        .arg(localEngineVersion())
        .arg(runtime.absoluteFilePath())
        .arg(runtime.size())
        .arg(runtime.lastModified().toUTC().toString(Qt::ISODateWithMs))
        .arg(paddleManifest.absoluteFilePath())
        .arg(paddleManifestHash)
        .arg(m_benchmarkSampleLimit);
}

QString SubtitleOcrController::cacheFilePath() const
{
    return QDir(PathUtils::cacheDir()).filePath(
        // Completed OCR artifacts must survive a clean staging workspace, but
        // must not live inside it.  A cancel/retry can therefore remove every
        // temporary crop without deleting a valid, separately keyed result.
        QStringLiteral("subtitle-ocr-cache/results/%1.json").arg(m_cacheKey));
}

bool SubtitleOcrController::loadCachedResult()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = document.object();
    if (document.isNull() || root.value(QStringLiteral("version")).toInt() != kSubtitleOcrCacheVersion
        || root.value(QStringLiteral("key")).toString() != m_cacheKey) return false;
    QString error;
    const QVector<SubtitleOcrSegment> cached = segmentsFromVariant(
        root.value(QStringLiteral("segments")).toArray().toVariantList(), &error);
    const bool requireHan = m_ocrLanguage.compare(QStringLiteral("chi_sim"), Qt::CaseInsensitive) == 0;
    if (!SubtitleOcrPipeline::validatePublishableSegments(cached, requireHan, &error)) return false;
    m_segments = segmentsToVariant(cached);
    m_publishedSegmentCount = m_segments.size();
    m_cacheReused = true;
    m_completedSampleCount = m_samples.size();
    appendDiagnostic(QStringLiteral("result-cache-hit"),
                     QStringLiteral("key=%1; segments=%2").arg(m_cacheKey).arg(m_segments.size()));
    cleanWorkspace();
    setProcessing(false);
    setProgress(100, true);
    setPhase(QStringLiteral("completed"));
    setResultStatus(QStringLiteral("completed"));
    setError({});
    emit runStatisticsChanged();
    emit segmentsChanged();
    return true;
}

bool SubtitleOcrController::storeCachedResult()
{
    if (m_cacheKey.isEmpty() || m_segments.isEmpty()) return false;
    const QFileInfo destination(cacheFilePath());
    if (!QDir().mkpath(destination.absolutePath())) return false;
    QJsonObject root{{QStringLiteral("version"), kSubtitleOcrCacheVersion},
                     {QStringLiteral("key"), m_cacheKey},
                     {QStringLiteral("sourceFingerprint"), m_sourceFingerprint},
                     {QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                     {QStringLiteral("segments"), QJsonArray::fromVariantList(m_segments)}};
    QSaveFile file(destination.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0) return false;
    return file.commit();
}

void SubtitleOcrController::releaseActiveCacheKey()
{
    if (!m_cacheKeyActive) return;
    s_activeOcrCacheKeys.remove(m_cacheKey);
    m_cacheKeyActive = false;
}

void SubtitleOcrController::onSourceFingerprintReady()
{
    if (!m_processing || m_executionRoute != QStringLiteral("local-cpu")) return;
    m_sourceFingerprint = m_sourceFingerprintWatcher.result();
    if (m_sourceFingerprint.isEmpty()) {
        fail(QStringLiteral("Could not fingerprint the Subtitle OCR source for a safe reusable result."));
        return;
    }
    m_cacheKey = QString::fromLatin1(QCryptographicHash::hash(
        cacheKeyMaterial().toUtf8(), QCryptographicHash::Sha256).toHex());
    if (loadCachedResult()) return;
    if (s_activeOcrCacheKeys.contains(m_cacheKey)) {
        fail(QStringLiteral("An identical Subtitle OCR job is already running. Wait for that job or use its published result."),
             Operation::None, QStringLiteral("matching_job_active"));
        return;
    }
    s_activeOcrCacheKeys.insert(m_cacheKey);
    m_cacheKeyActive = true;
    m_lastForwardProgressMs = m_runElapsed.elapsed();
    m_forwardProgressTimer.start();
    beginNextChunk();
}


