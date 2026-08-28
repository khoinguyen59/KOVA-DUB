#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QAtomicInteger>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

namespace LAStudio {

class MediaToolService;

class DubbingExportJob final : public QObject
{
    Q_OBJECT
public:
    explicit DubbingExportJob(QObject *parent = nullptr);
    ~DubbingExportJob() override;

    bool running() const { return m_running; }
    bool renderPreview(const QVariantList &segments, const QString &projectPath,
                       const QString &backgroundPath, const QString &path = QString(),
                       const QVariantMap &mixConfiguration = QVariantMap());
    bool startExport(const QString &sourceMediaPath, const QString &audioPath,
                     const QString &outputPath, const QVariantList &segments = {},
                     const QVariantMap &subtitleConfiguration = QVariantMap());
    void cancel();

signals:
    void progressChanged(const QString &stage, int progress);
    void previewReady(const QString &path);
    void exported(const QString &path);
    void failed(const QString &message);

private slots:
    void onRenderFinished();
    void onMediaFinished(bool success, const QString &outputPath, const QString &error);
    void onValidationReadyReadStandardOutput();
    void onValidationReadyReadStandardError();
    void onValidationFinished(int exitCode, QProcess::ExitStatus status);
    void onValidationError(QProcess::ProcessError error);
    void onValidationTimeout();

private:
    enum class ValidationStage { None, Source, Export };

    void startMediaValidation(const QString &path, ValidationStage stage);
    bool validateProbeResult(const QByteArray &payload, ValidationStage stage,
                             QString *errorMessage);
    void fail(const QString &message);
    void clearExportPaths();

    bool m_running = false;
    QString m_renderStagingPath;
    QString m_renderVocalStagingPath;
    QString m_exportDestination;
    QString m_exportStagingPath;
    QString m_exportAudioPath;
    QString m_exportSubtitlePath;
    QString m_exportSubtitleFontDirectory;
    QString m_sourceMediaPath;
    bool m_exportBurnIn = false;
    bool m_expectSubtitle = false;
    bool m_sourceHasVideo = false;
    qint64 m_sourceDurationMs = 0;
    QFutureWatcher<QVariantMap> *m_renderWatcher = nullptr;
    std::shared_ptr<QAtomicInteger<bool>> m_renderCancel;
    MediaToolService *m_mediaTools = nullptr;
    QProcess m_validationProcess;
    QTimer m_validationTimeout;
    QByteArray m_validationOutput;
    QByteArray m_validationError;
    ValidationStage m_validationStage = ValidationStage::None;
};

} // namespace LAStudio
