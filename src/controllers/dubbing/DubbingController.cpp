#include "controllers/dubbing/DubbingController.h"

#include "SttSessionController.h"
#include "tts/engine/TtsEngine.h"
#include "tts/pipeline/TtsSavedVoiceProfile.h"
#include "controllers/dubbing/DubbingJobRunner.h"
#include "controllers/dubbing/DubbingColabModelRoutes.h"
#include "controllers/dubbing/DubbingTranslationFixService.h"
#include "controllers/subtitles/SubtitleOcrController.h"
#include "controllers/shared/VoiceClonePresetService.h"
#include "dubbing/exporters/CapCutDraftExporter.h"
#include "dubbing/exporters/DubbingSubtitleService.h"
#include "dubbing/fusion/DubbingTranscriptFusionService.h"
#include "dubbing/timing/DubbingTimingService.h"
#include "dubbing/timing/EspeakNgPhonemizer.h"
#include "dubbing/media/MediaToolService.h"
#include "dubbing/media/RemoteMediaImportService.h"
#include "dubbing/workflow/DubbingWorkflowDefinition.h"
#include "dubbing/workflow/DubbingWorkflowNodes.h"
#include "workflows/graph/WorkflowGraphRunner.h"
#include "core/storage/PathUtils.h"
#include "core/utils/Logger.h"
#include "controllers/app/AppController.h"
#include "controllers/models/ModelSessionRegistry.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include "controllers/models/CapabilitySettingsSchema.h"
#include "controllers/models/DownloadInstallService.h"
#include "core/models/CapabilityFamilyModel.h"
#include "core/models/DownloadManager.h"
#include "core/models/ModelManager.h"
#include "core/models/RuntimeManager.h"
#include "remote/gateway/ExecutionProvider.h"
#include "remote/colab/ColabSession.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QRegularExpression>

namespace LAStudio {

namespace {

QString automaticDefaultFamilyId(const QString &capabilityId,
                                 const QString &dubbingQuality = QString())
{
    if (capabilityId == QStringLiteral("stt"))
        return QStringLiteral("whisper.cpp");
    if (capabilityId == QStringLiteral("voice-isolation"))
        return QStringLiteral("sherpa-onnx-spleeter-2stems-fp16");
    if (capabilityId == QStringLiteral("translation"))
        return QStringLiteral("hy-mt2-1.8b");
    if (capabilityId == QStringLiteral("llm-chat"))
        return QStringLiteral("qwen3.5-2b");
    if (capabilityId == QStringLiteral("tts"))
        return dubbingQuality == QStringLiteral("fast")
            ? QStringLiteral("vieneu-tts-v2-turbo")
            : QStringLiteral("omnivoice");
    return {};
}

QString normalizedVoiceCloneTargetModel(const QString &requestedModel)
{
    const QString model = requestedModel.trimmed().toLower();
    if (model == QStringLiteral("vieneu"))
        return QStringLiteral("vieneu-tts-v3-turbo");
    if (model.startsWith(QStringLiteral("vieneu-tts-v")))
        return model;
    if (model == QStringLiteral("omnivoice"))
        return model;
    // Preserve legacy exact clone targets (Qwen/VoxCPM2) when a project has
    // one. New or empty selections use OmniVoice as the universal default.
    if (model.startsWith(QStringLiteral("qwen3-tts"))
        || model == QStringLiteral("voxcpm2"))
        return model;
    return model.isEmpty() ? QStringLiteral("omnivoice") : model;
}

QString configuredVoiceCloneTargetModel(const QVariantMap &parameters)
{
    const QString explicitTarget = parameters.value(
        QStringLiteral("voiceCloneModelId")).toString();
    if (!explicitTarget.trimmed().isEmpty())
        return normalizedVoiceCloneTargetModel(explicitTarget);
    // `modelId` is the ordinary TTS model and is intentionally not used as a
    // clone target.  A project can select Kokoro (or another non-cloning TTS
    // route) and then switch back to a saved reference voice; deriving the
    // clone worker from that ordinary model would make every universal preset
    // look incompatible.  New selections therefore use the universal
    // OmniVoice clone target until the picker writes an explicit target.
    return QStringLiteral("omnivoice");
}

QString canonicalVoiceCloneTarget(const QString &modelId)
{
    if (modelId.startsWith(QStringLiteral("vieneu-tts"), Qt::CaseInsensitive)
        || modelId.compare(QStringLiteral("vieneu"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("vieneu");
    if (modelId.compare(QStringLiteral("omnivoice"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("omnivoice");
    return modelId.trimmed().toLower();
}

QString dubbingProjectStem(const QString &projectPath)
{
    const QString fileName = QFileInfo(projectPath).fileName();
    const QString projectSuffix = QStringLiteral(".ladub.json");
    QString stem = fileName.endsWith(projectSuffix, Qt::CaseInsensitive)
        ? fileName.left(fileName.size() - projectSuffix.size())
        : QFileInfo(fileName).completeBaseName();
    stem.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                 QStringLiteral("_"));
    return stem.isEmpty() ? QStringLiteral("project") : stem;
}

QString dubbingProjectArtifactRoot(const QString &projectPath)
{
    const QFileInfo projectInfo(projectPath);
    const QString projectId = dubbingProjectStem(projectInfo.absoluteFilePath());
    return QDir(projectInfo.absolutePath()).filePath(
        QStringLiteral(".workflow-artifacts/%1").arg(projectId));
}

QString dubbingArtifactStageDirectory(const QString &projectPath, const QString &artifactId)
{
    QString stageDirectory = artifactId.trimmed().toLower();
    if (stageDirectory == QStringLiteral("transcribe") || stageDirectory == QStringLiteral("stt"))
        stageDirectory = QStringLiteral("01-stt");
    else if (stageDirectory == QStringLiteral("subtitle-ocr") || stageDirectory == QStringLiteral("ocr"))
        stageDirectory = QStringLiteral("02-ocr");
    else if (stageDirectory == QStringLiteral("review-transcript") || stageDirectory == QStringLiteral("review"))
        stageDirectory = QStringLiteral("03-review");
    else if (stageDirectory == QStringLiteral("translate"))
        stageDirectory = QStringLiteral("04-translation");
    return QDir(dubbingProjectArtifactRoot(projectPath)).filePath(stageDirectory);
}

QString unifiedColabStageUrl(const QUrl &baseUrl, const QString &capability,
                             const QString &model)
{
    // Keep the exact route boundary even when a single Colab coordinator owns
    // the public tunnel. Each ColabSession therefore continues to verify its
    // own health/capability document and every runner keeps its existing
    // capability-specific endpoint contract.
    QUrl endpoint = baseUrl;
    QString basePath = endpoint.path();
    while (basePath.endsWith(QLatin1Char('/')) && basePath.size() > 1)
        basePath.chop(1);
    if (basePath == QStringLiteral("/")) basePath.clear();
    const QString encodedCapability = QString::fromLatin1(
        QUrl::toPercentEncoding(capability.trimmed().toLower()));
    const QString encodedModel = QString::fromLatin1(
        QUrl::toPercentEncoding(model.trimmed().toLower()));
    endpoint.setPath(basePath + QStringLiteral("/v1/unified/")
                     + encodedCapability + QLatin1Char('/') + encodedModel);
    return endpoint.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery
                             | QUrl::RemoveFragment);
}

void unloadConflictingDubbingRuntime(ModelSessionRegistry *registry,
                                     const QString &capabilityId)
{
    if (!registry) return;

    QStringList conflictingCapabilities;
    if (capabilityId == QStringLiteral("tts") ||
        capabilityId == QStringLiteral("stt")) {
        conflictingCapabilities.append(QStringLiteral("translation"));
        conflictingCapabilities.append(QStringLiteral("llm-chat"));
    } else if (capabilityId == QStringLiteral("translation")) {
        conflictingCapabilities.append(QStringLiteral("stt"));
        conflictingCapabilities.append(QStringLiteral("tts"));
        conflictingCapabilities.append(QStringLiteral("llm-chat"));
    } else {
        return;
    }

    for (const QString &conflictingCapability : conflictingCapabilities) {
        IModelSession *conflictingSession =
            registry->sessionForCapability(conflictingCapability);
        if (!conflictingSession) continue;

        const QList<SessionConfiguration> loaded =
            conflictingSession->loadedConfigurations();
        if (loaded.isEmpty()) continue;

        Logger::info(
            QStringLiteral("DubbingController"),
            QStringLiteral("Dubbing runtime handoff: unloading %1 before loading %2 "
                           "to avoid incompatible shared DLLs in one process.")
                .arg(conflictingCapability, capabilityId));
        for (const SessionConfiguration &configuration : loaded) {
            conflictingSession->requestUnloadConfiguration(configuration.signature);
        }
    }
}

QString subtitleTimestamp(qint64 milliseconds, bool webVtt)
{
    const qint64 hours = milliseconds / 3600000;
    milliseconds %= 3600000;
    const qint64 minutes = milliseconds / 60000;
    milliseconds %= 60000;
    const qint64 seconds = milliseconds / 1000;
    const qint64 millis = milliseconds % 1000;
    return QStringLiteral("%1:%2:%3%4%5")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(webVtt ? QLatin1Char('.') : QLatin1Char(','))
        .arg(millis, 3, 10, QLatin1Char('0'));
}

bool writeDubbingSubtitles(const QVariantList &segments, const QString &path,
                           bool useTargetText, QString *error)
{
    return DubbingSubtitleService::writeSidecar(segments, path, useTargetText, error);
}

bool replaceCopy(const QString &source, const QString &destination, QString *error)
{
    if (source.isEmpty() || !QFileInfo(source).isFile()) return true;
    if (QFileInfo(source).absoluteFilePath().compare(
            QFileInfo(destination).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        if (error) *error = QStringLiteral("Cannot replace package file: %1").arg(destination);
        return false;
    }
    if (!QFile::copy(source, destination)) {
        if (error) *error = QStringLiteral("Cannot copy %1 to %2.").arg(source, destination);
        return false;
    }
    return true;
}

QVariantMap workflowArtifactSpecForNode(const QString &nodeId)
{
    const QString id = nodeId.trimmed().toLower();
    if (id == QStringLiteral("ingest")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Normalized working audio")},
                {QStringLiteral("description"), QStringLiteral("Upload the normalized WAV or FLAC produced by the media-preparation worker. It replaces only the working audio; source media remains unchanged.")},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/normalize/")},
                {QStringLiteral("workerPath"), QStringLiteral("Download normalized.wav or normalized.flac from this folder, then upload it here.")},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("normalized.wav")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".wav"), QStringLiteral(".flac")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("source-separate")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Voice isolation")},
                {QStringLiteral("description"), QStringLiteral("Upload the two lossless stems saved by the Spleeter/UVR Colab notebook. Both files must use the selected transfer format." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la-studio-separation-jobs/<model-id>/<job-id>/")},
                {QStringLiteral("workerPath"), QStringLiteral("In Colab Files, open la-studio-separation-jobs/<model-id>/<job-id>/ and download the matching vocals and background stem files. The job-id directory is created by this run; source.wav is input and must not be uploaded." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("vocals.flac"), QStringLiteral("background.flac")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".flac")}},
                {QStringLiteral("artifactTransferFormat"), QStringLiteral("flac")},
                {QStringLiteral("multiple"), true}};
    }
    if (id == QStringLiteral("transcribe")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("STT transcript")},
                {QStringLiteral("description"), QStringLiteral("Upload one timed or line-mapped source transcript. This is imported as the reviewed source text; it does not start a local or remote model." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/stt/<model-id>/")},
                {QStringLiteral("workerPath"), QStringLiteral("Save one timed transcript file with any project/run-scoped basename from the STT Colab notebook before downloading." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("one timed transcript file (any accepted filename)")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".srt"), QStringLiteral(".vtt"), QStringLiteral(".ass"), QStringLiteral(".ssa"), QStringLiteral(".txt"), QStringLiteral(".md"), QStringLiteral(".markdown")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("subtitle-ocr")) {
        QVariantMap spec = workflowArtifactSpecForNode(QStringLiteral("transcribe"));
        spec.insert(QStringLiteral("nodeId"), id);
        spec.insert(QStringLiteral("title"), QStringLiteral("Subtitle OCR transcript"));
        spec.insert(QStringLiteral("description"), QStringLiteral("Upload the reviewed OCR subtitle file produced by the exact Subtitle OCR Colab notebook."));
        spec.insert(QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/ocr/<model-id>/"));
        spec.insert(QStringLiteral("workerPath"), QStringLiteral("Save one timed OCR subtitle file with any project/run-scoped basename before downloading."));
        spec.insert(QStringLiteral("expectedFiles"), QStringList{QStringLiteral("one timed OCR subtitle file (any accepted filename)")});
        return spec;
    }
    if (id == QStringLiteral("review-transcript")) {
        QVariantMap spec = workflowArtifactSpecForNode(QStringLiteral("transcribe"));
        spec.insert(QStringLiteral("nodeId"), id);
        spec.insert(QStringLiteral("title"), QStringLiteral("Reviewed STT + OCR transcript"));
        spec.insert(QStringLiteral("description"), QStringLiteral("Upload the single reviewed source transcript after reconciling STT and OCR. This preserves the required review gate; it is not an unreviewed fallback."));
        spec.insert(QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/stt-ocr-review/"));
        spec.insert(QStringLiteral("workerPath"), QStringLiteral("Save one reconciled transcript file with any project/run-scoped basename before downloading."));
        spec.insert(QStringLiteral("expectedFiles"), QStringList{QStringLiteral("one reconciled transcript file (any accepted filename)")});
        return spec;
    }
    if (id == QStringLiteral("translate")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Translated subtitles")},
                {QStringLiteral("description"), QStringLiteral("Upload one target-language subtitle file. Cue count must match the reviewed transcript; LA Studio will not invent timing or silently drop rows." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/translate/<model-id>/")},
                {QStringLiteral("workerPath"), QStringLiteral("Save one translated subtitle file with any project/run-scoped basename from the translation Colab notebook before downloading." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("one translated subtitle file (any accepted filename)")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".srt"), QStringLiteral(".vtt"), QStringLiteral(".ass"), QStringLiteral(".ssa"), QStringLiteral(".txt"), QStringLiteral(".md"), QStringLiteral(".markdown")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("synthesize")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Generated voice")},
                {QStringLiteral("description"), QStringLiteral("Upload the rendered WAV or FLAC voice output exported by the TTS/voice-clone Colab notebook." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/tts/<model-id>/")},
                {QStringLiteral("workerPath"), QStringLiteral("Save voice.wav or voice.flac before downloading. This full timed voice bed is mixed with Background in the next Export/Output step; segment bundles are deliberately not accepted." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("voice.wav")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".wav"), QStringLiteral(".flac")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("mix")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Mixed voice audio")},
                {QStringLiteral("description"), QStringLiteral("Upload the final WAV or FLAC mix exported by the Colab mix step." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/mix/")},
                {QStringLiteral("workerPath"), QStringLiteral("Save mix.wav or mix.flac before downloading. It is the complete audio mix used directly by export." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("mix.wav")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".wav"), QStringLiteral(".flac")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("export")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Final media")},
                {QStringLiteral("description"), QStringLiteral("Upload the final video exported by the Colab render step." )},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/export/")},
                {QStringLiteral("workerPath"), QStringLiteral("Save final.mp4 (or final.mkv/final.webm/final.mov) before downloading." )},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("final.mp4 (or same name with an accepted video extension)")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".mp4"), QStringLiteral(".mkv"), QStringLiteral(".webm"), QStringLiteral(".mov")}},
                {QStringLiteral("multiple"), false}};
    }
    if (id == QStringLiteral("fit-timing")) {
        return {{QStringLiteral("nodeId"), id},
                {QStringLiteral("title"), QStringLiteral("Timing-aligned voice")},
                {QStringLiteral("description"), QStringLiteral("Upload the fully timing-aligned voice WAV or FLAC from the alignment worker. Export/Output will mix this one full voice bed with Background.")},
                {QStringLiteral("colabFolder"), QStringLiteral("/content/la_studio_outputs/alignment/")},
                {QStringLiteral("workerPath"), QStringLiteral("Download timed-voice.wav or timed-voice.flac from this folder, then upload it here.")},
                {QStringLiteral("expectedFiles"), QStringList{QStringLiteral("timed-voice.wav")}},
                {QStringLiteral("allowedExtensions"), QStringList{QStringLiteral(".wav"), QStringLiteral(".flac")}},
                {QStringLiteral("multiple"), false}};
    }
    return {};
}

QString normalizedTranscriptSource(QString source)
{
    source = source.trimmed().toLower();
    // Projects written before 0.0.7.4 coupled both remote workers behind the
    // `stt+ocr` setting. Keep those projects usable without retaining the
    // unsafe behaviour: the saved setting now means the local reconcile pass.
    if (source == QStringLiteral("stt+ocr")) return QStringLiteral("reconcile");
    if (source == QStringLiteral("ocr") || source == QStringLiteral("reconcile")) return source;
    return QStringLiteral("stt");
}

QString activityNodeId(const QString &stage)
{
    const QString normalized = stage.trimmed().toLower();
    if (normalized == QStringLiteral("import")) return QStringLiteral("media-input");
    if (normalized == QStringLiteral("source-separation")) return QStringLiteral("source-separate");
    if (normalized == QStringLiteral("transcription")) return QStringLiteral("transcribe");
    if (normalized == QStringLiteral("translation") || normalized == QStringLiteral("translation-fix"))
        return QStringLiteral("translate");
    if (normalized == QStringLiteral("tts")) return QStringLiteral("synthesize");
    if (normalized == QStringLiteral("timing")) return QStringLiteral("fit-timing");
    return normalized;
}

QString artifactProductionNodeId(const QString &nodeId)
{
    const QString id = nodeId.trimmed().toLower();
    if (id == QStringLiteral("normalize")) return QStringLiteral("ingest");
    if (id == QStringLiteral("isolator") || id == QStringLiteral("separate")
        || id == QStringLiteral("source-separation"))
        return QStringLiteral("source-separate");
    if (id == QStringLiteral("stt") || id == QStringLiteral("transcribe-stt"))
        return QStringLiteral("transcribe");
    if (id == QStringLiteral("ocr")) return QStringLiteral("subtitle-ocr");
    if (id == QStringLiteral("alignment-subtitle") || id == QStringLiteral("alignment"))
        return QStringLiteral("fit-timing");
    if (id == QStringLiteral("tts") || id == QStringLiteral("synthesize-voice"))
        return QStringLiteral("synthesize");
    if (id == QStringLiteral("review-translation")) return QStringLiteral("translate");
    if (id == QStringLiteral("export-output")) return QStringLiteral("export");
    return id;
}

bool artifactMatchesActiveStage(const QString &artifactNodeId, const QString &runnerStage)
{
    const QString requested = artifactProductionNodeId(artifactNodeId);
    const QString active = activityNodeId(runnerStage);
    if (requested == active) return true;
    // OCR and review are accepted variants of the one visible Transcribe/STT
    // stage.  They can replace only a currently-running Transcribe worker.
    return active == QStringLiteral("transcribe")
        && (requested == QStringLiteral("subtitle-ocr")
            || requested == QStringLiteral("review-transcript"));
}

} // namespace

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : DubbingController(sttSession, tts, nullptr, models, runtimes, parent)
{
}

DubbingController::~DubbingController() = default;

QUrl DubbingController::sourceThumbnailUrl() const
{
    // This getter is read from QML bindings and Image::source.  Never stat the
    // filesystem here: a binding reevaluation can happen many times per frame
    // and synchronous filesystem metadata calls can spin the UI thread.  The async
    // thumbnail request and finished handler validate the file before updating
    // m_sourceThumbnailPath; Image handles a later external deletion normally.
    return m_sourceThumbnailPath.isEmpty()
        ? QUrl()
        : QUrl::fromLocalFile(m_sourceThumbnailPath);
}

bool DubbingController::requestSourceThumbnail()
{
    const QFileInfo sourceInfo(m_project.sourceMediaPath);
    const QString suffix = sourceInfo.suffix().toLower();
    const bool isVideo = suffix == QStringLiteral("mp4")
        || suffix == QStringLiteral("mkv") || suffix == QStringLiteral("mov")
        || suffix == QStringLiteral("webm") || suffix == QStringLiteral("avi");
    if (!sourceInfo.isFile() || !isVideo || m_project.projectPath.isEmpty() || !m_thumbnailTools) {
        if (m_thumbnailTools && m_thumbnailTools->busy())
            m_thumbnailTools->cancel();
        m_pendingThumbnailOutputPath.clear();
        if (!m_sourceThumbnailPath.isEmpty()) {
            m_sourceThumbnailPath.clear();
            emit sourceThumbnailChanged();
        }
        return false;
    }

    const QByteArray cacheKey = sourceInfo.absoluteFilePath().toUtf8()
        + QByteArrayLiteral("|") + QByteArray::number(sourceInfo.size())
        + QByteArrayLiteral("|") + QByteArray::number(sourceInfo.lastModified().toMSecsSinceEpoch());
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex());
    QDir thumbnailDirectory(QFileInfo(m_project.projectPath).absolutePath()
                            + QStringLiteral("/.la-studio/thumbnails"));
    if (!thumbnailDirectory.mkpath(QStringLiteral("."))) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Cannot create source thumbnail cache directory: %1")
                            .arg(thumbnailDirectory.absolutePath()));
        return false;
    }
    const QString outputPath = thumbnailDirectory.filePath(key + QStringLiteral(".jpg"));
    if (m_sourceThumbnailPath == outputPath && QFileInfo(outputPath).isFile()
        && QFileInfo(outputPath).size() > 0)
        return true;

    if (m_sourceThumbnailPath != outputPath) {
        m_sourceThumbnailPath.clear();
        emit sourceThumbnailChanged();
    }
    if (m_thumbnailTools->busy()) {
        // Keep an earlier extraction alive when it is for this same source;
        // otherwise cancel it and retry after QProcess has emitted finished.
        if (m_pendingThumbnailOutputPath == outputPath)
            return true;
        m_pendingThumbnailOutputPath = outputPath;
        m_thumbnailTools->cancel();
        QTimer::singleShot(100, this, [this]() { requestSourceThumbnail(); });
        return true;
    }
    if (QFileInfo(outputPath).isFile() && QFileInfo(outputPath).size() > 0) {
        m_sourceThumbnailPath = outputPath;
        m_pendingThumbnailOutputPath.clear();
        emit sourceThumbnailChanged();
        return true;
    }
    if (!m_thumbnailTools->available()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Source thumbnail skipped because FFmpeg is unavailable."));
        return false;
    }

    m_pendingThumbnailOutputPath = outputPath;
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Extracting source thumbnail: %1").arg(sourceInfo.absoluteFilePath()));
    m_thumbnailTools->extractVideoThumbnail(sourceInfo.absoluteFilePath(), outputPath);
    return true;
}

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     TranslationEngine *translation,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts),
      m_models(models), m_runtimes(runtimes)
{
    m_translation = translation;
    m_runner = new DubbingJobRunner(sttSession, tts, translation, models, runtimes, this);
    // Public-media download is a CPU-only, app-owned operation. It is not an
    // AI route and must never require, create, or reuse a Colab credential.
    m_remoteMediaImport = new RemoteMediaImportService({}, this);
    m_thumbnailTools = new MediaToolService(this);
    connect(m_thumbnailTools, &MediaToolService::finished, this,
            [this](bool success, const QString &outputPath, const QString &error) {
        // A source can change while FFmpeg is extracting. Only publish the
        // result belonging to the currently requested cache file.
        if (outputPath.isEmpty() || outputPath != m_pendingThumbnailOutputPath)
            return;
        m_pendingThumbnailOutputPath.clear();
        if (success && QFileInfo(outputPath).isFile() && QFileInfo(outputPath).size() > 0) {
            m_sourceThumbnailPath = outputPath;
            Logger::info(QStringLiteral("DubbingController"),
                         QStringLiteral("Source thumbnail ready: %1").arg(outputPath));
        } else {
            m_sourceThumbnailPath.clear();
            Logger::warning(QStringLiteral("DubbingController"),
                            QStringLiteral("Source thumbnail unavailable: %1")
                                .arg(error.isEmpty() ? QStringLiteral("FFmpeg produced no image") : error));
        }
        emit sourceThumbnailChanged();
    });
    connect(m_remoteMediaImport, &RemoteMediaImportService::transferProgress, this,
            [this](qint64 receivedBytes, qint64 totalBytes) {
        const int index = mediaQueueIndex(m_activeMediaQueueDownloadId);
        if (index < 0) return;
        QVariantMap item = m_mediaQueueItems.at(index).toMap();
        item.insert(QStringLiteral("receivedBytes"), receivedBytes);
        item.insert(QStringLiteral("totalBytes"), totalBytes);
        item.insert(QStringLiteral("downloadState"), QStringLiteral("downloading"));
        item.insert(QStringLiteral("status"), totalBytes > 0
                        ? QStringLiteral("Receiving completed media %1 / %2 bytes").arg(receivedBytes).arg(totalBytes)
                        : QStringLiteral("Downloading locally (%1 bytes)").arg(receivedBytes));
        replaceMediaQueueItem(index, item);
    });
    connect(m_remoteMediaImport, &RemoteMediaImportService::finished, this,
            &DubbingController::onBatchMediaDownloadFinished);
    m_translationFix = new DubbingTranslationFixService(this);
    connect(m_translationFix, &DubbingTranslationFixService::stateChanged,
            this, [this]() {
        emit translationFixChanged();
        emit processingChanged();
        emit errorChanged();
        emit workflowChanged();
    });
    connect(m_translationFix, &DubbingTranslationFixService::completed,
            this, [this](const QVariantList &segments, int, int) {
        m_project.segments = segments;
        emit segmentsChanged();
        emit translationFixChanged();
        emit workflowChanged();
        persistAfterEdit();
    });
    connect(m_translationFix, &DubbingTranslationFixService::reconciliationCompleted,
            this, [this](const QVariantList &segments, int, int) {
        // Suggestions preserve each segment's conflict evidence and remain
        // pending until an explicit accept/reject/manual review action.
        m_project.segments = segments;
        emit segmentsChanged();
        emit translationFixChanged();
        emit workflowChanged();
        persistAfterEdit();
    });
    connect(m_translationFix, &DubbingTranslationFixService::connectionTested,
            this, &DubbingController::translationFixConnectionTested);
    if (AppController::instance() && AppController::instance()->sessionRegistry()) {
        for (IModelSession *session : AppController::instance()->sessionRegistry()->sessions()) {
            if (!session) continue;
            connect(session, &IModelSession::stateChanged, this, [this]() {
                emit workflowChanged();
                scheduleAutomaticSetupAdvance();
            });
            connect(session, &IModelSession::activeConfigurationChanged, this, [this]() {
                emit workflowChanged();
                scheduleAutomaticSetupAdvance();
            });
        }
    }
    m_workflowRegistry = new NodeRegistry(this);
    registerDubbingWorkflowNodes(*m_workflowRegistry, m_runner);
    loadHistory();
    m_workflowRunner = new WorkflowGraphRunner(m_workflowRegistry, this);
    connect(m_workflowRunner, &WorkflowGraphRunner::stateChanged, this, [this]() {
        emit processingChanged();
        emit errorChanged();
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::nodeStarted, this,
            [this](const QString &nodeId) {
        const QString visibleStep = visibleStepForNode(nodeId);
        setCurrentStep(visibleStep);
        // Persist the active checkpoint before a long-running node starts so
        // an unexpected app close can reopen at the node that was actually
        // being processed, rather than at the previous completed task.
        persistAfterEdit();
        if (m_workflowMode == QStringLiteral("automatic")) {
            appendAutomaticEvent(QStringLiteral("Running %1").arg(visibleStep),
                                 QStringLiteral("running"), nodeId);
            setAutomaticStatus(QStringLiteral("Running node: %1").arg(visibleStep));
        }
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::nodeCompleted, this,
            [this](const QString &nodeId, const QVariantMap &) {
        if (m_workflowMode == QStringLiteral("automatic")) {
            appendAutomaticEvent(QStringLiteral("Completed %1").arg(visibleStepForNode(nodeId)),
                                 QStringLiteral("completed"), nodeId);
            if (nodeId == QStringLiteral("transcribe")
                && normalizedTranscriptSource(
                       m_project.transcriptConfiguration.value(QStringLiteral("transcriptSource"),
                                                               QStringLiteral("stt")).toString())
                       == QStringLiteral("stt")) {
                if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
                    if (IModelSession *stt = app->sessionRegistry()->sessionForCapability(
                            QStringLiteral("stt"))) {
                        const QList<SessionConfiguration> loaded = stt->loadedConfigurations();
                        for (const SessionConfiguration &configuration : loaded)
                            stt->requestUnloadConfiguration(configuration.signature);
                    }
                }
                appendAutomaticEvent(QStringLiteral("Releasing Whisper runtime before translation"),
                                     QStringLiteral("running"), QStringLiteral("transcribe"));
            } else if (nodeId == QStringLiteral("translate")) {
                if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
                    if (IModelSession *translation = app->sessionRegistry()->sessionForCapability(
                            QStringLiteral("translation"))) {
                        const QList<SessionConfiguration> loaded = translation->loadedConfigurations();
                        for (const SessionConfiguration &configuration : loaded)
                            translation->requestUnloadConfiguration(configuration.signature);
                    }
                }
                prepareAutomaticVoiceRuntime();
            }
        }
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::reviewRequested, this, [this](const QVariantMap &request) {
        m_workflowReviewRequest = request;
        m_activeReviewId = request.value(QStringLiteral("reviewId")).toString();
        if (!m_project.projectPath.isEmpty() && !m_activeReviewId.isEmpty()) {
            if (!m_workflowReviewStore) {
                m_workflowReviewStore = std::make_unique<WorkflowReviewStore>(
                    QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
            }
            WorkflowReviewRequest stored;
            stored.reviewId = m_activeReviewId;
            stored.runId = workflowRunId();
            stored.nodeRunId = workflowNodeRunId();
            stored.nodeId = m_workflowRunner->activeNodeId();
            stored.mode = request.value(QStringLiteral("mode")).toString();
            stored.editor = request.value(QStringLiteral("editor")).toString();
            stored.artifact = request.value(QStringLiteral("artifact"));
            stored.createdAt = QDateTime::currentDateTimeUtc();
            QString ignoredError;
            m_workflowReviewStore->save(stored, &ignoredError);
        }
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::completed, this, [this](const QVariantMap &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setCurrentStep(QStringLiteral("completed"));
        if (m_workflowMode == QStringLiteral("automatic")) {
            setAutomaticStatus(QStringLiteral("Final dubbed media is ready"));
            appendAutomaticEvent(QStringLiteral("Final dubbed media is ready"),
                                 QStringLiteral("completed"), QStringLiteral("export"));
        }
        discoverInterruptedWorkflow();
        persistAfterEdit();
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::failed, this, [this](const QString &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setWorkflowMode(QStringLiteral("idle"));
        discoverInterruptedWorkflow();
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::cancelled, this, [this]() {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setWorkflowMode(QStringLiteral("idle"));
        discoverInterruptedWorkflow();
        emit workflowChanged();
    });

    if (AppController::instance()) {
        if (auto *downloads = AppController::instance()->downloads()) {
            connect(downloads, &DownloadManager::activeDownloadsChanged,
                    this, &DubbingController::scheduleAutomaticSetupAdvance);
            connect(downloads, &DownloadManager::error, this,
                    [this](const QString &, const QString &, const QString &message) {
                if (m_automaticSetupActive) finishAutomaticSetupFailure(message);
            });
        }
        if (auto *install = AppController::instance()->downloadInstall()) {
            connect(install, &DownloadInstallService::installStatesChanged,
                    this, &DubbingController::scheduleAutomaticSetupAdvance);
        }
        if (auto *registry = AppController::instance()->sessionRegistry()) {
            const auto watchAutomaticLoad = [this, registry](const QString &capabilityId,
                                                              const QString &nodeId) {
                IModelSession *session = registry->sessionForCapability(capabilityId);
                if (!session) return;
                connect(session, &IModelSession::errorOccurred, this,
                        [this, capabilityId, nodeId](const QString &message) {
                    if (!m_automaticSetupActive) return;
                    const QVariantMap configuration =
                        m_workflowNodeConfigurations.value(nodeId).toMap();
                    const QString familyId = configuration.value(
                        QStringLiteral("familyId")).toString();
                    if (!familyId.isEmpty() && m_automaticConfiguredNodes.contains(nodeId)) {
                        appendAutomaticEvent(
                            QStringLiteral("Required default model %1 failed to load")
                                .arg(familyId),
                            QStringLiteral("failed"), nodeId);
                        finishAutomaticSetupFailure(
                            QStringLiteral("Failed to load required default model %1: %2")
                                .arg(familyId, message));
                        return;
                    }
                    finishAutomaticSetupFailure(
                        QStringLiteral("Failed to load %1 model: %2")
                            .arg(capabilityId, message));
                });
            };
            watchAutomaticLoad(QStringLiteral("stt"), QStringLiteral("transcribe"));
            watchAutomaticLoad(QStringLiteral("tts"), QStringLiteral("synthesize"));
        }
    }
    if (m_models)
        connect(m_models, &ModelManager::registryUpdated,
                this, &DubbingController::scheduleAutomaticSetupAdvance);
    if (m_runtimes)
        connect(m_runtimes, &RuntimeManager::registryUpdated,
                this, &DubbingController::scheduleAutomaticSetupAdvance);

    connect(m_runner, &DubbingJobRunner::stateChanged, this, [this]() {
        updateMediaQueueProgressFromRunner();
        emit processingChanged();
        emit errorChanged();
        emit previewChanged();
        emit exportChanged();
        emit workflowChanged();
    });

    connect(m_runner, &DubbingJobRunner::errorOccurred, this, [this](const QString &) {
        m_pendingExportPath.clear();
    });

    connect(m_runner, &DubbingJobRunner::segmentsUpdated, this, [this](const QVariantList &segments) {
        m_project.segments = segments;
        emit segmentsChanged();
        emit workflowChanged();
        persistAfterEdit();
    });

    connect(m_runner, &DubbingJobRunner::segmentUpdated, this, [this](int index, const QVariantMap &patch) {
        if (index >= 0 && index < m_project.segments.size()) {
            m_project.segments[index] = patch;
            emit segmentsChanged();
            emit workflowChanged();
            persistAfterEdit();
        }
    });

    connect(m_runner, &DubbingJobRunner::ingestFinished, this, &DubbingController::onIngestFinished);
    connect(m_runner, &DubbingJobRunner::sourceSeparationFinished, this, [this](const QVariantMap &outputs) {
        const QString vocalsPath = outputs.value(QStringLiteral("vocals")).toString().trimmed();
        const QString backgroundPath = outputs.value(QStringLiteral("background")).toString().trimmed();
        if (vocalsPath.isEmpty() || backgroundPath.isEmpty()
                || !QFileInfo(vocalsPath).isFile() || QFileInfo(vocalsPath).size() <= 0
                || !QFileInfo(backgroundPath).isFile() || QFileInfo(backgroundPath).size() <= 0) {
            setError(QStringLiteral(
                "Source separation completed without readable Vocals and Background stems. "
                "The normalized analysis audio was not used as a substitute."));
            return;
        }
        m_project.vocalsAudioPath = vocalsPath;
        m_project.backgroundAudioPath = backgroundPath;
        m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
        emit projectChanged();
        emit workflowChanged();
        persistAfterEdit();
    });
    connect(m_runner, &DubbingJobRunner::stageCompleted, this,
            [this](const QString &nodeId, const QVariantMap &outputs) {
        QVariantMap persistedOutputs = outputs;
        if (nodeId == QStringLiteral("transcribe")) {
            const QString source = outputs.value(QStringLiteral("transcriptSource")).toString();
            const QVariantList transcript = outputs.value(QStringLiteral("transcript")).toList();
            // Keep each source independently durable. A later OCR pass must
            // never erase a usable STT result (and vice versa); reconciliation
            // is the only action that promotes the fused result to review.
            if (source == QStringLiteral("stt"))
                m_project.transcriptConfiguration.insert(QStringLiteral("sttSegments"), transcript);
            else if (source == QStringLiteral("ocr"))
                m_project.transcriptConfiguration.insert(QStringLiteral("ocrSegments"), transcript);
            emit projectChanged();
            QString artifactPath;
            const QString artifactId = source == QStringLiteral("ocr")
                ? QStringLiteral("subtitle-ocr") : QStringLiteral("transcribe");
            if (persistWorkflowTranscriptArtifact(artifactId, transcript, false, &artifactPath)) {
                persistedOutputs.insert(QStringLiteral("path"), artifactPath);
                if (artifactId == QStringLiteral("subtitle-ocr")) {
                    QVariantMap ocrOutputs = persistedOutputs;
                    ocrOutputs.insert(QStringLiteral("transcriptSource"), QStringLiteral("ocr"));
                    m_stepOutputs.insert(QStringLiteral("subtitle-ocr"), ocrOutputs);
                }
            }
            persistAfterEdit();
        } else if (nodeId == QStringLiteral("translate")) {
            QString artifactPath;
            if (persistWorkflowTranscriptArtifact(
                    QStringLiteral("translate"),
                    outputs.value(QStringLiteral("transcript")).toList(), true, &artifactPath)) {
                persistedOutputs.insert(QStringLiteral("path"), artifactPath);
            }
        }
        m_stepOutputs.insert(nodeId, persistedOutputs);
        m_lastCompletedStepId = nodeId;
        if (nodeId == QStringLiteral("mix") && !m_pendingExportPath.isEmpty()) {
            const QString destination = m_pendingExportPath;
            m_pendingExportPath.clear();
            if (!m_runner->startExport(m_project.sourceMediaPath,
                                       outputs.value(QStringLiteral("audio")).toString(),
                                       destination, m_project.segments, subtitleConfiguration())) {
                persistAfterEdit();
                emit workflowChanged();
                return;
            }
        }
        if (m_workflowMode == QStringLiteral("step")
            && (!m_workflowRunner || !m_workflowRunner->running())) {
            clearError();
            setAutomaticStatus(
                QStringLiteral("Manual node completed: %1").arg(visibleStepForNode(nodeId)));
            // STT and Subtitle OCR are two independent actions inside the
            // same Transcribe screen. Completing STT must not move the user
            // away before they have a chance to run OCR.
            if (nodeId != QStringLiteral("transcribe"))
                advanceManualStep(nodeId);
        }
        persistAfterEdit();
        emit workflowChanged();
    });
    // Batch work reuses the production runner one item at a time.  Delaying
    // the transition by one event-loop turn lets the normal project/segment
    // signal handlers commit the real result before the next stage begins.
    connect(m_runner, &DubbingJobRunner::stageCompleted, this,
            [this](const QString &nodeId, const QVariantMap &outputs) {
        if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
        QTimer::singleShot(0, this, [this, nodeId, outputs]() {
            if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
            const int index = mediaQueueIndex(m_activeMediaQueueItemId);
            if (index < 0) return;
            QVariantMap item = m_mediaQueueItems.at(index).toMap();
            QVariantList completedStages = item.value(QStringLiteral("completedStages")).toList();
            if (!completedStages.contains(nodeId)) completedStages.append(nodeId);
            item.insert(QStringLiteral("completedStages"), completedStages);
            item.insert(QStringLiteral("progress"), 0);
            replaceMediaQueueItem(index, item);

            if (nodeId == QStringLiteral("source-separate")) {
                const QString outputDirectory = mediaQueueOutputDirectory(m_activeMediaQueueItemId);
                QString error;
                const QString vocalsName = QFileInfo(m_project.vocalsAudioPath).fileName();
                const QString backgroundName = QFileInfo(m_project.backgroundAudioPath).fileName();
                const bool wroteVocals = QFileInfo(m_project.vocalsAudioPath).isFile()
                    && replaceCopy(m_project.vocalsAudioPath,
                                   QDir(outputDirectory).filePath(vocalsName), &error);
                if (wroteVocals) recordMediaQueueOutput(
                    QStringLiteral("vocals"), QDir(outputDirectory).filePath(vocalsName));
                const bool wroteBackground = QFileInfo(m_project.backgroundAudioPath).isFile()
                    && replaceCopy(m_project.backgroundAudioPath,
                                   QDir(outputDirectory).filePath(backgroundName), &error);
                if (wroteBackground) recordMediaQueueOutput(
                    QStringLiteral("background"), QDir(outputDirectory).filePath(backgroundName));
                if (!wroteVocals || !wroteBackground) {
                    completeCurrentMediaQueueItem(false, error.isEmpty()
                        ? QStringLiteral("Voice isolation completed without writable vocal and background stem files.")
                        : error);
                    return;
                }
            }
            if (nodeId == QStringLiteral("transcribe")) {
                if (!writeMediaQueueSubtitles(QStringLiteral("sourceSrt"), false)) return;
            }
            if (nodeId == QStringLiteral("translate")) {
                if (!writeMediaQueueSubtitles(QStringLiteral("translatedSrt"), true)) return;
            }
            if (nodeId == QStringLiteral("mix")) {
                const QString outputPath = QDir(mediaQueueOutputDirectory(m_activeMediaQueueItemId))
                    .filePath(QStringLiteral("voice.wav"));
                QString error;
                if (!QFileInfo(m_runner->previewPath()).isFile()
                    || !replaceCopy(m_runner->previewPath(), outputPath, &error)) {
                    completeCurrentMediaQueueItem(false, error.isEmpty()
                        ? QStringLiteral("Voice synthesis completed without a writable WAV output.") : error);
                    return;
                }
                recordMediaQueueOutput(QStringLiteral("voiceWav"), outputPath);
            }
            if (nodeId == QStringLiteral("export")) {
                const QString outputPath = outputs.value(QStringLiteral("media")).toString();
                if (!QFileInfo(outputPath).isFile()) {
                    completeCurrentMediaQueueItem(false,
                        QStringLiteral("Export completed without a writable media output."));
                    return;
                }
                recordMediaQueueOutput(QStringLiteral("exportedMedia"), outputPath);
            }

            if (m_mediaQueueExecutionMode == QStringLiteral("stage-by-stage")) {
                completeCurrentMediaQueueStage(nodeId);
                return;
            }
            const int completedStageIndex = m_mediaQueueStagePlan.indexOf(nodeId);
            if (completedStageIndex < 0) {
                completeCurrentMediaQueueItem(false,
                    QStringLiteral("The completed batch action was not in the selected plan."));
                return;
            }
            const int nextStageIndex = completedStageIndex + 1;
            if (nextStageIndex < m_mediaQueueStagePlan.size()) {
                startMediaQueueStage(m_mediaQueueStagePlan.at(nextStageIndex));
                return;
            }
            completeCurrentMediaQueueItem(true);
        });
    });
    connect(m_runner, &DubbingJobRunner::errorOccurred, this, [this](const QString &message) {
        if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
        QTimer::singleShot(0, this, [this, message]() {
            if (!m_mediaQueueProcessing || m_activeMediaQueueItemId.isEmpty()) return;
            completeCurrentMediaQueueItem(false, message);
        });
    });

    // Restoring the most recently used project is intentionally deferred
    // until every runner, journal and signal connection exists.  It is
    // disabled for disposable test/smoke profiles so a visual test cannot
    // open or modify a user's live work.
    const QString applicationName = QCoreApplication::applicationName();
    const bool shouldRestoreProject = !qEnvironmentVariableIsSet("LASTUDIO_DISABLE_PROJECT_RESTORE")
        && !qEnvironmentVariableIsSet("LASTUDIO_QML_SMOKE")
        && !applicationName.contains(QStringLiteral("UnitTest"), Qt::CaseInsensitive);
    if (shouldRestoreProject && !m_history.isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            if (hasProject() || m_history.isEmpty()) return;
            const QString path = m_history.constFirst().toMap()
                .value(QStringLiteral("projectPath")).toString();
            if (QFileInfo(path).isFile() && !openProject(path)) {
                Logger::warning(QStringLiteral("DubbingController"),
                                QStringLiteral("Could not restore recent project: %1").arg(lastError()));
                clearError();
            }
        });
    }
}

bool DubbingController::processing() const
{
    return m_automaticSetupActive
        || m_mediaQueueProcessing
        || (m_translationFix && m_translationFix->busy())
        || m_runner->processing()
        || subtitleOcrProcessing()
        || (m_workflowRunner && m_workflowRunner->running());
}

QString DubbingController::stage() const
{
    if (m_automaticSetupActive) return QStringLiteral("model-setup");
    if (m_translationFix && m_translationFix->busy())
        return QStringLiteral("translation-fix");
    if (m_workflowRunner && m_workflowRunner->running()
        && !m_workflowRunner->activeNodeId().isEmpty())
        return m_workflowRunner->activeNodeId();
    if (m_runner && m_runner->processing()) return m_runner->stage();
    if (subtitleOcrProcessing()) return QStringLiteral("subtitle-ocr");
    return m_runner ? m_runner->stage() : QString();
}

QVariantMap DubbingController::activityStageInfo() const
{
    QString nodeId;
    if (m_automaticSetupActive) {
        nodeId = m_automaticSetupNodeId;
    } else if (m_translationFix && m_translationFix->busy()) {
        nodeId = QStringLiteral("translate");
    } else if (m_workflowRunner && m_workflowRunner->running()) {
        nodeId = m_workflowRunner->activeNodeId();
    } else if (m_runner && m_runner->processing()) {
        nodeId = activityNodeId(m_runner ? m_runner->stage() : QString());
    } else if (subtitleOcrProcessing()) {
        nodeId = QStringLiteral("subtitle-ocr");
    } else {
        nodeId = activityNodeId(m_runner ? m_runner->stage() : QString());
    }

    QVariantMap result;
    result.insert(QStringLiteral("nodeId"), nodeId);
    const QVariantList stages = workflowStages();
    for (int index = 0; index < stages.size(); ++index) {
        const QVariantMap candidate = stages.at(index).toMap();
        const QString actionNodeId = candidate.value(QStringLiteral("actionNodeId")).toString();
        bool matches = nodeId == actionNodeId;
        for (const QVariant &productionNode : candidate.value(
                 QStringLiteral("productionNodeIds")).toList()) {
            matches = matches || nodeId == productionNode.toString();
        }
        if (!matches) continue;

        result.insert(QStringLiteral("stageId"), candidate.value(QStringLiteral("id")));
        result.insert(QStringLiteral("title"), candidate.value(QStringLiteral("title")));
        result.insert(QStringLiteral("index"), index + 1);
        result.insert(QStringLiteral("count"), stages.size());

        const QVariantMap configuration = m_workflowNodeConfigurations.value(actionNodeId).toMap();
        const QVariantMap parameters = configuration.value(QStringLiteral("parameters")).toMap();
        const QString provider = configuration.value(
            QStringLiteral("executionProvider"), parameters.value(
                QStringLiteral("executionProvider"))).toString().trimmed().toLower();
        const QString model = configuration.value(
            QStringLiteral("modelId"), parameters.value(QStringLiteral("modelId"))).toString().trimmed();
        if (nodeId == QStringLiteral("subtitle-ocr") && m_subtitleOcr) {
            const QString ocrRoute = m_subtitleOcr->executionRoute().trimmed().toLower();
            result.insert(QStringLiteral("route"),
                          ocrRoute == QStringLiteral("colab-gpu")
                              ? QStringLiteral("Direct Colab GPU")
                              : QStringLiteral("Local CPU"));
            result.insert(QStringLiteral("model"),
                          ocrRoute == QStringLiteral("colab-gpu")
                              ? m_subtitleOcr->colabModelId()
                              : m_subtitleOcr->localEngineId());
        } else if (provider == QStringLiteral("colab-direct"))
            result.insert(QStringLiteral("route"), QStringLiteral("Direct Colab GPU"));
        else if (provider == QStringLiteral("api-gateway"))
            result.insert(QStringLiteral("route"), QStringLiteral("API Gateway"));
        else if (!provider.isEmpty() || !model.isEmpty()
                 || actionNodeId == QStringLiteral("ingest")
                 || actionNodeId == QStringLiteral("media-input"))
            result.insert(QStringLiteral("route"), QStringLiteral("Local CPU"));
        if (!result.contains(QStringLiteral("model")))
            result.insert(QStringLiteral("model"), model);
        break;
    }

    if (result.value(QStringLiteral("title")).toString().isEmpty()) {
        result.insert(QStringLiteral("title"),
                      m_automaticSetupActive ? QStringLiteral("Preparing workflow")
                                             : QStringLiteral("Dubbing"));
        result.insert(QStringLiteral("count"), stages.size());
    }
    if (m_automaticSetupActive) {
        result.insert(QStringLiteral("status"), m_automaticStatusText);
    } else if (subtitleOcrProcessing() && m_subtitleOcr) {
        result.insert(QStringLiteral("status"), m_subtitleOcr->phase().trimmed().isEmpty()
                                             ? QStringLiteral("Running Subtitle OCR")
                                             : m_subtitleOcr->phase());
    } else if (m_runner) {
        const QString status = m_runner->activityStatus().trimmed();
        if (!status.isEmpty()) result.insert(QStringLiteral("status"), status);
        const QVariantMap transfer = m_runner->activityTransferProgress();
        if (!transfer.isEmpty()) result.insert(QStringLiteral("artifactTransfer"), transfer);
    }
    return result;
}

int DubbingController::progress() const
{
    if (m_automaticSetupActive) {
        const QVariantList downloads = automaticSetupDownloads();
        if (downloads.isEmpty()) return 0;
        qint64 received = 0;
        qint64 total = 0;
        for (const QVariant &entry : downloads) {
            const QVariantMap download = entry.toMap();
            received += download.value(QStringLiteral("bytesReceived")).toLongLong();
            total += download.value(QStringLiteral("bytesTotal")).toLongLong();
        }
        // Download byte counts are the only measurable part of automatic
        // setup. Loading/installing a model has no truthful percentage.
        return total > 0 ? qBound(0, int(received * 100 / total), 100) : 0;
    }
    if (m_translationFix && m_translationFix->busy())
        return m_translationFix->progress();
    return m_workflowRunner && m_workflowRunner->running() ? m_workflowRunner->progress() : m_runner->progress();
}

bool DubbingController::progressAvailable() const
{
    if (m_automaticSetupActive) {
        const QVariantList downloads = automaticSetupDownloads();
        qint64 total = 0;
        for (const QVariant &entry : downloads)
            total += entry.toMap().value(QStringLiteral("bytesTotal")).toLongLong();
        return total > 0;
    }
    if (m_translationFix && m_translationFix->busy()) return true;
    if (m_workflowRunner && m_workflowRunner->running())
        return m_workflowRunner->progressAvailable();
    // Individual manual nodes report heterogeneous data. Until their runner
    // exposes a measured unit, show a working state rather than a made-up %.
    return false;
}

bool DubbingController::settingsLocked() const
{
    return m_automaticSetupActive
        || (m_workflowMode == QStringLiteral("automatic") && processing());
}

QString DubbingController::lastError() const
{
    if (m_translationFix && !m_translationFix->lastError().isEmpty())
        return m_translationFix->lastError();
    return (m_workflowRunner && !m_workflowRunner->error().isEmpty()) ? m_workflowRunner->error() : m_runner->lastError();
}

bool DubbingController::translationFixing() const
{
    return m_translationFix && m_translationFix->busy();
}

int DubbingController::translationFixProgress() const
{
    return m_translationFix ? m_translationFix->progress() : 0;
}

QString DubbingController::translationFixStatus() const
{
    return m_translationFix ? m_translationFix->statusText() : QString();
}

QVariantMap DubbingController::translationFixConfiguration() const
{
    return m_translationFix ? m_translationFix->configuration() : QVariantMap();
}

int DubbingController::translationFixCandidateCount() const
{
    return DubbingTranslationFixService::eligibleSegmentCount(
        m_project.segments, m_project.targetLanguage);
}

QString DubbingController::adaptiveProvider() const
{
    return translationFixConfiguration().value(QStringLiteral("provider"),
                                               QStringLiteral("lmstudio")).toString();
}

bool DubbingController::adaptiveReady() const
{
    const QVariantMap config = translationFixConfiguration();
    if (!config.value(QStringLiteral("configured")).toBool()) return false;
    const QString provider = adaptiveProvider();
    if (provider == QStringLiteral("colab-direct")) {
        ColabSession *session = colabSessionForStage(QStringLiteral("adaptive-llm"));
        QString routeError;
        return session && session->hasVerifiedRoute(
            QStringLiteral("llm-chat"),
            config.value(QStringLiteral("model")).toString(), &routeError);
    }
    if (provider == QStringLiteral("local")) {
        StudioConfiguration selection;
        selection.capabilityId = QStringLiteral("llm-chat");
        selection.familyId = config.value(QStringLiteral("model")).toString();
        selection.runtimeId = config.value(QStringLiteral("runtimeId")).toString();
        selection.runtimeVersion = config.value(QStringLiteral("runtimeVersion")).toString();
        selection.selectedFiles = config.value(QStringLiteral("selectedFiles")).toMap();
        return StudioConfigurationResolver::resolve(selection).isValid;
    }
    if (provider == QStringLiteral("cli")) {
        const QString cliAgent = config.value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
        return !DubbingTranslationFixService::cliExecutablePath(cliAgent).isEmpty();
    }
    return !config.value(QStringLiteral("serverUrl")).toString().trimmed().isEmpty()
        && !config.value(QStringLiteral("model")).toString().trimmed().isEmpty();
}

QString DubbingController::adaptiveStatusText() const
{
    if (!adaptiveReady()) return QStringLiteral("LLM setup required");
    const QString provider = adaptiveProvider();
    if (provider == QStringLiteral("local")) {
        return translationFixConfiguration()
            .value(QStringLiteral("model"), QStringLiteral("Local LLM ready")).toString();
    }
    if (provider == QStringLiteral("cli")) {
        const QString cliAgent = translationFixConfiguration().value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
        const QString model = translationFixConfiguration().value(QStringLiteral("model")).toString();
        QString label = QStringLiteral("Claude Code");
        if (cliAgent == QStringLiteral("codex")) label = QStringLiteral("Codex CLI");
        else if (cliAgent == QStringLiteral("antigravity")) label = QStringLiteral("Antigravity");
        return (model.isEmpty() || model == QStringLiteral("default"))
            ? QStringLiteral("%1 (CLI)").arg(label)
            : QStringLiteral("%1 · %2").arg(label, model);
    }
    const QString model = translationFixConfiguration().value(QStringLiteral("model")).toString();
    if (provider == QStringLiteral("colab-direct"))
        return QStringLiteral("Direct Colab GPU · %1").arg(model);
    return provider == QStringLiteral("api")
        ? QStringLiteral("LLM API · %1").arg(model)
        : QStringLiteral("LM Studio · %1").arg(model);
}

QVariantMap DubbingController::firstCustomSetupIssue() const
{
    const QString transcriptSource = normalizedTranscriptSource(
        m_project.transcriptConfiguration.value(QStringLiteral("transcriptSource"),
                                                QStringLiteral("stt")).toString());
    const QString persistedOcrRoute = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrExecutionRoute")).toString().trimmed().toLower();
    const bool usesColabOcr = persistedOcrRoute == QStringLiteral("colab-gpu")
        || (persistedOcrRoute.isEmpty() && m_subtitleOcr
            && m_subtitleOcr->executionRoute() == QStringLiteral("colab-gpu"));
    const bool ocrRouteReady = m_subtitleOcr
        && (usesColabOcr ? m_subtitleOcr->colabRouteReady() : m_subtitleOcr->runtimeAvailable());
    // OCR is an independent source. Reconciliation consumes two already saved
    // transcripts locally, so it must never block on an OCR worker route.
    if (transcriptSource == QStringLiteral("ocr")
        && !ocrRouteReady) {
        return {{QStringLiteral("nodeId"), QStringLiteral("transcribe")},
                {QStringLiteral("setupKind"), QStringLiteral("subtitle-ocr-route")},
                {QStringLiteral("message"),
                 usesColabOcr
                    ? QStringLiteral("Connect and check the exact Colab Subtitle OCR worker before using OCR transcript source.")
                    : QStringLiteral("Install the Subtitle OCR runtime before using OCR transcript source.")}};
    }
    QList<QPair<QString, QString>> requiredNodes{
        {QStringLiteral("source-separate"), QStringLiteral("voice-isolation")},
        {QStringLiteral("translate"), QStringLiteral("translation")},
        {QStringLiteral("synthesize"), QStringLiteral("tts")}
    };
    if (transcriptSource == QStringLiteral("stt"))
        requiredNodes.insert(1, {QStringLiteral("transcribe"), QStringLiteral("stt")});
    for (const auto &required : requiredNodes) {
        const QVariantMap selected = m_workflowNodeConfigurations.value(required.first).toMap();
        if (selected.isEmpty()) {
            return {{QStringLiteral("nodeId"), required.first},
                    {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                    {QStringLiteral("message"),
                     QStringLiteral("Choose a model for the %1 node before running Custom dubbing.")
                         .arg(visibleStepForNode(required.first))}};
        }
        if (required.first == QStringLiteral("source-separate")
            || required.first == QStringLiteral("transcribe")
            || required.first == QStringLiteral("translate")
            || required.first == QStringLiteral("synthesize")) {
            const QVariantMap parameters = selected.value(QStringLiteral("parameters")).toMap();
            ExecutionProvider provider = ExecutionProvider::LocalDev;
            const QString providerId = selected.value(
                QStringLiteral("executionProvider"), parameters.value(QStringLiteral("executionProvider"),
                QStringLiteral("local-dev"))).toString();
            if (executionProviderFromId(providerId, &provider)
                && provider != ExecutionProvider::LocalDev) {
                const QString modelId = selected.value(
                    QStringLiteral("modelId"), parameters.value(QStringLiteral("modelId"))).toString().trimmed();
                if (modelId.isEmpty()) {
                    return {{QStringLiteral("nodeId"), required.first},
                            {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                            {QStringLiteral("message"),
                                 QStringLiteral("Choose a %1 model for the %2 node before running Custom dubbing.")
                                 .arg(executionProviderDisplayName(provider), visibleStepForNode(required.first))}};
                }
                if (provider == ExecutionProvider::ColabDirect
                    && !DubbingColabModelRoutes::supports(required.first, modelId)) {
                    return {{QStringLiteral("nodeId"), required.first},
                            {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                            {QStringLiteral("message"),
                             QStringLiteral("The selected model has no exact Colab notebook for the %1 node.")
                                 .arg(visibleStepForNode(required.first))}};
                }
                continue;
            }
        }
        StudioConfiguration configuration;
        configuration.capabilityId = required.second;
        configuration.familyId = selected.value(QStringLiteral("familyId")).toString();
        configuration.runtimeId = selected.value(QStringLiteral("runtimeId")).toString();
        configuration.runtimeVersion = selected.value(QStringLiteral("runtimeVersion")).toString();
        configuration.selectedFiles = selected.value(QStringLiteral("selectedFiles")).toMap();
        if (!StudioConfigurationResolver::resolve(configuration).isValid) {
            return {{QStringLiteral("nodeId"), required.first},
                    {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                    {QStringLiteral("message"),
                     QStringLiteral("The selected model or runtime for %1 is not installed. Choose an available setup.")
                         .arg(visibleStepForNode(required.first))}};
        }
    }
    const bool rewriteEnabled =
        m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
        && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool();
    if (rewriteEnabled && !adaptiveReady()) {
        return {{QStringLiteral("nodeId"), QStringLiteral("translate")},
                {QStringLiteral("setupKind"), QStringLiteral("rewrite-model")},
                {QStringLiteral("message"),
                 QStringLiteral("Choose a local, CLI, or API rewrite model for the Translate node, or turn off automatic rewriting.")}};
    }
    return {};
}

bool DubbingController::customReady() const
{
    return firstCustomSetupIssue().isEmpty();
}

QString DubbingController::customStatusText() const
{
    const QVariantMap issue = firstCustomSetupIssue();
    if (!issue.isEmpty()) return issue.value(QStringLiteral("message")).toString();
    if (!m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool())
        return QStringLiteral("Custom models ready; long translations require manual review");
    return QStringLiteral("Custom models ready; rewrite: %1").arg(adaptiveStatusText());
}

QString DubbingController::previewPath() const
{
    return m_runner->previewPath();
}

QString DubbingController::dubbedVocalPath() const
{
    return m_runner->dubbedVocalPath();
}

QUrl DubbingController::playbackMediaUrl() const
{
    const QString exported = m_runner ? m_runner->exportPath() : QString();
    const QString suffix = QFileInfo(exported).suffix().toLower();
    if (!exported.isEmpty() && QFileInfo::exists(exported)
        && (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
            || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm")
            || suffix == QStringLiteral("avi")))
        return QUrl::fromLocalFile(exported);
    return sourceMediaUrl();
}

QString DubbingController::exportPath() const
{
    return m_runner->exportPath();
}

void DubbingController::setRemoteServices(Settings *settings, ColabSession *translationSession,
                                           ColabSession *ttsSession, ColabSession *voiceCloneSession,
                                           ColabSession *separationSession,
                                           ColabSession *alignmentSession)
{
    m_settings = settings;
    if (m_runner) {
        m_runner->setRemoteServices(settings, translationSession, ttsSession,
                                    voiceCloneSession, separationSession,
                                    alignmentSession,
                                    AppController::instance()
                                        ? AppController::instance()->colabChatSession() : nullptr);
    }
    if (m_translationFix) m_translationFix->setDirectColabSession(
        AppController::instance() ? AppController::instance()->colabChatSession() : nullptr);
    // The sessions remain the sole holders of transient URLs/tokens.  Dubbing
    // observes verification results only to remember which exact model was
    // checked in this process; the snapshot deliberately contains no secret.
    observeColabSession(QStringLiteral("source-separate"), separationSession);
    observeColabSession(QStringLiteral("transcribe"),
                        AppController::instance() ? AppController::instance()->colabSttSession() : nullptr);
    observeColabSession(QStringLiteral("subtitle-ocr"),
                        AppController::instance() ? AppController::instance()->colabSubtitleOcrSession() : nullptr);
    observeColabSession(QStringLiteral("translate"), translationSession);
    observeColabSession(QStringLiteral("synthesize"), ttsSession);
    Q_UNUSED(voiceCloneSession);
    observeColabSession(QStringLiteral("alignment"), alignmentSession);
    observeColabSession(QStringLiteral("adaptive-llm"),
                        AppController::instance() ? AppController::instance()->colabChatSession() : nullptr);
    emit colabSetupChanged();
}

#include "controllers/dubbing/parts/DubbingController_Colab.cpp"

void DubbingController::setSubtitleOcrController(SubtitleOcrController *controller)
{
    for (const QMetaObject::Connection &connection : std::as_const(m_independentSubtitleOcrConnections))
        QObject::disconnect(connection);
    m_independentSubtitleOcrConnections.clear();
    m_independentSubtitleOcrActive = false;
    m_independentSubtitleOcrLoadingSource = false;
    m_independentSubtitleOcrSourcePath.clear();
    m_subtitleOcr = controller;
    applyStoredSubtitleOcrConfiguration();
    if (m_runner) m_runner->setSubtitleOcrController(controller);
    if (!m_subtitleOcr) {
        emit subtitleOcrProcessingChanged();
        return;
    }

    // The production runner still owns its legacy OCR route for restored
    // workflows.  These connections own only an explicitly requested OCR
    // pass, so an audio STT run and OCR can proceed at the same time without
    // either worker stealing the other's completion or failure event.
    m_independentSubtitleOcrConnections = {
        connect(m_subtitleOcr, &SubtitleOcrController::sourceChanged, this, [this]() {
            if (!m_independentSubtitleOcrActive || !m_independentSubtitleOcrLoadingSource
                || !m_subtitleOcr) return;
            if (QFileInfo(m_subtitleOcr->sourcePath()).absoluteFilePath()
                != QFileInfo(m_independentSubtitleOcrSourcePath).absoluteFilePath()) return;
            m_independentSubtitleOcrLoadingSource = false;
            if (!m_subtitleOcr->run()) {
                const QString message = m_subtitleOcr->error().trimmed().isEmpty()
                    ? QStringLiteral("The Subtitle OCR route could not be started.")
                    : m_subtitleOcr->error();
                m_independentSubtitleOcrActive = false;
                setError(QStringLiteral("OCR transcript failed: %1").arg(message));
                emit subtitleOcrProcessingChanged();
                emit processingChanged();
                emit workflowChanged();
            }
        }),
        connect(m_subtitleOcr, &SubtitleOcrController::segmentsChanged, this, [this]() {
            if (!m_independentSubtitleOcrActive || !m_subtitleOcr
                || m_subtitleOcr->processing()
                || m_subtitleOcr->phase() != QStringLiteral("completed")) return;
            const QVariantList segments = DubbingTranscriptFusionService::normalizeOcrSegments(
                m_subtitleOcr->segments());
            m_independentSubtitleOcrActive = false;
            m_independentSubtitleOcrLoadingSource = false;
            if (segments.isEmpty()) {
                setError(QStringLiteral("OCR completed without usable subtitle segments."));
                emit subtitleOcrProcessingChanged();
                emit processingChanged();
                emit workflowChanged();
                return;
            }
            m_project.transcriptConfiguration.insert(QStringLiteral("ocrSegments"), segments);
            m_project.transcriptConfiguration.insert(QStringLiteral("ocrCompletedAt"),
                                                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            const QVariantList sttSegments = m_project.transcriptConfiguration
                .value(QStringLiteral("sttSegments")).toList();
            // OCR-only is a valid transcript path. Promote it to the active
            // editable segment list only when STT has not already produced the
            // primary transcript; a later OCR pass remains non-destructive.
            if (sttSegments.isEmpty()) {
                m_project.segments = segments;
                emit segmentsChanged();
            }
            QVariantMap ocrOutput;
            ocrOutput.insert(QStringLiteral("transcript"), segments);
            ocrOutput.insert(QStringLiteral("transcriptSource"), QStringLiteral("ocr"));
            QString artifactPath;
            if (persistWorkflowTranscriptArtifact(
                    QStringLiteral("subtitle-ocr"), segments, false, &artifactPath)) {
                ocrOutput.insert(QStringLiteral("path"), artifactPath);
            }
            m_stepOutputs.insert(QStringLiteral("subtitle-ocr"), ocrOutput);
            m_lastCompletedStepId = QStringLiteral("subtitle-ocr");
            clearError();
            emit subtitleOcrProcessingChanged();
            emit processingChanged();
            emit projectChanged();
            emit workflowChanged();
            persistAfterEdit();
        }),
        connect(m_subtitleOcr, &SubtitleOcrController::errorChanged, this, [this]() {
            if (!m_independentSubtitleOcrActive || !m_subtitleOcr
                || m_subtitleOcr->error().trimmed().isEmpty()) return;
            m_independentSubtitleOcrActive = false;
            m_independentSubtitleOcrLoadingSource = false;
            setError(QStringLiteral("OCR transcript failed: %1").arg(m_subtitleOcr->error()));
            emit subtitleOcrProcessingChanged();
            emit processingChanged();
            emit workflowChanged();
        }),
        connect(m_subtitleOcr, &SubtitleOcrController::processingChanged, this, [this]() {
            emit subtitleOcrProcessingChanged();
            emit processingChanged();
            emit workflowChanged();
        })
    };
    emit subtitleOcrProcessingChanged();
    emit processingChanged();
}

bool DubbingController::subtitleOcrProcessing() const
{
    return m_independentSubtitleOcrActive || (m_subtitleOcr && m_subtitleOcr->processing());
}

bool DubbingController::isTranscriptionRunnerActive() const
{
    if (!m_runner || !m_runner->processing())
        return false;

    const QString stage = m_runner->stage().trimmed().toLower();
    return stage == QStringLiteral("transcription")
        || stage == QStringLiteral("transcribe");
}

bool DubbingController::speechToTextProcessing() const
{
    return isTranscriptionRunnerActive();
}

bool DubbingController::sttCanRunAlongsideSubtitleOcr() const
{
    return canRunIndependentAudioSttAlongsideCurrentWork();
}

bool DubbingController::subtitleOcrCanRunAlongsideStt() const
{
    return canRunIndependentSubtitleOcrAlongsideCurrentWork();
}

QVariantMap DubbingController::dubbingOcrRoi() const
{
    if (m_subtitleOcr) {
        return {{QStringLiteral("x"), m_subtitleOcr->roiX()},
                {QStringLiteral("y"), m_subtitleOcr->roiY()},
                {QStringLiteral("width"), m_subtitleOcr->roiWidth()},
                {QStringLiteral("height"), m_subtitleOcr->roiHeight()}};
    }
    return m_project.transcriptConfiguration.value(QStringLiteral("ocrRoi")).toMap();
}

int DubbingController::unresolvedTranscriptConflictCount() const
{
    int count = 0;
    for (const QVariant &value : m_project.segments) {
        const QVariantMap segment = value.toMap();
        if (segment.value(QStringLiteral("fusionNeedsReview")).toBool()
            || segment.value(QStringLiteral("fusionStatus")).toString()
                   == QStringLiteral("conflict")) {
            ++count;
        }
    }
    return count;
}

bool DubbingController::hasUnresolvedTranscriptConflicts() const
{
    return unresolvedTranscriptConflictCount() > 0;
}

bool DubbingController::dubbingOcrRoiVisible() const
{
    // OCR is a separately runnable source. Its crop editor must remain
    // available while STT is selected or running, otherwise the user cannot
    // prepare OCR independently before reconciling the two saved results.
    return m_subtitleOcr != nullptr;
}

bool DubbingController::setDubbingOcrRoi(const QVariantMap &roi)
{
    if (!m_subtitleOcr || !dubbingOcrRoiVisible()) return false;
    const bool accepted = m_subtitleOcr->setRoi(roi.value(QStringLiteral("x")).toDouble(),
        roi.value(QStringLiteral("y")).toDouble(), roi.value(QStringLiteral("width")).toDouble(),
        roi.value(QStringLiteral("height")).toDouble());
    if (!accepted) return false;
    // The OCR cache key includes this normalized rectangle. Keeping STT
    // segments intact means switching OCR/STT/OCR never destroys STT work.
    m_project.transcriptConfiguration.insert(QStringLiteral("ocrRoi"), dubbingOcrRoi());
    persistAfterEdit();
    emit projectChanged();
    emit workflowChanged();
    return true;
}

bool DubbingController::presetDubbingOcrLowerRegion()
{
    if (!m_subtitleOcr || !dubbingOcrRoiVisible()) return false;
    m_subtitleOcr->setLowerRegionPreset();
    m_project.transcriptConfiguration.insert(QStringLiteral("ocrRoi"), dubbingOcrRoi());
    persistAfterEdit();
    emit projectChanged();
    return true;
}

bool DubbingController::resetDubbingOcrRoi()
{
    if (!m_subtitleOcr || !dubbingOcrRoiVisible()) return false;
    m_subtitleOcr->resetRoi();
    m_project.transcriptConfiguration.insert(QStringLiteral("ocrRoi"), dubbingOcrRoi());
    persistAfterEdit();
    emit projectChanged();
    return true;
}

bool DubbingController::previewDubbingOcrCrop(qint64 positionMs)
{
    return m_subtitleOcr && dubbingOcrRoiVisible()
        && m_subtitleOcr->requestCropPreview(positionMs);
}

void DubbingController::applyStoredSubtitleOcrConfiguration()
{
    if (!m_subtitleOcr) return;
    const QString route = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrExecutionRoute")).toString().trimmed().toLower();
    if (route == QStringLiteral("local-cpu") || route == QStringLiteral("colab-gpu"))
        m_subtitleOcr->setExecutionRoute(route);
    const QString localEngine = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrLocalEngineId")).toString().trimmed().toLower();
    if (!localEngine.isEmpty()) m_subtitleOcr->setLocalEngine(localEngine);
    const QString model = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrColabModelId")).toString().trimmed().toLower();
    if (!model.isEmpty()) m_subtitleOcr->setColabModelId(model);
    const QString language = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrLanguage")).toString().trimmed();
    if (!language.isEmpty()) m_subtitleOcr->setOcrLanguage(language);
    const QVariantMap roi = m_project.transcriptConfiguration.value(QStringLiteral("ocrRoi")).toMap();
    if (!roi.isEmpty()) m_subtitleOcr->setRoi(roi.value(QStringLiteral("x")).toDouble(),
                                               roi.value(QStringLiteral("y")).toDouble(),
                                               roi.value(QStringLiteral("width")).toDouble(),
                                               roi.value(QStringLiteral("height")).toDouble());
    const qint64 interval = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrSampleIntervalMs")).toLongLong();
    if (interval > 0) m_subtitleOcr->setSampleIntervalMs(interval);
    const double confidence = m_project.transcriptConfiguration.value(
        QStringLiteral("ocrMinimumConfidence")).toDouble();
    if (confidence > 0) m_subtitleOcr->setMinimumConfidence(confidence);
}

QVariantMap DubbingController::effectiveTranscriptConfiguration(bool captureOcrSettings)
{
    QVariantMap selected = m_workflowNodeConfigurations.value(QStringLiteral("transcribe")).toMap();
    QVariantMap parameters = selected.value(QStringLiteral("parameters")).toMap();
    if (parameters.isEmpty()) parameters = selected;
    for (auto it = m_project.transcriptConfiguration.cbegin();
         it != m_project.transcriptConfiguration.cend(); ++it) {
        parameters.insert(it.key(), it.value());
    }
    const QString persistedSttProvider = parameters.value(
        QStringLiteral("sttExecutionProvider")).toString().trimmed();
    const QString persistedSttModel = parameters.value(
        QStringLiteral("sttModelId")).toString().trimmed();
    if (!persistedSttProvider.isEmpty()) {
        parameters.insert(QStringLiteral("executionProvider"), persistedSttProvider);
        selected.insert(QStringLiteral("executionProvider"), persistedSttProvider);
    }
    if (!persistedSttModel.isEmpty()) {
        parameters.insert(QStringLiteral("modelId"), persistedSttModel);
        selected.insert(QStringLiteral("modelId"), persistedSttModel);
    }
    QString mode = parameters.value(QStringLiteral("transcriptSource"), QStringLiteral("stt"))
                       .toString().trimmed().toLower();
    // Older projects stored the coupled `stt+ocr` mode. Preserve the user's
    // intent but migrate it to the explicit local reconciliation step.
    mode = normalizedTranscriptSource(mode);
    parameters.insert(QStringLiteral("transcriptSource"), mode);
    parameters.insert(QStringLiteral("fusionPolicy"),
                      DubbingTranscriptFusionService::normalizePolicy(
                      parameters.value(QStringLiteral("fusionPolicy"),
                                           QStringLiteral("prefer-ocr")).toString()));
    if (captureOcrSettings && m_subtitleOcr) {
        const QVariantMap roi{{QStringLiteral("x"), m_subtitleOcr->roiX()},
                              {QStringLiteral("y"), m_subtitleOcr->roiY()},
                              {QStringLiteral("width"), m_subtitleOcr->roiWidth()},
                              {QStringLiteral("height"), m_subtitleOcr->roiHeight()}};
        parameters.insert(QStringLiteral("ocrLanguage"), m_subtitleOcr->ocrLanguage());
        parameters.insert(QStringLiteral("ocrExecutionRoute"), m_subtitleOcr->executionRoute());
        parameters.insert(QStringLiteral("ocrLocalEngineId"), m_subtitleOcr->localEngineId());
        parameters.insert(QStringLiteral("ocrLocalEngineVersion"), m_subtitleOcr->localEngineVersion());
        parameters.insert(QStringLiteral("ocrColabModelId"), m_subtitleOcr->colabModelId());
        parameters.insert(QStringLiteral("ocrRoi"), roi);
        parameters.insert(QStringLiteral("ocrSampleIntervalMs"), m_subtitleOcr->sampleIntervalMs());
        parameters.insert(QStringLiteral("ocrMinimumConfidence"), m_subtitleOcr->minimumConfidence());
    }
    m_project.transcriptConfiguration = {
        {QStringLiteral("transcriptSource"), parameters.value(QStringLiteral("transcriptSource"))},
        {QStringLiteral("fusionPolicy"), parameters.value(QStringLiteral("fusionPolicy"))},
        {QStringLiteral("sttExecutionProvider"), parameters.value(QStringLiteral("sttExecutionProvider"))},
        {QStringLiteral("sttModelId"), parameters.value(QStringLiteral("sttModelId"))},
        {QStringLiteral("ocrLanguage"), parameters.value(QStringLiteral("ocrLanguage"))},
        {QStringLiteral("ocrExecutionRoute"), parameters.value(QStringLiteral("ocrExecutionRoute"))},
        {QStringLiteral("ocrLocalEngineId"), parameters.value(QStringLiteral("ocrLocalEngineId"))},
        {QStringLiteral("ocrLocalEngineVersion"), parameters.value(QStringLiteral("ocrLocalEngineVersion"))},
        {QStringLiteral("ocrColabModelId"), parameters.value(QStringLiteral("ocrColabModelId"))},
        {QStringLiteral("ocrRoi"), parameters.value(QStringLiteral("ocrRoi"))},
        {QStringLiteral("ocrSampleIntervalMs"), parameters.value(QStringLiteral("ocrSampleIntervalMs"))},
        {QStringLiteral("ocrMinimumConfidence"), parameters.value(QStringLiteral("ocrMinimumConfidence"))},
        {QStringLiteral("sttSegments"), parameters.value(QStringLiteral("sttSegments"))},
        {QStringLiteral("ocrSegments"), parameters.value(QStringLiteral("ocrSegments"))},
        {QStringLiteral("reconciledSegments"), parameters.value(QStringLiteral("reconciledSegments"))}
    };
    selected.insert(QStringLiteral("parameters"), parameters);
    return selected;
}

QVariantMap DubbingController::selectedCloneVoicePreset() const
{
    const QString selectedId = m_project.ttsVoiceId.trimmed();
    if (selectedId.startsWith(QStringLiteral("builtin:"))) return {};
    if (selectedId.isEmpty()) return {};
    for (const QVariant &entry : m_cloneVoicePresets) {
        const QVariantMap preset = entry.toMap();
        if (preset.value(QStringLiteral("id")).toString() == selectedId)
            return preset;
    }
    return {};
}

bool DubbingController::cloneVoiceSelectionValid() const
{
    const QString selectedId = m_project.ttsVoiceId.trimmed();
    if (selectedId.startsWith(QStringLiteral("builtin:"))) {
        const QString voice = selectedId.mid(QStringLiteral("builtin:").size()).trimmed();
        return !voice.isEmpty();
    }
    if (!cloneVoiceSelectionRequired()) return false;
    const QVariantMap preset = selectedCloneVoicePreset();
    const QString audioPath = PathUtils::urlToLocalPath(
        preset.value(QStringLiteral("audioPath")).toString());
    const bool validPreset = !preset.value(QStringLiteral("id")).toString().trimmed().isEmpty()
        && !audioPath.isEmpty() && QFileInfo(audioPath).isFile()
        && preset.value(QStringLiteral("valid"), true).toBool()
        && preset.value(QStringLiteral("compatible"), false).toBool();
    if (!validPreset)
        return false;

    const QVariantMap synthesis = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    const ExecutionProvider provider = configuredSynthesisProvider(synthesis);
    if (provider == ExecutionProvider::ApiGateway)
        return false;
    // Dubbing uses the backend's real reference-clone operation. An unloaded
    // model is handled separately by the workflow's normal readiness state.
    if (provider != ExecutionProvider::LocalDev || !m_tts || !m_tts->isModelLoaded())
        return true;
    if (!localTtsSupportsReferenceClone(m_tts->familyConfig()))
        return false;
    const QString selectedTarget = parameters.value(
        QStringLiteral("voiceCloneModelId")).toString().trimmed();
    return selectedTarget.isEmpty()
        || localTtsMatchesReferenceCloneTarget(m_tts->familyConfig(), selectedTarget);
}

QString DubbingController::cloneVoiceSelectionError() const
{
    if (m_project.ttsVoiceId.trimmed().isEmpty()) {
        return m_cloneVoicePresets.isEmpty()
            ? QStringLiteral("Select a built-in TTS voice before generating dubbing audio. Saved voices can be created in Voice Cloning Studio.")
            : QStringLiteral("Select one TTS voice before generating dubbing audio.");
    }
    if (m_project.ttsVoiceId.startsWith(QStringLiteral("builtin:"))) return {};
    if (!cloneVoiceSelectionRequired())
        return QStringLiteral("Saved Voice Cloning library is unavailable. Select a built-in TTS voice or restore the library.");
    const QVariantMap preset = selectedCloneVoicePreset();
    if (preset.isEmpty())
        return QStringLiteral("The selected clone voice is no longer available. Select another saved voice; LA Studio will not substitute one automatically.");
    if (!preset.value(QStringLiteral("compatible"), false).toBool()) {
        return QStringLiteral("The selected saved voice has no valid VieNeu or OmniVoice target route. Repair its reference audio or choose another voice.");
    }
    const QString validationError = preset.value(QStringLiteral("validationError")).toString().trimmed();
    if (!validationError.isEmpty())
        return QStringLiteral("The selected clone voice cannot be used: %1").arg(validationError);
    if (!QFileInfo(PathUtils::urlToLocalPath(
            preset.value(QStringLiteral("audioPath")).toString())).isFile()) {
        return QStringLiteral("The reference audio for the selected clone voice is missing. Repair or replace that preset before generating dubbing audio.");
    }
    const QVariantMap synthesis = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    const ExecutionProvider provider = configuredSynthesisProvider(synthesis);
    if (provider == ExecutionProvider::ApiGateway) {
        return QStringLiteral("This API Gateway TTS route cannot reuse saved clone voices. Select a built-in voice or Direct Colab; LA Studio will not substitute a voice.");
    }
    if (provider == ExecutionProvider::LocalDev && m_tts && m_tts->isModelLoaded()
        && !localTtsSupportsReferenceClone(m_tts->familyConfig())) {
        return QStringLiteral("The active local TTS runtime cannot clone a reference voice. Load VieNeu, OmniVoice, or Qwen3-TTS, or choose Direct Colab.");
    }
    if (provider == ExecutionProvider::LocalDev && m_tts && m_tts->isModelLoaded()) {
        const QString selectedTarget = parameters.value(
            QStringLiteral("voiceCloneModelId")).toString().trimmed();
        if (!selectedTarget.isEmpty()
            && !localTtsMatchesReferenceCloneTarget(m_tts->familyConfig(), selectedTarget)) {
            return QStringLiteral("The selected clone target does not match the loaded local TTS runtime. Load the selected VieNeu or OmniVoice model, then try again.");
        }
    }
    return {};
}

void DubbingController::refreshCloneVoicePresets()
{
    QVariantList refreshed;
    const QVariantMap synthesis = m_workflowNodeConfigurations.value(
        QStringLiteral("synthesize")).toMap();
    const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    const QString targetModel = configuredVoiceCloneTargetModel(parameters);
    const QString target = canonicalVoiceCloneTarget(targetModel);
    if (m_voiceClonePresetsService) {
        for (const QVariant &entry : m_voiceClonePresetsService->allPresets()) {
            QVariantMap preset = entry.toMap();
            const QString id = preset.value(QStringLiteral("id")).toString().trimmed();
            QString familyId = preset.value(QStringLiteral("familyId")).toString().trimmed().toLower();
            if (familyId.isEmpty()) {
                familyId = preset.value(QStringLiteral("modelFamily")).toString().trimmed().toLower();
            }
            if (familyId.isEmpty()) {
                familyId = QStringLiteral("omnivoice");
            }
            const QString audioPath = PathUtils::urlToLocalPath(
                preset.value(QStringLiteral("audioPath")).toString());
            if (id.isEmpty()) continue;
            preset.insert(QStringLiteral("audioPath"), audioPath);
            preset.insert(QStringLiteral("familyId"), familyId);
            preset.insert(QStringLiteral("sourceModelFamily"), familyId);
            preset.insert(QStringLiteral("voiceCloneTarget"), target);
            preset.insert(QStringLiteral("voiceCloneModelId"), targetModel);
            preset.insert(QStringLiteral("valid"), preset.value(QStringLiteral("valid"), true).toBool());
            // Compatibility is determined by the selected target and managed
            // reference asset, never by the source model that created it.
            preset.insert(QStringLiteral("compatible"),
                          preset.value(QStringLiteral("valid"), false).toBool()
                              && DubbingColabModelRoutes::supports(
                                  QStringLiteral("voice-cloning"), targetModel));
            refreshed.append(preset);
        }
    }
    if (m_cloneVoicePresets == refreshed) return;
    m_cloneVoicePresets = refreshed;
    emit cloneVoiceSelectionChanged();
    emit workflowChanged();
}

bool DubbingController::selectCloneVoicePreset(const QString &presetId)
{
    const QVariantMap synthesis = m_workflowNodeConfigurations.value(
        QStringLiteral("synthesize")).toMap();
    const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    return selectCloneVoicePresetForTarget(presetId,
                                           configuredVoiceCloneTargetModel(parameters));
}

bool DubbingController::selectCloneVoicePresetForTarget(const QString &presetId,
                                                        const QString &targetModel)
{
    refreshCloneVoicePresets();
    const QString normalized = presetId.trimmed();
    if (normalized.isEmpty()) {
        setError(QStringLiteral("Select a saved clone voice before generating dubbing audio."));
        return false;
    }
    QVariantMap selected;
    for (const QVariant &entry : m_cloneVoicePresets) {
        if (entry.toMap().value(QStringLiteral("id")).toString() == normalized) {
            selected = entry.toMap();
            break;
        }
    }
    if (selected.isEmpty()) {
        setError(QStringLiteral("The selected clone voice is unavailable or its reference audio is missing."));
        return false;
    }
    const QString cloneModel = normalizedVoiceCloneTargetModel(targetModel);
    const QString cloneTarget = canonicalVoiceCloneTarget(cloneModel);
    const QVariantList targets = selected.value(QStringLiteral(
        "voiceModelTargets")).toList();
    const QVariantList compatibleFamilies = selected.value(QStringLiteral(
        "compatibleModelFamilies")).toList();
    const QString sourceFamily = selected.value(QStringLiteral(
        "sourceModelFamily"), selected.value(QStringLiteral("familyId"))).toString()
        .trimmed().toLower();
    const bool universalTarget = cloneTarget == QStringLiteral("vieneu")
        || cloneTarget == QStringLiteral("omnivoice");
    const bool targetDeclared = universalTarget
        ? targets.contains(cloneTarget)
        : (compatibleFamilies.contains(cloneModel) || sourceFamily == cloneModel);
    if (!targetDeclared
        || !selected.value(QStringLiteral("valid"), false).toBool()
        || !DubbingColabModelRoutes::supports(QStringLiteral("voice-cloning"), cloneModel)) {
        setError(QStringLiteral("That saved voice cannot be prepared for the selected clone target. Repair the reference audio or choose another target."));
        return false;
    }

    QVariantMap synthesis = m_workflowNodeConfigurations.value(
        QStringLiteral("synthesize")).toMap();
    QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    const bool sameSelection = m_project.ttsVoiceId == normalized
        && parameters.value(QStringLiteral("voiceCloneModelId")).toString().trimmed().toLower()
            == cloneModel;
    if (sameSelection) return true;
    m_project.ttsVoiceId = normalized;
    m_project.cloneVoicePresetId = normalized;
    // Persist the exact clone target. The source family remains display
    // provenance and never controls worker routing.
    parameters.insert(QStringLiteral("voiceCloneModelId"), cloneModel);
    synthesis.insert(QStringLiteral("parameters"), parameters);
    m_workflowNodeConfigurations.insert(QStringLiteral("synthesize"), synthesis);
    m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    refreshCloneVoicePresets();
    emit cloneVoiceSelectionChanged();
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

bool DubbingController::applySelectedCloneVoiceToSynthesis(QVariantMap *settings)
{
    if (!settings) return false;
    refreshCloneVoicePresets();
    if (!cloneVoiceSelectionValid()) {
        setError(cloneVoiceSelectionError());
        return false;
    }
    const QString selectedId = m_project.ttsVoiceId.trimmed();
    settings->insert(QStringLiteral("ttsVoiceId"), selectedId);
    if (selectedId.startsWith(QStringLiteral("builtin:"))) {
        // Settings are assembled from a persisted node configuration.  Strip
        // every clone-only transient key when the user returns to an ordinary
        // TTS voice so a previous saved profile can never alter this run.
        settings->remove(QStringLiteral("savedTtsVoicePreset"));
        settings->remove(QStringLiteral("cloneVoicePreset"));
        settings->remove(QStringLiteral("voiceCloningEnabled"));
        settings->remove(QStringLiteral("voiceCloneModelId"));
        settings->insert(QStringLiteral("voice"), selectedId.mid(QStringLiteral("builtin:").size()));
    } else {
        // Dubbing receives the verified managed reference and explicit target
        // model. The synthesis job dispatches the target backend's clone path.
        settings->remove(QStringLiteral("voice"));
        settings->remove(QStringLiteral("cloneVoicePreset"));
        settings->remove(QStringLiteral("voiceCloningEnabled"));
        const QVariantMap selected = selectedCloneVoicePreset();
        settings->insert(QStringLiteral("voiceCloneModelId"), selected.value(
            QStringLiteral("voiceCloneModelId")));
        settings->insert(QStringLiteral("savedTtsVoicePreset"), selected);
    }
    return true;
}

QVariantList DubbingController::ttsVoiceOptions() const
{
    QVariantList result;
    const QVariantMap synthesis = m_workflowNodeConfigurations.value(
        QStringLiteral("synthesize")).toMap();
    const QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
    QString voice = parameters.value(QStringLiteral("voice")).toString().trimmed();
    if (voice.isEmpty()) {
        voice = DubbingColabModelRoutes::defaultVoiceForTtsModel(
            parameters.value(QStringLiteral("modelId")).toString());
    }
    if (!voice.isEmpty()) {
        result.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("builtin:") + voice},
                                  {QStringLiteral("name"), QStringLiteral("Mặc định · %1").arg(voice)},
                                  {QStringLiteral("group"), QStringLiteral("Giọng hệ thống mặc định")},
                                  {QStringLiteral("kind"), QStringLiteral("builtin")},
                                  {QStringLiteral("valid"), true}});
    }
    for (const QVariant &entry : m_cloneVoicePresets) {
        QVariantMap option = entry.toMap();
        option.insert(QStringLiteral("group"), QStringLiteral("Thư viện giọng đọc (Voice Gallery)"));
        option.insert(QStringLiteral("kind"), QStringLiteral("saved-clone"));
        const QString savedVoiceName = option.value(QStringLiteral("name")).toString();
        option.insert(QStringLiteral("name"), savedVoiceName);
        result.append(option);
    }
    return result;
}

bool DubbingController::selectTtsVoice(const QString &voiceId)
{
    refreshCloneVoicePresets();
    const QString normalized = voiceId.trimmed();
    if (normalized.startsWith(QStringLiteral("builtin:"))) {
        const QString voice = normalized.mid(QStringLiteral("builtin:").size()).trimmed();
        if (voice.isEmpty()) {
            setError(QStringLiteral("Choose a valid built-in TTS voice."));
            return false;
        }
        QVariantMap synthesis = m_workflowNodeConfigurations.value(
            QStringLiteral("synthesize")).toMap();
        QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
        const bool changed = m_project.ttsVoiceId != normalized
            || parameters.contains(QStringLiteral("voiceCloneModelId"));
        if (!changed) return true;
        m_project.ttsVoiceId = normalized;
        m_project.cloneVoicePresetId = normalized;
        // A built-in voice is not a saved clone.  Clearing only the clone
        // identity prevents a later run from silently using an old clone
        // worker while preserving the ordinary selected TTS model.
        parameters.remove(QStringLiteral("voiceCloneModelId"));
        synthesis.insert(QStringLiteral("parameters"), parameters);
        m_workflowNodeConfigurations.insert(QStringLiteral("synthesize"), synthesis);
        m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
        emit cloneVoiceSelectionChanged();
        emit projectChanged();
        emit workflowChanged();
        persistAfterEdit();
        return true;
    }
    return selectCloneVoicePreset(normalized);
}




// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "controllers/dubbing/parts/DubbingController_Preflight.cpp"
#include "controllers/dubbing/parts/DubbingController_MediaQueue.cpp"
#include "controllers/dubbing/parts/DubbingController_Workflow.cpp"
#include "controllers/dubbing/parts/DubbingController_Project.cpp"
#include "controllers/dubbing/parts/DubbingController_Stages.cpp"
#include "controllers/dubbing/parts/DubbingController_Artifacts.cpp"
#include "controllers/dubbing/parts/DubbingController_Segments.cpp"

} // namespace LAStudio
