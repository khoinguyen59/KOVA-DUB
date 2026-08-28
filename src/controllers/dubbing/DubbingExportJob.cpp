#include "controllers/dubbing/DubbingExportJob.h"

#include "dubbing/audio/AudioTimelineMixer.h"
#include "dubbing/exporters/DubbingSubtitleService.h"
#include "dubbing/media/AtomicMediaCommit.h"
#include "dubbing/media/MediaToolService.h"
#include "dubbing/media/MediaProcessTimeout.h"
#include "core/services/MediaRuntimeLocator.h"
#include "audio/io/WavIO.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrent>

namespace LAStudio {

namespace {
QString srtTime(qint64 value)
{
    value = qMax<qint64>(0, value);
    const qint64 hours = value / 3600000;
    const qint64 minutes = (value / 60000) % 60;
    const qint64 seconds = (value / 1000) % 60;
    const qint64 millis = value % 1000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0')).arg(millis, 3, 10, QLatin1Char('0'));
}

bool writeTargetSubtitles(const QVariantList &segments, const QString &path)
{
    QStringList lines;
    int cue = 1;
    for (const QVariant &value : segments) {
        const QVariantMap segment = value.toMap();
        const QString text = segment.value(QStringLiteral("targetText")).toString().trimmed();
        const qint64 start = segment.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endMs")).toLongLong();
        if (text.isEmpty() || end <= start) continue;
        lines.append(QString::number(cue++));
        lines.append(QStringLiteral("%1 --> %2").arg(srtTime(start), srtTime(end)));
        lines.append(text);
        lines.append(QString());
    }
    if (lines.isEmpty()) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(lines.join(QLatin1Char('\n')).toUtf8()) >= 0
        && file.commit();
}
}

DubbingExportJob::DubbingExportJob(QObject *parent)
    : QObject(parent)
{
    m_validationTimeout.setSingleShot(true);
    m_renderWatcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_renderWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &DubbingExportJob::onRenderFinished);
    m_mediaTools = new MediaToolService(this);
    connect(m_mediaTools, &MediaToolService::finished,
            this, &DubbingExportJob::onMediaFinished);
    connect(&m_validationProcess, &QProcess::readyReadStandardOutput,
            this, &DubbingExportJob::onValidationReadyReadStandardOutput);
    connect(&m_validationProcess, &QProcess::readyReadStandardError,
            this, &DubbingExportJob::onValidationReadyReadStandardError);
    connect(&m_validationProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &DubbingExportJob::onValidationFinished);
    connect(&m_validationProcess, &QProcess::errorOccurred,
            this, &DubbingExportJob::onValidationError);
    connect(&m_validationTimeout, &QTimer::timeout,
            this, &DubbingExportJob::onValidationTimeout);
}

DubbingExportJob::~DubbingExportJob()
{
    cancel();
    if (m_renderWatcher) {
        m_renderWatcher->cancel();
        m_renderWatcher->waitForFinished();
    }
}

bool DubbingExportJob::renderPreview(const QVariantList &segments, const QString &projectPath,
                                     const QString &backgroundPath, const QString &path,
                                     const QVariantMap &mixConfiguration)
{
    if (m_running) { fail(QStringLiteral("Finish the active export operation first.")); return false; }
    QString outputPath = path;
    if (outputPath.isEmpty()) {
        if (projectPath.isEmpty()) { fail(QStringLiteral("Save the project before rendering a preview.")); return false; }
        outputPath = QFileInfo(projectPath).absolutePath() + QStringLiteral("/preview.wav");
    }
    if (!m_renderWatcher || m_renderWatcher->isRunning()) { fail(QStringLiteral("An audio mix is already running.")); return false; }
    m_running = true;
    m_renderStagingPath = outputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".staging");
    const QString vocalOutputPath = AudioTimelineMixer::vocalStemPath(outputPath);
    m_renderVocalStagingPath = vocalOutputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".staging");
    m_renderCancel = std::make_shared<QAtomicInteger<bool>>(false);
    const auto cancel = m_renderCancel;
    const QString stagingPath = m_renderStagingPath;
    const QString vocalStagingPath = m_renderVocalStagingPath;
    emit progressChanged(QStringLiteral("mix"), 0);
    m_renderWatcher->setFuture(QtConcurrent::run(
        [segments, outputPath, stagingPath, backgroundPath, vocalOutputPath, vocalStagingPath,
         mixConfiguration, cancel]() {
        QString error;
        const bool mixed = AudioTimelineMixer::mixSegments(
            segments, stagingPath, backgroundPath, vocalStagingPath, &error, cancel.get(),
            mixConfiguration);
        const bool vocalCommitted = mixed && !cancel->loadAcquire()
            && AtomicMediaCommit::commit(vocalStagingPath, vocalOutputPath, &error);
        const bool committed = vocalCommitted && !cancel->loadAcquire()
            && AtomicMediaCommit::commit(stagingPath, outputPath, &error);
        QFile::remove(stagingPath);
        QFile::remove(vocalStagingPath);
        return QVariantMap{{QStringLiteral("success"), committed},
                           {QStringLiteral("outputPath"), outputPath},
                           {QStringLiteral("vocalOutputPath"), vocalOutputPath},
                           {QStringLiteral("error"), error}};
    }));
    return true;
}

bool DubbingExportJob::startExport(const QString &sourceMediaPath, const QString &audioPath,
                                   const QString &outputPath, const QVariantList &segments,
                                   const QVariantMap &subtitleConfiguration)
{
    if (m_running) { fail(QStringLiteral("Finish the active export operation first.")); return false; }
    if (outputPath.isEmpty()) { fail(QStringLiteral("Choose an output path.")); return false; }
    if (sourceMediaPath.isEmpty()) { fail(QStringLiteral("Import source media before exporting.")); return false; }
    if (!QFileInfo(sourceMediaPath).isFile()) {
        fail(QStringLiteral("The source media file does not exist.")); return false;
    }
    if (audioPath.isEmpty() || !QFileInfo::exists(audioPath)) {
        fail(QStringLiteral("Generate and render audio preview before exporting.")); return false;
    }
    m_exportDestination = outputPath;
    m_exportAudioPath = audioPath;
    m_sourceMediaPath.clear();
    m_expectSubtitle = false;
    m_sourceHasVideo = false;
    m_sourceDurationMs = 0;
    const bool exportBurnIn = subtitleConfiguration.value(QStringLiteral("burnIn")).toBool();
    const QVariantMap subtitleStyle = subtitleConfiguration.value(QStringLiteral("style")).toMap();
    const bool subtitleUsesTargetText = subtitleConfiguration.value(
        QStringLiteral("textSource"), QStringLiteral("target")).toString().trimmed().toLower()
        != QStringLiteral("source");
    QString subtitleFontDirectory;
    if (exportBurnIn) {
        const QString fontFile = subtitleStyle.value(QStringLiteral("fontFile")).toString().trimmed();
        if (!fontFile.isEmpty()) {
            const QFileInfo fontInfo(fontFile);
            if (!fontInfo.isFile()) {
                clearExportPaths();
                fail(QStringLiteral("The configured subtitle font file no longer exists."));
                return false;
            }
            subtitleFontDirectory = fontInfo.absolutePath();
        }
    }
    const QString suffix = QFileInfo(sourceMediaPath).suffix().toLower();
    const bool video = suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm")
        || suffix == QStringLiteral("avi");
    const QFileInfo destinationInfo(outputPath);
    const QString stagingSuffix = destinationInfo.suffix().isEmpty()
        ? QStringLiteral(".staging") : QStringLiteral(".") + destinationInfo.suffix();
    m_exportStagingPath = outputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + stagingSuffix;
    if (!video) {
        if (!QFile::copy(m_exportAudioPath, m_exportStagingPath)) {
            clearExportPaths();
            fail(QStringLiteral("Failed to stage rendered WAV for export: %1").arg(m_exportStagingPath));
            return false;
        }
        const WavIO::WavData stagedAudio = WavIO::loadAsFloat(m_exportStagingPath);
        if (stagedAudio.samples.isEmpty() || stagedAudio.sampleRate <= 0 || stagedAudio.channels <= 0) {
            QFile::remove(m_exportStagingPath);
            clearExportPaths();
            fail(QStringLiteral("Rendered audio failed validation before export."));
            return false;
        }
        QString error;
        if (!AtomicMediaCommit::commit(m_exportStagingPath, outputPath, &error)) {
            QFile::remove(m_exportStagingPath);
            clearExportPaths();
            fail(error);
            return false;
        }
        QFile::remove(m_exportStagingPath);
        clearExportPaths();
        emit exported(outputPath);
        return true;
    }
    if (!m_mediaTools) { clearExportPaths(); fail(QStringLiteral("Media tool service is unavailable.")); return false; }
    m_running = true;
    emit progressChanged(QStringLiteral("export"), 0);
    m_exportBurnIn = exportBurnIn;
    m_exportSubtitlePath = m_exportStagingPath + (m_exportBurnIn ? QStringLiteral(".ass")
                                                                   : QStringLiteral(".srt"));
    QString subtitleError;
    const bool wroteSubtitle = m_exportBurnIn
        ? DubbingSubtitleService::writeAss(segments,
                                           subtitleStyle,
                                           m_exportSubtitlePath, subtitleUsesTargetText, &subtitleError)
        : DubbingSubtitleService::writeSidecar(segments, m_exportSubtitlePath,
                                                subtitleUsesTargetText, &subtitleError);
    if (!wroteSubtitle) {
        if (m_exportBurnIn) {
            QFile::remove(m_exportStagingPath);
            clearExportPaths();
            m_running = false;
            fail(subtitleError.isEmpty() ? QStringLiteral("Cannot create the burn-in subtitle track.")
                                         : subtitleError);
            return false;
        }
        m_exportSubtitlePath.clear();
    }
    m_sourceMediaPath = sourceMediaPath;
    m_expectSubtitle = !m_exportBurnIn && !m_exportSubtitlePath.isEmpty();
    m_exportSubtitleFontDirectory = subtitleFontDirectory;
    startMediaValidation(sourceMediaPath, ValidationStage::Source);
    return true;
}

void DubbingExportJob::cancel()
{
    if (!m_running) return;
    if (m_renderCancel) m_renderCancel->storeRelease(true);
    if (m_mediaTools) m_mediaTools->cancel();
    m_validationTimeout.stop();
    m_validationStage = ValidationStage::None;
    if (m_validationProcess.state() != QProcess::NotRunning)
        m_validationProcess.kill();
    m_validationOutput.clear();
    m_validationError.clear();
    QFile::remove(m_renderStagingPath);
    QFile::remove(m_renderVocalStagingPath);
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    m_running = false;
    clearExportPaths();
    m_sourceMediaPath.clear();
    m_expectSubtitle = false;
    m_sourceHasVideo = false;
    m_sourceDurationMs = 0;
}

void DubbingExportJob::onRenderFinished()
{
    const QVariantMap result = m_renderWatcher->result();
    QFile::remove(m_renderStagingPath);
    QFile::remove(m_renderVocalStagingPath);
    m_renderStagingPath.clear();
    m_renderVocalStagingPath.clear();
    m_renderCancel.reset();
    m_running = false;
    if (!result.value(QStringLiteral("success")).toBool()) {
        fail(result.value(QStringLiteral("error"), QStringLiteral("Audio mix failed.")).toString());
        return;
    }
    const QString outputPath = result.value(QStringLiteral("outputPath")).toString();
    if (!QFileInfo::exists(outputPath)) { fail(QStringLiteral("Audio mix completed without an output file.")); return; }
    emit progressChanged(QStringLiteral("mix"), 100);
    emit previewReady(outputPath);
}

void DubbingExportJob::onMediaFinished(bool success, const QString &outputPath, const QString &error)
{
    if (!m_running) return;
    if (!success || outputPath != m_exportStagingPath || !QFileInfo::exists(m_exportStagingPath)) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(error.isEmpty() ? QStringLiteral("Media export failed.") : error);
        return;
    }
    startMediaValidation(m_exportStagingPath, ValidationStage::Export);
}

void DubbingExportJob::startMediaValidation(const QString &path, ValidationStage stage)
{
    if (!m_running) return;
    const MediaRuntimePaths media = MediaRuntimeLocator::resolve();
    if (media.ffprobe.isEmpty()) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(QStringLiteral("FFprobe was not found; media validation cannot run."));
        return;
    }
    if (m_validationProcess.state() != QProcess::NotRunning) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(QStringLiteral("Another media validation is already running."));
        return;
    }
    m_validationStage = stage;
    m_validationOutput.clear();
    m_validationError.clear();
    m_validationProcess.setProgram(media.ffprobe);
    m_validationProcess.setWorkingDirectory(QFileInfo(media.ffprobe).absolutePath());
    m_validationProcess.setProcessChannelMode(QProcess::SeparateChannels);
    m_validationProcess.setArguments({QStringLiteral("-v"), QStringLiteral("error"),
                                      QStringLiteral("-print_format"), QStringLiteral("json"),
                                      QStringLiteral("-show_streams"), QStringLiteral("-show_format"),
                                      path});
    m_validationProcess.start();
    m_validationTimeout.start(MediaProcessTimeout::configured(
        MediaProcessTimeout::kValidationTimeoutMs));
}

void DubbingExportJob::onValidationReadyReadStandardOutput()
{
    m_validationOutput += m_validationProcess.readAllStandardOutput();
    if (m_validationOutput.size() > 4 * 1024 * 1024)
        m_validationOutput = m_validationOutput.right(4 * 1024 * 1024);
}

void DubbingExportJob::onValidationReadyReadStandardError()
{
    m_validationError += m_validationProcess.readAllStandardError();
    if (m_validationError.size() > 1024 * 1024)
        m_validationError = m_validationError.right(1024 * 1024);
}

void DubbingExportJob::onValidationFinished(int exitCode, QProcess::ExitStatus status)
{
    onValidationReadyReadStandardOutput();
    onValidationReadyReadStandardError();
    m_validationTimeout.stop();
    if (!m_running || m_validationStage == ValidationStage::None) return;
    QString validationError;
    const ValidationStage stage = m_validationStage;
    m_validationStage = ValidationStage::None;
    if (status != QProcess::NormalExit || exitCode != 0
        || !validateProbeResult(m_validationOutput, stage, &validationError)) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        const QString diagnostics = QString::fromLocal8Bit(m_validationError).trimmed();
        fail(validationError.isEmpty()
                 ? QStringLiteral("Media validation failed: %1").arg(diagnostics)
                 : validationError);
        return;
    }
    if (stage == ValidationStage::Source) {
        m_mediaTools->muxVideoWithAudio(m_sourceMediaPath, m_exportAudioPath,
                                        m_exportSubtitlePath, m_exportStagingPath,
                                        m_exportBurnIn, m_exportSubtitleFontDirectory);
        return;
    }
    QString commitError;
    if (!AtomicMediaCommit::commit(m_exportStagingPath, m_exportDestination, &commitError)) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(commitError);
        return;
    }
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    const QString destination = m_exportDestination;
    clearExportPaths();
    m_sourceMediaPath.clear();
    m_expectSubtitle = false;
    m_sourceHasVideo = false;
    m_sourceDurationMs = 0;
    m_running = false;
    emit progressChanged(QStringLiteral("export"), 100);
    emit exported(destination);
}

void DubbingExportJob::onValidationError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart || !m_running
        || m_validationStage == ValidationStage::None) return;
    m_validationTimeout.stop();
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    clearExportPaths();
    m_validationStage = ValidationStage::None;
    m_running = false;
    fail(QStringLiteral("FFprobe could not be started: %1").arg(m_validationProcess.errorString()));
}

void DubbingExportJob::onValidationTimeout()
{
    if (!m_running || m_validationStage == ValidationStage::None
        || m_validationProcess.state() == QProcess::NotRunning) return;
    const QString stage = m_validationStage == ValidationStage::Source
        ? QStringLiteral("source media") : QStringLiteral("exported media");
    const int timeoutMilliseconds = m_validationTimeout.interval();
    m_validationTimeout.stop();
    m_validationStage = ValidationStage::None;
    m_validationProcess.kill();
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    clearExportPaths();
    m_sourceMediaPath.clear();
    m_expectSubtitle = false;
    m_sourceHasVideo = false;
    m_sourceDurationMs = 0;
    m_running = false;
    fail(QStringLiteral("FFprobe validation timed out during %1 after %2 ms. Check the media runtime and try again.")
             .arg(stage).arg(timeoutMilliseconds));
}

bool DubbingExportJob::validateProbeResult(const QByteArray &payload, ValidationStage stage,
                                          QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (!document.isObject()) {
        if (errorMessage) *errorMessage = QStringLiteral("FFprobe returned invalid JSON: %1")
            .arg(parseError.errorString());
        return false;
    }
    const QJsonArray streams = document.object().value(QStringLiteral("streams")).toArray();
    bool hasVideo = false;
    bool hasAudio = false;
    bool hasSubtitle = false;
    for (const QJsonValue &value : streams) {
        const QString codecType = value.toObject().value(QStringLiteral("codec_type")).toString();
        hasVideo |= codecType == QStringLiteral("video");
        hasAudio |= codecType == QStringLiteral("audio");
        hasSubtitle |= codecType == QStringLiteral("subtitle");
    }
    const QJsonObject format = document.object().value(QStringLiteral("format")).toObject();
    const QJsonValue durationValue = format.value(QStringLiteral("duration"));
    const double duration = durationValue.isString() ? durationValue.toString().toDouble()
                                                      : durationValue.toDouble();
    if (duration <= 0.0) {
        if (errorMessage) *errorMessage = QStringLiteral("FFprobe reported no usable media duration.");
        return false;
    }
    if (stage == ValidationStage::Source) {
        if (!hasVideo) {
            if (errorMessage) *errorMessage = QStringLiteral("Source media has no video stream.");
            return false;
        }
        m_sourceHasVideo = hasVideo;
        m_sourceDurationMs = qRound64(duration * 1000.0);
        return true;
    }
    if (m_sourceHasVideo && !hasVideo) {
        if (errorMessage) *errorMessage = QStringLiteral("Export validation found no video stream.");
        return false;
    }
    if (!hasAudio) {
        if (errorMessage) *errorMessage = QStringLiteral("Export validation found no audio stream.");
        return false;
    }
    if (m_expectSubtitle && !hasSubtitle) {
        if (errorMessage) *errorMessage = QStringLiteral("Export validation found no subtitle stream.");
        return false;
    }
    const qint64 exportDurationMs = qRound64(duration * 1000.0);
    const qint64 toleranceMs = qMax<qint64>(250, m_sourceDurationMs / 20);
    if (qAbs(exportDurationMs - m_sourceDurationMs) > toleranceMs) {
        if (errorMessage) *errorMessage = QStringLiteral("Export duration drift exceeds %1 ms.").arg(toleranceMs);
        return false;
    }
    return true;
}

void DubbingExportJob::clearExportPaths()
{
    m_exportDestination.clear();
    m_exportStagingPath.clear();
    m_exportAudioPath.clear();
    m_exportSubtitlePath.clear();
    m_exportSubtitleFontDirectory.clear();
    m_exportBurnIn = false;
}

void DubbingExportJob::fail(const QString &message)
{
    m_running = false;
    emit failed(message);
}

} // namespace LAStudio
