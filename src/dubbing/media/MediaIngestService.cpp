#include "dubbing/media/MediaIngestService.h"

#include "core/services/MediaRuntimeLocator.h"
#include "core/storage/PathUtils.h"
#include "dubbing/media/AtomicMediaCommit.h"
#include "dubbing/media/MediaProcessTimeout.h"
#include "audio/io/WavIO.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>

namespace LAStudio {

namespace {

bool readableAudioArtifact(const QString &path)
{
    const WavIO::WavData audio = WavIO::loadAsFloat(path);
    return !audio.samples.isEmpty() && audio.sampleRate > 0 && audio.channels > 0;
}

} // namespace

MediaIngestService::MediaIngestService(QObject *parent)
    : QObject(parent)
{
    m_processTimeout.setSingleShot(true);
    connect(&m_hashWatcher, &QFutureWatcher<HashResult>::finished,
            this, &MediaIngestService::onHashFinished);
    connect(&m_artifactWatcher, &QFutureWatcher<bool>::finished,
            this, &MediaIngestService::onArtifactValidationFinished);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &MediaIngestService::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &MediaIngestService::onProcessError);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &MediaIngestService::onReadyReadStandardError);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (m_stage == Stage::Probe) m_probeOutput += m_process.readAllStandardOutput();
    });
    connect(&m_processTimeout, &QTimer::timeout,
            this, &MediaIngestService::onProcessTimeout);
}

QString MediaIngestService::ffmpegPath() const
{
    return MediaRuntimeLocator::resolve().ffmpeg;
}

QString MediaIngestService::ffprobePath() const
{
    return MediaRuntimeLocator::resolve().ffprobe;
}

bool MediaIngestService::available() const
{
    return !ffmpegPath().isEmpty() && !ffprobePath().isEmpty();
}

void MediaIngestService::ingest(const QString &path)
{
    cancel();
    m_terminal = false;
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        fail(QStringLiteral("Media file does not exist: %1").arg(path));
        return;
    }
    if (!available()) {
        fail(QStringLiteral("Bundled FFmpeg and FFprobe are unavailable. Repair or reinstall LA Studio."));
        return;
    }

    m_inputPath = info.absoluteFilePath();
    const QString inputPath = m_inputPath;
    const quint64 requestId = ++m_nextHashRequestId;
    m_activeHashRequestId = requestId;
    emit progress(1);
    m_hashWatcher.setFuture(QtConcurrent::run([inputPath, requestId]() {
        HashResult result;
        result.requestId = requestId;
        QFile input(inputPath);
        if (!input.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("Cannot read media file: %1").arg(input.errorString());
            return result;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!input.atEnd()) {
            const QByteArray chunk = input.read(1024 * 1024);
            if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
                result.error = QStringLiteral("Cannot hash media file: %1").arg(input.errorString());
                return result;
            }
            hash.addData(chunk);
        }
        result.success = true;
        result.hash = QString::fromLatin1(hash.result().toHex());
        return result;
    }));
}

void MediaIngestService::onHashFinished()
{
    const HashResult result = m_hashWatcher.result();
    if (m_terminal || result.requestId != m_activeHashRequestId) return;
    if (!result.success) {
        fail(result.error.isEmpty() ? QStringLiteral("Cannot hash media file.") : result.error);
        return;
    }
    m_hash = result.hash;
    m_workspace = PathUtils::cacheDir() + QStringLiteral("/dubbing/imports/") + m_hash;
    m_masterPath = m_workspace + QStringLiteral("/master.wav");
    m_analysisPath = m_workspace + QStringLiteral("/analysis.wav");
    const QString stagingId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_masterStagingPath = m_workspace + QStringLiteral("/master.workflow-") + stagingId
        + QStringLiteral(".staging.wav");
    m_analysisStagingPath = m_workspace + QStringLiteral("/analysis.workflow-") + stagingId
        + QStringLiteral(".staging.wav");
    m_manifest.clear();
    m_loudnessMeasurements.clear();
    m_probeOutput.clear();
    m_stderr.clear();
    QDir().mkpath(m_workspace);
    startProbe();
}

void MediaIngestService::startProbe()
{
    m_stage = Stage::Probe;
    m_process.setProgram(ffprobePath());
    m_process.setArguments({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-print_format"), QStringLiteral("json"),
                            QStringLiteral("-show_format"), QStringLiteral("-show_streams"), m_inputPath});
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    m_processTimeout.start(MediaProcessTimeout::configured(
        MediaProcessTimeout::kProbeTimeoutMs));
}

void MediaIngestService::startLoudnessMeasurement()
{
    m_stage = Stage::LoudnessMeasurement;
    m_stderr.clear();
    m_process.setProgram(ffmpegPath());
    m_process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                            QStringLiteral("-i"), m_inputPath,
                            QStringLiteral("-map"), QStringLiteral("0:a:0"),
                            QStringLiteral("-vn"), QStringLiteral("-af"),
                            QStringLiteral("loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json"),
                            QStringLiteral("-f"), QStringLiteral("null"),
                            QStringLiteral("NUL")});
    m_process.start();
    m_processTimeout.start(MediaProcessTimeout::configured(
        MediaProcessTimeout::kFfmpegTimeoutMs));
}

void MediaIngestService::startMaster()
{
    m_stage = Stage::Master;
    m_stderr.clear();
    m_process.setProgram(ffmpegPath());
    const QString filter = QStringLiteral(
        "loudnorm=I=-16:TP=-1.5:LRA=11:measured_I=%1:measured_TP=%2:"
        "measured_LRA=%3:measured_thresh=%4:offset=%5:linear=true:print_format=summary")
        .arg(m_loudnessMeasurements.value(QStringLiteral("input_i")).toDouble(), 0, 'f', 6)
        .arg(m_loudnessMeasurements.value(QStringLiteral("input_tp")).toDouble(), 0, 'f', 6)
        .arg(m_loudnessMeasurements.value(QStringLiteral("input_lra")).toDouble(), 0, 'f', 6)
        .arg(m_loudnessMeasurements.value(QStringLiteral("input_thresh")).toDouble(), 0, 'f', 6)
        .arg(m_loudnessMeasurements.value(QStringLiteral("target_offset")).toDouble(), 0, 'f', 6);
    m_process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                            QStringLiteral("-i"), m_inputPath, QStringLiteral("-map"), QStringLiteral("0:a:0"),
                            QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("2"), QStringLiteral("-ar"), QStringLiteral("48000"),
                            QStringLiteral("-af"), filter, QStringLiteral("-c:a"), QStringLiteral("pcm_f32le"),
                            m_masterStagingPath});
    m_process.start();
    m_processTimeout.start(MediaProcessTimeout::configured(
        MediaProcessTimeout::kFfmpegTimeoutMs));
}

void MediaIngestService::startAnalysis()
{
    m_stage = Stage::Analysis;
    m_stderr.clear();
    m_process.setProgram(ffmpegPath());
    m_process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                            QStringLiteral("-i"), m_masterPath, QStringLiteral("-map"), QStringLiteral("0:a:0"),
                            QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-ar"), QStringLiteral("16000"),
                            QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"), m_analysisStagingPath});
    m_process.start();
    m_processTimeout.start(MediaProcessTimeout::configured(
        MediaProcessTimeout::kFfmpegTimeoutMs));
}

void MediaIngestService::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_terminal || m_stage == Stage::None) return;
    m_processTimeout.stop();
    const bool ok = status == QProcess::NormalExit && exitCode == 0;
    if (!ok) {
        fail(QStringLiteral("Media import failed during %1: %2")
             .arg(m_stage == Stage::Probe ? QStringLiteral("probe")
                   : m_stage == Stage::LoudnessMeasurement ? QStringLiteral("EBU R128 loudness measurement")
                   : QStringLiteral("audio normalization"),
                  QString::fromLocal8Bit(m_stderr).trimmed()));
        return;
    }
    if (m_stage == Stage::Probe) {
        m_probeOutput += m_process.readAllStandardOutput();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(m_probeOutput, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            fail(QStringLiteral("FFprobe returned invalid JSON: %1").arg(parseError.errorString()));
            return;
        }
        const QJsonObject root = doc.object();
        const QJsonObject format = root.value(QStringLiteral("format")).toObject();
        const double duration = format.value(QStringLiteral("duration")).toString().toDouble();
        const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
        bool video = false;
        bool audio = false;
        int sampleRate = 0;
        int channels = 0;
        for (const QJsonValue &value : streams) {
            const QJsonObject stream = value.toObject();
            const QString type = stream.value(QStringLiteral("codec_type")).toString();
            video = video || type == QStringLiteral("video");
            audio = audio || type == QStringLiteral("audio");
            if (type == QStringLiteral("audio") && sampleRate == 0) {
                sampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt();
                channels = stream.value(QStringLiteral("channels")).toInt();
            }
        }
        if (streams.isEmpty() || !streams.at(0).isObject() || !audio) {
            fail(QStringLiteral("Media contains no readable audio stream."));
            return;
        }
        m_manifest.insert(QStringLiteral("sourcePath"), m_inputPath);
        m_manifest.insert(QStringLiteral("sourceHash"), QStringLiteral("sha256:") + m_hash);
        m_manifest.insert(QStringLiteral("sourceDurationMs"), qRound64(duration * 1000.0));
        m_manifest.insert(QStringLiteral("sourceSampleRate"), sampleRate);
        m_manifest.insert(QStringLiteral("sourceChannels"), channels);
        m_manifest.insert(QStringLiteral("sourceIsVideo"), video);
        m_manifest.insert(QStringLiteral("workspacePath"), m_workspace);
        QFile cachedManifest(m_workspace + QStringLiteral("/manifest.json"));
        bool cacheIsValidated = false;
        if (cachedManifest.open(QIODevice::ReadOnly)) {
            QJsonParseError cachedParseError;
            const QJsonDocument cachedDocument = QJsonDocument::fromJson(
                cachedManifest.readAll(), &cachedParseError);
            cacheIsValidated = cachedParseError.error == QJsonParseError::NoError
                && cachedDocument.isObject()
                && cachedDocument.object().value(QStringLiteral("normalizationMethod")).toString()
                    == QStringLiteral("ebur128-r128-2pass");
            if (cacheIsValidated) {
                const QVariantMap cached = cachedDocument.object().toVariantMap();
                m_manifest.insert(QStringLiteral("normalizationMethod"),
                                  cached.value(QStringLiteral("normalizationMethod")));
                m_manifest.insert(QStringLiteral("normalization"),
                                  cached.value(QStringLiteral("normalization")));
            }
            // Windows does not allow QSaveFile::commit() to replace a target
            // that is still held open by this reader.  Close the cache
            // descriptor before either the atomic refresh or a full rebuild.
            cachedManifest.close();
        }
        if (cacheIsValidated) {
            startCacheValidation();
            return;
        }
        startLoudnessMeasurement();
    } else if (m_stage == Stage::LoudnessMeasurement) {
        QString measurementError;
        if (!parseLoudnessMeasurement(&measurementError)) {
            fail(measurementError);
            return;
        }
        startMaster();
    } else if (m_stage == Stage::Master) {
        startArtifactValidation(m_masterStagingPath, Stage::MasterValidation);
    } else if (m_stage == Stage::Analysis) {
        startArtifactValidation(m_analysisStagingPath, Stage::AnalysisValidation);
    }
}

void MediaIngestService::startArtifactValidation(const QString &path, Stage validationStage)
{
    if (m_terminal) return;
    m_stage = validationStage;
    m_artifactWatcher.setFuture(QtConcurrent::run([path]() {
        return readableAudioArtifact(path);
    }));
}

void MediaIngestService::startCacheValidation()
{
    if (m_terminal) return;
    m_stage = Stage::CacheValidation;
    const QString masterPath = m_masterPath;
    const QString analysisPath = m_analysisPath;
    m_artifactWatcher.setFuture(QtConcurrent::run([masterPath, analysisPath]() {
        return readableAudioArtifact(masterPath) && readableAudioArtifact(analysisPath);
    }));
}

void MediaIngestService::onArtifactValidationFinished()
{
    if (m_terminal || (m_stage != Stage::MasterValidation
                       && m_stage != Stage::AnalysisValidation
                       && m_stage != Stage::CacheValidation)) return;
    const Stage validationStage = m_stage;
    if (!m_artifactWatcher.result()) {
        if (validationStage == Stage::CacheValidation) {
            startLoudnessMeasurement();
            return;
        }
        fail(validationStage == Stage::MasterValidation
                 ? QStringLiteral("FFmpeg did not create a readable normalized master audio file.")
                 : QStringLiteral("FFmpeg did not create a readable analysis audio file."));
        return;
    }
    if (validationStage == Stage::CacheValidation) {
        m_manifest.insert(QStringLiteral("masterAudioPath"), m_masterPath);
        m_manifest.insert(QStringLiteral("analysisAudioPath"), m_analysisPath);
        finishCached();
        return;
    }
    QString commitError;
    if (validationStage == Stage::MasterValidation) {
        if (!AtomicMediaCommit::commit(m_masterStagingPath, m_masterPath, &commitError)) {
            fail(commitError);
            return;
        }
        QFile::remove(m_masterStagingPath);
        startAnalysis();
        return;
    }
    if (!AtomicMediaCommit::commit(m_analysisStagingPath, m_analysisPath, &commitError)) {
        fail(commitError);
        return;
    }
    QFile::remove(m_analysisStagingPath);
    finishAnalysis();
}

void MediaIngestService::finishCached()
{
    m_manifest.insert(QStringLiteral("manifestVersion"), 1);
    const QByteArray payload = QJsonDocument::fromVariant(m_manifest)
        .toJson(QJsonDocument::Indented);
    QSaveFile file(m_workspace + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly)
        || file.write(payload) != payload.size()
        || !file.commit()) {
        fail(QStringLiteral("Cannot write normalized media manifest."));
        return;
    }
    m_terminal = true;
    m_stage = Stage::None;
    emit progress(100);
    emit finished(true, m_manifest, QString());
}

void MediaIngestService::finishAnalysis()
{
    m_manifest.insert(QStringLiteral("masterAudioPath"), m_masterPath);
    m_manifest.insert(QStringLiteral("analysisAudioPath"), m_analysisPath);
    m_manifest.insert(QStringLiteral("normalizationMethod"),
                      QStringLiteral("ebur128-r128-2pass"));
    m_manifest.insert(QStringLiteral("normalization"), QVariantMap{
        {QStringLiteral("standard"), QStringLiteral("EBU R128")},
        {QStringLiteral("filter"), QStringLiteral("loudnorm")},
        {QStringLiteral("targetI"), -16.0},
        {QStringLiteral("targetTP"), -1.5},
        {QStringLiteral("targetLRA"), 11.0},
        {QStringLiteral("passes"), 2},
        {QStringLiteral("measurement"), m_loudnessMeasurements}
    });
    const QByteArray payload = QJsonDocument::fromVariant(m_manifest)
        .toJson(QJsonDocument::Indented);
    QSaveFile file(m_workspace + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly)
        || file.write(payload) != payload.size()
        || !file.commit()) {
        fail(QStringLiteral("Cannot write normalized media manifest."));
        return;
    }
    m_terminal = true;
    m_stage = Stage::None;
    emit progress(100);
    emit finished(true, m_manifest, QString());
}

void MediaIngestService::onProcessError(QProcess::ProcessError error)
{
    if (m_terminal || m_stage == Stage::None || error == QProcess::UnknownError) return;
    m_processTimeout.stop();
    const QString tool = m_stage == Stage::Probe ? QStringLiteral("FFprobe") : QStringLiteral("FFmpeg");
    fail(QStringLiteral("Could not run %1: %2").arg(tool, m_process.errorString()));
}

void MediaIngestService::onProcessTimeout()
{
    if (m_terminal || m_stage == Stage::None
        || m_process.state() == QProcess::NotRunning) return;
    const QString stage = m_stage == Stage::Probe
        ? QStringLiteral("probe")
        : m_stage == Stage::LoudnessMeasurement
            ? QStringLiteral("EBU R128 loudness measurement")
            : m_stage == Stage::Master
                ? QStringLiteral("audio normalization")
                : QStringLiteral("analysis audio generation");
    const int timeoutMilliseconds = m_processTimeout.interval();
    m_processTimeout.stop();
    fail(QStringLiteral("Media import timed out during %1 after %2 ms. Check the media runtime and try again.")
             .arg(stage).arg(timeoutMilliseconds));
}

void MediaIngestService::onReadyReadStandardError()
{
    m_stderr += m_process.readAllStandardError();
    if (m_stderr.size() > 1024 * 1024) m_stderr = m_stderr.right(1024 * 1024);
}

void MediaIngestService::fail(const QString &error)
{
    if (m_terminal) return;
    m_terminal = true;
    m_processTimeout.stop();
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    QFile::remove(m_masterStagingPath);
    QFile::remove(m_analysisStagingPath);
    m_stage = Stage::None;
    emit progress(0);
    emit finished(false, {}, error);
}

void MediaIngestService::cancel()
{
    m_terminal = true;
    m_processTimeout.stop();
    m_hashWatcher.cancel();
    m_artifactWatcher.cancel();
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    QFile::remove(m_masterStagingPath);
    QFile::remove(m_analysisStagingPath);
    m_stage = Stage::None;
}

bool MediaIngestService::parseLoudnessMeasurement(QString *error)
{
    const qsizetype objectStart = m_stderr.lastIndexOf('{');
    const qsizetype objectEnd = m_stderr.lastIndexOf('}');
    if (objectStart < 0 || objectEnd <= objectStart) {
        if (error) *error = QStringLiteral("FFmpeg loudnorm did not return an EBU R128 measurement.");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_stderr.mid(objectStart, objectEnd - objectStart + 1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Invalid loudnorm measurement JSON: %1")
            .arg(parseError.errorString());
        return false;
    }
    const QJsonObject object = document.object();
    const QStringList requiredKeys{
        QStringLiteral("input_i"), QStringLiteral("input_tp"),
        QStringLiteral("input_lra"), QStringLiteral("input_thresh"),
        QStringLiteral("target_offset")};
    QVariantMap measurement;
    for (const QString &key : requiredKeys) {
        bool ok = false;
        const double value = object.value(key).toString().toDouble(&ok);
        if (!ok || !qIsFinite(value)) {
            if (error) *error = QStringLiteral("Loudnorm measurement is missing numeric field: %1").arg(key);
            return false;
        }
        measurement.insert(key, value);
    }
    m_loudnessMeasurements = measurement;
    return true;
}

} // namespace LAStudio
