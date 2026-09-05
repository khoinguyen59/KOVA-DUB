#include "dubbing/audio/AudioTimelineMixer.h"

#include "audio/io/AudioFileDecoder.h"
#include "audio/io/WavIO.h"
#include "core/services/MediaRuntimeLocator.h"
#include "dubbing/media/MediaProcessTimeout.h"

#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtMath>

#include <cmath>

namespace LAStudio {
namespace {

constexpr int kOutputRate = 48000;
constexpr qint64 kMaximumDurationMs = 60LL * 60LL * 1000LL;
// Release packages always bundle FFmpeg. Without it, cap the compatibility
// fallback so an accidental long source cannot consume all process memory.
constexpr qint64 kFallbackMaximumSamples = kOutputRate * 60LL * 10LL;
constexpr float kBaseBackgroundGain = 0.35f;
constexpr float kSidechainThresholdDb = -24.0f;
constexpr float kSidechainRatio = 4.0f;
constexpr float kMaximumDuckDb = 12.0f;
constexpr float kAttackMs = 10.0f;
constexpr float kReleaseMs = 120.0f;
// Keep each FFmpeg invocation comfortably below Windows' CreateProcess command
// length limit. Each cue can include a long project artifact path, so a large
// timeline must be rendered as a reduction tree rather than one giant graph.
constexpr int kMaximumTimelineInputsPerPass = 24;
// FFmpeg's implicit stereo-to-mono matrix is -3 dB per channel.  The native
// fallback averages channels, so use the same deterministic mono/stereo
// mapping in the streaming path. FC keeps a mono source intact; FL/FR average
// the usual stereo source without doubling it.
constexpr auto kExplicitMonoDownmix = "pan=mono|c0=FC+0.5*FL+0.5*FR";

struct TimedClip {
    QString path;
    qint64 startMs = 0;
    qint64 endMs = 0;
};

bool isCancelled(QAtomicInteger<bool> *cancel)
{
    return cancel && cancel->loadAcquire();
}

QString seconds(qint64 milliseconds)
{
    return QString::number(static_cast<double>(milliseconds) / 1000.0, 'f', 3);
}

float decibelsToLinear(float decibels)
{
    return std::pow(10.0f, decibels / 20.0f);
}

float sidechainBackgroundGain(float voiceSample, float currentGain, int sampleRate,
                              float configuredOriginalGain)
{
    const float voiceDb = 20.0f * std::log10(qMax(std::abs(voiceSample), 1.0e-6f));
    const float overThreshold = qMax(0.0f, voiceDb - kSidechainThresholdDb);
    const float requestedDuckDb = qMin(kMaximumDuckDb,
                                       overThreshold * (1.0f - 1.0f / kSidechainRatio));
    const float targetGain = kBaseBackgroundGain * configuredOriginalGain
                             * decibelsToLinear(-requestedDuckDb);
    const float timeConstantMs = targetGain < currentGain ? kAttackMs : kReleaseMs;
    const float coefficient = std::exp(-1000.0f
                                       / (timeConstantMs * static_cast<float>(sampleRate)));
    return targetGain + (currentGain - targetGain) * coefficient;
}

bool runFfmpeg(const QString &program, const QStringList &arguments,
               QAtomicInteger<bool> *cancel, QString *error)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(5000)) {
        if (error) *error = QStringLiteral("Could not start bundled FFmpeg: %1")
            .arg(process.errorString());
        return false;
    }

    QElapsedTimer timeout;
    timeout.start();
    const int timeoutMs = MediaProcessTimeout::configured(MediaProcessTimeout::kFfmpegTimeoutMs);
    while (!process.waitForFinished(100)) {
        if (isCancelled(cancel)) {
            process.kill();
            process.waitForFinished(1000);
            if (error) *error = QStringLiteral("Audio mix cancelled.");
            return false;
        }
        if (timeout.elapsed() > timeoutMs) {
            process.kill();
            process.waitForFinished(1000);
            if (error) *error = QStringLiteral("FFmpeg audio mix timed out after %1 ms.").arg(timeoutMs);
            return false;
        }
    }
    if (isCancelled(cancel)) {
        if (error) *error = QStringLiteral("Audio mix cancelled.");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (detail.size() > 1800) detail = detail.right(1800);
        if (error) *error = QStringLiteral("FFmpeg could not render the audio timeline%1")
            .arg(detail.isEmpty() ? QStringLiteral(".") : QStringLiteral(": %1").arg(detail));
        return false;
    }
    return true;
}

qint64 probeDurationMs(const QString &path, QAtomicInteger<bool> *cancel)
{
    const MediaRuntimePaths runtime = MediaRuntimeLocator::resolve();
    if (!runtime.hasFfprobe()) return 0;
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(runtime.ffprobe, {QStringLiteral("-v"), QStringLiteral("error"),
                                    QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
                                    QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
                                    path});
    if (!process.waitForStarted(5000)) return 0;
    QElapsedTimer timeout;
    timeout.start();
    const int timeoutMs = MediaProcessTimeout::configured(MediaProcessTimeout::kProbeTimeoutMs);
    while (!process.waitForFinished(100)) {
        if (isCancelled(cancel) || timeout.elapsed() > timeoutMs) {
            process.kill();
            process.waitForFinished(1000);
            return 0;
        }
    }
    bool ok = false;
    const double duration = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toDouble(&ok);
    return ok && duration > 0.0 ? qRound64(duration * 1000.0) : 0;
}

bool renderTimelineClipBatch(const MediaRuntimePaths &runtime, const QList<TimedClip> &clips,
                             qint64 durationMs, const QString &outputPath, float dubbedGain,
                             QAtomicInteger<bool> *cancel, QString *error)
{
    if (clips.isEmpty()) {
        if (error) *error = QStringLiteral("Audio timeline batch has no clips.");
        return false;
    }

    QStringList inputs;
    QStringList filterParts;
    QStringList labels;
    const QString duration = seconds(durationMs);
    for (int index = 0; index < clips.size(); ++index) {
        const TimedClip &clip = clips.at(index);
        inputs << QStringLiteral("-i") << clip.path;
        const QString label = QStringLiteral("v%1").arg(index);
        labels << QStringLiteral("[%1]").arg(label);
        const qint64 cueDuration = qMax<qint64>(1, qMin(clip.endMs - clip.startMs,
                                                        durationMs - clip.startMs));
        filterParts << QStringLiteral("[%1:a]aresample=%2,%3,atrim=duration=%4,adelay=%5:all=1,volume=%6[%7]")
            .arg(index).arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix))
            .arg(seconds(cueDuration)).arg(clip.startMs)
            .arg(QString::number(dubbedGain, 'f', 4)).arg(label);
    }
    filterParts << QStringLiteral("%1amix=inputs=%2:normalize=0,apad=pad_dur=%3,atrim=duration=%3[voices]")
        .arg(labels.join(QString())).arg(clips.size()).arg(duration);

    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                          QStringLiteral("-y")};
    arguments << inputs << QStringLiteral("-filter_complex") << filterParts.join(QLatin1Char(';'))
              << QStringLiteral("-map") << QStringLiteral("[voices]")
              << QStringLiteral("-ac") << QStringLiteral("1")
              << QStringLiteral("-ar") << QString::number(kOutputRate)
              << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le")
              << QStringLiteral("-f") << QStringLiteral("wav") << outputPath;
    return runFfmpeg(runtime.ffmpeg, arguments, cancel, error);
}

bool combineTimelineBatches(const MediaRuntimePaths &runtime, const QStringList &inputPaths,
                            qint64 durationMs, const QString &outputPath,
                            QAtomicInteger<bool> *cancel, QString *error)
{
    if (inputPaths.isEmpty()) {
        if (error) *error = QStringLiteral("Audio timeline reduction has no inputs.");
        return false;
    }
    if (inputPaths.size() == 1) {
        QFile::remove(outputPath);
        if (QFile::copy(inputPaths.constFirst(), outputPath)) return true;
        if (error) *error = QStringLiteral("Could not commit reduced audio timeline.");
        return false;
    }

    const QString duration = seconds(durationMs);
    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                          QStringLiteral("-y")};
    QStringList filterParts;
    QStringList labels;
    for (int index = 0; index < inputPaths.size(); ++index) {
        arguments << QStringLiteral("-i") << inputPaths.at(index);
        const QString label = QStringLiteral("b%1").arg(index);
        labels << QStringLiteral("[%1]").arg(label);
        filterParts << QStringLiteral("[%1:a]aresample=%2,atrim=duration=%3,apad=pad_dur=%3[%4]")
            .arg(index).arg(kOutputRate).arg(duration).arg(label);
    }
    filterParts << QStringLiteral("%1amix=inputs=%2:normalize=0,atrim=duration=%3[out]")
        .arg(labels.join(QString())).arg(labels.size()).arg(duration);
    arguments << QStringLiteral("-filter_complex") << filterParts.join(QLatin1Char(';'))
              << QStringLiteral("-map") << QStringLiteral("[out]")
              << QStringLiteral("-ac") << QStringLiteral("1")
              << QStringLiteral("-ar") << QString::number(kOutputRate)
              << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le")
              << QStringLiteral("-f") << QStringLiteral("wav") << outputPath;
    return runFfmpeg(runtime.ffmpeg, arguments, cancel, error);
}

bool renderBoundedVocalTimeline(const MediaRuntimePaths &runtime, const QList<TimedClip> &clips,
                                qint64 durationMs, const QString &outputPath, float dubbedGain,
                                QAtomicInteger<bool> *cancel, QString *error)
{
    if (clips.size() <= kMaximumTimelineInputsPerPass)
        return renderTimelineClipBatch(runtime, clips, durationMs, outputPath, dubbedGain, cancel, error);

    const QFileInfo outputInfo(outputPath);
    QTemporaryDir workspace(outputInfo.dir().filePath(QStringLiteral(".timeline-batches-XXXXXX")));
    if (!workspace.isValid()) {
        if (error) *error = QStringLiteral("Could not create temporary audio-mix workspace.");
        return false;
    }

    QStringList currentInputs;
    int batchNumber = 0;
    for (int offset = 0; offset < clips.size(); offset += kMaximumTimelineInputsPerPass) {
        if (isCancelled(cancel)) {
            if (error) *error = QStringLiteral("Audio mix cancelled.");
            return false;
        }
        QList<TimedClip> batch;
        const int end = qMin(offset + kMaximumTimelineInputsPerPass, clips.size());
        for (int index = offset; index < end; ++index) batch.append(clips.at(index));
        const QString batchPath = workspace.filePath(QStringLiteral("cue-%1.wav")
                                                           .arg(batchNumber++, 4, 10, QLatin1Char('0')));
        if (!renderTimelineClipBatch(runtime, batch, durationMs, batchPath, dubbedGain, cancel, error))
            return false;
        currentInputs.append(batchPath);
    }

    int reductionPass = 0;
    while (currentInputs.size() > 1) {
        QStringList nextInputs;
        for (int offset = 0; offset < currentInputs.size(); offset += kMaximumTimelineInputsPerPass) {
            if (isCancelled(cancel)) {
                if (error) *error = QStringLiteral("Audio mix cancelled.");
                return false;
            }
            const QStringList batch = currentInputs.mid(offset, kMaximumTimelineInputsPerPass);
            const QString batchPath = workspace.filePath(QStringLiteral("reduce-%1-%2.wav")
                                                               .arg(reductionPass, 3, 10, QLatin1Char('0'))
                                                               .arg(nextInputs.size(), 4, 10, QLatin1Char('0')));
            if (!combineTimelineBatches(runtime, batch, durationMs, batchPath, cancel, error))
                return false;
            nextInputs.append(batchPath);
        }
        currentInputs = nextInputs;
        ++reductionPass;
    }

    QFile::remove(outputPath);
    if (!QFile::copy(currentInputs.constFirst(), outputPath)) {
        if (error) *error = QStringLiteral("Could not commit batched audio timeline.");
        return false;
    }
    return true;
}

bool renderStreaming(const QList<TimedClip> &clips, qint64 durationMs,
                     const QString &outputPath, const QString &backgroundPath,
                     const QString &sourceVocalsPath, const QString &vocalOutputPath,
                     float dubbedGain, float originalGain, float backgroundGain,
                     QAtomicInteger<bool> *cancel, QString *error)
{
    const MediaRuntimePaths runtime = MediaRuntimeLocator::resolve();
    if (!runtime.hasFfmpeg()) return false;

    const QString temporaryVocal = vocalOutputPath.isEmpty()
        ? outputPath + QStringLiteral(".voices.tmp.wav") : vocalOutputPath;
    const QString duration = seconds(durationMs);
    if (!renderBoundedVocalTimeline(runtime, clips, durationMs, temporaryVocal, dubbedGain, cancel, error))
        return false;
    if (isCancelled(cancel)) {
        QFile::remove(temporaryVocal);
        if (error) *error = QStringLiteral("Audio mix cancelled.");
        return false;
    }

    const bool hasBackground = !backgroundPath.isEmpty();
    const bool hasSourceVocals = !sourceVocalsPath.isEmpty();
    if (hasBackground && !QFileInfo(backgroundPath).isFile()) {
        if (error) *error = QStringLiteral("Background audio is no longer available: %1").arg(backgroundPath);
        if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
        return false;
    }
    if (hasSourceVocals && !QFileInfo(sourceVocalsPath).isFile()) {
        if (error) *error = QStringLiteral("Original vocals are no longer available: %1").arg(sourceVocalsPath);
        if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
        return false;
    }
    if (!hasBackground && !hasSourceVocals) {
        if (temporaryVocal != outputPath) {
            QFile::remove(outputPath);
            if (!QFile::copy(temporaryVocal, outputPath)) {
                if (error) *error = QStringLiteral("Could not commit the streamed vocal mix.");
                if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
                return false;
            }
        }
        if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
        return true;
    }

    // Build the final program from three independent buses: dubbed speech,
    // separated BGM, and separated original vocals.  The source-voice level
    // must never be reused as the BGM level.
    QStringList mixArguments{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                             QStringLiteral("-y"), QStringLiteral("-i"), temporaryVocal};
    int nextInput = 1;
    int backgroundInput = -1;
    int originalVocalsInput = -1;
    if (hasBackground) {
        backgroundInput = nextInput++;
        mixArguments << QStringLiteral("-i") << backgroundPath;
    }
    if (hasSourceVocals) {
        originalVocalsInput = nextInput++;
        mixArguments << QStringLiteral("-i") << sourceVocalsPath;
    }

    QStringList mixFilterParts;
    QStringList finalLabels;
    if (hasBackground) {
        mixFilterParts << QStringLiteral("[%1:a]aresample=%2,%3,atrim=duration=%4,apad=pad_dur=%4,volume=%5[bg]")
            .arg(backgroundInput).arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix)).arg(duration)
            .arg(QString::number(kBaseBackgroundGain * backgroundGain, 'f', 4));
        mixFilterParts << QStringLiteral("[0:a]aresample=%1,%2,atrim=duration=%3,apad=pad_dur=%3,asplit=2[voice][voice-sidechain]")
            .arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix)).arg(duration);
        // The first sidechaincompress input is the program audio it emits;
        // the second is only the detector. Keep a separate dubbed-voice
        // branch so it is mixed exactly once after ducking the BGM.
        mixFilterParts << QStringLiteral("[bg][voice-sidechain]sidechaincompress=threshold=0.0630957:ratio=4:attack=10:release=120[ducked]");
        finalLabels << QStringLiteral("[ducked]") << QStringLiteral("[voice]");
    } else {
        mixFilterParts << QStringLiteral("[0:a]aresample=%1,%2,atrim=duration=%3,apad=pad_dur=%3[voice]")
            .arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix)).arg(duration);
        finalLabels << QStringLiteral("[voice]");
    }
    if (hasSourceVocals) {
        mixFilterParts << QStringLiteral("[%1:a]aresample=%2,%3,atrim=duration=%4,apad=pad_dur=%4,volume=%5[original-vocals]")
            .arg(originalVocalsInput).arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix)).arg(duration)
            .arg(QString::number(originalGain, 'f', 4));
        finalLabels << QStringLiteral("[original-vocals]");
    }
    const QString outputLabel = finalLabels.size() == 1 ? finalLabels.constFirst()
        : QStringLiteral("[out]");
    if (finalLabels.size() > 1) {
        mixFilterParts << QStringLiteral("%1amix=inputs=%2:normalize=0,atrim=duration=%3[out]")
            .arg(finalLabels.join(QString())).arg(finalLabels.size()).arg(duration);
    }
    mixArguments << QStringLiteral("-filter_complex") << mixFilterParts.join(QLatin1Char(';'))
                 << QStringLiteral("-map") << outputLabel << QStringLiteral("-ac")
                 << QStringLiteral("1") << QStringLiteral("-ar") << QString::number(kOutputRate)
                 << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le") << QStringLiteral("-f")
                 << QStringLiteral("wav") << outputPath;
    const bool mixed = runFfmpeg(runtime.ffmpeg, mixArguments, cancel, error);
    if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
    return mixed;
}

} // namespace

QVector<float> AudioTimelineMixer::resampleToCount(const QVector<float> &source, int targetCount)
{
    if (source.isEmpty() || targetCount <= 0 || source.size() == targetCount) return source;
    QVector<float> result(targetCount);
    const double ratio = static_cast<double>(source.size() - 1) / qMax(1, targetCount - 1);
    for (int i = 0; i < targetCount; ++i) {
        const double sourceIndex = i * ratio;
        const int left = qBound(0, static_cast<int>(sourceIndex), source.size() - 1);
        const int right = qMin(left + 1, source.size() - 1);
        const float fraction = static_cast<float>(sourceIndex - left);
        result[i] = source.at(left) * (1.0f - fraction) + source.at(right) * fraction;
    }
    return result;
}

QString AudioTimelineMixer::vocalStemPath(const QString &outputPath)
{
    if (outputPath.isEmpty()) return QString();
    const QFileInfo info(outputPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral("-vocals.wav"));
}

bool AudioTimelineMixer::mixSegments(const QVariantList &segments, const QString &outputPath, QString *error)
{
    return mixSegments(segments, outputPath, QString(), error, nullptr);
}

bool AudioTimelineMixer::mixSegments(const QVariantList &segments, const QString &outputPath,
                                     const QString &backgroundPath, QString *error,
                                     QAtomicInteger<bool> *cancel)
{
    return mixSegments(segments, outputPath, backgroundPath, QString(), error, cancel, QVariantMap());
}

bool AudioTimelineMixer::mixSegments(const QVariantList &segments, const QString &outputPath,
                                     const QString &backgroundPath, const QString &vocalOutputPath,
                                     QString *error, QAtomicInteger<bool> *cancel,
                                     const QVariantMap &mixConfiguration)
{
    if (isCancelled(cancel)) {
        if (error) *error = QStringLiteral("Audio mix cancelled.");
        return false;
    }
    if (segments.isEmpty()) {
        if (error) *error = QStringLiteral("There are no dubbing segments to render.");
        return false;
    }

    const float dubbedGain = qBound(0.0f,
                                    mixConfiguration.value(QStringLiteral("dubbedGainPercent"), 100).toFloat(),
                                    100.0f) / 100.0f;
    const float originalGain = qBound(0.0f,
                                      mixConfiguration.value(QStringLiteral("originalGainPercent"), 100).toFloat(),
                                      100.0f) / 100.0f;
    const float backgroundGain = qBound(0.0f,
                                        mixConfiguration.value(QStringLiteral("backgroundGainPercent"), 100).toFloat(),
                                        100.0f) / 100.0f;
    const QString sourceVocalsPath = mixConfiguration.value(
        QStringLiteral("sourceVocalsPath")).toString().trimmed();
    const qint64 sourceDurationMs = qMax<qint64>(0,
        mixConfiguration.value(QStringLiteral("sourceDurationMs")).toLongLong());
    QList<TimedClip> clips;
    qint64 lastCueEndMs = 0;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        // Skipping a cue is an explicit editorial decision.  Its old clip
        // may remain on disk for audit/recovery, but must never be rendered
        // into a new preview or export.
        if (segment.value(QStringLiteral("skipped")).toBool()
            || segment.value(QStringLiteral("skip")).toBool()
            || segment.value(QStringLiteral("state")).toString().trimmed().toLower()
                == QStringLiteral("skipped")) {
            continue;
        }
        const QString clipPath = segment.value(QStringLiteral("clipPath")).toString();
        const qint64 startMs = qMax<qint64>(0, segment.value(QStringLiteral("startMs")).toLongLong());
        qint64 endMs = qMax(startMs, segment.value(QStringLiteral("endMs")).toLongLong());
        if (segment.value(QStringLiteral("fullProgramClip")).toBool() && endMs <= startMs) {
            endMs = sourceDurationMs > startMs ? sourceDurationMs : probeDurationMs(clipPath, cancel);
            // Test/developer fallback only. Production uses ffprobe above and
            // never decodes this full file on the GUI thread.
            if (endMs <= startMs && !MediaRuntimeLocator::resolve().hasFfprobe()) {
                QString ignored;
                const WavIO::WavData data = AudioFileDecoder::decode(clipPath, &ignored);
                if (data.sampleRate > 0 && data.channels > 0)
                    endMs = startMs + static_cast<qint64>(data.samples.size() / data.channels)
                        * 1000 / data.sampleRate;
            }
        }
        if (clipPath.isEmpty() || !QFileInfo(clipPath).isFile() || endMs <= startMs) continue;
        clips.append({clipPath, startMs, endMs});
        lastCueEndMs = qMax(lastCueEndMs, endMs);
    }
    const qint64 outputDurationMs = qMax(lastCueEndMs, sourceDurationMs);
    if (clips.isEmpty() || outputDurationMs <= 0 || outputDurationMs > kMaximumDurationMs) {
        if (error) *error = QStringLiteral("No generated clips are available, or the render duration is invalid.");
        return false;
    }

    const MediaRuntimePaths runtime = MediaRuntimeLocator::resolve();
    if (runtime.hasFfmpeg()) {
        return renderStreaming(clips, outputDurationMs, outputPath, backgroundPath, sourceVocalsPath,
                               vocalOutputPath, dubbedGain, originalGain, backgroundGain, cancel, error);
    }

    const qint64 outputSamples = outputDurationMs * kOutputRate / 1000;
    if (outputSamples > kFallbackMaximumSamples) {
        if (error) *error = QStringLiteral(
            "Bundled FFmpeg is required to mix this timeline safely. The development fallback is limited to 10 minutes.");
        return false;
    }
    QVector<float> mix(static_cast<int>(outputSamples), 0.0f);
    for (const TimedClip &clipEntry : clips) {
        if (isCancelled(cancel)) {
            if (error) *error = QStringLiteral("Audio mix cancelled.");
            return false;
        }
        QString clipError;
        const WavIO::WavData clip = AudioFileDecoder::decode(clipEntry.path, &clipError);
        if (clip.samples.isEmpty() || clip.sampleRate <= 0 || clip.channels <= 0) {
            if (error) *error = clipError.isEmpty()
                ? QStringLiteral("Dubbing clip could not be decoded: %1").arg(clipEntry.path)
                : QStringLiteral("Dubbing clip could not be decoded: %1").arg(clipError);
            return false;
        }
        const int frames = clip.samples.size() / clip.channels;
        QVector<float> mono(frames);
        for (int frame = 0; frame < frames; ++frame) {
            float sum = 0.0f;
            for (int channel = 0; channel < clip.channels; ++channel)
                sum += clip.samples.at(frame * clip.channels + channel);
            mono[frame] = sum / static_cast<float>(clip.channels);
        }
        const qint64 desiredSamples = qMin<qint64>(clipEntry.endMs - clipEntry.startMs,
                                                    outputDurationMs - clipEntry.startMs)
                                    * kOutputRate / 1000;
        mono = resampleToCount(mono, static_cast<int>(desiredSamples));
        const qint64 startSample = clipEntry.startMs * kOutputRate / 1000;
        for (int index = 0; index < mono.size() && startSample + index < mix.size(); ++index) {
            mix[static_cast<int>(startSample + index)] = qBound(
                -1.0f, mix.at(static_cast<int>(startSample + index)) + mono.at(index) * dubbedGain, 1.0f);
        }
    }
    if (!vocalOutputPath.isEmpty()
        && !WavIO::saveFloat(vocalOutputPath, mix.constData(), mix.size(), kOutputRate)) {
        if (error) *error = QStringLiteral("Failed to render vocal preview WAV: %1").arg(vocalOutputPath);
        return false;
    }
    if (!backgroundPath.isEmpty()) {
        QString backgroundError;
        const WavIO::WavData background = AudioFileDecoder::decode(backgroundPath, &backgroundError);
        if (background.samples.isEmpty() || background.sampleRate <= 0 || background.channels <= 0) {
            if (error) *error = backgroundError.isEmpty()
                ? QStringLiteral("Background audio could not be decoded for the final mix.")
                : QStringLiteral("Background audio could not be decoded for the final mix: %1").arg(backgroundError);
            return false;
        }
        const int frames = background.samples.size() / background.channels;
        QVector<float> mono(frames);
        for (int frame = 0; frame < frames; ++frame) {
            float sum = 0.0f;
            for (int channel = 0; channel < background.channels; ++channel)
                sum += background.samples.at(frame * background.channels + channel);
            mono[frame] = sum / static_cast<float>(background.channels);
        }
        float currentBackgroundGain = kBaseBackgroundGain * backgroundGain;
        for (int index = 0; index < mix.size(); ++index) {
            if ((index & 0x3fff) == 0 && isCancelled(cancel)) {
                if (error) *error = QStringLiteral("Audio mix cancelled.");
                return false;
            }
            const int source = qMin(mono.size() - 1,
                                    static_cast<int>(static_cast<double>(index) * background.sampleRate / kOutputRate));
            currentBackgroundGain = sidechainBackgroundGain(mix.at(index), currentBackgroundGain,
                                                            kOutputRate, backgroundGain);
            mix[index] = qBound(-1.0f, mix.at(index) + mono.at(source) * currentBackgroundGain, 1.0f);
        }
    }
    if (!sourceVocalsPath.isEmpty()) {
        QString originalVocalsError;
        const WavIO::WavData originalVocals = AudioFileDecoder::decode(sourceVocalsPath,
                                                                         &originalVocalsError);
        if (originalVocals.samples.isEmpty() || originalVocals.sampleRate <= 0
            || originalVocals.channels <= 0) {
            if (error) *error = originalVocalsError.isEmpty()
                ? QStringLiteral("Original vocals could not be decoded for the final mix.")
                : QStringLiteral("Original vocals could not be decoded for the final mix: %1")
                      .arg(originalVocalsError);
            return false;
        }
        const int frames = originalVocals.samples.size() / originalVocals.channels;
        QVector<float> mono(frames);
        for (int frame = 0; frame < frames; ++frame) {
            float sum = 0.0f;
            for (int channel = 0; channel < originalVocals.channels; ++channel)
                sum += originalVocals.samples.at(frame * originalVocals.channels + channel);
            mono[frame] = sum / static_cast<float>(originalVocals.channels);
        }
        for (int index = 0; index < mix.size(); ++index) {
            if ((index & 0x3fff) == 0 && isCancelled(cancel)) {
                if (error) *error = QStringLiteral("Audio mix cancelled.");
                return false;
            }
            const int source = qMin(mono.size() - 1,
                                    static_cast<int>(static_cast<double>(index)
                                                     * originalVocals.sampleRate / kOutputRate));
            mix[index] = qBound(-1.0f, mix.at(index) + mono.at(source) * originalGain, 1.0f);
        }
    }
    if (!WavIO::saveFloat(outputPath, mix.constData(), mix.size(), kOutputRate)) {
        if (error) *error = QStringLiteral("Failed to render preview WAV: %1").arg(outputPath);
        return false;
    }
    return true;
}

} // namespace LAStudio
