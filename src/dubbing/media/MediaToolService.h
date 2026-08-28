#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

namespace LAStudio {

// Small process boundary for media operations. Keeping FFmpeg behind this
// class lets the controller stay testable and avoids shell-string execution.
class MediaToolService final : public QObject
{
    Q_OBJECT
public:
    explicit MediaToolService(QObject *parent = nullptr);
    ~MediaToolService() override;

    QString executablePath() const;
    bool available() const;
    bool busy() const;
    void muxVideoWithAudio(const QString &videoPath,
                           const QString &audioPath,
                           const QString &subtitlePath,
                           const QString &outputPath,
                           bool burnInSubtitles = false,
                           const QString &subtitleFontDirectory = QString());
    void extractVideoThumbnail(const QString &videoPath,
                               const QString &outputPath);
    void cancel();

signals:
    void finished(bool success, const QString &outputPath, const QString &error);
    void progress(int percent);

private slots:
    void onReadyReadStandardError();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onProcessTimeout();

private:
    enum class Operation {
        Mux,
        Thumbnail
    };

    QProcess m_process;
    QTimer m_processTimeout;
    QString m_outputPath;
    QByteArray m_stderr;
    bool m_processTimedOut = false;
    Operation m_operation = Operation::Mux;
};

} // namespace LAStudio
