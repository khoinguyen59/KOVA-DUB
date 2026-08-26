#include "VoiceClonePresetService.h"
#include "core/utils/Logger.h"
#include "core/storage/PathUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace LAStudio {

namespace {
constexpr int kVoiceClonePresetSchemaVersion = 1;
constexpr auto kVoiceClonePresetSchemaKey = "schemaVersion";
constexpr auto kVoiceClonePresetItemsKey = "presets";
}

VoiceClonePresetService::VoiceClonePresetService(QObject *parent)
    : QObject(parent)
{
}

QString VoiceClonePresetService::presetsFilePath() const
{
    return PathUtils::dataDir() + QStringLiteral("/presets/voice_clone_presets.json");
}

QString VoiceClonePresetService::audioStorageDir() const
{
    return PathUtils::dataDir() + QStringLiteral("/presets/voice_clone_refs");
}

QVariantList VoiceClonePresetService::loadAllPresets() const
{
    QVariantList combined;
    QSet<QString> loadedIds;

    // 1. Always load system built-in presets (61 master voices) from bundled resources
    QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/data/presets/voice_clone_presets.json");
    if (!QFile::exists(bundled)) {
#ifdef LASTUDIO_SOURCE_DIR
        const QString sourceBundled = QStringLiteral(LASTUDIO_SOURCE_DIR) + QStringLiteral("/data/presets/voice_clone_presets.json");
        if (QFile::exists(sourceBundled)) bundled = sourceBundled;
#endif
    }

    QFile bundledFile(bundled);
    if (bundledFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(bundledFile.readAll());
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            arr = doc.object().value(QLatin1String(kVoiceClonePresetItemsKey)).toArray();
        }
        for (const QJsonValue &val : arr) {
            if (val.isObject()) {
                QVariantMap map = val.toObject().toVariantMap();
                map.insert(QStringLiteral("isBuiltin"), true);
                map.insert(QStringLiteral("canDelete"), false);
                const QString id = map.value(QStringLiteral("id")).toString();
                if (!id.isEmpty()) {
                    loadedIds.insert(id);
                }
                combined.append(map);
            }
        }
    }

    // 2. Load user-saved custom presets from PathUtils::dataDir() (~/.lastudio/presets/voice_clone_presets.json)
    const QString userFile = presetsFilePath();
    if (QFile::exists(userFile)) {
        QFile file(userFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonArray arr;
            if (doc.isArray()) {
                arr = doc.array();
            } else if (doc.isObject()) {
                arr = doc.object().value(QLatin1String(kVoiceClonePresetItemsKey)).toArray();
            }
            for (const QJsonValue &val : arr) {
                if (val.isObject()) {
                    QVariantMap map = val.toObject().toVariantMap();
                    const QString id = map.value(QStringLiteral("id")).toString();
                    if (!id.isEmpty() && loadedIds.contains(id)) {
                        continue;
                    }
                    map.insert(QStringLiteral("isBuiltin"), false);
                    map.insert(QStringLiteral("canDelete"), true);
                    map.insert(QStringLiteral("isUserPreset"), true);
                    combined.prepend(map);
                }
            }
        }
    }

    return combined;
}

bool VoiceClonePresetService::saveAllPresets(const QVariantList &presets)
{
    const QString path = presetsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("VoiceClonePresetService", "Failed to write presets file: " + path);
        emit errorOccurred(QStringLiteral("Failed to save voice clone presets locally."));
        return false;
    }

    QJsonArray arr;
    for (const QVariant &item : presets) {
        QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("isBuiltin")).toBool() || !map.value(QStringLiteral("canDelete"), true).toBool()) {
            continue;
        }
        arr.append(QJsonObject::fromVariantMap(map));
    }

    QJsonObject root;
    root.insert(QLatin1String(kVoiceClonePresetSchemaKey), kVoiceClonePresetSchemaVersion);
    root.insert(QLatin1String(kVoiceClonePresetItemsKey), arr);
    const QByteArray document = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(document) != document.size() || !file.commit()) {
        Logger::error("VoiceClonePresetService", "Failed to atomically commit presets file: " + path);
        emit errorOccurred(QStringLiteral("Failed to save voice clone presets locally."));
        return false;
    }
    return true;
}

QVariantMap VoiceClonePresetService::persistReferenceAudio(const QString &id, const QString &audioPath)
{
    const QString sourcePath = PathUtils::urlToLocalPath(audioPath);
    const QFileInfo sourceInfo(sourcePath);
    if (sourcePath.isEmpty() || !sourceInfo.isFile() || sourceInfo.size() <= 0) {
        emit errorOccurred(QStringLiteral("Reference audio file was not found."));
        return {};
    }

    QDir().mkpath(audioStorageDir());

    QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = QStringLiteral("wav");
    }

    // A new owned filename makes an update crash-safe: old media stays in
    // place until the new metadata document has committed successfully.
    const QString destPath = audioStorageDir() + QStringLiteral("/") + id
        + QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces)
        + QStringLiteral(".") + suffix;
    QFile source(sourcePath);
    QSaveFile destination(destPath);
    if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly)) {
        Logger::error("VoiceClonePresetService", "Failed to open reference audio for safe import.");
        emit errorOccurred(QStringLiteral("Failed to save reference audio locally."));
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 bytesWritten = 0;
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && source.error() != QFile::NoError) {
            destination.cancelWriting();
            emit errorOccurred(QStringLiteral("Failed while reading reference audio for import."));
            return {};
        }
        hash.addData(chunk);
        if (destination.write(chunk) != chunk.size()) {
            destination.cancelWriting();
            emit errorOccurred(QStringLiteral("Failed to save reference audio locally."));
            return {};
        }
        bytesWritten += chunk.size();
    }
    if (bytesWritten <= 0 || !destination.commit()) {
        emit errorOccurred(QStringLiteral("Failed to save reference audio locally."));
        return {};
    }
    return {{QStringLiteral("audioPath"), destPath},
            {QStringLiteral("referenceSha256"), QString::fromLatin1(hash.result().toHex())},
            {QStringLiteral("referenceBytes"), bytesWritten},
            {QStringLiteral("storageVersion"), 1}};
}

bool VoiceClonePresetService::isStoredReferenceAudio(const QString &audioPath) const
{
    const QString localPath = PathUtils::urlToLocalPath(audioPath);
    if (localPath.isEmpty()) return false;
    const QString storagePath = QDir::cleanPath(QFileInfo(audioStorageDir()).absoluteFilePath());
    const QString absolutePath = QDir::cleanPath(QFileInfo(localPath).absoluteFilePath());
    if (!absolutePath.isEmpty() && absolutePath.startsWith(storagePath + QLatin1Char('/')))
        return true;
    const QString bundledPath = QDir::cleanPath(QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/data/presets/voice_clone_refs")).absoluteFilePath());
    if (!absolutePath.isEmpty() && (absolutePath == bundledPath || absolutePath.startsWith(bundledPath + QLatin1Char('/'))))
        return true;
#ifdef LASTUDIO_SOURCE_DIR
    const QString sourceBundled = QDir::cleanPath(QFileInfo(QStringLiteral(LASTUDIO_SOURCE_DIR) + QStringLiteral("/data/presets/voice_clone_refs")).absoluteFilePath());
    if (!absolutePath.isEmpty() && (absolutePath == sourceBundled || absolutePath.startsWith(sourceBundled + QLatin1Char('/'))))
        return true;
#endif
    return false;
}

QVariantMap VoiceClonePresetService::validatePreset(const QVariantMap &preset) const
{
    QVariantMap result = preset;
    QString audioPathStr = preset.value(QStringLiteral("audioPath")).toString();
    if (audioPathStr.isEmpty()) {
        audioPathStr = preset.value(QStringLiteral("referenceAudio")).toString();
    }
    if (audioPathStr.isEmpty()) {
        audioPathStr = preset.value(QStringLiteral("refAudio")).toString();
    }
    QString audioPath = PathUtils::urlToLocalPath(audioPathStr);
    if (!QFileInfo(audioPath).isAbsolute() || !QFile::exists(audioPath)) {
        const QString fileName = QFileInfo(audioPath).fileName();
        const QString candidateInStorage = QDir(audioStorageDir()).filePath(fileName);
        if (QFile::exists(candidateInStorage)) {
            audioPath = candidateInStorage;
        } else {
            const QString candidateInBundled = QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/data/presets/voice_clone_refs")).filePath(fileName);
            if (QFile::exists(candidateInBundled)) {
                audioPath = candidateInBundled;
            }
#ifdef LASTUDIO_SOURCE_DIR
            else {
                const QString candidateInSource = QDir(QStringLiteral(LASTUDIO_SOURCE_DIR) + QStringLiteral("/data/presets/voice_clone_refs")).filePath(fileName);
                if (QFile::exists(candidateInSource)) {
                    audioPath = candidateInSource;
                }
            }
#endif
        }
    }
    const QFileInfo info(audioPath);
    QString error;
    if (!isStoredReferenceAudio(audioPath)) {
        error = QStringLiteral("The reference audio is not an LA Studio managed file.");
    } else if (!info.isFile() || info.size() <= 0) {
        error = QStringLiteral("The stored reference audio is missing or empty.");
    } else {
        const QString expectedHash = preset.value(QStringLiteral("referenceSha256")).toString().trimmed().toLower();
        if (!expectedHash.isEmpty()) {
            QFile file(audioPath);
            if (!file.open(QIODevice::ReadOnly)) {
                error = QStringLiteral("The stored reference audio cannot be read.");
            } else {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                while (!file.atEnd()) hash.addData(file.read(1024 * 1024));
                if (QString::fromLatin1(hash.result().toHex()) != expectedHash)
                    error = QStringLiteral("The stored reference audio is corrupt (checksum mismatch).");
            }
        }
    }
    result.insert(QStringLiteral("audioPath"), audioPath);
    result.insert(QStringLiteral("valid"), error.isEmpty());
    result.insert(QStringLiteral("validationError"), error);
    return result;
}

void VoiceClonePresetService::removeStoredReferenceAudio(const QString &audioPath)
{
    const QString localPath = PathUtils::urlToLocalPath(audioPath);
    if (isStoredReferenceAudio(localPath)) QFile::remove(localPath);
}

QVariantList VoiceClonePresetService::presetsForFamily(const QString &familyId)
{
    const QString targetFamily = familyId.trimmed().toLower();
    QVariantList filtered;
    for (const QVariant &val : loadAllPresets()) {
        const QVariantMap preset = val.toMap();
        QString itemFamily = preset.value(QStringLiteral("familyId")).toString().trimmed().toLower();
        if (itemFamily.isEmpty()) {
            itemFamily = preset.value(QStringLiteral("modelFamily")).toString().trimmed().toLower();
        }
        if (targetFamily.isEmpty() || itemFamily.isEmpty() || itemFamily == targetFamily) {
            filtered.append(validatePreset(preset));
        }
    }
    return filtered;
}

QVariantList VoiceClonePresetService::allPresets()
{
    QVariantList validated;
    for (const QVariant &val : loadAllPresets()) {
        validated.append(validatePreset(val.toMap()));
    }
    return validated;
}

bool VoiceClonePresetService::addPreset(const QString &familyId,
                                        const QString &name,
                                        const QString &audioPath,
                                        const QString &referenceText)
{
    if (familyId.isEmpty() || name.trimmed().isEmpty() || audioPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Voice name and reference audio are required."));
        return false;
    }

    QVariantList all = loadAllPresets();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QVariantMap storedReference = persistReferenceAudio(id, audioPath);
    const QString storedAudioPath = storedReference.value(QStringLiteral("audioPath")).toString();
    if (storedAudioPath.isEmpty()) {
        return false;
    }

    const QString nowStr = QDateTime::currentDateTime().toString(Qt::ISODate);
    QVariantMap preset;
    preset.insert(QStringLiteral("id"), id);
    preset.insert(QStringLiteral("familyId"), familyId);
    preset.insert(QStringLiteral("name"), name.trimmed());
    preset.insert(QStringLiteral("audioPath"), storedAudioPath);
    preset.insert(QStringLiteral("referenceSha256"), storedReference.value(QStringLiteral("referenceSha256")));
    preset.insert(QStringLiteral("referenceBytes"), storedReference.value(QStringLiteral("referenceBytes")));
    preset.insert(QStringLiteral("storageVersion"), storedReference.value(QStringLiteral("storageVersion")));
    preset.insert(QStringLiteral("referenceText"), referenceText.trimmed());
    preset.insert(QStringLiteral("originalAudioName"), QFileInfo(PathUtils::urlToLocalPath(audioPath)).fileName());
    preset.insert(QStringLiteral("createdAt"), nowStr);
    preset.insert(QStringLiteral("updatedAt"), nowStr);

    all.prepend(preset);

    if (saveAllPresets(all)) {
        emit presetsChanged(familyId);
        return true;
    }

    removeStoredReferenceAudio(storedAudioPath);
    return false;
}

bool VoiceClonePresetService::updatePreset(const QString &id,
                                           const QString &name,
                                           const QString &audioPath,
                                           const QString &referenceText)
{
    if (id.isEmpty() || name.trimmed().isEmpty() || audioPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Voice name and reference audio are required."));
        return false;
    }

    QVariantList all = loadAllPresets();
    QString familyId;
    QString oldAudioPath;
    QString replacementAudioPath;
    bool found = false;

    for (int i = 0; i < all.size(); ++i) {
        QVariantMap preset = all[i].toMap();
        if (preset.value(QStringLiteral("id")).toString() != id) {
            continue;
        }

        familyId = preset.value(QStringLiteral("familyId")).toString();
        oldAudioPath = preset.value(QStringLiteral("audioPath")).toString();
        QString storedAudioPath = oldAudioPath;
        QVariantMap newStoredReference;
        if (QFileInfo(PathUtils::urlToLocalPath(audioPath)).absoluteFilePath()
            != QFileInfo(PathUtils::urlToLocalPath(oldAudioPath)).absoluteFilePath()) {
            newStoredReference = persistReferenceAudio(id, audioPath);
            storedAudioPath = newStoredReference.value(QStringLiteral("audioPath")).toString();
            if (storedAudioPath.isEmpty()) {
                return false;
            }
            replacementAudioPath = storedAudioPath;
        }

        preset.insert(QStringLiteral("name"), name.trimmed());
        preset.insert(QStringLiteral("audioPath"), storedAudioPath);
        if (!newStoredReference.isEmpty()) {
            preset.insert(QStringLiteral("referenceSha256"), newStoredReference.value(QStringLiteral("referenceSha256")));
            preset.insert(QStringLiteral("referenceBytes"), newStoredReference.value(QStringLiteral("referenceBytes")));
            preset.insert(QStringLiteral("storageVersion"), newStoredReference.value(QStringLiteral("storageVersion")));
        }
        preset.insert(QStringLiteral("referenceText"), referenceText.trimmed());
        preset.insert(QStringLiteral("originalAudioName"), QFileInfo(PathUtils::urlToLocalPath(audioPath)).fileName());
        preset.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        all[i] = preset;
        found = true;
        break;
    }

    if (!found) {
        emit errorOccurred(QStringLiteral("Voice preset not found."));
        return false;
    }

    if (saveAllPresets(all)) {
        if (!replacementAudioPath.isEmpty() && !oldAudioPath.isEmpty()
            && oldAudioPath != replacementAudioPath) {
            removeStoredReferenceAudio(oldAudioPath);
        }
        emit presetsChanged(familyId);
        return true;
    }
    if (!replacementAudioPath.isEmpty()) removeStoredReferenceAudio(replacementAudioPath);
    return false;
}

bool VoiceClonePresetService::deletePreset(const QString &id)
{
    if (id.isEmpty()) {
        return false;
    }

    QVariantList all = loadAllPresets();
    QVariantList remaining;
    QString familyId;
    QString audioPath;
    bool found = false;

    for (const QVariant &val : all) {
        QVariantMap preset = val.toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            if (preset.value(QStringLiteral("isBuiltin")).toBool() || !preset.value(QStringLiteral("canDelete"), true).toBool()) {
                Logger::warning("VoiceClonePresetService", "Cannot delete system built-in voice preset: " + id);
                return false;
            }
            familyId = preset.value(QStringLiteral("familyId")).toString();
            audioPath = preset.value(QStringLiteral("audioPath")).toString();
            found = true;
        } else {
            remaining.append(preset);
        }
    }

    if (!found) {
        return false;
    }

    if (saveAllPresets(remaining)) {
        removeStoredReferenceAudio(audioPath);
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

} // namespace LAStudio
