#include "dubbing/project/DubbingProject.h"
#include "workflows/session/WorkflowTranscript.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaType>
#include <QSaveFile>
#include <QSet>

namespace LAStudio {

namespace {
void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString serializeAssetPath(const QString &projectPath, const QString &path)
{
    if (path.trimmed().isEmpty() || projectPath.trimmed().isEmpty())
        return path;
    const QFileInfo asset(path);
    if (!asset.isAbsolute())
        return path;
    const QDir root(QFileInfo(projectPath).absolutePath());
    const QString relative = root.relativeFilePath(asset.absoluteFilePath());
    // Never turn an external source into a misleading relative reference.
    if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")))
        return asset.absoluteFilePath();
    return relative;
}

QString deserializeAssetPath(const QString &projectPath, const QString &path)
{
    if (path.trimmed().isEmpty() || QFileInfo(path).isAbsolute()
        || projectPath.trimmed().isEmpty())
        return path;
    return QDir(QFileInfo(projectPath).absolutePath()).absoluteFilePath(path);
}

QVariantMap serializeArtifactMap(const QString &projectPath, QVariantMap value)
{
    static const QSet<QString> pathKeys{
        QStringLiteral("clipPath"), QStringLiteral("path"), QStringLiteral("audioPath"),
        QStringLiteral("audio"), QStringLiteral("video"), QStringLiteral("media"),
        QStringLiteral("vocals"), QStringLiteral("background"), QStringLiteral("preview"),
        QStringLiteral("export"), QStringLiteral("transcript"), QStringLiteral("outputPath")};
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (pathKeys.contains(it.key()) && it.value().metaType().id() == QMetaType::QString)
            it.value() = serializeAssetPath(projectPath, it.value().toString());
        else if (it.value().metaType().id() == QMetaType::QVariantMap)
            it.value() = serializeArtifactMap(projectPath, it.value().toMap());
        else if (it.value().metaType().id() == QMetaType::QVariantList) {
            QVariantList entries = it.value().toList();
            for (QVariant &entry : entries) {
                if (entry.metaType().id() == QMetaType::QVariantMap)
                    entry = serializeArtifactMap(projectPath, entry.toMap());
            }
            it.value() = entries;
        }
    }
    return value;
}

QVariantMap deserializeArtifactMap(const QString &projectPath, QVariantMap value)
{
    static const QSet<QString> pathKeys{
        QStringLiteral("clipPath"), QStringLiteral("path"), QStringLiteral("audioPath"),
        QStringLiteral("audio"), QStringLiteral("video"), QStringLiteral("media"),
        QStringLiteral("vocals"), QStringLiteral("background"), QStringLiteral("preview"),
        QStringLiteral("export"), QStringLiteral("transcript"), QStringLiteral("outputPath")};
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (pathKeys.contains(it.key()) && it.value().metaType().id() == QMetaType::QString)
            it.value() = deserializeAssetPath(projectPath, it.value().toString());
        else if (it.value().metaType().id() == QMetaType::QVariantMap)
            it.value() = deserializeArtifactMap(projectPath, it.value().toMap());
        else if (it.value().metaType().id() == QMetaType::QVariantList) {
            QVariantList entries = it.value().toList();
            for (QVariant &entry : entries) {
                if (entry.metaType().id() == QMetaType::QVariantMap)
                    entry = deserializeArtifactMap(projectPath, entry.toMap());
            }
            it.value() = entries;
        }
    }
    return value;
}
}

bool DubbingProject::mergeSegmentPatches(const QVariantList &segments,
                                         const QVariantList &patches,
                                         QVariantList &merged,
                                         QString *error)
{
    WorkflowTranscriptArtifact base;
    if (!WorkflowTranscriptArtifact::fromVariantList(segments, base, error)) return false;
    WorkflowTranscriptArtifact result;
    if (!WorkflowTranscriptArtifact::mergePatches(base, patches, result, error)) return false;
    merged = result.toVariantList();
    return true;
}

QJsonObject DubbingProject::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), CurrentSchemaVersion);
    json.insert(QStringLiteral("sourceMediaPath"), serializeAssetPath(projectPath, sourceMediaPath));
    json.insert(QStringLiteral("sourceHash"), sourceHash);
    json.insert(QStringLiteral("masterAudioPath"), serializeAssetPath(projectPath, masterAudioPath));
    json.insert(QStringLiteral("analysisAudioPath"), serializeAssetPath(projectPath, analysisAudioPath));
    json.insert(QStringLiteral("vocalsAudioPath"), serializeAssetPath(projectPath, vocalsAudioPath));
    json.insert(QStringLiteral("backgroundAudioPath"), serializeAssetPath(projectPath, backgroundAudioPath));
    json.insert(QStringLiteral("sourceDurationMs"), sourceDurationMs);
    json.insert(QStringLiteral("sourceSampleRate"), sourceSampleRate);
    json.insert(QStringLiteral("sourceChannels"), sourceChannels);
    json.insert(QStringLiteral("sourceIsVideo"), sourceIsVideo);
    json.insert(QStringLiteral("sourceLanguage"), sourceLanguage);
    json.insert(QStringLiteral("targetLanguage"), targetLanguage);
    json.insert(QStringLiteral("dubbingQuality"), dubbingQuality);
    json.insert(QStringLiteral("workflowEntryMode"), workflowEntryMode);
    json.insert(QStringLiteral("ttsVoiceId"),
                ttsVoiceId.isEmpty() ? cloneVoicePresetId : ttsVoiceId);
    json.insert(QStringLiteral("durationControl"), QJsonObject::fromVariantMap(durationControl));
    json.insert(QStringLiteral("workflowNodeConfigurations"),
                QJsonObject::fromVariantMap(workflowNodeConfigurations));
    json.insert(QStringLiteral("transcriptConfiguration"),
                QJsonObject::fromVariantMap(transcriptConfiguration));
    json.insert(QStringLiteral("subtitleConfiguration"),
                QJsonObject::fromVariantMap(subtitleConfiguration));
    json.insert(QStringLiteral("timingConfiguration"),
                QJsonObject::fromVariantMap(timingConfiguration));
    json.insert(QStringLiteral("audioMixConfiguration"),
                QJsonObject::fromVariantMap(audioMixConfiguration));
    json.insert(QStringLiteral("customRewriteConfiguration"),
                QJsonObject::fromVariantMap(customRewriteConfiguration));
    json.insert(QStringLiteral("speakers"), QJsonArray::fromVariantList(speakers));
    QVariantList serializedSegments;
    serializedSegments.reserve(segments.size());
    for (const QVariant &segment : segments)
        serializedSegments.append(serializeArtifactMap(projectPath, segment.toMap()));
    json.insert(QStringLiteral("segments"), QJsonArray::fromVariantList(serializedSegments));
    json.insert(QStringLiteral("workflowCurrentStepId"), workflowCurrentStepId);
    json.insert(QStringLiteral("workflowLastCompletedStepId"), workflowLastCompletedStepId);
    json.insert(QStringLiteral("workflowStepOutputs"),
                QJsonObject::fromVariantMap(serializeArtifactMap(projectPath, workflowStepOutputs)));
    json.insert(QStringLiteral("previewAudioPath"), serializeAssetPath(projectPath, previewAudioPath));
    json.insert(QStringLiteral("dubbedVocalAudioPath"), serializeAssetPath(projectPath, dubbedVocalAudioPath));
    json.insert(QStringLiteral("exportMediaPath"), serializeAssetPath(projectPath, exportMediaPath));
    json.insert(QStringLiteral("capCutDraftPath"), serializeAssetPath(projectPath, capCutDraftPath));
    return json;
}

bool DubbingProject::fromJson(const QJsonObject &json, DubbingProject &project, QString *error)
{
    const int version = json.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version < 1 || version > CurrentSchemaVersion) {
        setError(error, QStringLiteral("Unsupported dubbing project schema version: %1").arg(version));
        return false;
    }

    // Clear fields introduced by newer schemas before loading so callers may
    // safely reuse a DubbingProject instance for more than one file.  This is
    // also important for a legacy project: version-gated fields are not read
    // below, so leaving their previous values in place would silently merge
    // the old project with whichever newer project was loaded first.
    project.workflowEntryMode.clear();
    project.ttsVoiceId.clear();
    project.cloneVoicePresetId.clear();
    project.workflowNodeConfigurations.clear();
    project.transcriptConfiguration.clear();
    project.subtitleConfiguration.clear();
    project.timingConfiguration.clear();
    project.audioMixConfiguration.clear();
    project.customRewriteConfiguration.clear();
    project.workflowCurrentStepId.clear();
    project.workflowLastCompletedStepId.clear();
    project.workflowStepOutputs.clear();
    project.previewAudioPath.clear();
    project.dubbedVocalAudioPath.clear();
    project.exportMediaPath.clear();
    project.capCutDraftPath.clear();

    project.sourceMediaPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("sourceMediaPath")).toString());
    project.sourceHash = json.value(QStringLiteral("sourceHash")).toString();
    project.masterAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("masterAudioPath")).toString());
    project.analysisAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("analysisAudioPath")).toString());
    if (version >= 14)
        project.vocalsAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("vocalsAudioPath")).toString());
    else
        project.vocalsAudioPath.clear();
    project.backgroundAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("backgroundAudioPath")).toString());
    project.sourceDurationMs = json.value(QStringLiteral("sourceDurationMs")).toVariant().toLongLong();
    project.sourceSampleRate = json.value(QStringLiteral("sourceSampleRate")).toInt();
    project.sourceChannels = json.value(QStringLiteral("sourceChannels")).toInt();
    project.sourceIsVideo = json.value(QStringLiteral("sourceIsVideo")).toBool();
    project.sourceLanguage = json.value(QStringLiteral("sourceLanguage")).toString(QStringLiteral("zh"));
    project.targetLanguage = json.value(QStringLiteral("targetLanguage")).toString(QStringLiteral("vi"));
    project.dubbingQuality = json.value(QStringLiteral("dubbingQuality")).toString(QStringLiteral("adaptive"));
    if (project.dubbingQuality != QStringLiteral("adaptive")
        && project.dubbingQuality != QStringLiteral("custom"))
        project.dubbingQuality = QStringLiteral("fast");
    if (version >= 13) {
        const QString entryMode = json.value(QStringLiteral("workflowEntryMode")).toString().trimmed().toLower();
        if (entryMode == QStringLiteral("automatic") || entryMode == QStringLiteral("step"))
            project.workflowEntryMode = entryMode;
    }
    // Schema 12 renames the project decision from the implementation-specific
    // clone preset to a TTS voice.  Preserve every existing selection on load
    // but write only ttsVoiceId on the next save.
    if (version >= 12)
        project.ttsVoiceId = json.value(QStringLiteral("ttsVoiceId")).toString().trimmed();
    else if (version >= 8)
        project.ttsVoiceId = json.value(QStringLiteral("cloneVoicePresetId")).toString().trimmed();
    project.cloneVoicePresetId = project.ttsVoiceId;
    project.durationControl = json.value(QStringLiteral("durationControl")).toObject().toVariantMap();
    if (project.durationControl.isEmpty()) {
        project.durationControl = QVariantMap{{QStringLiteral("enabled"), version >= 3},
                                              {QStringLiteral("unit"), QStringLiteral("phoneme-v1")},
                                              {QStringLiteral("lowerToleranceRatio"), 0.20},
                                              {QStringLiteral("upperToleranceRatio"), 0.20},
                                              {QStringLiteral("autoRewrite"), true}};
    } else if (version < 4) {
        // Schema 4 separates faithful translation from length adaptation. The old
        // opt-in controlled a different single-pass/outside-tolerance workflow.
        project.durationControl.insert(QStringLiteral("autoRewrite"), true);
    }
    if (version >= 6) {
        project.workflowNodeConfigurations =
            json.value(QStringLiteral("workflowNodeConfigurations")).toObject().toVariantMap();
    }
    if (version >= 7) {
        project.customRewriteConfiguration =
            json.value(QStringLiteral("customRewriteConfiguration")).toObject().toVariantMap();
    }
    if (version >= 9) {
        project.transcriptConfiguration =
            json.value(QStringLiteral("transcriptConfiguration")).toObject().toVariantMap();
    }
    if (project.transcriptConfiguration.isEmpty()) {
        project.transcriptConfiguration = {{QStringLiteral("transcriptSource"), QStringLiteral("stt")},
                                           {QStringLiteral("fusionPolicy"), QStringLiteral("prefer-ocr")}};
    } else if (!project.transcriptConfiguration.contains(QStringLiteral("fusionPolicy"))) {
        project.transcriptConfiguration.insert(QStringLiteral("fusionPolicy"), QStringLiteral("prefer-ocr"));
    }
    if (version >= 10) {
        project.subtitleConfiguration =
            json.value(QStringLiteral("subtitleConfiguration")).toObject().toVariantMap();
    }
    if (version >= 11) {
        project.timingConfiguration =
            json.value(QStringLiteral("timingConfiguration")).toObject().toVariantMap();
    }
    if (version >= 15) {
        project.audioMixConfiguration =
            json.value(QStringLiteral("audioMixConfiguration")).toObject().toVariantMap();
    }
    if (project.audioMixConfiguration.isEmpty()) {
        project.audioMixConfiguration = QVariantMap{{QStringLiteral("originalGainPercent"), 0},
                                                     {QStringLiteral("dubbedGainPercent"), 100}};
    }
    project.speakers = json.value(QStringLiteral("speakers")).toArray().toVariantList();
    project.segments = json.value(QStringLiteral("segments")).toArray().toVariantList();
    for (QVariant &segment : project.segments)
        segment = deserializeArtifactMap(project.projectPath, segment.toMap());
    if (version >= 16) {
        project.workflowCurrentStepId = json.value(QStringLiteral("workflowCurrentStepId"))
            .toString().trimmed();
        project.workflowLastCompletedStepId = json.value(QStringLiteral("workflowLastCompletedStepId"))
            .toString().trimmed();
        project.workflowStepOutputs = deserializeArtifactMap(project.projectPath,
            json.value(QStringLiteral("workflowStepOutputs")).toObject().toVariantMap());
        project.previewAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("previewAudioPath")).toString());
        project.dubbedVocalAudioPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("dubbedVocalAudioPath")).toString());
        project.exportMediaPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("exportMediaPath")).toString());
        project.capCutDraftPath = deserializeAssetPath(project.projectPath, json.value(QStringLiteral("capCutDraftPath")).toString());
    }
    return true;
}

bool DubbingProject::save(QString *error) const
{
    if (projectPath.isEmpty()) {
        setError(error, QStringLiteral("Dubbing project path is empty."));
        return false;
    }

    const QFileInfo info(projectPath);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(error, QStringLiteral("Cannot create project directory: %1").arg(info.absolutePath()));
        return false;
    }

    QSaveFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Cannot write dubbing project: %1").arg(file.errorString()));
        return false;
    }
    const QByteArray payload = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        setError(error, QStringLiteral("Cannot write dubbing project: %1").arg(file.errorString()));
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Cannot commit dubbing project: %1").arg(file.errorString()));
        return false;
    }
    return true;
}

bool DubbingProject::load(const QString &path, DubbingProject &project, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open dubbing project: %1").arg(file.errorString()));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid dubbing project JSON: %1").arg(parseError.errorString()));
        return false;
    }

    DubbingProject loaded;
    // `fromJson` needs the root to resolve schema 17 relative artifact paths.
    loaded.projectPath = QFileInfo(path).absoluteFilePath();
    if (!fromJson(document.object(), loaded, error)) {
        return false;
    }
    loaded.projectPath = QFileInfo(path).absoluteFilePath();
    project = std::move(loaded);
    return true;
}

} // namespace LAStudio
