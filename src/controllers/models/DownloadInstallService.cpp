#include "DownloadInstallService.h"

#include "core/storage/Settings.h"
#include "core/models/DownloadManager.h"
#include "core/models/ModelManager.h"
#include "core/models/RuntimeManager.h"
#include "core/utils/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QMetaType>
#include <QDateTime>
#include <QThreadPool>
#include <QCryptographicHash>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QDirIterator>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QUrl>

#include <limits>

#include <curl/curl.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>
#endif

namespace LAStudio {

namespace {
QVersionNumber parsedRuntimeVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    return QVersionNumber::fromString(version);
}

bool runtimeVersionGreater(const QString &left, const QString &right)
{
    if (right.isEmpty())
        return !left.isEmpty();
    const QVersionNumber leftVersion = parsedRuntimeVersion(left);
    const QVersionNumber rightVersion = parsedRuntimeVersion(right);
    if (!leftVersion.isNull() && !rightVersion.isNull())
        return QVersionNumber::compare(leftVersion, rightVersion) > 0;
    return QString::compare(left, right, Qt::CaseInsensitive) > 0;
}

size_t discardBodyCallback(char *ptr, size_t size, size_t nmemb, void *)
{
    Q_UNUSED(ptr);
    return size * nmemb;
}

size_t metadataHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    const size_t total = size * nitems;
    auto *headers = static_cast<QVariantMap *>(userdata);
    const QString line = QString::fromUtf8(buffer, static_cast<qsizetype>(total)).trimmed();
    const int separator = line.indexOf(QLatin1Char(':'));
    if (separator <= 0) {
        return total;
    }

    const QString key = line.left(separator).trimmed().toLower();
    const QString value = line.mid(separator + 1).trimmed();
    if (key == QStringLiteral("x-repo-commit")) {
        headers->insert(QStringLiteral("repoCommit"), value);
    } else if (key == QStringLiteral("etag")) {
        headers->insert(QStringLiteral("etag"), value);
    } else if (key == QStringLiteral("x-linked-etag")) {
        headers->insert(QStringLiteral("linkedEtag"), value);
    } else if (key == QStringLiteral("x-linked-size")) {
        headers->insert(QStringLiteral("linkedSize"), value.toLongLong());
    } else if (key == QStringLiteral("content-length")) {
        headers->insert(QStringLiteral("contentLength"), value.toLongLong());
    } else if (key == QStringLiteral("x-xet-hash")) {
        headers->insert(QStringLiteral("xetHash"), value);
    }
    return total;
}

QString cleanFingerprint(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')) && value.size() > 1) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

QString archiveExtractor(const QString &name)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundled = QDir(appDir).absoluteFilePath(name);
    return QFileInfo(bundled).isExecutable() ? bundled : QString();
}

QString systemMsiexecPath()
{
#ifdef Q_OS_WIN
    wchar_t systemDirectory[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    const QString path = QDir(QString::fromWCharArray(systemDirectory))
                             .absoluteFilePath(QStringLiteral("msiexec.exe"));
    return QFileInfo::exists(path) ? path : QString();
#else
    return {};
#endif
}

bool hasTrustedAuthenticodeSignature(const QString &path)
{
#ifdef Q_OS_WIN
    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = reinterpret_cast<LPCWSTR>(path.utf16());

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG verification = WinVerifyTrust(nullptr, &policy, &trustData);
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &trustData);
    return verification == ERROR_SUCCESS;
#else
    Q_UNUSED(path);
    return false;
#endif
}

QString fileFingerprint(const QVariantMap &metadata)
{
    QString value = metadata.value(QStringLiteral("linkedEtag")).toString();
    if (value.isEmpty()) value = metadata.value(QStringLiteral("etag")).toString();
    if (value.isEmpty()) value = metadata.value(QStringLiteral("xetHash")).toString();
    return cleanFingerprint(value);
}

qint64 remoteSize(const QVariantMap &metadata)
{
    qint64 value = metadata.value(QStringLiteral("linkedSize")).toLongLong();
    if (value <= 0) value = metadata.value(QStringLiteral("contentLength")).toLongLong();
    return value;
}

QVariantMap fetchRemoteFileMetadata(const QString &modelId, const QString &filename)
{
    QVariantMap metadata;
    if (modelId.isEmpty() || filename.isEmpty()) {
        return metadata;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        return metadata;
    }

    const QString url = QStringLiteral("https://huggingface.co/%1/resolve/main/%2").arg(modelId, filename);
    const QByteArray urlBytes = url.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardBodyCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, metadataHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &metadata);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LAStudio/0.1");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    const CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || responseCode >= 400) {
        metadata.clear();
        return metadata;
    }

    metadata.insert(QStringLiteral("provider"), QStringLiteral("huggingface"));
    metadata.insert(QStringLiteral("modelId"), modelId);
    metadata.insert(QStringLiteral("filename"), filename);
    metadata.insert(QStringLiteral("sourceUrl"), url);
    metadata.insert(QStringLiteral("checkedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    metadata.insert(QStringLiteral("fingerprint"), fileFingerprint(metadata));
    return metadata;
}

bool hasExpectedArchiveSignature(const QString &path, const QString &filename)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray header = file.read(4);
    if (filename.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        return header.startsWith("PK");
    }
    if (filename.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tgz"), Qt::CaseInsensitive)) {
        return header.size() >= 2 &&
               static_cast<unsigned char>(header[0]) == 0x1f &&
               static_cast<unsigned char>(header[1]) == 0x8b;
    }
    if (filename.endsWith(QStringLiteral(".tar.bz2"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tbz2"), Qt::CaseInsensitive)) {
        return header.size() >= 3 &&
               header[0] == 'B' &&
               header[1] == 'Z' &&
               header[2] == 'h';
    }
    return true;
}

QString yamlScalar(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) return QStringLiteral("null");
    if (value.typeId() == QMetaType::Bool) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.canConvert<int>() && value.typeId() != QMetaType::QString) return value.toString();

    QString text = value.toString();
    text.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    text.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(text);
}

void appendYamlValue(QStringList &lines, const QVariant &value, int indent);

void appendYamlMap(QStringList &lines, const QVariantMap &map, int indent)
{
    const QString pad(indent, QLatin1Char(' '));
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const QVariant value = it.value();
        if (value.typeId() == QMetaType::QVariantMap || value.typeId() == QMetaType::QVariantList) {
            lines << pad + it.key() + QStringLiteral(":");
            appendYamlValue(lines, value, indent + 2);
        } else {
            lines << pad + it.key() + QStringLiteral(": ") + yamlScalar(value);
        }
    }
}

void appendYamlList(QStringList &lines, const QVariantList &list, int indent)
{
    const QString pad(indent, QLatin1Char(' '));
    for (const QVariant &item : list) {
        if (item.typeId() == QMetaType::QVariantMap) {
            lines << pad + QStringLiteral("-");
            appendYamlMap(lines, item.toMap(), indent + 2);
        } else if (item.typeId() == QMetaType::QVariantList) {
            lines << pad + QStringLiteral("-");
            appendYamlList(lines, item.toList(), indent + 2);
        } else {
            lines << pad + QStringLiteral("- ") + yamlScalar(item);
        }
    }
}

void appendYamlValue(QStringList &lines, const QVariant &value, int indent)
{
    if (value.typeId() == QMetaType::QVariantMap) {
        appendYamlMap(lines, value.toMap(), indent);
    } else if (value.typeId() == QMetaType::QVariantList) {
        appendYamlList(lines, value.toList(), indent);
    } else {
        lines << QString(indent, QLatin1Char(' ')) + yamlScalar(value);
    }
}

QString modelYamlText(const QVariantMap &modelYaml)
{
    QStringList lines;
    QVariantMap remaining = modelYaml;
    const QStringList orderedKeys = {
        QStringLiteral("model"),
        QStringLiteral("base"),
        QStringLiteral("metadataOverrides"),
        QStringLiteral("config"),
        QStringLiteral("customFields"),
        QStringLiteral("suggestions")
    };

    for (const QString &key : orderedKeys) {
        if (!remaining.contains(key)) continue;
        const QVariant value = remaining.take(key);
        if (value.typeId() == QMetaType::QVariantMap || value.typeId() == QMetaType::QVariantList) {
            lines << key + QStringLiteral(":");
            appendYamlValue(lines, value, 2);
        } else {
            lines << key + QStringLiteral(": ") + yamlScalar(value);
        }
    }
    appendYamlMap(lines, remaining, 0);
    return lines.join(QStringLiteral("\n")) + QStringLiteral("\n");
}

QVariantMap virtualModelMetadata(const QVariantMap &family)
{
    return {
        {QStringLiteral("kind"), QStringLiteral("modelFile")},
        {QStringLiteral("familyId"), family.value(QStringLiteral("id")).toString()},
        {QStringLiteral("virtualModelId"), family.value(QStringLiteral("modelId")).toString()},
        {QStringLiteral("modelYaml"), family.value(QStringLiteral("modelYaml")).toMap()},
        {QStringLiteral("hubFiles"), family.value(QStringLiteral("hubFiles")).toMap()}
    };
}

bool writeTextFile(const QString &path, const QByteArray &content, QIODevice::OpenMode mode = QIODevice::Text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | mode)) {
        return false;
    }
    file.write(content);
    file.close();
    return true;
}

bool writeVirtualModelFilesToDisk(ModelManager *models, const QVariantMap &metadata, QString *errorMessage = nullptr)
{
    if (!models) {
        if (errorMessage) *errorMessage = QStringLiteral("Model manager is not available");
        return false;
    }

    const QString virtualModelId = metadata.value(QStringLiteral("virtualModelId")).toString();
    const QVariantMap modelYaml = metadata.value(QStringLiteral("modelYaml")).toMap();
    const QVariantMap hubFiles = metadata.value(QStringLiteral("hubFiles")).toMap();
    if (virtualModelId.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Virtual model id is empty");
        return false;
    }
    if (modelYaml.isEmpty() && hubFiles.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Virtual model metadata is empty");
        return false;
    }

    const QString virtualDir = models->virtualModelDir(virtualModelId);
    if (!QDir().mkpath(virtualDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create virtual model directory: %1").arg(virtualDir);
        }
        return false;
    }

    bool ok = true;

    if (!modelYaml.isEmpty()) {
        if (!writeTextFile(QDir(virtualDir).absoluteFilePath(QStringLiteral("model.yaml")),
                           modelYamlText(modelYaml).toUtf8())) {
            ok = false;
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Failed to write virtual model.yaml for %1").arg(virtualModelId));
        }
    }

    const QVariantMap manifest = hubFiles.value(QStringLiteral("manifest")).toMap();
    if (!manifest.isEmpty()) {
        const QJsonDocument doc(QJsonObject::fromVariantMap(manifest));
        if (!writeTextFile(QDir(virtualDir).absoluteFilePath(QStringLiteral("manifest.json")), doc.toJson(QJsonDocument::Indented))) {
            ok = false;
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Failed to write virtual manifest.json for %1").arg(virtualModelId));
        }
    }

    const QVariantMap readme = hubFiles.value(QStringLiteral("readme")).toMap();
    const QString readmeContent = readme.value(QStringLiteral("content")).toString();
    if (!readmeContent.isEmpty()) {
        if (!writeTextFile(QDir(virtualDir).absoluteFilePath(QStringLiteral("README.md")), readmeContent.toUtf8())) {
            ok = false;
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Failed to write virtual README.md for %1").arg(virtualModelId));
        }
    }

    const QVariantMap thumbnail = hubFiles.value(QStringLiteral("thumbnail")).toMap();
    const QByteArray thumbnailBytes = QByteArray::fromBase64(thumbnail.value(QStringLiteral("base64")).toString().toLatin1());
    if (!thumbnailBytes.isEmpty()) {
        QString thumbnailFilename = QFileInfo(thumbnail.value(QStringLiteral("filename")).toString()).fileName();
        if (thumbnailFilename.isEmpty()) {
            thumbnailFilename = QStringLiteral("thumbnail.png");
        }
        if (!writeTextFile(QDir(virtualDir).absoluteFilePath(thumbnailFilename),
                           thumbnailBytes,
                           QIODevice::OpenMode())) {
            ok = false;
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Failed to write virtual %1 for %2").arg(thumbnailFilename, virtualModelId));
        }
    }

    if (!ok && errorMessage) {
        *errorMessage = QStringLiteral("Failed to write one or more virtual model files for %1").arg(virtualModelId);
    }
    return ok;
}

QString normalizedSha256(const QVariantMap &metadata)
{
    QString expected = metadata.value(QStringLiteral("sha256")).toString().trimmed();
    if (expected.isEmpty()) {
        expected = metadata.value(QStringLiteral("checksum")).toString().trimmed();
    }
    if (expected.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)) {
        expected = expected.mid(QStringLiteral("sha256:").size()).trimmed();
    }
    return expected.toLower();
}

bool fileMatchesSha256(const QString &path, const QString &expectedSha256, QString *actualSha256)
{
    if (expectedSha256.isEmpty()) {
        if (actualSha256) {
            actualSha256->clear();
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (actualSha256) {
            actualSha256->clear();
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (actualSha256) {
            actualSha256->clear();
        }
        return false;
    }

    const QString actual = QString::fromLatin1(hash.result().toHex());
    if (actualSha256) {
        *actualSha256 = actual;
    }
    return actual.compare(expectedSha256, Qt::CaseInsensitive) == 0;
}

bool mergeDirectoryContents(const QString &sourcePath, const QString &targetPath)
{
    QDir source(sourcePath);
    if (!source.exists() || !QDir().mkpath(targetPath)) return false;

    bool ok = true;
    for (const QFileInfo &entry : source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString target = QDir(targetPath).absoluteFilePath(entry.fileName());
        if (entry.isDir()) {
            ok = mergeDirectoryContents(entry.absoluteFilePath(), target) && ok;
            QDir(entry.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(target);
            ok = QFile::rename(entry.absoluteFilePath(), target) && ok;
        }
    }
    return ok;
}
}


// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "controllers/models/parts/DownloadInstall_ArchiveSafety.cpp"
#include "controllers/models/parts/DownloadInstall_Queue.cpp"
#include "controllers/models/parts/DownloadInstall_PostProcess.cpp"

} // namespace LAStudio
