void CapabilityFamilyModel::updateItems()
{
    m_items.clear();
    if (!m_registry) return;

    QVariantList all;
    if (m_capabilityId == QStringLiteral("all")) {
        // Combined list without duplicates
        QVariantList combined = m_registry->ttsFamilies() + m_registry->sttFamilies() + m_registry->llmFamilies();
        QHash<QString, bool> seen;
        for (const QVariant &itemVal : combined) {
            QVariantMap family = itemVal.toMap();
            QString id = family.value(QStringLiteral("id")).toString();
            if (!seen.contains(id)) {
                seen.insert(id, true);
                all.append(itemVal);
            }
        }
    } else {
        const QString domain = StudioCapabilityRegistry::instance()->familyDomain(m_capabilityId);
        all = domain == QStringLiteral("stt")
            ? m_registry->sttFamilies()
            : (domain == QStringLiteral("llm") ? m_registry->llmFamilies()
                                                : m_registry->ttsFamilies());
    }

    QList<QPair<QString, QString>> rawLangs;
    for (const QVariant &itemVal : all) {
        QVariantMap family = itemVal.toMap();
        bool supports = false;
        if (m_capabilityId == QStringLiteral("all")) {
            supports = true;
        } else {
            supports = StudioCapabilityRegistry::instance()->familySupportsCapability(family, m_capabilityId);
        }
        if (!supports) continue;
        collectLanguagesForFamily(family, rawLangs);
    }

    QVariantList finalLangs;
    QVariantMap allLangsItem;
    allLangsItem.insert(QStringLiteral("text"), QStringLiteral("All Languages"));
    allLangsItem.insert(QStringLiteral("value"), QStringLiteral("all"));
    finalLangs.append(allLangsItem);

    QHash<QString, QString> uniqueMap;
    for (const auto &pair : rawLangs) {
        QString code = canonicalLanguageCode(pair.first);
        QString name = pair.second.trimmed();
        if (code.isEmpty()) continue;
        if (!uniqueMap.contains(code) || (uniqueMap.value(code) == code && name != code)) {
            uniqueMap.insert(code, name);
        }
    }

    QList<QVariantMap> sortedLangs;
    for (auto it = uniqueMap.constBegin(); it != uniqueMap.constEnd(); ++it) {
        QVariantMap item;
        item.insert(QStringLiteral("value"), it.key());
        QString name = it.value();
        if (name.contains(QStringLiteral(" ("))) {
            name = name.left(name.indexOf(QStringLiteral(" ("))).trimmed();
        }
        item.insert(QStringLiteral("text"), name);
        sortedLangs.append(item);
    }

    std::sort(sortedLangs.begin(), sortedLangs.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("text")).toString().compare(b.value(QStringLiteral("text")).toString(), Qt::CaseInsensitive) < 0;
    });

    for (const auto &item : sortedLangs) {
        finalLangs.append(item);
    }

    if (m_availableLanguages != finalLangs) {
        m_availableLanguages = finalLangs;
        emit availableLanguagesChanged();
    }

    for (const QVariant &itemVal : all) {
        QVariantMap family = itemVal.toMap();
        QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();

        bool supports = false;
        if (m_capabilityId == QStringLiteral("all")) {
            supports = true;
        } else {
            supports = StudioCapabilityRegistry::instance()->familySupportsCapability(family, m_capabilityId);
        }

        if (!supports) continue;

        FamilyItem item;
        item.rawMap = family;
        item.id = family.value(QStringLiteral("id")).toString();
        item.displayName = family.value(QStringLiteral("title")).toString();
        item.subtitle = family.value(QStringLiteral("subtitle")).toString();
        item.description = family.value(QStringLiteral("description")).toString();
        item.accent = family.value(QStringLiteral("accent")).toString();
        item.modelCardUrl = modelCardUrl(family);
        item.readmeContent = readmeContent(family);
        item.thumbnailSource = thumbnailSource(family);
        item.iconName = familyIconName(family);
        item.familyCapability = familyCapability(family);
        item.infoBadges = modelInfoBadges(family);
        item.capabilityBadges = capabilityBadges(family);
        item.statsBadges = statsBadges(family);
        item.isLastudioPick = family.value(QStringLiteral("isLastudioPick")).toBool();
        item.pickLabel = family.value(QStringLiteral("pickLabel")).toString();
        if (item.isLastudioPick && item.pickLabel.isEmpty())
            item.pickLabel = QStringLiteral("LA Studio Pick");
        item.pickReason = family.value(QStringLiteral("pickReason")).toString();
        item.supported = true;
        item.selected = (item.id == m_selectedFamilyId);

        if (!m_searchText.trimmed().isEmpty()) {
            const QString searchable = QStringList{
                item.id,
                item.displayName,
                item.subtitle,
                family.value(QStringLiteral("modelId")).toString(),
                family.value(QStringLiteral("tags")).toStringList().join(QLatin1Char(' '))
            }.join(QLatin1Char(' '));
            if (!searchable.contains(m_searchText.trimmed(), Qt::CaseInsensitive)) {
                continue;
            }
        }

        if (!m_languageFilter.isEmpty() && m_languageFilter != QStringLiteral("all")) {
            if (!familySupportsLanguage(family, m_languageFilter)) {
                continue;
            }
        }

        // Calculate missing files
        QVariantList reqFiles = family.value(QStringLiteral("requiredFiles")).toList();
        bool missingAnyFiles = false;
        QVariantList missingReqs;
        QVariantList requiredFileOptions;
        QVariantMap selectedFiles;

        QString activeCap = capabilityForFamily(family);
        QString sourceModelId = family.value(QStringLiteral("modelId")).toString();
        const StudioConfiguration storedSelection = m_selectionRepository
            ? m_selectionRepository->selectionFor(activeCap)
            : StudioConfiguration{};
        const bool useStoredSelection = storedSelection.isValid() && storedSelection.familyId == item.id;
        const QVariantMap storedReqSelections = storedFilesByRequirement(family, activeCap, item.id);

        for (const QVariant &reqVal : reqFiles) {
            QVariantMap req = reqVal.toMap();
            if (!requirementRequiredForCapability(req, activeCap)) continue;

            QString role = req.value(QStringLiteral("role")).toString();
            QString reqFile = req.value(QStringLiteral("file")).toString();
            QVariantList candidates = req.value(QStringLiteral("candidates")).toList();

            QString selectedFile;
            bool hasUserSel = false;
            if (m_userSelectedFiles.contains(item.id)) {
                const QVariantMap &familySel = m_userSelectedFiles.value(item.id);
                if (familySel.contains(reqFile)) {
                    selectedFile = familySel.value(reqFile).toString().trimmed();
                    hasUserSel = !selectedFile.isEmpty();
                }
            }
            if (!hasUserSel && storedReqSelections.contains(reqFile)) {
                selectedFile = storedReqSelections.value(reqFile).toString().trimmed();
                hasUserSel = !selectedFile.isEmpty();
            }

            if (hasUserSel) {
                bool knownCandidate = candidates.isEmpty()
                    ? selectedFile == reqFile
                    : candidates.contains(selectedFile);
                if (!knownCandidate) {
                    // Ignore stale cross-family or removed catalog variants even
                    // when the old file is still present on disk. Native backends
                    // may reject variants removed for compatibility reasons.
                    selectedFile.clear();
                    hasUserSel = false;
                }
            }

            bool foundInstalled = false;
            if (hasUserSel) {
                foundInstalled = isFileInstalled(family, selectedFile, req);
            } else {
                if (!candidates.isEmpty()) {
                    for (const QVariant &cand : candidates) {
                        if (isFileInstalled(family, cand.toString(), req)) {
                            selectedFile = cand.toString();
                            foundInstalled = true;
                            break;
                        }
                    }
                } else {
                    selectedFile = reqFile;
                    foundInstalled = isFileInstalled(family, selectedFile, req);
                }
                if (!foundInstalled) {
                    selectedFile = recommendedFileForRequirement(family, req, candidates);
                }
            }

            selectedFiles.insert(role, selectedFile);

            QString reqModelId = req.value(QStringLiteral("modelId")).toString();
            if (reqModelId.isEmpty()) {
                reqModelId = sourceModelId;
            }
            const bool validInstalledFile = isFileInstalled(family, selectedFile, req);
            int installState = validInstalledFile ? 3 : 0; // Installed / NotInstalled
            AppController *app = AppController::instance();
            if (app && app->downloadInstall()) {
                installState = app->downloadInstall()->modelFileState(reqModelId, selectedFile);
            }
            if ((installState == 3 || installState == 5) && !validInstalledFile) {
                installState = 0;
            }
            foundInstalled = (installState == 3 || installState == 5); // Installed or update available

            QVariantMap requiredFile = req;
            requiredFile[QStringLiteral("selectedFile")] = selectedFile;
            requiredFile[QStringLiteral("installed")] = foundInstalled;
            requiredFile[QStringLiteral("installState")] = installState;
            requiredFile[QStringLiteral("statusText")] = installState == 5
                ? QStringLiteral("Update available")
                : (foundInstalled ? QStringLiteral("Installed") : QStringLiteral("Not installed"));
            requiredFile[QStringLiteral("selectedSize")] = estimateSize(
                selectedFile,
                req.value(QStringLiteral("file")).toString(),
                req.value(QStringLiteral("size")).toString());
            requiredFileOptions.append(requiredFile);

            if (!foundInstalled) {
                missingAnyFiles = true;
                QVariantMap missing;
                missing[QStringLiteral("role")] = role;
                missing[QStringLiteral("displayName")] = req.value(QStringLiteral("name")).toString();
                missing[QStringLiteral("file")] = selectedFile;
                missing[QStringLiteral("purpose")] = req.value(QStringLiteral("purpose")).toString();
                missingReqs.append(missing);
            }
        }
        item.missingRequirements = missingReqs;
        item.requiredFiles = requiredFileOptions;
        item.selectedFiles = selectedFiles;

        // Runtime check
        QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
        bool hasCompatibleRuntime = false;
        bool hasInstalledCompatibleRuntime = false;
        QVariantList options;
        QHash<QString, QVariantList> runtimesById;
        QStringList runtimeIdOrder;

        for (const QVariant &rtVal : runtimes) {
            QVariantMap rt = rtVal.toMap();
            QString runtimeId = rt.value(QStringLiteral("id")).toString();
            if (runtimeId.isEmpty())
                continue;
            if (!runtimesById.contains(runtimeId)) {
                runtimeIdOrder.append(runtimeId);
            }
            runtimesById[runtimeId].append(rt);
        }

        const QString savedRuntimeId = useStoredSelection
            ? storedSelection.runtimeId
            : (activeCap == QStringLiteral("stt")
            ? (m_settings ? m_settings->selectedSttRuntime() : QString())
            : (m_settings ? m_settings->selectedTtsRuntime() : QString()));
        const QString savedRuntimeVersion = useStoredSelection
            ? storedSelection.runtimeVersion
            : (activeCap == QStringLiteral("stt")
            ? (m_settings ? m_settings->selectedSttRuntimeVersion() : QString())
            : (m_settings ? m_settings->selectedTtsRuntimeVersion() : QString()));

        for (const QString &runtimeId : runtimeIdOrder) {
            const QVariantList runtimeEntries = runtimesById.value(runtimeId);
            if (runtimeEntries.isEmpty())
                continue;

            QVariantList installedVers = m_runtimes->runtimeVersions(runtimeId);
            QString latestVersion;
            QVariantMap latestRuntime;
            QVariantList availableVersions;
            QVariantList versionOptions;
            QString selectedInstalledVersion;
            QString highestInstalledVersion;
            QVariantMap selectedInstalledRuntime;
            QVariantMap highestInstalledRuntime;
            bool hasInstalling = false;
            bool hasDownloading = false;
            int latestInstallState = 0;
            bool latestInstalled = false;
            bool installed = false;

            for (const QVariant &entryVal : runtimeEntries) {
                QVariantMap rt = entryVal.toMap();
                const QString reqVer = rt.value(QStringLiteral("version")).toString();
                availableVersions.append(reqVer);
                versionOptions.append(rt);

                if (runtimeVersionGreater(reqVer, latestVersion)) {
                    latestVersion = reqVer;
                    latestRuntime = rt;
                }

                // Filesystem fallback:
                // Runtime extraction + manifest write can complete before runtime registry refresh
                // propagates to this model. If runtimeVersions() is temporarily empty but the
                // expected runtime library already exists in backends/, treat it as installed.
                QString installedPath;
                const QString runtimeKind = rt.value(QStringLiteral("kind"), QStringLiteral("dynamic-library")).toString();
                const QString runtimeFile = runtimeKind == QStringLiteral("process")
                    ? rt.value(QStringLiteral("entrypoint")).toString()
                    : rt.value(QStringLiteral("library")).toString();
                const QString engineFamily = rt.value(QStringLiteral("engineFamily")).toString();
                if (!runtimeFile.isEmpty() && !engineFamily.isEmpty() && runtimeId.startsWith(engineFamily + QStringLiteral("-"))) {
                    const QString variant = runtimeId.mid(engineFamily.length() + 1);
                    const QString versionSuffix = reqVer.isEmpty() ? QString() : (QStringLiteral("-") + reqVer);
                    const QString runtimeDir = PathUtils::backendsDir()
                        + QStringLiteral("/") + engineFamily
                        + QStringLiteral("/") + variant + versionSuffix;
                    installedPath = runtimeDir + QStringLiteral("/") + runtimeFile;
                }

                int versionInstallState = 0; // NotInstalled
                AppController *appInstance = AppController::instance();
                if (appInstance && appInstance->downloadInstall()) {
                    versionInstallState = appInstance->downloadInstall()->runtimeState(
                        runtimeId,
                        reqVer,
                        installedPath,
                        rt.value(QStringLiteral("asset")).toString());
                }
                if (reqVer == latestVersion)
                    latestInstallState = versionInstallState;
                if (versionInstallState == 2)
                    hasInstalling = true;
                if (versionInstallState == 1)
                    hasDownloading = true;
                if (versionInstallState == 3 || installedRuntimeVersion(installedVers, reqVer)) {
                    installed = true;
                    if (reqVer == latestVersion)
                        latestInstalled = true;
                    if (runtimeId == savedRuntimeId && reqVer == savedRuntimeVersion) {
                        selectedInstalledVersion = reqVer;
                        selectedInstalledRuntime = rt;
                    }
                    if (runtimeVersionGreater(reqVer, highestInstalledVersion)) {
                        highestInstalledVersion = reqVer;
                        highestInstalledRuntime = rt;
                    }
                }
            }

            if (latestRuntime.isEmpty())
                latestRuntime = runtimeEntries.first().toMap();

            const QString selectedVersion = !selectedInstalledVersion.isEmpty()
                ? selectedInstalledVersion
                : highestInstalledVersion;
            QVariantMap displayRuntime = !selectedInstalledRuntime.isEmpty()
                ? selectedInstalledRuntime
                : highestInstalledRuntime;

            QString resolvedSelectedVersion = selectedVersion;
            if (!installed && !installedVers.isEmpty()) {
                installed = true;
                for (const QVariant &installedVal : installedVers) {
                    const QString installedVersion = installedVal.toMap().value(QStringLiteral("version")).toString();
                    if (runtimeId == savedRuntimeId && installedVersion == savedRuntimeVersion) {
                        resolvedSelectedVersion = installedVersion;
                        break;
                    }
                    if (runtimeVersionGreater(installedVersion, resolvedSelectedVersion)) {
                        resolvedSelectedVersion = installedVersion;
                    }
                }
            }
            if (displayRuntime.isEmpty() && installed) {
                for (const QVariant &entryVal : runtimeEntries) {
                    const QVariantMap rt = entryVal.toMap();
                    if (rt.value(QStringLiteral("version")).toString() == resolvedSelectedVersion) {
                        displayRuntime = rt;
                        break;
                    }
                }
            }

            int installState = installed ? 3 : latestInstallState;
            if (!installed && installState == 0) {
                if (hasInstalling)
                    installState = 2;
                else if (hasDownloading)
                    installState = 1;
            }

            QVariantMap comp = HardwareManager::instance()->runtimeCompatibility(latestRuntime);
            bool isComp = comp.value(QStringLiteral("compatible")).toBool();

            if (isComp) {
                hasCompatibleRuntime = true;
                if (installed) {
                    hasInstalledCompatibleRuntime = true;
                }
            }

            QVariantMap opt = installed && !displayRuntime.isEmpty() ? displayRuntime : latestRuntime;
            opt[QStringLiteral("version")] = installed ? resolvedSelectedVersion : latestVersion;
            opt[QStringLiteral("latestVersion")] = latestVersion;
            opt[QStringLiteral("latestInstalled")] = latestInstalled;
            opt[QStringLiteral("latestInstallState")] = latestInstallState;
            opt[QStringLiteral("defaultVersion")] = latestVersion;
            opt[QStringLiteral("availableVersions")] = availableVersions;
            opt[QStringLiteral("versionOptions")] = versionOptions;
            opt[QStringLiteral("compatible")] = isComp;
            opt[QStringLiteral("installed")] = installed;
            opt[QStringLiteral("installState")] = installState;
            opt[QStringLiteral("compatibilityTitle")] = comp.value(QStringLiteral("title")).toString();
            opt[QStringLiteral("compatibilityDetail")] = comp.value(QStringLiteral("detail")).toString();
            opt[QStringLiteral("description")] = runtimeDescription(opt);
            opt[QStringLiteral("iconName")] = runtimeIconName(opt);
            options.append(opt);
        }
        item.runtimeOptions = options;

        const auto preferred = preferredRuntime(options);
        item.preferredRuntimeId = preferred.first;
        item.preferredRuntimeVersion = preferred.second;
        if (useStoredSelection && !storedSelection.runtimeId.isEmpty()) {
            for (const QVariant &optionValue : options) {
                const QVariantMap option = optionValue.toMap();
                if (option.value(QStringLiteral("id")).toString() == storedSelection.runtimeId &&
                    option.value(QStringLiteral("installed")).toBool()) {
                    const QString optionVersion = option.value(QStringLiteral("version")).toString();
                    if (storedSelection.runtimeVersion.isEmpty() || storedSelection.runtimeVersion == optionVersion) {
                        item.preferredRuntimeId = storedSelection.runtimeId;
                        item.preferredRuntimeVersion = optionVersion;
                        break;
                    }
                }
            }
        }
        item.selectedRuntimeId = item.preferredRuntimeId;
        item.selectedRuntimeVersion = item.preferredRuntimeVersion;

        item.installed = !missingAnyFiles && hasInstalledCompatibleRuntime;

        if (runtimes.isEmpty()) {
            // No runtime required by model family (e.g. built-in only or CPU)
            item.ready = !missingAnyFiles;
            item.statusReason = item.ready ? QStringLiteral("Ready") : QStringLiteral("Setup Required");
        } else if (!hasCompatibleRuntime) {
            item.ready = false;
            item.statusReason = QStringLiteral("Incompatible");
        } else if (missingAnyFiles || !hasInstalledCompatibleRuntime) {
            item.ready = false;
            item.statusReason = QStringLiteral("Setup Required");
        } else {
            item.ready = true;
            item.statusReason = QStringLiteral("Ready");
        }
        item.statusKind = statusKind(item.ready, item.statusReason);
        item.statusTitle = item.statusReason.isEmpty() ? QStringLiteral("Setup Required") : item.statusReason;

        if (m_statusFilter == QStringLiteral("installed") && !item.ready) {
            continue;
        }
        if (m_statusFilter == QStringLiteral("missing") && item.ready) {
            continue;
        }

        m_items.append(item);
    }
}

