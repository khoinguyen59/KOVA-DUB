void DubbingTranslationFixService::processCandidate(const QString &candidate)
{
    if (m_reconciliation) {
        processReconciliationCandidate(candidate);
        return;
    }
    const QString candidateKey = candidate.simplified().toCaseFolded();
    if (m_seenCandidates.contains(candidateKey)) {
        ++m_attempt;
        Logger::warning(QStringLiteral("DubbingTranslationFix"),
                        QStringLiteral("Repeated rewrite rejected at attempt %1/%2")
                            .arg(m_attempt).arg(m_maxAttempts));
        if (m_attempt < m_maxAttempts) {
            requestAttempt();
            return;
        }
        if (!m_bestCandidate.isEmpty()) {
            const QVariantMap current =
                m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
            QVariantMap improved = current;
            applyCandidate(improved, m_bestCandidate, m_bestCandidatePhonemes, false);
            m_segments[m_eligibleIndices.at(m_segmentPosition)] = improved;
            finishSegment(false, true);
        } else {
            finishSegment(false);
        }
        return;
    }
    m_seenCandidates.insert(candidateKey);

    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    const int phonemes = EspeakNgPhonemizer::count(candidate, m_targetLanguage);
    if (phonemes < 0) {
        setError(QStringLiteral(
            "eSpeak NG became unavailable while validating the rewritten translation."));
        return;
    }
    const QStringList tokens = protectedTokens(
        segment.value(QStringLiteral("sourceText")).toString());
    const bool tokensPreserved = preservesProtectedTokens(candidate, tokens);
    const double semanticScore =
        DubbingDurationPlanner::semanticFidelityScore(
            m_originalTranslation, candidate);
    const bool semanticGuardPassed = semanticScore >= 0.25;
    const bool withinBudget =
        phonemes >= budget.value(QStringLiteral("minUnits")).toInt()
        && phonemes <= budget.value(QStringLiteral("maxUnits")).toInt();
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Response segment=%1 attempt=%2 chars=%3 phonemes=%4 range=%5-%6 withinBudget=%7 protectedTokens=%8 semanticScore=%9 semanticGuard=%10")
            .arg(segment.value(QStringLiteral("id")).toString())
            .arg(m_attempt + 1).arg(candidate.size()).arg(phonemes)
            .arg(budget.value(QStringLiteral("minUnits")).toInt())
            .arg(budget.value(QStringLiteral("maxUnits")).toInt())
            .arg(withinBudget ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(tokensPreserved ? QStringLiteral("preserved")
                                 : QStringLiteral("missing"))
            .arg(semanticScore, 0, 'f', 3)
            .arg(semanticGuardPassed ? QStringLiteral("passed")
                                     : QStringLiteral("rejected")));

    m_lastCandidate = candidate;
    m_lastCandidatePhonemes = phonemes;
    ++m_attempt;
    if (withinBudget && tokensPreserved && semanticGuardPassed) {
        QVariantMap accepted = segment;
        applyCandidate(accepted, candidate, phonemes, true);
        m_segments[m_eligibleIndices.at(m_segmentPosition)] = accepted;
        finishSegment(true);
        return;
    }

    const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    if (tokensPreserved && semanticGuardPassed
        && isCloserToBudget(m_bestCandidatePhonemes, phonemes, minimum, maximum)) {
        m_bestCandidate = candidate;
        m_bestCandidatePhonemes = phonemes;
        m_promptTranslation = candidate;
        m_promptPhonemes = phonemes;
    }
    if (m_attempt < m_maxAttempts) {
        requestAttempt();
        return;
    }
    if (!m_bestCandidate.isEmpty()) {
        QVariantMap improved = segment;
        applyCandidate(improved, m_bestCandidate, m_bestCandidatePhonemes, false);
        m_segments[m_eligibleIndices.at(m_segmentPosition)] = improved;
        Logger::info(
            QStringLiteral("DubbingTranslationFix"),
            QStringLiteral("Keeping closest safe rewrite segment=%1 phonemes=%2 range=%3-%4")
                .arg(segment.value(QStringLiteral("id")).toString())
                .arg(m_bestCandidatePhonemes).arg(minimum).arg(maximum));
        finishSegment(false, true);
        return;
    }
    finishSegment(false);
}

void DubbingTranslationFixService::processReconciliationCandidate(const QString &candidate)
{
    if (!m_reconciliation || m_segmentPosition >= m_eligibleIndices.size()) return;
    const QString suggestion = cleanAssistantText(candidate);
    const int index = m_eligibleIndices.at(m_segmentPosition);
    QVariantMap segment = m_segments.at(index).toMap();
    if (suggestion.isEmpty()) {
        finishReconciliationSegment(false);
        return;
    }

    // The proposal is intentionally stored beside, never instead of, STT/OCR
    // evidence. The controller must receive an explicit accept/reject action
    // before sourceText changes or Translate becomes available.
    segment.insert(QStringLiteral("fusionAiSuggestion"), suggestion);
    segment.insert(QStringLiteral("fusionAiSuggestionStatus"), QStringLiteral("pending"));
    segment.insert(QStringLiteral("fusionAiSuggestionLanguage"), m_sourceLanguage);
    segment.insert(QStringLiteral("fusionAiSuggestionProvider"),
                   m_configuration.value(QStringLiteral("provider")).toString());
    segment.insert(QStringLiteral("fusionAiSuggestionModel"),
                   m_configuration.value(QStringLiteral("model")).toString());
    segment.insert(QStringLiteral("fusionAiSuggestionEvidence"), QVariantMap{
        {QStringLiteral("sttText"), segment.value(QStringLiteral("fusionSttText"))},
        {QStringLiteral("ocrText"), segment.value(QStringLiteral("fusionOcrText"))},
        {QStringLiteral("sttConfidence"), segment.value(QStringLiteral("sttConfidence"))},
        {QStringLiteral("ocrConfidence"), segment.value(QStringLiteral("ocrConfidence"))},
        {QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))},
        {QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))}
    });
    segment.insert(QStringLiteral("fusionNeedsReview"), true);
    segment.insert(QStringLiteral("fusionStatus"), QStringLiteral("conflict"));
    segment.insert(QStringLiteral("state"), QStringLiteral("needs-review"));
    m_segments[index] = segment;
    Logger::info(
        QStringLiteral("DubbingTranscriptReconciliation"),
        QStringLiteral("Suggestion stored for segment=%1 chars=%2 sourceLanguage=%3")
            .arg(segment.value(QStringLiteral("id")).toString())
            .arg(suggestion.size()).arg(m_sourceLanguage));
    finishReconciliationSegment(true);
}

void DubbingTranslationFixService::finishSegment(bool fixed, bool improved)
{
    if (fixed) ++m_fixedCount;
    else {
        ++m_unresolvedCount;
        if (improved) ++m_improvedCount;
    }
    ++m_segmentPosition;
    setProgress(qRound(m_segmentPosition * 100.0
                       / qMax(1, m_eligibleIndices.size())));
    beginSegment();
}

void DubbingTranslationFixService::finishReconciliationSegment(bool suggested)
{
    if (suggested) ++m_suggestedCount;
    else ++m_unresolvedCount;
    ++m_segmentPosition;
    setProgress(qRound(m_segmentPosition * 100.0
                       / qMax(1, m_eligibleIndices.size())));
    beginSegment();
}

void DubbingTranslationFixService::finishRun()
{
    if (m_reconciliation) {
        setProgress(100);
        setStatus(QStringLiteral("Prepared %1 AI suggestion(s); %2 conflict(s) remain without a suggestion. Review is still required.")
                      .arg(m_suggestedCount).arg(m_unresolvedCount));
        Logger::info(
            QStringLiteral("DubbingTranscriptReconciliation"),
            QStringLiteral("Completed suggested=%1 unresolved=%2 total=%3")
                .arg(m_suggestedCount).arg(m_unresolvedCount)
                .arg(m_eligibleIndices.size()));
        m_reconciliation = false;
        setBusy(false);
        emit reconciliationCompleted(m_segments, m_suggestedCount, m_unresolvedCount);
        return;
    }
    setProgress(100);
    setStatus(QStringLiteral("Fixed %1 segment(s); improved %2; %3 still need review.")
                  .arg(m_fixedCount).arg(m_improvedCount).arg(m_unresolvedCount));
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Rewrite completed fixed=%1 improved=%2 unresolved=%3 total=%4")
            .arg(m_fixedCount).arg(m_improvedCount).arg(m_unresolvedCount)
            .arg(m_eligibleIndices.size()));
    setBusy(false);
    emit completed(m_segments, m_fixedCount, m_unresolvedCount);
}

QString DubbingTranslationFixService::buildPrompt(
    const QVariantMap &segment) const
{
    if (m_reconciliation) return buildReconciliationPrompt(segment);
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    const QString direction = m_promptPhonemes > maximum
        ? QStringLiteral("Shorten the wording without dropping any source meaning.")
        : QStringLiteral("Keep the wording inside the required range.");
    QString feedback;
    if (m_attempt > 0) {
        feedback = QStringLiteral(
            "\nThe previous rewrite had %1 phonemes and did not pass validation. "
            "Use a different construction and correct the length.")
                       .arg(m_lastCandidatePhonemes);
    }
    const QString tokens = protectedTokens(
        segment.value(QStringLiteral("sourceText")).toString())
                               .join(QStringLiteral(", "));
    return QStringLiteral(
               "Source language: %1\nTarget language: %2\n"
               "Original source:\n%3\n\n"
               "Faithful current translation:\n%4\n\n"
               "Rewrite starting point:\n%5\n\n"
               "External eSpeak NG measurement: %6 phonemes.\n"
               "Required range: %7-%8 phonemes; ideal target: %9 phonemes.\n"
               "Protected tokens that must remain exactly unchanged: %10\n"
               "Keep the translation as semantically faithful as possible. %11%12")
        .arg(m_sourceLanguage, m_targetLanguage,
             segment.value(QStringLiteral("sourceText")).toString(),
             m_originalTranslation, m_promptTranslation)
        .arg(m_promptPhonemes)
        .arg(budget.value(QStringLiteral("minUnits")).toInt())
        .arg(budget.value(QStringLiteral("maxUnits")).toInt())
        .arg(budget.value(QStringLiteral("targetUnits")).toInt())
        .arg(tokens.isEmpty() ? QStringLiteral("(none)") : tokens,
             direction, feedback);
}

QString DubbingTranslationFixService::buildReconciliationPrompt(
    const QVariantMap &segment) const
{
    const int currentIndex = m_eligibleIndices.value(m_segmentPosition, -1);
    QString previous;
    QString next;
    if (currentIndex > 0)
        previous = m_segments.at(currentIndex - 1).toMap()
                       .value(QStringLiteral("sourceText")).toString().trimmed();
    if (currentIndex >= 0 && currentIndex + 1 < m_segments.size())
        next = m_segments.at(currentIndex + 1).toMap()
                   .value(QStringLiteral("sourceText")).toString().trimmed();
    return QStringLiteral(
               "Source language: %1\n"
               "Time: %2-%3 ms\n"
               "STT observation (confidence %4):\n%5\n\n"
               "OCR observation (confidence %6):\n%7\n\n"
               "Previous transcript context:\n%8\n\n"
               "Next transcript context:\n%9\n\n"
               "Propose one source-language transcript for a human reviewer. Do not translate it. Return only the proposed text.")
        .arg(m_sourceLanguage,
             segment.value(QStringLiteral("startMs")).toString(),
             segment.value(QStringLiteral("endMs")).toString(),
             QString::number(segment.value(QStringLiteral("sttConfidence")).toDouble(), 'f', 2),
             segment.value(QStringLiteral("fusionSttText")).toString(),
             QString::number(segment.value(QStringLiteral("ocrConfidence")).toDouble(), 'f', 2),
             segment.value(QStringLiteral("fusionOcrText")).toString(),
             previous.isEmpty() ? QStringLiteral("(none)") : previous,
             next.isEmpty() ? QStringLiteral("(none)") : next);
}

QStringList DubbingTranslationFixService::protectedTokens(
    const QString &text) const
{
    QStringList result;
    const QRegularExpression expression(
        QStringLiteral("(?:https?://\\S+|\\b\\d[\\d.,/%-]*|\\b[A-Z]{2,}\\b)"));
    auto matches = expression.globalMatch(text);
    while (matches.hasNext()) result.append(matches.next().captured(0));
    result.removeDuplicates();
    return result;
}

bool DubbingTranslationFixService::preservesProtectedTokens(
    const QString &candidate, const QStringList &tokens) const
{
    for (const QString &token : tokens) {
        if (!candidate.contains(token, Qt::CaseInsensitive)) return false;
    }
    return true;
}

void DubbingTranslationFixService::applyCandidate(
    QVariantMap &segment, const QString &candidate, int phonemes,
    bool withinBudget) const
{
    const QVariantMap budget =
        segment.value(QStringLiteral("durationBudget")).toMap();
    if (!segment.contains(QStringLiteral("referenceTranslation")))
        segment.insert(QStringLiteral("referenceTranslation"), m_originalTranslation);
    segment.insert(QStringLiteral("targetText"), candidate);
    segment.insert(QStringLiteral("durationUnits"), phonemes);
    segment.insert(QStringLiteral("phonemeDistance"),
                   qAbs(phonemes - budget.value(QStringLiteral("targetUnits")).toInt()));
    segment.insert(QStringLiteral("durationStatus"), withinBudget
                       ? QStringLiteral("within-budget")
                       : QStringLiteral("needs-review"));
    segment.insert(QStringLiteral("durationMetric"), QStringLiteral("phoneme-distance"));
    segment.insert(QStringLiteral("candidateSelectionMetric"), withinBudget
                       ? QStringLiteral("lm-studio-qwen-rewrite-v1")
                       : QStringLiteral("lm-studio-closest-safe-rewrite-v1"));
    segment.insert(QStringLiteral("rewriteProvider"),
                   m_configuration.value(QStringLiteral("provider")));
    segment.insert(QStringLiteral("rewriteModel"),
                   m_configuration.value(QStringLiteral("model")));
    segment.insert(QStringLiteral("rewriteAttempts"), m_attempt);
    segment.insert(QStringLiteral("targetChunks"),
                   DubbingDurationPlanner::pauseChunks(
                       candidate, budget.value(QStringLiteral("pauses")).toList()));
    segment.insert(QStringLiteral("pauseAligned"), true);
    segment.insert(QStringLiteral("pauseAlignmentMethod"),
                   QStringLiteral("deterministic-even-split-v1"));
    segment.insert(QStringLiteral("state"), QStringLiteral("translated"));
}

void DubbingTranslationFixService::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit stateChanged();
}

void DubbingTranslationFixService::setProgress(int progress)
{
    const int normalized = qBound(0, progress, 100);
    if (m_progress == normalized) return;
    m_progress = normalized;
    emit stateChanged();
}

void DubbingTranslationFixService::setStatus(const QString &status)
{
    if (m_statusText == status) return;
    m_statusText = status;
    emit stateChanged();
}

void DubbingTranslationFixService::setError(const QString &message)
{
    m_lastError = message;
    m_statusText = message;
    Logger::error(QStringLiteral("DubbingTranslationFix"), message);
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (m_cliProcess) {
        m_cliProcess->kill();
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }
    setBusy(false);
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
    emit stateChanged();
    emit failed(message);
}

