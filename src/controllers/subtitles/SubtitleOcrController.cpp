#include "controllers/subtitles/SubtitleOcrController.h"

#include "controllers/dubbing/DubbingController.h"
#include "controllers/tts/SubtitleVoiceController.h"
#include "core/services/MediaRuntimeLocator.h"
#include "core/storage/PathUtils.h"
#include "remote/colab/ColabSession.h"
#include "subtitles/ocr/ColabSubtitleOcrRunner.h"
#include "subtitles/locators/PaddleOcrRuntimeLocator.h"
#include "subtitles/locators/SubtitleOcrRuntimeLocator.h"
#include "subtitles/ocr/SubtitleOcrRuntimeService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace LAStudio {
namespace {

constexpr int kSubtitleOcrProjectVersion = 2;
const QString kColabSubtitleOcrCapability = QStringLiteral("subtitle-ocr");
const QString kColabSubtitleOcrModel = QStringLiteral("pp-ocrv5-multilingual-3.1");
const QString kColabSubtitleOcrNotebook = QStringLiteral("LA_STUDIO_SUBTITLE_OCR_PP_OCRV5_GPU.ipynb");
constexpr qsizetype kMaxDiagnosticCharacters = 16000;
constexpr int kFrameExtractionTimeoutMs = 30000;
constexpr int kForwardProgressTimeoutMs = 60000;
constexpr int kChunkSampleCount = 48;
constexpr int kSubtitleOcrCacheVersion = 2;
QSet<QString> s_activeOcrCacheKeys;

QString ffmpegTime(qint64 timestampMs)
{
    return QString::number(qMax<qint64>(0, timestampMs) / 1000.0, 'f', 3);
}

QString processFailure(const QString &stage, const QByteArray &standardError)
{
    const QString detail = QString::fromUtf8(standardError).trimmed();
    return detail.isEmpty() ? QStringLiteral("Subtitle OCR %1 failed.").arg(stage)
                            : QStringLiteral("Subtitle OCR %1 failed: %2").arg(stage, detail);
}

QString boundedDiagnosticText(const QString &value)
{
    const QString normalized = value.trimmed();
    return normalized.size() <= 4000
        ? normalized
        : normalized.left(4000) + QStringLiteral(" [truncated]");
}

QString normalizedRoiText(const SubtitleOcrRoi &roi)
{
    return QStringLiteral("x=%1 y=%2 w=%3 h=%4")
        .arg(roi.x, 0, 'f', 6).arg(roi.y, 0, 'f', 6)
        .arg(roi.width, 0, 'f', 6).arg(roi.height, 0, 'f', 6);
}

QString cropText(const SubtitleOcrRect &crop)
{
    return QStringLiteral("x=%1 y=%2 w=%3 h=%4")
        .arg(crop.x).arg(crop.y).arg(crop.width).arg(crop.height);
}

int normalizedRotation(int rotation)
{
    rotation %= 360;
    if (rotation < 0) rotation += 360;
    return rotation;
}

bool parseAspectRatio(const QString &text, double *value)
{
    const QStringList parts = text.split(QLatin1Char(':'));
    if (parts.size() != 2) return false;
    bool numeratorOk = false;
    bool denominatorOk = false;
    const double numerator = parts.at(0).toDouble(&numeratorOk);
    const double denominator = parts.at(1).toDouble(&denominatorOk);
    if (!numeratorOk || !denominatorOk || numerator <= 0.0 || denominator <= 0.0) return false;
    if (value) *value = numerator / denominator;
    return true;
}

int frameExtractionTimeoutMs()
{
#ifdef LASTUDIO_UNIT_TESTS
    bool parsed = false;
    const int requested = qEnvironmentVariableIntValue(
        "LASTUDIO_TEST_SUBTITLE_OCR_FRAME_TIMEOUT_MS", &parsed);
    if (parsed && requested > 0) return requested;
#endif
    return kFrameExtractionTimeoutMs;
}

QString sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray bytes = file.read(1024 * 1024);
        if (bytes.isEmpty() && file.error() != QFile::NoError) return {};
        hash.addData(bytes);
    }
    return QString::fromLatin1(hash.result().toHex());
}

int defaultOcrWorkerCount()
{
    return qBound(1, QThread::idealThreadCount(), 4);
}

QString normalizedLocalEngineId(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("tesseract-baseline")) return normalized;
    if (normalized == QString::fromLatin1(PaddleOcrRuntimeLocator::engineId())) return normalized;
    return {};
}

bool parsePaddleHealth(const QByteArray &output, QString *error)
{
    const QJsonDocument document = QJsonDocument::fromJson(output);
    const QJsonObject root = document.object();
    if (!document.isObject() || !root.value(QStringLiteral("ok")).toBool()
        || root.value(QStringLiteral("engineId")).toString()
               != QString::fromLatin1(PaddleOcrRuntimeLocator::engineId())
        || root.value(QStringLiteral("engineVersion")).toString()
               != QString::fromLatin1(PaddleOcrRuntimeLocator::engineVersion())
        || !root.value(QStringLiteral("manifestVerified")).toBool()) {
        if (error) {
            *error = root.value(QStringLiteral("error")).toString().trimmed();
            if (error->isEmpty()) *error = QStringLiteral("PaddleOCR health response is invalid.");
        }
        return false;
    }
    return true;
}

} // namespace

SubtitleOcrController::SubtitleOcrController(SubtitleVoiceController *subtitleVoice,
                                             DubbingController *dubbing, QObject *parent)
    : QObject(parent), m_subtitleVoice(subtitleVoice), m_dubbing(dubbing)
{
    m_maxConcurrentWorkers = defaultOcrWorkerCount();
    const QString configuredEngine = normalizedLocalEngineId(
        qEnvironmentVariable("LASTUDIO_SUBTITLE_OCR_ENGINE"));
    if (!configuredEngine.isEmpty()) m_localEngineId = configuredEngine;
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SubtitleOcrController::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &SubtitleOcrController::onProcessError);
    m_frameExtractionTimeout.setSingleShot(true);
    connect(&m_frameExtractionTimeout, &QTimer::timeout,
            this, &SubtitleOcrController::onFrameExtractionTimeout);
    m_forwardProgressTimer.setInterval(1000);
    connect(&m_forwardProgressTimer, &QTimer::timeout,
            this, &SubtitleOcrController::checkForwardProgress);
    connect(&m_sourceFingerprintWatcher, &QFutureWatcher<QString>::finished,
            this, &SubtitleOcrController::onSourceFingerprintReady);
    m_colabRunner = new ColabSubtitleOcrRunner;
    m_colabRunner->moveToThread(&m_colabThread);
    connect(&m_colabThread, &QThread::finished, m_colabRunner, &QObject::deleteLater);
    connect(m_colabRunner, &ColabSubtitleOcrRunner::finished,
            this, &SubtitleOcrController::onColabRecognitionFinished);
    connect(m_colabRunner, &ColabSubtitleOcrRunner::failed,
            this, &SubtitleOcrController::onColabRecognitionFailed);
    m_colabThread.start();
}

SubtitleOcrController::~SubtitleOcrController()
{
    m_frameExtractionTimeout.stop();
    m_forwardProgressTimer.stop();
    if (m_sourceFingerprintWatcher.isRunning()) m_sourceFingerprintWatcher.cancel();
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    releaseRecognitionWorkers();
    releaseActiveCacheKey();
    if (m_colabRunner && m_colabThread.isRunning()) {
        QMetaObject::invokeMethod(m_colabRunner, "cancel", Qt::QueuedConnection);
        m_colabThread.quit();
        m_colabThread.wait(3000);
    }
    cleanWorkspace();
}

QUrl SubtitleOcrController::sourceUrl() const
{
    return m_sourcePath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_sourcePath);
}

QUrl SubtitleOcrController::cropPreviewUrl() const
{
    return m_cropPreviewPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_cropPreviewPath);
}

bool SubtitleOcrController::runtimeAvailable() const
{
    if (usesPaddleLocalEngine()) return PaddleOcrRuntimeLocator::resolve().isUsable();
    if (m_runtimeService) return m_runtimeService->runtimeAvailable();
    return !SubtitleOcrRuntimeLocator::resolveTesseract().isEmpty();
}

bool SubtitleOcrController::localRouteReady() const
{
    if (!runtimeAvailable()) return false;
    if (usesPaddleLocalEngine())
        return PaddleOcrRuntimeLocator::supportsBundledLanguage(m_ocrLanguage);
    // Tesseract's managed language state is intentionally verified through
    // the same `--list-langs` process boundary that recognition will use.
    // This also covers an explicit environment runtime, whose language data
    // is outside of the package service's inventory.
    return true;
}

QString SubtitleOcrController::localRuntimeState() const
{
    if (usesPaddleLocalEngine()) {
        QString error;
        if (!PaddleOcrRuntimeLocator::resolve().isUsable(&error)) return QStringLiteral("Missing");
        return PaddleOcrRuntimeLocator::supportsBundledLanguage(m_ocrLanguage)
            ? QStringLiteral("Ready") : QStringLiteral("Unsupported language");
    }
    if (m_runtimeService) return m_runtimeService->stateName();
    return runtimeAvailable() ? QStringLiteral("Ready") : QStringLiteral("Missing");
}

QString SubtitleOcrController::runtimePath() const
{
    if (usesPaddleLocalEngine()) return PaddleOcrRuntimeLocator::resolve().pythonPath;
    if (m_runtimeService) return m_runtimeService->runtimePath();
    return SubtitleOcrRuntimeLocator::resolveTesseract();
}

int SubtitleOcrController::activeChildProcessCount() const
{
    int active = m_process.state() == QProcess::NotRunning ? 0 : 1;
    for (const RecognitionWorker &worker : m_recognitionWorkers) {
        if (worker.process && worker.process->state() != QProcess::NotRunning) ++active;
    }
    return active;
}

QString SubtitleOcrController::localEngineVersion() const
{
    return usesPaddleLocalEngine() ? QString::fromLatin1(PaddleOcrRuntimeLocator::engineVersion())
                                   : QStringLiteral("5.5.1");
}

bool SubtitleOcrController::usesPaddleLocalEngine() const
{
    return m_localEngineId == QString::fromLatin1(PaddleOcrRuntimeLocator::engineId());
}

bool SubtitleOcrController::usesTesseractLocalEngine() const
{
    return m_localEngineId == QStringLiteral("tesseract-baseline");
}

void SubtitleOcrController::refreshRuntime()
{
    if (usesTesseractLocalEngine() && m_runtimeService) m_runtimeService->refresh();
    emit runtimeChanged();
}

void SubtitleOcrController::setRuntimeService(SubtitleOcrRuntimeService *runtimeService)
{
    if (m_runtimeService == runtimeService) return;
    if (m_runtimeService) disconnect(m_runtimeService, nullptr, this, nullptr);
    m_runtimeService = runtimeService;
    if (m_runtimeService) {
        connect(m_runtimeService, &SubtitleOcrRuntimeService::runtimeChanged,
                this, &SubtitleOcrController::runtimeChanged);
        connect(m_runtimeService, &SubtitleOcrRuntimeService::stateChanged,
                this, &SubtitleOcrController::runtimeChanged);
    }
    emit runtimeChanged();
}

bool SubtitleOcrController::colabRouteReady() const
{
    return m_colabSession
        && m_colabSession->hasVerifiedRoute(kColabSubtitleOcrCapability, m_colabModelId);
}

QString SubtitleOcrController::colabRouteStatus() const
{
    if (!m_colabSession) return QStringLiteral("Subtitle OCR Colab session is unavailable in this build.");
    QString diagnostic;
    if (m_colabSession->hasVerifiedRoute(kColabSubtitleOcrCapability, m_colabModelId, &diagnostic)) {
        const QString gpu = m_colabSession->reportedGpu().trimmed();
        return gpu.isEmpty() ? QStringLiteral("Verified CUDA worker for %1.").arg(m_colabModelId)
                             : QStringLiteral("Verified CUDA worker (%1) for %2.").arg(gpu, m_colabModelId);
    }
    if (!diagnostic.isEmpty()) return diagnostic;
    return m_colabSession->verificationMessage().trimmed();
}

QString SubtitleOcrController::colabNotebookFile() const
{
    return kColabSubtitleOcrNotebook;
}

QVariantMap SubtitleOcrController::runStatistics() const
{
    return {{QStringLiteral("scheduledSamples"), m_scheduledSampleCount},
            {QStringLiteral("extractedFrames"), m_extractedFrameCount},
            {QStringLiteral("readableCrops"), m_readableCropCount},
            {QStringLiteral("deduplicatedFrames"), m_deduplicatedFrameCount},
            {QStringLiteral("recognizedFrames"), m_recognizedFrameCount},
            {QStringLiteral("ocrAttempts"), m_ocrAttemptCount},
            {QStringLiteral("ocrSuccesses"), m_ocrSuccessCount},
            {QStringLiteral("nonEmptyRawResults"), m_nonEmptyRawResultCount},
            {QStringLiteral("filterCandidates"), m_filterCandidateCount},
            {QStringLiteral("publishedSegments"), m_publishedSegmentCount},
            {QStringLiteral("exportedSegments"), m_exportedSegmentCount},
            {QStringLiteral("ffmpegProcessCount"), m_ffmpegProcessCount},
            {QStringLiteral("tesseractProcessCount"), m_tesseractProcessCount},
            {QStringLiteral("paddleProcessCount"), m_paddleProcessCount},
            {QStringLiteral("ocrWorkerCpuSeconds"), m_paddleCpuSeconds},
            {QStringLiteral("ocrWorkerPeakWorkingSetBytes"), m_paddlePeakWorkingSetBytes},
            {QStringLiteral("ocrEngineId"), m_executionRoute == QStringLiteral("local-cpu")
                ? m_localEngineId : m_colabModelId},
            {QStringLiteral("ocrEngineVersion"), m_executionRoute == QStringLiteral("local-cpu")
                ? localEngineVersion() : QStringLiteral("colab-contract-v1")},
            {QStringLiteral("completedSamples"), m_completedSampleCount},
            {QStringLiteral("elapsedMs"), m_runElapsed.isValid() ? m_runElapsed.elapsed() : 0},
            {QStringLiteral("cacheReused"), m_cacheReused},
            {QStringLiteral("resultStatus"), m_resultStatus}};
}

void SubtitleOcrController::setColabSession(ColabSession *session)
{
    if (m_colabSession == session) return;
    if (m_colabSession) disconnect(m_colabSession, nullptr, this, nullptr);
    m_colabSession = session;
    if (m_colabSession) {
        connect(m_colabSession, &ColabSession::sessionChanged,
                this, &SubtitleOcrController::colabRouteChanged);
        connect(m_colabSession, &ColabSession::verificationChanged,
                this, &SubtitleOcrController::colabRouteChanged);
    }
    emit colabRouteChanged();
}

void SubtitleOcrController::setError(const QString &message)
{
    if (m_error == message) return;
    m_error = message;
    emit errorChanged();
}

bool SubtitleOcrController::canRetryFrameExtraction() const
{
    return !m_processing && m_phase == QStringLiteral("error")
        && (m_lastFailedOperation == Operation::ExtractFrame
            || m_lastFailedOperation == Operation::ExtractChunk)
        && !m_sourcePath.isEmpty() && m_frameWidth > 0 && m_frameHeight > 0
        && m_sampleIndex >= 0 && m_sampleIndex < m_samples.size();
}

void SubtitleOcrController::clearDiagnostics()
{
    if (m_diagnostics.isEmpty()) return;
    m_diagnostics.clear();
    emit diagnosticsChanged();
}

void SubtitleOcrController::appendDiagnostic(const QString &event, const QString &detail)
{
    const QString entry = QStringLiteral("[%1] %2\n%3")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), event,
             boundedDiagnosticText(detail));
    if (!m_diagnostics.isEmpty()) m_diagnostics += QStringLiteral("\n\n");
    m_diagnostics += entry;
    if (m_diagnostics.size() > kMaxDiagnosticCharacters)
        m_diagnostics = m_diagnostics.right(kMaxDiagnosticCharacters);
    emit diagnosticsChanged();
}

void SubtitleOcrController::setPhase(const QString &phase)
{
    if (m_phase == phase) return;
    m_phase = phase;
    emit phaseChanged();
}

void SubtitleOcrController::setResultStatus(const QString &status)
{
    if (m_resultStatus == status) return;
    m_resultStatus = status;
    emit resultChanged();
    emit runStatisticsChanged();
}

void SubtitleOcrController::setProcessing(bool processing)
{
    if (m_processing == processing) return;
    m_processing = processing;
    emit processingChanged();
}

void SubtitleOcrController::setProgress(int value, bool available)
{
    value = qBound(0, value, 100);
    if (m_progress == value && m_progressAvailable == available) return;
    m_progress = value;
    m_progressAvailable = available;
    emit progressChanged();
}

void SubtitleOcrController::resetRunStatistics()
{
    m_scheduledSampleCount = 0;
    m_readableCropCount = 0;
    m_ocrAttemptCount = 0;
    m_ocrSuccessCount = 0;
    m_nonEmptyRawResultCount = 0;
    m_filterCandidateCount = 0;
    m_publishedSegmentCount = 0;
    m_exportedSegmentCount = 0;
    m_extractedFrameCount = 0;
    m_deduplicatedFrameCount = 0;
    m_recognizedFrameCount = 0;
    m_ffmpegProcessCount = 0;
    m_tesseractProcessCount = 0;
    m_paddleProcessCount = 0;
    m_paddleCpuSeconds = 0.0;
    m_paddlePeakWorkingSetBytes = 0;
    m_completedSampleCount = 0;
    m_lastForwardProgressMs = 0;
    m_cacheReused = false;
    emit runStatisticsChanged();
}

void SubtitleOcrController::appendObservation(const SubtitleOcrObservation &observation)
{
    m_observations.append(observation);
    if (!observation.text.trimmed().isEmpty()) ++m_nonEmptyRawResultCount;
    if (!observation.text.trimmed().isEmpty()
        && observation.confidence >= m_minimumConfidence) {
        ++m_filterCandidateCount;
    }
    emit runStatisticsChanged();
}

bool SubtitleOcrController::ensureWorkspace()
{
    if (!m_workspacePath.isEmpty()) return true;
    const QString root = QDir(PathUtils::cacheDir()).filePath(QStringLiteral("subtitle-ocr"));
    if (!QDir().mkpath(root)) {
        fail(QStringLiteral("Cannot create app-owned Subtitle OCR staging storage."));
        return false;
    }
    m_workspacePath = QDir(root).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(m_workspacePath)) {
        m_workspacePath.clear();
        fail(QStringLiteral("Cannot create Subtitle OCR staging workspace."));
        return false;
    }
    return true;
}

void SubtitleOcrController::cleanWorkspace(bool retainDiagnostics)
{
    if (!m_workspacePath.isEmpty()) {
        const QString workspace = m_workspacePath;
        const bool existed = QFileInfo(workspace).isDir();
        const bool removed = QDir(workspace).removeRecursively();
        if (retainDiagnostics || !m_diagnostics.isEmpty()) {
            appendDiagnostic(QStringLiteral("workspace-cleanup"),
                             QStringLiteral("workspace=%1 existed=%2 removed=%3 completedUtc=%4")
                                 .arg(workspace)
                                 .arg(existed ? QStringLiteral("true") : QStringLiteral("false"))
                                 .arg(removed ? QStringLiteral("true") : QStringLiteral("false"))
                                 .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)));
        }
    }
    m_workspacePath.clear();
    if (!m_cropPreviewPath.isEmpty()) {
        m_cropPreviewPath.clear();
        emit cropPreviewChanged();
    }
}

void SubtitleOcrController::startProcess(Operation operation, const QString &program,
                                         const QStringList &arguments)
{
    if (program.isEmpty()) {
        fail(QStringLiteral("Required Subtitle OCR runtime is unavailable."), operation);
        return;
    }
    m_operation = operation;
    m_process.setProgram(program);
    QStringList processArguments = arguments;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (m_runtimeService && (operation == Operation::VerifyLanguage ||
                             operation == Operation::RecognizeFrame)) {
        // Keep the UI/runtime preflight and every actual OCR invocation on the
        // same explicitly resolved tessdata directory.  This avoids a system
        // TESSDATA_PREFIX making a language look installed when this worker
        // cannot use the verified app-managed file.
        processArguments = m_runtimeService->tesseractDataArguments() + processArguments;
        environment = m_runtimeService->tesseractProcessEnvironment();
    }
    m_process.setArguments(processArguments);
    m_process.setProcessEnvironment(environment);
    if (operation == Operation::ExtractFrame || operation == Operation::ExtractChunk)
        m_frameExtractionTimedOut = false;
    m_process.start();
    if (operation == Operation::ExtractFrame || operation == Operation::ExtractChunk) {
        m_frameExtractionTimeout.start(frameExtractionTimeoutMs());
    }
}

void SubtitleOcrController::recordFrameExtractionStart(const MediaRuntimePaths &media,
                                                        qint64 timestampMs,
                                                        const SubtitleOcrRect &crop)
{
    appendDiagnostic(QStringLiteral("frame-extraction-start"),
                     QStringLiteral("source=%1; ffmpeg=%2; timestampMs=%3; timestamp=%4; frame=%5x%6; "
                                    "rotation=%7; SAR=%8; normalizedRoi=%9; pixelCrop=%10; output=%11")
                         .arg(m_sourcePath, media.ffmpeg).arg(timestampMs).arg(ffmpegTime(timestampMs))
                         .arg(m_frameWidth).arg(m_frameHeight).arg(m_rotationDegrees)
                         .arg(m_sampleAspectRatio.isEmpty() ? QStringLiteral("unknown") : m_sampleAspectRatio)
                         .arg(normalizedRoiText(m_roi), cropText(crop), m_currentFramePath));
}

bool SubtitleOcrController::validateCurrentFrame(QByteArray *hash, QString *errorMessage)
{
    return validateFrame(m_currentFramePath, hash, errorMessage);
}

bool SubtitleOcrController::validateFrame(const QString &path, QByteArray *hash, QString *errorMessage)
{
    const QFileInfo info(path);
    const bool exists = info.isFile();
    const qint64 bytes = exists ? info.size() : 0;
    QByteArray signature;
    QString decodeDetail;
    QImage image;
    if (!exists) {
        decodeDetail = QStringLiteral("crop file is missing");
    } else if (bytes <= 0) {
        decodeDetail = QStringLiteral("crop file is empty");
    } else {
        QFile frame(path);
        if (!frame.open(QIODevice::ReadOnly)) {
            decodeDetail = QStringLiteral("crop file cannot be opened: %1").arg(frame.errorString());
        } else {
            signature = frame.read(8);
            if (signature != QByteArrayLiteral("\x89PNG\r\n\x1a\n")) {
                decodeDetail = QStringLiteral("crop does not have a PNG signature");
            } else {
                QImageReader reader(path);
                reader.setAutoTransform(false);
                image = reader.read();
                if (image.isNull()) {
                    decodeDetail = QStringLiteral("PNG decode failed: %1").arg(reader.errorString());
                } else {
                    frame.seek(0);
                    if (hash) *hash = QCryptographicHash::hash(frame.readAll(), QCryptographicHash::Sha256);
                }
            }
        }
    }
    appendDiagnostic(QStringLiteral("frame-extraction-output"),
                     QStringLiteral("output=%1; exists=%2; bytes=%3; signature=%4; decoded=%5x%6; result=%7")
                         .arg(path)
                         .arg(exists ? QStringLiteral("true") : QStringLiteral("false"))
                         .arg(bytes)
                         .arg(QString::fromLatin1(signature.toHex()))
                         .arg(image.width()).arg(image.height())
                         .arg(decodeDetail.isEmpty() ? QStringLiteral("readable") : decodeDetail));
    if (!decodeDetail.isEmpty()) {
        if (errorMessage) *errorMessage = decodeDetail;
        return false;
    }
    return true;
}


// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "controllers/subtitles/parts/SubtitleOcrController_Media.cpp"

bool SubtitleOcrController::setRoi(double x, double y, double width, double height)
{
    const SubtitleOcrRoi candidate{x, y, width, height};
    if (!candidate.isValid()) {
        setError(QStringLiteral("Subtitle OCR region must remain inside the source frame and cannot be empty."));
        return false;
    }
    m_roi = candidate;
    setError({});
    emit roiChanged();
    return true;
}

void SubtitleOcrController::resetRoi()
{
    // Reset means remove the crop and return to the complete source frame.
    // The subtitle-oriented crop is a separate, explicit preset so the two
    // visible actions never silently do the same thing.
    m_roi = SubtitleOcrRoi{0.0, 0.0, 1.0, 1.0};
    emit roiChanged();
}

void SubtitleOcrController::setLowerRegionPreset()
{
    m_roi = SubtitleOcrRoi{};
    emit roiChanged();
}

bool SubtitleOcrController::setOcrLanguage(const QString &language)
{
    const QString normalized = language.trimmed();
    if (normalized.isEmpty() || normalized.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_+]")))) {
        setError(QStringLiteral("Choose a valid Subtitle OCR language code."));
        return false;
    }
    if (m_ocrLanguage == normalized) return true;
    m_ocrLanguage = normalized;
    emit settingsChanged();
    emit runtimeChanged();
    return true;
}

bool SubtitleOcrController::setLocalEngine(const QString &engineId)
{
    if (m_processing) {
        setError(QStringLiteral("Wait for Subtitle OCR to finish before changing the local OCR engine."));
        return false;
    }
    const QString normalized = normalizedLocalEngineId(engineId);
    if (normalized.isEmpty()) {
        setError(QStringLiteral("Choose PaddleOCR PP-OCRv6 tiny or the explicit Tesseract baseline."));
        return false;
    }
    if (m_localEngineId == normalized) return true;
    m_localEngineId = normalized;
    setError({});
    emit settingsChanged();
    emit runtimeChanged();
    return true;
}

bool SubtitleOcrController::setExecutionRoute(const QString &route)
{
    const QString normalized = route.trimmed().toLower();
    if (normalized != QStringLiteral("local-cpu") && normalized != QStringLiteral("colab-gpu")) {
        setError(QStringLiteral("Subtitle OCR route must be Local CPU or Colab GPU."));
        return false;
    }
    if (m_executionRoute == normalized) return true;
    m_executionRoute = normalized;
    setError({});
    emit settingsChanged();
    emit colabRouteChanged();
    return true;
}

bool SubtitleOcrController::setColabModelId(const QString &modelId)
{
    const QString normalized = modelId.trimmed().toLower();
    if (normalized != kColabSubtitleOcrModel) {
        setError(QStringLiteral("Choose the supported exact Colab Subtitle OCR model."));
        return false;
    }
    if (m_colabModelId == normalized) return true;
    m_colabModelId = normalized;
    setError({});
    emit settingsChanged();
    emit colabRouteChanged();
    return true;
}

bool SubtitleOcrController::setSampleIntervalMs(qint64 intervalMs)
{
    if (intervalMs < 100 || intervalMs > 30000) {
        setError(QStringLiteral("Subtitle OCR sample interval must be between 100 ms and 30 seconds."));
        return false;
    }
    m_sampleIntervalMs = intervalMs;
    emit settingsChanged();
    return true;
}

bool SubtitleOcrController::setMinimumConfidence(double confidence)
{
    if (confidence < 0.0 || confidence > 1.0) {
        setError(QStringLiteral("Subtitle OCR confidence must be between 0 and 1."));
        return false;
    }
    m_minimumConfidence = confidence;
    emit settingsChanged();
    return true;
}

bool SubtitleOcrController::setMaxConcurrentWorkers(int workers)
{
    if (m_processing) {
        setError(QStringLiteral("Wait for Subtitle OCR to finish before changing the worker limit."));
        return false;
    }
    if (workers < 1 || workers > 4) {
        setError(QStringLiteral("Subtitle OCR worker limit must be between 1 and 4."));
        return false;
    }
    m_maxConcurrentWorkers = workers;
    setError({});
    emit settingsChanged();
    return true;
}

bool SubtitleOcrController::setBenchmarkSampleLimit(int limit)
{
    if (m_processing || limit < 0) return false;
    m_benchmarkSampleLimit = limit;
    return true;
}

bool SubtitleOcrController::requestCropPreview(qint64 positionMs)
{
    if (m_processing || m_sourcePath.isEmpty() || m_frameWidth <= 0 || m_frameHeight <= 0) return false;
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    if (!media.hasFfmpeg()) {
        setError(QStringLiteral("FFmpeg is required to preview the Subtitle OCR region."));
        return false;
    }
    const QStringList crop = SubtitleOcrPipeline::ffmpegCropArguments(m_roi, m_frameWidth, m_frameHeight);
    if (crop.isEmpty()) {
        setError(QStringLiteral("Choose a valid Subtitle OCR region before previewing it."));
        return false;
    }
    if (!ensureWorkspace()) return false;
    m_cropPreviewPath = QDir(m_workspacePath).filePath(QStringLiteral("crop-preview.png"));
    m_cancelRequested = false;
    setError({});
    setProcessing(true);
    setPhase(QStringLiteral("previewing-crop"));
    setProgress(0, false);
    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                          QStringLiteral("-ss"), ffmpegTime(qMin(positionMs,
                                                                   SubtitleOcrPipeline::lastDecodableTimestamp(m_durationMs))),
                          QStringLiteral("-i"), m_sourcePath};
    arguments += crop;
    arguments += {QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-y"), m_cropPreviewPath};
    startProcess(Operation::CropPreview, media.ffmpeg, arguments);
    return true;
}

bool SubtitleOcrController::run()
{
    if (m_processing) return false;
    if (m_sourcePath.isEmpty() || m_frameWidth <= 0 || m_frameHeight <= 0 || m_durationMs <= 0) {
        setError(QStringLiteral("Choose and inspect a video before running Subtitle OCR."));
        return false;
    }
    const bool useColab = m_executionRoute == QStringLiteral("colab-gpu");
    const bool usePaddle = !useColab && usesPaddleLocalEngine();
    const QString tesseract = runtimePath();
    if (!useColab && !runtimeAvailable()) {
        setError(usePaddle
                     ? QStringLiteral("The package-provisioned PaddleOCR PP-OCRv6 tiny runtime is unavailable or incomplete. Repair the package; LA Studio will not fall back silently to Tesseract.")
                     : QStringLiteral("Subtitle OCR Tesseract baseline runtime is unavailable. Install runtime or repair the package before running."));
        emit runtimeChanged();
        return false;
    }
    if (!useColab && !localRouteReady()) {
        setError(usePaddle
                     ? QStringLiteral("The bundled PaddleOCR PP-OCRv6 tiny runtime is verified only for Simplified Chinese (chi_sim). Select chi_sim, the explicit Tesseract baseline with its matching language pack, or Direct Colab GPU.")
                     : QStringLiteral("The selected Tesseract language data is not installed. Install the matching language pack and retry."));
        emit runtimeChanged();
        return false;
    }
    QString routeError;
    if (useColab && (!m_colabSession
                     || !m_colabSession->hasVerifiedRoute(kColabSubtitleOcrCapability,
                                                          m_colabModelId, &routeError))) {
        const QString guidance = QStringLiteral(
            "Connect and check the exact Colab Subtitle OCR worker before running.");
        setError(routeError.isEmpty() ? guidance
                                      : QStringLiteral("%1 %2").arg(guidance, routeError));
        return false;
    }
    if (useColab && m_ocrLanguage.contains(QLatin1Char('+'))) {
        setError(QStringLiteral("Colab Subtitle OCR runs one explicit language profile at a time. Choose one language before running."));
        return false;
    }
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    if (!media.hasFfmpeg()) {
        setError(QStringLiteral("FFmpeg is required to extract Subtitle OCR frames."));
        return false;
    }
    if (SubtitleOcrPipeline::ffmpegCropArguments(m_roi, m_frameWidth, m_frameHeight).isEmpty()) {
        setError(QStringLiteral("Subtitle OCR region resolves to zero source pixels. Enlarge the region before running."));
        return false;
    }
    cleanWorkspace();
    releaseRecognitionWorkers();
    releaseActiveCacheKey();
    if (!ensureWorkspace()) return false;
    m_samples = SubtitleOcrPipeline::sampleTimes(m_durationMs, m_sampleIntervalMs);
    if (m_benchmarkSampleLimit > 0 && m_samples.size() > m_benchmarkSampleLimit)
        m_samples.resize(m_benchmarkSampleLimit);
    if (m_samples.isEmpty()) {
        fail(QStringLiteral("No Subtitle OCR sample timestamps could be created."));
        return false;
    }
    m_observations.clear();
    resetRunStatistics();
    if (!m_segments.isEmpty()) {
        m_segments.clear();
        emit segmentsChanged();
    }
    m_sampleIndex = 0;
    m_previousFrameHash.clear();
    m_previousText.clear();
    m_previousConfidence = 0.0;
    m_recognitionQueue.clear();
    m_uniqueFrames.clear();
    m_chunkStartIndex = 0;
    m_chunkEndIndex = 0;
    m_sourceFingerprint.clear();
    m_cacheKey.clear();
    m_lastFailedOperation = Operation::None;
    m_scheduledSampleCount = m_samples.size();
    setResultStatus(QStringLiteral("running"));
    emit runStatisticsChanged();
    emit frameRetryChanged();
    m_cancelRequested = false;
    m_runElapsed.start();
    m_lastForwardProgressMs = 0;
    setError({});
    setProcessing(true);
    // Local engine selection is explicit.  PaddleOCR gets a package health
    // check; Tesseract remains an explicit compatibility baseline.  Colab is
    // a separate verified route and intentionally never falls back locally.
    setPhase(useColab ? QStringLiteral("checking-colab-route")
                      : (usePaddle ? QStringLiteral("checking-paddleocr-runtime")
                                   : QStringLiteral("checking-language")));
    setProgress(0, false);
    if (useColab) beginOcrSamples();
    else if (usePaddle) {
        const PaddleOcrRuntimeResolution paddle = PaddleOcrRuntimeLocator::resolve();
        startProcess(Operation::VerifyPaddleRuntime, paddle.pythonPath,
                     {paddle.workerPath, QStringLiteral("--cache-root"), paddle.modelCachePath,
                      QStringLiteral("--manifest"), paddle.manifestPath,
                      QStringLiteral("--health")});
    } else {
        startProcess(Operation::VerifyLanguage, tesseract, {QStringLiteral("--list-langs")});
    }
    return true;
}

bool SubtitleOcrController::retry()
{
    return run();
}

bool SubtitleOcrController::retryFrameExtraction()
{
    if (!canRetryFrameExtraction()) {
        setError(QStringLiteral("There is no failed Subtitle OCR frame extraction available to retry."));
        return false;
    }
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    const QStringList crop = SubtitleOcrPipeline::ffmpegCropArguments(
        m_roi, m_frameWidth, m_frameHeight);
    if (!media.hasFfmpeg() || crop.isEmpty()) {
        setError(QStringLiteral("Subtitle OCR frame extraction is no longer configured."));
        return false;
    }
    cleanWorkspace();
    if (!ensureWorkspace()) return false;
    m_lastFailedOperation = Operation::None;
    emit frameRetryChanged();
    m_cancelRequested = false;
    setError({});
    setProcessing(true);
    setPhase(QStringLiteral("retrying-frame-extraction"));
    setProgress(qRound(100.0 * m_sampleIndex / m_samples.size()), true);
    appendDiagnostic(QStringLiteral("frame-extraction-retry"),
                     QStringLiteral("sample=%1/%2; source=%3; ffmpeg=%4")
                         .arg(m_sampleIndex + 1).arg(m_samples.size())
                         .arg(m_sourcePath, media.ffmpeg));
    if (m_executionRoute == QStringLiteral("local-cpu")) beginNextChunk();
    else beginNextSample();
    return true;
}

void SubtitleOcrController::beginOcrSamples()
{
    setPhase(m_executionRoute == QStringLiteral("local-cpu")
                 ? QStringLiteral("fingerprinting-source")
                 : QStringLiteral("extracting-frame"));
    setProgress(0, true);
    if (m_executionRoute == QStringLiteral("local-cpu")) beginCacheLookup();
    else beginNextSample();
}


#include "controllers/subtitles/parts/SubtitleOcrController_Engine.cpp"
#include "controllers/subtitles/parts/SubtitleOcrController_Project.cpp"

} // namespace LAStudio
