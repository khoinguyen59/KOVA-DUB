#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

class Settings;
class ColabSession;

class DubbingColabCoordinatorService : public QObject
{
    Q_OBJECT

public:
    explicit DubbingColabCoordinatorService(Settings *settings = nullptr, QObject *parent = nullptr);
    ~DubbingColabCoordinatorService() override = default;

    void setRemoteSessions(ColabSession *translation, ColabSession *tts,
                           ColabSession *voiceClone, ColabSession *separation,
                           ColabSession *subtitleOcr = nullptr);

    QVariantList colabSetupStages() const;
    bool colabSetupChecking() const { return m_checking; }
    QString colabSetupSummary() const { return m_summary; }

    bool selectWorkflowColabModel(const QString &nodeId, const QString &modelId);
    QString colabNotebookForNode(const QString &nodeId, const QString &modelId = QString()) const;
    QVariantList colabModelOptionsForNode(const QString &nodeId) const;
    QString defaultColabModelForNode(const QString &nodeId) const;

    void checkColabSetup();

signals:
    void colabSetupChanged();
    void modelSelected(const QString &nodeId, const QString &modelId);

private:
    Settings *m_settings{nullptr};
    ColabSession *m_translationSession{nullptr};
    ColabSession *m_ttsSession{nullptr};
    ColabSession *m_voiceCloneSession{nullptr};
    ColabSession *m_separationSession{nullptr};
    ColabSession *m_subtitleOcrSession{nullptr};

    bool m_checking{false};
    QString m_summary;
};

} // namespace LAStudio
