bool DownloadInstallService::enqueueModelFile(const QVariantMap &family, const QVariantMap &requirement)
{
    if (rejectLocalDownloadInRemoteFirstMode()) return false;
    if (!m_downloads || !m_models) {
        emit errorOccurred(QStringLiteral("Download services are not available"));
        return false;
    }

    const QString selectedFile = requirement.value(QStringLiteral("selectedFile")).toString();
    if (family.isEmpty() || selectedFile.isEmpty()) {
        emit errorOccurred(QStringLiteral("Model download request is incomplete"));
        return false;
    }
    if (requirement.value(QStringLiteral("installed")).toBool() &&
        requirement.value(QStringLiteral("installState")).toInt() != UpdateAvailable) {
        return true;
    }

    QVariantMap metadata = virtualModelMetadata(family);
    if (!writeVirtualModelFiles(metadata))
        return false;

    QString sourceModelId = requirement.value(QStringLiteral("modelId")).toString();
    if (sourceModelId.isEmpty())
        sourceModelId = family.value(QStringLiteral("modelId")).toString();
    if (sourceModelId.isEmpty()) {
        emit errorOccurred(QStringLiteral("Model source id is empty"));
        return false;
    }

    metadata.insert(QStringLiteral("sourceModelId"), sourceModelId);
    metadata.insert(QStringLiteral("filename"), selectedFile);
    metadata.insert(QStringLiteral("expectedSize"), requirement.value(QStringLiteral("size")));
    metadata.insert(QStringLiteral("expectedBytes"), requirement.value(QStringLiteral("sizeBytes")));

    const QVariantList sources = requirement.value(QStringLiteral("sources")).toList();
    for (const QVariant &sourceValue : sources) {
        const QVariantMap source = sourceValue.toMap();
        const QString url = source.value(QStringLiteral("url")).toString();
        bool containsFile = false;
        for (const QVariant &fileValue : source.value(QStringLiteral("files")).toList()) {
            const QVariantMap file = fileValue.toMap();
            if (file.value(QStringLiteral("name")).toString() == selectedFile ||
                file.value(QStringLiteral("file")).toString() == selectedFile) {
                containsFile = true;
                break;
            }
        }
        if (!url.isEmpty() && containsFile) {
            metadata.insert(QStringLiteral("sha256"), source.value(QStringLiteral("sha256")).toString());
            metadata.insert(QStringLiteral("checksum"), source.value(QStringLiteral("checksum")).toString());
            const QString archiveName = QFileInfo(QUrl(url).path()).fileName();
            const bool archive = archiveName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) ||
                archiveName.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive) ||
                archiveName.endsWith(QStringLiteral(".tar.bz2"), Qt::CaseInsensitive) ||
                archiveName.endsWith(QStringLiteral(".tbz2"), Qt::CaseInsensitive);
            if (archive && !archiveName.isEmpty()) {
                metadata.insert(QStringLiteral("archiveMember"), selectedFile);
                metadata.insert(QStringLiteral("requestedFilename"), selectedFile);
                m_updateAvailable.remove(sourceModelId + QStringLiteral("::") + selectedFile);
                return m_downloads->enqueueUrl(url, archiveName,
                                               m_models->concreteModelDir(sourceModelId), metadata);
            }
            metadata.insert(QStringLiteral("sourceUrl"), url);
            m_updateAvailable.remove(sourceModelId + QStringLiteral("::") + selectedFile);
            return m_downloads->enqueueUrl(url, selectedFile,
                                           m_models->concreteModelDir(sourceModelId), metadata);
        }
    }

    const QString key = sourceModelId + QStringLiteral("::") + selectedFile;
    m_updateAvailable.remove(key);
    return m_downloads->enqueue(sourceModelId, selectedFile,
                                m_models->concreteModelDir(sourceModelId), metadata);
}

bool DownloadInstallService::enqueueRuntime(const QVariantMap &family,
                                            const QString &capability,
                                            const QString &familyId,
                                            const QVariantMap &runtime)
{
    if (rejectLocalDownloadInRemoteFirstMode()) return false;
    if (!m_downloads || !m_runtimes) {
        emit errorOccurred(QStringLiteral("Runtime download services are not available"));
        return false;
    }
    if (runtime.isEmpty() || runtime.value(QStringLiteral("installed")).toBool())
        return true;

    const QString asset = runtime.value(QStringLiteral("asset")).toString();
    const QString source = runtime.value(QStringLiteral("source")).toString();
    const QString version = runtime.value(QStringLiteral("version")).toString();
    if (asset.isEmpty() || source.isEmpty()) {
        emit errorOccurred(QStringLiteral("Runtime download request is incomplete"));
        return false;
    }

    const QVariantMap virtualMetadata = virtualModelMetadata(family);
    if (!writeVirtualModelFiles(virtualMetadata))
        return false;

    const QString baseUrl = source + version + QStringLiteral("/");
    QVariantMap metadata;
    metadata.insert(QStringLiteral("id"), runtime.value(QStringLiteral("id")).toString());
    metadata.insert(QStringLiteral("version"), version);
    metadata.insert(QStringLiteral("engineName"),
                    !runtime.value(QStringLiteral("name")).toString().isEmpty()
                        ? runtime.value(QStringLiteral("name")).toString()
                        : (!runtime.value(QStringLiteral("label")).toString().isEmpty()
                            ? runtime.value(QStringLiteral("label")).toString()
                            : runtime.value(QStringLiteral("id")).toString()));
    metadata.insert(QStringLiteral("engineFamily"), runtime.value(QStringLiteral("engineFamily")).toString());
    QString runtimeType = QStringLiteral("tts");
    if (capability == QStringLiteral("stt")) {
        runtimeType = QStringLiteral("stt");
    } else if (capability == QStringLiteral("translation")) {
        // Translation is exposed through the STT/runtime registry domain;
        // registry_schema.sql intentionally accepts only stt/tts/alignment types.
        runtimeType = QStringLiteral("stt");
    } else if (capability == QStringLiteral("forced-alignment")) {
        runtimeType = QStringLiteral("alignment");
    }
    metadata.insert(QStringLiteral("type"), runtimeType);
    metadata.insert(QStringLiteral("library"), runtime.value(QStringLiteral("library")).toString());
    metadata.insert(QStringLiteral("kind"), runtime.value(QStringLiteral("kind"), QStringLiteral("dynamic-library")).toString());
    metadata.insert(QStringLiteral("entrypoint"), runtime.value(QStringLiteral("entrypoint")).toString());
    metadata.insert(QStringLiteral("protocolVersion"), runtime.value(QStringLiteral("protocolVersion")).toString());
    metadata.insert(QStringLiteral("nativeDependencies"), runtime.value(QStringLiteral("nativeDependencies")).toList());
    metadata.insert(QStringLiteral("capabilities"), runtime.value(QStringLiteral("capabilities")).toList());
    metadata.insert(QStringLiteral("modelFormats"), runtime.value(QStringLiteral("modelFormats")).toList());
    metadata.insert(QStringLiteral("dependencyDownloads"), runtime.value(QStringLiteral("dependencyDownloads")).toList());
    metadata.insert(QStringLiteral("sha256"), runtime.value(QStringLiteral("sha256")).toString());
    metadata.insert(QStringLiteral("checksum"), runtime.value(QStringLiteral("checksum")).toString());
    metadata.insert(QStringLiteral("expectedSize"), runtime.value(QStringLiteral("size")));
    metadata.insert(QStringLiteral("expectedBytes"), runtime.value(QStringLiteral("sizeBytes")));

    QVariantMap runtimeMetadata;
    runtimeMetadata.insert(QStringLiteral("backend"), runtime.value(QStringLiteral("backend")).toString());
    runtimeMetadata.insert(QStringLiteral("modelFamily"), familyId);
    runtimeMetadata.insert(QStringLiteral("modelId"), family.value(QStringLiteral("modelId")).toString());
    runtimeMetadata.insert(QStringLiteral("modelVersion"), runtime.value(QStringLiteral("modelVersion")).toString());
    runtimeMetadata.insert(QStringLiteral("runtimeVersion"), version);
    runtimeMetadata.insert(QStringLiteral("asset"), asset);
    runtimeMetadata.insert(QStringLiteral("source"), baseUrl + asset);
    metadata.insert(QStringLiteral("metadata"), runtimeMetadata);

    return m_downloads->enqueueUrl(baseUrl + asset, asset, m_runtimes->backendsPath(), metadata);
}

bool DownloadInstallService::enqueueRecommendedSetup(const QVariantMap &familyItem)
{
    if (rejectLocalDownloadInRemoteFirstMode()) return false;
    QVariantMap family = familyItem.value(QStringLiteral("rawMetadata")).toMap();
    if (family.isEmpty()) {
        family = familyItem;
    }
    const QString familyId = !familyItem.value(QStringLiteral("familyId")).toString().isEmpty()
        ? familyItem.value(QStringLiteral("familyId")).toString()
        : family.value(QStringLiteral("id")).toString();
    const QString capability = !familyItem.value(QStringLiteral("familyCapability")).toString().isEmpty()
        ? familyItem.value(QStringLiteral("familyCapability")).toString()
        : QStringLiteral("tts");

    if (family.isEmpty() || familyId.isEmpty()) {
        emit errorOccurred(QStringLiteral("Recommended setup request is incomplete"));
        return false;
    }

    bool ok = true;
    bool hasWorkOrActiveInstall = false;

    const QVariantList requiredFiles = familyItem.value(QStringLiteral("requiredFiles")).toList();
    for (const QVariant &requirementValue : requiredFiles) {
        QVariantMap requirement = requirementValue.toMap();
        const int installState = requirement.contains(QStringLiteral("installState"))
            ? requirement.value(QStringLiteral("installState")).toInt()
            : NotInstalled;
        const bool installed = requirement.value(QStringLiteral("installed")).toBool() || installState == Installed;
        if (installed) {
            continue;
        }
        if (installState == Downloading || installState == Installing) {
            hasWorkOrActiveInstall = true;
            continue;
        }

        if (requirement.value(QStringLiteral("selectedFile")).toString().isEmpty()) {
            requirement.insert(QStringLiteral("selectedFile"), requirement.value(QStringLiteral("file")).toString());
        }
        if (!enqueueModelFile(family, requirement)) {
            ok = false;
        } else {
            hasWorkOrActiveInstall = true;
        }
    }

    const QVariantList runtimeOptions = familyItem.value(QStringLiteral("runtimeOptions")).toList();
    bool hasCompatibleRuntime = runtimeOptions.isEmpty();
    bool hasLatestCompatibleRuntime = false;
    bool hasActiveRuntimeInstall = false;
    QVariantMap runtimeToInstall;

    for (const QVariant &runtimeValue : runtimeOptions) {
        const QVariantMap runtime = runtimeValue.toMap();
        if (!runtime.value(QStringLiteral("compatible")).toBool()) {
            continue;
        }

        hasCompatibleRuntime = true;
        const QVariantMap latestRuntime = latestSupportedRuntime(runtime);
        const QString latestVersion = latestRuntime.value(QStringLiteral("version")).toString();
        const QString installedVersion = runtime.value(QStringLiteral("version")).toString();
        const int installState = runtime.contains(QStringLiteral("installState"))
            ? runtime.value(QStringLiteral("installState")).toInt()
            : NotInstalled;
        const bool installed = runtime.value(QStringLiteral("installed")).toBool() || installState == Installed;
        const bool latestInstalled = runtime.contains(QStringLiteral("latestInstalled"))
            ? runtime.value(QStringLiteral("latestInstalled")).toBool()
            : (installed && installedVersion == latestVersion);
        if (latestInstalled) {
            hasLatestCompatibleRuntime = true;
            continue;
        }
        const int latestInstallState = runtime.contains(QStringLiteral("latestInstallState"))
            ? runtime.value(QStringLiteral("latestInstallState")).toInt()
            : installState;
        if (latestInstallState == Downloading || latestInstallState == Installing) {
            hasActiveRuntimeInstall = true;
            continue;
        }
        if (runtimeToInstall.isEmpty()) {
            runtimeToInstall = latestRuntime;
        }
    }

    if (!hasCompatibleRuntime) {
        emit errorOccurred(QStringLiteral("No compatible runtime is available for this model on the detected hardware"));
        return false;
    }

    if (!hasLatestCompatibleRuntime) {
        if (!runtimeToInstall.isEmpty()) {
            if (!enqueueRuntime(family, capability, familyId, runtimeToInstall)) {
                ok = false;
            } else {
                hasWorkOrActiveInstall = true;
            }
        } else if (hasActiveRuntimeInstall) {
            hasWorkOrActiveInstall = true;
        }
    }

    if (hasWorkOrActiveInstall) {
        emit installStatesChanged();
    }
    return ok;
}

