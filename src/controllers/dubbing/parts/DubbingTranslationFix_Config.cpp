QVariantMap DubbingTranslationFixService::normalizedConfiguration(
    const QVariantMap &configuration)
{
    QVariantMap result;
    QString provider = configuration.value(QStringLiteral("provider"),
                                           QStringLiteral("lmstudio"))
                           .toString().trimmed().toLower();
    if (provider != QStringLiteral("api") && provider != QStringLiteral("local")
        && provider != QStringLiteral("cli") && provider != QStringLiteral("colab-direct"))
        provider = QStringLiteral("lmstudio");
    result.insert(QStringLiteral("provider"), provider);

    QString cliAgent = configuration.value(QStringLiteral("cliAgent"),
                                            QStringLiteral("claude"))
                           .toString().trimmed().toLower();
    if (cliAgent != QStringLiteral("codex") && cliAgent != QStringLiteral("antigravity"))
        cliAgent = QStringLiteral("claude");
    result.insert(QStringLiteral("cliAgent"), cliAgent);
    result.insert(QStringLiteral("configured"),
                  configuration.value(QStringLiteral("configured"), false).toBool());
    // Reconciliation is a structured LLM task, not ordinary translation.
    // Keep this explicit so an arbitrary OpenAI-compatible endpoint or a
    // machine-translation model is never assumed capable without user setup.
    result.insert(QStringLiteral("supportsStructuredReconciliation"),
                  configuration.value(QStringLiteral("supportsStructuredReconciliation"),
                                      false).toBool());
    result.insert(QStringLiteral("serverUrl"),
                  provider == QStringLiteral("colab-direct")
                      ? QString()
                      : configuration.value(QStringLiteral("serverUrl"),
                                            QStringLiteral("http://127.0.0.1:1234"))
                            .toString().trimmed());
    result.insert(QStringLiteral("model"),
                  configuration.value(QStringLiteral("model"),
                                      QStringLiteral("qwen3.5-2b"))
                      .toString().trimmed());
    // A remote route must never retain a local runtime/model selection.  It
    // otherwise makes a later Automatic run look local and can enqueue a
    // download after the user explicitly chose Colab or the API Gateway.
    const bool localProvider = provider == QStringLiteral("local");
    result.insert(QStringLiteral("runtimeId"), localProvider
                      ? configuration.value(QStringLiteral("runtimeId")).toString().trimmed()
                      : QString());
    result.insert(QStringLiteral("runtimeVersion"), localProvider
                      ? configuration.value(QStringLiteral("runtimeVersion")).toString().trimmed()
                      : QString());
    result.insert(QStringLiteral("selectedFiles"), localProvider
                      ? configuration.value(QStringLiteral("selectedFiles")).toMap()
                      : QVariantMap());
    result.insert(QStringLiteral("apiKey"), provider == QStringLiteral("colab-direct")
                      ? QString()
                      : configuration.value(QStringLiteral("apiKey")).toString().trimmed());
    result.insert(QStringLiteral("maxAttempts"),
                  qBound(1, configuration.value(QStringLiteral("maxAttempts"), 4).toInt(), 8));
    result.insert(QStringLiteral("temperature"),
                  qBound(0.0, configuration.value(QStringLiteral("temperature"), 0.35).toDouble(), 1.5));
    return result;
}

bool DubbingTranslationFixService::reconciliationAvailable(
    const QVariantMap &configuration, QString *reason)
{
    const QVariantMap normalized = normalizedConfiguration(configuration);
    const QString provider = normalized.value(QStringLiteral("provider")).toString();
    const auto reject = [reason](const QString &message) {
        if (reason) *reason = message;
        return false;
    };
    if (!normalized.value(QStringLiteral("configured")).toBool()) {
        return reject(QStringLiteral("AI suggestion requires the project Translation Fix LLM to be configured. A plain translation model is not used for reconciliation."));
    }
    if (!normalized.value(QStringLiteral("supportsStructuredReconciliation")).toBool()) {
        return reject(QStringLiteral("AI suggestion is disabled until this Translation Fix model is explicitly marked as supporting structured source-language reconciliation. Plain translation models are not used."));
    }
    if (provider == QStringLiteral("local")) {
        return reject(QStringLiteral("The selected local Translate model is a translation runtime, not a structured reconciliation LLM. Configure Translation Fix LLM or choose a manual policy."));
    }
    else if (provider == QStringLiteral("cli")) {
        const QString agent = normalized.value(QStringLiteral("cliAgent")).toString();
        if (cliExecutablePath(agent).isEmpty()) {
            return reject(QStringLiteral("The configured Translation Fix CLI is unavailable, so AI suggestion cannot run."));
        }
    } else if (provider != QStringLiteral("colab-direct")) {
        const QString base = normalizedServerBase(
            normalized.value(QStringLiteral("serverUrl")).toString());
        const QUrl endpoint(provider == QStringLiteral("api")
                                ? base + QStringLiteral("/v1/chat/completions")
                                : base + QStringLiteral("/api/v1/chat"));
        if (!endpoint.isValid() || endpoint.host().isEmpty()
            || normalized.value(QStringLiteral("model")).toString().trimmed().isEmpty()) {
            return reject(QStringLiteral("The configured Translation Fix LLM route or model is incomplete."));
        }
    }
    if (reason) reason->clear();
    return true;
}

QString DubbingTranslationFixService::cliExecutablePath(const QString &cliAgent)
{
    const QString normalized = cliAgent.trimmed().toLower();
    QString program = QStringLiteral("claude");
    if (normalized == QStringLiteral("codex"))
        program = QStringLiteral("codex");
    else if (normalized == QStringLiteral("antigravity"))
        program = QStringLiteral("agy");

    const QString fromPath = QStandardPaths::findExecutable(program);
    if (!fromPath.isEmpty()) return fromPath;

#ifdef Q_OS_WIN
    QStringList candidates;
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString userProfile = qEnvironmentVariable("USERPROFILE");
    if (normalized == QStringLiteral("antigravity") && !localAppData.isEmpty()) {
        candidates << localAppData + QStringLiteral("/agy/bin/agy.exe");
    } else if (normalized == QStringLiteral("codex")) {
        if (!appData.isEmpty())
            candidates << appData + QStringLiteral("/npm/codex.cmd");
        if (!localAppData.isEmpty())
            candidates << localAppData + QStringLiteral("/Programs/codex/codex.exe");
    } else if (normalized == QStringLiteral("claude")) {
        if (!userProfile.isEmpty())
            candidates << userProfile + QStringLiteral("/.local/bin/claude.exe");
        if (!localAppData.isEmpty())
            candidates << localAppData + QStringLiteral("/Programs/claude/claude.exe");
    }
    for (const QString &candidate : std::as_const(candidates)) {
        const QFileInfo info(candidate);
        if (info.isFile()) return info.absoluteFilePath();
    }
#endif
    return {};
}

QVariantList DubbingTranslationFixService::cliModelOptions(
    const QString &cliAgent, const QString &homePath)
{
    QString agent = cliAgent.trimmed().toLower();
    if (agent != QStringLiteral("codex")
        && agent != QStringLiteral("antigravity"))
        agent = QStringLiteral("claude");

    const QString home = homePath.trimmed().isEmpty()
        ? QDir::homePath() : QDir(homePath).absolutePath();
    QVariantList models;
    QSet<QString> seen;
    const QString configured = configuredCliModel(agent, home);
    appendCliModel(
        models, seen, QStringLiteral("default"),
        QStringLiteral("Default (CLI config)"),
        configured.isEmpty()
            ? QStringLiteral("Use the model selected by the CLI")
            : QStringLiteral("Configured model: %1").arg(configured));
    if (!configured.isEmpty()) {
        appendCliModel(models, seen, configured, configured,
                       QStringLiteral("Selected in the CLI configuration"));
    }

    if (agent == QStringLiteral("claude")) {
        const QJsonObject usage = readJsonObject(
            QDir(home).filePath(QStringLiteral(".claude/stats-cache.json")))
                                      .value(QStringLiteral("modelUsage"))
                                      .toObject();
        for (auto it = usage.constBegin(); it != usage.constEnd(); ++it) {
            appendCliModel(models, seen, it.key(), it.key(),
                           QStringLiteral("Found in Claude Code usage cache"));
        }
        const QList<QPair<QString, QString>> fallbacks = {
            {QStringLiteral("sonnet"), QStringLiteral("Sonnet (latest alias)")},
            {QStringLiteral("opus"), QStringLiteral("Opus (latest alias)")},
            {QStringLiteral("haiku"), QStringLiteral("Haiku (latest alias)")}
        };
        for (const auto &option : fallbacks) {
            appendCliModel(models, seen, option.first, option.second,
                           QStringLiteral("Claude Code model alias"));
        }
        return models;
    }

    if (agent == QStringLiteral("codex")) {
        const QJsonArray cachedModels = readJsonObject(
            QDir(home).filePath(QStringLiteral(".codex/models_cache.json")))
                                            .value(QStringLiteral("models"))
                                            .toArray();
        for (const QJsonValue &value : cachedModels) {
            const QJsonObject item = value.toObject();
            if (item.value(QStringLiteral("visibility")).toString()
                == QStringLiteral("hidden"))
                continue;
            const QString id =
                item.value(QStringLiteral("slug")).toString().trimmed();
            appendCliModel(
                models, seen, id,
                item.value(QStringLiteral("display_name")).toString(),
                QStringLiteral("Available in the Codex model cache"));
        }
        const QStringList fallbacks = {
            QStringLiteral("gpt-5.6-sol"),
            QStringLiteral("gpt-5.6-terra"),
            QStringLiteral("gpt-5.6-luna"),
            QStringLiteral("gpt-5.5"),
            QStringLiteral("gpt-5.4"),
            QStringLiteral("gpt-5.4-mini")
        };
        for (const QString &model : fallbacks) {
            appendCliModel(models, seen, model, model,
                           QStringLiteral("Codex CLI fallback model"));
        }
        return models;
    }

    // agy 1.1.5 exposes these exact ids through `agy models`. Unlike the
    // display labels used by agy 1.0.3 and the older Open Design adapter,
    // these slugs are accepted directly by the current --model option.
    const QList<QPair<QString, QString>> antigravityModels = {
        {QStringLiteral("gemini-3.6-flash-high"),
         QStringLiteral("Gemini 3.6 Flash (High)")},
        {QStringLiteral("gemini-3.6-flash-medium"),
         QStringLiteral("Gemini 3.6 Flash (Medium)")},
        {QStringLiteral("gemini-3.6-flash-low"),
         QStringLiteral("Gemini 3.6 Flash (Low)")},
        {QStringLiteral("gemini-3.5-flash-high"),
         QStringLiteral("Gemini 3.5 Flash (High)")},
        {QStringLiteral("gemini-3.5-flash-medium"),
         QStringLiteral("Gemini 3.5 Flash (Medium)")},
        {QStringLiteral("gemini-3.5-flash-low"),
         QStringLiteral("Gemini 3.5 Flash (Low)")},
        {QStringLiteral("gemini-3.1-pro-high"),
         QStringLiteral("Gemini 3.1 Pro (High)")},
        {QStringLiteral("gemini-3.1-pro-low"),
         QStringLiteral("Gemini 3.1 Pro (Low)")},
        {QStringLiteral("claude-sonnet-4-6"),
         QStringLiteral("Claude Sonnet 4.6")},
        {QStringLiteral("claude-opus-4-6-thinking"),
         QStringLiteral("Claude Opus 4.6 (Thinking)")},
        {QStringLiteral("gpt-oss-120b-medium"),
         QStringLiteral("GPT-OSS 120B (Medium)")}
    };
    for (const auto &option : antigravityModels) {
        appendCliModel(models, seen, option.first, option.second,
                       QStringLiteral("Available from Antigravity CLI"));
    }
    return models;
}

DubbingTranslationFixService::CliInvocation
DubbingTranslationFixService::cliInvocation(
    const QString &cliAgent, const QString &model,
    const QString &prompt, const QString &executablePath,
    const QString &diagnosticLogPath, int timeoutSeconds)
{
    CliInvocation invocation;
    invocation.agentId = cliAgent.trimmed().toLower();
    invocation.program = executablePath;
    invocation.workingDirectory = QDir::tempPath();

    if (invocation.agentId == QStringLiteral("codex")) {
        invocation.binaryName = QStringLiteral("codex");
        invocation.displayName = QStringLiteral("Codex CLI");
        invocation.arguments
            << QStringLiteral("exec") << QStringLiteral("--json")
            << QStringLiteral("--ephemeral")
            << QStringLiteral("--sandbox") << QStringLiteral("read-only")
            << QStringLiteral("--skip-git-repo-check");
    } else if (invocation.agentId == QStringLiteral("antigravity")) {
        invocation.binaryName = QStringLiteral("agy");
        invocation.displayName = QStringLiteral("Google Antigravity");
        invocation.diagnosticLogPath = diagnosticLogPath;
        if (!diagnosticLogPath.isEmpty()) {
            // agy requires --log-file before -p for the diagnostic file to be
            // populated reliably.
            invocation.arguments
                << QStringLiteral("--log-file") << diagnosticLogPath;
        }
        invocation.arguments
            // Keep terminal activity restricted to agy's sandbox while
            // auto-approving requests that a headless process cannot prompt for.
            << QStringLiteral("--sandbox")
            << QStringLiteral("--dangerously-skip-permissions")
            << QStringLiteral("--print-timeout")
            << QStringLiteral("%1s").arg(qMax(1, timeoutSeconds));
        // agy 1.1.5 treats `-p -` as the literal one-character prompt "-".
        // Supply the prompt as -p's value instead of writing it to stdin.
        invocation.promptViaStdin = false;
    } else {
        invocation.agentId = QStringLiteral("claude");
        invocation.binaryName = QStringLiteral("claude");
        invocation.displayName = QStringLiteral("Claude Code");
        invocation.arguments
            << QStringLiteral("-p")
            << QStringLiteral("--input-format") << QStringLiteral("text")
            << QStringLiteral("--output-format") << QStringLiteral("json")
            << QStringLiteral("--no-session-persistence")
            // Dubbing repair only needs a text response; disable Claude's tools.
            << QStringLiteral("--tools") << QString();
    }

    if (!model.isEmpty() && model != QStringLiteral("default"))
        invocation.arguments << QStringLiteral("--model") << model;
    if (invocation.agentId == QStringLiteral("antigravity"))
        invocation.arguments << QStringLiteral("-p") << prompt;

#ifdef Q_OS_WIN
    const QString lowerExe = executablePath.toLower();
    if (lowerExe.endsWith(QStringLiteral(".cmd"))
        || lowerExe.endsWith(QStringLiteral(".bat"))) {
        invocation.program = QStringLiteral("cmd.exe");
        invocation.arguments.prepend(executablePath);
        invocation.arguments.prepend(QStringLiteral("/c"));
    }
#endif
    return invocation;
}

QString DubbingTranslationFixService::cliFailureMessage(
    const QString &cliAgent, const QByteArray &stdoutData,
    const QByteArray &stderrData, const QByteArray &diagnosticLog)
{
    const QString classified = classifiedCliFailure(
        cliAgent, stdoutData, stderrData, diagnosticLog);
    if (!classified.isEmpty()) return classified;
    if (!stderrData.trimmed().isEmpty()) return cliConnectionError(stderrData);
    if (!stdoutData.trimmed().isEmpty()
        && containsCliAuthSuccess(QString::fromUtf8(diagnosticLog)))
        return {};
    if (!diagnosticLog.trimmed().isEmpty()
        && !containsCliAuthSuccess(QString::fromUtf8(diagnosticLog)))
        return cliConnectionError(diagnosticLog);
    return QStringLiteral("The CLI completed without returning a model response.");
}

void DubbingTranslationFixService::setConfiguration(const QVariantMap &configuration)
{
    if (m_busy || m_testing) return;
    m_configuration = normalizedConfiguration(configuration);
    saveConfiguration();
    emit stateChanged();
}

QUrl DubbingTranslationFixService::chatUrl(const QString &serverUrl)
{
    return QUrl(normalizedServerBase(serverUrl) + QStringLiteral("/api/v1/chat"));
}

QUrl DubbingTranslationFixService::modelsUrl(const QString &serverUrl)
{
    return QUrl(normalizedServerBase(serverUrl) + QStringLiteral("/api/v1/models"));
}

QString DubbingTranslationFixService::cleanAssistantText(const QString &content)
{
    QString result = content.trimmed();
    result.remove(QRegularExpression(QStringLiteral("<think>.*?</think>"),
                                     QRegularExpression::DotMatchesEverythingOption
                                         | QRegularExpression::CaseInsensitiveOption));
    result = result.trimmed();
    if (result.startsWith(QStringLiteral("```"))) {
        result.remove(QRegularExpression(QStringLiteral("^```(?:text|json)?\\s*"),
                                         QRegularExpression::CaseInsensitiveOption));
        result.remove(QRegularExpression(QStringLiteral("\\s*```$")));
    }
    result.remove(QRegularExpression(
        QStringLiteral("^(?:translation|revised translation|bản dịch|câu viết lại)\\s*:\\s*"),
        QRegularExpression::CaseInsensitiveOption));
    result = result.trimmed();
    if (result.size() >= 2
        && ((result.startsWith(QLatin1Char('"')) && result.endsWith(QLatin1Char('"')))
            || (result.startsWith(QChar(0x201c)) && result.endsWith(QChar(0x201d)))))
        result = result.mid(1, result.size() - 2).trimmed();
    return result;
}

int DubbingTranslationFixService::eligibleSegmentCount(
    const QVariantList &segments, const QString &targetLanguage)
{
    int count = 0;
    for (const QVariant &value : segments) {
        if (isOverBudget(value.toMap(), targetLanguage)) ++count;
    }
    return count;
}

bool DubbingTranslationFixService::isCloserToBudget(
    int currentPhonemes, int candidatePhonemes, int minimum, int maximum)
{
    return distanceToBudget(candidatePhonemes, minimum, maximum)
        < distanceToBudget(currentPhonemes, minimum, maximum);
}

void DubbingTranslationFixService::saveConfiguration()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    const bool directColab = m_configuration.value(QStringLiteral("provider"))
        .toString() == QStringLiteral("colab-direct");
    settings.setValue(QStringLiteral("dubbing/adaptiveProvider"),
                      m_configuration.value(QStringLiteral("provider")));
    settings.setValue(QStringLiteral("dubbing/adaptiveCliAgent"),
                      m_configuration.value(QStringLiteral("cliAgent")));
    settings.setValue(QStringLiteral("dubbing/adaptiveConfigured"),
                      m_configuration.value(QStringLiteral("configured")));
    // Colab URL/token are intentionally memory-only.  More importantly, a
    // Direct Colab choice must not erase a separately configured API Gateway
    // endpoint or credential that the user may choose again later.
    if (!directColab) {
        settings.setValue(QStringLiteral("dubbing/translationFixServerUrl"),
                          m_configuration.value(QStringLiteral("serverUrl")));
    }
    settings.setValue(QStringLiteral("dubbing/translationFixModel"),
                      m_configuration.value(QStringLiteral("model")));
    settings.setValue(QStringLiteral("dubbing/adaptiveRuntimeId"),
                      m_configuration.value(QStringLiteral("runtimeId")));
    settings.setValue(QStringLiteral("dubbing/adaptiveRuntimeVersion"),
                      m_configuration.value(QStringLiteral("runtimeVersion")));
    settings.setValue(QStringLiteral("dubbing/adaptiveSelectedFiles"),
                      m_configuration.value(QStringLiteral("selectedFiles")));
    if (!directColab) {
        QString credentialError;
        if (!SecureCredentialStore::write(settings, QStringLiteral("dubbing-translation-fix"),
                                          m_configuration.value(QStringLiteral("apiKey")).toString(),
                                          &credentialError)) {
            Logger::error(QStringLiteral("DubbingTranslationFixService"),
                          QStringLiteral("Translation API credential was not persisted: %1").arg(credentialError));
        }
    }
    settings.remove(QStringLiteral("dubbing/translationFixApiKey"));
    settings.setValue(QStringLiteral("dubbing/translationFixMaxAttempts"),
                      m_configuration.value(QStringLiteral("maxAttempts")));
    settings.setValue(QStringLiteral("dubbing/translationFixTemperature"),
                      m_configuration.value(QStringLiteral("temperature")));
    settings.sync();
}

