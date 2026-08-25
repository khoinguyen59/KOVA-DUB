void ApiServerService::processRequestAsync(const QPointer<QTcpSocket> &socket, const HttpRequest &request)
{
    QtConcurrent::run([this, socket, request]() {
        HttpResponse response = handleRequest(request);
        QMetaObject::invokeMethod(this, [this, socket, response]() {
            if (socket) {
                writeResponse(socket, response);
            }
        }, Qt::QueuedConnection);
    });
}

ApiServerService::HttpResponse ApiServerService::handleRequest(const HttpRequest &request)
{
    if (!isTrustedRequestOrigin(request)) {
        return errorResponse(403, QStringLiteral("Request Host or Origin is not permitted."),
                             QStringLiteral("forbidden"));
    }

    if (!checkAuthorization(request)) {
        return unauthorizedResponse();
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/health")) {
        return jsonResponse(buildHealthDocument());
    }
    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/source")) {
        return jsonResponse(buildSourceDocument());
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/v1/models")) {
        return jsonResponse(buildModelsDocument());
    }
    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/v1/audio/voices")) {
        return jsonResponse(buildVoicesDocument());
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voice_consents")) {
        return handleCreateVoiceConsentRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voices")) {
        return handleCreateVoiceRequest(request);
    }
    if (request.method == QStringLiteral("POST") &&
        (request.path == QStringLiteral("/v1/audio/speech") ||
         request.path == QStringLiteral("/v1/tts/speech")))
    {
        return handleSpeechRequest(request);
    }
    if (request.method == QStringLiteral("POST") &&
        (request.path == QStringLiteral("/v1/audio/transcriptions") ||
         request.path == QStringLiteral("/v1/stt/transcriptions")))
    {
        return handleTranscriptionRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voice_designs")) {
        return handleVoiceDesignRequest(request);
    }

    return errorResponse(404, QStringLiteral("Route not found: %1").arg(request.path), QStringLiteral("not_found"));
}

ApiServerService::HttpResponse ApiServerService::runTtsGeneration(const QString &input,
                                                                  const QString &format,
                                                                  const QVariantMap &settings,
                                                                  float speed,
                                                                  const QString &mode,
                                                                  const QString &referencePath,
                                                                  const QString &modelId)
{
    if (!m_tts) {
        return errorResponse(503, QStringLiteral("TTS service is not available."));
    }
    if (!m_tts->isModelLoaded()) {
        return errorResponse(409, QStringLiteral("Load a TTS model in the app before calling /v1/audio/speech."));
    }

    const QString targetModelId = modelId.isEmpty() ? m_tts->activeSignature() : modelId;
    TtsEngineInstance *target = m_tts->instance(targetModelId);
    if (!target) {
        return errorResponse(404, QStringLiteral("Requested TTS model is not loaded: %1").arg(targetModelId), QStringLiteral("not_found"));
    }
    if (target->isProcessing()) {
        return errorResponse(409, QStringLiteral("Requested TTS model is busy: %1").arg(targetModelId));
    }

    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }
    if (format != QStringLiteral("wav") && format != QStringLiteral("pcm")) {
        return errorResponse(415, QStringLiteral("This build currently supports response_format=wav or pcm."));
    }

    const QString generationMode = mode.isEmpty() ? QStringLiteral("speech") : mode;
    if (generationMode == QStringLiteral("voice-cloning") && referencePath.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing reference audio for voice cloning."));
    }

    QEventLoop loop;
    struct Result {
        bool ok = false;
        QByteArray pcm16;
        int sampleRate = 24000;
        QString error;
    } result;

    QMetaObject::Connection okConn = connect(target, &TtsEngineInstance::synthesisFinished, &loop,
                                             [&](const QByteArray &pcm16, int sampleRate) {
        result.ok = true;
        result.pcm16 = pcm16;
        result.sampleRate = sampleRate;
        loop.quit();
    });
    QMetaObject::Connection errConn = connect(target, &TtsEngineInstance::errorOccurred, &loop,
                                              [&](const QString &msg) {
        result.ok = false;
        result.error = msg;
        loop.quit();
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, [&]() {
        result.ok = false;
        result.error = QStringLiteral("TTS request timed out.");
        loop.quit();
    });

    if (generationMode == QStringLiteral("voice-cloning")) {
        QMetaObject::invokeMethod(target, [target, input, referencePath, settings]() {
            target->cloneVoice(input, referencePath, settings);
        }, Qt::QueuedConnection);
    } else if (generationMode == QStringLiteral("voice-design")) {
        QMetaObject::invokeMethod(target, [target, input, settings]() {
            target->designVoice(input, settings);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(target, [target, input, speed, settings]() {
            target->synthesize(input, 0, speed, settings);
        }, Qt::QueuedConnection);
    }

    timeout.start(kTtsTimeoutMs);
    loop.exec();

    disconnect(okConn);
    disconnect(errConn);

    if (!result.ok) {
        return errorResponse(500, result.error.isEmpty() ? QStringLiteral("TTS synthesis failed.") : result.error);
    }

    const QByteArray body = format == QStringLiteral("pcm")
        ? result.pcm16
        : buildWavBytes(result.pcm16, result.sampleRate, 1);
    return binaryResponse(body, guessContentType(format).toUtf8());
}

ApiServerService::HttpResponse ApiServerService::handleSpeechRequest(const HttpRequest &request)
{
    QJsonObject json = request.jsonBody;
    if (json.isEmpty()) {
        QString error;
        json = parseJsonObject(request.body, &error);
        if (!error.isEmpty()) {
            return errorResponse(400, error);
        }
    }

    const QString input = jsonString(json, QStringLiteral("input")).trimmed();
    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }

    const QString format = jsonString(json, QStringLiteral("response_format")).isEmpty()
        ? QStringLiteral("wav")
        : jsonString(json, QStringLiteral("response_format")).toLower();
    if (format != QStringLiteral("wav") && format != QStringLiteral("pcm")) {
        return errorResponse(415, QStringLiteral("This build currently supports response_format=wav or pcm."));
    }

    QVariantMap settings = extraSettingsFromJson(json);
    const QString voice = jsonString(json, QStringLiteral("voice"));
    QString mode = QStringLiteral("speech");
    QString referencePath;
    if (!voice.isEmpty() && m_customVoices.contains(voice)) {
        const CustomVoice custom = m_customVoices.value(voice);
        settings.insert(QStringLiteral("voice"), custom.name);
        mode = QStringLiteral("voice-cloning");
        referencePath = custom.samplePath;
    } else if (!voice.isEmpty()) {
        settings.insert(QStringLiteral("voice"), voice);
    }
    const QString language = jsonString(json, QStringLiteral("language"));
    if (!language.isEmpty()) {
        settings.insert(QStringLiteral("lang"), language);
    }
    const QString instruct = jsonString(json, QStringLiteral("instruct"));
    if (!instruct.isEmpty()) {
        settings.insert(QStringLiteral("instruct"), instruct);
    }
    if (json.contains(QStringLiteral("seed"))) {
        settings.insert(QStringLiteral("seed"), json.value(QStringLiteral("seed")).toVariant());
    }
    const float speed = static_cast<float>(json.value(QStringLiteral("speed")).toDouble(1.0));
    const QString modelId = jsonString(json, QStringLiteral("model"));

    return runTtsGeneration(input, format, settings, speed, mode, referencePath, modelId);
}

ApiServerService::HttpResponse ApiServerService::handleCreateVoiceConsentRequest(const HttpRequest &request)
{
    QHash<QString, QString> fields;
    QByteArray recordingBytes;
    QString fileName;
    QString mimeType;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &recordingBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        fields.insert(QStringLiteral("name"), jsonString(json, QStringLiteral("name")));
        fields.insert(QStringLiteral("language"), jsonString(json, QStringLiteral("language")));
        const QString recordingBase64 = jsonString(json, QStringLiteral("recording_base64"));
        if (!recordingBase64.isEmpty()) {
            recordingBytes = QByteArray::fromBase64(recordingBase64.toUtf8());
            fileName = QStringLiteral("consent.wav");
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (recordingBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: recording."));
    }

    const QString id = randomObjectId(QStringLiteral("cons"));
    QJsonObject doc;
    doc.insert(QStringLiteral("id"), id);
    doc.insert(QStringLiteral("object"), QStringLiteral("audio.voice_consent"));
    doc.insert(QStringLiteral("created_at"), static_cast<qint64>(std::time(nullptr)));
    doc.insert(QStringLiteral("language"), fields.value(QStringLiteral("language"), QStringLiteral("und")));
    doc.insert(QStringLiteral("name"), fields.value(QStringLiteral("name"), fileName.isEmpty() ? QStringLiteral("Voice consent") : fileName));
    m_voiceConsents.insert(id, doc);
    return jsonResponse(doc, 201);
}

ApiServerService::HttpResponse ApiServerService::handleCreateVoiceRequest(const HttpRequest &request)
{
    QHash<QString, QString> fields;
    QByteArray sampleBytes;
    QString fileName;
    QString mimeType;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &sampleBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        fields.insert(QStringLiteral("name"), jsonString(json, QStringLiteral("name")));
        fields.insert(QStringLiteral("consent"), jsonString(json, QStringLiteral("consent")));
        const QString audioBase64 = jsonString(json, QStringLiteral("audio_sample_base64"));
        if (!audioBase64.isEmpty()) {
            sampleBytes = QByteArray::fromBase64(audioBase64.toUtf8());
            fileName = QStringLiteral("voice.wav");
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (sampleBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: audio_sample."));
    }

    const QString suffix = QFileInfo(fileName).suffix().isEmpty()
        ? QStringLiteral("wav")
        : QFileInfo(fileName).suffix();
    QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/lastudio-api-voice-XXXXXX.") + suffix);
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return errorResponse(500, QStringLiteral("Failed to store custom voice audio."));
    }
    temp.write(sampleBytes);
    temp.flush();
    const QString samplePath = temp.fileName();
    temp.close();

    CustomVoice voice;
    voice.id = randomObjectId(QStringLiteral("voice"));
    voice.name = fields.value(QStringLiteral("name"), fileName.isEmpty() ? voice.id : QFileInfo(fileName).completeBaseName());
    voice.consentId = fields.value(QStringLiteral("consent"));
    voice.samplePath = samplePath;
    voice.fileName = fileName;
    voice.mimeType = mimeType;
    voice.createdAt = static_cast<qint64>(std::time(nullptr));
    m_customVoices.insert(voice.id, voice);

    QJsonObject doc;
    doc.insert(QStringLiteral("id"), voice.id);
    doc.insert(QStringLiteral("object"), QStringLiteral("audio.voice"));
    doc.insert(QStringLiteral("created_at"), voice.createdAt);
    doc.insert(QStringLiteral("name"), voice.name);
    return jsonResponse(doc, 201);
}

ApiServerService::HttpResponse ApiServerService::handleVoiceDesignRequest(const HttpRequest &request)
{
    QJsonObject json = request.jsonBody;
    if (json.isEmpty()) {
        QString error;
        json = parseJsonObject(request.body, &error);
        if (!error.isEmpty()) {
            return errorResponse(400, error);
        }
    }

    const QString input = jsonString(json, QStringLiteral("input")).trimmed();
    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }

    const QString format = jsonString(json, QStringLiteral("response_format")).isEmpty()
        ? QStringLiteral("wav")
        : jsonString(json, QStringLiteral("response_format")).toLower();
    QVariantMap settings = extraSettingsFromJson(json);
    const QString voiceDescription = jsonString(json, QStringLiteral("voice_description"));
    if (!voiceDescription.isEmpty()) {
        settings.insert(QStringLiteral("voice_description"), voiceDescription);
        settings.insert(QStringLiteral("description"), voiceDescription);
    }
    const QString language = jsonString(json, QStringLiteral("language"));
    if (!language.isEmpty()) {
        settings.insert(QStringLiteral("lang"), language);
    }

    const QString modelId = jsonString(json, QStringLiteral("model"));
    return runTtsGeneration(input, format, settings, 1.0f, QStringLiteral("voice-design"), QString(), modelId);
}

ApiServerService::HttpResponse ApiServerService::handleTranscriptionRequest(const HttpRequest &request)
{
    if (!m_stt) {
        return errorResponse(503, QStringLiteral("STT service is not available."));
    }
    if (!m_stt->isModelLoaded()) {
        return errorResponse(409, QStringLiteral("Load an STT model in the app before calling /v1/audio/transcriptions."));
    }

    QByteArray fileBytes;
    QString fileName;
    QString mimeType;
    QHash<QString, QString> fields;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &fileBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        const QString audioBase64 = jsonString(json, QStringLiteral("audio_base64"));
        if (audioBase64.isEmpty()) {
            return errorResponse(422, QStringLiteral("Expected multipart/form-data or JSON with audio_base64."));
        }
        fileBytes = QByteArray::fromBase64(audioBase64.toUtf8());
        fileName = QStringLiteral("upload.wav");
        fields.insert(QStringLiteral("language"), jsonString(json, QStringLiteral("language")));
        fields.insert(QStringLiteral("model"), jsonString(json, QStringLiteral("model")));
        fields.insert(QStringLiteral("translate"), json.value(QStringLiteral("translate")).toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        if (json.contains(QStringLiteral("threads"))) {
            fields.insert(QStringLiteral("threads"), QString::number(json.value(QStringLiteral("threads")).toInt(0)));
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (fileBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing uploaded audio file."));
    }

    const QString language = fields.value(QStringLiteral("language"), QStringLiteral("en"));
    const QString modelId = fields.value(QStringLiteral("model")).trimmed();
    const bool translate = fields.value(QStringLiteral("translate")).toLower() == QStringLiteral("true");
    const int threads = qBound(0, fields.value(QStringLiteral("threads"), QStringLiteral("0")).toInt(), 64);

    const QString targetModelId = modelId.isEmpty() ? m_stt->activeSignature() : modelId;
    SttEngineInstance *target = m_stt->instance(targetModelId);
    if (!target) {
        return errorResponse(404, QStringLiteral("Requested STT model is not loaded: %1").arg(targetModelId), QStringLiteral("not_found"));
    }
    if (target->isProcessing()) {
        return errorResponse(409, QStringLiteral("Requested STT model is busy: %1").arg(targetModelId));
    }

    QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/lastudio-api-XXXXXX") +
                        (fileName.isEmpty() ? QStringLiteral(".wav") : QStringLiteral(".") + QFileInfo(fileName).suffix()));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return errorResponse(500, QStringLiteral("Failed to create a temporary upload file."));
    }
    temp.write(fileBytes);
    temp.flush();
    const QString tempPath = temp.fileName();
    temp.close();

    QEventLoop loop;
    struct DecodeResult {
        bool ok = false;
        QVector<float> samples;
        QString error;
    } decodeResult;

    SttAudioDecoder decoder;
    QMetaObject::Connection decodeConn = connect(&decoder, &SttAudioDecoder::finished, &loop,
                                                 [&](const QVector<float> &samples) {
        decodeResult.ok = true;
        decodeResult.samples = samples;
        loop.quit();
    });
    QMetaObject::Connection decodeErrConn = connect(&decoder, &SttAudioDecoder::errorOccurred, &loop,
                                                     [&](const QString &msg) {
        decodeResult.ok = false;
        decodeResult.error = msg;
        loop.quit();
    });
    QTimer decodeTimeout;
    decodeTimeout.setSingleShot(true);
    connect(&decodeTimeout, &QTimer::timeout, &loop, [&]() {
        decodeResult.ok = false;
        decodeResult.error = QStringLiteral("Audio decode timed out.");
        loop.quit();
    });

    decoder.startDecode(tempPath);
    decodeTimeout.start(kSttTimeoutMs);
    loop.exec();

    disconnect(decodeConn);
    disconnect(decodeErrConn);

    if (!decodeResult.ok) {
        QFile::remove(tempPath);
        return errorResponse(500, decodeResult.error.isEmpty() ? QStringLiteral("Audio decode failed.") : decodeResult.error);
    }

    QEventLoop transcribeLoop;
    struct TranscribeResult {
        bool ok = false;
        QString text;
        QString error;
    } transcribeResult;

    QMetaObject::Connection transcribeConn = connect(target, &SttEngineInstance::transcriptionFinished, &transcribeLoop,
                                                      [&](const QString &text, const QVariantList &) {
        transcribeResult.ok = true;
        transcribeResult.text = text;
        transcribeLoop.quit();
    });
    QMetaObject::Connection transcribeErrConn = connect(target, &SttEngineInstance::errorOccurred, &transcribeLoop,
                                                        [&](const QString &msg) {
        transcribeResult.ok = false;
        transcribeResult.error = msg;
        transcribeLoop.quit();
    });
    QTimer transcribeTimeout;
    transcribeTimeout.setSingleShot(true);
    connect(&transcribeTimeout, &QTimer::timeout, &transcribeLoop, [&]() {
        transcribeResult.ok = false;
        transcribeResult.error = QStringLiteral("Transcription request timed out.");
        transcribeLoop.quit();
    });

    QMetaObject::invokeMethod(target, [target, decodeResult, language, threads, translate]() {
        target->transcribeSamples(decodeResult.samples, language, threads, translate, QVariantMap());
    }, Qt::QueuedConnection);
    transcribeTimeout.start(kSttTimeoutMs);
    transcribeLoop.exec();

    disconnect(transcribeConn);
    disconnect(transcribeErrConn);
    QFile::remove(tempPath);

    if (!transcribeResult.ok) {
        return errorResponse(500, transcribeResult.error.isEmpty() ? QStringLiteral("Transcription failed.") : transcribeResult.error);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("text"), transcribeResult.text);
    return jsonResponse(doc);
}

