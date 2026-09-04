#include "test_AudioPreviewService.h"
#include <QtTest>
#include <QSignalSpy>
#include <QProcess>
#include <QThreadPool>

#include "controllers/shared/AudioPreviewService.h"
#include "tts/engine/TtsEngine.h"
#include "audio/player/AudioPlayer.h"
#include "audio/io/WaveformProvider.h"
#include "audio/io/WavIO.h"
#include "core/services/MediaRuntimeLocator.h"

namespace LAStudio {

void TestAudioPreviewService::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestAudioPreviewService::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestAudioPreviewService::testAudioPreviewService()
{
    qDebug() << "--- START: testAudioPreviewService ---";
    TtsEngine tts;
    AudioPlayer player;
    WaveformProvider provider;

    AudioPreviewService service(&tts, &player, &provider);

    QSignalSpy spyError(&service, &AudioPreviewService::errorOccurred);

    // Save with no samples should trigger error
    tts.clearLastSamples();
    service.saveWav(m_tempDir.filePath(QStringLiteral("output.wav")));
    
    // Wait for async worker or check synchronous validation
    if (spyError.isEmpty()) {
        spyError.wait(1000);
    }
    QVERIFY(spyError.size() > 0);
}

void TestAudioPreviewService::decodesFlacWaveformOffUiThread()
{
    const QString wavPath = m_tempDir.filePath(QStringLiteral("preview-input.wav"));
    const QString flacPath = m_tempDir.filePath(QStringLiteral("preview-input.flac"));
    QVector<float> samples(16000, 0.02F);
    QVERIFY(WavIO::saveFloat(wavPath, samples.constData(), samples.size(), 16000));

    const QString ffmpeg = MediaRuntimeLocator::resolve().ffmpeg;
    QVERIFY2(!ffmpeg.isEmpty(), "The bundled FFmpeg runtime is required for the FLAC preview fixture.");
    QProcess encoder;
    encoder.setProgram(ffmpeg);
    encoder.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                          QStringLiteral("-nostdin"), QStringLiteral("-y"), QStringLiteral("-i"), wavPath,
                          QStringLiteral("-c:a"), QStringLiteral("flac"), flacPath});
    encoder.start();
    QVERIFY(encoder.waitForFinished(10000));
    QCOMPARE(encoder.exitStatus(), QProcess::NormalExit);
    QCOMPARE(encoder.exitCode(), 0);
    QVERIFY(QFileInfo(flacPath).isFile());

    TtsEngine tts;
    AudioPlayer player;
    WaveformProvider provider;
    AudioPreviewService service(&tts, &player, &provider);
    service.requestWavSamples(flacPath);
    QTRY_VERIFY_WITH_TIMEOUT(!service.wavSamplesLoading(), 10000);
    QVERIFY(!service.wavSamples().isEmpty());
    QCOMPARE(service.wavSamplesSourcePath(), flacPath);
}

} // namespace LAStudio
