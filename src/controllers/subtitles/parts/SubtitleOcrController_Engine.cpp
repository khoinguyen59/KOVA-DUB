void SubtitleOcrController::updateForwardProgress()
{
    m_lastForwardProgressMs = m_runElapsed.isValid() ? m_runElapsed.elapsed() : 0;
    setProgress(qRound(100.0 * m_completedSampleCount / qMax(1, m_samples.size())), true);
    emit runStatisticsChanged();
}

void SubtitleOcrController::checkForwardProgress()
{
    if (!m_processing || m_executionRoute != QStringLiteral("local-cpu") || !m_runElapsed.isValid()) return;
    if (m_runElapsed.elapsed() - m_lastForwardProgressMs > kForwardProgressTimeoutMs) {
        fail(QStringLiteral("Subtitle OCR made no forward progress for 60 seconds; the job was stopped before a misleading long wait."),
             m_operation == Operation::ExtractChunk ? Operation::ExtractChunk : Operation::RecognizeFrame);
    }
}

void SubtitleOcrController::beginNextChunk()
{
    if (!m_processing || m_cancelRequested) {
        completeCancellation();
        return;
    }
    if (m_chunkStartIndex >= m_samples.size()) {
        completeRun();
        return;
    }
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    const QStringList crop = SubtitleOcrPipeline::ffmpegCropArguments(m_roi, m_frameWidth, m_frameHeight);
    const SubtitleOcrRect cropRect = SubtitleOcrPipeline::sourceRect(
        m_roi, m_frameWidth, m_frameHeight);
    if (!media.hasFfmpeg() || crop.size() != 2) {
        fail(QStringLiteral("Subtitle OCR batch frame extraction is no longer configured."), Operation::ExtractChunk);
        return;
    }
    m_chunkEndIndex = qMin(m_chunkStartIndex + kChunkSampleCount, m_samples.size());
    // The final safe-end timestamp is deliberately allowed to be off the
    // regular interval cadence.  Keep it in a one-frame trailing chunk so the
    // fps filter cannot silently omit it (for example 0,30,60,90,109 seconds).
    if (m_chunkEndIndex == m_samples.size() && m_chunkEndIndex - m_chunkStartIndex > 1
        && m_samples.at(m_chunkEndIndex - 1) - m_samples.at(m_chunkEndIndex - 2)
            != m_sampleIntervalMs) {
        --m_chunkEndIndex;
    }
    const int count = m_chunkEndIndex - m_chunkStartIndex;
    const qint64 startMs = m_samples.at(m_chunkStartIndex);
    const QString pattern = QDir(m_workspacePath).filePath(QStringLiteral("frame-%06d.png"));
    // A trailing safe-end sample may be a one-frame chunk.  In that case an
    // fps cadence can wait for a future tick beyond EOF, so extract the seeked
    // frame directly while keeping the same crop/preprocess chain.
    const QString filter = count == 1
        ? crop.at(1)
        : crop.at(1) + QStringLiteral(",fps=fps=1000/%1:round=near").arg(m_sampleIntervalMs);
    setPhase(QStringLiteral("extracting chunk %1-%2/%3")
                 .arg(m_chunkStartIndex + 1).arg(m_chunkEndIndex).arg(m_samples.size()));
    appendDiagnostic(QStringLiteral("batch-extraction-start"),
                     QStringLiteral("chunk=%1-%2; samples=%3; timestampMs=%4; ffmpeg=%5; "
                                    "normalizedRoi=%6; pixelCrop=%7")
                         .arg(m_chunkStartIndex + 1).arg(m_chunkEndIndex).arg(count)
                         .arg(startMs).arg(media.ffmpeg).arg(normalizedRoiText(m_roi))
                         .arg(cropText(cropRect)));
    ++m_ffmpegProcessCount;
    emit runStatisticsChanged();
    startProcess(Operation::ExtractChunk, media.ffmpeg,
                 {QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                  QStringLiteral("error"), QStringLiteral("-ss"), ffmpegTime(startMs),
                  QStringLiteral("-i"), m_sourcePath, QStringLiteral("-an"), QStringLiteral("-vf"), filter,
                  QStringLiteral("-frames:v"), QString::number(count), QStringLiteral("-start_number"),
                  QString::number(m_chunkStartIndex), QStringLiteral("-y"), pattern});
}

void SubtitleOcrController::queueChunkFrames()
{
    bool reusedCompletedRecognition = false;
    for (int index = m_chunkStartIndex; index < m_chunkEndIndex; ++index) {
        const QString framePath = QDir(m_workspacePath).filePath(
            QStringLiteral("frame-%1.png").arg(index, 6, 10, QLatin1Char('0')));
        QByteArray hash;
        QString error;
        ++m_extractedFrameCount;
        if (!validateFrame(framePath, &hash, &error)) {
            fail(QStringLiteral("Subtitle OCR batch extraction did not produce a readable PNG crop: %1").arg(error),
                 Operation::ExtractChunk);
            return;
        }
        ++m_readableCropCount;
        const auto existing = m_uniqueFrames.constFind(hash);
        if (existing != m_uniqueFrames.cend()) {
            ++m_deduplicatedFrameCount;
            QFile::remove(framePath);
            // A later chunk can contain a frame already recognized by a
            // completed worker.  Reuse that exact result immediately instead
            // of queuing a worker that will never run, and count it as real
            // forward progress.  If the worker is still active, append the
            // sample so its completion publishes both timestamps in order.
            if (existing.value()->completed) {
                appendObservation({m_samples.at(index), existing.value()->recognizedText,
                                   existing.value()->recognizedConfidence});
                ++m_completedSampleCount;
                reusedCompletedRecognition = true;
            } else {
                existing.value()->sampleIndexes.append(index);
            }
            continue;
        }
        auto item = QSharedPointer<RecognitionItem>::create();
        item->frameHash = hash;
        item->framePath = framePath;
        item->sampleIndexes.append(index);
        m_uniqueFrames.insert(hash, item);
        m_recognitionQueue.enqueue(item);
    }
    if (reusedCompletedRecognition) updateForwardProgress();
    emit runStatisticsChanged();
    pumpRecognitionQueue();
    if (usesPaddleLocalEngine() && m_operation == Operation::RecognizePaddleChunk) return;
    const bool busy = std::any_of(m_recognitionWorkers.cbegin(), m_recognitionWorkers.cend(),
                                  [](const RecognitionWorker &worker) { return worker.item; });
    if (!busy && m_recognitionQueue.isEmpty()) {
        m_chunkStartIndex = m_chunkEndIndex;
        // Do not start a new FFmpeg process from inside the current process'
        // finished signal.  Queuing preserves the event ordering and handles
        // an all-duplicate trailing chunk without leaving the job running.
        QTimer::singleShot(0, this, [this] {
            if (m_processing && !m_cancelRequested) beginNextChunk();
        });
    }
}

void SubtitleOcrController::pumpRecognitionQueue()
{
    if (!m_processing || m_executionRoute != QStringLiteral("local-cpu")) return;
    if (usesPaddleLocalEngine()) {
        beginPaddleRecognitionChunk();
        return;
    }
    if (m_recognitionWorkers.isEmpty()) {
        for (int index = 0; index < m_maxConcurrentWorkers; ++index) {
            RecognitionWorker worker;
            worker.process = new QProcess(this);
            connect(worker.process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                    [this, process = worker.process](int code, QProcess::ExitStatus status) {
                        onRecognitionFinished(process, code, status);
                    });
            connect(worker.process, &QProcess::errorOccurred, this,
                    [this, process = worker.process](QProcess::ProcessError error) {
                        onRecognitionError(process, error);
                    });
            m_recognitionWorkers.append(worker);
        }
    }
    const QString tesseract = runtimePath();
    if (tesseract.isEmpty()) {
        fail(QStringLiteral("Subtitle OCR runtime became unavailable during batch recognition."));
        return;
    }
    for (RecognitionWorker &worker : m_recognitionWorkers) {
        if (m_recognitionQueue.isEmpty()) break;
        if (worker.item || worker.process->state() != QProcess::NotRunning) continue;
        worker.item = m_recognitionQueue.dequeue();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        QStringList arguments{worker.item->framePath, QStringLiteral("stdout"), QStringLiteral("-l"),
                              m_ocrLanguage, QStringLiteral("--psm"), QStringLiteral("6"), QStringLiteral("tsv")};
        if (m_runtimeService) {
            arguments = m_runtimeService->tesseractDataArguments() + arguments;
            environment = m_runtimeService->tesseractProcessEnvironment();
        }
        ++m_ocrAttemptCount;
        ++m_tesseractProcessCount;
        setPhase(QStringLiteral("recognizing %1/%2 (%3 workers)")
                     .arg(m_completedSampleCount + 1).arg(m_samples.size()).arg(m_maxConcurrentWorkers));
        worker.process->setProgram(tesseract);
        worker.process->setArguments(arguments);
        worker.process->setProcessEnvironment(environment);
        worker.process->start();
    }
    emit runStatisticsChanged();
}

bool SubtitleOcrController::beginPaddleRecognitionChunk()
{
    if (m_recognitionQueue.isEmpty()) return false;
    const PaddleOcrRuntimeResolution paddle = PaddleOcrRuntimeLocator::resolve();
    if (!paddle.isUsable()) {
        fail(QStringLiteral("PaddleOCR runtime became unavailable during batch recognition."),
             Operation::RecognizePaddleChunk);
        return false;
    }

    m_paddleChunkItems.clear();
    QJsonArray frames;
    while (!m_recognitionQueue.isEmpty()) {
        const QSharedPointer<RecognitionItem> item = m_recognitionQueue.dequeue();
        m_paddleChunkItems.append(item);
        frames.append(QJsonObject{{QStringLiteral("hash"), QString::fromLatin1(item->frameHash.toHex())},
                                  {QStringLiteral("path"), item->framePath}});
    }
    const QString stem = QStringLiteral("paddle-chunk-%1-%2")
        .arg(m_chunkStartIndex).arg(m_chunkEndIndex);
    m_paddleRequestPath = QDir(m_workspacePath).filePath(stem + QStringLiteral(".request.json"));
    m_paddleResponsePath = QDir(m_workspacePath).filePath(stem + QStringLiteral(".response.json"));
    QSaveFile request(m_paddleRequestPath);
    const QJsonObject payload{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("engineId"), QString::fromLatin1(PaddleOcrRuntimeLocator::engineId())},
                              {QStringLiteral("language"), m_ocrLanguage},
                              {QStringLiteral("frames"), frames}};
    if (!request.open(QIODevice::WriteOnly)
        || request.write(QJsonDocument(payload).toJson(QJsonDocument::Compact)) < 0
        || !request.commit()) {
        fail(QStringLiteral("Cannot write the PaddleOCR batch request."), Operation::RecognizePaddleChunk);
        return false;
    }
    QFile::remove(m_paddleResponsePath);
    m_ocrAttemptCount += m_paddleChunkItems.size();
    ++m_paddleProcessCount;
    setPhase(QStringLiteral("recognizing PaddleOCR batch %1-%2/%3")
                 .arg(m_chunkStartIndex + 1).arg(m_chunkEndIndex).arg(m_samples.size()));
    emit runStatisticsChanged();
    startProcess(Operation::RecognizePaddleChunk, paddle.pythonPath,
                 {paddle.workerPath, QStringLiteral("--cache-root"), paddle.modelCachePath,
                  QStringLiteral("--manifest"), paddle.manifestPath,
                  QStringLiteral("--request"), m_paddleRequestPath,
                  QStringLiteral("--response"), m_paddleResponsePath});
    return true;
}

void SubtitleOcrController::onRecognitionError(QProcess *process, QProcess::ProcessError error)
{
    if (!m_processing || error == QProcess::Crashed) return;
    if (error == QProcess::FailedToStart)
        fail(QStringLiteral("Subtitle OCR worker could not be started."), Operation::RecognizeFrame);
    Q_UNUSED(process)
}

void SubtitleOcrController::onRecognitionFinished(QProcess *process, int exitCode, QProcess::ExitStatus status)
{
    auto worker = std::find_if(m_recognitionWorkers.begin(), m_recognitionWorkers.end(),
                               [process](const RecognitionWorker &candidate) { return candidate.process == process; });
    if (worker == m_recognitionWorkers.end() || !worker->item) return;
    const QSharedPointer<RecognitionItem> item = worker->item;
    worker->item.reset();
    const QByteArray output = process->readAllStandardOutput();
    const QByteArray standardError = process->readAllStandardError();
    if (!m_processing || m_cancelRequested) {
        QFile::remove(item->framePath);
        if (m_cancelRequested) completeCancellation();
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        fail(processFailure(QStringLiteral("batch Tesseract recognition"), standardError), Operation::RecognizeFrame);
        return;
    }
    const SubtitleOcrObservation recognized = SubtitleOcrPipeline::parseTesseractTsv(
        output, m_samples.at(item->sampleIndexes.constFirst()));
    item->completed = true;
    item->recognizedText = recognized.text;
    item->recognizedConfidence = recognized.confidence;
    ++m_ocrSuccessCount;
    ++m_recognizedFrameCount;
    for (const int index : item->sampleIndexes) {
        appendObservation({m_samples.at(index), recognized.text, recognized.confidence});
        ++m_completedSampleCount;
    }
    QFile::remove(item->framePath);
    updateForwardProgress();
    pumpRecognitionQueue();
    const bool busy = std::any_of(m_recognitionWorkers.cbegin(), m_recognitionWorkers.cend(),
                                  [](const RecognitionWorker &candidate) { return candidate.item; });
    if (!busy && m_recognitionQueue.isEmpty()) {
        m_chunkStartIndex = m_chunkEndIndex;
        beginNextChunk();
    }
}

void SubtitleOcrController::releaseRecognitionWorkers()
{
    for (RecognitionWorker &worker : m_recognitionWorkers) {
        if (!worker.process) continue;
        if (worker.process->state() != QProcess::NotRunning) {
            worker.process->kill();
            worker.process->waitForFinished(3000);
        }
        worker.process->deleteLater();
        worker.process = nullptr;
        worker.item.reset();
    }
    m_recognitionWorkers.clear();
    m_recognitionQueue.clear();
    m_paddleChunkItems.clear();
    m_paddleRequestPath.clear();
    m_paddleResponsePath.clear();
    m_uniqueFrames.clear();
}

void SubtitleOcrController::beginNextSample()
{
    if (!m_processing || m_cancelRequested) {
        completeCancellation();
        return;
    }
    if (m_sampleIndex >= m_samples.size()) {
        completeRun();
        return;
    }
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    const QStringList crop = SubtitleOcrPipeline::ffmpegCropArguments(m_roi, m_frameWidth, m_frameHeight);
    if (!media.hasFfmpeg() || crop.isEmpty()) {
        fail(QStringLiteral("Subtitle OCR frame extraction is no longer configured."), Operation::ExtractFrame);
        return;
    }
    const qint64 timestampMs = m_samples.at(m_sampleIndex);
    m_currentFramePath = QDir(m_workspacePath).filePath(
        QStringLiteral("frame-%1.png").arg(m_sampleIndex, 6, 10, QLatin1Char('0')));
    m_currentCrop = SubtitleOcrPipeline::sourceRect(m_roi, m_frameWidth, m_frameHeight);
    setPhase(QStringLiteral("extracting frame %1/%2").arg(m_sampleIndex + 1).arg(m_samples.size()));
    recordFrameExtractionStart(media, timestampMs, m_currentCrop);
    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                          QStringLiteral("-ss"), ffmpegTime(timestampMs), QStringLiteral("-i"), m_sourcePath};
    arguments += crop;
    arguments += {QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-y"), m_currentFramePath};
    startProcess(Operation::ExtractFrame, media.ffmpeg, arguments);
}

void SubtitleOcrController::beginRecognition()
{
    ++m_ocrAttemptCount;
    emit runStatisticsChanged();
    if (m_executionRoute == QStringLiteral("colab-gpu")) {
        if (!m_colabRunner || !m_colabSession) {
            fail(QStringLiteral("Colab Subtitle OCR worker is unavailable."));
            return;
        }
        QString routeError;
        if (!m_colabSession->hasVerifiedRoute(kColabSubtitleOcrCapability, m_colabModelId, &routeError)) {
            fail(routeError.isEmpty() ? QStringLiteral("Colab Subtitle OCR route is no longer verified.") : routeError);
            return;
        }
        QFile frame(m_currentFramePath);
        if (!frame.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("Subtitle OCR frame extraction did not produce a readable crop."));
            return;
        }
        const QByteArray croppedFrame = frame.readAll();
        if (croppedFrame.isEmpty() || croppedFrame.size() > 16 * 1024 * 1024) {
            fail(QStringLiteral("Subtitle OCR crop must be a non-empty PNG smaller than 16 MiB."));
            return;
        }
        setPhase(QStringLiteral("recognizing GPU frame %1/%2").arg(m_sampleIndex + 1).arg(m_samples.size()));
        m_operation = Operation::RecognizeColabFrame;
        if (!QMetaObject::invokeMethod(m_colabRunner, "recognize", Qt::QueuedConnection,
                                       Q_ARG(QUrl, m_colabSession->endpoint()),
                                       Q_ARG(QString, m_colabSession->bearerTokenForRequest()),
                                       Q_ARG(QString, m_colabModelId), Q_ARG(QString, m_ocrLanguage),
                                       Q_ARG(QByteArray, croppedFrame),
                                       Q_ARG(bool, m_colabSession->allowsInsecureLocalhostForTests()))) {
            fail(QStringLiteral("Could not dispatch the cropped frame to Colab Subtitle OCR."));
        }
        return;
    }
    const QString tesseract = runtimePath();
    if (tesseract.isEmpty()) {
        fail(QStringLiteral("Subtitle OCR runtime became unavailable during processing."));
        emit runtimeChanged();
        return;
    }
    setPhase(QStringLiteral("recognizing frame %1/%2").arg(m_sampleIndex + 1).arg(m_samples.size()));
    startProcess(Operation::RecognizeFrame, tesseract,
                 {m_currentFramePath, QStringLiteral("stdout"), QStringLiteral("-l"), m_ocrLanguage,
                  QStringLiteral("--psm"), QStringLiteral("6"), QStringLiteral("tsv")});
}

void SubtitleOcrController::onColabRecognitionFinished(const QString &text, double confidence)
{
    if (!m_processing || m_operation != Operation::RecognizeColabFrame) return;
    m_operation = Operation::None;
    if (m_cancelRequested) {
        completeCancellation();
        return;
    }
    const SubtitleOcrObservation observation{m_samples.at(m_sampleIndex), text.trimmed(),
                                             qBound(0.0, confidence, 1.0)};
    m_previousText = observation.text;
    m_previousConfidence = observation.confidence;
    ++m_ocrSuccessCount;
    appendObservation(observation);
    ++m_sampleIndex;
    setProgress((m_sampleIndex * 100) / m_samples.size(), true);
    beginNextSample();
}

void SubtitleOcrController::onColabRecognitionFailed(const QString &message)
{
    if (!m_processing || m_operation != Operation::RecognizeColabFrame) return;
    if (m_cancelRequested) {
        completeCancellation();
        return;
    }
    fail(message.isEmpty() ? QStringLiteral("Colab Subtitle OCR request failed.") : message);
}

void SubtitleOcrController::onProcessError(QProcess::ProcessError error)
{
    if (!m_processing || error == QProcess::Crashed) return;
    if (m_operation == Operation::ExtractFrame || m_operation == Operation::ExtractChunk) {
        appendDiagnostic(QStringLiteral("frame-extraction-process-error"),
                         QStringLiteral("ffmpeg=%1; output=%2; processError=%3; detail=%4")
                             .arg(m_process.program(), m_currentFramePath)
                             .arg(static_cast<int>(error)).arg(m_process.errorString()));
    }
    if (error == QProcess::FailedToStart) {
        fail(QStringLiteral("Subtitle OCR process could not be started. Check the managed runtime installation."),
             m_operation);
    }
}

void SubtitleOcrController::onFrameExtractionTimeout()
{
    if (!m_processing || (m_operation != Operation::ExtractFrame
                          && m_operation != Operation::ExtractChunk)) return;
    appendDiagnostic(QStringLiteral("frame-extraction-timeout"),
                     QStringLiteral("ffmpeg=%1; output=%2; timeoutMs=%3; state=%4")
                         .arg(m_process.program(), m_currentFramePath)
                         .arg(frameExtractionTimeoutMs())
                         .arg(static_cast<int>(m_process.state())));
    m_frameExtractionTimedOut = true;
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
}

void SubtitleOcrController::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_processing) return;
    const QByteArray output = m_process.readAllStandardOutput();
    const QByteArray standardError = m_process.readAllStandardError();
    const Operation operation = m_operation;
    if (operation == Operation::ExtractFrame || operation == Operation::ExtractChunk)
        m_frameExtractionTimeout.stop();
    m_operation = Operation::None;
    if (operation == Operation::ExtractFrame || operation == Operation::ExtractChunk) {
        appendDiagnostic(QStringLiteral("frame-extraction-exit"),
                         QStringLiteral("ffmpeg=%1; output=%2; exitCode=%3; exitStatus=%4; stderr=%5")
                             .arg(m_process.program(), m_currentFramePath).arg(exitCode)
                             .arg(status == QProcess::NormalExit ? QStringLiteral("normal")
                                                                  : QStringLiteral("crashed"))
                             .arg(boundedDiagnosticText(QString::fromUtf8(standardError))));
    }
    if (operation == Operation::Probe) {
        appendDiagnostic(QStringLiteral("probe-exit"),
                         QStringLiteral("ffprobe=%1; exitCode=%2; exitStatus=%3; stderr=%4")
                             .arg(m_process.program()).arg(exitCode)
                             .arg(status == QProcess::NormalExit ? QStringLiteral("normal")
                                                                  : QStringLiteral("crashed"))
                             .arg(boundedDiagnosticText(QString::fromUtf8(standardError))));
    }
    if (m_cancelRequested) {
        completeCancellation();
        return;
    }
    if ((operation == Operation::ExtractFrame || operation == Operation::ExtractChunk)
        && m_frameExtractionTimedOut) {
        m_frameExtractionTimedOut = false;
        fail(QStringLiteral("Subtitle OCR frame extraction timed out. Use Retry frame extraction or Open diagnostics."),
             operation);
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString stage = operation == Operation::Probe ? QStringLiteral("video probe")
            : operation == Operation::CropPreview ? QStringLiteral("crop preview")
            : operation == Operation::VerifyLanguage ? QStringLiteral("Tesseract language check")
            : operation == Operation::VerifyPaddleRuntime ? QStringLiteral("PaddleOCR runtime health check")
            : (operation == Operation::ExtractFrame || operation == Operation::ExtractChunk)
                ? QStringLiteral("frame extraction")
            : operation == Operation::RecognizePaddleChunk ? QStringLiteral("PaddleOCR batch recognition")
            : QStringLiteral("Tesseract recognition");
        fail(processFailure(stage, standardError), operation);
        return;
    }
    if (operation == Operation::VerifyPaddleRuntime) {
        QString healthError;
        if (!parsePaddleHealth(output, &healthError)) {
            fail(QStringLiteral("PaddleOCR runtime health check failed: %1").arg(healthError), operation);
            return;
        }
        appendDiagnostic(QStringLiteral("paddle-runtime-health"),
                         QStringLiteral("engine=%1; version=%2; result=passed")
                             .arg(PaddleOcrRuntimeLocator::engineId(), PaddleOcrRuntimeLocator::engineVersion()));
        beginOcrSamples();
        return;
    }
    if (operation == Operation::RecognizePaddleChunk) {
        QFile response(m_paddleResponsePath);
        const QJsonDocument responseDocument = response.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(response.readAll()) : QJsonDocument();
        const QJsonObject root = responseDocument.object();
        if (!responseDocument.isObject() || root.value(QStringLiteral("schemaVersion")).toInt() != 1
            || root.value(QStringLiteral("engineId")).toString()
                   != QString::fromLatin1(PaddleOcrRuntimeLocator::engineId())
            || root.value(QStringLiteral("engineVersion")).toString()
                   != QString::fromLatin1(PaddleOcrRuntimeLocator::engineVersion())
            || !root.value(QStringLiteral("manifestVerified")).toBool()) {
            fail(QStringLiteral("PaddleOCR returned an invalid batch response."), operation);
            return;
        }
        const QJsonObject telemetry = root.value(QStringLiteral("telemetry")).toObject();
        const double cpuSeconds = telemetry.value(QStringLiteral("cpuSeconds")).toDouble(-1.0);
        const qint64 peakWorkingSet = telemetry.value(QStringLiteral("peakWorkingSetBytes")).toVariant().toLongLong();
        if (telemetry.isEmpty() || !std::isfinite(cpuSeconds) || cpuSeconds < 0.0 || peakWorkingSet < 0) {
            fail(QStringLiteral("PaddleOCR batch response is missing valid runtime telemetry."), operation);
            return;
        }
        m_paddleCpuSeconds += cpuSeconds;
        m_paddlePeakWorkingSetBytes = qMax(m_paddlePeakWorkingSetBytes, peakWorkingSet);
        QHash<QByteArray, SubtitleOcrObservation> results;
        for (const QJsonValue &value : root.value(QStringLiteral("results")).toArray()) {
            const QJsonObject item = value.toObject();
            const QByteArray hash = QByteArray::fromHex(item.value(QStringLiteral("hash")).toString().toLatin1());
            const QString text = item.value(QStringLiteral("text")).toString().trimmed();
            const double confidence = item.value(QStringLiteral("confidence")).toDouble(-1.0);
            if (hash.isEmpty() || results.contains(hash) || !std::isfinite(confidence)
                || confidence < 0.0 || confidence > 1.0) {
                fail(QStringLiteral("PaddleOCR returned an invalid recognition result."), operation);
                return;
            }
            results.insert(hash, {0, text, confidence});
        }
        for (const QSharedPointer<RecognitionItem> &item : m_paddleChunkItems) {
            const auto result = results.constFind(item->frameHash);
            if (result == results.cend()) {
                fail(QStringLiteral("PaddleOCR did not return every requested cropped frame."), operation);
                return;
            }
            item->completed = true;
            item->recognizedText = result->text;
            item->recognizedConfidence = result->confidence;
            ++m_ocrSuccessCount;
            ++m_recognizedFrameCount;
            for (const int index : item->sampleIndexes) {
                appendObservation({m_samples.at(index), item->recognizedText, item->recognizedConfidence});
                ++m_completedSampleCount;
            }
            QFile::remove(item->framePath);
        }
        appendDiagnostic(QStringLiteral("paddle-batch-complete"),
                         QStringLiteral("frames=%1; engine=%2; version=%3")
                             .arg(m_paddleChunkItems.size()).arg(PaddleOcrRuntimeLocator::engineId(),
                                 PaddleOcrRuntimeLocator::engineVersion()));
        m_paddleChunkItems.clear();
        updateForwardProgress();
        m_chunkStartIndex = m_chunkEndIndex;
        beginNextChunk();
        return;
    }
    if (operation == Operation::Probe) {
        completeProbe(output);
        return;
    }
    if (operation == Operation::CropPreview) {
        if (!QFileInfo(m_cropPreviewPath).isFile() || QFileInfo(m_cropPreviewPath).size() <= 0) {
            fail(QStringLiteral("Subtitle OCR crop preview did not produce an image."), operation);
            return;
        }
        setProcessing(false);
        setPhase(QStringLiteral("ready"));
        setProgress(0, false);
        emit cropPreviewChanged();
        return;
    }
    if (operation == Operation::VerifyLanguage) {
        const QStringList installedLanguages = QString::fromUtf8(output).split(
            QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        QStringList missingLanguages;
        for (const QString &language : m_ocrLanguage.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
            if (!installedLanguages.contains(language)) missingLanguages.append(language);
        }
        if (!missingLanguages.isEmpty()) {
            fail(QStringLiteral("The selected Tesseract language data is not installed: %1. Install the matching traineddata file, refresh the OCR runtime, and try again.")
                 .arg(missingLanguages.join(QStringLiteral(", "))), operation);
            return;
        }
        beginOcrSamples();
        return;
    }
    if (operation == Operation::ExtractFrame) {
        QByteArray hash;
        QString validationError;
        if (!validateCurrentFrame(&hash, &validationError)) {
            fail(QStringLiteral("Subtitle OCR frame extraction did not produce a readable PNG crop: %1. "
                                "Use Retry frame extraction or Open diagnostics.")
                     .arg(validationError), operation);
            return;
        }
        ++m_readableCropCount;
        emit runStatisticsChanged();
        if (!m_previousFrameHash.isEmpty() && hash == m_previousFrameHash) {
            appendObservation({m_samples.at(m_sampleIndex), m_previousText, m_previousConfidence});
            ++m_sampleIndex;
            setProgress(qRound(100.0 * m_sampleIndex / m_samples.size()), true);
            beginNextSample();
            return;
        }
        m_previousFrameHash = hash;
        beginRecognition();
        return;
    }
    if (operation == Operation::ExtractChunk) {
        updateForwardProgress();
        queueChunkFrames();
        return;
    }
    if (operation == Operation::RecognizeFrame) {
        const SubtitleOcrObservation observation = SubtitleOcrPipeline::parseTesseractTsv(
            output, m_samples.at(m_sampleIndex));
        ++m_ocrSuccessCount;
        m_previousText = observation.text;
        m_previousConfidence = observation.confidence;
        appendObservation(observation);
        ++m_sampleIndex;
        setProgress(qRound(100.0 * m_sampleIndex / m_samples.size()), true);
        beginNextSample();
    }
}

void SubtitleOcrController::completeRun()
{
    m_forwardProgressTimer.stop();
    QVector<SubtitleOcrSegment> merged = SubtitleOcrPipeline::mergeObservations(
        m_observations, m_sampleIntervalMs, m_minimumConfidence);
    for (SubtitleOcrSegment &segment : merged) {
        segment.endMs = qMin(segment.endMs, m_durationMs);
    }
    QString validationError;
    const bool requireHanText = m_ocrLanguage.compare(QStringLiteral("chi_sim"), Qt::CaseInsensitive) == 0;
    if (!SubtitleOcrPipeline::validatePublishableSegments(merged, requireHanText, &validationError)) {
        m_segments.clear();
        m_publishedSegmentCount = 0;
        m_exportedSegmentCount = 0;
        emit segmentsChanged();
        emit runStatisticsChanged();
        appendDiagnostic(QStringLiteral("result-validation"),
                         QStringLiteral("status=no_text_detected; scheduled=%1; readableCrops=%2; "
                                        "ocrSuccesses=%3; rawNonEmpty=%4; filterCandidates=%5; "
                                        "publishedSegments=0; detail=%6")
                             .arg(m_scheduledSampleCount).arg(m_readableCropCount)
                             .arg(m_ocrSuccessCount).arg(m_nonEmptyRawResultCount)
                             .arg(m_filterCandidateCount).arg(validationError));
        fail(QStringLiteral("no_text_detected: %1").arg(validationError),
             Operation::RecognizeFrame, QStringLiteral("no_text_detected"));
        return;
    }
    m_segments = segmentsToVariant(merged);
    m_publishedSegmentCount = m_segments.size();
    m_exportedSegmentCount = 0;
    emit runStatisticsChanged();
    if (!storeCachedResult()) {
        appendDiagnostic(QStringLiteral("result-cache-write-failed"),
                         QStringLiteral("key=%1").arg(m_cacheKey));
    }
    releaseActiveCacheKey();
    releaseRecognitionWorkers();
    cleanWorkspace();
    setProcessing(false);
    setProgress(100, true);
    setPhase(QStringLiteral("completed"));
    setResultStatus(QStringLiteral("completed"));
    setError({});
    emit segmentsChanged();
}

void SubtitleOcrController::completeCancellation()
{
    m_frameExtractionTimeout.stop();
    m_forwardProgressTimer.stop();
    m_frameExtractionTimedOut = false;
    m_cancelRequested = false;
    m_operation = Operation::None;
    m_pendingSourcePath.clear();
    releaseRecognitionWorkers();
    releaseActiveCacheKey();
    cleanWorkspace();
    setProcessing(false);
    setProgress(0, false);
    setPhase(QStringLiteral("canceled"));
    setResultStatus(QStringLiteral("canceled"));
    setError({});
}

void SubtitleOcrController::fail(const QString &message, Operation failedOperation,
                                 const QString &resultStatus)
{
    const Operation recordedOperation = failedOperation == Operation::None
        ? m_operation : failedOperation;
    m_frameExtractionTimeout.stop();
    m_forwardProgressTimer.stop();
    m_frameExtractionTimedOut = false;
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    releaseRecognitionWorkers();
    releaseActiveCacheKey();
    if (m_operation == Operation::RecognizeColabFrame && m_colabRunner)
        QMetaObject::invokeMethod(m_colabRunner, "cancel", Qt::QueuedConnection);
    m_operation = Operation::None;
    m_cancelRequested = false;
    m_pendingSourcePath.clear();
    m_lastFailedOperation = recordedOperation;
    if (!m_segments.isEmpty()) {
        m_segments.clear();
        emit segmentsChanged();
    }
    m_publishedSegmentCount = 0;
    m_exportedSegmentCount = 0;
    emit runStatisticsChanged();
    cleanWorkspace(recordedOperation == Operation::ExtractFrame
                   || recordedOperation == Operation::ExtractChunk);
    setProcessing(false);
    setProgress(0, false);
    setPhase(QStringLiteral("error"));
    setResultStatus(resultStatus);
    setError(message);
    emit frameRetryChanged();
}

void SubtitleOcrController::cancel()
{
    if (!m_processing) return;
    m_cancelRequested = true;
    const bool primaryProcessRunning = m_process.state() != QProcess::NotRunning;
    const Operation operation = m_operation;
    if (primaryProcessRunning) m_process.kill();
    releaseRecognitionWorkers();
    if (primaryProcessRunning) return;
    if (operation == Operation::RecognizeColabFrame && m_colabRunner)
        QMetaObject::invokeMethod(m_colabRunner, "cancel", Qt::QueuedConnection);
    else completeCancellation();
}


