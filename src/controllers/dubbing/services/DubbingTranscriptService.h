#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "dubbing/project/DubbingProject.h"

namespace LAStudio {

class DubbingTranscriptService : public QObject
{
    Q_OBJECT

public:
    explicit DubbingTranscriptService(DubbingProject *project, QObject *parent = nullptr);
    ~DubbingTranscriptService() override = default;

    void setProject(DubbingProject *project);

    QVariantList segments() const;
    void setSegments(const QVariantList &segs);

    bool updateSegment(int index, const QVariantMap &data);
    bool removeSegment(int index);
    bool reorderSegments(int fromIndex, int toIndex);

    QVariantMap transcriptConfiguration() const;
    void setTranscriptConfiguration(const QVariantMap &config);
    void setTranscriptFusionPolicy(const QString &policy);

    int unresolvedTranscriptConflictCount() const;

    bool resolveTranscriptConflict(int index, const QString &source);
    bool resolveAllTranscriptConflicts(const QString &source);

    QVariantMap transcriptConflictAiAvailability() const;
    bool requestTranscriptConflictAiSuggestion(int segmentIndex = -1);
    bool acceptTranscriptConflictAiSuggestion(int segmentIndex);
    bool rejectTranscriptConflictAiSuggestion(int segmentIndex);

signals:
    void segmentsChanged();
    void transcriptConfigurationChanged();
    void aiSuggestionRequested(int index);

private:
    void refreshConflictCount();

    DubbingProject *m_project{nullptr};
    int m_unresolvedConflictCount{0};
};

} // namespace LAStudio
