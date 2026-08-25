bool DubbingTranslationFixService::start(
    const QString &sourceLanguage, const QString &targetLanguage,
    const QVariantList &segments, const QVariantMap &configuration,
    int segmentIndex)
{
    if (m_busy || m_testing) return false;
    m_reconciliation = false;
    m_configuration = normalizedConfiguration(configuration.isEmpty()
                                                  ? m_configuration : configuration);
    const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
    if (provider == QStringLiteral("local")) {
        setError(QStringLiteral("Local translation models do not use the remote rewrite service."));
        return false;
    }
    if (provider == QStringLiteral("colab-direct")) {
        QString routeError;
        if (!m_directColabSession
            || !m_directColabSession->hasVerifiedRoute(
                QStringLiteral("llm-chat"),
                m_configuration.value(QStringLiteral("model")).toString(), &routeError)) {
            setError(routeError.isEmpty()
                         ? QStringLiteral("Connect and check the exact Direct Colab Adaptive LLM worker before rewriting translations.")
                         : routeError);
            return false;
        }
    } else if (provider == QStringLiteral("cli")) {
        const QString cliAgent = m_configuration.value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
        QString binName = QStringLiteral("claude");
        if (cliAgent == QStringLiteral("codex")) binName = QStringLiteral("codex");
        else if (cliAgent == QStringLiteral("antigravity")) binName = QStringLiteral("agy");
        if (cliExecutablePath(cliAgent).isEmpty()) {
            setError(QStringLiteral("Local CLI Agent binary '%1' is not found on system PATH.").arg(binName));
            return false;
        }
    } else {
        const QString base = normalizedServerBase(
            m_configuration.value(QStringLiteral("serverUrl")).toString());
        const QUrl endpoint(provider == QStringLiteral("api")
                                ? base + QStringLiteral("/v1/chat/completions")
                                : base + QStringLiteral("/api/v1/chat"));
        if (!endpoint.isValid() || endpoint.host().isEmpty()) {
            setError(provider == QStringLiteral("api")
                         ? QStringLiteral("LLM API URL is invalid.")
                         : QStringLiteral("LM Studio server URL is invalid."));
            return false;
        }
        if (m_configuration.value(QStringLiteral("model")).toString().isEmpty()) {
            setError(provider == QStringLiteral("api")
                         ? QStringLiteral("LLM API model identifier is required.")
                         : QStringLiteral("LM Studio model identifier is required."));
            return false;
        }
    }

    m_segments = segments;
    m_sourceLanguage = sourceLanguage;
    m_targetLanguage = targetLanguage;
    m_eligibleIndices.clear();
    for (int i = 0; i < m_segments.size(); ++i) {
        if (segmentIndex >= 0 && i != segmentIndex) continue;
        const QVariantMap segment = m_segments.at(i).toMap();
        if (actualPhonemeCount(segment, targetLanguage) < 0) {
            setError(QStringLiteral(
                "eSpeak NG is unavailable, so translated phonemes cannot be verified."));
            return false;
        }
        if (isOverBudget(segment, targetLanguage)) m_eligibleIndices.append(i);
    }
    if (m_eligibleIndices.isEmpty()) {
        setError(segmentIndex >= 0
                     ? QStringLiteral("This translation does not exceed its phoneme limit.")
                     : QStringLiteral("No translated segment exceeds its phoneme limit."));
        return false;
    }

    saveConfiguration();
    m_maxAttempts = m_configuration.value(QStringLiteral("maxAttempts")).toInt();
    m_segmentPosition = 0;
    m_fixedCount = 0;
    m_improvedCount = 0;
    m_unresolvedCount = 0;
    m_lastError.clear();
    setProgress(0);
    setBusy(true);
    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("Starting %1 rewrite model=%2 segments=%3 selectedIndex=%4 maxAttempts=%5 targetLanguage=%6")
            .arg(provider,
                 m_configuration.value(QStringLiteral("model")).toString())
            .arg(m_eligibleIndices.size()).arg(segmentIndex).arg(m_maxAttempts)
            .arg(targetLanguage));
    beginSegment();
    return true;
}

void DubbingTranslationFixService::setDirectColabSession(ColabSession *session)
{
    m_directColabSession = session;
}

bool DubbingTranslationFixService::startReconciliation(
    const QString &sourceLanguage, const QVariantList &segments,
    const QVariantMap &configuration, int segmentIndex)
{
    if (m_busy || m_testing) return false;
    m_configuration = normalizedConfiguration(configuration.isEmpty()
                                                  ? m_configuration : configuration);
    QString unavailableReason;
    if (!reconciliationAvailable(m_configuration, &unavailableReason)) {
        setError(unavailableReason);
        return false;
    }

    m_reconciliation = true;
    m_segments = segments;
    m_sourceLanguage = sourceLanguage.trimmed().isEmpty()
        ? QStringLiteral("auto") : sourceLanguage.trimmed();
    m_targetLanguage.clear();
    m_eligibleIndices.clear();
    for (int i = 0; i < m_segments.size(); ++i) {
        if (segmentIndex >= 0 && i != segmentIndex) continue;
        const QVariantMap segment = m_segments.at(i).toMap();
        if (segment.value(QStringLiteral("fusionStatus")).toString()
                == QStringLiteral("conflict")
            && segment.value(QStringLiteral("fusionNeedsReview")).toBool()) {
            m_eligibleIndices.append(i);
        }
    }
    if (m_eligibleIndices.isEmpty()) {
        m_reconciliation = false;
        setError(segmentIndex >= 0
                     ? QStringLiteral("This transcript conflict no longer needs review.")
                     : QStringLiteral("No unresolved STT/OCR conflict is available for AI suggestion."));
        return false;
    }

    saveConfiguration();
    // One response per conflict creates a review suggestion; it is never
    // retried into an automatically accepted answer.
    m_maxAttempts = 1;
    m_segmentPosition = 0;
    m_fixedCount = 0;
    m_improvedCount = 0;
    m_unresolvedCount = 0;
    m_suggestedCount = 0;
    m_lastError.clear();
    setProgress(0);
    setBusy(true);
    Logger::info(
        QStringLiteral("DubbingTranscriptReconciliation"),
        QStringLiteral("Starting provider=%1 model=%2 conflicts=%3 selectedIndex=%4 sourceLanguage=%5")
            .arg(m_configuration.value(QStringLiteral("provider")).toString(),
                 m_configuration.value(QStringLiteral("model")).toString())
            .arg(m_eligibleIndices.size()).arg(segmentIndex).arg(m_sourceLanguage));
    beginSegment();
    return true;
}

void DubbingTranslationFixService::testConnection(
    const QVariantMap &configuration)
{
    if (m_busy || m_testing) return;
    m_configuration = normalizedConfiguration(configuration.isEmpty()
                                                  ? m_configuration : configuration);
    const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
    if (provider == QStringLiteral("colab-direct")) {
        QString routeError;
        const bool ready = m_directColabSession
            && m_directColabSession->hasVerifiedRoute(
                QStringLiteral("llm-chat"),
                m_configuration.value(QStringLiteral("model")).toString(), &routeError);
        saveConfiguration();
        emit stateChanged();
        emit connectionTested(
            ready,
            ready ? QStringLiteral("Verified Direct Colab Adaptive LLM worker is ready.")
                  : (routeError.isEmpty()
                         ? QStringLiteral("Connect and check the exact Direct Colab Adaptive LLM worker.")
                         : routeError));
        return;
    }
    if (provider == QStringLiteral("local")) {
        saveConfiguration();
        emit connectionTested(true, QStringLiteral("Local LA Studio model selected."));
        emit stateChanged();
        return;
    }
    if (provider == QStringLiteral("cli")) {
        const QString cliAgent = m_configuration
                                     .value(QStringLiteral("cliAgent"),
                                            QStringLiteral("claude"))
                                     .toString();
        saveConfiguration();
        const QString exePath = cliExecutablePath(cliAgent);
        if (exePath.isEmpty()) {
            const CliInvocation unresolved = cliInvocation(
                cliAgent, {}, {}, {}, {}, 30);
            emit connectionTested(false, QStringLiteral("CLI binary \"%1\" was not found on system PATH.")
                                             .arg(unresolved.binaryName));
            emit stateChanged();
            return;
        }

        const QString prompt = QStringLiteral(
            "Connection health check for LA Studio. This is a text-only request. "
            "Do not call any tools. Reply with exactly: OK");
        const QString logPath = createCliDiagnosticLogPath(cliAgent);
        const CliInvocation invocation = cliInvocation(
            cliAgent,
            m_configuration.value(QStringLiteral("model")).toString(),
            prompt, exePath, logPath, 30);

        QProcess *process = new QProcess(this);
        process->setWorkingDirectory(invocation.workingDirectory);
        connect(process, &QObject::destroyed,
                [logPath = invocation.diagnosticLogPath]() {
            if (!logPath.isEmpty()) QFile::remove(logPath);
        });
        m_cliProcess = process;
        m_testing = true;
        emit stateChanged();
        connect(process, &QProcess::finished, this,
                [this, process, invocation](int exitCode,
                                            QProcess::ExitStatus exitStatus) {
            if (m_cliProcess == process) m_cliProcess = nullptr;
            const QByteArray diagnosticLog =
                takeCliDiagnosticLog(invocation.diagnosticLogPath);
            if (!m_testing) {
                process->deleteLater();
                return;
            }
            m_testing = false;
            const QByteArray stdoutData = process->readAllStandardOutput();
            const QByteArray stderrData = process->readAllStandardError();
            const QString response = parseCliResponse(stdoutData);
            const QString classifiedFailure = classifiedCliFailure(
                invocation.agentId, stdoutData, stderrData, diagnosticLog);
            const bool expectedSmokeReply =
                response.compare(QStringLiteral("OK"),
                                 Qt::CaseInsensitive) == 0;
            const bool success = exitStatus == QProcess::NormalExit
                && exitCode == 0 && !response.isEmpty()
                && classifiedFailure.isEmpty() && expectedSmokeReply;
            const QString message = success
                ? QStringLiteral("%1 is authenticated and returned a valid model response.")
                      .arg(invocation.displayName)
                : QStringLiteral("%1 connection failed: %2")
                      .arg(invocation.displayName,
                           classifiedFailure.isEmpty()
                               ? (!response.isEmpty() && !expectedSmokeReply
                                      ? QStringLiteral("The CLI returned an unexpected smoke-test response instead of OK: \"%1\"")
                                            .arg(response.left(160))
                                      : cliFailureMessage(invocation.agentId,
                                                          stdoutData, stderrData,
                                                          diagnosticLog))
                               : classifiedFailure);
            Logger::info(QStringLiteral("DubbingTranslationFix"),
                         QStringLiteral("CLI connection test agent=%1 success=%2 exitCode=%3 responseChars=%4 message=%5")
                             .arg(invocation.displayName,
                                  success ? QStringLiteral("true") : QStringLiteral("false"))
                             .arg(exitCode).arg(response.size()).arg(message));
            process->deleteLater();
            emit stateChanged();
            emit connectionTested(success, message);
        });

        Logger::info(
            QStringLiteral("DubbingTranslationFix"),
            QStringLiteral("CLI connection launch agent=%1 executable=%2 args=%3 promptViaStdin=%4 diagnosticLog=%5")
                .arg(invocation.agentId, invocation.program,
                     cliArgumentsForLog(invocation),
                     invocation.promptViaStdin ? QStringLiteral("true")
                                               : QStringLiteral("false"),
                     invocation.diagnosticLogPath.isEmpty()
                         ? QStringLiteral("disabled") : QStringLiteral("enabled")));
        connect(process, &QProcess::errorOccurred, this,
                [this, process, invocation, exePath](QProcess::ProcessError processError) {
            if (processError != QProcess::FailedToStart || m_cliProcess != process) return;
            takeCliDiagnosticLog(invocation.diagnosticLogPath);
            m_cliProcess = nullptr;
            m_testing = false;
            process->deleteLater();
            emit stateChanged();
            emit connectionTested(false, QStringLiteral("Failed to launch %1 at %2.")
                                           .arg(invocation.displayName, exePath));
        });
        connect(process, &QProcess::started, this, [process, prompt, invocation]() {
            if (invocation.promptViaStdin) process->write(prompt.toUtf8());
            process->closeWriteChannel();
        });
        process->start(invocation.program, invocation.arguments);

        QTimer::singleShot(45000, process, [this, process, invocation]() {
            if (m_cliProcess != process || !m_testing) return;
            m_cliProcess = nullptr;
            m_testing = false;
            process->kill();
            emit stateChanged();
            emit connectionTested(
                false, QStringLiteral("%1 connection test timed out. Check sign-in and network access.")
                           .arg(invocation.displayName));
        });
        emit stateChanged();
        return;
    }
    const QString base = normalizedServerBase(
        m_configuration.value(QStringLiteral("serverUrl")).toString());
    const QUrl endpoint(provider == QStringLiteral("api")
                            ? base + QStringLiteral("/v1/models")
                            : base + QStringLiteral("/api/v1/models"));
    if (!endpoint.isValid() || endpoint.host().isEmpty()) {
        emit connectionTested(false, provider == QStringLiteral("api")
                                          ? QStringLiteral("LLM API URL is invalid.")
                                          : QStringLiteral("LM Studio server URL is invalid."));
        return;
    }
    saveConfiguration();
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LA-Studio"));
    request.setRawHeader("Accept", "application/json");
    const QString apiKey = m_configuration.value(QStringLiteral("apiKey")).toString();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(10000);
    m_testing = true;
    emit stateChanged();
    QNetworkReply *pending = m_network->get(request);
    m_reply = pending;
    connect(pending, &QNetworkReply::finished, this, [this, pending]() {
        if (m_reply == pending) m_reply = nullptr;
        if (!m_testing) {
            pending->deleteLater();
            return;
        }
        m_testing = false;
        QNetworkReply *reply = pending;
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString model = m_configuration.value(QStringLiteral("model")).toString();
        bool found = false;
        const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
        const QJsonObject response = QJsonDocument::fromJson(body).object();
        const QJsonArray models = provider == QStringLiteral("api")
            ? response.value(QStringLiteral("data")).toArray()
            : response.value(QStringLiteral("models")).toArray();
        for (const QJsonValue &value : models) {
            const QJsonObject modelObject = value.toObject();
            if (modelObject.value(provider == QStringLiteral("api")
                                      ? QStringLiteral("id") : QStringLiteral("key")).toString() == model) {
                found = true;
                break;
            }
            const QJsonArray instances =
                modelObject.value(QStringLiteral("loaded_instances")).toArray();
            for (const QJsonValue &instance : instances) {
                if (instance.toObject().value(QStringLiteral("id")).toString() == model) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        const bool success = reply->error() == QNetworkReply::NoError
            && status >= 200 && status < 300 && found;
        QString message;
        if (success)
            message = QStringLiteral("Connected. Model \"%1\" is available.").arg(model);
        else if (reply->error() != QNetworkReply::NoError)
            message = QStringLiteral("Connection failed: %1").arg(reply->errorString());
        else if (!found)
            message = QStringLiteral("Connected, but model \"%1\" was not listed by %2.")
                          .arg(model, provider == QStringLiteral("api")
                                          ? QStringLiteral("the LLM API")
                                          : QStringLiteral("LM Studio"));
        else
            message = QStringLiteral("%1 returned HTTP %2: %3")
                          .arg(provider == QStringLiteral("api")
                                   ? QStringLiteral("LLM API") : QStringLiteral("LM Studio"))
                          .arg(status).arg(responseError(body));
        Logger::info(QStringLiteral("DubbingTranslationFix"),
                     QStringLiteral("Connection test success=%1 endpoint=%2 model=%3 message=%4")
                         .arg(success ? QStringLiteral("true") : QStringLiteral("false"),
                              reply->url().toString(), model, message));
        reply->deleteLater();
        emit stateChanged();
        emit connectionTested(success, message);
    });
}

void DubbingTranslationFixService::cancel()
{
    if (!m_busy && !m_testing) return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (m_cliProcess) {
        m_cliProcess->kill();
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }
    m_testing = false;
    if (m_busy) {
        setStatus(QStringLiteral("Translation fix cancelled."));
        setBusy(false);
    } else {
        emit stateChanged();
    }
    if (reply) {
        reply->abort();
        reply->deleteLater();
    }
}

void DubbingTranslationFixService::clearError()
{
    if (m_lastError.isEmpty()) return;
    m_lastError.clear();
    emit stateChanged();
}

void DubbingTranslationFixService::beginSegment()
{
    if (!m_busy) return;
    if (m_segmentPosition >= m_eligibleIndices.size()) {
        finishRun();
        return;
    }
    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    if (m_reconciliation) {
        m_originalTranslation.clear();
        m_promptTranslation.clear();
        m_lastCandidate.clear();
        m_bestCandidate.clear();
        m_seenCandidates.clear();
        m_lastCandidatePhonemes = 0;
        m_bestCandidatePhonemes = 0;
        m_promptPhonemes = 0;
        m_attempt = 0;
        setStatus(QStringLiteral("Preparing AI suggestion for conflict %1 of %2")
                      .arg(m_segmentPosition + 1).arg(m_eligibleIndices.size()));
        requestAttempt();
        return;
    }
    m_originalTranslation =
        segment.value(QStringLiteral("targetText")).toString().trimmed();
    m_promptTranslation = m_originalTranslation;
    m_lastCandidate.clear();
    m_bestCandidate.clear();
    m_seenCandidates.clear();
    m_lastCandidatePhonemes = actualPhonemeCount(segment, m_targetLanguage);
    m_bestCandidatePhonemes = m_lastCandidatePhonemes;
    m_promptPhonemes = m_lastCandidatePhonemes;
    m_attempt = 0;
    setStatus(QStringLiteral("Fixing segment %1 of %2")
                  .arg(m_segmentPosition + 1).arg(m_eligibleIndices.size()));
    requestAttempt();
}

void DubbingTranslationFixService::requestAttempt()
{
    if (!m_busy) return;
    const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
    if (provider == QStringLiteral("cli")) {
        executeCliAttempt();
        return;
    }

    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    const bool directColab = provider == QStringLiteral("colab-direct");
    const bool openAiCompatible = provider == QStringLiteral("api") || directColab;
    QJsonObject payload;
    payload.insert(QStringLiteral("model"),
                   m_configuration.value(QStringLiteral("model")).toString());
    payload.insert(QStringLiteral("temperature"),
                   m_configuration.value(QStringLiteral("temperature")).toDouble());
    payload.insert(openAiCompatible ? QStringLiteral("max_tokens")
                                    : QStringLiteral("max_output_tokens"), 384);
    payload.insert(QStringLiteral("stream"), false);
    if (!openAiCompatible) {
        payload.insert(QStringLiteral("store"), false);
        payload.insert(QStringLiteral("reasoning"), QStringLiteral("off"));
    }
    payload.insert(QStringLiteral("top_p"), 0.8);
    if (!openAiCompatible)
        payload.insert(QStringLiteral("top_k"), 20);
    const QString systemPrompt = m_reconciliation
        ? QStringLiteral("You reconcile two conflicting transcript observations for timed dubbing. This is a text-only task: do not call tools. Return only one concise proposed source-language transcript. Preserve names, numbers, negation, meaning, and the language of the supplied source observations. Do not translate it and do not add analysis, labels, or quotes.")
        : translationRepairSystemPrompt();
    if (openAiCompatible) {
        payload.insert(QStringLiteral("messages"), QJsonArray{
            QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                        {QStringLiteral("content"), systemPrompt}},
            QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                        {QStringLiteral("content"), buildPrompt(segment)}}});
    } else {
        payload.insert(QStringLiteral("system_prompt"), systemPrompt);
        payload.insert(QStringLiteral("input"), buildPrompt(segment));
    }

    const QString base = directColab && m_directColabSession
        ? normalizedServerBase(m_directColabSession->endpoint().toString())
        : normalizedServerBase(m_configuration.value(QStringLiteral("serverUrl")).toString());
    QNetworkRequest request(QUrl(openAiCompatible
                                     ? base + QStringLiteral("/v1/chat/completions")
                                     : base + QStringLiteral("/api/v1/chat")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LA-Studio"));
    request.setRawHeader("Accept", "application/json");
    const QString apiKey = directColab && m_directColabSession
        ? m_directColabSession->bearerTokenForRequest()
        : m_configuration.value(QStringLiteral("apiKey")).toString();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(120000);

    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
            QStringLiteral("Request operation=%1 segment=%2 attempt=%3/%4 currentPhonemes=%5")
            .arg(m_reconciliation ? QStringLiteral("reconcile") : QStringLiteral("rewrite"))
            .arg(segment.value(QStringLiteral("id")).toString())
            .arg(m_attempt + 1).arg(m_maxAttempts).arg(m_lastCandidatePhonemes));
    QNetworkReply *pending = m_network->post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_reply = pending;
    connect(pending, &QNetworkReply::finished, this, [this, pending]() {
        if (m_reply == pending) m_reply = nullptr;
        handleAttemptResponse(pending);
    });
}

void DubbingTranslationFixService::executeCliAttempt()
{
    if (!m_busy) return;
    const QVariantMap segment =
        m_segments.at(m_eligibleIndices.at(m_segmentPosition)).toMap();
    const QString cliAgent = m_configuration.value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
    const QString model = m_configuration.value(QStringLiteral("model")).toString();

    const QString exePath = cliExecutablePath(cliAgent);
    if (exePath.isEmpty()) {
        const CliInvocation unresolved =
            cliInvocation(cliAgent, model, {}, {}, {}, 180);
        setError(QStringLiteral("CLI Agent binary '%1' is not found on system PATH.")
                     .arg(unresolved.binaryName));
        return;
    }

    const QString systemPrompt = m_reconciliation
        ? QStringLiteral("You reconcile two conflicting transcript observations for timed dubbing. This is a text-only task: do not call tools. Return only one concise proposed source-language transcript. Preserve names, numbers, negation, meaning, and the language of the supplied source observations. Do not translate it and do not add analysis, labels, or quotes.")
        : translationRepairSystemPrompt();
    const QString fullPrompt = systemPrompt + QStringLiteral("\n\n") + buildPrompt(segment);
    const QString logPath = createCliDiagnosticLogPath(cliAgent);
    const CliInvocation invocation =
        cliInvocation(cliAgent, model, fullPrompt, exePath, logPath, 180);

    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
            QStringLiteral("CLI Request operation=%1 agent=%2 program=%3 segment=%4 attempt=%5/%6 currentPhonemes=%7")
            .arg(m_reconciliation ? QStringLiteral("reconcile") : QStringLiteral("rewrite"))
            .arg(cliAgent, invocation.binaryName,
                 segment.value(QStringLiteral("id")).toString())
            .arg(m_attempt + 1).arg(m_maxAttempts).arg(m_lastCandidatePhonemes));

    QProcess *process = new QProcess(this);
    process->setWorkingDirectory(invocation.workingDirectory);
    connect(process, &QObject::destroyed,
            [logPath = invocation.diagnosticLogPath]() {
        if (!logPath.isEmpty()) QFile::remove(logPath);
    });
    m_cliProcess = process;

    connect(process, &QProcess::finished, this,
            [this, process, invocation](int exitCode,
                                        QProcess::ExitStatus status) {
        if (m_cliProcess == process) m_cliProcess = nullptr;
        const QByteArray diagnosticLog =
            takeCliDiagnosticLog(invocation.diagnosticLogPath);
        if (!m_busy) {
            process->deleteLater();
            return;
        }

        const QByteArray stdoutData = process->readAllStandardOutput();
        const QByteArray stderrData = process->readAllStandardError();
        process->deleteLater();
        const QString classifiedFailure = classifiedCliFailure(
            invocation.agentId, stdoutData, stderrData, diagnosticLog);
        if (exitCode != 0 || status != QProcess::NormalExit
            || !classifiedFailure.isEmpty()) {
            const QString detail = classifiedFailure.isEmpty()
                ? cliFailureMessage(invocation.agentId, stdoutData,
                                    stderrData, diagnosticLog)
                : classifiedFailure;
            setError(QStringLiteral("CLI Agent process failed (exit code %1): %2")
                         .arg(exitCode).arg(detail));
            return;
        }

        const QString candidate = parseCliResponse(stdoutData);
        if (candidate.isEmpty()) {
            setError(QStringLiteral("CLI Agent did not return a translation: %1")
                         .arg(cliFailureMessage(invocation.agentId, stdoutData,
                                                stderrData, diagnosticLog)));
            return;
        }

        processCandidate(candidate);
    });

    Logger::info(
        QStringLiteral("DubbingTranslationFix"),
        QStringLiteral("CLI rewrite launch agent=%1 executable=%2 args=%3 promptViaStdin=%4 diagnosticLog=%5")
            .arg(invocation.agentId, invocation.program,
                 cliArgumentsForLog(invocation),
                 invocation.promptViaStdin ? QStringLiteral("true")
                                           : QStringLiteral("false"),
                 invocation.diagnosticLogPath.isEmpty()
                     ? QStringLiteral("disabled") : QStringLiteral("enabled")));
    connect(process, &QProcess::errorOccurred, this,
            [this, process, invocation](QProcess::ProcessError processError) {
        if (processError != QProcess::FailedToStart || m_cliProcess != process) return;
        takeCliDiagnosticLog(invocation.diagnosticLogPath);
        process->deleteLater();
        m_cliProcess = nullptr;
        setError(QStringLiteral("Failed to launch CLI Agent binary '%1'.")
                     .arg(invocation.binaryName));
    });
    connect(process, &QProcess::started, this, [process, fullPrompt, invocation]() {
        if (invocation.promptViaStdin) process->write(fullPrompt.toUtf8());
        process->closeWriteChannel();
    });
    process->start(invocation.program, invocation.arguments);

    QTimer::singleShot(180000, process, [this, process]() {
        if (m_cliProcess != process || !m_busy || !process->state()) return;
        process->kill();
        setError(QStringLiteral("CLI Agent timed out while rewriting the translation."));
    });
}

QString DubbingTranslationFixService::parseCliResponse(const QByteArray &body)
{
    const QString raw = QString::fromUtf8(body).trimmed();
    if (raw.isEmpty()) return {};

    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString lastMessage;
    QString accumulatedText;

    for (const QString &line : lines) {
        const QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        const QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("result")) && obj.value(QStringLiteral("result")).isString()) {
            return cleanAssistantText(obj.value(QStringLiteral("result")).toString());
        }
        // Codex exec --json emits JSONL events such as:
        // {"type":"item.completed","item":{"type":"agent_message","text":"..."}}
        const QJsonObject item = obj.value(QStringLiteral("item")).toObject();
        if (item.value(QStringLiteral("type")).toString() == QStringLiteral("agent_message")) {
            const QString text = item.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) lastMessage = text;
            continue;
        }
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("assistant")) {
            const QJsonObject message = obj.value(QStringLiteral("message")).toObject();
            const QJsonArray contentArr = message.value(QStringLiteral("content")).toArray();
            for (const QJsonValue &val : contentArr) {
                if (val.isObject() && val.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                    accumulatedText += val.toObject().value(QStringLiteral("text")).toString();
                }
            }
        }
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("agent_message") ||
            obj.contains(QStringLiteral("content"))) {
            if (obj.value(QStringLiteral("content")).isString()) {
                accumulatedText += obj.value(QStringLiteral("content")).toString();
            }
        }
    }

    if (!lastMessage.isEmpty())
        return cleanAssistantText(lastMessage);
    if (!accumulatedText.isEmpty()) {
        return cleanAssistantText(accumulatedText);
    }

    return cleanAssistantText(raw);
}

void DubbingTranslationFixService::handleAttemptResponse(QNetworkReply *reply)
{
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!m_busy) {
        reply->deleteLater();
        return;
    }
    if (reply->error() != QNetworkReply::NoError
        || status < 200 || status >= 300) {
        const QString detail = reply->error() != QNetworkReply::NoError
            ? reply->errorString() : responseError(body);
        reply->deleteLater();
        const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
        setError(QStringLiteral("%1 request failed (HTTP %2): %3")
                     .arg(provider == QStringLiteral("api") || provider == QStringLiteral("colab-direct")
                              ? (provider == QStringLiteral("colab-direct")
                                     ? QStringLiteral("Direct Colab LLM")
                                     : QStringLiteral("LLM API"))
                              : QStringLiteral("LM Studio"))
                     .arg(status).arg(detail));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    QString content;
    const QString provider = m_configuration.value(QStringLiteral("provider")).toString();
    if (provider == QStringLiteral("api") || provider == QStringLiteral("colab-direct")) {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty())
            content = choices.first().toObject().value(QStringLiteral("message"))
                          .toObject().value(QStringLiteral("content")).toString();
    }
    const QJsonArray output = root.value(QStringLiteral("output")).toArray();
    for (const QJsonValue &value : output) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("type")).toString()
            == QStringLiteral("message")) {
            content = item.value(QStringLiteral("content")).toString();
            if (!content.isEmpty()) break;
        }
    }
    const QString candidate = cleanAssistantText(content);
    reply->deleteLater();
    if (candidate.isEmpty()) {
        setError(provider == QStringLiteral("api")
                     ? QStringLiteral("LLM API returned an empty translation.")
                     : provider == QStringLiteral("colab-direct")
                         ? QStringLiteral("Direct Colab LLM returned an empty translation.")
                         : QStringLiteral("LM Studio returned an empty translation."));
        return;
    }

    processCandidate(candidate);
}

