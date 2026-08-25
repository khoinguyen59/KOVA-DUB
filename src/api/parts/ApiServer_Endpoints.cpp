QJsonObject ApiServerService::buildHealthDocument() const
{
    QJsonObject doc;
    doc.insert(QStringLiteral("status"), m_running ? QStringLiteral("ok") : QStringLiteral("stopped"));
    doc.insert(QStringLiteral("enabled"), m_enabled);
    doc.insert(QStringLiteral("running"), m_running);
    doc.insert(QStringLiteral("allow_lan"), m_allowLan);
    doc.insert(QStringLiteral("bind_address"), bindAddress());
    doc.insert(QStringLiteral("base_url"), baseUrl());
    doc.insert(QStringLiteral("port"), m_port);
    doc.insert(QStringLiteral("api_key_required"), !m_apiKey.isEmpty());
    doc.insert(QStringLiteral("source"), QStringLiteral("/source"));
    if (m_tts) {
        doc.insert(QStringLiteral("tts_loaded"), m_tts->isModelLoaded());
        doc.insert(QStringLiteral("tts_state"), static_cast<int>(m_tts->state()));
    }
    if (m_stt) {
        doc.insert(QStringLiteral("stt_loaded"), m_stt->isModelLoaded());
        doc.insert(QStringLiteral("stt_state"), static_cast<int>(m_stt->state()));
    }
    if (m_settings) {
        doc.insert(QStringLiteral("tts_family"), m_settings->selectedTtsFamily());
        doc.insert(QStringLiteral("stt_family"), m_settings->selectedSttFamily());
    }
    return doc;
}

QJsonObject ApiServerService::buildSourceDocument() const
{
    return QJsonObject{
        {QStringLiteral("license"), QStringLiteral("AGPL-3.0-only")},
        {QStringLiteral("version"), QCoreApplication::applicationVersion()},
        {QStringLiteral("repository"), QStringLiteral("https://github.com/dduongtrandai/LA-Studio")}
    };
}

QJsonObject ApiServerService::buildModelsDocument() const
{
    QJsonArray data;
    if (m_tts) {
        const QString active = m_tts->activeSignature();
        for (const QString &signature : m_tts->loadedSignatures()) {
            QJsonObject tts;
            tts.insert(QStringLiteral("id"), signature);
            tts.insert(QStringLiteral("object"), QStringLiteral("model"));
            tts.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
            tts.insert(QStringLiteral("purpose"), QStringLiteral("tts"));
            tts.insert(QStringLiteral("ready"), true);
            tts.insert(QStringLiteral("active"), signature == active);
            data.append(tts);
        }
    }
    if (m_stt) {
        const QString active = m_stt->activeSignature();
        for (const QString &signature : m_stt->loadedSignatures()) {
            QJsonObject stt;
            stt.insert(QStringLiteral("id"), signature);
            stt.insert(QStringLiteral("object"), QStringLiteral("model"));
            stt.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
            stt.insert(QStringLiteral("purpose"), QStringLiteral("stt"));
            stt.insert(QStringLiteral("ready"), true);
            stt.insert(QStringLiteral("active"), signature == active);
            data.append(stt);
        }
    }

    if (data.isEmpty() && m_settings) {
        QJsonObject tts;
        tts.insert(QStringLiteral("id"), QStringLiteral("tts"));
        tts.insert(QStringLiteral("object"), QStringLiteral("model"));
        tts.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
        tts.insert(QStringLiteral("purpose"), QStringLiteral("tts"));
        tts.insert(QStringLiteral("ready"), false);
        tts.insert(QStringLiteral("active"), false);
        data.append(tts);

        QJsonObject stt;
        stt.insert(QStringLiteral("id"), QStringLiteral("stt"));
        stt.insert(QStringLiteral("object"), QStringLiteral("model"));
        stt.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
        stt.insert(QStringLiteral("purpose"), QStringLiteral("stt"));
        stt.insert(QStringLiteral("ready"), false);
        stt.insert(QStringLiteral("active"), false);
        data.append(stt);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("object"), QStringLiteral("list"));
    doc.insert(QStringLiteral("data"), data);
    return doc;
}

QJsonObject ApiServerService::buildVoicesDocument() const
{
    QJsonArray data;
    for (auto it = m_customVoices.cbegin(); it != m_customVoices.cend(); ++it) {
        const CustomVoice &custom = it.value();
        QJsonObject voice;
        voice.insert(QStringLiteral("id"), custom.id);
        voice.insert(QStringLiteral("object"), QStringLiteral("audio.voice"));
        voice.insert(QStringLiteral("name"), custom.name);
        voice.insert(QStringLiteral("detail"), QStringLiteral("Custom voice from reference audio"));
        voice.insert(QStringLiteral("created_at"), custom.createdAt);
        data.append(voice);
    }

    if (m_tts) {
        const QVariantList schema = m_tts->currentSchema();
        for (const QVariant &var : schema) {
            const QVariantMap item = var.toMap();
            if (item.value(QStringLiteral("id")).toString() != QStringLiteral("voice")) {
                continue;
            }
            const QVariantList choices = item.value(QStringLiteral("choices")).toList();
            for (const QVariant &choiceVar : choices) {
                const QVariantMap choice = choiceVar.toMap();
                QJsonObject voice;
                voice.insert(QStringLiteral("id"), choice.value(QStringLiteral("value")).toString());
                voice.insert(QStringLiteral("name"), choice.value(QStringLiteral("text")).toString());
                voice.insert(QStringLiteral("detail"), choice.value(QStringLiteral("detail")).toString());
                data.append(voice);
            }
            break;
        }
    }

    if (data.isEmpty()) {
        QJsonObject voice;
        voice.insert(QStringLiteral("id"), QStringLiteral("default"));
        voice.insert(QStringLiteral("name"), QStringLiteral("default"));
        voice.insert(QStringLiteral("detail"), QStringLiteral("Current loaded TTS voice"));
        data.append(voice);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("object"), QStringLiteral("list"));
    doc.insert(QStringLiteral("data"), data);
    return doc;
}

bool ApiServerService::checkAuthorization(const HttpRequest &request) const
{
    if (m_apiKey.isEmpty()) {
        return false;
    }

    const QString auth = headerValue(request.headers, QStringLiteral("authorization"));
    return auth.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)
        && constantTimeEquals(auth.mid(7).trimmed(), m_apiKey);
}

bool ApiServerService::isTrustedRequestOrigin(const HttpRequest &request) const
{
    const QString host = hostFromHeader(headerValue(request.headers, QStringLiteral("host")));
    if (host.isEmpty() || !isAllowedApiHost(host, m_allowLan)) return false;

    const QString originValue = headerValue(request.headers, QStringLiteral("origin"));
    if (originValue.isEmpty()) return true;
    const QUrl origin(originValue);
    return origin.isValid() && origin.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        && origin.host().compare(host, Qt::CaseInsensitive) == 0
        && origin.port() == m_port;
}

QVariantMap ApiServerService::extraSettingsFromJson(const QJsonObject &json) const
{
    QVariantMap settings;
    for (auto it = json.begin(); it != json.end(); ++it) {
        const QString key = it.key();
        if (key == QStringLiteral("input") ||
            key == QStringLiteral("response_format") ||
            key == QStringLiteral("speed") ||
            key == QStringLiteral("model") ||
            key == QStringLiteral("voice")) {
            continue;
        }
        settings.insert(key, it.value().toVariant());
    }
    return settings;
}

QVariantMap ApiServerService::extraSettingsFromMultipart(const QHash<QString, QString> &fields) const
{
    QVariantMap settings;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (it.key() == QStringLiteral("file")) {
            continue;
        }
        settings.insert(it.key(), it.value());
    }
    return settings;
}

QVariantMap ApiServerService::currentVoiceSettings() const
{
    QVariantMap out;
    if (!m_tts) {
        return out;
    }
    const QVariantList schema = m_tts->currentSchema();
    for (const QVariant &itemVar : schema) {
        const QVariantMap item = itemVar.toMap();
        if (item.value(QStringLiteral("id")).toString() == QStringLiteral("voice")) {
            out = item;
            break;
        }
    }
    return out;
}

QVariantList ApiServerService::availableVoiceEntries() const
{
    QVariantList entries;
    const QVariantMap voiceSchema = currentVoiceSettings();
    const QVariantList choices = voiceSchema.value(QStringLiteral("choices")).toList();
    for (const QVariant &choiceVar : choices) {
        const QVariantMap choice = choiceVar.toMap();
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), choice.value(QStringLiteral("value")).toString());
        entry.insert(QStringLiteral("name"), choice.value(QStringLiteral("text")).toString());
        entry.insert(QStringLiteral("detail"), choice.value(QStringLiteral("detail")).toString());
        entries.append(entry);
    }
    if (entries.isEmpty()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), QStringLiteral("default"));
        entry.insert(QStringLiteral("name"), QStringLiteral("default"));
        entry.insert(QStringLiteral("detail"), QStringLiteral("Current loaded TTS voice"));
        entries.append(entry);
    }
    return entries;
}

QVariantList ApiServerService::availableModelEntries() const
{
    QVariantList entries;
    if (!m_settings) {
        return entries;
    }
    QVariantMap tts;
    tts.insert(QStringLiteral("id"), m_settings->selectedTtsFamily().isEmpty() ? QStringLiteral("tts") : m_settings->selectedTtsFamily());
    tts.insert(QStringLiteral("kind"), QStringLiteral("tts"));
    tts.insert(QStringLiteral("family"), m_settings->selectedTtsFamily());
    tts.insert(QStringLiteral("runtime"), m_settings->selectedTtsRuntime());
    tts.insert(QStringLiteral("ready"), m_tts && m_tts->isModelLoaded());
    entries.append(tts);

    QVariantMap stt;
    stt.insert(QStringLiteral("id"), m_settings->selectedSttFamily().isEmpty() ? QStringLiteral("stt") : m_settings->selectedSttFamily());
    stt.insert(QStringLiteral("kind"), QStringLiteral("stt"));
    stt.insert(QStringLiteral("family"), m_settings->selectedSttFamily());
    stt.insert(QStringLiteral("runtime"), m_settings->selectedSttRuntime());
    stt.insert(QStringLiteral("ready"), m_stt && m_stt->isModelLoaded());
    entries.append(stt);

    return entries;
}

QJsonObject ApiServerService::parseJsonObject(const QByteArray &body, QString *error) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON body: %1").arg(parseError.errorString());
        }
        return {};
    }
    return doc.object();
}

QString ApiServerService::multipartBoundary(const QString &contentType)
{
    return ::LAStudio::multipartBoundary(contentType);
}

QByteArray ApiServerService::trimMultipartPart(const QByteArray &part)
{
    return trimPart(part);
}

QHash<QString, QString> ApiServerService::parseDispositionParams(const QString &value)
{
    return ::LAStudio::parseContentDisposition(value);
}

QHash<QString, QString> ApiServerService::parseHeaderLines(const QByteArray &headersBlob)
{
    return parseHeaders(headersBlob);
}

bool ApiServerService::parseMultipart(const QByteArray &body,
                                      const QString &contentType,
                                      QHash<QString, QString> *fields,
                                      QByteArray *fileData,
                                      QString *fileName,
                                      QString *mimeType,
                                      QString *error) const
{
    const QString boundaryString = multipartBoundary(contentType);
    if (boundaryString.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Missing multipart boundary.");
        }
        return false;
    }

    const QByteArray boundary = "--" + boundaryString.toUtf8();
    const QByteArray boundaryMarker = QByteArrayLiteral("\r\n") + boundary;

    int partStart = body.indexOf(boundary);
    if (partStart < 0) {
        if (error) {
            *error = QStringLiteral("Multipart boundary not found.");
        }
        return false;
    }
    partStart += boundary.size();

    while (partStart < body.size()) {
        if (body.mid(partStart, 2) == "--") {
            break;
        }
        if (body.mid(partStart, 2) == "\r\n") {
            partStart += 2;
        }

        const int nextBoundary = body.indexOf(boundaryMarker, partStart);
        QByteArray rawPart = nextBoundary >= 0 ? body.mid(partStart, nextBoundary - partStart) : body.mid(partStart);
        QByteArray part = trimPart(rawPart);
        if (part.isEmpty() || part == "--" || part.startsWith("--")) {
            if (nextBoundary < 0) {
                break;
            }
            partStart = nextBoundary + boundaryMarker.size();
            continue;
        }

        const int sep = part.indexOf("\r\n\r\n");
        if (sep < 0) {
            continue;
        }

        const QByteArray headerBlock = part.left(sep);
        QByteArray partBody = part.mid(sep + 4);
        partBody = trimPart(partBody);

        const QHash<QString, QString> headers = parseHeaders("X: dummy\r\n" + headerBlock);
        const QString disposition = headers.value(QStringLiteral("content-disposition"));
        if (disposition.isEmpty()) {
            continue;
        }

        const QHash<QString, QString> params = parseContentDisposition(disposition);
        const QString name = params.value(QStringLiteral("name"));
        const QString filename = params.value(QStringLiteral("filename"));
        if (!filename.isEmpty() || name == QStringLiteral("file")) {
            if (fileData) {
                *fileData = partBody;
            }
            if (fileName) {
                *fileName = filename.isEmpty() ? QStringLiteral("upload.wav") : filename;
            }
            if (mimeType) {
                *mimeType = headers.value(QStringLiteral("content-type"));
            }
        } else if (fields && !name.isEmpty()) {
            (*fields)[name] = QString::fromUtf8(partBody).trimmed();
        }

        if (nextBoundary < 0) {
            break;
        }
        partStart = nextBoundary + boundaryMarker.size();
    }

    return fileData && !fileData->isEmpty();
}

