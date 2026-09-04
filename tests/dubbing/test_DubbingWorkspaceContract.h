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
    void colabSetupExposesUnifiedNotebookAndIndependentTtsOcrRoutes();
    void verifiedDirectColabRunDoesNotReopenModelPicker();
    void alignmentResultsExposeRunActions();
    void historyIsAnOverlayAndMoreMenuStaysAnchored();
    void exportOffersDirectCapCutOpenAndThumbnailStaysVisibleUntilPlayback();
    void thumbnailPropertyGetterDoesNotTouchFilesystem();
    void pausedVideoKeepsLastFrameVisible();
    void playerTimelineRemainsInteractiveAboveOcrRoi();
    void ocrEditorCanCrossPlayerSeekBar();
    void dubbedPreviewAvoidsUnnecessaryDecodeAndSeekJitter();
    void aiTranscriptGuideIsImmediateAndProjectScoped();
    void projectSetupResolvesQmlVariantListLanguageDefaults();
    void packagingRequiresPreBuildReleaseGate();
};

} // namespace LAStudio
