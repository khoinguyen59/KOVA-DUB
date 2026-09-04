#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

namespace LAStudio {

// Imports one media file into a content-addressed dubbing workspace. The
// service owns probing and audio normalization so controllers never need to
// parse FFmpeg output or pass an unnormalized source to STT.
class MediaIngestService final : public QObject
{
    Q_OBJECT
public:
    explicit MediaIngestService(QObject *parent = nullptr);

    bool available() const;
    void ingest(const QString &path);
    void cancel();

signals:
    void progress(int percent);
    void finished(bool success, const QVariantMap &manifest, const QString &error);

private slots:
    void onHashFinished();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onProcessTimeout();
    void onReadyReadStandardError();
    void onArtifactValidationFinished();

private:
    struct HashResult {
        quint64 requestId = 0;
        bool success = false;
        QString hash;
        QString error;
    };

    enum class Stage {
        None, Probe, LoudnessMeasurement, Master, MasterValidation,
        Analysis, AnalysisValidation, CacheValidation
    };
    void fail(const QString &error);
    void startProbe();
    void startLoudnessMeasurement();
    void startMaster();
    void startAnalysis();
    void startArtifactValidation(const QString &path, Stage validationStage);
    void startCacheValidation();
    void finishCached();
    void finishAnalysis();
    bool parseLoudnessMeasurement(QString *error);
    QString ffmpegPath() const;
    QString ffprobePath() const;

    QProcess m_process;
    QTimer m_processTimeout;
    QString m_inputPath;
    QString m_hash;
    QString m_workspace;
    QString m_masterPath;
    QString m_analysisPath;
    QString m_cacheMasterPath;
    QString m_cacheAnalysisPath;
    QString m_masterStagingPath;
    QString m_analysisStagingPath;
    QByteArray m_stderr;
    QByteArray m_probeOutput;
    QVariantMap m_loudnessMeasurements;
    QVariantMap m_manifest;
    QFutureWatcher<HashResult> m_hashWatcher;
    QFutureWatcher<bool> m_artifactWatcher;
    quint64 m_nextHashRequestId = 0;
    quint64 m_activeHashRequestId = 0;
    Stage m_stage = Stage::None;
    bool m_terminal = true;
};

} // namespace LAStudio
