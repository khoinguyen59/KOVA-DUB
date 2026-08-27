#pragma once

#include <QObject>

namespace LAStudio {

class TestAppErrorCatalog final : public QObject
{
    Q_OBJECT

private slots:
    void classifiesVoiceIsolationRuntimeFailure();
    void preservesTechnicalDetailsForGenericFailure();
    void exposesStructuredErrorGuidanceContract();
};

} // namespace LAStudio
