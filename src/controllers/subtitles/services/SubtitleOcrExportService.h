#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace LAStudio {

class SubtitleOcrExportService : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleOcrExportService(QObject *parent = nullptr);
    ~SubtitleOcrExportService() override = default;

    static QString formatSrtTimestamp(qint64 ms);
    static QString generateSrtContent(const QVariantList &segments);
    static bool exportToSrtFile(const QString &filePath, const QVariantList &segments, QString *error = nullptr);
    static bool exportToVttFile(const QString &filePath, const QVariantList &segments, QString *error = nullptr);
};

} // namespace LAStudio
