#include "ApiServerService.h"

#include "core/utils/Logger.h"
#include "core/storage/Settings.h"
#include "controllers/stt/SttAudioDecoder.h"
#include "stt/engine/SttEngine.h"
#include "stt/engine/SttEngineInstance.h"
#include "tts/engine/TtsEngine.h"
#include "tts/engine/TtsEngineInstance.h"

#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
#include <algorithm>
#include <cstring>
#include <ctime>

namespace LAStudio {

namespace {

constexpr int kMaxBodyBytes = 50 * 1024 * 1024;
constexpr int kTtsTimeoutMs = 120000;
constexpr int kSttTimeoutMs = 120000;

QString statusText(int status)
{
    switch (status) {
    case 200: return QStringLiteral("OK");
    case 201: return QStringLiteral("Created");
    case 400: return QStringLiteral("Bad Request");
    case 401: return QStringLiteral("Unauthorized");
    case 403: return QStringLiteral("Forbidden");
    case 404: return QStringLiteral("Not Found");
    case 409: return QStringLiteral("Conflict");
    case 415: return QStringLiteral("Unsupported Media Type");
    case 422: return QStringLiteral("Unprocessable Entity");
    case 429: return QStringLiteral("Too Many Requests");
    case 500: return QStringLiteral("Internal Server Error");
    case 503: return QStringLiteral("Service Unavailable");
    default: return QStringLiteral("OK");
    }
}

QByteArray toJsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString jsonString(const QJsonObject &object, const QString &key)
{
    const auto value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

QHash<QString, QString> parseHeaders(const QByteArray &headerBlock)
{
    QHash<QString, QString> headers;
    const QList<QByteArray> lines = headerBlock.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = QString::fromUtf8(line.left(colon)).trimmed().toLower();
        const QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        headers.insert(key, value);
    }
    return headers;
}

QString headerValue(const QHash<QString, QString> &headers, const QString &name)
{
    return headers.value(name.trimmed().toLower());
}

QString pathFromTarget(const QString &target)
{
    const QUrl url(QStringLiteral("http://localhost") + target);
    return url.path().isEmpty() ? QStringLiteral("/") : url.path();
}

QString hostFromHeader(const QString &value)
{
    const QUrl url(QStringLiteral("http://") + value.trimmed());
    return url.isValid() ? url.host().toLower() : QString();
}

bool isAllowedApiHost(const QString &host, bool allowLan)
{
    if (host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1")) {
        return true;
    }
    if (!allowLan) return false;

    QHostAddress address;
    if (!address.setAddress(host)) return false;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
            if (entry.ip() == address) return true;
        }
    }
    return false;
}

bool constantTimeEquals(const QString &left, const QString &right)
{
    const QByteArray leftBytes = left.toUtf8();
    const QByteArray rightBytes = right.toUtf8();
    const qsizetype largest = std::max(leftBytes.size(), rightBytes.size());
    quint64 difference = static_cast<quint64>(leftBytes.size())
                       ^ static_cast<quint64>(rightBytes.size());
    for (qsizetype i = 0; i < largest; ++i) {
        const unsigned char leftByte = i < leftBytes.size()
            ? static_cast<unsigned char>(leftBytes.at(i)) : 0;
        const unsigned char rightByte = i < rightBytes.size()
            ? static_cast<unsigned char>(rightBytes.at(i)) : 0;
        difference |= static_cast<quint64>(leftByte ^ rightByte);
    }
    return difference == 0;
}

QString multipartBoundary(const QString &contentType)
{
    const int idx = contentType.toLower().indexOf(QStringLiteral("boundary="));
    if (idx < 0) {
        return {};
    }
    QString boundary = contentType.mid(idx + 9).trimmed();
    if (boundary.startsWith(QLatin1Char('"')) && boundary.endsWith(QLatin1Char('"')) && boundary.size() >= 2) {
        boundary = boundary.mid(1, boundary.size() - 2);
    }
    return boundary;
}

QHash<QString, QString> parseContentDisposition(const QString &value)
{
    QHash<QString, QString> out;
    const QStringList parts = value.split(QLatin1Char(';'));
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        const int eq = trimmed.indexOf(QLatin1Char('='));
        if (eq < 0) {
            continue;
        }
        QString key = trimmed.left(eq).trimmed().toLower();
        QString val = trimmed.mid(eq + 1).trimmed();
        if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
            val = val.mid(1, val.size() - 2);
        }
        out.insert(key, val);
    }
    return out;
}

QByteArray trimPart(const QByteArray &part)
{
    QByteArray out = part;
    while (out.startsWith("\r\n")) {
        out.remove(0, 2);
    }
    while (out.endsWith("\r\n")) {
        out.chop(2);
    }
    return out;
}

QByteArray buildWavBytes(const QByteArray &pcm16, int sampleRate, int channels)
{
    struct RiffHeader {
        char riff[4];
        quint32 chunkSize;
        char wave[4];
    };
    struct FmtChunk {
        char fmt[4];
        quint32 subchunkSize;
        quint16 audioFormat;
        quint16 numChannels;
        quint32 sampleRate;
        quint32 byteRate;
        quint16 blockAlign;
        quint16 bitsPerSample;
    };
    struct DataChunkHeader {
        char data[4];
        quint32 dataSize;
    };

    const quint32 dataSize = static_cast<quint32>(pcm16.size());

    RiffHeader riff;
    memcpy(riff.riff, "RIFF", 4);
    riff.chunkSize = 36 + dataSize;
    memcpy(riff.wave, "WAVE", 4);

    FmtChunk fmt;
    memcpy(fmt.fmt, "fmt ", 4);
    fmt.subchunkSize = 16;
    fmt.audioFormat = 1;
    fmt.numChannels = static_cast<quint16>(channels);
    fmt.sampleRate = static_cast<quint32>(sampleRate);
    fmt.bitsPerSample = 16;
    fmt.blockAlign = static_cast<quint16>(channels * 2);
    fmt.byteRate = fmt.sampleRate * fmt.blockAlign;

    DataChunkHeader data;
    memcpy(data.data, "data", 4);
    data.dataSize = dataSize;

    QByteArray out;
    out.reserve(sizeof(riff) + sizeof(fmt) + sizeof(data) + pcm16.size());
    out.append(reinterpret_cast<const char *>(&riff), sizeof(riff));
    out.append(reinterpret_cast<const char *>(&fmt), sizeof(fmt));
    out.append(reinterpret_cast<const char *>(&data), sizeof(data));
    out.append(pcm16);
    return out;
}

} // namespace

ApiServerService::ApiServerService(Settings *settings,
                                   TtsEngine *tts,
                                   SttEngine *stt,
                                   QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_tts(tts)
    , m_stt(stt)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ApiServerService::onNewConnection);

    if (m_settings) {
        connect(m_settings, &Settings::apiServerEnabledChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerAllowLanChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerPortChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerApiKeyChanged, this, &ApiServerService::syncFromSettings);
    }

    syncFromSettings();
}

ApiServerService::~ApiServerService()
{
    stopServer();
}

bool ApiServerService::enabled() const
{
    return m_enabled;
}

void ApiServerService::setEnabled(bool value)
{
    if (m_enabled == value) {
        return;
    }
    m_enabled = value;
    emit enabledChanged();
    if (m_settings && m_settings->apiServerEnabled() != value) {
        m_settings->setApiServerEnabled(value);
    } else {
        applySettingsState();
    }
}

bool ApiServerService::running() const
{
    return m_running;
}

bool ApiServerService::allowLan() const
{
    return m_allowLan;
}

void ApiServerService::setAllowLan(bool value)
{
    if (m_allowLan == value) {
        return;
    }
    m_allowLan = value;
    emit allowLanChanged();
    if (m_settings && m_settings->apiServerAllowLan() != value) {
        m_settings->setApiServerAllowLan(value);
    } else {
        applySettingsState();
    }
}

int ApiServerService::port() const
{
    return m_port;
}

void ApiServerService::setPort(int value)
{
    value = qBound(1, value, 65535);
    if (m_port == value) {
        return;
    }
    m_port = value;
    emit portChanged();
    if (m_settings && m_settings->apiServerPort() != value) {
        m_settings->setApiServerPort(value);
    } else {
        applySettingsState();
    }
}

QString ApiServerService::apiKey() const
{
    return m_apiKey;
}

void ApiServerService::setApiKey(const QString &value)
{
    const QString normalized = value.trimmed();
    if (m_apiKey == normalized) {
        return;
    }
    m_apiKey = normalized;
    emit apiKeyChanged();
    if (m_settings && m_settings->apiServerApiKey() != normalized) {
        m_settings->setApiServerApiKey(normalized);
    }
}

QString ApiServerService::bindAddress() const
{
    if (!m_running) {
        return QString();
    }
    return m_allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1");
}

QString ApiServerService::baseUrl() const
{
    if (!m_running) {
        return QString();
    }
    return QStringLiteral("http://%1:%2").arg(bindAddress()).arg(m_port);
}

QString ApiServerService::lastError() const
{
    return m_lastError;
}

void ApiServerService::syncFromSettings()
{
    if (!m_settings) {
        return;
    }

    const bool nextEnabled = m_settings->apiServerEnabled();
    const bool nextAllowLan = m_settings->apiServerAllowLan();
    const int nextPort = qBound(1, m_settings->apiServerPort(), 65535);
    QString nextKey = m_settings->apiServerApiKey().trimmed();
    if (nextEnabled && nextKey.isEmpty()) {
        nextKey = randomApiKey();
        m_settings->setApiServerApiKey(nextKey);
    }

    const bool changed = m_enabled != nextEnabled || m_allowLan != nextAllowLan || m_port != nextPort || m_apiKey != nextKey;
    m_enabled = nextEnabled;
    m_allowLan = nextAllowLan;
    m_port = nextPort;
    m_apiKey = nextKey;
    if (changed) {
        emit enabledChanged();
        emit allowLanChanged();
        emit portChanged();
        emit apiKeyChanged();
    }
    applySettingsState();
}

void ApiServerService::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &ApiServerService::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ApiServerService::onSocketDisconnected);
    }
}

void ApiServerService::onSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QByteArray &buffer = m_buffers[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > kMaxBodyBytes) {
        buffer.clear();
        HttpResponse response;
        response.status = 400;
        response.body = toJsonBytes(jsonErrorObject(QStringLiteral("Request body too large.")));
        writeResponse(socket, response);
        return;
    }

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QHash<QString, QString> headers = parseHeaders(headerBlock);
    qint64 contentLength = 0;
    const QString contentLengthHeader = headers.value(QStringLiteral("content-length"));
    if (!contentLengthHeader.isEmpty()) {
        bool ok = false;
        contentLength = contentLengthHeader.toLongLong(&ok);
        if (!ok || contentLength < 0 || contentLength > kMaxBodyBytes) {
            HttpResponse response;
            response.status = 400;
            response.body = toJsonBytes(jsonErrorObject(QStringLiteral("Invalid Content-Length.")));
            writeResponse(socket, response);
            return;
        }
    }
    if (buffer.size() < headerEnd + 4 + contentLength) {
        return;
    }

    HttpRequest request;
    const QString requestLine = QString::fromUtf8(headerBlock.split('\n').value(0)).trimmed();
    const QStringList parts = requestLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        HttpResponse response;
        response.status = 400;
        response.body = toJsonBytes(jsonErrorObject(QStringLiteral("Malformed request line.")));
        writeResponse(socket, response);
        return;
    }

    request.method = parts.at(0).trimmed().toUpper();
    request.target = parts.at(1).trimmed();
    request.path = pathFromTarget(request.target);
    request.peerAddress = socket->peerAddress();
    request.headers = headers;
    request.body = buffer.mid(headerEnd + 4, static_cast<qsizetype>(contentLength));

    if (headerValue(headers, QStringLiteral("content-type")).startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            request.jsonBody = doc.object();
        }
    }

    buffer.remove(0, headerEnd + 4 + contentLength);
    processRequestAsync(QPointer<QTcpSocket>(socket), request);
}

void ApiServerService::onSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    m_buffers.remove(socket);
    socket->deleteLater();
}

void ApiServerService::applySettingsState()
{
    if (!m_enabled) {
        stopServer();
        return;
    }
    if (m_running) {
        restartServer();
    } else {
        startServer();
    }
}

bool ApiServerService::startServer()
{
    if (m_running) {
        return true;
    }

    clearLastError();
    const QHostAddress address = m_allowLan ? QHostAddress::AnyIPv4 : QHostAddress::LocalHost;
    if (!m_server.listen(address, static_cast<quint16>(m_port))) {
        setLastError(QStringLiteral("Failed to start API server: %1").arg(m_server.errorString()));
        if (m_enabled) {
            m_enabled = false;
            emit enabledChanged();
            if (m_settings && m_settings->apiServerEnabled()) {
                m_settings->setApiServerEnabled(false);
            }
        }
        return false;
    }

    m_running = true;
    emit runningChanged();
    Logger::info("ApiServerService", QStringLiteral("API server listening on %1").arg(baseUrl()));
    return true;
}

void ApiServerService::stopServer()
{
    if (!m_running && !m_server.isListening()) {
        return;
    }
    m_server.close();
    m_running = false;
    emit runningChanged();
}

void ApiServerService::restartServer()
{
    stopServer();
    if (m_enabled) {
        startServer();
    }
}

void ApiServerService::setLastError(const QString &value)
{
    if (m_lastError == value) {
        return;
    }
    m_lastError = value;
    emit lastErrorChanged();
}

void ApiServerService::clearLastError()
{
    setLastError(QString());
}


// =========================================================================
// Modular Implementation Parts
// =========================================================================
#include "api/parts/ApiServer_Endpoints.cpp"
#include "api/parts/ApiServer_HttpParser.cpp"
#include "api/parts/ApiServer_Router.cpp"

} // namespace LAStudio
