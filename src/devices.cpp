#include "devices.h"

#include "sessionlog.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

namespace {

QString quoted(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("(unnamed)") : QStringLiteral("“%1”").arg(name);
}

QString kindName(const QString &kind)
{
    return kind == QLatin1String("midi") ? QStringLiteral("MIDI") : kind == QLatin1String("rosstalk") ? QStringLiteral("RossTalk")
                                                                                                         : QStringLiteral("HTTP Requests");
}

// Whether a port's name, as a connection keeps it, is this port (see AlsaMidi::find)
bool namesPort(const QString &name, const QString &client, const QString &port)
{
    return name == client + u':' + port || name == port || name == client;
}

// How long a RossTalk connection waits before it tries again, and how long an HTTP
// request is given
constexpr int retryMilliseconds = 3000;
constexpr int requestMilliseconds = 10000;

}

// What a connection is doing now, beside what it is
struct Devices::Live
{
    QString state = QStringLiteral("off");
    QString problem;
    QTcpSocket *socket = nullptr;
    QTimer *retry = nullptr;
};

Devices::Devices(QObject *parent)
    : QObject(parent)
{
    connect(&m_midi, &AlsaMidi::portsChanged, this, [this] {
        // A port that comes back is listened to again by a connection that reconnects
        listenToMidi();
        emit portsChanged();
    });
    connect(&m_midi, &AlsaMidi::noteHeard, this, &Devices::noteHeard);
    m_midiMap = communications::midiMap({});
}

Devices::~Devices()
{
    qDeleteAll(m_live);
}

QString Devices::file() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/SimplePresenter/Devices.json");
}

void Devices::save()
{
    if (!m_remember || m_workspace.isEmpty())
        return;
    const QString error = communications::write(file(), m_workspace, m_list);
    if (!error.isEmpty())
        SessionLog::write("PROBLEM", error);
}

Devices::Live &Devices::liveOf(const QString &id)
{
    Live *&live = m_live[id];
    if (!live)
        live = new Live;
    return *live;
}

int Devices::indexOf(const QString &id) const
{
    for (int i = 0; i < m_list.size(); ++i) {
        if (m_list.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantList Devices::connections() const
{
    QVariantList list;
    for (const communications::Connection &connection : m_list) {
        QVariantMap map = communications::toMap(connection);
        const Live *live = m_live.value(connection.id);
        map.insert("state", connection.kind == QLatin1String("http") ? QStringLiteral("ready") : live ? live->state : QStringLiteral("off"));
        map.insert("problem", live ? live->problem : QString());
        map.insert("kindName", kindName(connection.kind));
        list.append(map);
    }
    return list;
}

QVariantList Devices::offered() const
{
    QVariantList list;
    for (const communications::Connection &device : m_pro) {
        if (indexOf(device.id) < 0) {
            QVariantMap map = communications::toMap(device);
            map.insert("kindName", kindName(device.kind));
            list.append(map);
        }
    }
    return list;
}

QStringList Devices::midiSources() const
{
    QStringList names;
    for (const AlsaMidi::Port &port : m_midi.ports()) {
        if (port.source && !names.contains(AlsaMidi::nameOf(port)))
            names.append(AlsaMidi::nameOf(port));
    }
    return names;
}

QStringList Devices::midiDestinations() const
{
    QStringList names;
    for (const AlsaMidi::Port &port : m_midi.ports()) {
        if (port.destination && !names.contains(AlsaMidi::nameOf(port)))
            names.append(AlsaMidi::nameOf(port));
    }
    return names;
}

QVariantList Devices::midiMap() const
{
    QVariantList list;
    for (const communications::MidiCommand &command : communications::midiCommands()) {
        const int note = m_midiMap.value(command.key, command.note);
        list.append(QVariantMap {{"key", command.key}, {"label", command.label}, {"group", command.group}, {"note", note},
                                 {"noteName", communications::noteName(note)}, {"byIndex", command.byIndex}});
    }
    return list;
}

void Devices::open(const QString &workspace)
{
    for (const communications::Connection &connection : std::as_const(m_list))
        disconnectOne(connection.id);
    qDeleteAll(m_live);
    m_live.clear();
    m_workspace = workspace;
    int skipped = 0;
    m_pro = communications::proDevices(workspace, &skipped);
    m_list = m_remember ? communications::read(file(), workspace) : QList<communications::Connection>();
    m_midiMap = communications::midiMap(m_remember ? communications::readMidiMap(file()) : QVariantMap());
    QStringList said;
    for (communications::Connection &connection : m_list) {
        // As ProPresenter has Auto Reconnect: what was connected, and reconnects, is
        // connected again; anything else starts down
        if (connection.connected && !connection.autoReconnect)
            connection.connected = false;
        said << QStringLiteral("%1 (%2%3)").arg(quoted(connection.name), kindName(connection.kind),
                                                connection.connected ? QStringLiteral(", to be connected") : QString());
    }
    SessionLog::write("devices", QStringLiteral("%1 for this workspace%2; ProPresenter's file has %3 more of the kinds there are here%4")
                                     .arg(m_list.size()).arg(said.isEmpty() ? QString() : QStringLiteral(": ") + said.join(QStringLiteral(", ")))
                                     .arg(offered().size())
                                     .arg(skipped > 0 ? QStringLiteral(", and %1 of kinds this app does not have").arg(skipped) : QString()));
    for (const communications::Connection &connection : std::as_const(m_list)) {
        if (connection.connected)
            connectOne(connection);
    }
    emit changed();
    emit midiMapChanged();
    emit portsChanged();
}

QVariantMap Devices::add(const QString &kind, const QString &proId)
{
    communications::Connection made;
    if (!proId.isEmpty()) {
        for (const communications::Connection &device : std::as_const(m_pro)) {
            if (device.id == proId)
                made = device;
        }
        if (made.id.isEmpty())
            return {{"id", QString()}, {"error", QStringLiteral("ProPresenter's file no longer has that device")}};
        if (indexOf(made.id) >= 0)
            return {{"id", made.id}, {"error", QString()}};
    } else {
        if (kind != QLatin1String("midi") && kind != QLatin1String("rosstalk") && kind != QLatin1String("http"))
            return {{"id", QString()}, {"error", QStringLiteral("That is not a kind of device this app has")}};
        made.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
        made.kind = kind;
        // Named for its kind, as ProPresenter names a new device, with a number for a
        // name that is taken
        const QString base = kindName(kind);
        made.name = base;
        for (int n = 2; std::any_of(m_list.cbegin(), m_list.cend(), [&made](const auto &c) { return c.name == made.name; }); ++n)
            made.name = base + u' ' + QString::number(n);
        if (kind == QLatin1String("http"))
            made.baseUrl = QStringLiteral("http://");
    }
    made.connected = false;
    m_list.append(made);
    save();
    SessionLog::write("devices", QStringLiteral("%1 added (%2)%3").arg(quoted(made.name), kindName(made.kind),
                                                                       proId.isEmpty() ? QString() : QStringLiteral(", from ProPresenter's file")));
    emit changed();
    return {{"id", made.id}, {"error", QString()}};
}

QString Devices::change(const QString &id, const QVariantMap &changes)
{
    const int at = indexOf(id);
    if (at < 0)
        return QStringLiteral("That device is not there any more");
    QVariantMap map = communications::toMap(m_list.at(at));
    for (auto change = changes.constBegin(); change != changes.constEnd(); ++change) {
        if (change.key() != QLatin1String("id") && change.key() != QLatin1String("kind") && map.contains(change.key()))
            map.insert(change.key(), change.value());
    }
    const communications::Connection now = communications::fromMap(map);
    if (now.name.trimmed().isEmpty())
        return QStringLiteral("A device needs a name");
    const communications::Connection was = m_list.at(at);
    m_list[at] = now;
    save();
    // What it connects to has changed: connected again, as it now is
    if (now.connected && (was.sources != now.sources || was.destinations != now.destinations || was.address != now.address
                          || was.port != now.port)) {
        disconnectOne(id);
        connectOne(now);
    }
    emit changed();
    return {};
}

QString Devices::remove(const QString &id)
{
    const int at = indexOf(id);
    if (at < 0)
        return QStringLiteral("That device is not there any more");
    disconnectOne(id);
    delete m_live.take(id);
    SessionLog::write("devices", QStringLiteral("%1 taken away").arg(quoted(m_list.at(at).name)));
    m_list.removeAt(at);
    save();
    listenToMidi();
    emit changed();
    return {};
}

void Devices::setConnected(const QString &id, bool on)
{
    const int at = indexOf(id);
    if (at < 0 || m_list.at(at).kind == QLatin1String("http"))
        return;
    m_list[at].connected = on;
    save();
    if (on) {
        const QString problem = communications::problemWith(m_list.at(at));
        if (!problem.isEmpty()) {
            Live &live = liveOf(id);
            live.state = QStringLiteral("off");
            live.problem = problem;
            m_list[at].connected = false;
            emit changed();
            return;
        }
        SessionLog::write("devices", QStringLiteral("%1 connecting").arg(quoted(m_list.at(at).name)));
        connectOne(m_list.at(at));
    } else {
        SessionLog::write("devices", QStringLiteral("%1 disconnected").arg(quoted(m_list.at(at).name)));
        disconnectOne(id);
        Live &live = liveOf(id);
        live.state = QStringLiteral("off");
        live.problem.clear();
    }
    emit changed();
}

void Devices::connectOne(const communications::Connection &connection)
{
    if (connection.kind == QLatin1String("rosstalk")) {
        connectRossTalk(connection);
    } else if (connection.kind == QLatin1String("midi")) {
        Live &live = liveOf(connection.id);
        QString error;
        if (!m_midi.open(&error)) {
            m_midiProblem = error;
            live.state = QStringLiteral("off");
            live.problem = error;
            SessionLog::write("PROBLEM", error);
            emit portsChanged();
            return;
        }
        listenToMidi();
    }
}

void Devices::disconnectOne(const QString &id)
{
    Live *live = m_live.value(id);
    if (!live)
        return;
    if (live->retry)
        live->retry->stop();
    if (live->socket) {
        live->socket->disconnect(this);
        live->socket->abort();
        live->socket->deleteLater();
        live->socket = nullptr;
    }
    live->state = QStringLiteral("off");
    const int at = indexOf(id);
    if (at >= 0 && m_list.at(at).kind == QLatin1String("midi"))
        QTimer::singleShot(0, this, &Devices::listenToMidi);
}

void Devices::connectRossTalk(const communications::Connection &connection)
{
    const QString id = connection.id;
    Live &live = liveOf(id);
    if (!live.retry) {
        live.retry = new QTimer(this);
        live.retry->setSingleShot(true);
        live.retry->setInterval(retryMilliseconds);
        connect(live.retry, &QTimer::timeout, this, [this, id] {
            const int at = indexOf(id);
            if (at >= 0 && m_list.at(at).connected)
                connectRossTalk(m_list.at(at));
        });
    }
    if (live.socket) {
        live.socket->disconnect(this);
        live.socket->abort();
        live.socket->deleteLater();
    }
    live.socket = new QTcpSocket(this);
    live.state = QStringLiteral("connecting");
    QTcpSocket *socket = live.socket;
    connect(socket, &QTcpSocket::connected, this, [this, id, socket] {
        Live &live = liveOf(id);
        if (live.socket != socket)
            return;
        live.state = QStringLiteral("connected");
        live.problem.clear();
        const int at = indexOf(id);
        SessionLog::write("devices", QStringLiteral("%1 connected to %2:%3").arg(quoted(at >= 0 ? m_list.at(at).name : QString()),
                                                                               socket->peerName()).arg(socket->peerPort()));
        emit changed();
    });
    const auto lost = [this, id, socket](const QString &why) {
        Live &live = liveOf(id);
        if (live.socket != socket)
            return;
        const int at = indexOf(id);
        const bool again = at >= 0 && m_list.at(at).connected && m_list.at(at).autoReconnect;
        const bool was = live.state == QStringLiteral("connected");
        // (Said once for each new reason, and not at each try of every few seconds.)
        const bool news = was || live.problem != why || !again;
        live.state = again ? QStringLiteral("connecting") : QStringLiteral("off");
        live.problem = why;
        if (news)
            SessionLog::write(again ? "devices" : "PROBLEM", QStringLiteral("%1: %2%3").arg(quoted(at >= 0 ? m_list.at(at).name : QString()), why,
                                                                                        again ? QStringLiteral("; tried again every few seconds") : QString()));
        if (again)
            live.retry->start();
        else if (at >= 0)
            m_list[at].connected = false;
        emit changed();
    };
    connect(socket, &QTcpSocket::errorOccurred, this, [lost, socket] { lost(socket->errorString()); });
    connect(socket, &QTcpSocket::disconnected, this, [lost] { lost(QStringLiteral("the switcher closed the connection")); });
    socket->connectToHost(connection.address.trimmed(), quint16(connection.port));
    emit changed();
}

void Devices::listenToMidi()
{
    if (!m_midi.isOpen())
        return;
    QStringList wanted;
    for (const communications::Connection &connection : std::as_const(m_list)) {
        if (connection.kind == QLatin1String("midi") && connection.connected) {
            for (const QString &source : connection.sources) {
                if (!wanted.contains(source))
                    wanted.append(source);
            }
        }
    }
    const QStringList missing = m_midi.listenTo(wanted);
    // Each MIDI connection: connected, and what of it is not there
    const QList<AlsaMidi::Port> ports = m_midi.ports();
    bool any = false;
    for (communications::Connection &connection : m_list) {
        if (connection.kind != QLatin1String("midi"))
            continue;
        Live &live = liveOf(connection.id);
        if (!connection.connected) {
            live.state = QStringLiteral("off");
            continue;
        }
        QStringList gone;
        for (const QString &source : connection.sources) {
            if (missing.contains(source))
                gone.append(source);
        }
        for (const QString &destination : connection.destinations) {
            const bool there = std::any_of(ports.cbegin(), ports.cend(), [&destination](const AlsaMidi::Port &port) {
                return port.destination && namesPort(destination, port.clientName, port.portName);
            });
            if (!there)
                gone.append(destination);
        }
        const QString problem = gone.isEmpty() ? QString() : QStringLiteral("Not here: %1").arg(gone.join(QStringLiteral(", ")));
        if (problem != live.problem && !problem.isEmpty())
            SessionLog::write("devices", QStringLiteral("%1: %2").arg(quoted(connection.name), problem));
        live.problem = problem;
        // A port that has gone, without Auto Reconnect, takes the connection down
        if (!gone.isEmpty() && !connection.autoReconnect && live.state == QStringLiteral("connected")) {
            connection.connected = false;
            live.state = QStringLiteral("off");
        } else {
            live.state = QStringLiteral("connected");
        }
        any = true;
    }
    if (any)
        emit changed();
}

QVariantMap Devices::find(const QString &id, const QString &name) const
{
    const QVariantList all = connections();
    for (const QVariant &entry : all) {
        if (!id.isEmpty() && entry.toMap().value("id").toString() == id)
            return entry.toMap();
    }
    for (const QVariant &entry : all) {
        if (!name.isEmpty() && entry.toMap().value("name").toString() == name)
            return entry.toMap();
    }
    return {};
}

void Devices::tell(const QString &device, bool ok, const QString &said)
{
    SessionLog::write(ok ? "devices" : "PROBLEM", quoted(device) + QStringLiteral(": ") + said);
    emit sentOut(device, ok, said);
}

void Devices::send(const QVariantMap &action)
{
    const QVariantMap found = find(action.value("deviceId").toString(), action.value("deviceName").toString());
    const QString name = action.value("deviceName").toString();
    if (found.isEmpty()) {
        tell(name, false, QStringLiteral("there is no such device here: Settings, Devices"));
        return;
    }
    const communications::Connection connection = m_list.at(indexOf(found.value("id").toString()));
    const QString command = action.value("command").toString();
    const QString wantedKind = command == QLatin1String("midi") ? QStringLiteral("midi") : command == QLatin1String("words") ? QStringLiteral("rosstalk")
                             : QStringLiteral("http");
    if (connection.kind != wantedKind) {
        tell(connection.name, false, QStringLiteral("it is a %1 device, and this is for a %2 one").arg(kindName(connection.kind), kindName(wantedKind)));
        return;
    }
    const Live *live = m_live.value(connection.id);
    if (command == QLatin1String("midi")) {
        if (!connection.connected || !live || live->state != QStringLiteral("connected")) {
            tell(connection.name, false, QStringLiteral("not sent: it is not connected"));
            return;
        }
        const bool on = action.value("on").toBool();
        const int channel = std::max(1, action.value("channel").toInt());
        const int note = action.value("note").toInt();
        const int velocity = action.value("intensity").toInt();
        const QStringList missing = m_midi.send(connection.destinations, on, channel, note, velocity);
        const QString what = QStringLiteral("note %1 %2 (%3), velocity %4, channel %5").arg(on ? QStringLiteral("on") : QStringLiteral("off"))
                                 .arg(note).arg(communications::noteName(note)).arg(velocity).arg(channel);
        if (connection.destinations.isEmpty())
            tell(connection.name, false, what + QStringLiteral(" not sent: it has no port to send to"));
        else if (missing.size() == connection.destinations.size())
            tell(connection.name, false, what + QStringLiteral(" not sent: ") + missing.join(QStringLiteral(", ")) + QStringLiteral(" not here"));
        else
            tell(connection.name, true, what + QStringLiteral(" sent to ")
                                          + QStringList(connection.destinations).join(QStringLiteral(", "))
                                          + (missing.isEmpty() ? QString() : QStringLiteral(" (not ") + missing.join(QStringLiteral(", ")) + u')'));
    } else if (command == QLatin1String("words")) {
        const QString words = action.value("words").toString();
        if (!live || !live->socket || live->state != QStringLiteral("connected")) {
            tell(connection.name, false, QStringLiteral("%1 not sent: it is not connected").arg(quoted(words)));
            return;
        }
        live->socket->write(words.toUtf8() + "\r\n");
        tell(connection.name, true, QStringLiteral("%1 sent").arg(quoted(words)));
    } else {
        QString error;
        const communications::Request request = communications::httpRequest(connection, action, &error);
        if (!error.isEmpty()) {
            tell(connection.name, false, error);
            return;
        }
        if (!m_network)
            m_network = new QNetworkAccessManager(this);
        QNetworkRequest asked(request.url);
        asked.setTransferTimeout(requestMilliseconds);
        for (const auto &header : request.headers)
            asked.setRawHeader(header.first, header.second);
        QNetworkReply *reply = m_network->sendCustomRequest(asked, request.method.toLatin1(), request.body);
        const QString what = request.method + u' ' + request.url.toString(QUrl::RemoveUserInfo);
        auto *clock = new QElapsedTimer;
        clock->start();
        const QString device = connection.name;
        connect(reply, &QNetworkReply::finished, this, [this, reply, what, clock, device] {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const qint64 took = clock->elapsed();
            delete clock;
            if (reply->error() == QNetworkReply::NoError)
                tell(device, true, QStringLiteral("%1: %2 in %3 ms").arg(what).arg(status).arg(took));
            else
                tell(device, false, QStringLiteral("%1: %2%3").arg(what, reply->errorString(),
                                                                    status > 0 ? QStringLiteral(" (%1)").arg(status) : QString()));
            reply->deleteLater();
        });
    }
}

void Devices::noteHeard(const QString &source, int channel, int note, int velocity, bool on)
{
    // Only from the sources of a MIDI connection that is connected
    const QList<AlsaMidi::Port> ports = m_midi.ports();
    const AlsaMidi::Port *port = nullptr;
    for (const AlsaMidi::Port &candidate : ports) {
        if (AlsaMidi::nameOf(candidate) == source)
            port = &candidate;
    }
    QString heardBy;
    for (const communications::Connection &connection : std::as_const(m_list)) {
        if (connection.kind != QLatin1String("midi") || !connection.connected)
            continue;
        for (const QString &wanted : connection.sources) {
            if (port ? namesPort(wanted, port->clientName, port->portName) : wanted == source)
                heardBy = connection.name;
        }
    }
    const QString key = on ? communications::commandForNote(m_midiMap, note) : QString();
    QString label;
    for (const communications::MidiCommand &command : communications::midiCommands()) {
        if (command.key == key)
            label = command.label;
    }
    m_lastNote = QStringLiteral("%1 (%2), velocity %3, channel %4, from %5%6").arg(communications::noteName(note)).arg(note).arg(velocity)
                     .arg(channel).arg(source.isEmpty() ? QStringLiteral("a port") : source)
                     .arg(!on ? QStringLiteral(": off") : label.isEmpty() ? QStringLiteral(": no command") : QStringLiteral(": ") + label);
    emit lastNoteChanged();
    if (!on || heardBy.isEmpty() || key.isEmpty())
        return;
    SessionLog::write("devices", QStringLiteral("MIDI from %1: %2").arg(quoted(heardBy), m_lastNote));
    emit midiCommand(key, velocity);
}

void Devices::setMidiNote(const QString &key, int note)
{
    if (!m_midiMap.contains(key) || note < -1 || note > 127)
        return;
    m_midiMap.insert(key, note);
    if (m_remember) {
        const QString error = communications::writeMidiMap(file(), m_midiMap);
        if (!error.isEmpty())
            SessionLog::write("PROBLEM", error);
    }
    emit midiMapChanged();
}

void Devices::autoFillMidiMap(int first)
{
    m_midiMap = communications::autoFilled(first);
    if (m_remember) {
        const QString error = communications::writeMidiMap(file(), m_midiMap);
        if (!error.isEmpty())
            SessionLog::write("PROBLEM", error);
    }
    SessionLog::write("devices", QStringLiteral("the MIDI Map filled from note %1").arg(first));
    emit midiMapChanged();
}
