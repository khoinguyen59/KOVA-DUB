QVariantMap CapabilityFamilyModel::recommendedConfiguration() const
{
    return recommendedConfigurationExcluding({});
}

QVariantMap CapabilityFamilyModel::configurationForFamily(const QString &familyId) const
{
    for (const FamilyItem &item : m_items) {
        if (item.id != familyId || !item.supported || item.preferredRuntimeId.isEmpty())
            continue;
        QVariantMap runtime;
        for (const QVariant &runtimeValue : item.runtimeOptions) {
            const QVariantMap candidate = runtimeValue.toMap();
            if (candidate.value(QStringLiteral("id")).toString() == item.preferredRuntimeId) {
                runtime = candidate;
                break;
            }
        }
        if (runtime.isEmpty() || !runtime.value(QStringLiteral("compatible")).toBool())
            return {};
        return {
            {QStringLiteral("familyId"), item.id},
            {QStringLiteral("modelName"), item.displayName},
            {QStringLiteral("runtimeId"), item.preferredRuntimeId},
            {QStringLiteral("runtimeVersion"), item.preferredRuntimeVersion},
            {QStringLiteral("runtimeName"), runtime.value(QStringLiteral("label"),
                                                            runtime.value(QStringLiteral("name"),
                                                                          item.preferredRuntimeId))},
            {QStringLiteral("selectedFiles"), item.selectedFiles},
            {QStringLiteral("ready"), item.ready},
            {QStringLiteral("installed"), item.installed}
        };
    }
    return {};
}

QVariantMap CapabilityFamilyModel::recommendedConfigurationExcluding(
    const QStringList &excludedFamilyIds) const
{
    const double ramBytes = HardwareManager::instance()->ramTotal() * 1024.0 * 1024.0 * 1024.0;
    const double vramBytes = HardwareManager::instance()->vramTotal() * 1024.0 * 1024.0 * 1024.0;

    qint64 bestScore = std::numeric_limits<qint64>::min();
    const FamilyItem *bestItem = nullptr;
    QVariantMap bestRuntime;
    QString bestReason;

    for (const FamilyItem &item : m_items) {
        if (excludedFamilyIds.contains(item.id))
            continue;
        if (!item.supported || item.preferredRuntimeId.isEmpty())
            continue;

        QVariantMap runtime;
        for (const QVariant &runtimeValue : item.runtimeOptions) {
            const QVariantMap candidate = runtimeValue.toMap();
            if (candidate.value(QStringLiteral("id")).toString() == item.preferredRuntimeId) {
                runtime = candidate;
                break;
            }
        }
        if (runtime.isEmpty() || !runtime.value(QStringLiteral("compatible")).toBool())
            continue;

        qint64 modelBytes = 0;
        for (const QVariant &requirementValue : item.requiredFiles) {
            const QVariantMap requirement = requirementValue.toMap();
            modelBytes += parseSizeBytes(requirement.value(QStringLiteral("selectedSize")).toString());
        }

        const QString runtimeIdentity = (runtime.value(QStringLiteral("id")).toString() + QLatin1Char(' ')
            + runtime.value(QStringLiteral("label")).toString() + QLatin1Char(' ')
            + runtime.value(QStringLiteral("name")).toString()).toLower();
        const bool gpuRuntime = runtimeIdentity.contains(QStringLiteral("cuda"))
            || runtimeIdentity.contains(QStringLiteral("vulkan"))
            || runtimeIdentity.contains(QStringLiteral("hip"))
            || runtimeIdentity.contains(QStringLiteral("radeon"))
            || runtimeIdentity.contains(QStringLiteral("sycl"))
            || runtimeIdentity.contains(QStringLiteral("openvino"))
            || runtimeIdentity.contains(QStringLiteral("gpu"));
        const bool runtimeInstalled = runtime.value(QStringLiteral("installed")).toBool();

        qint64 score = 0;
        if (item.ready) score += 100000;
        if (item.installed) score += 50000;
        if (runtimeInstalled) score += 10000;
        if (item.isLastudioPick) score += 3000;

        QString reason;
        if (gpuRuntime && vramBytes > 0.0) {
            const bool fitsVram = modelBytes <= 0 || modelBytes <= vramBytes * 0.80;
            score += fitsVram ? 8000 : -12000;
            reason = fitsVram
                ? QStringLiteral("Recommended GPU configuration for the detected hardware")
                : QStringLiteral("GPU runtime is compatible, but the model may exceed available VRAM");
        } else {
            const bool fitsRam = modelBytes <= 0 || modelBytes <= ramBytes * 0.65;
            score += fitsRam ? 4000 : -16000;
            reason = fitsRam
                ? QStringLiteral("Recommended CPU configuration for available system memory")
                : QStringLiteral("Compatible configuration, but the model may exceed available system memory");
        }

        // Prefer a smaller footprint when two configurations have otherwise
        // equivalent readiness and hardware support.
        if (modelBytes > 0)
            score -= static_cast<qint64>(modelBytes / (256.0 * 1024.0 * 1024.0));

        if (score > bestScore) {
            bestScore = score;
            bestItem = &item;
            bestRuntime = runtime;
            bestReason = reason;
        }
    }

    if (!bestItem)
        return {};

    return {
        {QStringLiteral("familyId"), bestItem->id},
        {QStringLiteral("modelName"), bestItem->displayName},
        {QStringLiteral("runtimeId"), bestItem->preferredRuntimeId},
        {QStringLiteral("runtimeVersion"), bestItem->preferredRuntimeVersion},
        {QStringLiteral("runtimeName"), bestRuntime.value(QStringLiteral("label"),
                                                          bestRuntime.value(QStringLiteral("name"),
                                                                            bestItem->preferredRuntimeId))},
        {QStringLiteral("selectedFiles"), bestItem->selectedFiles},
        {QStringLiteral("ready"), bestItem->ready},
        {QStringLiteral("installed"), bestItem->installed},
        {QStringLiteral("reason"), bestReason}
    };
}

QString CapabilityFamilyModel::recommendedFileForRequirement(const QVariantMap &family,
                                                             const QVariantMap &requirement,
                                                             const QVariantList &candidates) const
{
    QStringList files;
    QSet<QString> seen;
    for (const QVariant &candidate : candidates) {
        const QString file = candidate.toString().trimmed();
        if (file.isEmpty() || seen.contains(file)) {
            continue;
        }
        seen.insert(file);
        files.append(file);
    }

    const QString defaultFile = requirement.value(QStringLiteral("file")).toString().trimmed();
    if (files.isEmpty() && !defaultFile.isEmpty()) {
        files.append(defaultFile);
    }
    if (files.isEmpty()) {
        return {};
    }

    if (requirement.value(QStringLiteral("preferDefault")).toBool()
        && !defaultFile.isEmpty()
        && isModelSuitable(defaultFile, family, requirement)) {
        return defaultFile;
    }

    const QString defaultSize = requirement.value(QStringLiteral("size")).toString();
    QString bestSuitable;
    qint64 bestSuitableSize = -1;
    QString firstSuitableUnknownSize;
    QString smallestKnown = files.first();
    qint64 smallestKnownSize = std::numeric_limits<qint64>::max();

    for (const QString &file : files) {
        const qint64 sizeBytes = parseSizeBytes(estimateSize(file, defaultFile, defaultSize));
        if (sizeBytes > 0 && sizeBytes < smallestKnownSize) {
            smallestKnown = file;
            smallestKnownSize = sizeBytes;
        }

        if (!isModelSuitable(file, family, requirement)) {
            continue;
        }

        if (sizeBytes > 0) {
            if (sizeBytes > bestSuitableSize) {
                bestSuitable = file;
                bestSuitableSize = sizeBytes;
            }
        } else if (firstSuitableUnknownSize.isEmpty()) {
            firstSuitableUnknownSize = file;
        }
    }

    if (!bestSuitable.isEmpty()) {
        return bestSuitable;
    }
    if (!firstSuitableUnknownSize.isEmpty()) {
        return firstSuitableUnknownSize;
    }
    return smallestKnown;
}


bool CapabilityFamilyModel::isModelSuitable(const QString &filename, const QVariantMap &family, const QVariantMap &requirement) const
{
    QString defFile = requirement.value(QStringLiteral("file")).toString();
    if (defFile.isEmpty()) defFile = family.value(QStringLiteral("file")).toString();

    QString defSize = requirement.value(QStringLiteral("size")).toString();
    if (defSize.isEmpty()) defSize = family.value(QStringLiteral("size")).toString();

    QString sizeStr = estimateSize(filename, defFile, defSize);
    double sizeGb = 0;
    QRegularExpression regex("^([\\d.]+)\\s*(MB|GB|KB)$", QRegularExpression::CaseInsensitiveOption);
    auto match = regex.match(sizeStr);
    if (match.hasMatch()) {
        double val = match.captured(1).toDouble();
        QString unit = match.captured(2).toUpper();
        if (unit == "MB") sizeGb = val / 1024.0;
        else if (unit == "KB") sizeGb = val / (1024.0 * 1024.0);
        else sizeGb = val;
    }
    if (sizeGb <= 0) return true;

    bool isGpu = false;
    if (m_settings) {
        QString savedId = (m_capabilityId == "stt" || m_capabilityId == "forced-alignment")
            ? m_settings->selectedSttRuntime()
            : m_settings->selectedTtsRuntime();
        QVariantList runtimes = family.value("runtimes").toList();
        QVariantMap runtime;
        for (const QVariant &rtVal : runtimes) {
            QVariantMap rt = rtVal.toMap();
            if (rt.value("id").toString() == savedId) {
                runtime = rt;
                break;
            }
        }
        if (runtime.isEmpty() && !runtimes.isEmpty()) {
            runtime = runtimes.first().toMap();
        }
        if (!runtime.isEmpty()) {
            QString id = runtime.value("id").toString().toLower();
            if (id.contains("cuda") || id.contains("vulkan") || id.contains("hip") ||
                id.contains("radeon") || id.contains("sycl") || id.contains("openvino")) {
                isGpu = true;
            }
        }
    }

    double memoryLimit = isGpu ? HardwareManager::instance()->vramTotal() : HardwareManager::instance()->ramTotal();
    if (isGpu && memoryLimit < 1.0) {
        memoryLimit = HardwareManager::instance()->ramTotal();
    }
    if (memoryLimit <= 0) {
        memoryLimit = HardwareManager::instance()->ramTotal();
    }
    if (memoryLimit <= 0) {
        return true;
    }

    double requiredGb = sizeGb;
    if (isGpu &&
        isVoxCpm2Family(family) &&
        isFullPrecisionVoxCpm2Candidate(filename, sizeGb)) {
        requiredGb = qMax(sizeGb * 1.55, 8.0);
    }

    return requiredGb <= memoryLimit;
}

QString CapabilityFamilyModel::estimateSize(const QString &filename, const QString &defaultFile, const QString &defaultSizeStr) const
{
    if (filename.isEmpty()) return QString();

    if (m_registry) {
        QVariantList categories = m_registry->modelCategories();
        auto findSize = [](const QVariantList &cats, const QString &fn) -> QString {
            for (const QVariant &catVal : cats) {
                QVariantMap cat = catVal.toMap();
                QVariantList items = cat.value("items").toList();
                for (const QVariant &itemVal : items) {
                    QVariantMap item = itemVal.toMap();
                    if (item.contains("components")) {
                        QVariantList comps = item.value("components").toList();
                        for (const QVariant &compVal : comps) {
                            QVariantMap comp = compVal.toMap();
                            QVariantList variants = comp.value("variants").toList();
                            for (const QVariant &varVal : variants) {
                                QVariantMap variant = varVal.toMap();
                                if (variant.value("file").toString() == fn && variant.contains("size")) {
                                    return variant.value("size").toString();
                                }
                            }
                        }
                    }
                    if (item.contains("variants")) {
                        QVariantList variants = item.value("variants").toList();
                        for (const QVariant &varVal : variants) {
                            QVariantMap variant = varVal.toMap();
                            if (variant.value("file").toString() == fn && variant.contains("size")) {
                                    return variant.value("size").toString();
                            }
                        }
                    }
                    if (item.value("file").toString() == fn && item.contains("size")) {
                        return item.value("size").toString();
                    }
                }
            }
            return QString();
        };

        QString registrySize = findSize(categories, filename);
        if (!registrySize.isEmpty()) {
            return registrySize;
        }
    }

    if (defaultSizeStr.isEmpty()) return QStringLiteral("Unknown size");

    QRegularExpression regex("^([\\d.]+)\\s*(MB|GB|KB)$", QRegularExpression::CaseInsensitiveOption);
    auto match = regex.match(defaultSizeStr);
    if (!match.hasMatch()) return defaultSizeStr;

    double val = match.captured(1).toDouble();
    QString unit = match.captured(2).toUpper();
    double bytes = val;
    if (unit == "GB") bytes *= 1024;
    else if (unit == "KB") bytes /= 1024;

    auto getBits = [](const QString &fn) -> double {
        QString lower = fn.toLower();
        if (lower.contains("q4_k_m") || lower.contains("q4_k") || lower.contains("q4_0")) return 4.5;
        if (lower.contains("q8_0") || lower.contains("q8")) return 8.5;
        if (lower.contains("bf16") || lower.contains("f16")) return 16.0;
        if (lower.contains("f32")) return 32.0;
        if (lower.contains("tokenizer")) return 32.0;
        return 16.0;
    };

    double defaultBits = getBits(defaultFile);
    double targetBits = getBits(filename);

    double targetBytes = bytes * (targetBits / defaultBits);
    if (targetBytes >= 1024) {
        return QString::number(targetBytes / 1024.0, 'f', 2) + QStringLiteral(" GB");
    } else {
        return QString::number(qRound(targetBytes)) + QStringLiteral(" MB");
    }
}

void CapabilityFamilyModel::selectFileForRequirement(const QString &familyId, const QString &reqFile, const QString &selectedFile)
{
    Logger::info(QStringLiteral("CapabilityFamilyModel"),
                 QStringLiteral("selectFileForRequirement familyId: %1, reqFile: %2, selectedFile: %3")
                 .arg(familyId, reqFile, selectedFile));

    if (familyId.isEmpty() || reqFile.isEmpty() || selectedFile.isEmpty()) {
        Logger::warning(QStringLiteral("CapabilityFamilyModel"),
                       QStringLiteral("selectFileForRequirement ignored: familyId, reqFile, or selectedFile is empty"));
        return;
    }
    if (m_userSelectedFiles.contains(familyId) && m_userSelectedFiles[familyId].value(reqFile).toString() == selectedFile) {
        return; // No change
    }
    m_userSelectedFiles[familyId][reqFile] = selectedFile;
    refresh();
}

void CapabilityFamilyModel::setInitialSelectedFiles(const QString &familyId, const QVariantMap &initialSelected)
{
    Logger::info(QStringLiteral("CapabilityFamilyModel"),
                 QStringLiteral("setInitialSelectedFiles familyId: %1, count: %2")
                 .arg(familyId).arg(initialSelected.size()));

    if (familyId.isEmpty()) return;

    // An empty map means that the gallery has no initial file override for
    // this family.  It is emitted on every family-card click by several QML
    // hosts.  It must never clear a previous per-family choice or rebuild the
    // complete catalogue on the GUI thread.
    if (initialSelected.isEmpty()) {
        return;
    }

    QVariantMap resolvedSelections;

    QVariantMap familyMap = itemForFamily(familyId);
    if (!familyMap.isEmpty()) {
        QVariantList reqFiles = familyMap.value(QStringLiteral("rawMetadata")).toMap().value(QStringLiteral("requiredFiles")).toList();
        for (const QVariant &reqVal : reqFiles) {
            QVariantMap req = reqVal.toMap();
            QString role = req.value(QStringLiteral("role")).toString();
            QString reqFile = req.value(QStringLiteral("file")).toString();
            if (initialSelected.contains(role)) {
                QString chosenFile = initialSelected.value(role).toString().trimmed();
                const QVariantList candidates = req.value(QStringLiteral("candidates")).toList();
                const bool knownCandidate = candidates.isEmpty()
                    ? chosenFile == reqFile
                    : candidates.contains(chosenFile);
                if (chosenFile.isEmpty() || !knownCandidate) {
                    continue;
                }
                resolvedSelections.insert(reqFile, chosenFile);
            }
        }
    }

    // QML re-sends an empty map whenever the gallery highlights a family.
    // Rebuilding every catalog card for that no-op causes synchronous file and
    // runtime checks on the GUI thread.  Only invalidate the model when the
    // effective per-family selection has actually changed.
    const QVariantMap existingSelections = m_userSelectedFiles.value(familyId);
    if (existingSelections == resolvedSelections) {
        return;
    }
    // Invalid or stale input must be harmless as well.  It is not a request
    // to erase a selection, and treating it as one used to trigger a costly
    // reset while the user was merely changing the highlighted model.
    if (resolvedSelections.isEmpty()) return;

    m_userSelectedFiles.insert(familyId, resolvedSelections);
    refresh();
}

void CapabilityFamilyModel::saveSelectionForFamily(const QString &familyId,
                                                   const QString &runtimeId,
                                                   const QString &runtimeVersion,
                                                   const QVariantMap &selectedFiles)
{
    if (!m_selectionRepository || familyId.isEmpty()) {
        return;
    }

    QVariantMap family;
    for (const FamilyItem &item : std::as_const(m_items)) {
        if (item.id == familyId) {
            family = item.rawMap;
            break;
        }
    }
    if (family.isEmpty() && m_registry) {
        const QVariantList all = m_registry->ttsFamilies() + m_registry->sttFamilies() + m_registry->llmFamilies();
        for (const QVariant &itemVal : all) {
            const QVariantMap candidate = itemVal.toMap();
            if (candidate.value(QStringLiteral("id")).toString() == familyId) {
                family = candidate;
                break;
            }
        }
    }
    if (family.isEmpty()) {
        return;
    }

    StudioConfiguration selection;
    selection.capabilityId = capabilityForFamily(family);
    selection.familyId = familyId;
    selection.runtimeId = runtimeId;
    selection.runtimeVersion = runtimeVersion;
    selection.selectedFiles = selectedFiles;
    QString saveError;
    if (!m_selectionRepository->saveActiveSelection(selection, &saveError)) {
        Logger::error(QStringLiteral("CapabilityFamilyModel"), saveError);
        return;
    }

    if (m_settings) {
        if (selection.capabilityId == QStringLiteral("stt")) {
            m_settings->setSelectedSttFamily(familyId);
            m_settings->setSelectedSttRuntime(runtimeId);
            m_settings->setSelectedSttRuntimeVersion(runtimeVersion);
            const QString modelFile = selectedFiles.value(QStringLiteral("model")).toString();
            if (!modelFile.isEmpty()) {
                m_settings->setSelectedSttModelFile(modelFile);
            }
        } else if (selection.capabilityId == QStringLiteral("forced-alignment")) {
            // Active capability selections are the source of truth for tools.
        } else {
            if (selection.capabilityId == QStringLiteral("voice-cloning")) {
                m_settings->setSelectedVoiceCloneFamily(familyId);
            } else {
                m_settings->setSelectedTtsFamily(familyId);
            }
            m_settings->setSelectedTtsRuntime(runtimeId);
            m_settings->setSelectedTtsRuntimeVersion(runtimeVersion);
        }
    }
}

