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
#include <QStringList>
#include <QUuid>

namespace LAStudio {

namespace {
constexpr int kVoiceClonePresetSchemaVersion = 2;
constexpr auto kVoiceClonePresetSchemaKey = "schemaVersion";
constexpr auto kVoiceClonePresetItemsKey = "presets";
constexpr auto kVieNeuTarget = "vieneu";
constexpr auto kOmniVoiceTarget = "omnivoice";

QStringList canonicalVoiceTargets()
{
    return {QString::fromLatin1(kVieNeuTarget), QString::fromLatin1(kOmniVoiceTarget)};
}

QVariantList canonicalVoiceTargetVariants()
{
    QVariantList targets;
    for (const QString &target : canonicalVoiceTargets())
        targets.append(target);
    return targets;
}

QString targetModelId(const QString &target)
{
    return target.compare(QString::fromLatin1(kVieNeuTarget), Qt::CaseInsensitive) == 0
        ? QStringLiteral("vieneu-tts-v3-turbo")
        : QStringLiteral("omnivoice");
}

bool isCanonicalVoiceTarget(const QString &value)
{
    return value.compare(QString::fromLatin1(kVieNeuTarget), Qt::CaseInsensitive) == 0
        || value.compare(QString::fromLatin1(kOmniVoiceTarget), Qt::CaseInsensitive) == 0
        || value.startsWith(QStringLiteral("vieneu-tts"), Qt::CaseInsensitive);
}
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

QVariantMap VoiceClonePresetService::normalizePresetMetadata(const QVariantMap &input) const
{
    QVariantMap preset = input;
    QString familyId = preset.value(QStringLiteral("familyId")).toString().trimmed().toLower();
    if (familyId.isEmpty())
        familyId = preset.value(QStringLiteral("modelFamily")).toString().trimmed().toLower();
    if (familyId.isEmpty())
        familyId = QStringLiteral("omnivoice");

    QString name = preset.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        name = preset.value(QStringLiteral("displayName")).toString().trimmed();
    if (name.isEmpty())
        name = preset.value(QStringLiteral("id")).toString().trimmed();

    QString referenceText = preset.value(QStringLiteral("referenceText")).toString();
    if (referenceText.trimmed().isEmpty())
        referenceText = preset.value(QStringLiteral("referenceTranscript")).toString();

    QVariantList compatibleFamilies = preset.value(
        QStringLiteral("compatibleModelFamilies")).toList();
    auto addCompatibleFamily = [&compatibleFamilies](const QString &family) {
        if (family.isEmpty()) return;
        for (const QVariant &entry : compatibleFamilies) {
            if (entry.toString().trimmed().toLower() == family) return;
        }
        compatibleFamilies.append(family);
    };
    addCompatibleFamily(familyId);
    // Source family is provenance only. Every reference-backed voice can be
    // enrolled for either supported target; the actual runtime/worker health
    // is evaluated later by validatePreset() and the execution controller.
    addCompatibleFamily(QString::fromLatin1(kVieNeuTarget));
    addCompatibleFamily(QString::fromLatin1(kOmniVoiceTarget));

    QVariantList voiceTargets;
    const QVariantList storedTargets = preset.value(
        QStringLiteral("voiceModelTargets")).toList();
    for (const QVariant &entry : storedTargets) {
        const QString target = entry.toString().trimmed().toLower();
        if (!target.isEmpty() && isCanonicalVoiceTarget(target)
            && !voiceTargets.contains(target)) {
            voiceTargets.append(target == QStringLiteral("vieneu-tts-v3-turbo")
                                    ? QString::fromLatin1(kVieNeuTarget) : target);
        }
    }
    for (const QString &target : canonicalVoiceTargets()) {
        if (!voiceTargets.contains(target)) voiceTargets.append(target);
    }

    QVariantMap targetBindings = preset.value(
        QStringLiteral("targetBindings")).toMap();
    const QString audioPath = preset.value(QStringLiteral("audioPath"),
                                            preset.value(QStringLiteral("referenceAudio")))
                                  .toString().trimmed();
    for (const QString &target : canonicalVoiceTargets()) {
        QVariantMap binding = targetBindings.value(target).toMap();
        binding.insert(QStringLiteral("target"), target);
        binding.insert(QStringLiteral("modelId"), targetModelId(target));
        binding.insert(QStringLiteral("referenceAudio"), audioPath);
        binding.insert(QStringLiteral("referenceText"), referenceText);
        if (binding.value(QStringLiteral("status")).toString().trimmed().isEmpty())
            binding.insert(QStringLiteral("status"), QStringLiteral("pending-validation"));
        targetBindings.insert(target, binding);
    }

    preset.insert(QStringLiteral("displayName"), name);
    preset.insert(QStringLiteral("familyId"), familyId);
    preset.insert(QStringLiteral("category"), preset.value(
        QStringLiteral("category"), familyId).toString().trimmed());
    preset.insert(QStringLiteral("language"), preset.value(
        QStringLiteral("language"), QStringLiteral("auto")).toString().trimmed());
    preset.insert(QStringLiteral("referenceText"), referenceText);
    preset.insert(QStringLiteral("compatibleModelFamilies"), compatibleFamilies);
    preset.insert(QStringLiteral("voiceModelTargets"), voiceTargets);
    preset.insert(QStringLiteral("targetBindings"), targetBindings);
    preset.insert(QStringLiteral("isCustomVoice"),
                  preset.value(QStringLiteral("isUserPreset")).toBool()
                      || !preset.value(QStringLiteral("isBuiltin"), false).toBool());
    return preset;
}

QVariantList VoiceClonePresetService::loadAllPresets() const
{
    QVariantList combined;
    QSet<QString> loadedIds;

    // 1. Always load the system built-in catalog from bundled resources.  The
    // UI derives counts from the records that actually load successfully.
    QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/data/presets/voice_clone_presets.json");
    if (!QFile::exists(bundled)) {
#ifdef LASTUDIO_SOURCE_DIR
        const QString sourceBundled = QStringLiteral(LASTUDIO_SOURCE_DIR) + QStringLiteral("/data/presets/voice_clone_presets.json");
        if (QFile::exists(sourceBundled)) bundled = sourceBundled;
#endif
    }

    auto resolveAudioPath = [](const QString &path) -> QString {
        if (path.isEmpty()) return QString();
        const QString local = PathUtils::urlToLocalPath(path);
        if (QFileInfo(local).isAbsolute() && QFile::exists(local)) {
            return local;
        }
        const QString filename = QFileInfo(local).fileName();
        if (filename.isEmpty()) return local;

        // Prefer the installed, app-owned copy.  It is present in portable
        // packages and is covered by the managed-reference allow-list below.
        const QString appManagedVoices = QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/presets/voice_clone_refs/") + filename;
        if (QFile::exists(appManagedVoices)) return appManagedVoices;

        const QString appVoices = QCoreApplication::applicationDirPath()
            + QStringLiteral("/resources/voices/") + filename;
        if (QFile::exists(appVoices)) return appVoices;

#ifdef LASTUDIO_SOURCE_DIR
        const QString sourceManagedVoices = QStringLiteral(LASTUDIO_SOURCE_DIR)
            + QStringLiteral("/data/presets/voice_clone_refs/") + filename;
        if (QFile::exists(sourceManagedVoices)) return sourceManagedVoices;

        const QString srcVoices = QStringLiteral(LASTUDIO_SOURCE_DIR)
            + QStringLiteral("/resources/voices/") + filename;
        if (QFile::exists(srcVoices)) return srcVoices;

        const QString pkgVoices = QStringLiteral(LASTUDIO_SOURCE_DIR) + QStringLiteral("/vietnamese_voices_package/audio/") + filename;
        if (QFile::exists(pkgVoices)) return pkgVoices;
#endif

        const QString dataVoices = PathUtils::dataDir() + QStringLiteral("/presets/voice_clone_refs/") + filename;
        if (QFile::exists(dataVoices)) return dataVoices;

        return appVoices;
    };

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

                const QString rawAudio = map.value(QStringLiteral("audioPath")).toString();
                const QString resolvedAudio = resolveAudioPath(rawAudio);
                if (!resolvedAudio.isEmpty()) {
                    map.insert(QStringLiteral("audioPath"), resolvedAudio);
                    map.insert(QStringLiteral("referenceAudio"), resolvedAudio);
                }

                map = normalizePresetMetadata(map);

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

                    const QString rawAudio = map.value(QStringLiteral("audioPath")).toString();
                    const QString resolvedAudio = resolveAudioPath(rawAudio);
                    if (!resolvedAudio.isEmpty()) {
                        map.insert(QStringLiteral("audioPath"), resolvedAudio);
                        map.insert(QStringLiteral("referenceAudio"), resolvedAudio);
                    }

                    map = normalizePresetMetadata(map);

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

#if defined(Q_OS_WIN)
    const auto cs = Qt::CaseInsensitive;
#else
    const auto cs = Qt::CaseSensitive;
#endif

    const auto isWithin = [&absolutePath, cs](const QString &root) {
        const QString normalizedRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        return !absolutePath.isEmpty()
            && (absolutePath.compare(normalizedRoot, cs) == 0
                || absolutePath.startsWith(normalizedRoot + QLatin1Char('/'), cs));
    };

    // User-imported references and the read-only references shipped with the
    // voice catalog are both safe clone inputs.  The latter must be accepted
    // here; otherwise every bundled voice displays in the gallery but fails at
    // the moment a user tries to clone it.
    if (isWithin(storagePath)
        || isWithin(QCoreApplication::applicationDirPath()
                    + QStringLiteral("/data/presets/voice_clone_refs"))
        || isWithin(QCoreApplication::applicationDirPath()
                    + QStringLiteral("/resources/voices")))
        return true;
#ifdef LASTUDIO_SOURCE_DIR
    if (isWithin(QStringLiteral(LASTUDIO_SOURCE_DIR)
                 + QStringLiteral("/data/presets/voice_clone_refs"))
        || isWithin(QStringLiteral(LASTUDIO_SOURCE_DIR)
                    + QStringLiteral("/resources/voices"))
        || isWithin(QStringLiteral(LASTUDIO_SOURCE_DIR)
                    + QStringLiteral("/vietnamese_voices_package/audio")))
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

    QVariantMap targetBindings = result.value(QStringLiteral("targetBindings")).toMap();
    for (const QString &target : canonicalVoiceTargets()) {
        QVariantMap binding = targetBindings.value(target).toMap();
        binding.insert(QStringLiteral("target"), target);
        binding.insert(QStringLiteral("modelId"), targetModelId(target));
        binding.insert(QStringLiteral("referenceAudio"), audioPath);
        binding.insert(QStringLiteral("referenceText"), result.value(
            QStringLiteral("referenceText")).toString());
        binding.insert(QStringLiteral("valid"), error.isEmpty());
        binding.insert(QStringLiteral("status"), error.isEmpty()
            ? QStringLiteral("reference-ready") : QStringLiteral("unavailable"));
        binding.insert(QStringLiteral("error"), error);
        targetBindings.insert(target, binding);
    }
    result.insert(QStringLiteral("voiceModelTargets"), canonicalVoiceTargetVariants());
    result.insert(QStringLiteral("targetBindings"), targetBindings);
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
    const bool targetLookup = targetFamily == QStringLiteral("vieneu")
        || targetFamily.startsWith(QStringLiteral("vieneu-tts"))
        || targetFamily == QStringLiteral("omnivoice");
    QVariantList filtered;
    for (const QVariant &val : loadAllPresets()) {
        const QVariantMap preset = val.toMap();
        if (targetLookup) {
            const QVariantList targets = preset.value(
                QStringLiteral("voiceModelTargets")).toList();
            const bool vieNeuTarget = targetFamily == QStringLiteral("vieneu")
                || targetFamily.startsWith(QStringLiteral("vieneu-tts"));
            const QString canonicalTarget = vieNeuTarget
                ? QString::fromLatin1(kVieNeuTarget)
                : QString::fromLatin1(kOmniVoiceTarget);
            bool supportsTarget = targets.contains(canonicalTarget);
            if (!supportsTarget) continue;
            filtered.append(validatePreset(preset));
            continue;
        }
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
