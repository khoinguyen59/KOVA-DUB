#include "test_SourceSeparation.h"
#include "separation/io/SeparationTypes.h"
#include "separation/engine/SourceSeparationService.h"
#include "separation/io/SeparationAudioIO.h"
#include "audio/io/AudioFileDecoder.h"
#include "audio/io/WavIO.h"
#include "core/hardware/InferenceThreadPolicy.h"

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <memory>
#include <atomic>

namespace LAStudio {

class FakeTestBackend : public SeparationBackend {
public:
    FakeTestBackend(const QString &id,
                    std::shared_ptr<std::atomic<int>> callCounter,
                    int delayMs = 100,
                    std::shared_ptr<std::atomic_bool> entered = nullptr,
                    std::shared_ptr<std::atomic_bool> release = nullptr,
                    std::shared_ptr<std::atomic_bool> completed = nullptr,
                    std::shared_ptr<std::atomic_bool> observedCancellation = nullptr)
        : m_id(id), m_callCounter(callCounter), m_delayMs(delayMs),
          m_entered(std::move(entered)), m_release(std::move(release)),
          m_completed(std::move(completed)),
          m_observedCancellation(std::move(observedCancellation)) {}

    QString id() const override { return m_id; }

    BackendResult separate(
        const DecodedAudio &audio,
        const SeparationConfiguration &configuration,
        int numThreads,
        const CancellationToken &cancellation,
        ProgressCallback progress) override
    {
        Q_UNUSED(configuration);
        Q_UNUSED(numThreads);
        
        if (m_callCounter) {
            (*m_callCounter)++;
        }
        if (m_entered) {
            m_entered->store(true, std::memory_order_release);
        }

        BackendResult res;
        res.success = false;

        if (cancellation.isCancelled()) {
            res.error = QStringLiteral("Cancelled");
            return res;
        }

        if (progress) progress(50, QStringLiteral("Fake separating"));

        if (m_release) {
            // Simulate a native inference call that does not observe the
            // cancellation flag until it returns. The service destructor must
            // not wait forever for this path.
            while (!m_release->load(std::memory_order_acquire)) {
                QThread::msleep(5);
            }
            if (cancellation.isCancelled()) {
                if (m_observedCancellation) {
                    m_observedCancellation->store(true, std::memory_order_release);
                }
                if (m_completed) {
                    m_completed->store(true, std::memory_order_release);
                }
                res.error = QStringLiteral("Cancelled");
                return res;
            }
        } else {
            for (int elapsed = 0; elapsed < m_delayMs; elapsed += 2) {
                if (cancellation.isCancelled()) {
                    res.error = QStringLiteral("Cancelled");
                    return res;
                }
                QThread::msleep(2);
            }

            if (cancellation.isCancelled()) {
                res.error = QStringLiteral("Cancelled");
                return res;
            }
        }

        res.success = true;
        res.sampleRate = audio.sampleRate;

        BackendStem vocals;
        vocals.id = QStringLiteral("vocals");
        vocals.channels = audio.channels;
        res.stems.append(vocals);

        BackendStem bg;
        bg.id = QStringLiteral("background");
        bg.channels = audio.channels;
        res.stems.append(bg);

        if (m_completed) {
            m_completed->store(true, std::memory_order_release);
        }

        return res;
    }

private:
    QString m_id;
    std::shared_ptr<std::atomic<int>> m_callCounter;
    int m_delayMs = 100;
    std::shared_ptr<std::atomic_bool> m_entered;
    std::shared_ptr<std::atomic_bool> m_release;
    std::shared_ptr<std::atomic_bool> m_completed;
    std::shared_ptr<std::atomic_bool> m_observedCancellation;
};

void TestSourceSeparation::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    m_testWavPath = m_tempDir.filePath(QStringLiteral("test.wav"));
    
    // Create 1 second of stereo silent audio
    QVector<float> samples(48000 * 2, 0.0f);
    QVERIFY(WavIO::saveFloat(m_testWavPath, samples.constData(), samples.size(), 48000, 2));
}

void TestSourceSeparation::cleanupTestCase()
{
}

void TestSourceSeparation::testBackendFactory()
{
    SeparationBackendFactory factory;
    QVERIFY(factory.hasBackend(QStringLiteral("sherpa-onnx")));

    // Register duplicate should fail
    QVERIFY(!factory.registerBackend(QStringLiteral("sherpa-onnx"), []() { return nullptr; }));

    // Register a new one
    QVERIFY(factory.registerBackend(QStringLiteral("test-backend"), []() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("test-backend"), nullptr);
    }));
    QVERIFY(factory.hasBackend(QStringLiteral("test-backend")));

    auto testBackend = factory.createBackend(QStringLiteral("test-backend"));
    QVERIFY(testBackend != nullptr);
    QCOMPARE(testBackend->id(), QStringLiteral("test-backend"));

    // Querying unknown backend returns nullptr
    QVERIFY(factory.createBackend(QStringLiteral("unknown")) == nullptr);
}

void TestSourceSeparation::testWavIoRejectsMalformedChunks()
{
    auto writeFixture = [this](const QString &name, const QByteArray &bytes) -> QString {
        const QString path = m_tempDir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return {};
        return path;
    };

    QByteArray truncatedFmt("RIFF\0\0\0\0WAVEfmt ", 16);
    truncatedFmt.append(QByteArray::fromHex("10000000"));
    const QString truncatedPath = writeFixture(QStringLiteral("truncated-fmt.wav"), truncatedFmt);
    QVERIFY(!truncatedPath.isEmpty());
    QVERIFY(WavIO::loadAsFloat(truncatedPath).samples.isEmpty());

    QByteArray valid;
    const QString validPath = m_tempDir.filePath(QStringLiteral("valid-for-mutation.wav"));
    const QVector<float> sample{0.0f};
    QVERIFY(WavIO::saveFloat(validPath, sample.constData(), sample.size(), 16000));
    QFile validFile(validPath);
    QVERIFY(validFile.open(QIODevice::ReadOnly));
    valid = validFile.readAll();

    QByteArray zeroBits = valid;
    zeroBits[34] = 0;
    zeroBits[35] = 0;
    const QString zeroBitsPath = writeFixture(QStringLiteral("zero-bps.wav"), zeroBits);
    QVERIFY(!zeroBitsPath.isEmpty());
    QVERIFY(WavIO::loadAsFloat(zeroBitsPath).samples.isEmpty());

    QByteArray oversizedData = valid;
    oversizedData[40] = char(0xff);
    oversizedData[41] = char(0xff);
    oversizedData[42] = char(0xff);
    oversizedData[43] = char(0x7f);
    const QString oversizedPath = writeFixture(QStringLiteral("oversized-data.wav"), oversizedData);
    QVERIFY(!oversizedPath.isEmpty());
    QVERIFY(WavIO::loadAsFloat(oversizedPath).samples.isEmpty());
}

void TestSourceSeparation::testSharedAudioDecoderNormalizesReferenceAudio()
{
    QString error;
    const WavIO::WavData audio = AudioFileDecoder::decodeMono(m_testWavPath, 24000, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(audio.channels, 1);
    QCOMPARE(audio.sampleRate, 24000);
    QCOMPARE(audio.samples.size(), 24000);

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return;

    const QString mp3Path = m_tempDir.filePath(QStringLiteral("test.mp3"));
    QProcess encoder;
    encoder.start(ffmpeg, {QStringLiteral("-hide_banner"),
                           QStringLiteral("-loglevel"), QStringLiteral("error"),
                           QStringLiteral("-y"),
                           QStringLiteral("-i"), m_testWavPath,
                           mp3Path});
    QVERIFY(encoder.waitForStarted(5000));
    QVERIFY(encoder.waitForFinished(30000));
    QCOMPARE(encoder.exitCode(), 0);

    error.clear();
    const WavIO::WavData compressed = AudioFileDecoder::decodeMono(mp3Path, 24000, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(compressed.channels, 1);
    QCOMPARE(compressed.sampleRate, 24000);
    QVERIFY(!compressed.samples.isEmpty());
}

void TestSourceSeparation::testServiceReentryBusy()
{
    auto factory = std::make_shared<SeparationBackendFactory>();
    auto callCounter = std::make_shared<std::atomic<int>>(0);

    factory->registerBackend(QStringLiteral("fake-busy"), [callCounter]() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("fake-busy"), callCounter);
    });

    SourceSeparationService service(factory);

    SeparationConfiguration config;
    config.backendId = QStringLiteral("fake-busy");
    config.pipelineProfile = QStringLiteral("uvr-2stems");
    config.runtimeId = QStringLiteral("fake-runtime");
    config.runtimePath = QStringLiteral("dummy_path");
    
    SeparationRequest req;
    req.sourcePath = m_testWavPath;
    req.outputRoot = m_tempDir.path();
    req.configuration = config;

    QSignalSpy finishedSpy(&service, &SourceSeparationService::finished);

    // First request should succeed starting
    QVERIFY(service.isolate(req));
    QVERIFY(service.processing());

    // Second request should fail with Busy
    QString isolateError;
    QVERIFY(!service.isolate(req, &isolateError));
    QCOMPARE(isolateError, QStringLiteral("Busy"));

    // Wait for the first request to finish
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(finishedSpy.count(), 1);
    
    SeparationResult res = finishedSpy.takeFirst().at(0).value<SeparationResult>();
    QVERIFY(res.success);
    QCOMPARE(res.errorCode, SeparationErrorCode::None);
    QCOMPARE(callCounter->load(), 1);
}

void TestSourceSeparation::testServiceStartReturnsBeforeInferenceCompletes()
{
    auto factory = std::make_shared<SeparationBackendFactory>();
    auto entered = std::make_shared<std::atomic_bool>(false);
    factory->registerBackend(QStringLiteral("fake-slow-start"), [entered]() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("fake-slow-start"),
                                                  nullptr, 600, entered);
    });

    SourceSeparationService service(factory);
    SeparationConfiguration config;
    config.backendId = QStringLiteral("fake-slow-start");
    config.pipelineProfile = QStringLiteral("uvr-2stems");
    config.runtimeId = QStringLiteral("fake-runtime");
    config.runtimePath = QStringLiteral("dummy_path");

    SeparationRequest request;
    request.sourcePath = m_testWavPath;
    request.outputRoot = m_tempDir.path();
    request.configuration = config;

    QSignalSpy finishedSpy(&service, &SourceSeparationService::finished);
    QElapsedTimer timer;
    timer.start();
    QVERIFY(service.isolate(request));
    QVERIFY2(timer.elapsed() < 150,
             "Starting source separation waited for native inference on the caller thread");
    QTRY_VERIFY_WITH_TIMEOUT(entered->load(std::memory_order_acquire), 2000);
    // The fake backend may finish while QTRY_VERIFY is pumping the event loop.
    // Do not call wait() after the signal has already been delivered: QtTest's
    // wait() only observes a signal emitted after it starts waiting. The
    // completion bound includes decoder setup and atomic stem writes, so it is
    // intentionally separate from the <150 ms caller-thread assertion above.
    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() > 0, 8000);
}

void TestSourceSeparation::testCancellation()
{
    auto factory = std::make_shared<SeparationBackendFactory>();
    auto callCounter = std::make_shared<std::atomic<int>>(0);

    factory->registerBackend(QStringLiteral("fake-cancel"), [callCounter]() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("fake-cancel"), callCounter);
    });

    SourceSeparationService service(factory);

    SeparationConfiguration config;
    config.backendId = QStringLiteral("fake-cancel");
    config.pipelineProfile = QStringLiteral("uvr-2stems");
    config.runtimeId = QStringLiteral("fake-runtime");
    config.runtimePath = QStringLiteral("dummy_path");

    SeparationRequest req;
    req.sourcePath = m_testWavPath;
    req.outputRoot = m_tempDir.path();
    req.configuration = config;

    QSignalSpy finishedSpy(&service, &SourceSeparationService::finished);

    QVERIFY(service.isolate(req));
    QVERIFY(service.processing());

    // Immediately cancel
    service.cancel();

    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(finishedSpy.count(), 1);

    SeparationResult res = finishedSpy.takeFirst().at(0).value<SeparationResult>();
    QVERIFY(!res.success);
    QCOMPARE(res.errorCode, SeparationErrorCode::Cancelled);
}

void TestSourceSeparation::testDestroyServiceRunning()
{
    auto factory = std::make_shared<SeparationBackendFactory>();
    auto callCounter = std::make_shared<std::atomic<int>>(0);

    factory->registerBackend(QStringLiteral("fake-destroy"), [callCounter]() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("fake-destroy"), callCounter);
    });

    SeparationConfiguration config;
    config.backendId = QStringLiteral("fake-destroy");
    config.pipelineProfile = QStringLiteral("uvr-2stems");
    config.runtimeId = QStringLiteral("fake-runtime");
    config.runtimePath = QStringLiteral("dummy_path");

    SeparationRequest req;
    req.sourcePath = m_testWavPath;
    req.outputRoot = m_tempDir.path();
    req.configuration = config;

    {
        SourceSeparationService service(factory);
        QVERIFY(service.isolate(req));
        QVERIFY(service.processing());
        // Destructor should safely stop the worker thread and clean up without crash
    }
}

void TestSourceSeparation::testDestroyServiceDoesNotBlockOnUninterruptibleWorker()
{
    auto factory = std::make_shared<SeparationBackendFactory>();
    auto entered = std::make_shared<std::atomic_bool>(false);
    auto release = std::make_shared<std::atomic_bool>(false);
    auto completed = std::make_shared<std::atomic_bool>(false);
    auto observedCancellation = std::make_shared<std::atomic_bool>(false);

    factory->registerBackend(QStringLiteral("fake-uninterruptible"),
                             [entered, release, completed, observedCancellation]() {
        return std::make_unique<FakeTestBackend>(QStringLiteral("fake-uninterruptible"),
                                                  nullptr, 0, entered, release, completed,
                                                  observedCancellation);
    });

    SeparationConfiguration config;
    config.backendId = QStringLiteral("fake-uninterruptible");
    config.pipelineProfile = QStringLiteral("uvr-2stems");
    config.runtimeId = QStringLiteral("fake-runtime");
    config.runtimePath = QStringLiteral("dummy_path");

    SeparationRequest req;
    req.sourcePath = m_testWavPath;
    req.outputRoot = m_tempDir.path();
    req.configuration = config;

    auto *service = new SourceSeparationService(factory);
    QVERIFY(service->isolate(req));
    QTRY_VERIFY_WITH_TIMEOUT(entered->load(std::memory_order_acquire), 1000);

    QElapsedTimer destructionTimer;
    destructionTimer.start();
    delete service;
    QVERIFY2(destructionTimer.elapsed() < 1500,
             qPrintable(QStringLiteral("service destruction took %1 ms")
                            .arg(destructionTimer.elapsed())));

    release->store(true, std::memory_order_release);
    QTRY_VERIFY_WITH_TIMEOUT(observedCancellation->load(std::memory_order_acquire), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(completed->load(std::memory_order_acquire), 3000);
}

void TestSourceSeparation::testSourceSeparationThreadPolicyProtectsUiCapacity()
{
    const int automatic = InferenceThreadPolicy::recommendedThreadCount(
        InferenceBackendProfile::SourceSeparationCpu);
    const int explicitMaximum = InferenceThreadPolicy::recommendedThreadCount(
        InferenceBackendProfile::SourceSeparationCpu, 64);

    QVERIFY(automatic >= 1);
    QVERIFY(automatic <= 3);
    QCOMPARE(explicitMaximum, 3);
}

} // namespace LAStudio
