#pragma once

#include <QString>
#include <QVariantMap>

namespace LAStudio {

// A presentation is deliberately separate from the raw error string.  The
// raw message remains available to diagnostics while the UI receives a stable
// code, plain-language explanation and an optional safe route to recovery.
struct AppErrorPresentation
{
    QString code;
    QString source;
    QString title;
    QString summary;
    QString guidance;
    QString actionId;
    QString actionLabel;
    QString actionRoute;
    QString technicalDetails;

    QVariantMap toVariantMap() const;
};

AppErrorPresentation classifyAppError(const QString &technicalMessage,
                                      const QString &source = {});

} // namespace LAStudio
