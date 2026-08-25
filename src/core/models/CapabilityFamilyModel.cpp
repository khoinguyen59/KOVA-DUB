#include "CapabilityFamilyModel.h"
#include "core/models/RegistryManager.h"
#include "core/models/ModelManager.h"
#include "core/models/RuntimeManager.h"
#include "core/hardware/HardwareManager.h"
#include "core/storage/Settings.h"
#include "core/storage/PathUtils.h"
#include "core/utils/Logger.h"
#include "core/storage/StudioSelectionRepository.h"
#include "core/services/StudioCapabilityRegistry.h"
#include "controllers/app/AppController.h"
#include <algorithm>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QLocale>
#include <QVersionNumber>
#include <limits>
#include <utility>

namespace LAStudio {

namespace {

QVariantList asList(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return {};
    if (value.typeId() == QMetaType::QVariantList || value.typeId() == QMetaType::QStringList)
        return value.toList();
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? QVariantList{} : QVariantList{text};
}

QStringList uniqueStrings(const QVariantList &values)
{
    QStringList out;
    QSet<QString> seen;
    for (const QVariant &value : values) {
        QString text = value.toString().trimmed();
        if (text.isEmpty())
            continue;
        const QString key = text.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        out.append(text);
    }
    return out;
}

QString displayToken(const QString &value)
{
    const QString text = value.trimmed();
    const QString lower = text.toLower();
    if (lower == QStringLiteral("gguf") ||
        lower == QStringLiteral("ggml") ||
        lower == QStringLiteral("onnx") ||
        lower == QStringLiteral("tts") ||
        lower == QStringLiteral("stt")) {
        return text.toUpper();
    }
    if (lower == QStringLiteral("styletts2"))
        return QStringLiteral("StyleTTS2");
    if (lower == QStringLiteral("voice-cloning"))
        return QStringLiteral("Voice Cloning");
    if (lower == QStringLiteral("forced-alignment"))
        return QStringLiteral("Alignment");
    if (lower == QStringLiteral("tool-use"))
        return QStringLiteral("Tool Use");
    if (text.size() <= 3)
        return text.toUpper();
    return text;
}

QString joinTokens(const QVariantList &values)
{
    QStringList out;
    for (const QString &value : uniqueStrings(values))
        out.append(displayToken(value));
    return out.join(QStringLiteral(", "));
}

bool isVoxCpm2Family(const QVariantMap &family)
{
    const QStringList keys = {
        QStringLiteral("id"),
        QStringLiteral("familyId"),
        QStringLiteral("modelId"),
        QStringLiteral("realId"),
        QStringLiteral("localDir")
    };
    for (const QString &key : keys) {
        if (family.value(key).toString().contains(QStringLiteral("voxcpm2"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool isFullPrecisionVoxCpm2Candidate(const QString &filename, double sizeGb)
{
    const QString lower = filename.toLower();
    if (lower.contains(QStringLiteral("f16")) ||
        lower.contains(QStringLiteral("fp16"))) {
        return true;
    }
    return sizeGb >= 3.0;
}

QVariantMap metadataMap(const QVariantMap &family)
{
    QVariantMap metadata = family.value(QStringLiteral("metadataOverrides")).toMap();
    if (!metadata.isEmpty())
        return metadata;
    metadata = family.value(QStringLiteral("metadata")).toMap();
    if (!metadata.isEmpty())
        return metadata;
    return family.value(QStringLiteral("modelYaml")).toMap().value(QStringLiteral("metadataOverrides")).toMap();
}

QVariantMap badge(const QString &label, const QString &value, const QString &tone)
{
    QVariantMap item;
    item.insert(QStringLiteral("label"), label);
    item.insert(QStringLiteral("value"), value);
    item.insert(QStringLiteral("tone"), tone);
    return item;
}

QVariantMap capabilityBadge(const QString &value)
{
    QVariantMap item;
    item.insert(QStringLiteral("value"), displayToken(value));
    item.insert(QStringLiteral("tone"), QStringLiteral("success"));
    return item;
}

QString compactCount(qint64 value)
{
    if (value >= 1000000000)
        return QString::number(value / 1000000000.0, 'f', value >= 10000000000LL ? 0 : 1) + QStringLiteral("B");
    if (value >= 1000000)
        return QString::number(value / 1000000.0, 'f', value >= 10000000 ? 0 : 1) + QStringLiteral("M");
    if (value >= 10000)
        return QString::number(value / 1000.0, 'f', value >= 100000 ? 0 : 1) + QStringLiteral("K");
    return QLocale().toString(value);
}

qint64 parseSizeBytes(const QString &sizeText)
{
    const QString text = sizeText.trimmed();
    if (text.isEmpty() || text.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0)
        return 0;

    static const QRegularExpression regex(
        QStringLiteral("^([\\d.]+)\\s*(B|KB|MB|GB|TB)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch())
        return 0;

    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    if (!ok || value <= 0)
        return 0;

    const QString unit = match.captured(2).toUpper();
    double multiplier = 1.0;
    if (unit == QStringLiteral("KB"))
        multiplier = 1024.0;
    else if (unit == QStringLiteral("MB"))
        multiplier = 1024.0 * 1024.0;
    else if (unit == QStringLiteral("GB"))
        multiplier = 1024.0 * 1024.0 * 1024.0;
    else if (unit == QStringLiteral("TB"))
        multiplier = 1024.0 * 1024.0 * 1024.0 * 1024.0;

    return static_cast<qint64>(value * multiplier);
}

QVariantMap statBadge(const QString &label, qint64 value, const QString &tone, const QString &source)
{
    QVariantMap item = badge(label, compactCount(value), tone);
    item.insert(QStringLiteral("rawValue"), value);
    item.insert(QStringLiteral("source"), source);
    return item;
}

QVariantList formatValues(const QVariantMap &family, const QVariantMap &metadata)
{
    QVariantList values = asList(metadata.value(QStringLiteral("compatibilityTypes")));
    if (!values.isEmpty())
        return values;

    const QVariantList tags = asList(family.value(QStringLiteral("tags")));
    for (const QVariant &tag : tags) {
        const QString lower = tag.toString().toLower();
        if (lower == QStringLiteral("gguf") ||
            lower == QStringLiteral("ggml") ||
            lower == QStringLiteral("onnx") ||
            lower == QStringLiteral("safetensors")) {
            values.append(tag);
        }
    }
    return values;
}

QVariantList modelInfoBadges(const QVariantMap &family)
{
    const QVariantMap metadata = metadataMap(family);
    QVariantList params = asList(metadata.value(QStringLiteral("paramsStrings")));
    QVariantList architectures = asList(metadata.value(QStringLiteral("architectures")));
    QVariantList domain = asList(metadata.value(QStringLiteral("domain")));

    if (params.isEmpty())
        params = asList(family.value(QStringLiteral("params")));
    if (architectures.isEmpty())
        architectures = asList(family.value(QStringLiteral("architectures")));
    if (domain.isEmpty())
        domain = asList(family.value(QStringLiteral("type")));

    QVariantList out;
    const QString paramsText = joinTokens(params);
    const QString archText = joinTokens(architectures);
    const QString domainText = joinTokens(domain);
    const QString contextText = joinTokens(asList(metadata.value(QStringLiteral("contextLengths"))));
    const QString formatText = joinTokens(formatValues(family, metadata));

    if (!paramsText.isEmpty())
        out.append(badge(QStringLiteral("Params"), paramsText, QStringLiteral("neutral")));
    if (!archText.isEmpty())
        out.append(badge(QStringLiteral("Arch"), archText, QStringLiteral("neutral")));
    if (!domainText.isEmpty())
        out.append(badge(QStringLiteral("Domain"), domainText, QStringLiteral("accent")));
    if (!contextText.isEmpty())
        out.append(badge(QStringLiteral("Context"), contextText, QStringLiteral("info")));
    if (!formatText.isEmpty())
        out.append(badge(QStringLiteral("Format"), formatText, QStringLiteral("format")));
    return out;
}

bool metadataFlag(const QVariantMap &metadata, const QString &key)
{
    const QVariant value = metadata.value(key);
    if (!value.isValid() || value.isNull())
        return false;
    if (value.typeId() == QMetaType::Bool)
        return value.toBool();
    return value.toString().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QVariantList capabilityBadges(const QVariantMap &family)
{
    const QVariantMap metadata = metadataMap(family);
    QVariantList values = asList(metadata.value(QStringLiteral("capabilities")));
    if (values.isEmpty())
        values = asList(family.value(QStringLiteral("capabilities")));
    if (metadataFlag(metadata, QStringLiteral("vision")))
        values.append(QStringLiteral("Vision"));
    if (metadataFlag(metadata, QStringLiteral("trainedForToolUse")))
        values.append(QStringLiteral("Tool Use"));
    if (metadataFlag(metadata, QStringLiteral("reasoning")))
        values.append(QStringLiteral("Reasoning"));

    QVariantList out;
    for (const QString &value : uniqueStrings(values))
        out.append(capabilityBadge(value));
    return out;
}

QVariantList statsBadges(const QVariantMap &family)
{
    const QVariantMap stats = family.value(QStringLiteral("stats")).toMap();
    QVariantList out;

    qint64 downloads = stats.value(QStringLiteral("displayDownloads")).toLongLong();
    if (downloads <= 0)
        downloads = stats.value(QStringLiteral("localDownloads")).toLongLong();
    if (downloads > 0) {
        out.append(statBadge(
            QStringLiteral("HF Downloads"),
            downloads,
            QStringLiteral("info"),
            QStringLiteral("Original Hugging Face model")));
    }

    const qint64 upstreamLikes = stats.value(QStringLiteral("upstreamLikes")).toLongLong();
    if (upstreamLikes > 0) {
        out.append(statBadge(
            QStringLiteral("HF Stars"),
            upstreamLikes,
            QStringLiteral("accent"),
            QStringLiteral("Original Hugging Face model")));
    }

    return out;
}

QString familyCapability(const QVariantMap &family)
{
    const QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();
    if (capabilities.contains(QStringLiteral("voice-isolation")))
        return QStringLiteral("voice-isolation");
    if (capabilities.contains(QStringLiteral("forced-alignment")))
        return QStringLiteral("forced-alignment");
    if (capabilities.contains(QStringLiteral("stt")))
        return QStringLiteral("stt");
    if (capabilities.contains(QStringLiteral("voice-cloning")))
        return QStringLiteral("voice-cloning");
    return QStringLiteral("tts");
}

QString familyIconName(const QVariantMap &family)
{
    const QString capability = familyCapability(family);
    if (capability == QStringLiteral("voice-isolation"))
        return QStringLiteral("voice-isolator");
    if (capability == QStringLiteral("stt"))
        return QStringLiteral("mic");
    if (capability == QStringLiteral("forced-alignment"))
        return QStringLiteral("sliders");
    if (capability == QStringLiteral("voice-cloning"))
        return QStringLiteral("users");
    return QStringLiteral("volume");
}

QString modelCardUrl(const QVariantMap &family)
{
    const QString explicitUrl = family.value(QStringLiteral("modelCardUrl")).toString();
    if (!explicitUrl.isEmpty())
        return explicitUrl;
    const QString source = family.value(QStringLiteral("source")).toString();
    if (!source.isEmpty())
        return source;
    const QVariantMap manifest = family.value(QStringLiteral("hubFiles")).toMap().value(QStringLiteral("manifest")).toMap();
    const QString manifestSource = manifest.value(QStringLiteral("source")).toString();
    if (!manifestSource.isEmpty())
        return manifestSource;
    const QString modelId = !family.value(QStringLiteral("modelId")).toString().isEmpty()
        ? family.value(QStringLiteral("modelId")).toString()
        : (!family.value(QStringLiteral("realId")).toString().isEmpty()
            ? family.value(QStringLiteral("realId")).toString()
            : family.value(QStringLiteral("id")).toString());
    return modelId.isEmpty() ? QString() : QStringLiteral("https://huggingface.co/") + modelId;
}

QString readmeContent(const QVariantMap &family)
{
    const QVariantMap hubReadme = family.value(QStringLiteral("hubFiles")).toMap().value(QStringLiteral("readme")).toMap();
    const QString hubContent = hubReadme.value(QStringLiteral("content")).toString();
    if (!hubContent.isEmpty())
        return hubContent;

    const QVariant readme = family.value(QStringLiteral("readme"));
    if (readme.typeId() == QMetaType::QVariantMap)
        return readme.toMap().value(QStringLiteral("content")).toString();
    return readme.toString();
}

QString thumbnailSource(const QVariantMap &family)
{
    const QVariantMap thumbnail = family.value(QStringLiteral("hubFiles")).toMap().value(QStringLiteral("thumbnail")).toMap();
    const QString base64 = thumbnail.value(QStringLiteral("base64")).toString();
    if (base64.isEmpty())
        return {};
    const QString mimeType = thumbnail.value(QStringLiteral("mimeType")).toString().isEmpty()
        ? QStringLiteral("image/png")
        : thumbnail.value(QStringLiteral("mimeType")).toString();
    return QStringLiteral("data:%1;base64,%2").arg(mimeType, base64);
}

QString runtimeDescription(const QVariantMap &runtime)
{
    const QString label = (runtime.value(QStringLiteral("label")).toString() + QStringLiteral(" ") +
                           runtime.value(QStringLiteral("name")).toString()).toLower();
    if (label.contains(QStringLiteral("cuda")))
        return QStringLiteral("NVIDIA CUDA accelerated inference engine");
    if (label.contains(QStringLiteral("vulkan")))
        return QStringLiteral("Vulkan GPU accelerated inference engine");
    if (label.contains(QStringLiteral("hip")) || label.contains(QStringLiteral("radeon")))
        return QStringLiteral("AMD Radeon GPU accelerated inference engine");
    if (label.contains(QStringLiteral("sycl")))
        return QStringLiteral("Intel SYCL accelerated inference engine");
    if (label.contains(QStringLiteral("openvino")))
        return QStringLiteral("Intel OpenVINO accelerated inference engine");
    return QStringLiteral("CPU-only inference engine");
}

QString runtimeIconName(const QVariantMap &runtime)
{
    return runtime.value(QStringLiteral("label")).toString() == QStringLiteral("CPU")
        ? QStringLiteral("cpu")
        : QStringLiteral("spark");
}

QPair<QString, QString> preferredRuntime(const QVariantList &runtimeOptions)
{
    QPair<QString, QString> firstOption;
    QPair<QString, QString> installedGpu;
    QPair<QString, QString> installedCpu;
    QPair<QString, QString> compatibleGpu;
    QPair<QString, QString> compatibleCpu;

    for (const QVariant &value : runtimeOptions) {
        const QVariantMap option = value.toMap();
        const QString optionId = option.value(QStringLiteral("id")).toString();
        const QString optionVersion = option.value(QStringLiteral("version")).toString();
        const QString runtimeIdentity = (optionId + QStringLiteral(" ")
            + option.value(QStringLiteral("label")).toString() + QStringLiteral(" ")
            + option.value(QStringLiteral("name")).toString()).toLower();
        const bool gpu = runtimeIdentity.contains(QStringLiteral("cuda"))
            || runtimeIdentity.contains(QStringLiteral("vulkan"))
            || runtimeIdentity.contains(QStringLiteral("hip"))
            || runtimeIdentity.contains(QStringLiteral("radeon"))
            || runtimeIdentity.contains(QStringLiteral("sycl"))
            || runtimeIdentity.contains(QStringLiteral("openvino"))
            || runtimeIdentity.contains(QStringLiteral("gpu"));
        if (firstOption.first.isEmpty()) firstOption = {optionId, optionVersion};
        if (!option.value(QStringLiteral("compatible")).toBool()) {
            continue;
        }

        if (option.value(QStringLiteral("installed")).toBool()) {
            if (gpu && installedGpu.first.isEmpty()) installedGpu = {optionId, optionVersion};
            if (!gpu && installedCpu.first.isEmpty()) installedCpu = {optionId, optionVersion};
        } else {
            if (gpu && compatibleGpu.first.isEmpty()) compatibleGpu = {optionId, optionVersion};
            if (!gpu && compatibleCpu.first.isEmpty()) compatibleCpu = {optionId, optionVersion};
        }
    }
    if (!installedGpu.first.isEmpty()) return installedGpu;
    if (!installedCpu.first.isEmpty()) return installedCpu;
    if (!compatibleGpu.first.isEmpty()) return compatibleGpu;
    if (!compatibleCpu.first.isEmpty()) return compatibleCpu;
    return firstOption;
}

// Keep the gallery filter on canonical ISO 639-1 codes while accepting
// ISO 639-3 aliases from model metadata (for example vie -> vi).
QString canonicalLanguageCode(const QString &raw)
{
    const QString code = raw.trimmed().toLower();
    if (code == QStringLiteral("vie"))
        return QStringLiteral("vi");
    return code;
}

QVersionNumber parsedRuntimeVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    return QVersionNumber::fromString(version);
}

int compareRuntimeVersions(const QString &left, const QString &right)
{
    const QVersionNumber leftVersion = parsedRuntimeVersion(left);
    const QVersionNumber rightVersion = parsedRuntimeVersion(right);
    if (!leftVersion.isNull() && !rightVersion.isNull())
        return QVersionNumber::compare(leftVersion, rightVersion);
    return QString::compare(left, right, Qt::CaseInsensitive);
}

bool runtimeVersionGreater(const QString &left, const QString &right)
{
    if (right.isEmpty())
        return !left.isEmpty();
    return compareRuntimeVersions(left, right) > 0;
}

bool installedRuntimeVersion(const QVariantList &installedVersions, const QString &version)
{
    if (version.isEmpty())
        return !installedVersions.isEmpty();
    for (const QVariant &installed : installedVersions) {
        if (installed.toMap().value(QStringLiteral("version")).toString() == version)
            return true;
    }
    return false;
}

QString statusKind(bool ready, const QString &statusReason)
{
    if (ready)
        return QStringLiteral("ready");
    if (statusReason == QStringLiteral("Incompatible"))
        return QStringLiteral("incompatible");
    return QStringLiteral("setup");
}

bool familySupportsLanguage(const QVariantMap &family, const QString &langFilter)
{
    if (langFilter.isEmpty() || langFilter.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString targetLang = canonicalLanguageCode(langFilter);

    // 1. Check supportedLanguages
    const QVariantList supportedLangs = family.value(QStringLiteral("supportedLanguages")).toList();
    for (const QVariant &val : supportedLangs) {
        if (val.typeId() == QMetaType::QVariantMap) {
            const QVariantMap m = val.toMap();
            if (canonicalLanguageCode(m.value(QStringLiteral("value")).toString()) == targetLang) {
                return true;
            }
        } else if (canonicalLanguageCode(val.toString()) == targetLang) {
            return true;
        }
    }

    // 2. Check featuredLanguages
    const QVariantList featuredLangs = family.value(QStringLiteral("featuredLanguages")).toList();
    for (const QVariant &val : featuredLangs) {
        if (val.typeId() == QMetaType::QVariantMap) {
            const QVariantMap m = val.toMap();
            if (canonicalLanguageCode(m.value(QStringLiteral("value")).toString()) == targetLang) {
                return true;
            }
        } else if (canonicalLanguageCode(val.toString()) == targetLang) {
            return true;
        }
    }

    // 3. Check supportedLanguageSetId
    const QString setId = family.value(QStringLiteral("supportedLanguageSetId")).toString();
    if (!setId.isEmpty()) {
        AppController *app = AppController::instance();
        if (app && app->catalog()) {
            const QVariantList setLangs = app->catalog()->languageSet(setId);
            for (const QVariant &val : setLangs) {
                if (val.typeId() == QMetaType::QVariantMap) {
                    const QVariantMap m = val.toMap();
                    if (canonicalLanguageCode(m.value(QStringLiteral("code")).toString()) == targetLang) {
                        return true;
                    }
                } else if (canonicalLanguageCode(val.toString()) == targetLang) {
                    return true;
                }
            }
        }
    }

    return false;
}

void collectLanguagesForFamily(const QVariantMap &family, QList<QPair<QString, QString>> &langs)
{
    // 1. Check supportedLanguages
    const QVariantList supportedLangs = family.value(QStringLiteral("supportedLanguages")).toList();
    for (const QVariant &val : supportedLangs) {
        if (val.typeId() == QMetaType::QVariantMap) {
            const QVariantMap m = val.toMap();
            const QString code = m.value(QStringLiteral("value")).toString().trimmed();
            const QString name = m.value(QStringLiteral("text")).toString().trimmed();
            if (!code.isEmpty()) {
                langs.append({code, name.isEmpty() ? code : name});
            }
        } else {
            const QString code = val.toString().trimmed();
            if (!code.isEmpty()) {
                langs.append({code, code});
            }
        }
    }

    // 2. Check featuredLanguages
    const QVariantList featuredLangs = family.value(QStringLiteral("featuredLanguages")).toList();
    for (const QVariant &val : featuredLangs) {
        if (val.typeId() == QMetaType::QVariantMap) {
            const QVariantMap m = val.toMap();
            const QString code = m.value(QStringLiteral("value")).toString().trimmed();
            const QString name = m.value(QStringLiteral("text")).toString().trimmed();
            if (!code.isEmpty()) {
                langs.append({code, name.isEmpty() ? code : name});
            }
        } else {
            const QString code = val.toString().trimmed();
            if (!code.isEmpty()) {
                langs.append({code, code});
            }
        }
    }

    // 3. Check supportedLanguageSetId
    const QString setId = family.value(QStringLiteral("supportedLanguageSetId")).toString();
    if (!setId.isEmpty()) {
        AppController *app = AppController::instance();
        if (app && app->catalog()) {
            const QVariantList setLangs = app->catalog()->languageSet(setId);
            for (const QVariant &val : setLangs) {
                if (val.typeId() == QMetaType::QVariantMap) {
                    const QVariantMap m = val.toMap();
                    const QString code = m.value(QStringLiteral("code")).toString().trimmed();
                    const QString name = m.value(QStringLiteral("name")).toString().trimmed();
                    if (!code.isEmpty()) {
                        langs.append({code, name.isEmpty() ? code : name});
                    }
                } else {
                    const QString code = val.toString().trimmed();
                    if (!code.isEmpty()) {
                        langs.append({code, code});
                    }
                }
            }
        }
    }
}

}

CapabilityFamilyModel::CapabilityFamilyModel(ModelManager *models, RuntimeManager *runtimes, RegistryManager *registry, Settings *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_models(models)
    , m_runtimes(runtimes)
    , m_registry(registry)
    , m_settings(settings)
{
    if (m_models) {
        connect(m_models, &ModelManager::registryUpdated, this, &CapabilityFamilyModel::refresh);
    }
    if (m_runtimes) {
        connect(m_runtimes, &RuntimeManager::registryUpdated, this, &CapabilityFamilyModel::refresh);
    }
    connect(HardwareManager::instance(), &HardwareManager::hardwareInfoChanged,
            this, &CapabilityFamilyModel::refresh);
    if (m_settings) {
        connect(m_settings, &Settings::selectedTtsRuntimeChanged, this, &CapabilityFamilyModel::refresh);
        connect(m_settings, &Settings::selectedTtsRuntimeVersionChanged, this, &CapabilityFamilyModel::refresh);
        connect(m_settings, &Settings::selectedSttRuntimeChanged, this, &CapabilityFamilyModel::refresh);
        connect(m_settings, &Settings::selectedSttRuntimeVersionChanged, this, &CapabilityFamilyModel::refresh);
    }
    if (m_registry) {
        m_selectionRepository = new StudioSelectionRepository(m_registry->connectionName(), this);
        if (m_settings) {
            m_selectionRepository->migrateLegacySelectionsIfNeeded(m_settings);
        }
    }
}


// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "core/parts/CapabilityFamilyModel_Filtering.cpp"
#include "core/parts/CapabilityFamilyModel_Config.cpp"
#include "core/parts/CapabilityFamilyModel_Items.cpp"

} // namespace LAStudio
