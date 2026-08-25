#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

class DubbingMediaQueueManager : public QObject
{
    Q_OBJECT

public:
    explicit DubbingMediaQueueManager(QObject *parent = nullptr);
    ~DubbingMediaQueueManager() override = default;

    QVariantList mediaQueueItems() const { return m_items; }
    bool mediaQueueDownloading() const { return m_downloading; }
    bool mediaQueueProcessing() const { return m_processing; }
    QString mediaQueueStatus() const { return m_status; }
    int mediaQueueProgress() const { return m_progress; }
    bool mediaDownloadCookieFileConfigured() const { return !m_cookieFile.isEmpty(); }

    void addMediaQueueItem(const QString &sourceUrlOrPath);
    bool removeMediaQueueItem(int index);
    void clearMediaQueue();
    void setCookieFile(const QString &path);

    void startDownload();
    void cancelDownload();

signals:
    void mediaQueueChanged();
    void itemReadyForImport(const QString &localPath);

private:
    QVariantList m_items;
    bool m_downloading{false};
    bool m_processing{false};
    QString m_status;
    int m_progress{0};
    QString m_cookieFile;
};

} // namespace LAStudio
