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
        QStringLiteral("alignment-subtitle"), QStringLiteral("translate"),
        QStringLiteral("tts"), QStringLiteral("export")};
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
    QVERIFY(!controller.workflowRecovery().value(QStringLiteral("fallbackUsed"), false).toBool());

    const QString implementation = readSourceFile(
        QStringLiteral("src/controllers/dubbing/parts/DubbingController_Workflow.cpp"));
    QVERIFY(implementation.contains(QStringLiteral("vocalsAudioPath")));
    QVERIFY(implementation.contains(QStringLiteral("backgroundAudioPath")));
    const QString artifacts = readSourceFile(
        QStringLiteral("src/controllers/dubbing/parts/DubbingController_Artifacts.cpp"));
    QVERIFY(artifacts.contains(QStringLiteral("Only the explicit")));
    QVERIFY(artifacts.contains(QStringLiteral("m_project.vocalsAudioPath")));
    QVERIFY(artifacts.contains(QStringLiteral("Background stem is missing or unreadable")));
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

    QVERIFY(page.contains(QStringLiteral("DubbingReviewPanel")));
    QVERIFY(page.contains(QStringLiteral("Right Pane: persistent task review and controls")));
    QVERIFY(!page.contains(QStringLiteral("DubbingTaskShelf {")));
    QVERIFY(!page.contains(QStringLiteral("DubbingContextDrawer {")));
    QVERIFY(page.contains(QStringLiteral("qmlPreviewSelectDubbingStep"))
            || readSourceFile(QStringLiteral("qml/Main.qml")).contains(
                QStringLiteral("qmlPreviewSelectDubbingStep")));
    QVERIFY(page.contains(QStringLiteral("showOcrTools: root.displayedStepId === \"transcribe\"")));
    QVERIFY(preview.contains(QStringLiteral("previewFrameAspectRatio: 16 / 9")));
    QVERIFY(preview.contains(QStringLiteral("VideoOutput.PreserveAspectFit")));
    QVERIFY(preview.contains(QStringLiteral("Text.ElideMiddle")));
    QVERIFY(preview.contains(QStringLiteral("dubbingVideoThumbnail")));
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
}

} // namespace LAStudio
