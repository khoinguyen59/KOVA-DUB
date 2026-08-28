#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace LAStudio {

// These keys are internal to the Dubbing -> TTS execution boundary. They are
// deliberately not model-schema settings and must never be exposed as editor
// controls or copied into a project file.
inline constexpr auto kTtsSavedVoiceId = "_lastudioSavedVoiceId";
inline constexpr auto kTtsSavedVoiceReferencePath = "_lastudioSavedVoiceReferencePath";
inline constexpr auto kTtsSavedVoiceReferenceText = "_lastudioSavedVoiceReferenceText";

inline bool isTtsSavedVoiceProfileSetting(const QString &key)
{
    return key == QLatin1String(kTtsSavedVoiceId)
        || key == QLatin1String(kTtsSavedVoiceReferencePath)
        || key == QLatin1String(kTtsSavedVoiceReferenceText);
}

// Qwen3-TTS owns a persistent profile primitive which is still used by the
// ordinary saved-profile path. Dubbing also supports reference-per-request
// cloning for the zero-shot backends below; that path is intentionally kept
// separate so a saved voice can be used without pretending every backend has
// the same profile ABI.
inline bool localTtsSupportsSavedVoiceProfile(const QVariantMap &familyConfig)
{
    const QString identity = QStringList{
        familyConfig.value(QStringLiteral("backend")).toString(),
        familyConfig.value(QStringLiteral("id")).toString(),
        familyConfig.value(QStringLiteral("familyId")).toString(),
        familyConfig.value(QStringLiteral("modelId")).toString(),
        familyConfig.value(QStringLiteral("model")).toString()
    }.join(QLatin1Char('|'));
    return identity.contains(QStringLiteral("qwen3-tts"), Qt::CaseInsensitive);
}

inline bool localTtsSupportsReferenceClone(const QVariantMap &familyConfig)
{
    const QString identity = QStringList{
        familyConfig.value(QStringLiteral("backend")).toString(),
        familyConfig.value(QStringLiteral("id")).toString(),
        familyConfig.value(QStringLiteral("familyId")).toString(),
        familyConfig.value(QStringLiteral("modelId")).toString(),
        familyConfig.value(QStringLiteral("model")).toString()
    }.join(QLatin1Char('|'));
    return identity.contains(QStringLiteral("qwen3-tts"), Qt::CaseInsensitive)
        || identity.contains(QStringLiteral("vieneu"), Qt::CaseInsensitive)
        || identity.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive);
}

// A saved reference can be used by both universal targets, but the loaded
// local runtime must still be the exact target selected by the user. Without
// this check a project could select OmniVoice while an older VieNeu session
// remained loaded and the request would silently run on the wrong backend.
inline bool localTtsMatchesReferenceCloneTarget(const QVariantMap &familyConfig,
                                                const QString &targetModel)
{
    const QString target = targetModel.trimmed().toLower();
    if (target.isEmpty()) return true;

    const QStringList identities{
        familyConfig.value(QStringLiteral("id")).toString().trimmed().toLower(),
        familyConfig.value(QStringLiteral("familyId")).toString().trimmed().toLower(),
        familyConfig.value(QStringLiteral("modelId")).toString().trimmed().toLower(),
        familyConfig.value(QStringLiteral("model")).toString().trimmed().toLower(),
        familyConfig.value(QStringLiteral("backend")).toString().trimmed().toLower()
    };
    const auto containsExactOrQualified = [&identities](const QString &value) {
        for (const QString &identity : identities) {
            if (identity == value || identity.contains(value)) return true;
        }
        return false;
    };

    if (target == QStringLiteral("omnivoice"))
        return containsExactOrQualified(QStringLiteral("omnivoice"));
    if (target.startsWith(QStringLiteral("vieneu-tts-v")))
        return containsExactOrQualified(target);
    if (target.startsWith(QStringLiteral("qwen3-tts")))
        return containsExactOrQualified(target);
    return containsExactOrQualified(target);
}

} // namespace LAStudio
