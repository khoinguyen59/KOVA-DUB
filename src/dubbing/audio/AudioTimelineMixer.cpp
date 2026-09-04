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

bool renderStreaming(const QList<TimedClip> &clips, qint64 durationMs,
                     const QString &outputPath, const QString &backgroundPath,
                     const QString &vocalOutputPath, float dubbedGain, float originalGain,
                     QAtomicInteger<bool> *cancel, QString *error)
{
    const MediaRuntimePaths runtime = MediaRuntimeLocator::resolve();
    if (!runtime.hasFfmpeg()) return false;

    const QString temporaryVocal = vocalOutputPath.isEmpty()
        ? outputPath + QStringLiteral(".voices.tmp.wav") : vocalOutputPath;
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

    QStringList vocalArguments{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                               QStringLiteral("-y")};
    vocalArguments << inputs << QStringLiteral("-filter_complex") << filterParts.join(QLatin1Char(';'))
                   << QStringLiteral("-map") << QStringLiteral("[voices]")
                   << QStringLiteral("-ac") << QStringLiteral("1")
                   << QStringLiteral("-ar") << QString::number(kOutputRate)
                   << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le")
                   << QStringLiteral("-f") << QStringLiteral("wav") << temporaryVocal;
    if (!runFfmpeg(runtime.ffmpeg, vocalArguments, cancel, error)) return false;
    if (isCancelled(cancel)) {
        QFile::remove(temporaryVocal);
        if (error) *error = QStringLiteral("Audio mix cancelled.");
        return false;
    }

    if (backgroundPath.isEmpty()) {
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
    if (!QFileInfo(backgroundPath).isFile()) {
        if (error) *error = QStringLiteral("Background audio is no longer available: %1").arg(backgroundPath);
        if (vocalOutputPath.isEmpty()) QFile::remove(temporaryVocal);
        return false;
    }

    const QString filter = QStringLiteral(
        "[0:a]aresample=%1,%2,atrim=duration=%3,apad=pad_dur=%3,volume=%4[bg];"
        "[1:a]aresample=%1,%2,atrim=duration=%3,apad=pad_dur=%3,asplit=2[voice][voice-sidechain];"
        // The first sidechaincompress input is the program audio it emits;
        // the second is only the detector.  Keep Background first, feed voice
        // as the detector, and retain a separate voice branch for final mix.
        // Reusing one labelled voice stream without asplit silently produced
        // a doubled voice bed.
        "[bg][voice-sidechain]sidechaincompress=threshold=0.0630957:ratio=4:attack=10:release=120[ducked];"
        "[ducked][voice]amix=inputs=2:normalize=0,atrim=duration=%3[out]")
        .arg(kOutputRate).arg(QLatin1String(kExplicitMonoDownmix)).arg(duration)
        .arg(QString::number(kBaseBackgroundGain * originalGain, 'f', 4));
    const QStringList mixArguments{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                                    QStringLiteral("-y"), QStringLiteral("-i"), backgroundPath,
                                    QStringLiteral("-i"), temporaryVocal, QStringLiteral("-filter_complex"), filter,
                                    QStringLiteral("-map"), QStringLiteral("[out]"), QStringLiteral("-ac"),
                                    QStringLiteral("1"), QStringLiteral("-ar"), QString::number(kOutputRate),
                                    QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"), QStringLiteral("-f"),
                                    QStringLiteral("wav"), outputPath};
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
    const qint64 sourceDurationMs = qMax<qint64>(0,
        mixConfiguration.value(QStringLiteral("sourceDurationMs")).toLongLong());
    QList<TimedClip> clips;
    qint64 lastCueEndMs = 0;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
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
        return renderStreaming(clips, outputDurationMs, outputPath, backgroundPath, vocalOutputPath,
                               dubbedGain, originalGain, cancel, error);
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
        float backgroundGain = kBaseBackgroundGain * originalGain;
        for (int index = 0; index < mix.size(); ++index) {
            if ((index & 0x3fff) == 0 && isCancelled(cancel)) {
                if (error) *error = QStringLiteral("Audio mix cancelled.");
                return false;
            }
            const int source = qMin(mono.size() - 1,
                                    static_cast<int>(static_cast<double>(index) * background.sampleRate / kOutputRate));
            backgroundGain = sidechainBackgroundGain(mix.at(index), backgroundGain, kOutputRate, originalGain);
            mix[index] = qBound(-1.0f, mix.at(index) + mono.at(source) * backgroundGain, 1.0f);
        }
    }
    if (!WavIO::saveFloat(outputPath, mix.constData(), mix.size(), kOutputRate)) {
        if (error) *error = QStringLiteral("Failed to render preview WAV: %1").arg(outputPath);
        return false;
    }
    return true;
}

} // namespace LAStudio
