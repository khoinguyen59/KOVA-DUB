bool DubbingController::exportPackage(const QString &directoryPath)
{
    const QString outputDirectory = QFileInfo(PathUtils::urlToLocalPath(directoryPath)).absoluteFilePath();
    if (directoryPath.isEmpty() || outputDirectory.isEmpty()) {
        setError(QStringLiteral("Choose a package output folder."));
        return false;
    }
    if (!hasProject()) {
        setError(QStringLiteral("Save the dubbing project before exporting a package."));
        return false;
    }

    QDir directory(outputDirectory);
    if (!directory.mkpath(QStringLiteral(".")) || !directory.mkpath(QStringLiteral("clips"))) {
        setError(QStringLiteral("Cannot create package folder: %1").arg(outputDirectory));
        return false;
    }

    QString error;
    if (!m_project.save(&error)
        || !replaceCopy(m_project.projectPath, directory.filePath(QStringLiteral("project.ladub.json")), &error)
        || !replaceCopy(previewPath(), directory.filePath(QStringLiteral("dubbed-mix.wav")), &error)
        || !replaceCopy(dubbedVocalPath(), directory.filePath(QStringLiteral("dubbed-vocals.wav")), &error)
        || !replaceCopy(m_project.vocalsAudioPath, directory.filePath(QStringLiteral("source-vocals.wav")), &error)
        || !replaceCopy(m_project.backgroundAudioPath, directory.filePath(QStringLiteral("background.wav")), &error)) {
        setError(error);
        return false;
    }

    int exportedClips = 0;
    for (int i = 0; i < m_project.segments.size(); ++i) {
        const QString clipPath = m_project.segments.at(i).toMap().value(QStringLiteral("clipPath")).toString();
        if (clipPath.isEmpty() || !QFileInfo(clipPath).isFile()) continue;
        const QString suffix = QFileInfo(clipPath).suffix().isEmpty()
            ? QStringLiteral("wav") : QFileInfo(clipPath).suffix();
        const QString clipName = QStringLiteral("%1.%2").arg(i + 1, 4, 10, QLatin1Char('0')).arg(suffix);
        if (!replaceCopy(clipPath, directory.filePath(QStringLiteral("clips/") + clipName), &error)) {
            setError(error);
            return false;
        }
        ++exportedClips;
    }

    const QString sourceSubtitlePath = directory.filePath(QStringLiteral("source.srt"));
    const QString dubbedSubtitlePath = directory.filePath(QStringLiteral("dubbed.srt"));
    if (!writeDubbingSubtitles(m_project.segments, sourceSubtitlePath, false, nullptr))
        QFile::remove(sourceSubtitlePath);
    if (!writeDubbingSubtitles(m_project.segments, dubbedSubtitlePath, true, nullptr))
        QFile::remove(dubbedSubtitlePath);

    const QJsonObject manifest{
        {QStringLiteral("format"), QStringLiteral("la-studio-dubbing-package")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("project"), QStringLiteral("project.ladub.json")},
        {QStringLiteral("sourceMediaPath"), m_project.sourceMediaPath},
        {QStringLiteral("segmentCount"), m_project.segments.size()},
        {QStringLiteral("exportedClipCount"), exportedClips}
    };
    QSaveFile manifestFile(directory.filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0
        || !manifestFile.commit()) {
        setError(QStringLiteral("Cannot write package manifest: %1").arg(manifestFile.errorString()));
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::importSubtitles(const QString &path, const QString &untimedStrategy)
{
    return importSubtitlesInternal(path, untimedStrategy, false);
}

bool DubbingController::importSubtitlesInternal(const QString &path,
                                                const QString &untimedStrategy,
                                                bool allowIndependentTranscriptWorker)
{
    const bool onlyIndependentTranscriptWorkersActive =
        !m_automaticSetupActive && !m_mediaQueueProcessing
        && !(m_translationFix && m_translationFix->busy())
        && !(m_workflowRunner && m_workflowRunner->running())
        && ((m_runner && m_runner->processing()
             && m_runner->stage() == QStringLiteral("transcribe"))
            || subtitleOcrProcessing());
    if (processing() && !(allowIndependentTranscriptWorker
                          && onlyIndependentTranscriptWorkersActive)) {
        setBusyError(QStringLiteral("Wait for the active Dubbing operation before importing subtitles."));
        return false;
    }
    if (!hasProject()) {
        setError(QStringLiteral("Open a Dubbing project before importing subtitles."));
        return false;
    }
    const QString localPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (path.isEmpty() || !QFileInfo(localPath).isFile()) {
        setError(QStringLiteral("Choose an existing SRT, VTT, ASS/SSA, TXT or Markdown subtitle file."));
        return false;
    }
    QVariantList imported;
    bool hasTiming = false;
    QString format;
    QString error;
    if (!DubbingSubtitleService::importFile(localPath, imported, hasTiming, format, &error)) {
        setError(error);
        return false;
    }
    QVariantList replacement = imported;
    if (!hasTiming) {
        if (untimedStrategy != QStringLiteral("existing-segment")) {
            setError(QStringLiteral("TXT/Markdown has no timestamps. Select line-per-existing-segment or run forced alignment before import; LA Studio will not invent timing."));
            return false;
        }
        if (!DubbingSubtitleService::mapUntimedLines(imported, m_project.segments, replacement, &error)) {
            setError(error);
            return false;
        }
    }
    m_project.segments = replacement;
    QVariantMap configuration = this->subtitleConfiguration();
    configuration.insert(QStringLiteral("source"), QStringLiteral("imported-") + format);
    configuration.insert(QStringLiteral("hasOriginalTiming"), hasTiming);
    configuration.insert(QStringLiteral("untimedMapping"), hasTiming ? QString() : untimedStrategy);
    configuration.insert(QStringLiteral("importedFileName"), QFileInfo(localPath).fileName());
    m_project.subtitleConfiguration = configuration;
    clearError();
    emit segmentsChanged();
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

QVariantMap DubbingController::workflowArtifactSpec(const QString &nodeId) const
{
    const QString requestedId = nodeId.trimmed().toLower();
    const bool explicitSttRoute = requestedId == QStringLiteral("stt")
        || requestedId == QStringLiteral("transcribe-stt");
    QString id = artifactProductionNodeId(requestedId);
    if (id == QStringLiteral("review-translation")) id = QStringLiteral("translate");
    QVariantMap spec = workflowArtifactSpecForNode(id);
    // The visible Transcribe task can be driven by OCR instead of audio STT.
    // Reflect that choice in the handoff instructions rather than asking the
    // operator to guess which Colab output belongs to the current route.
    if (id == QStringLiteral("transcribe") && !explicitSttRoute) {
        const QString source = normalizedTranscriptSource(
            m_project.transcriptConfiguration.value(
                QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
        if (source == QStringLiteral("ocr")) {
            spec = workflowArtifactSpecForNode(QStringLiteral("subtitle-ocr"));
        } else if (source == QStringLiteral("reconcile")) {
            spec = workflowArtifactSpecForNode(QStringLiteral("review-transcript"));
        }
        if (!spec.isEmpty()) spec.insert(QStringLiteral("nodeId"), id);
    }
    if (explicitSttRoute && !spec.isEmpty())
        spec.insert(QStringLiteral("nodeId"), requestedId);
    if (id == QStringLiteral("source-separate") && !spec.isEmpty()) {
        const QVariantMap configuration = m_workflowNodeConfigurations.value(id).toMap();
        const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
        const QString transferFormat = parameters.value(QStringLiteral("artifactTransferFormat"),
                                                        QStringLiteral("flac")).toString().trimmed().toLower()
            == QStringLiteral("wav") ? QStringLiteral("wav") : QStringLiteral("flac");
        const QString vocalsName = QStringLiteral("vocals.") + transferFormat;
        const QString backgroundName = QStringLiteral("background.") + transferFormat;
        spec.insert(QStringLiteral("artifactTransferFormat"), transferFormat);
        spec.insert(QStringLiteral("description"), QStringLiteral(
            "Upload the two %1 stems saved by the Spleeter/UVR Colab notebook. Both files are required. %2")
                        .arg(transferFormat.toUpper(), transferFormat == QStringLiteral("flac")
                             ? QStringLiteral("FLAC is lossless and greatly reduces Colab transfer size.")
                             : QStringLiteral("WAV is uncompressed for compatibility.")));
        spec.insert(QStringLiteral("workerPath"), QStringLiteral(
            "In Colab Files, open la-studio-separation-jobs/<model-id>/<job-id>/ and download %1 plus %2. The job-id directory is created by this run; source.wav is input and must not be uploaded.")
                        .arg(vocalsName, backgroundName));
        spec.insert(QStringLiteral("expectedFiles"), QStringList{vocalsName, backgroundName});
        spec.insert(QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".") + transferFormat});
        const QString modelId = configuration.value(
            QStringLiteral("modelId"), parameters.value(QStringLiteral("modelId"))).toString().trimmed();
        if (!modelId.isEmpty()) {
            spec.insert(QStringLiteral("colabFolder"),
                        spec.value(QStringLiteral("colabFolder")).toString().replace(
                            QStringLiteral("<model-id>"), modelId));
            spec.insert(QStringLiteral("workerPath"),
                        spec.value(QStringLiteral("workerPath")).toString().replace(
                            QStringLiteral("<model-id>"), modelId));
        }
    }
    return spec;
}

QVariantList DubbingController::workflowArtifactSpecsForStage(const QString &nodeId) const
{
    const QString id = nodeId.trimmed().toLower();
    QStringList productionIds;
    if (id == QStringLiteral("ingest") || id == QStringLiteral("normalize")) {
        productionIds << QStringLiteral("ingest");
    } else if (id == QStringLiteral("source-separate") || id == QStringLiteral("isolator")
               || id == QStringLiteral("separate") || id == QStringLiteral("source-separation")) {
        productionIds << QStringLiteral("source-separate");
    } else if (id == QStringLiteral("stt") || id == QStringLiteral("transcribe-stt")) {
        productionIds << QStringLiteral("stt");
    } else if (id == QStringLiteral("ocr")) {
        productionIds << QStringLiteral("ocr");
    } else if (id == QStringLiteral("transcribe")) {
        // workflowArtifactSpec deliberately keeps the presentation id
        // "transcribe" for the visible task.  Resolve the underlying
        // artifact separately, otherwise an OCR/review upload would be
        // displayed as a generic STT contract and rejected at import time.
        const QString source = normalizedTranscriptSource(
            m_project.transcriptConfiguration.value(
                QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
        if (source == QStringLiteral("reconcile")) {
            // STT and OCR are independent inputs.  Reconciliation happens
            // only after both have been saved, so offline handoff must expose
            // both upload controls instead of a blank/single review picker.
            productionIds << QStringLiteral("stt") << QStringLiteral("ocr");
        } else {
            productionIds << (source == QStringLiteral("ocr")
                              ? QStringLiteral("subtitle-ocr")
                              : QStringLiteral("transcribe"));
        }
    } else if (id == QStringLiteral("review-transcript")) {
        productionIds << QStringLiteral("review-transcript");
    } else if (id == QStringLiteral("fit-timing") || id == QStringLiteral("alignment-subtitle")
               || id == QStringLiteral("alignment")) {
        productionIds << QStringLiteral("fit-timing");
    } else if (id == QStringLiteral("translate") || id == QStringLiteral("review-translation")) {
        productionIds << QStringLiteral("translate");
    } else if (id == QStringLiteral("synthesize") || id == QStringLiteral("tts")) {
        productionIds << QStringLiteral("synthesize");
    } else if (id == QStringLiteral("mix")) {
        productionIds << QStringLiteral("mix");
    } else if (id == QStringLiteral("export")
               || id == QStringLiteral("export-output")) {
        // Export accepts its final rendered artifact only.  Mixing is an
        // internal predecessor, and surfacing it here made a manual export
        // handoff ambiguous (and could incorrectly offer two file pickers).
        productionIds << QStringLiteral("export");
    }

    QVariantList result;
    for (const QString &productionId : productionIds) {
        QVariantMap spec = workflowArtifactSpec(productionId);
        if (!spec.isEmpty()) result.append(spec);
    }
    return result;
}

bool DubbingController::canOverrideRunningWorkflowArtifact(const QString &nodeId) const
{
    const QString requested = artifactProductionNodeId(nodeId);
    if (requested == QStringLiteral("subtitle-ocr"))
        return m_independentSubtitleOcrActive
            || (m_subtitleOcr && m_subtitleOcr->processing());
    if (!m_runner || !m_runner->processing()) return false;
    return artifactMatchesActiveStage(nodeId, m_runner->stage());
}

QVariantMap DubbingController::workflowArtifactHandoffStatus(const QString &nodeId) const
{
    const bool active = canOverrideRunningWorkflowArtifact(nodeId);
    QVariantMap result{{QStringLiteral("active"), active},
                       {QStringLiteral("canOverride"), active}};
    if (!active || !m_runner) return result;

    result.insert(QStringLiteral("runnerStage"), m_runner->stage());
    const QString status = m_runner->activityStatus().trimmed();
    if (!status.isEmpty()) result.insert(QStringLiteral("status"), status);
    const QVariantMap transfer = m_runner->activityTransferProgress();
    if (!transfer.isEmpty()) result.insert(QStringLiteral("artifactTransfer"), transfer);
    // Manual Dubbing nodes do not have a universally measurable unit of
    // work.  Only expose a percentage for a byte-counted artifact transfer.
    result.insert(QStringLiteral("progressAvailable"),
                  transfer.value(QStringLiteral("available")).toBool());
    if (transfer.value(QStringLiteral("available")).toBool())
        result.insert(QStringLiteral("progress"), transfer.value(QStringLiteral("percent")));
    return result;
}

bool DubbingController::importWorkflowArtifactFiles(const QString &nodeId,
                                                     const QVariantList &paths)
{
    if (!hasProject()) {
        setError(QStringLiteral("Open or create a Dubbing project before importing a workflow output."));
        return false;
    }

    const QString requestedId = nodeId.trimmed().toLower();
    const bool explicitSttRoute = requestedId == QStringLiteral("stt")
        || requestedId == QStringLiteral("transcribe-stt");
    QString id = artifactProductionNodeId(requestedId);
    // The visible Transcribe presentation task maps to its active exact
    // source contract.  Persist/import under that contract id so a STT+OCR
    // review can never be mistaken for a raw STT or OCR fallback.
    if (id == QStringLiteral("transcribe") && !explicitSttRoute) {
        const QString source = normalizedTranscriptSource(
            m_project.transcriptConfiguration.value(
                QStringLiteral("transcriptSource"), QStringLiteral("stt")).toString());
        if (source == QStringLiteral("ocr"))
            id = QStringLiteral("subtitle-ocr");
        else if (source == QStringLiteral("reconcile"))
            id = QStringLiteral("review-transcript");
    }
    // Resolve the contract from the original presentation id.  This keeps
    // an explicit offline STT upload as STT even when the visible task is in
    // the combined STT + OCR mode, where a generic "transcribe" id means the
    // later reconciliation artifact.
    const QVariantMap spec = workflowArtifactSpec(requestedId);
    if (spec.isEmpty()) {
        setError(QStringLiteral("This Dubbing task does not accept a manual workflow output."));
        return false;
    }
    const bool overrideRunningWorker = canOverrideRunningWorkflowArtifact(id);
    if (processing() && !overrideRunningWorker) {
        setBusyError(QStringLiteral("A different Dubbing task is active. Upload may replace only the output of the currently-running task."));
        return false;
    }

    QStringList sourcePaths;
    for (const QVariant &value : paths) {
        const QString candidate = QFileInfo(PathUtils::urlToLocalPath(value.toString())).absoluteFilePath();
        if (candidate.isEmpty() || !QFileInfo(candidate).isFile()) {
            setError(QStringLiteral("Every selected workflow output must be an existing local file."));
            return false;
        }
        if (!sourcePaths.contains(candidate, Qt::CaseInsensitive)) sourcePaths.append(candidate);
    }

    const bool multiple = spec.value(QStringLiteral("multiple")).toBool();
    const int expectedCount = multiple ? 2 : 1;
    if (sourcePaths.size() != expectedCount) {
        setError(multiple
                     ? QStringLiteral("Voice isolation requires exactly the declared vocals and background stem files.")
                     : QStringLiteral("This task requires exactly one output file; select only the declared workflow artifact."));
        return false;
    }

    const QStringList allowedExtensions = spec.value(QStringLiteral("allowedExtensions")).toStringList();
    for (const QString &sourcePath : sourcePaths) {
        const QString extension = QStringLiteral(".") + QFileInfo(sourcePath).suffix().toLower();
        if (!allowedExtensions.contains(extension)) {
            setError(QStringLiteral("Unsupported workflow output format '%1'. Allowed formats: %2.")
                         .arg(extension, allowedExtensions.join(QStringLiteral(", "))));
            return false;
        }
    }
    const QStringList expectedFiles = spec.value(QStringLiteral("expectedFiles")).toStringList();
    if (id == QStringLiteral("source-separate")) {
        QSet<QString> names;
        for (const QString &sourcePath : sourcePaths)
            names.insert(QFileInfo(sourcePath).fileName().toLower());
        const QStringList exactNames = spec.value(QStringLiteral("expectedFiles")).toStringList();
        if (names.size() != 2 || exactNames.size() != 2
            || !names.contains(exactNames.at(0).toLower())
            || !names.contains(exactNames.at(1).toLower())) {
            setError(QStringLiteral("Voice isolation outputs must be named exactly %1 and %2.")
                         .arg(exactNames.value(0), exactNames.value(1)));
            return false;
        }
    } else if (!expectedFiles.isEmpty()) {
        const QString expected = expectedFiles.constFirst().section(QStringLiteral(" "), 0, 0)
            .section(QStringLiteral("("), 0, 0).trimmed().toLower();
        const QString actual = QFileInfo(sourcePaths.constFirst()).fileName().toLower();
        const QString expectedBase = QFileInfo(expected).completeBaseName().toLower();
        const QString actualBase = QFileInfo(actual).completeBaseName().toLower();
        if (!expectedBase.isEmpty() && actualBase != expectedBase) {
            setError(QStringLiteral("This task accepts only '%1' (with one of the declared extensions), not '%2'.")
                         .arg(expected, QFileInfo(sourcePaths.constFirst()).fileName()));
            return false;
        }
    }

    QString projectStem = QFileInfo(m_project.projectPath).completeBaseName();
    projectStem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
    if (projectStem.isEmpty()) projectStem = QStringLiteral("project");
    const QString destinationRoot = QDir(PathUtils::cacheDir()).filePath(
        QStringLiteral("dubbing-artifacts/%1/%2").arg(projectStem, id));
    if (!QDir().mkpath(destinationRoot)) {
        setError(QStringLiteral("Cannot create the Dubbing artifact cache: %1").arg(destinationRoot));
        return false;
    }

    QStringList copiedPaths;
    QString copyError;
    for (const QString &sourcePath : sourcePaths) {
        const QString destination = QDir(destinationRoot).filePath(QFileInfo(sourcePath).fileName());
        if (!replaceCopy(sourcePath, destination, &copyError)) {
            setError(copyError);
            return false;
        }
        copiedPaths.append(destination);
    }

    // The manual handoff is an override, not a second execution route.  Do
    // this only after the local files have passed the exact count/name/format
    // checks above, so an invalid picker choice cannot interrupt the worker.
    const auto cancelMatchingWorker = [this, id, overrideRunningWorker]() {
        if (!overrideRunningWorker) return;
        if (id == QStringLiteral("subtitle-ocr")) {
            if (!m_subtitleOcr || !subtitleOcrProcessing()) return;
            Logger::info(QStringLiteral("DubbingController"),
                         QStringLiteral("Manual OCR artifact accepted; cancelling the active OCR worker only."));
            m_independentSubtitleOcrActive = false;
            m_independentSubtitleOcrLoadingSource = false;
            m_independentSubtitleOcrSourcePath.clear();
            m_subtitleOcr->cancel();
            emit subtitleOcrProcessingChanged();
            emit processingChanged();
            emit workflowChanged();
            return;
        }
        if (!m_runner || !m_runner->processing()) return;
        Logger::info(QStringLiteral("DubbingController"),
                     QStringLiteral("Manual workflow artifact accepted; cancelling active worker transfer before continuing."));
        // A full automatic graph has its own scheduler.  A manual artifact
        // replaces exactly one graph node, so pause that scheduler first and
        // continue from the next visible step instead of letting it retry or
        // download the artifact the operator has already supplied.
        const bool automaticWorkflow = m_workflowMode == QStringLiteral("automatic")
            || (m_workflowRunner && m_workflowRunner->running());
        if (m_workflowRunner && m_workflowRunner->running())
            m_workflowRunner->cancel();
        if (automaticWorkflow) {
            m_automaticSetupActive = false;
            m_automaticOutputPath.clear();
            m_automaticDownloadsQueued.clear();
            m_automaticDownloadKeys.clear();
            m_automaticConfiguredNodes.clear();
            m_automaticSetupNodeId.clear();
            setWorkflowMode(QStringLiteral("step"));
            setAutomaticStatus(QStringLiteral("Manual workflow output accepted; automatic transfer stopped. Continue at the next task."));
            appendAutomaticEvent(QStringLiteral("Manual workflow output replaced active transfer"),
                                 QStringLiteral("completed"), currentStepId());
        }
        m_runner->cancel();
    };

    if (id == QStringLiteral("ingest")) {
        if (m_project.sourceMediaPath.isEmpty()) {
            setError(QStringLiteral("Choose source media before importing normalized.wav."));
            return false;
        }
        cancelMatchingWorker();
        m_project.masterAudioPath = copiedPaths.constFirst();
        m_project.analysisAudioPath = copiedPaths.constFirst();
        m_project.vocalsAudioPath.clear();
        m_project.backgroundAudioPath.clear();
        m_runner->setBackgroundAudioPath(QString());
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("audio"), copiedPaths.constFirst()},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    } else if (id == QStringLiteral("source-separate")) {
        QString vocalsPath;
        QString backgroundPath;
        for (const QString &path : copiedPaths) {
            if (QFileInfo(path).fileName().startsWith(QStringLiteral("vocals."), Qt::CaseInsensitive))
                vocalsPath = path;
            else if (QFileInfo(path).fileName().startsWith(QStringLiteral("background."), Qt::CaseInsensitive))
                backgroundPath = path;
        }
        cancelMatchingWorker();
        m_project.vocalsAudioPath = vocalsPath;
        m_project.backgroundAudioPath = backgroundPath;
        m_runner->setBackgroundAudioPath(backgroundPath);
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("vocals"), vocalsPath},
                                             {QStringLiteral("background"), backgroundPath},
                                             {QStringLiteral("path"), destinationRoot},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    } else if (id == QStringLiteral("transcribe") || id == QStringLiteral("subtitle-ocr")
               || id == QStringLiteral("review-transcript")) {
        cancelMatchingWorker();
        if (!importSubtitlesInternal(copiedPaths.constFirst(), QStringLiteral("existing-segment"), true)) return false;
        if (id == QStringLiteral("transcribe")) {
            m_project.transcriptConfiguration.insert(QStringLiteral("sttSegments"), m_project.segments);
        } else if (id == QStringLiteral("subtitle-ocr")) {
            m_project.transcriptConfiguration.insert(QStringLiteral("ocrSegments"), m_project.segments);
        } else if (id == QStringLiteral("review-transcript")) {
            m_project.transcriptConfiguration.insert(QStringLiteral("transcriptSource"),
                                                     QStringLiteral("reconcile"));
            m_project.transcriptConfiguration.insert(QStringLiteral("reconciledSegments"), m_project.segments);
            m_project.transcriptConfiguration.insert(QStringLiteral("manualReviewedArtifact"), true);
        }
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    } else if (id == QStringLiteral("translate")) {
        QVariantList imported;
        bool hasTiming = false;
        QString format;
        QString importError;
        if (!DubbingSubtitleService::importFile(copiedPaths.constFirst(), imported, hasTiming, format, &importError)) {
            setError(importError);
            return false;
        }
        QVariantList targetLines = imported;
        if (!hasTiming) {
            if (!DubbingSubtitleService::mapUntimedLines(imported, m_project.segments, targetLines, &importError)) {
                setError(importError);
                return false;
            }
        }
        if (targetLines.size() != m_project.segments.size()) {
            setError(QStringLiteral("The translated workflow output has %1 cues, but this transcript has %2. No rows were changed.")
                         .arg(targetLines.size()).arg(m_project.segments.size()));
            return false;
        }
        QVariantList updated = m_project.segments;
        for (int index = 0; index < updated.size(); ++index) {
            const QString text = targetLines.at(index).toMap().value(QStringLiteral("sourceText")).toString().trimmed();
            if (text.isEmpty()) {
                setError(QStringLiteral("The translated workflow output contains an empty cue at row %1.").arg(index + 1));
                return false;
            }
            QVariantMap segment = updated.at(index).toMap();
            segment.insert(QStringLiteral("targetText"), text);
            segment.insert(QStringLiteral("translationSource"), QStringLiteral("manual-colab-upload"));
            segment.insert(QStringLiteral("state"), QStringLiteral("translated"));
            updated[index] = segment;
        }
        cancelMatchingWorker();
        m_project.segments = updated;
        m_project.subtitleConfiguration.insert(QStringLiteral("translationSource"), QStringLiteral("manual-colab-upload"));
        m_project.subtitleConfiguration.insert(QStringLiteral("translatedFileName"), QFileInfo(copiedPaths.constFirst()).fileName());
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
        emit segmentsChanged();
    } else if (id == QStringLiteral("synthesize") || id == QStringLiteral("fit-timing")) {
        cancelMatchingWorker();
        m_runner->setDubbedVocalPath(copiedPaths.constFirst());
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("audio"), copiedPaths.constFirst()},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    } else if (id == QStringLiteral("mix")) {
        cancelMatchingWorker();
        m_runner->setPreviewPath(copiedPaths.constFirst());
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("audio"), copiedPaths.constFirst()},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    } else if (id == QStringLiteral("export")) {
        cancelMatchingWorker();
        m_runner->setExportPath(copiedPaths.constFirst());
        m_stepOutputs.insert(id, QVariantMap{{QStringLiteral("manualUpload"), true},
                                             {QStringLiteral("path"), copiedPaths.constFirst()},
                                             {QStringLiteral("colabFolder"), spec.value(QStringLiteral("colabFolder"))},
                                             {QStringLiteral("workerPath"), spec.value(QStringLiteral("workerPath"))}});
    }

    // subtitle-ocr and review-transcript are exact artifact variants of the
    // visible Transcribe/STT stage.  Advance the visible stage, never an
    // internal variant name that the manual workflow does not expose.
    const QString completedStepId = (id == QStringLiteral("subtitle-ocr")
                                     || id == QStringLiteral("review-transcript"))
        ? QStringLiteral("transcribe") : id;
    m_lastCompletedStepId = completedStepId;
    clearError();
    emit projectChanged();
    emit previewChanged();
    emit exportChanged();
    emit workflowChanged();
    persistAfterEdit();
    if (m_workflowMode == QStringLiteral("step")) advanceManualStep(completedStepId);
    return true;
}

bool DubbingController::setSubtitleStyle(const QVariantMap &style)
{
    QVariantMap normalized;
    QString error;
    if (!DubbingSubtitleService::normalizeStyle(style, normalized, &error)) {
        setError(error);
        return false;
    }
    QVariantMap configuration = this->subtitleConfiguration();
    configuration.insert(QStringLiteral("style"), normalized);
    m_project.subtitleConfiguration = configuration;
    clearError();
    emit projectChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::setSubtitleTextSource(const QString &source)
{
    const QString normalized = source.trimmed().toLower();
    if (normalized != QStringLiteral("source") && normalized != QStringLiteral("target")) {
        setError(QStringLiteral("Subtitle text source must be source or target."));
        return false;
    }
    QVariantMap configuration = this->subtitleConfiguration();
    configuration.insert(QStringLiteral("textSource"), normalized);
    m_project.subtitleConfiguration = configuration;
    clearError();
    emit projectChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::setSubtitleBurnIn(bool enabled)
{
    QVariantMap configuration = this->subtitleConfiguration();
    configuration.insert(QStringLiteral("burnIn"), enabled);
    m_project.subtitleConfiguration = configuration;
    clearError();
    emit projectChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::exportCapCutDraft(const QString &directoryPath)
{
    const QString outputDirectory = QFileInfo(PathUtils::urlToLocalPath(directoryPath)).absoluteFilePath();
    if (directoryPath.isEmpty() || outputDirectory.isEmpty()) {
        setError(QStringLiteral("Choose a parent folder for the CapCut draft."));
        return false;
    }
    if (!hasProject()) {
        setError(QStringLiteral("Save the dubbing project before exporting a CapCut draft."));
        return false;
    }
    QString error;
    if (!m_project.save(&error)) {
        setError(error);
        return false;
    }
    QString draftPath;
    QString warning;
    // analysisAudioPath is the normalized mono analysis input immediately
    // after ingest. It is never a source-vocals stem. Only the explicit
    // separated vocals field may be exported as editable source vocals.
    const bool hasSeparatedStems = !m_project.vocalsAudioPath.trimmed().isEmpty()
        && QFileInfo(m_project.vocalsAudioPath).isFile()
        && !m_project.backgroundAudioPath.trimmed().isEmpty()
        && QFileInfo(m_project.backgroundAudioPath).isFile();
    const QString vocalsStemPath = hasSeparatedStems ? m_project.vocalsAudioPath : QString();
    if (!CapCutDraftExporter::exportDraft(
            outputDirectory, QFileInfo(m_project.projectPath).completeBaseName(),
            m_project.sourceMediaPath, m_project.masterAudioPath, m_project.backgroundAudioPath,
            previewPath(), m_project.sourceIsVideo, m_project.sourceDurationMs,
            m_project.segments, vocalsStemPath, this->subtitleConfiguration(),
            this->timingConfiguration(), &draftPath, &warning, &error)) {
        setError(error);
        return false;
    }
    m_capCutDraftPath = draftPath;
    m_capCutDraftWarning = warning;
    clearError();
    emit exportChanged();
    return true;
}

bool DubbingController::openCapCutDraft()
{
    const QString draftPath = QFileInfo(
        PathUtils::urlToLocalPath(m_capCutDraftPath)).absoluteFilePath();
    if (m_capCutDraftPath.trimmed().isEmpty() || !QFileInfo(draftPath).isDir()) {
        setError(QStringLiteral(
            "Export the CapCut draft first, then use Open in CapCut."));
        return false;
    }

    // CapCut does not publish a stable Windows command-line contract for
    // opening an editable draft. Try the conventional path argument when the
    // desktop executable is installed, then keep the draft folder available as
    // a deterministic fallback. The UI/report must not claim that CapCut has
    // imported the draft until CapCut itself confirms that contract.
    QStringList candidates;
    candidates << QStandardPaths::findExecutable(QStringLiteral("CapCut.exe"));
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString programFiles = qEnvironmentVariable("PROGRAMFILES");
    const QString programFilesX86 = qEnvironmentVariable("PROGRAMFILES(X86)");
    for (const QString &base : {localAppData, programFiles, programFilesX86}) {
        if (base.isEmpty()) continue;
        candidates << QDir(base).filePath(QStringLiteral("CapCut/Apps/CapCut.exe"));
        candidates << QDir(base).filePath(QStringLiteral("CapCut/CapCut.exe"));
    }

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFileInfo(candidate).isFile()) continue;
        if (QProcess::startDetached(candidate, {draftPath})) {
            m_capCutDraftWarning = QStringLiteral(
                "CapCut opened. If the draft is not loaded automatically, import the draft folder from CapCut.");
            emit exportChanged();
            clearError();
            return true;
        }
    }

    const bool openedFolder = QDesktopServices::openUrl(
        QUrl::fromLocalFile(draftPath));
    m_capCutDraftWarning = openedFolder
        ? QStringLiteral("CapCut was not found; the draft folder was opened.")
        : QStringLiteral("CapCut was not found and the draft folder could not be opened.");
    emit exportChanged();
    if (!openedFolder) {
        setError(m_capCutDraftWarning);
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::renderPreview(const QString &path)
{
    const QFileInfo backgroundInfo(m_project.backgroundAudioPath);
    const QFileInfo masterInfo(m_project.masterAudioPath);
    const QString backgroundPath = backgroundInfo.isFile() && backgroundInfo.size() > 0
        ? m_project.backgroundAudioPath
        : (masterInfo.isFile() && masterInfo.size() > 0 ? m_project.masterAudioPath : QString());
    if (backgroundPath.isEmpty()) {
        setError(QStringLiteral(
            "Normalized source audio is missing or unreadable. Complete Normalize "
            "before mixing; Separate is optional."));
        return false;
    }
    m_runner->setBackgroundAudioPath(backgroundPath);
    return m_runner->renderPreview(m_project.segments, m_project.projectPath, path,
                                   audioMixConfiguration());
}
