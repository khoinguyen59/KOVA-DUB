#include "controllers/dubbing/DubbingTranslationFixService.h"

#include "core/utils/Logger.h"
#include "core/storage/PathUtils.h"
#include "core/storage/SecureCredentialStore.h"
#include "dubbing/timing/DubbingDuration.h"
#include "dubbing/timing/EspeakNgPhonemizer.h"
#include "remote/colab/ColabSession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

namespace LAStudio {
namespace {

QString settingsPath()
{
    return PathUtils::dataDir() + QStringLiteral("/settings.ini");
}

QString normalizedServerBase(QString value)
{
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('/'))) value.chop(1);
    const QStringList suffixes = {
        QStringLiteral("/api/v1/chat"),
        QStringLiteral("/v1/chat/completions"),
        QStringLiteral("/api/v1"),
        QStringLiteral("/v1")
    };
    for (const QString &suffix : suffixes) {
        if (!value.endsWith(suffix, Qt::CaseInsensitive)) continue;
        value.chop(suffix.size());
        break;
    }
    return value;
}

int actualPhonemeCount(const QVariantMap &segment, const QString &language)
{
    return EspeakNgPhonemizer::count(
        segment.value(QStringLiteral("targetText")).toString(), language);
}

bool isOverBudget(const QVariantMap &segment, const QString &language)
{
    const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
    if (budget.isEmpty()
        || segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty())
        return false;
    const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
    const int phonemes = actualPhonemeCount(segment, language);
    if (phonemes < 0) return false;
    return phonemes > maximum;
}

int distanceToBudget(int phonemes, int minimum, int maximum)
{
    if (phonemes < minimum) return minimum - phonemes;
    if (phonemes > maximum) return phonemes - maximum;
    return 0;
}

QString responseError(const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) return QString::fromUtf8(body).trimmed();
    const QJsonValue error = document.object().value(QStringLiteral("error"));
    if (error.isString()) return error.toString();
    if (error.isObject())
        return error.toObject().value(QStringLiteral("message")).toString();
    return {};
}

QString cliConnectionError(const QByteArray &stderrData)
{
    QString detail = QString::fromUtf8(stderrData).trimmed();
    if (detail.contains(QStringLiteral("not logged into"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("authentication required"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("not authenticated"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("login required"), Qt::CaseInsensitive)) {
        return QStringLiteral("CLI authentication is required. Open a terminal, run the selected CLI, and complete its sign-in flow.");
    }
    if (detail.contains(QStringLiteral("unauthorized"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("invalid api key"), Qt::CaseInsensitive)) {
        return QStringLiteral("CLI authentication was rejected. Sign in again and retry.");
    }
    if (detail.isEmpty())
        return QStringLiteral("The CLI completed without returning a model response.");

    const QStringList lines = detail.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    detail = lines.isEmpty() ? detail : lines.constLast().trimmed();
    if (detail.size() > 500) detail = detail.left(500) + QChar(0x2026);
    return detail;
}

QString translationRepairSystemPrompt()
{
    return QStringLiteral(
        "You repair translations for timed dubbing. This is a text-only "
        "transformation: do not call command, terminal, filesystem, web, search, "
        "or any other tool. Preserve the complete source meaning and the meaning "
        "of the current translation: facts, names, numbers, rank/order, time, "
        "comparison, causality, and negation. Rewrite naturally in the requested "
        "target language while meeting the supplied eSpeak NG phoneme maximum. "
        "Never invent or omit information. Return only the rewritten translation, "
        "without analysis, labels, quotes, or a phoneme count.");
}

QString createCliDiagnosticLogPath(const QString &cliAgent)
{
    if (cliAgent != QStringLiteral("antigravity")) return {};
    QTemporaryFile file(
        QDir(QDir::tempPath()).filePath(
            QStringLiteral("la-studio-agy-XXXXXX.log")));
    file.setAutoRemove(false);
    if (!file.open()) return {};
    const QString path = file.fileName();
    file.close();
    return path;
}

QByteArray takeCliDiagnosticLog(const QString &path)
{
    if (path.isEmpty()) return {};
    QFile file(path);
    QByteArray result;
    if (file.open(QIODevice::ReadOnly)) result = file.readAll();
    file.close();
    QFile::remove(path);
    return result;
}

bool containsCliAuthFailure(const QString &detail)
{
    return detail.contains(QStringLiteral("not logged into"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("authentication required"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("authentication timed out"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("not authenticated"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("login required"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("\"authenticated\":false"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("\"loggedIn\":false"), Qt::CaseInsensitive);
}

bool containsCliAuthSuccess(const QString &detail)
{
    // agy 1.1.5 may log transient "not logged into" errors while its backend
    // starts, then silently load a valid token from the OS keyring. Treat the
    // later successful authentication markers as the final state.
    return detail.contains(QStringLiteral("silent auth succeeded"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("authenticated via keyring"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("OAuth: authenticated successfully"), Qt::CaseInsensitive);
}

QString classifiedCliFailure(const QString &cliAgent,
                             const QByteArray &stdoutData,
                             const QByteArray &stderrData,
                             const QByteArray &diagnosticLog)
{
    const QString processDetail = QString::fromUtf8(
        stdoutData + QByteArrayLiteral("\n") + stderrData);
    const QString diagnosticDetail = QString::fromUtf8(diagnosticLog);
    const QString detail = processDetail + QLatin1Char('\n') + diagnosticDetail;
    if (detail.contains(QStringLiteral("RESOURCE_EXHAUSTED"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("individual quota reached"), Qt::CaseInsensitive)
        || (detail.contains(QStringLiteral("429"))
            && detail.contains(QStringLiteral("quota"), Qt::CaseInsensitive))) {
        return cliAgent == QStringLiteral("antigravity")
            ? QStringLiteral("Antigravity quota is exhausted for the selected model. Choose another model in LA Studio or wait for the quota to reset.")
            : QStringLiteral("The selected CLI model has reached its usage limit. Retry later or select another model.");
    }
    const bool processReportsAuthFailure = containsCliAuthFailure(processDetail);
    const bool diagnosticReportsFinalAuthFailure =
        containsCliAuthFailure(diagnosticDetail)
        && !containsCliAuthSuccess(diagnosticDetail);
    if (processReportsAuthFailure || diagnosticReportsFinalAuthFailure) {
        return cliAgent == QStringLiteral("antigravity")
            ? QStringLiteral("Antigravity authentication is required. Open a terminal, run agy once, complete Google sign-in, then retry.")
            : QStringLiteral("CLI authentication is required. Open a terminal, run the selected CLI, and complete its sign-in flow.");
    }
    if (detail.contains(QStringLiteral("unauthorized"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("invalid api key"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("oauth token expired"), Qt::CaseInsensitive)) {
        return QStringLiteral("CLI authentication was rejected. Sign in again and retry.");
    }
    if (detail.contains(QStringLiteral("headless mode cannot prompt"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("tool required the \"command\" permission"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("permission request was denied"), Qt::CaseInsensitive)) {
        return QStringLiteral("The CLI requested an interactive tool permission that cannot be approved in headless mode. Update the CLI and retry with the sandboxed non-interactive integration.");
    }
    if (detail.contains(QStringLiteral("unknown option"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("unknown flag"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("unexpected argument"), Qt::CaseInsensitive)) {
        return QStringLiteral("The installed CLI version does not support the required non-interactive options. Update the CLI and retry.");
    }
    return {};
}

QString cliArgumentsForLog(
    const DubbingTranslationFixService::CliInvocation &invocation)
{
    QStringList safeArguments = invocation.arguments;
    if (!invocation.promptViaStdin
        && invocation.agentId == QStringLiteral("antigravity")
        && !safeArguments.isEmpty()) {
        safeArguments.last() = QStringLiteral("<prompt>");
    }
    return safeArguments.join(QLatin1Char(' '));
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

QByteArray readLocalFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

void appendCliModel(QVariantList &models, QSet<QString> &seen,
                    const QString &value, const QString &text,
                    const QString &detail)
{
    const QString id = value.trimmed();
    if (id.isEmpty() || seen.contains(id)) return;
    seen.insert(id);
    models.append(QVariantMap{
        {QStringLiteral("value"), id},
        {QStringLiteral("text"), text.trimmed().isEmpty() ? id : text.trimmed()},
        {QStringLiteral("detail"), detail}
    });
}

QString configuredCliModel(const QString &agent, const QString &home)
{
    if (agent == QStringLiteral("claude")) {
        return readJsonObject(
                   QDir(home).filePath(QStringLiteral(".claude/settings.json")))
            .value(QStringLiteral("model")).toString().trimmed();
    }
    if (agent == QStringLiteral("codex")) {
        const QString config = QString::fromUtf8(readLocalFile(
            QDir(home).filePath(QStringLiteral(".codex/config.toml"))));
        const QRegularExpression modelPattern(
            QStringLiteral("^\\s*model\\s*=\\s*\"([^\"]+)\""),
            QRegularExpression::MultilineOption);
        const QRegularExpressionMatch match = modelPattern.match(config);
        return match.hasMatch() ? match.captured(1).trimmed() : QString();
    }
    return readJsonObject(
               QDir(home).filePath(
                   QStringLiteral(".gemini/antigravity-cli/settings.json")))
        .value(QStringLiteral("model")).toString().trimmed();
}

} // namespace

DubbingTranslationFixService::DubbingTranslationFixService(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    QString credentialError;
    m_configuration = normalizedConfiguration({
        {QStringLiteral("serverUrl"),
         settings.value(QStringLiteral("dubbing/translationFixServerUrl"),
                        QStringLiteral("http://127.0.0.1:1234")).toString()},
        {QStringLiteral("provider"),
         settings.value(QStringLiteral("dubbing/adaptiveProvider"),
                        settings.value(QStringLiteral("dubbing/cinematicProvider"),
                                       QStringLiteral("lmstudio"))).toString()},
        {QStringLiteral("cliAgent"),
         settings.value(QStringLiteral("dubbing/adaptiveCliAgent"),
                        QStringLiteral("claude")).toString()},
        {QStringLiteral("configured"),
         settings.value(QStringLiteral("dubbing/adaptiveConfigured"),
                        settings.value(QStringLiteral("dubbing/cinematicConfigured"), false)).toBool()},
        {QStringLiteral("model"),
         settings.value(QStringLiteral("dubbing/translationFixModel"),
                        QStringLiteral("qwen3.5-2b")).toString()},
        {QStringLiteral("runtimeId"),
         settings.value(QStringLiteral("dubbing/adaptiveRuntimeId")).toString()},
        {QStringLiteral("runtimeVersion"),
         settings.value(QStringLiteral("dubbing/adaptiveRuntimeVersion")).toString()},
        {QStringLiteral("selectedFiles"),
         settings.value(QStringLiteral("dubbing/adaptiveSelectedFiles")).toMap()},
        {QStringLiteral("apiKey"),
         SecureCredentialStore::migrateLegacy(settings, QStringLiteral("dubbing-translation-fix"),
                                               QStringLiteral("dubbing/translationFixApiKey"), &credentialError)},
        {QStringLiteral("maxAttempts"),
         settings.value(QStringLiteral("dubbing/translationFixMaxAttempts"), 4).toInt()},
        {QStringLiteral("temperature"),
         settings.value(QStringLiteral("dubbing/translationFixTemperature"), 0.35).toDouble()}
    });
    if (!credentialError.isEmpty()) {
        Logger::error(QStringLiteral("DubbingTranslationFixService"),
                      QStringLiteral("Translation API credential migration failed: %1").arg(credentialError));
    }
}


// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "controllers/dubbing/parts/DubbingTranslationFix_Config.cpp"
#include "controllers/dubbing/parts/DubbingTranslationFix_Execution.cpp"
#include "controllers/dubbing/parts/DubbingTranslationFix_Candidate.cpp"

} // namespace LAStudio
