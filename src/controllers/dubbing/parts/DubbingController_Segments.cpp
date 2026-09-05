bool DubbingController::replaceTranscriptSegments(const QVariantList &ocrSegments)
{
    if (processing()) {
        setBusyError(QStringLiteral("Wait for the current Dubbing operation before replacing its transcript."));
        return false;
    }
    if (!hasProject()) {
        setError(QStringLiteral("Open a Dubbing project before importing reviewed Subtitle OCR results."));
        return false;
    }
    if (ocrSegments.isEmpty()) {
        setError(QStringLiteral("Subtitle OCR did not provide any reviewed transcript segments."));
        return false;
    }

    QVariantList replacement;
    replacement.reserve(ocrSegments.size());
    for (const QVariant &entry : ocrSegments) {
        const QVariantMap ocr = entry.toMap();
        const qint64 startMs = ocr.value(QStringLiteral("startMs")).toLongLong();
        const qint64 endMs = ocr.value(QStringLiteral("endMs")).toLongLong();
        const QString sourceText = ocr.value(QStringLiteral("text")).toString().trimmed();
        if (startMs < 0 || endMs <= startMs || sourceText.isEmpty()) {
            setError(QStringLiteral("Subtitle OCR contains an invalid reviewed segment."));
            return false;
        }
        QVariantMap segment;
        segment.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        segment.insert(QStringLiteral("startMs"), startMs);
        segment.insert(QStringLiteral("endMs"), endMs);
        segment.insert(QStringLiteral("sourceText"), sourceText);
        segment.insert(QStringLiteral("targetText"), QString());
        segment.insert(QStringLiteral("speakerId"), QStringLiteral("speaker-1"));
        segment.insert(QStringLiteral("state"), QStringLiteral("draft"));
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("subtitle-ocr"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
        segment.insert(QStringLiteral("ocrConfidence"), ocr.value(QStringLiteral("confidence")));
        replacement.append(segment);
    }

    invalidateDerivedAudioForChangedSegments(m_project.segments, &replacement);
    m_project.segments = replacement;
    clearError();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::resolveTranscriptConflict(int index, const QString &choice)
{
    if (processing() || index < 0 || index >= m_project.segments.size()) return false;
    const QString normalized = choice.trimmed().toLower();
    if (normalized != QStringLiteral("stt") && normalized != QStringLiteral("ocr")) return false;
    QVariantMap segment = m_project.segments.at(index).toMap();
    if (segment.value(QStringLiteral("fusionStatus")).toString() != QStringLiteral("conflict")) return false;
    const QString text = segment.value(normalized == QStringLiteral("stt")
                                           ? QStringLiteral("fusionSttText")
                                           : QStringLiteral("fusionOcrText")).toString().trimmed();
    if (text.isEmpty()) return false;
    segment.insert(QStringLiteral("sourceText"), text);
    segment.insert(QStringLiteral("fusionChoice"), normalized);
    segment.insert(QStringLiteral("fusionStatus"), QStringLiteral("resolved"));
    segment.insert(QStringLiteral("fusionNeedsReview"), false);
    segment.insert(QStringLiteral("fusionResolutionPolicy"), QStringLiteral("manual"));
    segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
    m_project.segments[index] = segment;
    clearError();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::resolveAllTranscriptConflicts(const QString &choice)
{
    if (processing()) return false;
    const QString normalized = choice.trimmed().toLower();
    if (normalized != QStringLiteral("stt") && normalized != QStringLiteral("ocr")) return false;

    int resolved = 0;
    for (int index = 0; index < m_project.segments.size(); ++index) {
        QVariantMap segment = m_project.segments.at(index).toMap();
        if (segment.value(QStringLiteral("fusionStatus")).toString() != QStringLiteral("conflict"))
            continue;
        const QString text = segment.value(normalized == QStringLiteral("stt")
                                               ? QStringLiteral("fusionSttText")
                                               : QStringLiteral("fusionOcrText")).toString().trimmed();
        if (text.isEmpty()) continue;
        segment.insert(QStringLiteral("sourceText"), text);
        segment.insert(QStringLiteral("fusionChoice"), normalized);
        segment.insert(QStringLiteral("fusionStatus"), QStringLiteral("resolved"));
        segment.insert(QStringLiteral("fusionNeedsReview"), false);
        segment.insert(QStringLiteral("fusionResolutionPolicy"), QStringLiteral("bulk-manual"));
        segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
        m_project.segments[index] = segment;
        ++resolved;
    }
    if (resolved == 0) return false;
    clearError();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::setTranscriptFusionPolicy(const QString &policy)
{
    if (processing()) return false;
    const QString normalized = DubbingTranscriptFusionService::normalizePolicy(policy);
    m_project.transcriptConfiguration.insert(QStringLiteral("fusionPolicy"), normalized);
    QVariantMap selected = m_workflowNodeConfigurations.value(QStringLiteral("transcribe")).toMap();
    QVariantMap parameters = selected.value(QStringLiteral("parameters")).toMap();
    parameters.insert(QStringLiteral("fusionPolicy"), normalized);
    selected.insert(QStringLiteral("parameters"), parameters);
    m_workflowNodeConfigurations.insert(QStringLiteral("transcribe"), selected);
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    persistAfterEdit();
    emit projectChanged();
    emit workflowChanged();
    return true;
}

QVariantMap DubbingController::transcriptConflictAiAvailability() const
{
    QString reason;
    const bool available = DubbingTranslationFixService::reconciliationAvailable(
        translationFixConfiguration(), &reason);
    return {{QStringLiteral("available"), available},
            {QStringLiteral("reason"), reason},
            {QStringLiteral("provider"), translationFixConfiguration().value(QStringLiteral("provider"))},
            {QStringLiteral("model"), translationFixConfiguration().value(QStringLiteral("model"))}};
}

bool DubbingController::requestTranscriptConflictAiSuggestion(int index)
{
    if (!m_translationFix || processing()) return false;
    if (index >= m_project.segments.size()) return false;
    if (index >= 0) {
        const QVariantMap segment = m_project.segments.at(index).toMap();
        if (!segment.value(QStringLiteral("fusionNeedsReview")).toBool()) return false;
    } else if (!hasUnresolvedTranscriptConflicts()) {
        return false;
    }
    QString reason;
    const QVariantMap configuration = translationFixConfiguration();
    if (!DubbingTranslationFixService::reconciliationAvailable(configuration, &reason)) {
        setError(reason);
        return false;
    }
    clearError();
    return m_translationFix->startReconciliation(
        m_project.sourceLanguage, m_project.segments, configuration, index);
}

bool DubbingController::acceptTranscriptConflictAiSuggestion(int index)
{
    if (processing() || index < 0 || index >= m_project.segments.size()) return false;
    QVariantMap segment = m_project.segments.at(index).toMap();
    const QString suggestion = segment.value(QStringLiteral("fusionAiSuggestion")).toString().trimmed();
    if (segment.value(QStringLiteral("fusionStatus")).toString() != QStringLiteral("conflict")
        || segment.value(QStringLiteral("fusionAiSuggestionStatus")).toString()
               != QStringLiteral("pending") || suggestion.isEmpty()) {
        return false;
    }
    segment.insert(QStringLiteral("sourceText"), suggestion);
    segment.insert(QStringLiteral("fusionChoice"), QStringLiteral("ai-suggestion"));
    segment.insert(QStringLiteral("fusionStatus"), QStringLiteral("resolved"));
    segment.insert(QStringLiteral("fusionNeedsReview"), false);
    segment.insert(QStringLiteral("fusionResolutionPolicy"), QStringLiteral("ai-suggest"));
    segment.insert(QStringLiteral("fusionAiSuggestionStatus"), QStringLiteral("accepted"));
    segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
    m_project.segments[index] = segment;
    clearError();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::rejectTranscriptConflictAiSuggestion(int index)
{
    if (processing() || index < 0 || index >= m_project.segments.size()) return false;
    QVariantMap segment = m_project.segments.at(index).toMap();
    if (segment.value(QStringLiteral("fusionStatus")).toString() != QStringLiteral("conflict")
        || segment.value(QStringLiteral("fusionAiSuggestionStatus")).toString()
               != QStringLiteral("pending")) {
        return false;
    }
    // Retain the rejected suggestion and all source evidence for later audit;
    // rejection deliberately leaves the review gate in place.
    segment.insert(QStringLiteral("fusionAiSuggestionStatus"), QStringLiteral("rejected"));
    segment.insert(QStringLiteral("fusionNeedsReview"), true);
    m_project.segments[index] = segment;
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

void DubbingController::addSegment(qint64 startMs, qint64 endMs, const QString &sourceText)
{
    if (endMs <= startMs) {
        setError(QStringLiteral("Segment end must be after its start."));
        return;
    }
    QVariantMap segment;
    segment.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    segment.insert(QStringLiteral("sourceText"), sourceText);
    segment.insert(QStringLiteral("timingSource"), QStringLiteral("manual"));
    segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
    segment.insert(QStringLiteral("targetText"), QString());
    segment.insert(QStringLiteral("speakerId"), QStringLiteral("speaker-1"));
    segment.insert(QStringLiteral("state"), QStringLiteral("draft"));
    m_project.segments.append(segment);
    m_timingResolutionPreview.clear();
    m_timingUndoSegments.clear();
    invalidateTimingOutputs();
    emit segmentsChanged();
    emit timingResolutionChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::updateSegment(int index, const QVariantMap &patch)
{
    if (index < 0 || index >= m_project.segments.size()) return;
    QVariantMap segment = m_project.segments.at(index).toMap();
    const bool sourceTextChanged = patch.contains(QStringLiteral("sourceText"))
        && patch.value(QStringLiteral("sourceText")).toString()
            != segment.value(QStringLiteral("sourceText")).toString();
    const bool targetTextChanged = patch.contains(QStringLiteral("targetText"))
        && patch.value(QStringLiteral("targetText")).toString()
            != segment.value(QStringLiteral("targetText")).toString();
    const bool speakerChanged = patch.contains(QStringLiteral("speakerId"))
        && patch.value(QStringLiteral("speakerId")).toString()
            != segment.value(QStringLiteral("speakerId")).toString();
    const qint64 startMs = patch.value(QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))).toLongLong();
    const qint64 endMs = patch.value(QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))).toLongLong();
    const bool timingChanged = startMs != segment.value(QStringLiteral("startMs")).toLongLong()
        || endMs != segment.value(QStringLiteral("endMs")).toLongLong()
        || (patch.contains(QStringLiteral("durationMs"))
            && patch.value(QStringLiteral("durationMs")).toLongLong()
                != segment.value(QStringLiteral("durationMs")).toLongLong());
    if (endMs <= startMs) {
        setError(QStringLiteral("Segment end must be after its start."));
        return;
    }
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) segment.insert(it.key(), it.value());
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    if (sourceTextChanged) {
        // Word timestamps are derived from the source transcript. Any source edit
        // invalidates the previous alignment and forces the next refinement pass
        // to treat this segment as an ASR/manual-timing fallback.
        segment.remove(QStringLiteral("words"));
        segment.remove(QStringLiteral("alignmentCoverage"));
        segment.remove(QStringLiteral("alignmentMatchScore"));
        segment.remove(QStringLiteral("alignmentModel"));
        segment.remove(QStringLiteral("alignmentRuntime"));
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
        segment.remove(QStringLiteral("durationBudget"));
        segment.remove(QStringLiteral("durationUnits"));
        segment.remove(QStringLiteral("durationStatus"));
        segment.remove(QStringLiteral("phonemeDistance"));
        segment.remove(QStringLiteral("referenceTranslation"));
        segment.remove(QStringLiteral("targetChunks"));
        segment.remove(QStringLiteral("pauseAligned"));
        if (segment.value(QStringLiteral("fusionStatus")).toString()
                == QStringLiteral("conflict")
            || segment.value(QStringLiteral("fusionNeedsReview")).toBool()) {
            // A direct edit is an explicit reviewer decision. Preserve both
            // observations and any AI suggestion, but let this final text move
            // forward instead of leaving a stale, invisible review block.
            segment.insert(QStringLiteral("fusionChoice"), QStringLiteral("manual"));
            segment.insert(QStringLiteral("fusionStatus"), QStringLiteral("resolved"));
            segment.insert(QStringLiteral("fusionNeedsReview"), false);
            segment.insert(QStringLiteral("fusionResolutionPolicy"), QStringLiteral("manual"));
            segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
        }
    }
    if (targetTextChanged || speakerChanged) {
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
        if (targetTextChanged) {
            const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
            const int durationUnits = budget.isEmpty()
                ? -1
                : EspeakNgPhonemizer::count(
                      segment.value(QStringLiteral("targetText")).toString(),
                      m_project.targetLanguage);
            if (durationUnits >= 0) {
                const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
                const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
                segment.insert(QStringLiteral("durationUnits"), durationUnits);
                segment.insert(QStringLiteral("phonemeDistance"),
                               qAbs(durationUnits
                                    - budget.value(QStringLiteral("targetUnits")).toInt()));
                segment.insert(QStringLiteral("durationStatus"),
                               durationUnits >= minimum && durationUnits <= maximum
                                   ? QStringLiteral("within-budget")
                                   : QStringLiteral("needs-review"));
            } else {
                segment.remove(QStringLiteral("durationUnits"));
                segment.remove(QStringLiteral("durationStatus"));
                segment.remove(QStringLiteral("phonemeDistance"));
            }
            segment.remove(QStringLiteral("durationPrompt"));
            segment.remove(QStringLiteral("targetChunks"));
            segment.remove(QStringLiteral("referenceTranslation"));
            segment.remove(QStringLiteral("pauseAligned"));
            segment.remove(QStringLiteral("candidateSelectionMetric"));
        }
    }
    const bool audioInputChanged = sourceTextChanged || targetTextChanged || speakerChanged
        || timingChanged;
    if (audioInputChanged) {
        // `state=stale` alone is not sufficient: preview/mix historically
        // accepted any existing clipPath.  Remove every derived cue field so
        // a changed script, timing, or voice can never reuse old speech.
        segment.remove(QStringLiteral("clipPath"));
        segment.remove(QStringLiteral("cacheFingerprint"));
        segment.remove(QStringLiteral("waveformSamples"));
        segment.remove(QStringLiteral("clipDurationMs"));
    }
    m_project.segments[index] = segment;
    if (timingChanged) {
        m_timingResolutionPreview.clear();
        m_timingUndoSegments.clear();
        invalidateTimingOutputs();
        emit timingResolutionChanged();
    }
    if (audioInputChanged) invalidateSynthesisOutputs();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::removeSegment(int index)
{
    if (index < 0 || index >= m_project.segments.size()) return;
    m_project.segments.removeAt(index);
    m_timingResolutionPreview.clear();
    m_timingUndoSegments.clear();
    invalidateTimingOutputs();
    emit segmentsChanged();
    emit timingResolutionChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::addSpeaker(const QString &name)
{
    QVariantMap speaker;
    speaker.insert(QStringLiteral("id"), QStringLiteral("speaker-%1").arg(m_project.speakers.size() + 1));
    speaker.insert(QStringLiteral("name"), name.trimmed().isEmpty()
                   ? QStringLiteral("Speaker %1").arg(m_project.speakers.size() + 1) : name.trimmed());
    speaker.insert(QStringLiteral("voice"), QVariantMap());
    m_project.speakers.append(speaker);
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setSpeakerVoice(int speakerIndex, const QVariantMap &voice)
{
    if (speakerIndex < 0 || speakerIndex >= m_project.speakers.size()) return;
    QVariantMap speaker = m_project.speakers.at(speakerIndex).toMap();
    if (speaker.value(QStringLiteral("voice")) == voice) return;
    speaker.insert(QStringLiteral("voice"), voice);
    m_project.speakers[speakerIndex] = speaker;
    const QString speakerId = speaker.value(QStringLiteral("id")).toString();
    bool changedCue = false;
    for (int index = 0; index < m_project.segments.size(); ++index) {
        QVariantMap segment = m_project.segments.at(index).toMap();
        if (segment.value(QStringLiteral("speakerId")).toString() != speakerId) continue;
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
        segment.remove(QStringLiteral("clipPath"));
        segment.remove(QStringLiteral("cacheFingerprint"));
        segment.remove(QStringLiteral("waveformSamples"));
        segment.remove(QStringLiteral("clipDurationMs"));
        m_project.segments[index] = segment;
        changedCue = true;
    }
    if (changedCue) {
        invalidateSynthesisOutputs();
        emit segmentsChanged();
    }
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::clearError()
{
    if (m_translationFix) m_translationFix->clearError();
    m_runner->clearError();
}

void DubbingController::setError(const QString &message)
{
    m_runner->setError(message);
}

void DubbingController::setBusyError(const QString &message)
{
    // A rejected user action must never turn a valid in-flight worker into a
    // failed job. Keep the visible diagnostic without changing the stage,
    // progress, or cancellation state of that worker.
    m_runner->setBusyError(message);
}

void DubbingController::persistAfterEdit()
{
    if (!m_project.projectPath.isEmpty()) saveProject();
}

void DubbingController::invalidateSynthesisOutputs()
{
    m_stepOutputs.remove(QStringLiteral("synthesize"));
    m_stepOutputs.remove(QStringLiteral("fit-timing"));
    if (m_runner) m_runner->setDubbedVocalPath(QString());
    invalidateTimingOutputs();
}

void DubbingController::invalidateDerivedAudioForChangedSegments(
    const QVariantList &previous, QVariantList *updated)
{
    if (!updated) return;
    bool invalidated = previous.size() != updated->size();
    const QStringList audioInputKeys{
        QStringLiteral("id"), QStringLiteral("sourceText"), QStringLiteral("targetText"),
        QStringLiteral("speakerId"), QStringLiteral("startMs"), QStringLiteral("endMs"),
        QStringLiteral("voiceId"), QStringLiteral("voiceProfileId"), QStringLiteral("speed")};
    for (int index = 0; index < updated->size(); ++index) {
        QVariantMap segment = updated->at(index).toMap();
        const QVariantMap before = index < previous.size() ? previous.at(index).toMap()
                                                            : QVariantMap();
        bool changed = before.isEmpty();
        for (const QString &key : audioInputKeys) {
            if (before.value(key) != segment.value(key)) {
                changed = true;
                break;
            }
        }
        if (!changed) continue;
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
        segment.remove(QStringLiteral("clipPath"));
        segment.remove(QStringLiteral("cacheFingerprint"));
        segment.remove(QStringLiteral("waveformSamples"));
        segment.remove(QStringLiteral("clipDurationMs"));
        (*updated)[index] = segment;
        invalidated = true;
    }
    if (invalidated) invalidateSynthesisOutputs();
}

void DubbingController::invalidateTimingOutputs()
{
    // A ripple changes every downstream timestamp.  Keep the existing assets
    // on disk for recovery, but make neither preview nor export appear current.
    m_stepOutputs.remove(QStringLiteral("mix"));
    m_stepOutputs.remove(QStringLiteral("export"));
    m_pendingExportPath.clear();
    if (m_runner) {
        m_runner->setPreviewPath(QString());
        m_runner->setExportPath(QString());
    }
    emit previewChanged();
    emit exportChanged();
}

