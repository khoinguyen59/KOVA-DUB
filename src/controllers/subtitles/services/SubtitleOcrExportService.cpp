#include "SubtitleOcrExportService.h"

#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QVariantMap>

namespace LAStudio {

SubtitleOcrExportService::SubtitleOcrExportService(QObject *parent)
    : QObject(parent)
{
}

QString SubtitleOcrExportService::formatSrtTimestamp(qint64 ms)
{
    const int hours = static_cast<int>(ms / 3600000);
    const int minutes = static_cast<int>((ms % 3600000) / 60000);
    const int seconds = static_cast<int>((ms % 60000) / 1000);
    const int millis = static_cast<int>(ms % 1000);

    return QString::asprintf("%02d:%02d:%02d,%03d", hours, minutes, seconds, millis);
}

QString SubtitleOcrExportService::generateSrtContent(const QVariantList &segments)
{
    QString content;
    QTextStream stream(&content);

    int index = 1;
    for (const QVariant &v : segments) {
        const QVariantMap seg = v.toMap();
        const qint64 startMs = seg.value("startMs", seg.value("start", 0)).toLongLong();
        const qint64 endMs = seg.value("endMs", seg.value("end", startMs + 2000)).toLongLong();
        const QString text = seg.value("text", seg.value("sourceText")).toString().trimmed();

        if (text.isEmpty()) continue;

        stream << index++ << "\n";
        stream << formatSrtTimestamp(startMs) << " --> " << formatSrtTimestamp(endMs) << "\n";
        stream << text << "\n\n";
    }

    return content;
}

bool SubtitleOcrExportService::exportToSrtFile(const QString &filePath, const QVariantList &segments, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << generateSrtContent(segments);
    return true;
}

bool SubtitleOcrExportService::exportToVttFile(const QString &filePath, const QVariantList &segments, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "WEBVTT\n\n";

    int index = 1;
    for (const QVariant &v : segments) {
        const QVariantMap seg = v.toMap();
        const qint64 startMs = seg.value("startMs", seg.value("start", 0)).toLongLong();
        const qint64 endMs = seg.value("endMs", seg.value("end", startMs + 2000)).toLongLong();
        const QString text = seg.value("text", seg.value("sourceText")).toString().trimmed();

        if (text.isEmpty()) continue;

        QString startStr = formatSrtTimestamp(startMs).replace(',', '.');
        QString endStr = formatSrtTimestamp(endMs).replace(',', '.');

        out << index++ << "\n";
        out << startStr << " --> " << endStr << "\n";
        out << text << "\n\n";
    }

    return true;
}

} // namespace LAStudio
