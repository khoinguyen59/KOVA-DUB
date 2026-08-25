int CapabilityFamilyModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_items.size();
}

QHash<int, QByteArray> CapabilityFamilyModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FamilyIdRole] = "familyId";
    roles[DisplayNameRole] = "displayName";
    roles[SubtitleRole] = "subtitle";
    roles[DescriptionRole] = "description";
    roles[AccentRole] = "accent";
    roles[SupportedRole] = "supported";
    roles[InstalledRole] = "installed";
    roles[SelectedRole] = "selected";
    roles[ReadyRole] = "ready";
    roles[StatusReasonRole] = "statusReason";
    roles[SelectedRuntimeIdRole] = "selectedRuntimeId";
    roles[SelectedRuntimeVersionRole] = "selectedRuntimeVersion";
    roles[RuntimeOptionsRole] = "runtimeOptions";
    roles[MissingRequirementsRole] = "missingRequirements";
    roles[RequiredFilesRole] = "requiredFiles";
    roles[SelectedFilesRole] = "selectedFiles";
    roles[RawMetadataRole] = "rawMetadata";
    roles[ModelCardUrlRole] = "modelCardUrl";
    roles[ReadmeContentRole] = "readmeContent";
    roles[ThumbnailSourceRole] = "thumbnailSource";
    roles[IconNameRole] = "iconName";
    roles[FamilyCapabilityRole] = "familyCapability";
    roles[StatusKindRole] = "statusKind";
    roles[StatusTitleRole] = "statusTitle";
    roles[InfoBadgesRole] = "infoBadges";
    roles[CapabilityBadgesRole] = "capabilityBadges";
    roles[StatsBadgesRole] = "statsBadges";
    roles[PreferredRuntimeIdRole] = "preferredRuntimeId";
    roles[PreferredRuntimeVersionRole] = "preferredRuntimeVersion";
    roles[IsLastudioPickRole] = "isLastudioPick";
    roles[PickLabelRole] = "pickLabel";
    roles[PickReasonRole] = "pickReason";
    return roles;
}

QVariant CapabilityFamilyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) return {};

    const FamilyItem &item = m_items.at(index.row());
    switch (role) {
    case FamilyIdRole: return item.id;
    case DisplayNameRole: return item.displayName;
    case SubtitleRole: return item.subtitle;
    case DescriptionRole: return item.description;
    case AccentRole: return item.accent;
    case SupportedRole: return item.supported;
    case InstalledRole: return item.installed;
    case SelectedRole: return item.selected;
    case ReadyRole: return item.ready;
    case StatusReasonRole: return item.statusReason;
    case SelectedRuntimeIdRole: return item.selectedRuntimeId;
    case SelectedRuntimeVersionRole: return item.selectedRuntimeVersion;
    case RuntimeOptionsRole: return item.runtimeOptions;
    case MissingRequirementsRole: return item.missingRequirements;
    case RequiredFilesRole: return item.requiredFiles;
    case SelectedFilesRole: return item.selectedFiles;
    case RawMetadataRole: return item.rawMap;
    case ModelCardUrlRole: return item.modelCardUrl;
    case ReadmeContentRole: return item.readmeContent;
    case ThumbnailSourceRole: return item.thumbnailSource;
    case IconNameRole: return item.iconName;
    case FamilyCapabilityRole: return item.familyCapability;
    case StatusKindRole: return item.statusKind;
    case StatusTitleRole: return item.statusTitle;
    case InfoBadgesRole: return item.infoBadges;
    case CapabilityBadgesRole: return item.capabilityBadges;
    case StatsBadgesRole: return item.statsBadges;
    case PreferredRuntimeIdRole: return item.preferredRuntimeId;
    case PreferredRuntimeVersionRole: return item.preferredRuntimeVersion;
    case IsLastudioPickRole: return item.isLastudioPick;
    case PickLabelRole: return item.pickLabel;
    case PickReasonRole: return item.pickReason;
    }
    return {};
}

void CapabilityFamilyModel::setCapability(const QString &capabilityId)
{
    if (m_capabilityId == capabilityId) return;
    m_capabilityId = capabilityId;
    m_languageFilter = QStringLiteral("all");
    emit languageFilterChanged();
    refresh();
}

void CapabilityFamilyModel::setLanguageFilter(const QString &languageCode)
{
    if (m_languageFilter == languageCode) return;
    m_languageFilter = languageCode;
    emit languageFilterChanged();
    refresh();
}

void CapabilityFamilyModel::setSearchText(const QString &searchText)
{
    if (m_searchText == searchText) return;
    m_searchText = searchText;
    refresh();
}

void CapabilityFamilyModel::setStatusFilter(const QString &filterType)
{
    if (m_statusFilter == filterType) return;
    m_statusFilter = filterType;
    refresh();
}

void CapabilityFamilyModel::setSelectedFamilyId(const QString &familyId)
{
    if (m_selectedFamilyId == familyId) return;
    m_selectedFamilyId = familyId;

    // Update selection state in items
    for (int i = 0; i < m_items.size(); ++i) {
        bool selected = (m_items[i].id == m_selectedFamilyId);
        if (m_items[i].selected != selected) {
            m_items[i].selected = selected;
            emit dataChanged(this->index(i), this->index(i), {SelectedRole});
        }
    }
}

void CapabilityFamilyModel::refresh()
{
    beginResetModel();
    updateItems();
    endResetModel();
    ++m_revision;
    emit revisionChanged();
}

QString CapabilityFamilyModel::capabilityForFamily(const QVariantMap &family) const
{
    if (m_capabilityId != QStringLiteral("all")) {
        return m_capabilityId;
    }
    const QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();
    if (capabilities.contains(QStringLiteral("voice-isolation"))) {
        return QStringLiteral("voice-isolation");
    }
    if (capabilities.contains(QStringLiteral("forced-alignment"))) {
        return QStringLiteral("forced-alignment");
    }
    if (capabilities.contains(QStringLiteral("translation"))) {
        return QStringLiteral("translation");
    }
    if (capabilities.contains(QStringLiteral("stt"))) {
        return QStringLiteral("stt");
    }
    if (capabilities.contains(QStringLiteral("voice-cloning"))) {
        return QStringLiteral("voice-cloning");
    }
    return QStringLiteral("tts");
}

QVariantMap CapabilityFamilyModel::storedFilesByRequirement(const QVariantMap &family,
                                                            const QString &capabilityId,
                                                            const QString &familyId) const
{
    QVariantMap out;
    if (!m_selectionRepository) {
        return out;
    }

    const QVariantMap selectedFiles =
        m_selectionRepository->fileSelectionForFamily(capabilityId, familyId);
    if (selectedFiles.isEmpty()) {
        return out;
    }

    const QVariantList reqFiles = family.value(QStringLiteral("requiredFiles")).toList();
    for (const QVariant &reqVal : reqFiles) {
        const QVariantMap req = reqVal.toMap();
        const QString role = req.value(QStringLiteral("role")).toString();
        const QString reqFile = req.value(QStringLiteral("file")).toString();
        if (!role.isEmpty() && !reqFile.isEmpty() && selectedFiles.contains(role)) {
            out.insert(reqFile, selectedFiles.value(role));
        }
    }
    return out;
}

QVariantMap CapabilityFamilyModel::toVariantMap(const FamilyItem &item) const
{
    return {
        {QStringLiteral("familyId"), item.id},
        {QStringLiteral("modelId"), item.rawMap.value(QStringLiteral("modelId"))},
        {QStringLiteral("displayName"), item.displayName},
        {QStringLiteral("subtitle"), item.subtitle},
        {QStringLiteral("description"), item.description},
        {QStringLiteral("accent"), item.accent},
        {QStringLiteral("license"), item.rawMap.value(QStringLiteral("license"))},
        {QStringLiteral("licenseUrl"), item.rawMap.value(QStringLiteral("licenseUrl"))},
        {QStringLiteral("commercialUse"), item.rawMap.value(QStringLiteral("commercialUse"))},
        {QStringLiteral("attributionRequired"), item.rawMap.value(QStringLiteral("attributionRequired"))},
        {QStringLiteral("gated"), item.rawMap.value(QStringLiteral("gated"))},
        {QStringLiteral("supported"), item.supported},
        {QStringLiteral("installed"), item.installed},
        {QStringLiteral("ready"), item.ready},
        {QStringLiteral("statusReason"), item.statusReason},
        {QStringLiteral("runtimeOptions"), item.runtimeOptions},
        {QStringLiteral("missingRequirements"), item.missingRequirements},
        {QStringLiteral("requiredFiles"), item.requiredFiles},
        {QStringLiteral("selectedFiles"), item.selectedFiles},
        {QStringLiteral("rawMetadata"), item.rawMap},
        {QStringLiteral("modelCardUrl"), item.modelCardUrl},
        {QStringLiteral("readmeContent"), item.readmeContent},
        {QStringLiteral("thumbnailSource"), item.thumbnailSource},
        {QStringLiteral("iconName"), item.iconName},
        {QStringLiteral("familyCapability"), item.familyCapability},
        {QStringLiteral("statusKind"), item.statusKind},
        {QStringLiteral("statusTitle"), item.statusTitle},
        {QStringLiteral("infoBadges"), item.infoBadges},
        {QStringLiteral("capabilityBadges"), item.capabilityBadges},
        {QStringLiteral("statsBadges"), item.statsBadges},
        {QStringLiteral("preferredRuntimeId"), item.preferredRuntimeId},
        {QStringLiteral("preferredRuntimeVersion"), item.preferredRuntimeVersion},
        {QStringLiteral("selectedRuntimeId"), item.selectedRuntimeId},
        {QStringLiteral("selectedRuntimeVersion"), item.selectedRuntimeVersion},
        {QStringLiteral("isLastudioPick"), item.isLastudioPick},
        {QStringLiteral("pickLabel"), item.pickLabel},
        {QStringLiteral("pickReason"), item.pickReason}
    };
}

QVariantMap CapabilityFamilyModel::itemForFamily(const QString &familyId) const
{
    for (const FamilyItem &item : m_items) {
        if (item.id == familyId) {
            return toVariantMap(item);
        }
    }
    return {};
}

QString CapabilityFamilyModel::firstFamilyId() const
{
    return m_items.isEmpty() ? QString() : m_items.first().id;
}

bool CapabilityFamilyModel::requirementRequiredForCapability(const QVariantMap &req, const QString &capability) const
{
    if (req.isEmpty()) return false;
    if (req.contains(QStringLiteral("required"))) {
        return req.value(QStringLiteral("required")).toBool();
    }
    QVariantList requiredFor = req.value(QStringLiteral("requiredFor")).toList();
    if (!requiredFor.isEmpty()) {
        for (const QVariant &capVal : requiredFor) {
            if (capVal.toString() == capability) return true;
        }
        return false;
    }
    return true;
}

bool CapabilityFamilyModel::hasFamilyFile(const QVariantMap &family, const QString &modelId, const QString &fileName) const
{
    if (family.isEmpty() || fileName.isEmpty() || !m_models) return false;

    if (!modelId.isEmpty() && m_models->hasFile(modelId, fileName)) return true;

    QString familyModelId = family.value(QStringLiteral("modelId")).toString();
    if (!familyModelId.isEmpty() && modelId != familyModelId && m_models->hasFile(familyModelId, fileName)) return true;

    QString localDir = family.value(QStringLiteral("localDir")).toString();
    if (!localDir.isEmpty() && m_models->hasFile(localDir, fileName)) return true;

    if (!localDir.isEmpty()) {
        QString path = QDir(m_models->concreteModelDir(localDir)).absoluteFilePath(fileName);
        if (QFileInfo::exists(QDir::fromNativeSeparators(path))) return true;
    }
    return false;
}

bool CapabilityFamilyModel::isFileInstalled(const QVariantMap &family, const QString &fileName, const QVariantMap &req) const
{
    if (family.isEmpty() || fileName.isEmpty()) return false;
    QString modelId = req.value(QStringLiteral("modelId")).toString();
    if (modelId.isEmpty()) modelId = family.value(QStringLiteral("modelId")).toString();
    if (!hasFamilyFile(family, modelId, fileName)) return false;

    QString path = m_models ? m_models->filePath(modelId, fileName) : QString();
    if (path.isEmpty()) {
        QString localDir = family.value(QStringLiteral("localDir")).toString();
        if (!localDir.isEmpty() && m_models) {
            path = QDir(m_models->concreteModelDir(localDir)).absoluteFilePath(fileName);
        }
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    Q_UNUSED(req)
    return true;
}

