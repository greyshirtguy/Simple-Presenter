#include "httpapiserver.h"

#include "dnssd.h"
#include "httpapifiles.h"
#include "httpapijson.h"
#include "sessionlog.h"
#include "slidepictureservice.h"

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

// A stream that a client does not read is let go of before it holds more than this; and
// so is any client whose answers it leaves unread, which it asks for in a few bytes each
constexpr qint64 mostUnsent = 8 * 1024 * 1024;
// How many clients are kept at once, and how long one may be quiet, in milliseconds, before
// it is let go: a connection held open costs a file of the process's, which has only so
// many, and a few hundred held by one program that opens them and forgets them would keep
// the app from opening a video or a presentation. A stream is never quiet. (The app's own
// numbers: what ProPresenter allows is not known. Companion keeps one connection for its
// requests and one for its stream, and makes them again if they are closed.)
constexpr int mostClients = 64;
constexpr int quietLimit = 120 * 1000;
// How many streams one ProPresenter Remote keeps (see httpapiserver.h): the app's own
// number, some four times what the Remote asks for on connecting
constexpr int mostWatched = 100;

// A status as ProPresenter gives it in a Remote's error ("404 Not Found")
QByteArray statusText(int status)
{
    return QByteArray::number(status) + ' ' + httpapi::reason(status);
}

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
    settleRemote();
}

bool HttpApi::listening() const
{
    return m_server && m_server->isListening();
}

int HttpApi::streams() const
{
    int count = 0;
    for (const Client *client : m_clients)
        count += client->streaming || (client->remote && !client->watched.isEmpty()) ? 1 : 0;
    return count;
}

void HttpApi::setHandler(const QJSValue &handler)
{
    m_handler = handler;
    emit handlerChanged();
    changed();
}

bool HttpApi::drawingPictures() const
{
    return m_pictures && m_pictures->running();
}

void HttpApi::holdPictures(int ms)
{
    if (m_pictures)
        m_pictures->hold(ms);
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

QVariantMap HttpApi::timer(const QVariantMap &timer, int index) const
{
    return httpapijson::timer(timer, index);
}

QVariantMap HttpApi::timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index) const
{
    return httpapijson::timerCurrent(timer, state, index);
}

QVariantMap HttpApi::timerChanges(const QVariantMap &body) const
{
    QString error;
    const QVariantMap changes = httpapijson::timerChanges(body, &error);
    return {{"changes", changes}, {"error", error}};
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

QVariantMap HttpApi::videoInput(const QVariantMap &input, int index) const
{
    return httpapijson::videoInput(input, index);
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
    settleRemote();
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
    settleRemote();
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
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (m_clients.size() >= mostClients) {
            // (Said once for each time it fills, not for every connection turned away)
            if (!m_full)
                SessionLog::write("network", QStringLiteral("%1 clients are connected, which is as many as are kept: %2 is turned away")
                                                 .arg(mostClients).arg(peerOf(socket)));
            m_full = true;
            socket->abort();
            socket->deleteLater();
            continue;
        }
        auto *client = new Client;
        client->socket = socket;
        client->serial = ++m_lastSerial;
        m_clients << client;
        connect(client->socket, &QTcpSocket::readyRead, this, [this, client] { readFrom(client); });
        connect(client->socket, &QTcpSocket::disconnected, this, [this, client] {
            if (m_clients.contains(client))
                drop(client);
        });
        client->quiet = new QTimer(client->socket);
        client->quiet->setSingleShot(true);
        client->quiet->setInterval(quietLimit);
        connect(client->quiet, &QTimer::timeout, this, [this, client] {
            if (m_clients.contains(client) && !client->streaming && !client->remote)
                drop(client);
        });
        client->quiet->start();
    }
}

void HttpApi::drop(Client *client)
{
    m_clients.removeAll(client);
    const bool streamed = client->streaming || client->remote;
    const QString peer = peerOf(client->socket);
    client->socket->disconnect(this);
    client->socket->abort();
    client->socket->deleteLater();
    if (client->remote)
        SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1 has gone").arg(peer));
    else if (streamed)
        SessionLog::write("network", QStringLiteral("a stream to %1 has ended").arg(peer));
    delete client;
    m_full = false;
    if (streamed) {
        emit streamsChanged();
        updateTicking();
    }
}

void HttpApi::readFrom(Client *client)
{
    if (client->remote) {
        readRemote(client, client->socket->readAll());
        return;
    }
    // (A stream keeps the connection: anything sent after it is not for the API, and is not
    // kept either. It was read as requests and kept, every one, for as long as the stream
    // lasted: a client that went on sending had the app hold all of it.)
    if (client->streaming) {
        client->socket->readAll();
        return;
    }
    client->quiet->start();
    if (!client->parser.feed(client->socket->readAll())) {
        httpapi::Request none;
        none.version = "HTTP/1.1";
        none.headers.insert("connection", "close");
        send(client, none, client->parser.error());
        return;
    }
    respondQueued(client);
}

// The requests that have come, in turn: not past one that streams (anything sent after
// it is not for the API), nor past one waiting for a picture (until it is answered)
void HttpApi::respondQueued(Client *client)
{
    while (m_clients.contains(client) && client->parser.hasRequest() && !client->streaming && !client->waiting)
        respond(client, client->parser.take());
}

HttpApi::Client *HttpApi::clientOf(quint64 serial) const
{
    for (Client *client : m_clients) {
        if (client->serial == serial)
            return client;
    }
    return nullptr;
}

void HttpApi::answerPicture(Client *client, const httpapi::Request &request, const httpapi::Route &route, const QVariantMap &params,
                            const QVariantMap &query)
{
    // (Of which file and cue: the window's, as for a trigger; the cue past the last is
    // the worker's to find, which reads the file, which this thread does not)
    const Answer where = evaluate(route, params, query, {});
    if (where.status != 200) {
        send(client, request, where.status);
        return;
    }
    const QVariantMap at = where.data.toMap();
    slidepictures::Asked asked;
    asked.file = at.value(QStringLiteral("file")).toString();
    if (at.value(QStringLiteral("arrangement")).typeId() == QMetaType::QString)
        asked.arrangement = at.value(QStringLiteral("arrangement")).toString();
    asked.index = at.value(QStringLiteral("index")).toInt();
    asked.quality = slidepictures::qualityOf(request.queryValue(QStringLiteral("quality")));
    asked.png = slidepictures::pngAsked(request.queryValue(QStringLiteral("thumbnail_type")), request.headers.value("accept"));
    if (!m_pictures) {
        m_pictures = new SlidePictureService(this);
        connect(m_pictures, &SlidePictureService::runningChanged, this, &HttpApi::picturesChanged);
    }
    client->waiting = true;
    const quint64 serial = client->serial;
    m_pictures->ask(at.value(QStringLiteral("workspace")).toString(), asked, [this, serial, request](int status, const QByteArray &type, const QByteArray &bytes) {
        Client *waiting = clientOf(serial);
        if (!waiting)
            return;
        waiting->waiting = false;
        if (status == 200)
            send(waiting, request, 200, bytes, type);
        else
            send(waiting, request, status);
        respondQueued(waiting);
    });
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
    // ProPresenter Remote's WebSocket, while the Remote is on; otherwise a path like any
    // other the API has not (a GUESS at what ProPresenter does with the Remote off)
    if (request.path == QLatin1String("/v1/ws") && request.method == "GET") {
        const proremote::Upgrade asked {request.headers.value("upgrade"), request.headers.value("connection"),
                                        request.headers.value("sec-websocket-key"), request.headers.value("sec-websocket-version")};
        if (remoteOpen() && proremote::isUpgrade(asked))
            upgrade(client, request);
        else
            send(client, request, remoteOpen() ? 400 : 404);
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
    if (route.picture) {
        answerPicture(client, request, route, found.params, query);
        return;
    }

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
    // (A client that asks and asks and reads nothing of the answers, which are many times
    // the size of what it asks, is let go as a stream it does not read is)
    if (client->socket->bytesToWrite() > mostUnsent) {
        SessionLog::write("network", QStringLiteral("%1 is not reading its answers: let go of").arg(peerOf(client->socket)));
        drop(client);
        return;
    }
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
        if (client->remote) {
            sendRemote(client, client->key, proremote::answer(one.echo, data), one.binary);
            continue;
        }
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
        if ((client->streaming || client->remote) && m_clients.contains(client))
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

// ProPresenter Remote (see httpapiserver.h and proremote.h)

void HttpApi::setRemoteEnabled(bool on)
{
    if (m_remoteEnabled == on)
        return;
    m_remoteEnabled = on;
    emit settingsChanged();
    settleRemote();
}

void HttpApi::setRemoteController(bool on)
{
    if (m_remoteController == on)
        return;
    m_remoteController = on;
    emit settingsChanged();
    settleRemote();
}

void HttpApi::setRemoteControllerPassword(const QString &password)
{
    if (m_remoteControllerPassword == password)
        return;
    m_remoteControllerPassword = password;
    emit settingsChanged();
    settleRemote();
}

void HttpApi::setRemoteObserver(bool on)
{
    if (m_remoteObserver == on)
        return;
    m_remoteObserver = on;
    emit settingsChanged();
    settleRemote();
}

void HttpApi::setRemoteObserverPassword(const QString &password)
{
    if (m_remoteObserverPassword == password)
        return;
    m_remoteObserverPassword = password;
    emit settingsChanged();
    settleRemote();
}

bool HttpApi::remoteOpen() const
{
    return m_remoteEnabled && (m_remoteController || m_remoteObserver);
}

bool HttpApi::remoteAdvertised() const
{
    return m_dnssd && m_dnssd->advertised();
}

QString HttpApi::remoteName() const
{
    return m_dnssd ? m_dnssd->nameInUse() : QString();
}

QString HttpApi::remoteProblem() const
{
    return m_dnssd ? m_dnssd->problem() : QString();
}

int HttpApi::remotes() const
{
    int count = 0;
    for (const Client *client : m_clients)
        count += client->remote ? 1 : 0;
    return count;
}

// The Remote brought to the settings: the keys its passwords make, the Remotes connected
// let go of if their password no longer lets them in, and the advertisement on the
// network there while the API listens and the Remote is on, and gone otherwise
void HttpApi::settleRemote()
{
    m_controllerKey = remoteOpen() && m_remoteController ? proremote::key(m_remoteControllerPassword) : QByteArray();
    m_observerKey = remoteOpen() && m_remoteObserver ? proremote::key(m_remoteObserverPassword) : QByteArray();
    const QList<Client *> clients = m_clients;
    for (Client *client : clients) {
        if (!client->remote || !m_clients.contains(client))
            continue;
        const bool stillIn = remoteOpen()
                             && (client->key.isEmpty() || (client->observer ? client->key == m_observerKey && client->key != m_controllerKey
                                                                            : client->key == m_controllerKey));
        if (!stillIn)
            closeRemote(client, 1008);
    }

    const bool wanted = listening() && remoteOpen();
    if (wanted && !m_dnssd) {
        m_dnssd = new DnsSd(this);
        connect(m_dnssd, &DnsSd::stateChanged, this, &HttpApi::statusChanged);
    }
    if (m_dnssd)
        m_dnssd->advertise(wanted ? DnsSd::Service {m_name.isEmpty() ? computerName() : m_name, QStringLiteral("_proapiv1ws._tcp"), m_port,
                                                    {QByteArrayLiteral("path=/v1/ws")}}
                                  : DnsSd::Service {});
}

// The HTTP request turned into a WebSocket, and the answer to /version sent at once
// under each password that is on (the controller's first), as ProPresenter sends it under
// its one, so that the Remote can tell whether its password opens it. (Two of them, with
// both on, is a GUESS: ProPresenter's with an observer's password has not been seen.)
void HttpApi::upgrade(Client *client, const httpapi::Request &request)
{
    httpapi::Headers headers {{"connection", "upgrade"}, {"upgrade", "websocket"}};
    headers += httpapi::common(request);
    headers.emplaceBack("sec-websocket-accept", proremote::acceptKey(request.headers.value("sec-websocket-key")));
    headers.emplaceBack("date", httpapi::httpDate(QDateTime::currentDateTimeUtc()));
    client->socket->write(httpapi::head(101, headers));
    client->remote = true;
    client->quiet->stop();
    const QByteArray rest = client->parser.takeRest();
    SessionLog::write("network", QStringLiteral("ProPresenter Remote has connected from %1").arg(peerOf(client->socket)));

    const httpapi::Match version = httpapi::match("GET", QStringLiteral("/version"));
    const QByteArray welcome = proremote::answer(QStringLiteral("version"), httpapi::json(evaluate(*version.route, {}, {}, {}).data, false));
    for (const QByteArray &key : {m_controllerKey, m_observerKey}) {
        if (!key.isEmpty())
            sendRemote(client, key, welcome, false);
    }
    emit streamsChanged();
    if (!rest.isEmpty())
        readRemote(client, rest);
}

void HttpApi::readRemote(Client *client, const QByteArray &bytes)
{
    if (!client->frames.feed(bytes)) {
        SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1 sent what is not a WebSocket's frames: let go of").arg(peerOf(client->socket)));
        closeRemote(client, quint16(client->frames.error()));
        return;
    }
    while (client->frames.hasMessage() && m_clients.contains(client)) {
        const proremote::FrameReader::Message message = client->frames.take();
        if (message.opcode == proremote::Ping) {
            client->socket->write(proremote::frame(proremote::Pong, message.payload));
        } else if (message.opcode == proremote::Close) {
            // (Answered with a close of its own, empty, as ProPresenter answers it)
            client->socket->write(proremote::frame(proremote::Close, {}));
            client->socket->disconnectFromHost();
            return;
        } else if (message.opcode == proremote::Text || message.opcode == proremote::Binary) {
            heard(client, message);
        }
    }
    if (m_clients.contains(client) && client->socket->bytesToWrite() > mostUnsent) {
        SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1 is not reading its answers: let go of").arg(peerOf(client->socket)));
        drop(client);
    }
}

// A message: opened with the client's key once it has one, and until then with each
// password's in turn, the controller's first. One that opens with none is not answered,
// as ProPresenter answers none (the log says so once for each client).
void HttpApi::heard(Client *client, const proremote::FrameReader::Message &message)
{
    const bool binary = message.opcode == proremote::Binary;
    const QByteArray sealed = binary ? message.payload : QByteArray::fromBase64(message.payload);
    QList<std::pair<QByteArray, bool>> keys;
    if (!client->key.isEmpty())
        keys.append({client->key, client->observer});
    else {
        if (!m_controllerKey.isEmpty())
            keys.append({m_controllerKey, false});
        if (!m_observerKey.isEmpty())
            keys.append({m_observerKey, true});
    }
    for (const auto &[key, observer] : std::as_const(keys)) {
        proremote::Request request;
        if (!proremote::read(proremote::open(key, sealed), &request))
            continue;
        if (client->key.isEmpty()) {
            client->key = key;
            client->observer = observer;
            SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1 is in, as %2")
                                             .arg(peerOf(client->socket), observer ? QStringLiteral("an observer") : QStringLiteral("a controller")));
        }
        answerRemote(client, request, binary);
        return;
    }
    if (!client->toldUnopened) {
        client->toldUnopened = true;
        SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1 sent a message that does not open with the Remote's passwords: "
                                                    "it is not answered (a password that is not the one set?)").arg(peerOf(client->socket)));
    }
}

// A message answered by the HTTP API's routes, as its request would be, and the answer
// sent back with the url it was asked with
void HttpApi::answerRemote(Client *client, const proremote::Request &request, bool binary)
{
    const auto reply = [&](const QByteArray &json) { sendRemote(client, client->key, json, binary); };
    const httpapi::Match found = request.url.isEmpty() ? httpapi::Match {} : httpapi::match(request.method, request.path);
    if (!found || found.route->pattern == QLatin1String("status/updates") || found.route->picture) {
        reply(proremote::failure(request.url, statusText(request.url.isEmpty() ? 400 : 404)));
        return;
    }
    const httpapi::Route &route = *found.route;
    if (client->observer && httpapi::operates(route)) {
        SessionLog::write("network", QStringLiteral("ProPresenter Remote at %1, an observer, may not %2 %3").arg(peerOf(client->socket), QString::fromLatin1(request.method), request.path));
        reply(proremote::failure(request.url, statusText(403)));
        return;
    }
    QVariantMap query;
    for (const auto &[key, value] : request.query)
        query.insert(key, value);

    if (request.chunked) {
        // (Only what streams streams: anything else asked for so is refused, as
        // ProPresenter refuses libraries asked for so, with 400; but what is not there at
        // all is 404 first, as ProPresenter answers the Remote's own macro/collections.
        // What operates is never run to find that out.)
        if (!route.streams) {
            const bool absent = !httpapi::operates(route) && evaluate(route, found.params, query, request.body).status == 404;
            reply(proremote::failure(request.url, statusText(absent ? 404 : 400)));
            return;
        }
        const Answer first = evaluate(route, found.params, query, request.body);
        if (first.status != 200) {
            reply(proremote::failure(request.url, statusText(first.status)));
            return;
        }
        const QByteArray data = httpapi::json(first.data, false);
        const bool wasStreaming = !client->watched.isEmpty();
        client->watched.removeIf([&request](const Watched &one) { return one.echo == request.url; });
        while (client->watched.size() >= mostWatched)
            client->watched.removeFirst();
        Watched made {request.path, &route, found.params, query, data};
        made.echo = request.url;
        made.binary = binary;
        client->watched.append(made);
        reply(proremote::answer(request.url, data));
        if (!wasStreaming)
            emit streamsChanged();
        updateTicking();
        return;
    }
    const Answer answer = evaluate(route, found.params, query, request.body);
    // (What operates the show is in the log, with who asked, as the HTTP API's is)
    if (httpapi::operates(route))
        SessionLog::write("network", QStringLiteral("%1 %2 from ProPresenter Remote at %3: %4")
                                         .arg(QString::fromLatin1(request.method), request.path, peerOf(client->socket)).arg(answer.status));
    if (answer.status == 200)
        reply(proremote::answer(request.url, httpapi::json(answer.data, false)));
    else if (answer.status != 204)
        reply(proremote::failure(request.url, statusText(answer.status)));
}

void HttpApi::sendRemote(Client *client, const QByteArray &key, const QByteArray &json, bool binary)
{
    const QByteArray sealed = proremote::seal(key, json);
    client->socket->write(proremote::frame(binary ? proremote::Binary : proremote::Text, binary ? sealed : sealed.toBase64()));
}

// A Remote let go of with a close frame first, saying why (RFC 6455's codes: 1002 what
// it sent was not frames, 1008 its password no longer lets it in, 1009 too big)
void HttpApi::closeRemote(Client *client, quint16 code)
{
    QByteArray payload;
    payload.append(char(code >> 8));
    payload.append(char(code & 0xff));
    client->socket->write(proremote::frame(proremote::Close, payload));
    client->socket->flush();
    drop(client);
}
