QVariantList SubtitleOcrController::segmentsToVariant(const QVector<SubtitleOcrSegment> &segments)
{
    QVariantList result;
    result.reserve(segments.size());
    for (const SubtitleOcrSegment &segment : segments) {
        result.append(QVariantMap{{QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                  {QStringLiteral("startMs"), segment.startMs},
                                  {QStringLiteral("endMs"), segment.endMs},
                                  {QStringLiteral("text"), segment.text},
                                  {QStringLiteral("confidence"), segment.confidence}});
    }
    return result;
}

QVector<SubtitleOcrSegment> SubtitleOcrController::segmentsFromVariant(const QVariantList &segments,
                                                                        QString *error)
{
    QVector<SubtitleOcrSegment> result;
    result.reserve(segments.size());
    for (const QVariant &entry : segments) {
        const QVariantMap item = entry.toMap();
        const qint64 startMs = item.value(QStringLiteral("startMs")).toLongLong();
        const qint64 endMs = item.value(QStringLiteral("endMs")).toLongLong();
        const QString text = item.value(QStringLiteral("text")).toString().trimmed();
        const double confidence = item.value(QStringLiteral("confidence")).toDouble();
        if (startMs < 0 || endMs <= startMs || text.isEmpty() || confidence < 0.0 || confidence > 1.0) {
            if (error) *error = QStringLiteral("Subtitle OCR project contains an invalid transcript segment.");
            return {};
        }
        result.append({startMs, endMs, text, confidence});
    }
    return result;
}

void SubtitleOcrController::updateSegment(int index, const QVariantMap &patch)
{
    if (m_processing || index < 0 || index >= m_segments.size()) return;
    QVariantMap segment = m_segments.at(index).toMap();
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) segment.insert(it.key(), it.value());
    const qint64 startMs = segment.value(QStringLiteral("startMs")).toLongLong();
    const qint64 endMs = segment.value(QStringLiteral("endMs")).toLongLong();
    if (startMs < 0 || endMs <= startMs || segment.value(QStringLiteral("text")).toString().trimmed().isEmpty()) {
        setError(QStringLiteral("Subtitle OCR segment needs text and an end time after its start time."));
        return;
    }
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    segment.insert(QStringLiteral("text"), segment.value(QStringLiteral("text")).toString().trimmed());
    m_segments[index] = segment;
    setError({});
    emit segmentsChanged();
}

void SubtitleOcrController::removeSegment(int index)
{
    if (m_processing || index < 0 || index >= m_segments.size()) return;
    m_segments.removeAt(index);
    emit segmentsChanged();
}

bool SubtitleOcrController::writeTextFile(const QString &path, const QString &content)
{
    const QString localPath = PathUtils::urlToLocalPath(path);
    if (localPath.isEmpty()) return false;
    QSaveFile output(localPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray utf8 = content.toUtf8();
    return output.write(utf8) == utf8.size() && output.commit();
}

bool SubtitleOcrController::exportSrt(const QString &path)
{
    QString error;
    const QVector<SubtitleOcrSegment> parsed = segmentsFromVariant(m_segments, &error);
    if (m_resultStatus != QStringLiteral("completed") || parsed.isEmpty()) {
        if (m_error.isEmpty()) {
            setError(QStringLiteral("Subtitle OCR has no published transcript segments to export."));
        }
        return false;
    }
    if (!writeTextFile(path, SubtitleOcrPipeline::toSrt(parsed))) {
        setError(QStringLiteral("Cannot write the Subtitle OCR SRT export."));
        return false;
    }
    m_exportedSegmentCount = parsed.size();
    emit runStatisticsChanged();
    return true;
}

bool SubtitleOcrController::exportText(const QString &path)
{
    QString error;
    const QVector<SubtitleOcrSegment> parsed = segmentsFromVariant(m_segments, &error);
    if (m_resultStatus != QStringLiteral("completed") || parsed.isEmpty()) {
        if (m_error.isEmpty()) {
            setError(QStringLiteral("Subtitle OCR has no published transcript segments to export."));
        }
        return false;
    }
    QStringList lines;
    for (const SubtitleOcrSegment &segment : parsed) lines.append(segment.text);
    if (lines.isEmpty() || !writeTextFile(path, lines.join(QStringLiteral("\n\n")) + QLatin1Char('\n'))) {
        setError(QStringLiteral("Cannot write the Subtitle OCR text export."));
        return false;
    }
    m_exportedSegmentCount = parsed.size();
    emit runStatisticsChanged();
    return true;
}

bool SubtitleOcrController::saveProject(const QString &path)
{
    if (m_processing) return false;
    const QString destination = PathUtils::urlToLocalPath(path.trimmed().isEmpty() ? m_projectPath : path);
    if (destination.isEmpty()) {
        setError(QStringLiteral("Choose a Subtitle OCR project path before saving."));
        return false;
    }
    QString segmentsError;
    if (!m_segments.isEmpty() && segmentsFromVariant(m_segments, &segmentsError).isEmpty()) {
        setError(segmentsError);
        return false;
    }
    QJsonObject project{{QStringLiteral("schemaVersion"), kSubtitleOcrProjectVersion},
                        {QStringLiteral("sourcePath"), m_sourcePath},
                        {QStringLiteral("sourceWidth"), m_sourceWidth},
                        {QStringLiteral("sourceHeight"), m_sourceHeight},
                        {QStringLiteral("frameWidth"), m_frameWidth},
                        {QStringLiteral("frameHeight"), m_frameHeight},
                        {QStringLiteral("rotationDegrees"), m_rotationDegrees},
                        {QStringLiteral("sampleAspectRatio"), m_sampleAspectRatio},
                        {QStringLiteral("displayAspectRatio"), m_displayAspectRatio},
                        {QStringLiteral("durationMs"), static_cast<double>(m_durationMs)},
                        {QStringLiteral("roi"), QJsonObject{{QStringLiteral("x"), m_roi.x},
                                                            {QStringLiteral("y"), m_roi.y},
                                                            {QStringLiteral("width"), m_roi.width},
                                                            {QStringLiteral("height"), m_roi.height}}},
                        {QStringLiteral("ocrLanguage"), m_ocrLanguage},
                        {QStringLiteral("executionRoute"), m_executionRoute},
                        {QStringLiteral("localEngineId"), m_localEngineId},
                        {QStringLiteral("localEngineVersion"), localEngineVersion()},
                        {QStringLiteral("colabModelId"), m_colabModelId},
                        {QStringLiteral("sampleIntervalMs"), static_cast<double>(m_sampleIntervalMs)},
                        {QStringLiteral("minimumConfidence"), m_minimumConfidence},
                        {QStringLiteral("segments"), QJsonArray::fromVariantList(m_segments)}};
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(QStringLiteral("Cannot write Subtitle OCR project."));
        return false;
    }
    const QByteArray document = QJsonDocument(project).toJson(QJsonDocument::Indented);
    if (file.write(document) != document.size() || !file.commit()) {
        setError(QStringLiteral("Cannot atomically save Subtitle OCR project."));
        return false;
    }
    m_projectPath = QFileInfo(destination).absoluteFilePath();
    setError({});
    emit projectChanged();
    return true;
}

bool SubtitleOcrController::applyProject(const QVariantMap &project, const QString &absoluteProjectPath)
{
    const int version = project.contains(QStringLiteral("schemaVersion"))
        ? project.value(QStringLiteral("schemaVersion")).toInt() : -1;
    const QString source = PathUtils::urlToLocalPath(project.value(QStringLiteral("sourcePath")).toString());
    const QVariantMap roiMap = project.value(QStringLiteral("roi")).toMap();
    const SubtitleOcrRoi roi{roiMap.value(QStringLiteral("x")).toDouble(),
                             roiMap.value(QStringLiteral("y")).toDouble(),
                             roiMap.value(QStringLiteral("width")).toDouble(),
                             roiMap.value(QStringLiteral("height")).toDouble()};
    const int width = project.value(QStringLiteral("sourceWidth")).toInt();
    const int height = project.value(QStringLiteral("sourceHeight")).toInt();
    const int frameWidth = project.value(QStringLiteral("frameWidth"), width).toInt();
    const int frameHeight = project.value(QStringLiteral("frameHeight"), height).toInt();
    const int rotation = normalizedRotation(project.value(QStringLiteral("rotationDegrees")).toInt());
    const QString sampleAspectRatio = project.value(QStringLiteral("sampleAspectRatio")).toString();
    const QString displayAspectRatio = project.value(QStringLiteral("displayAspectRatio")).toString();
    const qint64 duration = project.value(QStringLiteral("durationMs")).toLongLong();
    const QString language = project.value(QStringLiteral("ocrLanguage")).toString().trimmed();
    const QString executionRoute = project.value(QStringLiteral("executionRoute"),
        QStringLiteral("local-cpu")).toString().trimmed().toLower();
    // Version 1 projects predate PaddleOCR.  Preserve their existing local
    // behaviour by migrating them explicitly to the named Tesseract baseline,
    // never by guessing a model or silently changing an old transcript route.
    const QString localEngine = version == 1 ? QStringLiteral("tesseract-baseline")
        : normalizedLocalEngineId(project.value(QStringLiteral("localEngineId")).toString());
    const QString localEngineVersion = project.value(QStringLiteral("localEngineVersion")).toString().trimmed();
    const QString colabModelId = project.value(QStringLiteral("colabModelId"),
        kColabSubtitleOcrModel).toString().trimmed().toLower();
    const qint64 interval = project.value(QStringLiteral("sampleIntervalMs")).toLongLong();
    const double confidence = project.value(QStringLiteral("minimumConfidence")).toDouble();
    QString segmentsError;
    const QVector<SubtitleOcrSegment> parsed = segmentsFromVariant(
        project.value(QStringLiteral("segments")).toList(), &segmentsError);
    if ((version != 1 && version != kSubtitleOcrProjectVersion) || source.isEmpty() || !QFileInfo(source).isFile()
        || width <= 0 || height <= 0 || frameWidth <= 0 || frameHeight <= 0 || duration <= 0 || !roi.isValid() || language.isEmpty()
        || interval < 100 || interval > 30000 || confidence < 0.0 || confidence > 1.0
        || (executionRoute != QStringLiteral("local-cpu") && executionRoute != QStringLiteral("colab-gpu"))
        || localEngine.isEmpty()
        || (version == kSubtitleOcrProjectVersion && localEngineVersion != QStringLiteral("5.5.1")
            && localEngineVersion != QString::fromLatin1(PaddleOcrRuntimeLocator::engineVersion()))
        || colabModelId != kColabSubtitleOcrModel
        || (!project.value(QStringLiteral("segments")).toList().isEmpty() && parsed.isEmpty())) {
        setError(segmentsError.isEmpty() ? QStringLiteral("Invalid or incompatible Subtitle OCR project.") : segmentsError);
        return false;
    }
    cleanWorkspace();
    m_sourcePath = QFileInfo(source).absoluteFilePath();
    m_sourceWidth = width;
    m_sourceHeight = height;
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    m_rotationDegrees = rotation;
    m_sampleAspectRatio = sampleAspectRatio;
    m_displayAspectRatio = displayAspectRatio;
    m_durationMs = duration;
    m_roi = roi;
    m_ocrLanguage = language;
    m_executionRoute = executionRoute;
    m_localEngineId = localEngine;
    m_colabModelId = colabModelId;
    m_sampleIntervalMs = interval;
    m_minimumConfidence = confidence;
    m_segments = project.value(QStringLiteral("segments")).toList();
    m_projectPath = absoluteProjectPath;
    setError({});
    setPhase(QStringLiteral("ready"));
    setProgress(0, false);
    emit sourceChanged();
    emit roiChanged();
    emit settingsChanged();
    emit colabRouteChanged();
    emit segmentsChanged();
    emit projectChanged();
    return true;
}

bool SubtitleOcrController::openProject(const QString &path)
{
    if (m_processing) return false;
    const QString localPath = PathUtils::urlToLocalPath(path);
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open Subtitle OCR project."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(QStringLiteral("Invalid Subtitle OCR project JSON."));
        return false;
    }
    return applyProject(document.object().toVariantMap(), QFileInfo(localPath).absoluteFilePath());
}

bool SubtitleOcrController::sendToSubtitleVoice()
{
    if (!m_subtitleVoice || m_segments.isEmpty()) {
        setError(QStringLiteral("Run Subtitle OCR and review at least one segment before opening Subtitle Voice."));
        return false;
    }
    QString parseError;
    const QVector<SubtitleOcrSegment> parsed = segmentsFromVariant(m_segments, &parseError);
    if (parsed.isEmpty()) {
        setError(parseError);
        return false;
    }
    const QString pattern = QDir(PathUtils::cacheDir()).filePath(QStringLiteral("subtitle-ocr-transfer-XXXXXX.srt"));
    QTemporaryFile transfer(pattern);
    if (!transfer.open()) {
        setError(QStringLiteral("Cannot create a temporary Subtitle OCR transfer file."));
        return false;
    }
    const QByteArray srt = SubtitleOcrPipeline::toSrt(parsed).toUtf8();
    if (transfer.write(srt) != srt.size() || !transfer.flush() || !m_subtitleVoice->importSrt(transfer.fileName())) {
        setError(QStringLiteral("Could not transfer reviewed OCR subtitles to Subtitle Voice."));
        return false;
    }
    setError({});
    return true;
}

bool SubtitleOcrController::sendToDubbing()
{
    if (!m_dubbing || m_segments.isEmpty()) {
        setError(QStringLiteral("Run Subtitle OCR and review at least one segment before using it in Dubbing."));
        return false;
    }
    if (!m_dubbing->replaceTranscriptSegments(m_segments)) {
        setError(m_dubbing->lastError());
        return false;
    }
    setError({});
    return true;
}


