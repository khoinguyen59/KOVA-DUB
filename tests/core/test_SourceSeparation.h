#pragma once
#include <QObject>
#include <QTemporaryDir>

namespace LAStudio {

class TestSourceSeparation : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testBackendFactory();
    void testWavIoRejectsMalformedChunks();
    void testSharedAudioDecoderNormalizesReferenceAudio();
    void testServiceReentryBusy();
    void testServiceStartReturnsBeforeInferenceCompletes();
    void testCancellation();
    void testDestroyServiceRunning();
    void testDestroyServiceDoesNotBlockOnUninterruptibleWorker();
    void testSourceSeparationThreadPolicyProtectsUiCapacity();

private:
    QTemporaryDir m_tempDir;
    QString m_testWavPath;
};

} // namespace LAStudio
