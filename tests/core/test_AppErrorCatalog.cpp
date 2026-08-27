#include "test_AppErrorCatalog.h"

#include "controllers/app/AppErrorCatalog.h"

#include <QFile>
#include <QtTest>

namespace {

QString readSourceFile(const QString &relativePath)
{
    const QString sourceRoot = QString::fromUtf8(LASTUDIO_SOURCE_DIR);
    QFile file(sourceRoot + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

namespace LAStudio {

void TestAppErrorCatalog::classifiesVoiceIsolationRuntimeFailure()
{
    const QString rawMessage = QStringLiteral(
        "Voice isolation runtime or model is unavailable. Install/configure it or connect an exact Colab GPU worker; "
        "normalized source audio will not be used as a substitute.");

    const AppErrorPresentation presentation = classifyAppError(rawMessage, QStringLiteral("Voice Isolator"));

    QCOMPARE(presentation.code, QStringLiteral("dubbing-separation-runtime"));
    QVERIFY(!presentation.title.isEmpty());
    QVERIFY(!presentation.summary.isEmpty());
    QVERIFY(presentation.guidance.contains(QStringLiteral("Colab"), Qt::CaseInsensitive));
    QVERIFY(!presentation.actionRoute.isEmpty());
    QCOMPARE(presentation.technicalDetails, rawMessage);
}

void TestAppErrorCatalog::preservesTechnicalDetailsForGenericFailure()
{
    const QString rawMessage = QStringLiteral("Unexpected worker failure: exit code 17");
    const AppErrorPresentation presentation = classifyAppError(rawMessage, QStringLiteral("Dubbing"));

    const QVariantMap map = presentation.toVariantMap();
    QCOMPARE(map.value(QStringLiteral("message")).toString(), rawMessage);
    QCOMPARE(map.value(QStringLiteral("technicalDetails")).toString(), rawMessage);
    QVERIFY(!map.value(QStringLiteral("guidance")).toString().isEmpty());
}

void TestAppErrorCatalog::exposesStructuredErrorGuidanceContract()
{
    const QString appController = readSourceFile(QStringLiteral("src/controllers/app/AppController.h"));
    const QString appControllerCpp = readSourceFile(QStringLiteral("src/controllers/app/AppController.cpp"));
    const QString errorCatalog = readSourceFile(QStringLiteral("src/controllers/app/AppErrorCatalog.h"));
    const QString workflowManager = readSourceFile(QStringLiteral("src/controllers/app/WorkflowActivityManager.cpp"));
    const QString mainQml = readSourceFile(QStringLiteral("qml/Main.qml"));
    const QString dialogQml = readSourceFile(QStringLiteral("qml/components/shared/ErrorGuidanceDialog.qml"));

    QVERIFY(appController.contains(QStringLiteral("currentError")));
    QVERIFY(errorCatalog.contains(QStringLiteral("technicalDetails")));
    QVERIFY(appControllerCpp.contains(QStringLiteral("presentation.toVariantMap()")));
    QVERIFY(appControllerCpp.contains(QStringLiteral("Logger::error")));
    QVERIFY(mainQml.contains(QStringLiteral("ErrorGuidanceDialog")));
    QVERIFY(mainQml.contains(QStringLiteral("requestStudioRoute")));
    QVERIFY(dialogQml.contains(QStringLiteral("ScrollView")));
    QVERIFY(dialogQml.contains(QStringLiteral("currentError")));
    QVERIFY(dialogQml.contains(QStringLiteral("technicalDetails")));
    QVERIFY(workflowManager.contains(QStringLiteral("subtitle-ocr")));
}

} // namespace LAStudio
