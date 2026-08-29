#include "test_DubbingWorkspaceContract.h"

#include "controllers/dubbing/DubbingColabModelRoutes.h"
#include "controllers/dubbing/DubbingController.h"
#include "controllers/shared/VoiceClonePresetService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QtTest>

namespace LAStudio {

namespace {

QString readSourceFile(const QString &relativePath)
{
    QFile file(QDir(QStringLiteral(LASTUDIO_SOURCE_DIR)).filePath(relativePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QVariantMap findById(const QVariantList &items, const QString &id)
{
    for (const QVariant &entry : items) {
        const QVariantMap value = entry.toMap();
        if (value.value(QStringLiteral("id")).toString() == id)
            return value;
    }
    return {};
}

} // namespace

void TestDubbingWorkspaceContract::workflowPresentationUsesStableStages()
{
    DubbingController controller(nullptr, nullptr);
    const QVariantList stages = controller.workflowStages();
    QCOMPARE(stages.size(), 8);

    const QStringList expectedIds{
        QStringLiteral("import"), QStringLiteral("normalize"),
        QStringLiteral("isolator"), QStringLiteral("transcribe"),
        QStringLiteral("translate"), QStringLiteral("tts"),
        QStringLiteral("alignment-subtitle"), QStringLiteral("export")};
    QStringList actualIds;
    for (const QVariant &entry : stages)
        actualIds.append(entry.toMap().value(QStringLiteral("id")).toString());
    QCOMPARE(actualIds, expectedIds);
    const QSet<QString> uniqueIds(actualIds.cbegin(), actualIds.cend());
    QCOMPARE(uniqueIds.size(), expectedIds.size());
}

void TestDubbingWorkspaceContract::voiceCatalogUsesNormalizedMetadataAndOneSourceOfTruth()
{
    VoiceClonePresetService service;
    const QVariantList catalog = service.allPresets();
    QVERIFY(!catalog.isEmpty());

    DubbingController controller(nullptr, nullptr);
    controller.setVoiceClonePresetService(&service);
    const QVariantList options = controller.ttsVoiceOptions();
    QVERIFY(options.size() >= catalog.size());

    QSet<QString> optionIds;
    for (const QVariant &entry : options)
        optionIds.insert(entry.toMap().value(QStringLiteral("id")).toString());

    bool foundVieNeu = false;
    for (const QVariant &entry : catalog) {
        const QVariantMap voice = entry.toMap();
        const QString id = voice.value(QStringLiteral("id")).toString().trimmed();
        QVERIFY(!id.isEmpty());
        QVERIFY(!voice.value(QStringLiteral("displayName")).toString().trimmed().isEmpty());
        QVERIFY(!voice.value(QStringLiteral("familyId")).toString().trimmed().isEmpty());
        QVERIFY(!voice.value(QStringLiteral("category")).toString().trimmed().isEmpty());
        QVERIFY(!voice.value(QStringLiteral("language")).toString().trimmed().isEmpty());
        QVERIFY(optionIds.contains(id));
        const QString family = voice.value(QStringLiteral("familyId")).toString().toLower();
        if (family.startsWith(QStringLiteral("vieneu"))) {
            foundVieNeu = true;
            QVERIFY(voice.value(QStringLiteral("compatibleModelFamilies")).toList()
                        .contains(QStringLiteral("omnivoice")));
        }
    }
    QVERIFY(foundVieNeu);
}

void TestDubbingWorkspaceContract::remoteVoiceCloneRoutesAreExact()
{
    QVERIFY(DubbingColabModelRoutes::supports(
        QStringLiteral("voice-cloning"), QStringLiteral("omnivoice")));
    QVERIFY(!DubbingColabModelRoutes::notebookForModel(
        QStringLiteral("voice-cloning"), QStringLiteral("omnivoice")).isEmpty());
    QVERIFY(DubbingColabModelRoutes::supports(
        QStringLiteral("synthesize"), QStringLiteral("omnivoice")));
    QCOMPARE(DubbingColabModelRoutes::notebookForModel(
                 QStringLiteral("synthesize"), QStringLiteral("vieneu")),
             QStringLiteral("LA_STUDIO_TTS_VIENEU_V3_TURBO_GPU.ipynb"));
}

void TestDubbingWorkspaceContract::missingSeparationStemCannotBecomeAHiddenFallback()
{
    DubbingController controller(nullptr, nullptr);
    const QVariantList nodes = controller.workflowNodes();
    const QVariantMap separation = findById(nodes, QStringLiteral("source-separate"));
    QVERIFY(!separation.isEmpty());
    QCOMPARE(separation.value(QStringLiteral("state")).toString(), QStringLiteral("missing"));
    QVERIFY(separation.value(QStringLiteral("optional")).toBool());
    QVERIFY(separation.value(QStringLiteral("canSkip")).toBool());
    QVERIFY(!controller.workflowRecovery().value(QStringLiteral("fallbackUsed"), false).toBool());

    const QString implementation = readSourceFile(
        QStringLiteral("src/controllers/dubbing/parts/DubbingController_Workflow.cpp"));
    QVERIFY(implementation.contains(QStringLiteral("vocalsAudioPath")));
    QVERIFY(implementation.contains(QStringLiteral("backgroundAudioPath")));
    const QString artifacts = readSourceFile(
        QStringLiteral("src/controllers/dubbing/parts/DubbingController_Artifacts.cpp"));
    QVERIFY(artifacts.contains(QStringLiteral("Only the explicit")));
    QVERIFY(artifacts.contains(QStringLiteral("m_project.vocalsAudioPath")));
    QVERIFY(artifacts.contains(QStringLiteral("Separate is optional")));
}

void TestDubbingWorkspaceContract::productionQmlExposesTheWorkspaceContract()
{
    const QString page = readSourceFile(QStringLiteral("qml/pages/DubbingPage.qml"));
    const QString preview = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingSourceMediaPanel.qml"));
    const QString shelf = readSourceFile(
        QStringLiteral("qml/components/dubbing/panels/DubbingTaskShelf.qml"));
    const QString header = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingWorkflowStep.qml"));
    const QString voice = readSourceFile(QStringLiteral("qml/components/shared/VoiceGalleryDialog.qml"));
    const QString separate = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingSeparateStep.qml"));
    const QString normalize = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingNormalizeStep.qml"));
    const QString transcribe = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingTranscribeStep.qml"));
    const QString translate = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingTranslateStep.qml"));
    const QString mix = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingMixStep.qml"));
    const QString alignment = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingAlignmentStep.qml"));

    QVERIFY(page.contains(QStringLiteral("DubbingReviewPanel")));
    QVERIFY(page.contains(QStringLiteral("Right Pane: persistent task review and controls")));
    QVERIFY(!page.contains(QStringLiteral("DubbingTaskShelf {")));
    QVERIFY(!page.contains(QStringLiteral("DubbingContextDrawer {")));
    QVERIFY(page.contains(QStringLiteral("function nodeNeedsModelSelection(nodeId)")));
    QVERIFY(page.contains(QStringLiteral("nodeModelDialog.openFor(nodeId)")));
    QVERIFY(page.contains(QStringLiteral("function colabStageIdForNode(nodeId)")));
    QVERIFY(page.contains(QStringLiteral("root.colabStageIdForNode(nodeId)")));
    QVERIFY(page.contains(QStringLiteral("onWorkflowSetupRequired(nodeId, setupKind, message)")));
    QVERIFY(page.contains(QStringLiteral("qmlPreviewSelectDubbingStep"))
            || readSourceFile(QStringLiteral("qml/Main.qml")).contains(
                QStringLiteral("qmlPreviewSelectDubbingStep")));
    QVERIFY(page.contains(QStringLiteral("showOcrTools: root.displayedStepId === \"transcribe\"")));
    QVERIFY(preview.contains(QStringLiteral("previewFrameAspectRatio: 16 / 9")));
    QVERIFY(preview.contains(QStringLiteral("VideoOutput.PreserveAspectFit")));
    QVERIFY(preview.contains(QStringLiteral("Text.ElideMiddle")));
    QVERIFY(preview.contains(QStringLiteral("dubbingVideoThumbnail")));
    QVERIFY(preview.contains(QStringLiteral("dubbingVideoThumbnailImage")));
    QVERIFY(preview.contains(QStringLiteral("sourceThumbnailUrl")));
    QVERIFY(preview.contains(QStringLiteral("asynchronous: true")));
    QVERIFY(preview.contains(QStringLiteral("Image {")));
    QVERIFY(preview.contains(QStringLiteral("readonly property rect sourceContent")));
    QVERIFY(preview.contains(QStringLiteral("var controlsTop = previewControls.y - Theme.paddingSmall")));
    QVERIFY(preview.contains(QStringLiteral("dubbingOcrRoiOverlay.y + dubbingOcrRoiOverlay.height")));
    QVERIFY(shelf.contains(QStringLiteral("contextRequested")));
    QVERIFY(shelf.contains(QStringLiteral("runStepRequested")));
    QVERIFY(header.contains(QStringLiteral("shortTitle")));
    QVERIFY(header.contains(QStringLiteral("detailTitle")));
    QVERIFY(voice.contains(QStringLiteral(
        "signal voiceSelected(string audioPath, string referenceText, string name, string familyId, string voiceId)")));
    QVERIFY(!readSourceFile(QStringLiteral(
        "qml/components/dubbing/steps/DubbingSynthesizeStep.qml")).contains(QStringLiteral("53+clone")));
    QVERIFY(alignment.contains(QStringLiteral("dubbingOriginalAudioLevelSlider")));
    QVERIFY(alignment.contains(QStringLiteral("dubbingDubbedAudioLevelSlider")));
    QVERIFY(page.contains(QStringLiteral("5. Dịch Thuật AI")));
    QVERIFY(page.contains(QStringLiteral("6. Lồng Tiếng AI")));
    QVERIFY(page.contains(QStringLiteral("7. Khớp Chữ & Căn Chỉnh")));

    for (const QString &step : {separate, normalize, transcribe, translate, mix}) {
        QVERIFY(step.contains(QStringLiteral("signal runRequested")));
        QVERIFY(!step.contains(QStringLiteral("root.dubbing.runWorkflowNode")));
    }
}

void TestDubbingWorkspaceContract::colabSetupExposesUnifiedNotebookAndIndependentTtsOcrRoutes()
{
    const QString colab = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingColabSetupDialog.qml"));
    QVERIFY(colab.contains(QStringLiteral("LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb")));
    QVERIFY(colab.contains(QStringLiteral("Open Unified Colab")));
    QVERIFY(colab.contains(QStringLiteral("dubbingTtsWorkerUrlField")));
    QVERIFY(colab.contains(QStringLiteral("dubbingOcrWorkerUrlField")));
    QVERIFY(colab.contains(QStringLiteral("dubbingSttWorkerUrlField")));
    QVERIFY(colab.contains(QStringLiteral("dubbingTranscriptOcrWorkerUrlField")));
    QVERIFY(colab.contains(QStringLiteral("showUnifiedRoutePanel")));
    QVERIFY(colab.contains(QStringLiteral("connectWorkflowColabStage")));
}

void TestDubbingWorkspaceContract::verifiedDirectColabRunDoesNotReopenModelPicker()
{
    const QString page = readSourceFile(QStringLiteral("qml/pages/DubbingPage.qml"));
    const int providerResolution = page.indexOf(
        QStringLiteral("var providerId = config.executionProvider"));
    const int remoteBranch = page.indexOf(
        QStringLiteral("if (providerId === \"colab-direct\" || providerId === \"colab-gpu\")"));
    const int familyGuard = page.indexOf(QStringLiteral("if (familyId === \"\")"));
    QVERIFY(providerResolution >= 0);
    QVERIFY(remoteBranch > providerResolution);
    QVERIFY(familyGuard > remoteBranch);
    QVERIFY(page.contains(QStringLiteral(
        "if (providerId === \"colab-direct\" || providerId === \"colab-gpu\") {")));
    QVERIFY(page.contains(QStringLiteral("return setupStages[i].verified !== true")));
}

void TestDubbingWorkspaceContract::alignmentResultsExposeRunActions()
{
    const QString reviewPanel = readSourceFile(
        QStringLiteral("qml/components/dubbing/panels/DubbingReviewPanel.qml"));
    const QString alignmentStep = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingAlignmentStep.qml"));
    QVERIFY(!alignmentStep.isEmpty());
    QVERIFY(reviewPanel.contains(QStringLiteral("DubbingAlignmentStep")));
    QVERIFY(reviewPanel.contains(QStringLiteral("fit-timing")));
    QVERIFY(reviewPanel.contains(QStringLiteral("review-conflicts")));
    QVERIFY(alignmentStep.contains(QStringLiteral("signal runRequested")));
    QVERIFY(alignmentStep.contains(QStringLiteral("runRequested(\"fit-timing\")")));
}

void TestDubbingWorkspaceContract::historyIsAnOverlayAndMoreMenuStaysAnchored()
{
    const QString page = readSourceFile(QStringLiteral("qml/pages/DubbingPage.qml"));
    const QString header = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingWorkflowHeader.qml"));
    QVERIFY(page.contains(QStringLiteral("DubbingHistoryOverlay")));
    QVERIFY(page.contains(QStringLiteral("parent: Overlay.overlay")));
    QVERIFY(!page.contains(QStringLiteral("// Left Pane 1: Dubbing History")));
    QVERIFY(header.contains(QStringLiteral("moreActionsButton.mapToItem(null, 0, moreActionsButton.height)")));
    QVERIFY(header.contains(QStringLiteral("root.width - actionMenu.width")));
    QVERIFY(header.contains(QStringLiteral("actionMenu.y = anchor.y")));
}

void TestDubbingWorkspaceContract::exportOffersDirectCapCutOpenAndThumbnailStaysVisibleUntilPlayback()
{
    const QString controller = readSourceFile(
        QStringLiteral("src/controllers/dubbing/DubbingController.h"));
    const QString exportStep = readSourceFile(
        QStringLiteral("qml/components/dubbing/steps/DubbingExportStep.qml"));
    const QString exportDialog = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingExportDialog.qml"));
    const QString preview = readSourceFile(
        QStringLiteral("qml/components/dubbing/DubbingSourceMediaPanel.qml"));
    QVERIFY(controller.contains(QStringLiteral("openCapCutDraft")));
    QVERIFY(exportStep.contains(QStringLiteral("openCapCutDraft")));
    QVERIFY(exportDialog.contains(QStringLiteral("Open in CapCut")));
    QVERIFY(preview.contains(QStringLiteral(
        "visible: root.isVideoSource && (!root.thumbnailReady")));
}

void TestDubbingWorkspaceContract::projectSetupResolvesQmlVariantListLanguageDefaults()
{
    const QString setup = readSourceFile(QStringLiteral(
        "qml/components/dubbing/DubbingProjectSetupDialog.qml"));
    QVERIFY(!setup.isEmpty());

    // CatalogManager exposes QVariantList to QML.  The setup dialog must not
    // require the value to be a JavaScript Array, otherwise both comboboxes
    // silently fall back to index 0 (English) for a new zh -> vi project.
    QVERIFY(!setup.contains(QStringLiteral("Array.isArray(languageCatalog)")));
    QVERIFY(setup.contains(QStringLiteral("typeof source.length === \"number\"")));
    QVERIFY(setup.contains(QStringLiteral("languageCatalogItem(index)")));
    QVERIFY(setup.contains(QStringLiteral("return source[index]")));
}

void TestDubbingWorkspaceContract::packagingRequiresPreBuildReleaseGate()
{
    const QString gate = readSourceFile(QStringLiteral("scripts/prebuild_gate.ps1"));
    const QString package = readSourceFile(QStringLiteral("scripts/package.ps1"));
    const QString checklist = readSourceFile(QStringLiteral("PRE_DELIVERY_CHECKLIST.md"));

    QVERIFY2(!gate.isEmpty(), "The mandatory pre-build release gate script is missing.");
    QVERIFY2(!checklist.isEmpty(), "The mandatory delivery checklist is missing.");
    QVERIFY(checklist.contains(QStringLiteral("cross-task regression sweep")));
    QVERIFY(checklist.contains(QStringLiteral("đủ 8 task canonical")));
    QVERIFY(checklist.contains(QStringLiteral("Error path")));
    QVERIFY(checklist.contains(QStringLiteral("Chỉ kiểm tra riêng task bị báo lỗi")));
    QVERIFY(gate.contains(QStringLiteral("PRE_DELIVERY_CHECKLIST.md")));
    QVERIFY(gate.contains(QStringLiteral("[regex]::Matches")));
    QVERIFY(gate.contains(QStringLiteral("highestIncidentNumber")));
    QVERIFY(gate.contains(QStringLiteral("git diff --check")));
    QVERIFY(gate.contains(QStringLiteral("lint_qml.ps1")));
    QVERIFY(gate.contains(QStringLiteral("run_tests.ps1")));
    QVERIFY(gate.contains(QStringLiteral("verify_remote_feature_surface.ps1")));
    QVERIFY(gate.contains(QStringLiteral("verify_colab_model_bindings.py")));
    QVERIFY(gate.contains(QStringLiteral("verify_generated_colab_notebooks.py")));
    QVERIFY(gate.contains(QStringLiteral("test_colab_worker_pins.py")));
    QVERIFY(gate.contains(QStringLiteral("verify_colab_worker_pins.py")));
    QVERIFY(gate.contains(QStringLiteral("verify_unified_dubbing_colab_notebook.py")));
    QVERIFY(checklist.contains(QStringLiteral("Self-contained Colab worker gate")));
    QVERIFY(checklist.contains(QStringLiteral("Embedded worker integrity")));
    QVERIFY(package.contains(QStringLiteral("prebuild_gate.ps1")));
    QVERIFY(package.contains(QStringLiteral("Pre-build release gate failed")));
    QVERIFY(package.contains(QStringLiteral("LASTUDIO_QML_SMOKE")));
    QVERIFY(package.contains(QStringLiteral("$env:ComSpec")));
    QVERIFY(package.contains(QStringLiteral("$LASTEXITCODE")));
    QVERIFY(package.contains(QStringLiteral("package-smoke")));
}

} // namespace LAStudio
