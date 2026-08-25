QVariantMap DownloadInstallService::latestSupportedRuntime(const QVariantMap &runtimeOption)
{
    const QVariantList versionOptions = runtimeOption.value(QStringLiteral("versionOptions")).toList();
    QVariantMap latestRuntime;
    QString latestVersion;

    for (const QVariant &value : versionOptions) {
        const QVariantMap candidate = value.toMap();
        const QString candidateVersion = candidate.value(QStringLiteral("version")).toString();
        if (latestRuntime.isEmpty() || runtimeVersionGreater(candidateVersion, latestVersion)) {
            latestRuntime = candidate;
            latestVersion = candidateVersion;
        }
    }

    // runtimeOptions are built from catalog entries. Older callers may not provide
    // versionOptions, so retain the option itself as a backwards-compatible fallback.
    if (latestRuntime.isEmpty()) {
        latestRuntime = runtimeOption;
        latestVersion = runtimeOption.value(QStringLiteral("latestVersion")).toString();
        if (!latestVersion.isEmpty())
            latestRuntime.insert(QStringLiteral("version"), latestVersion);
    }
    return latestRuntime;
}

DownloadInstallService::DownloadInstallService(DownloadManager *downloads,
                                               ModelManager *models,
                                               RuntimeManager *runtimes,
                                               Settings *settings,
                                               QObject *parent)
    : QObject(parent)
    , m_downloads(downloads)
    , m_models(models)
    , m_runtimes(runtimes)
    , m_settings(settings)
{
    if (m_downloads) {
        connect(m_downloads, &DownloadManager::finished, this, &DownloadInstallService::onDownloadFinished);
        connect(m_downloads, &DownloadManager::error, this,
                [this](const QString &, const QString &, const QString &message) {
                    emit errorOccurred(message);
                });
    }
}

bool DownloadInstallService::localDownloadsAllowed() const
{
    return !m_settings || !m_settings->remoteFirstMode();
}

bool DownloadInstallService::rejectLocalDownloadInRemoteFirstMode()
{
    if (localDownloadsAllowed()) return false;
    emit errorOccurred(QStringLiteral(
        "Remote-first mode disables local model and runtime downloads. "
        "Configure API Gateway or a direct Colab worker, or explicitly enable Local Dev models in Remote Inference settings."));
    return true;
}

bool DownloadInstallService::isSafeArchiveMemberPath(const QString &memberPath)
{
    QString normalized = memberPath.trimmed();
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalized = QDir::cleanPath(normalized);
    return !normalized.isEmpty() && normalized != QStringLiteral(".") &&
           !QDir::isAbsolutePath(normalized) &&
           !QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(normalized).hasMatch() &&
           normalized != QStringLiteral("..") &&
           !normalized.startsWith(QStringLiteral("../"));
}

bool DownloadInstallService::archiveContainsOnlySafeMembers(const QString &extractor,
                                                             const QString &archivePath,
                                                             qint64 *unpackedBytes,
                                                             QString *errorMessage)
{
    QProcess listing;
    listing.setProgram(extractor);
    listing.setArguments({QStringLiteral("-tf"), archivePath});
    listing.setProcessChannelMode(QProcess::MergedChannels);
    listing.start();
    if (!listing.waitForStarted(10000) || !listing.waitForFinished(30000) ||
        listing.exitStatus() != QProcess::NormalExit || listing.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not inspect archive members with %1: %2")
                .arg(extractor, QString::fromLocal8Bit(listing.readAll()).trimmed());
        }
        return false;
    }

    const QStringList members = QString::fromLocal8Bit(listing.readAll())
                                    .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &member : members) {
        if (!isSafeArchiveMemberPath(member)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Archive member escapes extraction directory: %1")
                    .arg(member.trimmed());
            }
            return false;
        }
    }

    // A safe-looking member name is insufficient when an archive also carries
    // symlinks or hardlinks: a later entry could be written through that link.
    // Runtime/model archives do not need links, so reject them rather than
    // depending on extractor-specific link semantics.
    QProcess verboseListing;
    verboseListing.setProgram(extractor);
    verboseListing.setArguments({QStringLiteral("-tvf"), archivePath});
    verboseListing.setProcessChannelMode(QProcess::MergedChannels);
    verboseListing.start();
    if (!verboseListing.waitForStarted(10000) || !verboseListing.waitForFinished(30000) ||
        verboseListing.exitStatus() != QProcess::NormalExit || verboseListing.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not inspect archive link entries with %1: %2")
                .arg(extractor, QString::fromLocal8Bit(verboseListing.readAll()).trimmed());
        }
        return false;
    }
    const QStringList verboseEntries = QString::fromLocal8Bit(verboseListing.readAll())
                                          .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    qint64 totalUnpackedBytes = 0;
    const QRegularExpression verboseSizePattern(
        QStringLiteral(R"(^\S+\s+\d+\s+\S+\s+\S+\s+(\d+)\s+)")
    );
    for (const QString &entry : verboseEntries) {
        const QString line = entry.trimmed();
        if (line.startsWith(QLatin1Char('l')) || line.startsWith(QLatin1Char('h'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Archive links are not permitted: %1").arg(line);
            }
            return false;
        }
        if (!line.startsWith(QLatin1Char('-')) && !line.startsWith(QLatin1Char('d'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Archive contains an unsupported special entry: %1").arg(line);
            }
            return false;
        }
        if (line.startsWith(QLatin1Char('-'))) {
            const QRegularExpressionMatch sizeMatch = verboseSizePattern.match(line);
            bool sizeOk = false;
            const qint64 memberSize = sizeMatch.hasMatch()
                ? sizeMatch.captured(1).toLongLong(&sizeOk) : -1;
            if (!sizeOk || memberSize < 0 ||
                totalUnpackedBytes > std::numeric_limits<qint64>::max() - memberSize) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Could not safely determine archive member size: %1")
                        .arg(line);
                }
                return false;
            }
            totalUnpackedBytes += memberSize;
        }
    }
    if (unpackedBytes) {
        *unpackedBytes = totalUnpackedBytes;
    }
    return true;
}

bool DownloadInstallService::hasSpaceForExtraction(const QString &extractDir,
                                                    qint64 unpackedBytes,
                                                    QString *errorMessage)
{
    constexpr qint64 kExtractionSafetyMarginBytes = 64LL * 1024 * 1024;
    if (unpackedBytes < 0 ||
        unpackedBytes > std::numeric_limits<qint64>::max() - kExtractionSafetyMarginBytes) {
        if (errorMessage) *errorMessage = QStringLiteral("Archive unpacked size is invalid or too large.");
        return false;
    }

    QString probePath = QDir(extractDir).absolutePath();
    while (!QFileInfo::exists(probePath)) {
        QDir parent(probePath);
        if (!parent.cdUp()) break;
        const QString nextPath = parent.absolutePath();
        if (nextPath == probePath) break;
        probePath = nextPath;
    }
    const QStorageInfo storage(probePath);
    const qint64 requiredBytes = unpackedBytes + kExtractionSafetyMarginBytes;
    if (!storage.isReady() || storage.bytesAvailable() < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not determine free disk space for archive extraction: %1")
                .arg(extractDir);
        }
        return false;
    }
    if (storage.bytesAvailable() < requiredBytes) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Not enough free disk space to extract this archive. Need %1 MiB, but only %2 MiB is available.")
                .arg((requiredBytes + 1024 * 1024 - 1) / (1024 * 1024))
                .arg(storage.bytesAvailable() / (1024 * 1024));
        }
        return false;
    }
    return true;
}

bool DownloadInstallService::extractedTreeIsContained(const QString &extractDir, QString *errorMessage)
{
    const QString canonicalRoot = QFileInfo(extractDir).canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Extraction directory cannot be canonicalized");
        return false;
    }

    const QDir root(canonicalRoot);
    QDirIterator it(canonicalRoot, QDir::AllEntries | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo entry(it.next());
        const QString canonicalEntry = entry.canonicalFilePath();
        const QString relative = canonicalEntry.isEmpty()
            ? QStringLiteral("..") : QDir::cleanPath(root.relativeFilePath(canonicalEntry));
        if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")) ||
            QDir::isAbsolutePath(relative)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Extracted path resolves outside extraction directory: %1")
                    .arg(entry.absoluteFilePath());
            }
            return false;
        }
    }
    return true;
}

bool DownloadInstallService::writeVirtualModelFiles(const QVariantMap &metadata)
{
    QString errorMessage;
    const bool ok = writeVirtualModelFilesToDisk(m_models, metadata, &errorMessage);
    if (!ok && errorMessage != QStringLiteral("Virtual model metadata is empty")) {
        emit errorOccurred(errorMessage);
    }
    return ok;
}

