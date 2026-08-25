#include "DubbingMediaQueueManager.h"

#include <QDateTime>
#include <QFileInfo>

namespace LAStudio {

DubbingMediaQueueManager::DubbingMediaQueueManager(QObject *parent)
    : QObject(parent)
{
}

void DubbingMediaQueueManager::addMediaQueueItem(const QString &sourceUrlOrPath)
{
    if (sourceUrlOrPath.isEmpty()) return;

    QVariantMap item;
    item["id"] = QString::number(QDateTime::currentMSecsSinceEpoch());
    item["source"] = sourceUrlOrPath;
    item["title"] = QFileInfo(sourceUrlOrPath).fileName();
    item["status"] = "pending";
    item["progress"] = 0;

    m_items.append(item);
    emit mediaQueueChanged();
}

bool DubbingMediaQueueManager::removeMediaQueueItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return false;

    m_items.removeAt(index);
    emit mediaQueueChanged();
    return true;
}

void DubbingMediaQueueManager::clearMediaQueue()
{
    m_items.clear();
    emit mediaQueueChanged();
}

void DubbingMediaQueueManager::setCookieFile(const QString &path)
{
    m_cookieFile = path;
    emit mediaQueueChanged();
}

void DubbingMediaQueueManager::startDownload()
{
    m_downloading = true;
    m_status = tr("Downloading queued media...");
    emit mediaQueueChanged();
}

void DubbingMediaQueueManager::cancelDownload()
{
    m_downloading = false;
    m_status = tr("Download cancelled.");
    emit mediaQueueChanged();
}

} // namespace LAStudio
