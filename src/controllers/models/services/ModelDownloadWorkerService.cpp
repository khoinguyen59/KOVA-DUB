#include "ModelDownloadWorkerService.h"

#include <QDir>
#include <QFile>

namespace LAStudio {

ModelDownloadWorkerService::ModelDownloadWorkerService(QObject *parent)
    : QObject(parent)
{
}

void ModelDownloadWorkerService::startDownload(const QString &modelId, const QUrl &url, const QString &destPath)
{
    cancelDownload();

    m_modelId = modelId;
    m_destPath = destPath;
    m_downloading = true;
    m_progress = 0;

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_netManager.get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_progress = static_cast<int>((received * 100) / total);
            emit progressChanged(m_progress, received, total);
        }
    });

    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        if (!m_currentReply) return;

        if (m_currentReply->error() == QNetworkReply::NoError) {
            QFile file(m_destPath);
            QDir().mkpath(QFileInfo(m_destPath).dir().absolutePath());
            if (file.open(QIODevice::WriteOnly)) {
                file.write(m_currentReply->readAll());
                file.close();
                m_downloading = false;
                m_progress = 100;
                emit downloadCompleted(m_modelId, m_destPath);
            } else {
                m_downloading = false;
                emit downloadFailed(m_modelId, tr("Failed to write downloaded file."));
            }
        } else {
            const QString err = m_currentReply->errorString();
            m_downloading = false;
            emit downloadFailed(m_modelId, err);
        }

        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    });
}

void ModelDownloadWorkerService::cancelDownload()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    m_downloading = false;
    m_progress = 0;
}

} // namespace LAStudio
