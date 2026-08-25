void DownloadInstallService::onDownloadFinished(const QString &modelId,
                                                const QString &filename,
                                                const QString &localPath,
                                                const QVariantMap &metadata)
{
    QString task = QStringLiteral("stt");
    QString format = QStringLiteral("bin");
    if (filename.endsWith(QStringLiteral(".gguf"))) format = QStringLiteral("gguf");
    else if (filename.endsWith(QStringLiteral(".onnx"))) format = QStringLiteral("onnx");
    
    if (modelId.contains(QStringLiteral("tts"), Qt::CaseInsensitive) || 
        modelId.contains(QStringLiteral("parler"), Qt::CaseInsensitive) || 
        modelId.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive) || 
        modelId.contains(QStringLiteral("vibevoice"), Qt::CaseInsensitive) || 
        modelId.contains(QStringLiteral("kokoro"), Qt::CaseInsensitive) ||
        modelId.contains(QStringLiteral("qwen3-tts"), Qt::CaseInsensitive) ||
        filename.contains(QStringLiteral("tts"), Qt::CaseInsensitive) || 
        filename.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive) ||
        filename.contains(QStringLiteral("vibevoice"), Qt::CaseInsensitive) ||
        filename.contains(QStringLiteral("kokoro"), Qt::CaseInsensitive) ||
        filename.contains(QStringLiteral("qwen3-tts"), Qt::CaseInsensitive)) {
        task = QStringLiteral("tts");
    }

    if (filename.contains(QStringLiteral("asr"), Qt::CaseInsensitive) || 
        filename.contains(QStringLiteral("stt"), Qt::CaseInsensitive) ||
        modelId.contains(QStringLiteral("asr"), Qt::CaseInsensitive) || 
        modelId.contains(QStringLiteral("stt"), Qt::CaseInsensitive)) {
        task = QStringLiteral("stt");
    }

    QFileInfo fi(localPath);
    QString dirPath = fi.absolutePath();
    bool isRuntime = dirPath.contains(QStringLiteral("backends"));
    const bool isRuntimeDependency =
        metadata.value(QStringLiteral("kind")).toString() == QStringLiteral("runtimeDependency");
    const QString dependencyRuntimeDir = metadata.value(QStringLiteral("runtimeDir")).toString();
    const QString expectedSha256 = normalizedSha256(metadata);

    // Runtime code and its dependencies are executable content. They must be
    // authenticated before an installer, extractor, or runtime scanner can see them.
    if (expectedSha256.isEmpty()) {
        if (isRuntime || isRuntimeDependency) {
            QFile::remove(localPath);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing runtime download without a SHA-256: %1").arg(filename));
            emit errorOccurred(QStringLiteral("Runtime download is missing a SHA-256: ") + filename);
            return;
        }
    } else {
        QString actualSha256;
        if (!fileMatchesSha256(localPath, expectedSha256, &actualSha256)) {
            QFile::remove(localPath);
            const QString detail = actualSha256.isEmpty()
                ? QStringLiteral("Could not calculate SHA-256")
                : QStringLiteral("Expected %1 but got %2").arg(expectedSha256, actualSha256);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing download with checksum mismatch: %1 (%2)")
                              .arg(filename, detail));
            emit errorOccurred(QStringLiteral("Download checksum does not match: ") + filename);
            return;
        }
    }

    if (isRuntimeDependency) {
        const QString dependency = metadata.value(QStringLiteral("dependency")).toString();
        const QString runtimeDir = metadata.value(QStringLiteral("runtimeDir")).toString();
        if (dependency == QStringLiteral("espeak-ng") &&
            filename.endsWith(QStringLiteral(".msi"), Qt::CaseInsensitive) &&
            !runtimeDir.isEmpty()) {
            if (!hasTrustedAuthenticodeSignature(localPath)) {
                QFile::remove(localPath);
                Logger::error(QStringLiteral("DownloadInstallService"),
                              QStringLiteral("Refusing eSpeak NG MSI without a trusted Authenticode signature: %1")
                                  .arg(filename));
                emit errorOccurred(QStringLiteral("eSpeak NG MSI signature verification failed: ") + filename);
                return;
            }

            const QString msiexec = systemMsiexecPath();
            if (msiexec.isEmpty()) {
                QFile::remove(localPath);
                Logger::error(QStringLiteral("DownloadInstallService"),
                              QStringLiteral("Windows Installer executable was not found in the system directory"));
                emit errorOccurred(QStringLiteral("Windows Installer is unavailable"));
                return;
            }

            const QString targetDir = QDir(runtimeDir).absoluteFilePath(QStringLiteral("espeak-ng"));
            QDir().mkpath(targetDir);

            QProcess *process = new QProcess(this);
            process->setProgram(msiexec);
            process->setArguments({
                QStringLiteral("/a"),
                QDir::toNativeSeparators(localPath),
                QStringLiteral("/qn"),
                QStringLiteral("TARGETDIR=%1").arg(QDir::toNativeSeparators(targetDir))
            });

            QPointer<DownloadInstallService> weakThis(this);
            connect(process, &QProcess::finished, this,
                    [weakThis, process, filename, targetDir, localPath](int exitCode, QProcess::ExitStatus status) {
                process->deleteLater();
                if (!weakThis) return;
                if (exitCode == 0 && status == QProcess::NormalExit) {
                    Logger::info(QStringLiteral("DownloadInstallService"),
                                 QStringLiteral("Extracted %1 dependency to %2").arg(filename, targetDir));
                    QFile::remove(localPath);
                    weakThis->m_runtimes->scanRuntimes();
                } else {
                    Logger::error(QStringLiteral("DownloadInstallService"),
                                  QStringLiteral("Failed to extract runtime dependency %1").arg(filename));
                    emit weakThis->errorOccurred(QStringLiteral("Failed to extract runtime dependency: ") + filename);
                }
            });

            process->start();
            return;
        }
    }

    if (filename.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tgz"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tar.bz2"), Qt::CaseInsensitive) ||
        filename.endsWith(QStringLiteral(".tbz2"), Qt::CaseInsensitive)) {
        if (QFileInfo(localPath).size() == 0) {
            QFile::remove(localPath);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing to extract empty archive: %1").arg(filename));
            emit errorOccurred(QStringLiteral("Refusing to extract empty archive: ") + filename);
            return;
        }
        if (!hasExpectedArchiveSignature(localPath, filename)) {
            QFile::remove(localPath);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing to extract invalid archive: %1").arg(filename));
            emit errorOccurred(QStringLiteral("Refusing to extract invalid archive: ") + filename);
            return;
        }

        QString extractName = fi.completeBaseName();
        if (filename.endsWith(QStringLiteral(".tar.gz"), Qt::CaseInsensitive) ||
            filename.endsWith(QStringLiteral(".tar.bz2"), Qt::CaseInsensitive)) {
            QFileInfo tarFi(fi.completeBaseName());
            extractName = tarFi.completeBaseName();
        }
        if (isRuntimeDependency && !dependencyRuntimeDir.isEmpty()) {
            dirPath = dependencyRuntimeDir;
            extractName.prepend(QStringLiteral(".dependency-"));
        } else if (isRuntime && metadata.contains("id") && metadata.contains("version")) {
            QString runtimeId = metadata.value("id").toString();
            QString runtimeVersion = metadata.value("version").toString();
            QString engineFamily = metadata.value("engineFamily").toString();

            if (engineFamily.isEmpty()) {
                for (const auto &plat : {QStringLiteral("-win-"), QStringLiteral("-linux-"), QStringLiteral("-macos-")}) {
                    int idx = runtimeId.indexOf(plat);
                    if (idx > 0) { engineFamily = runtimeId.left(idx); break; }
                }
                if (engineFamily.isEmpty()) engineFamily = runtimeId;
            }

            QString variant = runtimeId;
            if (runtimeId.startsWith(engineFamily + QStringLiteral("-"))) {
                variant = runtimeId.mid(engineFamily.length() + 1);
            }

            QString familyDir = dirPath + QStringLiteral("/") + engineFamily;
            QDir().mkpath(familyDir);
            extractName = variant + QStringLiteral("-") + runtimeVersion;
            dirPath = familyDir;
        }
        QString extractDir = dirPath + QStringLiteral("/") + extractName;

        const QString bsdtar = archiveExtractor(QStringLiteral("bsdtar.exe"));
        if (bsdtar.isEmpty()) {
            QFile::remove(localPath);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing archive extraction because bundled bsdtar.exe is missing"));
            emit errorOccurred(QStringLiteral("Bundled archive extractor is missing. Reinstall LA Studio."));
            return;
        }
        QString archiveInspectionError;
        qint64 unpackedBytes = 0;
        if (!archiveContainsOnlySafeMembers(bsdtar, localPath, &unpackedBytes, &archiveInspectionError)) {
            QFile::remove(localPath);
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing unsafe archive %1: %2").arg(filename, archiveInspectionError));
            emit errorOccurred(QStringLiteral("Refusing unsafe archive: ") + filename);
            return;
        }
        if (!hasSpaceForExtraction(extractDir, unpackedBytes, &archiveInspectionError)) {
            Logger::error(QStringLiteral("DownloadInstallService"),
                          QStringLiteral("Refusing extraction of %1: %2").arg(filename, archiveInspectionError));
            emit errorOccurred(archiveInspectionError);
            return;
        }
        QDir().mkpath(extractDir);

        if (isRuntime) {
            QString runtimeId = metadata.value("id").toString();
            QString runtimeVersion = metadata.value("version").toString();
            if (!runtimeId.isEmpty()) {
                m_activeExtractions.insert(runtimeId + QStringLiteral("::") + runtimeVersion);
                emit installStatesChanged();
            }
        }

        QProcess *process = new QProcess(this);
        process->setProgram(bsdtar);
        process->setArguments({QStringLiteral("-xf"), localPath, QStringLiteral("-C"), extractDir});
        process->setProcessChannelMode(QProcess::MergedChannels);
        
        QPointer<DownloadInstallService> weakThis(this);
        connect(process, &QProcess::finished, this, [weakThis, process, isRuntime, isRuntimeDependency, dependencyRuntimeDir, task, format, dirPath, filename, fi, extractDir, metadata, localPath, modelId](int exitCode, QProcess::ExitStatus status) {
            const QString output = QString::fromLocal8Bit(process->readAll()).trimmed();
            process->deleteLater();
            if (!weakThis) return;

            if (isRuntime) {
                QString runtimeId = metadata.value("id").toString();
                QString runtimeVersion = metadata.value("version").toString();
                if (!runtimeId.isEmpty()) {
                    weakThis->m_activeExtractions.remove(runtimeId + QStringLiteral("::") + runtimeVersion);
                    emit weakThis->installStatesChanged();
                }
            }

            if (exitCode == 0 && status == QProcess::NormalExit) {
                Logger::info(QStringLiteral("DownloadInstallService"), QStringLiteral("Extracted %1 to %2").arg(filename, extractDir));

                QString containmentError;
                if (!extractedTreeIsContained(extractDir, &containmentError)) {
                    QDir(extractDir).removeRecursively();
                    QFile::remove(localPath);
                    Logger::error(QStringLiteral("DownloadInstallService"),
                                  QStringLiteral("Rejected extracted archive %1: %2").arg(filename, containmentError));
                    emit weakThis->errorOccurred(QStringLiteral("Extracted archive contains an unsafe path: ") + filename);
                    return;
                }
                
                QFile::remove(localPath);

                if (isRuntimeDependency) {
                    if (dependencyRuntimeDir.isEmpty() ||
                        !mergeDirectoryContents(extractDir, dependencyRuntimeDir)) {
                        Logger::error(QStringLiteral("DownloadInstallService"),
                                      QStringLiteral("Failed to install runtime dependency %1").arg(filename));
                        emit weakThis->errorOccurred(QStringLiteral("Failed to install runtime dependency: ") + filename);
                        return;
                    }
                    QDir(extractDir).removeRecursively();
                    weakThis->m_runtimes->scanRuntimes();
                } else if (isRuntime) {
                    QDir dir(extractDir);
                    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    if (subdirs.size() == 1 && dir.entryList(QDir::Files).isEmpty()) {
                        QString subName = subdirs.first();
                        QDir subDir(dir.absoluteFilePath(subName));
                        QStringList entries = subDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                        for (const auto &entry : entries) {
                            QFile::rename(subDir.absoluteFilePath(entry), dir.absoluteFilePath(entry));
                        }
                        dir.rmdir(subName);
                    }

                    QFile manifestFile(dir.absoluteFilePath(QStringLiteral("backend-manifest.json")));
                    QJsonObject manifest;
                    bool manifestWasValid = false;
                    if (manifestFile.exists()) {
                        if (manifestFile.open(QIODevice::ReadOnly)) {
                            auto doc = QJsonDocument::fromJson(manifestFile.readAll());
                            manifestFile.close();
                            if (doc.isObject()) {
                                manifest = doc.object();
                                manifestWasValid = true;
                            }
                        }
                    }

                    const QString runtimeKind = metadata.value(QStringLiteral("kind"),
                                                               manifest.value(QStringLiteral("kind")).toString()).toString();
                    if (runtimeKind == QStringLiteral("process")) {
                        const QString entrypoint = metadata.value(QStringLiteral("entrypoint"),
                                                                  manifest.value(QStringLiteral("entrypoint")).toString()).toString();
                        const QString cleanEntrypoint = QDir::cleanPath(entrypoint);
                        const bool safeEntrypoint = !entrypoint.isEmpty() && !QDir::isAbsolutePath(entrypoint) &&
                            cleanEntrypoint != QStringLiteral("..") &&
                            !cleanEntrypoint.startsWith(QStringLiteral("../")) &&
                            !cleanEntrypoint.startsWith(QStringLiteral("..\\"));
                        const QString executablePath = safeEntrypoint ? dir.absoluteFilePath(cleanEntrypoint) : QString();
                        // Official llama.cpp release archives do not ship our
                        // backend-manifest.json; the catalog metadata is the
                        // trusted source for their safe relative entrypoint.
                        const bool hasTrustedCatalogEntrypoint = metadata.contains(QStringLiteral("entrypoint"));
                        if ((!manifestWasValid && !hasTrustedCatalogEntrypoint) ||
                            !safeEntrypoint || !QFileInfo(executablePath).isFile()) {
                            dir.removeRecursively();
                            Logger::error(QStringLiteral("DownloadInstallService"),
                                          QStringLiteral("Rejected process runtime package %1: manifest or entrypoint is invalid").arg(filename));
                            emit weakThis->errorOccurred(QStringLiteral("Invalid process runtime package: ") + filename);
                            return;
                        }
                    }

                    QString runtimeId = metadata.value("id").toString();
                    manifest["id"] = runtimeId;
                    manifest["name"] = metadata.value("engineName").toString();
                    manifest["version"] = metadata.value("version").toString();
                    manifest["type"] = metadata.value("type").toString().isEmpty() ? QStringLiteral("stt") : metadata.value("type").toString();

                    QString ef = metadata.value("engineFamily").toString();
                    if (ef.isEmpty()) {
                        for (const auto &plat : {QStringLiteral("-win-"), QStringLiteral("-linux-"), QStringLiteral("-macos-")}) {
                            int idx = runtimeId.indexOf(plat);
                            if (idx > 0) { ef = runtimeId.left(idx); break; }
                        }
                        if (ef.isEmpty()) ef = runtimeId;
                    }
                    manifest["engineFamily"] = ef;
                    QString vr = runtimeId;
                    if (runtimeId.startsWith(ef + QStringLiteral("-")))
                        vr = runtimeId.mid(ef.length() + 1);
                    manifest["variant"] = vr;

                    if (metadata.contains(QStringLiteral("library"))) {
                        manifest["library"] = metadata.value(QStringLiteral("library")).toString();
                    }
                    for (const QString &field : {QStringLiteral("kind"), QStringLiteral("entrypoint"),
                                                 QStringLiteral("protocolVersion")}) {
                        if (metadata.contains(field) && !metadata.value(field).toString().isEmpty()) {
                            manifest[field] = metadata.value(field).toString();
                        }
                    }
                    for (const QString &field : {QStringLiteral("nativeDependencies"), QStringLiteral("capabilities"), QStringLiteral("modelFormats")}) {
                        if (metadata.contains(field)) {
                            manifest[field] = QJsonArray::fromVariantList(metadata.value(field).toList());
                        }
                    }
                    if (metadata.contains(QStringLiteral("metadata"))) {
                        QJsonObject newMeta = QJsonObject::fromVariantMap(
                            metadata.value(QStringLiteral("metadata")).toMap());
                        QJsonObject existingMeta = manifest.value(QStringLiteral("metadata")).toObject();
                        for (auto it = newMeta.begin(); it != newMeta.end(); ++it) {
                            existingMeta[it.key()] = it.value();
                        }
                        manifest["metadata"] = existingMeta;
                    }
                    
                    QJsonDocument doc(manifest);
                    if (manifestFile.open(QIODevice::WriteOnly)) {
                        manifestFile.write(doc.toJson(QJsonDocument::Indented));
                        manifestFile.close();
                    }

                    const QVariantList dependencyDownloads = metadata.value(QStringLiteral("dependencyDownloads")).toList();
                    for (const QVariant &depValue : dependencyDownloads) {
                        const QVariantMap dep = depValue.toMap();
                        const QString url = dep.value(QStringLiteral("url")).toString();
                        const QString depFilename = dep.value(QStringLiteral("filename")).toString();
                        const QString dependency = dep.value(QStringLiteral("dependency")).toString();
                        if (url.isEmpty() || depFilename.isEmpty() || dependency.isEmpty()) continue;

                        QVariantMap depMetadata;
                        depMetadata[QStringLiteral("kind")] = QStringLiteral("runtimeDependency");
                        depMetadata[QStringLiteral("id")] = metadata.value(QStringLiteral("id")).toString();
                        depMetadata[QStringLiteral("version")] = metadata.value(QStringLiteral("version")).toString();
                        depMetadata[QStringLiteral("dependency")] = dependency;
                        depMetadata[QStringLiteral("runtimeDir")] = extractDir;
                        depMetadata[QStringLiteral("sha256")] = dep.value(QStringLiteral("sha256")).toString();
                        depMetadata[QStringLiteral("checksum")] = dep.value(QStringLiteral("checksum")).toString();
                        weakThis->m_downloads->enqueueUrl(url, depFilename, extractDir, depMetadata);
                    }
                    if (dependencyDownloads.isEmpty()) {
                        weakThis->m_runtimes->scanRuntimes();
                    }
                } else {
                    QString installedFilename = filename;
                    const QString archiveMember = metadata.value(QStringLiteral("archiveMember")).toString();
                    // Flatten extractDir if it has a single subdirectory
                    QDir extDir(extractDir);
                    QStringList subdirs = extDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    if (subdirs.size() == 1 && extDir.entryList(QDir::Files).isEmpty()) {
                        QString subName = subdirs.first();
                        QDir subDir(extDir.absoluteFilePath(subName));
                        QStringList entries = subDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                        for (const auto &entry : entries) {
                            QFile::rename(subDir.absoluteFilePath(entry), extDir.absoluteFilePath(entry));
                        }
                        extDir.rmdir(subName);
                    }

                    // Move all files from extractDir up to dirPath
                    QStringList entries = extDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const auto &entry : entries) {
                        QFile::rename(extDir.absoluteFilePath(entry), QDir(dirPath).absoluteFilePath(entry));
                    }
                    QDir().rmdir(extractDir);

                    if (!archiveMember.isEmpty()) {
                        QString memberPath;
                        QDirIterator memberIt(dirPath, QDir::Files, QDirIterator::Subdirectories);
                        while (memberIt.hasNext()) {
                            const QString candidate = memberIt.next();
                            if (QFileInfo(candidate).fileName() == archiveMember) {
                                memberPath = candidate;
                                break;
                            }
                        }
                        const QString targetPath = QDir(dirPath).absoluteFilePath(archiveMember);
                        if (!memberPath.isEmpty() && memberPath != targetPath) {
                            QFile::remove(targetPath);
                            if (!QFile::copy(memberPath, targetPath)) {
                                Logger::error(QStringLiteral("DownloadInstallService"),
                                              QStringLiteral("Failed to install archive member %1 from %2")
                                                  .arg(archiveMember, filename));
                                emit weakThis->errorOccurred(QStringLiteral("Failed to install model file: ") + archiveMember);
                                return;
                            }
                        }
                        if (QFileInfo::exists(targetPath)) installedFilename = archiveMember;
                    }

                    // Write .la-info.json to dirPath
                    QDir modelDir(dirPath);
                    QFile infoFile(modelDir.absoluteFilePath(QStringLiteral(".la-info.json")));
                    QJsonObject info;
                    if (infoFile.open(QIODevice::ReadOnly)) {
                        const QJsonDocument existingDoc = QJsonDocument::fromJson(infoFile.readAll());
                        if (existingDoc.isObject()) {
                            info = existingDoc.object();
                        }
                        infoFile.close();
                    }

                    QString resolvedId = metadata.value(QStringLiteral("familyId")).toString();
                    if (resolvedId.isEmpty()) {
                        resolvedId = metadata.value(QStringLiteral("virtualModelId")).toString();
                    }
                    if (resolvedId.isEmpty()) {
                        resolvedId = modelId;
                    }

                    if (infoFile.open(QIODevice::WriteOnly)) {
                        QJsonArray ids = info.value(QStringLiteral("ids")).toArray();
                        const QString existingId = info.value(QStringLiteral("id")).toString();
                        if (!existingId.isEmpty() && !ids.contains(QJsonValue(existingId))) {
                            ids.append(existingId);
                        }
                        if (!resolvedId.isEmpty() && !ids.contains(QJsonValue(resolvedId))) {
                            ids.append(resolvedId);
                        }
                        if (!modelId.isEmpty() && !ids.contains(QJsonValue(modelId))) {
                            ids.append(modelId);
                        }
                        info["id"] = resolvedId;
                        info["ids"] = ids;
                        info["task"] = task;
                        QJsonDocument doc(info);
                        infoFile.write(doc.toJson());
                        infoFile.close();
                    }

                    // Re-scan local models
                    weakThis->m_models->scanLocalModelsAsync();

                    const QString sourceModelId = metadata.value(QStringLiteral("sourceModelId")).toString().isEmpty()
                        ? modelId
                        : metadata.value(QStringLiteral("sourceModelId")).toString();
                    const QString installedPath = QDir(dirPath).absoluteFilePath(installedFilename);
                    const qint64 installedSize = QFileInfo::exists(installedPath) ? QFileInfo(installedPath).size() : fi.size();
                    const QString sizeStr = QString::number(installedSize / (1024.0 * 1024.0), 'f', 1) + " MB";
                    weakThis->m_models->addModel(resolvedId, task, format, dirPath, {installedFilename}, sizeStr);
                    weakThis->scheduleModelFileUpdateCheck(sourceModelId, installedFilename, true);
                }
            } else {
                Logger::error(QStringLiteral("DownloadInstallService"),
                              QStringLiteral("Failed to extract %1 (exit=%2, status=%3)%4")
                                  .arg(filename)
                                  .arg(exitCode)
                                  .arg(status == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crashed"))
                                  .arg(output.isEmpty() ? QString() : QStringLiteral(": %1").arg(output)));
                emit weakThis->errorOccurred(QStringLiteral("Failed to extract: ") + filename);
            }
        });
        
        process->start();
        return;
    }

    if (isRuntime) {
        m_runtimes->scanRuntimes();
    } else {
        QString virtualWriteError;
        if (!writeVirtualModelFilesToDisk(m_models, metadata, &virtualWriteError) &&
            virtualWriteError != QStringLiteral("Virtual model metadata is empty") &&
            virtualWriteError != QStringLiteral("Virtual model id is empty")) {
            emit errorOccurred(virtualWriteError);
        }

        QDir modelDir(dirPath);
        QFile infoFile(modelDir.absoluteFilePath(QStringLiteral(".la-info.json")));
        QJsonObject info;
        if (infoFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument existingDoc = QJsonDocument::fromJson(infoFile.readAll());
            if (existingDoc.isObject()) {
                info = existingDoc.object();
            }
            infoFile.close();
        }

        QString resolvedId = metadata.value(QStringLiteral("familyId")).toString();
        if (resolvedId.isEmpty()) {
            resolvedId = metadata.value(QStringLiteral("virtualModelId")).toString();
        }
        if (resolvedId.isEmpty()) {
            resolvedId = modelId;
        }

        if (infoFile.open(QIODevice::WriteOnly)) {
            QJsonArray ids = info.value(QStringLiteral("ids")).toArray();
            const QString existingId = info.value(QStringLiteral("id")).toString();
            if (!existingId.isEmpty() && !ids.contains(QJsonValue(existingId))) {
                ids.append(existingId);
            }
            if (!resolvedId.isEmpty() && !ids.contains(QJsonValue(resolvedId))) {
                ids.append(resolvedId);
            }
            if (!modelId.isEmpty() && !ids.contains(QJsonValue(modelId))) {
                ids.append(modelId);
            }
            info["id"] = resolvedId;
            info["ids"] = ids;
            info["task"] = task;
            QJsonDocument doc(info);
            infoFile.write(doc.toJson());
            infoFile.close();
        }

        QString sizeStr = QString::number(fi.size() / (1024.0 * 1024.0), 'f', 1) + " MB";
        m_models->addModel(resolvedId, task, format, dirPath, {filename}, sizeStr);
        const QString sourceModelId = metadata.value(QStringLiteral("sourceModelId")).toString().isEmpty()
            ? modelId
            : metadata.value(QStringLiteral("sourceModelId")).toString();
        scheduleModelFileUpdateCheck(sourceModelId, filename, true);
    }
}

void DownloadInstallService::scheduleModelFileUpdateCheck(const QString &modelId, const QString &filename, bool acceptRemoteAsBaseline) const
{
    if (!m_models || modelId.isEmpty() || filename.isEmpty()) {
        return;
    }

    const QString key = modelId + QStringLiteral("::") + filename;
    if (m_activeUpdateChecks.contains(key)) {
        return;
    }
    if (!acceptRemoteAsBaseline) {
        const QDateTime lastChecked = m_lastUpdateChecks.value(key);
        if (lastChecked.isValid() && lastChecked.secsTo(QDateTime::currentDateTimeUtc()) < 6 * 60 * 60) {
            return;
        }
    }

    const QString localPath = m_models->filePath(modelId, filename);
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
        return;
    }

    m_activeUpdateChecks.insert(key);
    QPointer<DownloadInstallService> weakThis(const_cast<DownloadInstallService *>(this));
    QThreadPool::globalInstance()->start([weakThis, modelId, filename, localPath, key, acceptRemoteAsBaseline]() {
        const QVariantMap remote = fetchRemoteFileMetadata(modelId, filename);
        if (!weakThis) return;
        QMetaObject::invokeMethod(weakThis.data(), [weakThis, modelId, filename, localPath, key, remote, acceptRemoteAsBaseline]() {
            if (!weakThis) return;
            auto *self = weakThis.data();
            self->m_activeUpdateChecks.remove(key);

            if (remote.isEmpty() || !self->m_models) {
                return;
            }
            self->m_lastUpdateChecks.insert(key, QDateTime::currentDateTimeUtc());

            if (acceptRemoteAsBaseline) {
                self->m_models->setFileMetadata(modelId, filename, remote);
                const bool previous = self->m_updateAvailable.value(key, false);
                self->m_updateAvailable.insert(key, false);
                if (previous) {
                    emit self->installStatesChanged();
                }
                return;
            }

            const QVariantMap local = self->m_models->fileMetadata(modelId, filename);
            bool updateAvailable = false;
            if (!local.isEmpty()) {
                const QString localFingerprint = fileFingerprint(local);
                const QString remoteFingerprint = fileFingerprint(remote);
                if (!localFingerprint.isEmpty() && !remoteFingerprint.isEmpty()) {
                    updateAvailable = localFingerprint != remoteFingerprint;
                } else {
                    const qint64 expectedSize = remoteSize(remote);
                    updateAvailable = expectedSize > 0 && QFileInfo(localPath).size() != expectedSize;
                }
            } else {
                const qint64 expectedSize = remoteSize(remote);
                updateAvailable = expectedSize > 0 && QFileInfo(localPath).size() != expectedSize;
            }

            if (!updateAvailable) {
                self->m_models->setFileMetadata(modelId, filename, remote);
            }

            const bool previous = self->m_updateAvailable.value(key, false);
            if (previous != updateAvailable) {
                self->m_updateAvailable.insert(key, updateAvailable);
                emit self->installStatesChanged();
            } else {
                self->m_updateAvailable.insert(key, updateAvailable);
            }
        }, Qt::QueuedConnection);
    });
}

int DownloadInstallService::modelFileState(const QString &modelId, const QString &filename) const
{
    if (m_downloads && m_downloads->isDownloading(modelId, filename)) {
        return Downloading;
    }
    if (m_models && m_models->hasFile(modelId, filename)) {
        const QString key = modelId + QStringLiteral("::") + filename;
        if (m_updateAvailable.value(key, false)) {
            return UpdateAvailable;
        }
        scheduleModelFileUpdateCheck(modelId, filename);
        return Installed;
    }
    return NotInstalled;
}

int DownloadInstallService::runtimeState(const QString &runtimeId, const QString &version, const QString &installedPath, const QString &assetName) const
{
    QString key = runtimeId + QStringLiteral("::") + version;
    if (m_activeExtractions.contains(key)) {
        return Installing;
    }

    bool installed = false;
    if (m_runtimes) {
        QVariantList installedVers = m_runtimes->runtimeVersions(runtimeId);
        if (version.isEmpty()) {
            installed = !installedVers.isEmpty();
        } else {
            for (const QVariant &inst : installedVers) {
                if (inst.toMap().value(QStringLiteral("version")).toString() == version) {
                    installed = true;
                    break;
                }
            }
        }
    }

    if (!installed && !installedPath.isEmpty()) {
        installed = QFileInfo::exists(installedPath);
    }

    if (installed) {
        return Installed;
    }

    if (m_downloads && !assetName.isEmpty()) {
        QVariantList active = m_downloads->activeDownloads();
        for (const QVariant &val : active) {
            const QVariantMap activeDownload = val.toMap();
            if (activeDownload.value(QStringLiteral("filename")).toString() != assetName) {
                continue;
            }
            const QVariantMap metadata = activeDownload.value(QStringLiteral("metadata")).toMap();
            const QString activeRuntimeId = metadata.value(QStringLiteral("id")).toString();
            const QString activeVersion = metadata.value(QStringLiteral("version")).toString();
            if ((!activeRuntimeId.isEmpty() || !activeVersion.isEmpty()) &&
                (activeRuntimeId != runtimeId || activeVersion != version)) {
                continue;
            }
            if (activeRuntimeId.isEmpty() || activeRuntimeId == runtimeId) {
                return Downloading;
            }
        }
    }

    return NotInstalled;
}

