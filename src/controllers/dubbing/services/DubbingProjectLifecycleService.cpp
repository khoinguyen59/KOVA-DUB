#include "DubbingProjectLifecycleService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>

#include "core/utils/Logger.h"
#include "core/storage/PathUtils.h"

namespace LAStudio {

DubbingProjectLifecycleService::DubbingProjectLifecycleService(QObject *parent)
    : QObject(parent)
{
    loadHistory();
}

QString DubbingProjectLifecycleService::defaultProjectsDirectory()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString primaryDir = QDir(appDir).filePath(QStringLiteral("projects"));
    if (QDir().mkpath(primaryDir)) {
        return primaryDir;
    }
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fallbackDir = QDir(appData).filePath(QStringLiteral("projects"));
    QDir().mkpath(fallbackDir);
    return fallbackDir;
}

bool DubbingProjectLifecycleService::createAutoProject(const QString &customName)
{
    const QString projDir = defaultProjectsDirectory();
    QString fileName = customName.trimmed();
    if (fileName.isEmpty()) {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
        fileName = QStringLiteral("Project_%1.ladub.json").arg(timestamp);
    } else {
        if (!fileName.endsWith(QStringLiteral(".ladub.json"), Qt::CaseInsensitive) &&
            !fileName.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            fileName += QStringLiteral(".ladub.json");
        }
    }
    const QString fullPath = QDir(projDir).filePath(fileName);
    return newProject(fullPath);
}

QUrl DubbingProjectLifecycleService::sourceMediaUrl() const
{
    const QString path = sourceMediaPath();
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

QUrl DubbingProjectLifecycleService::playbackMediaUrl() const
{
    const QString preview = previewPath();
    if (!preview.isEmpty() && QFile::exists(preview))
        return QUrl::fromLocalFile(preview);
    return sourceMediaUrl();
}

void DubbingProjectLifecycleService::setSourceLanguage(const QString &lang)
{
    if (m_project.sourceLanguage != lang) {
        m_project.sourceLanguage = lang;
        emit projectChanged();
    }
}

void DubbingProjectLifecycleService::setTargetLanguage(const QString &lang)
{
    if (m_project.targetLanguage != lang) {
        m_project.targetLanguage = lang;
        emit projectChanged();
    }
}

void DubbingProjectLifecycleService::setDurationControl(const QVariantMap &ctrl)
{
    m_project.durationControl = ctrl;
    emit projectChanged();
}

QString DubbingProjectLifecycleService::historyFilePath() const
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return QDir(appData).filePath("dubbing_history.json");
}

void DubbingProjectLifecycleService::loadHistory()
{
    m_history.clear();
    QFile file(historyFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &v : arr) {
                if (v.isObject()) {
                    m_history.append(v.toObject().toVariantMap());
                }
            }
        }
    }

    // Also scan default projects directory for any project files
    const QString projDir = defaultProjectsDirectory();
    QDir dir(projDir);
    const QStringList filters = {QStringLiteral("*.ladub.json"), QStringLiteral("*.json")};
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    for (const QFileInfo &fi : files) {
        const QString absPath = fi.absoluteFilePath();
        bool exists = false;
        for (const QVariant &item : m_history) {
            if (item.toMap().value(QStringLiteral("path")).toString() == absPath ||
                item.toMap().value(QStringLiteral("id")).toString() == absPath) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            QVariantMap entry;
            entry["id"] = absPath;
            entry["path"] = absPath;
            entry["mediaPath"] = QString();
            entry["fileName"] = fi.fileName();
            entry["lastOpened"] = fi.lastModified().toString(Qt::ISODate);
            m_history.append(entry);
        }
    }
    emit historyChanged();
}

void DubbingProjectLifecycleService::saveHistory()
{
    QJsonArray arr;
    for (const QVariant &v : m_history) {
        arr.append(QJsonObject::fromVariantMap(v.toMap()));
    }
    QFile file(historyFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    }
    emit historyChanged();
}

void DubbingProjectLifecycleService::recordHistoryEntry(const QString &path, const QString &mediaPath)
{
    if (path.isEmpty()) return;

    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].toMap().value("path").toString() == path) {
            m_history.removeAt(i);
            break;
        }
    }

    QVariantMap entry;
    entry["id"] = path;
    entry["path"] = path;
    entry["mediaPath"] = mediaPath;
    entry["fileName"] = QFileInfo(path).fileName();
    entry["lastOpened"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_history.prepend(entry);
    while (m_history.size() > 50)
        m_history.removeLast();

    saveHistory();
}

bool DubbingProjectLifecycleService::deleteHistoryItem(const QString &id)
{
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].toMap().value("id").toString() == id ||
            m_history[i].toMap().value("path").toString() == id) {
            m_history.removeAt(i);
            saveHistory();
            return true;
        }
    }
    return false;
}

void DubbingProjectLifecycleService::clearHistory()
{
    m_history.clear();
    saveHistory();
}

bool DubbingProjectLifecycleService::ensureProject(const QString &reason)
{
    Q_UNUSED(reason);
    if (!m_project.projectPath.isEmpty()) return true;

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString autoPath = QDir(appData).filePath("UntitledDubbingProject.json");
    return newProject(autoPath);
}

bool DubbingProjectLifecycleService::newProject(const QString &path)
{
    m_project = DubbingProject();
    m_project.projectPath = path;
    QString error;
    if (m_project.save(&error)) {
        recordHistoryEntry(path, m_project.sourceMediaPath);
        emit projectChanged();
        return true;
    }
    emit errorOccurred(error);
    return false;
}

bool DubbingProjectLifecycleService::openProject(const QString &path)
{
    QString error;
    DubbingProject loadedProject;
    if (DubbingProject::load(path, loadedProject, &error)) {
        m_project = loadedProject;
        m_project.projectPath = path;
        recordHistoryEntry(path, m_project.sourceMediaPath);
        emit projectChanged();
        emit previewChanged();
        emit exportChanged();
        return true;
    }
    emit errorOccurred(error);
    return false;
}

bool DubbingProjectLifecycleService::saveProject()
{
    QString error;
    if (m_project.save(&error)) {
        recordHistoryEntry(m_project.projectPath, m_project.sourceMediaPath);
        emit projectChanged();
        return true;
    }
    emit errorOccurred(error);
    return false;
}

bool DubbingProjectLifecycleService::saveProjectAs(const QString &path)
{
    m_project.projectPath = path;
    return saveProject();
}

bool DubbingProjectLifecycleService::importMedia(const QString &filePath)
{
    if (!ensureProject("importMedia")) return false;

    m_project.sourceMediaPath = filePath;
    QFileInfo fi(filePath);
    m_project.sourceIsVideo = fi.suffix().contains(QRegularExpression("^(mp4|mkv|mov|webm|avi)$", QRegularExpression::CaseInsensitiveOption));

    saveProject();
    emit projectChanged();
    emit previewChanged();
    return true;
}

void DubbingProjectLifecycleService::setNormalizedAudioPath(const QString &path)
{
    m_project.masterAudioPath = path;
    emit projectChanged();
}

void DubbingProjectLifecycleService::setVocalsPath(const QString &path)
{
    m_project.vocalsAudioPath = path;
    emit projectChanged();
}

void DubbingProjectLifecycleService::setBackgroundPath(const QString &path)
{
    m_project.backgroundAudioPath = path;
    emit projectChanged();
}

void DubbingProjectLifecycleService::setDubbedVocalPath(const QString &path)
{
    m_dubbedVocalPath = path;
    emit previewChanged();
}

void DubbingProjectLifecycleService::setPreviewPath(const QString &path)
{
    m_previewPath = path;
    emit previewChanged();
}

void DubbingProjectLifecycleService::setExportPath(const QString &path)
{
    m_exportPath = path;
    emit exportChanged();
}

void DubbingProjectLifecycleService::setCapCutDraft(const QString &draftPath, const QString &warning)
{
    m_capCutDraftPath = draftPath;
    m_capCutDraftWarning = warning;
    emit exportChanged();
}

} // namespace LAStudio
