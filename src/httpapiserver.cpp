#include "httpapiserver.h"

#include "httpapifiles.h"
#include "httpapijson.h"
#include "sessionlog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QHostInfo>
#include <QJSEngine>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QTcpServer>
#include <QTcpSocket>

namespace {

// Where a client is, as a person would write it (an IPv4 address seen through IPv6
// as itself)
QString peerOf(const QTcpSocket *socket)
{
    const QHostAddress address = socket->peerAddress();
    bool ok = false;
    const quint32 four = address.toIPv4Address(&ok);
    return ok ? QHostAddress(four).toString() : address.toString();
}

// A stream that a client does not read is let go of before it holds more than this
constexpr qint64 mostUnsent = 8 * 1024 * 1024;

}

HttpApi::HttpApi(QObject *parent)
    : QObject(parent)
{
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, [this] { refreshAll(true); });
}

HttpApi::~HttpApi()
{
    stopListening();
}

void HttpApi::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit settingsChanged();
    settle();
}

void HttpApi::setPort(int port)
{
    if (m_port == port)
        return;
    m_port = port;
    emit settingsChanged();
    settle();
}

void HttpApi::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    emit settingsChanged();
    changed();
}

bool HttpApi::listening() const
{
    return m_server && m_server->isListening();
}

int HttpApi::streams() const
{
    int count = 0;
    for (const Client *client : m_clients)
        count += client->streaming ? 1 : 0;
    return count;
}

void HttpApi::setHandler(const QJSValue &handler)
{
    m_handler = handler;
    emit handlerChanged();
    changed();
}

QString HttpApi::computerName() const
{
    return QHostInfo::localHostName();
}

QVariantMap HttpApi::id(const QString &uuid, const QString &name, double index) const
{
    return httpapijson::id(uuid, name, qint64(index));
}

QVariantMap HttpApi::color(const QColor &color) const
{
    return httpapijson::color(color);
}

QVariantMap HttpApi::slideStatus(const QVariantMap &current, const QVariantMap &next) const
{
    return httpapijson::slideStatus(current, next);
}

QVariantMap HttpApi::slideIndex(const QString &key, const QString &uuid, const QString &name, double listIndex, int index, int total) const
{
    return httpapijson::slideIndex(key, uuid, name, qint64(listIndex), index, total);
}

QString HttpApi::videoCountdown(double seconds) const
{
    return httpapijson::videoCountdown(seconds);
}

QVariantMap HttpApi::presentation(const QString &path, double listIndex) const
{
    return httpapifiles::presentation(path, qint64(listIndex));
}

int HttpApi::resolve(const QVariantList &items, const QString &id, const QString &uuidKey, const QString &nameKey) const
{
    QList<std::pair<QString, QString>> pairs;
    for (const QVariant &item : items) {
        const QVariantMap map = item.toMap();
        pairs.append({map.value(uuidKey).toString(), map.value(nameKey).toString()});
    }
    return httpapi::resolve(pairs, id);
}

double HttpApi::noIndex() const
{
    return double(httpapijson::noIndex);
}

QVariantList HttpApi::libraries(const QStringList &names) const
{
    return httpapijson::libraries(names);
}

QString HttpApi::libraryUuid(const QString &name) const
{
    return httpapijson::libraryUuid(name);
}

QVariantMap HttpApi::library(const QVariantList &rows) const
{
    return httpapifiles::library(rows);
}

QVariantList HttpApi::playlistTree(const QVariantList &nodes) const
{
    return httpapijson::playlistTree(nodes);
}

QVariantList HttpApi::playlistItems(const QVariantList &rows) const
{
    return httpapifiles::playlistItems(rows);
}

QVariantMap HttpApi::playlistItem(const QVariantMap &row, int index) const
{
    return httpapifiles::playlistItem(row, index);
}

QString HttpApi::presentationUuid(const QString &path) const
{
    return httpapifiles::presentationUuid(path);
}

QString HttpApi::findPresentation(const QString &uuid, const QStringList &files) const
{
    return httpapifiles::findPresentation(uuid, files);
}

QVariantMap HttpApi::timer(const QVariantMap &timer, int index) const
{
    return httpapijson::timer(timer, index);
}

QVariantMap HttpApi::timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index) const
{
    return httpapijson::timerCurrent(timer, state, index);
}

QVariantMap HttpApi::look(const QVariantMap &look, const QStringList &screenIds, const QString &name, int index) const
{
    return httpapijson::look(look, screenIds, name, index);
}

QVariantMap HttpApi::prop(const QVariantMap &prop, int index, bool active) const
{
    return httpapijson::prop(prop, index, active);
}

QVariantMap HttpApi::message(const QVariantMap &message, int index, bool active) const
{
    return httpapijson::message(message, index, active);
}

QVariantMap HttpApi::clearGroup(const QVariantMap &group, int index) const
{
    return httpapijson::clearGroup(group, index);
}

QVariantMap HttpApi::macro(const QVariantMap &macro, int index) const
{
    return httpapijson::macro(macro, index);
}

QVariantList HttpApi::binTree(const QVariantList &nodes) const
{
    return httpapijson::binTree(nodes);
}

QVariantMap HttpApi::binItem(const QVariantMap &row, int index, bool audio) const
{
    return httpapijson::binItem(row, index, audio);
}

// Listening, or not, on the port set
void HttpApi::settle()
{
    if (!m_enabled) {
        stopListening();
        return;
    }
    if (listening() && m_server->serverPort() == m_port)
        return;
    stopListening();
    listen();
}

void HttpApi::listen()
{
    if (m_port < 1 || m_port > 65535) {
        fail(QStringLiteral("%1 is not a port: choose one from 1024 to 65535.").arg(m_port));
        return;
    }
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, quint16(m_port))) {
        const QString why = m_server->serverError() == QAbstractSocket::AddressInUseError
                                ? QStringLiteral("port %1 is in use by another program: choose another.").arg(m_port)
                                : m_server->errorString();
        delete m_server;
        m_server = nullptr;
        fail(why);
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this, &HttpApi::accept);
    m_addresses.clear();
    for (const QNetworkInterface &face : QNetworkInterface::allInterfaces()) {
        if (!(face.flags() & QNetworkInterface::IsUp) || !(face.flags() & QNetworkInterface::IsRunning)
            || (face.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : face.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                m_addresses << entry.ip().toString();
        }
    }
    m_problem.clear();
    SessionLog::write("network", QStringLiteral("the API is listening on port %1, at %2").arg(m_port)
                                     .arg(m_addresses.isEmpty() ? QStringLiteral("no address on a network") : m_addresses.join(QStringLiteral(", "))));
    emit statusChanged();
}

void HttpApi::stopListening()
{
    const bool was = listening();
    while (!m_clients.isEmpty())
        drop(m_clients.first());
    delete m_server;
    m_server = nullptr;
    m_addresses.clear();
    if (was) {
        SessionLog::write("network", QStringLiteral("the API has stopped listening"));
        emit statusChanged();
    }
    if (!m_enabled && !m_problem.isEmpty()) {
        m_problem.clear();
        emit statusChanged();
    }
}

void HttpApi::fail(const QString &why)
{
    m_problem = why;
    m_addresses.clear();
    SessionLog::write("PROBLEM", QStringLiteral("The API is not listening: %1").arg(why));
    emit statusChanged();
}

void HttpApi::accept()
{
    while (m_server && m_server->hasPendingConnections()) {
        auto *client = new Client;
        client->socket = m_server->nextPendingConnection();
        m_clients << client;
        connect(client->socket, &QTcpSocket::readyRead, this, [this, client] { readFrom(client); });
        connect(client->socket, &QTcpSocket::disconnected, this, [this, client] {
            if (m_clients.contains(client))
                drop(client);
        });
    }
}

void HttpApi::drop(Client *client)
{
    m_clients.removeAll(client);
    const bool streamed = client->streaming;
    const QString peer = peerOf(client->socket);
    client->socket->disconnect(this);
    client->socket->abort();
    client->socket->deleteLater();
    if (streamed)
        SessionLog::write("network", QStringLiteral("a stream to %1 has ended").arg(peer));
    delete client;
    if (streamed) {
        emit streamsChanged();
        updateTicking();
    }
}

void HttpApi::readFrom(Client *client)
{
    if (!client->parser.feed(client->socket->readAll())) {
        httpapi::Request none;
        none.version = "HTTP/1.1";
        none.headers.insert("connection", "close");
        send(client, none, client->parser.error());
        return;
    }
    // (A stream keeps the connection: anything sent after it is not for the API.)
    while (client->parser.hasRequest() && !client->streaming && m_clients.contains(client))
        respond(client, client->parser.take());
}

void HttpApi::respond(Client *client, const httpapi::Request &request)
{
    // A browser's question whether it may (CORS): it may
    if (request.method == "OPTIONS") {
        httpapi::Headers headers = httpapi::common(request);
        headers.emplaceBack("access-control-allow-methods", "GET, OPTIONS, PUT, HEAD, TRACE, DELETE, CONNECT, PATCH, POST");
        if (request.headers.contains("access-control-request-headers"))
            headers.emplaceBack("access-control-allow-headers", request.headers.value("access-control-request-headers"));
        headers.emplaceBack("content-length", "0");
        headers.emplaceBack("date", httpapi::httpDate(QDateTime::currentDateTimeUtc()));
        client->socket->write(httpapi::head(200, headers));
        return;
    }
    const httpapi::Match found = httpapi::match(request.method, request.path);
    if (!found) {
        send(client, request, 404);
        return;
    }
    const httpapi::Route &route = *found.route;
    QVariantMap query;
    for (const auto &[key, value] : request.query)
        query.insert(key, value);

    if (route.pattern == QLatin1String("status/updates")) {
        QStringList paths;
        if (!httpapi::updatesPaths(request.body, &paths)) {
            httpapi::Request closing = request;
            closing.headers.insert("connection", "close");
            send(client, closing, 400);
            return;
        }
        // The clock every second whatever was asked for, first, as ProPresenter sends it
        QList<Watched> watched;
        if (!paths.contains(QStringLiteral("timer/system_time")))
            paths.prepend(QStringLiteral("timer/system_time"));
        for (const QString &path : std::as_const(paths)) {
            const httpapi::Match asked = httpapi::updatesPath(path);
            watched.append({path, asked.route, asked.params, {}, {}});
        }
        startStream(client, request, watched, true);
        return;
    }
    if (route.streams && request.queryOn(QStringLiteral("chunked"))) {
        const Answer first = evaluate(route, found.params, query, request.body);
        if (first.status != 200) {
            send(client, request, first.status);
            return;
        }
        startStream(client, request, {{request.path, &route, found.params, query, {}}}, false);
        return;
    }
    const Answer answer = evaluate(route, found.params, query, request.body);
    // (What operates the show is in the log, with who asked; what only reads it is not,
    // as a client may ask for it often.)
    if (!route.streams && route.pattern != QLatin1String("version"))
        SessionLog::write("network", QStringLiteral("%1 %2 from %3: %4").arg(QString::fromLatin1(request.method), request.path, peerOf(client->socket))
                                         .arg(answer.status));
    if (answer.status == 200)
        send(client, request, 200, httpapi::json(answer.data, route.pattern != QLatin1String("version")));
    else
        send(client, request, answer.status);
}

void HttpApi::send(Client *client, const httpapi::Request &request, int status, const QByteArray &body, const QByteArray &type)
{
    httpapi::Headers headers = httpapi::common(request);
    if (!body.isEmpty())
        headers.emplaceBack("content-type", type);
    // (A 204 has no body, and says so by saying nothing of its length.)
    if (status != 204)
        headers.emplaceBack("content-length", QByteArray::number(body.size()));
    const bool closing = !request.keepAlive();
    if (closing)
        headers.emplaceBack("connection", "close");
    headers.emplaceBack("date", httpapi::httpDate(QDateTime::currentDateTimeUtc()));
    client->socket->write(httpapi::head(status, headers) + body);
    if (closing)
        client->socket->disconnectFromHost();
}

void HttpApi::startStream(Client *client, const httpapi::Request &request, const QList<Watched> &watched, bool updates)
{
    httpapi::Headers headers = httpapi::common(request);
    headers.emplaceBack("transfer-encoding", "chunked");
    if (updates)
        headers.emplaceBack("connection", "close");
    headers.emplaceBack("content-type", updates ? "application/octet-stream" : "application/json");
    headers.emplaceBack("date", httpapi::httpDate(QDateTime::currentDateTimeUtc()));
    client->socket->write(httpapi::head(200, headers));
    client->streaming = true;
    client->updates = updates;
    client->watched = watched;
    QStringList paths;
    for (const Watched &one : watched)
        paths << one.path;
    SessionLog::write("network", QStringLiteral("%1 streams %2").arg(peerOf(client->socket), paths.join(QStringLiteral(", "))));
    emit streamsChanged();
    refresh(client, false);
    updateTicking();
}

void HttpApi::refresh(Client *client, bool clockedOnly)
{
    for (Watched &one : client->watched) {
        if (clockedOnly && !one.route->clocked)
            continue;
        const Answer answer = evaluate(*one.route, one.params, one.query, {});
        if (answer.status != 200)
            continue;
        const QByteArray data = httpapi::json(answer.data, false);
        if (data == one.last)
            continue;
        one.last = data;
        // ({"url": ..., "data": ...} in that order, as ProPresenter writes it)
        const QByteArray body = client->updates ? "{\"url\":" + httpapi::json(one.path, false) + ",\"data\":" + data + "}\r\n\r\n"
                                                : data + "\r\n\r\n";
        client->socket->write(httpapi::chunk(body));
    }
    if (client->socket->bytesToWrite() > mostUnsent) {
        SessionLog::write("network", QStringLiteral("%1 is not reading its stream: let go of").arg(peerOf(client->socket)));
        drop(client);
    }
}

void HttpApi::refreshAll(bool clockedOnly)
{
    const QList<Client *> clients = m_clients;
    for (Client *client : clients) {
        if (client->streaming && m_clients.contains(client))
            refresh(client, clockedOnly);
    }
}

void HttpApi::changed()
{
    if (m_refreshDue || streams() == 0)
        return;
    m_refreshDue = true;
    QMetaObject::invokeMethod(this, [this] {
        m_refreshDue = false;
        refreshAll(false);
    }, Qt::QueuedConnection);
}

void HttpApi::updateTicking()
{
    bool clocked = false;
    for (const Client *client : std::as_const(m_clients)) {
        for (const Watched &one : client->watched)
            clocked = clocked || one.route->clocked;
    }
    if (clocked && !m_tick.isActive())
        m_tick.start();
    else if (!clocked)
        m_tick.stop();
}

HttpApi::Answer HttpApi::evaluate(const httpapi::Route &route, const QVariantMap &params, const QVariantMap &query, const QByteArray &body)
{
    if (route.pattern == QLatin1String("version"))
        return {200, QVariantMap {{"name", m_name.isEmpty() ? computerName() : m_name},
                                  {"platform", QStringLiteral("linux")},
                                  {"os_version", QSysInfo::productVersion()},
                                  {"host_description", QStringLiteral("SimplePresenter %1").arg(QCoreApplication::applicationVersion())},
                                  {"api_version", QStringLiteral("v1")}}};
    // (Seconds since 1970, as ProPresenter gives it)
    if (route.pattern == QLatin1String("timer/system_time"))
        return {200, double(QDateTime::currentSecsSinceEpoch())};
    QJSEngine *engine = qjsEngine(this);
    if (!m_handler.isCallable() || !engine)
        return {503, {}};
    // (A route other than a GET is named with its method, "stage/message#PUT", as a GET
    // and a PUT may share a path.)
    const QString name = route.method == "GET" ? route.pattern : route.pattern + u'#' + QString::fromLatin1(route.method);
    const QJSValue result = m_handler.call({QJSValue(name), engine->toScriptValue(params), engine->toScriptValue(query),
                                            QJSValue(QString::fromUtf8(body))});
    if (result.isError()) {
        SessionLog::write("network", QStringLiteral("answering %1 went wrong: %2").arg(route.pattern, result.toString()));
        return {500, {}};
    }
    if (!result.isObject())
        return {404, {}};
    return {result.property(QStringLiteral("status")).toInt(), result.property(QStringLiteral("data")).toVariant()};
}
