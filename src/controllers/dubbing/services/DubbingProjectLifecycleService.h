#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "dubbing/project/DubbingProject.h"

namespace LAStudio {

class DubbingProjectLifecycleService : public QObject
{
    Q_OBJECT

public:
    explicit DubbingProjectLifecycleService(QObject *parent = nullptr);
    ~DubbingProjectLifecycleService() override = default;

    DubbingProject *project() { return &m_project; }
    const DubbingProject *project() const { return &m_project; }

    bool hasProject() const { return !m_project.projectPath.isEmpty(); }
    QString projectPath() const { return m_project.projectPath; }
    QString sourceMediaPath() const { return m_project.sourceMediaPath; }
    QUrl sourceMediaUrl() const;
    QUrl playbackMediaUrl() const;
    QString normalizedAudioPath() const { return m_project.masterAudioPath; }
    QString vocalsPath() const { return m_project.vocalsAudioPath; }
    QString backgroundPath() const { return m_project.backgroundAudioPath; }
    QString dubbedVocalPath() const { return m_dubbedVocalPath; }
    QString previewPath() const { return m_previewPath; }
    QString exportPath() const { return m_exportPath; }
    QString capCutDraftPath() const { return m_capCutDraftPath; }
    QString capCutDraftWarning() const { return m_capCutDraftWarning; }

    QString sourceLanguage() const { return m_project.sourceLanguage; }
    void setSourceLanguage(const QString &lang);
    QString targetLanguage() const { return m_project.targetLanguage; }
    void setTargetLanguage(const QString &lang);

    QVariantMap durationControl() const { return m_project.durationControl; }
    void setDurationControl(const QVariantMap &ctrl);

    QVariantList speakers() const { return m_project.speakers; }

    QVariantList history() const { return m_history; }
    void loadHistory();
    void saveHistory();
    bool deleteHistoryItem(const QString &id);
    void clearHistory();

    static QString defaultProjectsDirectory();
    bool createAutoProject(const QString &customName = QString());
    bool ensureProject(const QString &reason = QString());
    bool newProject(const QString &path);
    bool openProject(const QString &path);
    bool saveProject();
    bool saveProjectAs(const QString &path);
    bool importMedia(const QString &filePath);

    void setNormalizedAudioPath(const QString &path);
    void setVocalsPath(const QString &path);
    void setBackgroundPath(const QString &path);
    void setDubbedVocalPath(const QString &path);
    void setPreviewPath(const QString &path);
    void setExportPath(const QString &path);
    void setCapCutDraft(const QString &draftPath, const QString &warning = QString());

signals:
    void projectChanged();
    void historyChanged();
    void previewChanged();
    void exportChanged();
    void errorOccurred(const QString &message);

private:
    void recordHistoryEntry(const QString &path, const QString &mediaPath);
    QString historyFilePath() const;

    DubbingProject m_project;
    QVariantList m_history;
    QString m_dubbedVocalPath;
    QString m_previewPath;
    QString m_exportPath;
    QString m_capCutDraftPath;
    QString m_capCutDraftWarning;
};

} // namespace LAStudio
