#pragma once

#include <QObject>

namespace LAStudio {

class TestDubbingWorkspaceContract : public QObject
{
    Q_OBJECT

private slots:
    void workflowPresentationUsesStableStages();
    void voiceCatalogUsesNormalizedMetadataAndOneSourceOfTruth();
    void remoteVoiceCloneRoutesAreExact();
    void missingSeparationStemCannotBecomeAHiddenFallback();
    void productionQmlExposesTheWorkspaceContract();
};

} // namespace LAStudio
