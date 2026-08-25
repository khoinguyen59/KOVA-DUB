bool ApiServerService::parseHttpRequest(QByteArray &buffer, HttpRequest &request, int &consumed)
{
    consumed = 0;
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return false;
    }

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QHash<QString, QString> headers = parseHeaders(headerBlock);
    const int contentLength = headers.value(QStringLiteral("content-length")).toInt();
    if (buffer.size() < headerEnd + 4 + contentLength) {
        return false;
    }

    const QString requestLine = QString::fromUtf8(headerBlock.split('\n').value(0)).trimmed();
    const QStringList parts = requestLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    request.method = parts.at(0).trimmed().toUpper();
    request.target = parts.at(1).trimmed();
    request.path = pathFromTarget(request.target);
    request.headers = headers;
    request.body = buffer.mid(headerEnd + 4, contentLength);

    if (headerValue(headers, QStringLiteral("content-type")).startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            request.jsonBody = doc.object();
        }
    }

    consumed = headerEnd + 4 + contentLength;
    return true;
}

ApiServerService::HttpResponse ApiServerService::jsonResponse(const QJsonObject &object, int status) const
{
    HttpResponse response;
    response.status = status;
    response.body = toJsonBytes(object);
    response.contentType = QByteArrayLiteral("application/json; charset=utf-8");
    return response;
}

ApiServerService::HttpResponse ApiServerService::binaryResponse(const QByteArray &data, const QByteArray &contentType, int status) const
{
    HttpResponse response;
    response.status = status;
    response.body = data;
    response.contentType = contentType;
    return response;
}

ApiServerService::HttpResponse ApiServerService::errorResponse(int status, const QString &message, const QString &type) const
{
    return jsonResponse(jsonErrorObject(message, type), status);
}

ApiServerService::HttpResponse ApiServerService::unauthorizedResponse() const
{
    return errorResponse(401, QStringLiteral("API key required."), QStringLiteral("authentication_error"));
}

QJsonObject ApiServerService::jsonErrorObject(const QString &message, const QString &type)
{
    QJsonObject error;
    error.insert(QStringLiteral("message"), message);
    error.insert(QStringLiteral("type"), type);
    QJsonObject root;
    root.insert(QStringLiteral("error"), error);
    return root;
}

QString ApiServerService::guessContentType(const QString &format)
{
    const QString lowered = format.toLower();
    if (lowered == QStringLiteral("wav")) {
        return QStringLiteral("audio/wav");
    }
    if (lowered == QStringLiteral("pcm")) {
        return QStringLiteral("audio/pcm");
    }
    return QStringLiteral("application/octet-stream");
}

QString ApiServerService::randomApiKey()
{
    QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    key.remove(QLatin1Char('-'));
    return key;
}

QString ApiServerService::randomObjectId(const QString &prefix)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    id.remove(QLatin1Char('-'));
    return prefix + QStringLiteral("_") + id.left(24);
}

void ApiServerService::writeResponse(QTcpSocket *socket, const HttpResponse &response)
{
    if (!socket) {
        return;
    }

    QByteArray out;
    out += "HTTP/1.1 ";
    out += QByteArray::number(response.status);
    out += ' ';
    out += statusText(response.status).toUtf8();
    out += "\r\nConnection: close\r\n";
    out += "Content-Type: ";
    out += response.contentType;
    out += "\r\nContent-Length: ";
    out += QByteArray::number(response.body.size());
    out += "\r\n";
    for (const auto &header : response.headers) {
        out += header.first;
        out += ": ";
        out += header.second;
        out += "\r\n";
    }
    out += "\r\n";
    out += response.body;
    socket->write(out);
    socket->disconnectFromHost();
}

