#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace LAStudio {

class ModelDownloadWorkerService : public QObject
{
    Q_OBJECT

public:
    explicit ModelDownloadWorkerService(QObject *parent = nullptr);
    ~ModelDownloadWorkerService() override = default;

    bool isDownloading() const { return m_downloading; }
    int progressPercent() const { return m_progress; }
    QString currentModelId() const { return m_modelId; }

    void startDownload(const QString &modelId, const QUrl &url, const QString &destPath);
    void cancelDownload();

signals:
    void progressChanged(int percent, qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(const QString &modelId, const QString &filePath);
    void downloadFailed(const QString &modelId, const QString &errorMessage);

private:
    QNetworkAccessManager m_netManager;
    QNetworkReply *m_currentReply{nullptr};
    QString m_modelId;
    QString m_destPath;
    bool m_downloading{false};
    int m_progress{0};
};

} // namespace LAStudio
