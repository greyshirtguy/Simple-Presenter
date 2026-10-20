#include "communications.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUuid>
#include <QXmlStreamReader>

#include <algorithm>

namespace communications {

namespace {

using Communication = rv::data::Action::CommunicationType;

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

QString quoted(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("(unnamed)") : QStringLiteral("“%1”").arg(name);
}

// What ProPresenter calls its commands, as an action's name has them, and the Custom
// Control as ProPresenter writes one (seen in a file of the maintainer's, 2026-10-20)
const QString noteOnName = QStringLiteral("MIDI Note On");
const QString noteOffName = QStringLiteral("MIDI Note Off");
const QString customControlName = QStringLiteral("Custom Control");
const QString customControlFormat = QStringLiteral("CC x");
const QString customControlDescription = QStringLiteral("Executes the specified custom control.");
// An HTTP request, which is this app's own, in the same general form: a format that
// says what it is, and its parts as named values
const QString httpName = QStringLiteral("HTTP Request");
const QString httpFormat = QStringLiteral("HTTP");
const QString httpDescription = QStringLiteral("Sends an HTTP request.");

// The whole file, as JSON; an empty object for one that is not there or not JSON
QJsonObject readFile(const QString &file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(in.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

QString writeFile(const QString &file, const QJsonObject &whole)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly))
        return QStringLiteral("The devices could not be saved: %1").arg(out.errorString());
    out.write(QJsonDocument(whole).toJson(QJsonDocument::Indented));
    if (!out.commit())
        return QStringLiteral("The devices could not be saved: %1").arg(out.errorString());
    return {};
}

// A header written "Name: value", as its two parts; an empty name for one that is not
QPair<QByteArray, QByteArray> headerOf(const QString &line)
{
    const int colon = int(line.indexOf(u':'));
    if (colon <= 0)
        return {};
    return {line.left(colon).trimmed().toUtf8(), line.mid(colon + 1).trimmed().toUtf8()};
}

// The named value of a communication action of this name, or null
const Communication::Command *commandNamed(const Communication &communication, const QString &name)
{
    for (const Communication::Command &command : communication.commands()) {
        if (text(command.name()) == name)
            return &command;
    }
    return nullptr;
}

bool isHttp(const Communication &communication)
{
    return text(communication.format()) == httpFormat && commandNamed(communication, QStringLiteral("Method")) != nullptr;
}

Communication::Command *addCommand(Communication *communication, const QString &name, const QString &value)
{
    Communication::Command *made = communication->add_commands();
    made->set_name(name.toStdString());
    made->set_value(value.toStdString());
    return made;
}

} // namespace

// ---- Connections

QVariantMap toMap(const Connection &connection)
{
    return {
        {"id", connection.id},
        {"name", connection.name},
        {"kind", connection.kind},
        {"connected", connection.connected},
        {"autoReconnect", connection.autoReconnect},
        {"sources", connection.sources},
        {"destinations", connection.destinations},
        {"address", connection.address},
        {"port", connection.port},
        {"baseUrl", connection.baseUrl},
        {"headers", connection.headers},
    };
}

Connection fromMap(const QVariantMap &map)
{
    Connection connection;
    connection.id = map.value("id").toString();
    connection.name = map.value("name").toString();
    connection.kind = map.value("kind").toString();
    connection.connected = map.value("connected").toBool();
    connection.autoReconnect = map.value("autoReconnect").toBool();
    connection.sources = map.value("sources").toStringList();
    connection.destinations = map.value("destinations").toStringList();
    connection.address = map.value("address").toString();
    connection.port = map.contains("port") ? map.value("port").toInt() : 7788;
    connection.baseUrl = map.value("baseUrl").toString();
    connection.headers = map.value("headers").toStringList();
    return connection;
}

QString problemWith(const Connection &connection)
{
    if (connection.name.trimmed().isEmpty())
        return QStringLiteral("A device needs a name");
    if (connection.kind == QLatin1String("midi")) {
        if (connection.sources.isEmpty() && connection.destinations.isEmpty())
            return QStringLiteral("Choose a port to hear notes from, or one to send them to");
    } else if (connection.kind == QLatin1String("rosstalk")) {
        if (connection.address.trimmed().isEmpty())
            return QStringLiteral("A RossTalk device needs the switcher's address");
        if (connection.port < 1 || connection.port > 65535)
            return QStringLiteral("A port is a number from 1 to 65535");
    } else if (connection.kind == QLatin1String("http")) {
        const QUrl base(connection.baseUrl.trimmed());
        if (!base.isValid() || (base.scheme() != QLatin1String("http") && base.scheme() != QLatin1String("https")) || base.host().isEmpty())
            return QStringLiteral("The base address is an address that starts with http:// or https://");
        for (const QString &header : connection.headers) {
            if (headerOf(header).first.isEmpty())
                return QStringLiteral("A header is written Name: value");
        }
    } else {
        return QStringLiteral("That is not a kind of device this app has");
    }
    return {};
}

QList<Connection> read(const QString &file, const QString &workspace)
{
    QList<Connection> connections;
    const QJsonArray list = readFile(file).value("workspaces").toObject().value(workspace).toArray();
    for (const QJsonValue &entry : list)
        connections.append(fromMap(entry.toObject().toVariantMap()));
    return connections;
}

QString write(const QString &file, const QString &workspace, const QList<Connection> &connections)
{
    QJsonObject whole = readFile(file);
    QJsonObject workspaces = whole.value("workspaces").toObject();
    QJsonArray list;
    for (const Connection &connection : connections)
        list.append(QJsonObject::fromVariantMap(toMap(connection)));
    workspaces.insert(workspace, list);
    whole.insert("workspaces", workspaces);
    return writeFile(file, whole);
}

QList<Connection> proDevicesFrom(const QByteArray &json, int *skipped)
{
    if (skipped)
        *skipped = 0;
    QList<Connection> devices;
    const QJsonArray list = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &entry : list) {
        const QJsonObject device = entry.toObject();
        Connection made;
        made.id = device.value("id").toString();
        made.name = device.value("name").toString();
        made.autoReconnect = device.value("reconnect").toBool();
        // The device's parser: XML naming the kind of device and its hardware, in
        // UTF-16 with its byte-order mark, in base 64
        const QByteArray raw = QByteArray::fromBase64(device.value("parser").toString().toLatin1());
        QStringDecoder decoder(QStringDecoder::Utf16);
        QXmlStreamReader xml(QString(decoder(raw)));
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement)
                continue;
            const QString element = xml.name().toString();
            const QXmlStreamAttributes attributes = xml.attributes();
            if (element == QLatin1String("RVProtocolParserMIDI")) {
                made.kind = QStringLiteral("midi");
            } else if (element == QLatin1String("RVProtocolParserRossTalk")) {
                made.kind = QStringLiteral("rosstalk");
            } else if (element == QLatin1String("RVMIDIHardwareCommunicator")) {
                made.sources = attributes.value("sources").toString().split(u'|', Qt::SkipEmptyParts);
                made.destinations = attributes.value("destinations").toString().split(u'|', Qt::SkipEmptyParts);
            } else if (element == QLatin1String("RVTCPNetworkHardwareCommunicator")) {
                made.address = attributes.value("address").toString();
                const int port = attributes.value("port").toInt();
                if (port > 0)
                    made.port = port;
            }
        }
        if (made.kind.isEmpty() || made.id.isEmpty()) {
            if (skipped)
                ++*skipped;
            continue;
        }
        devices.append(made);
    }
    return devices;
}

QList<Connection> proDevices(const QString &workspace, int *skipped)
{
    QFile in(QDir(workspace).filePath(QStringLiteral("Configuration/CommunicationDevices")));
    if (!in.open(QIODevice::ReadOnly)) {
        if (skipped)
            *skipped = 0;
        return {};
    }
    return proDevicesFrom(in.readAll(), skipped);
}

// ---- What is sent

QString commandText(const Communication &communication)
{
    QString made = text(communication.format());
    // From the last part of the format to the first, so that each range is still where
    // it was when it comes to be filled
    QList<const Communication::Command *> filling;
    for (const Communication::Command &command : communication.commands()) {
        if (command.has_replacement_range())
            filling.append(&command);
    }
    std::sort(filling.begin(), filling.end(), [](const Communication::Command *a, const Communication::Command *b) {
        return a->replacement_range().start() > b->replacement_range().start();
    });
    for (const Communication::Command *command : std::as_const(filling)) {
        const int start = command->replacement_range().start();
        const int end = command->replacement_range().end();
        if (start < 0 || end < start || end > made.size())
            continue;
        made.replace(start, end - start, text(command->value()));
    }
    return made;
}

QByteArray midiMessage(bool on, int channel, int note, int velocity)
{
    const int lane = std::clamp(channel, 1, 16) - 1;
    QByteArray message;
    message.append(char((on ? 0x90 : 0x80) | lane));
    message.append(char(std::clamp(note, 0, 127)));
    message.append(char(std::clamp(velocity, 0, 127)));
    return message;
}

Request httpRequest(const Connection &connection, const QVariantMap &action, QString *error)
{
    Request request;
    error->clear();
    request.method = action.value("method").toString().toUpper();
    if (!httpMethods().contains(request.method)) {
        *error = QStringLiteral("%1 is not a method an HTTP request can have here").arg(request.method);
        return {};
    }
    const QString path = action.value("path").toString().trimmed();
    const QUrl whole(path);
    if (whole.isValid() && (whole.scheme() == QLatin1String("http") || whole.scheme() == QLatin1String("https"))) {
        request.url = whole;
    } else {
        QString base = connection.baseUrl.trimmed();
        while (base.endsWith(u'/'))
            base.chop(1);
        QString rest = path;
        while (rest.startsWith(u'/'))
            rest.remove(0, 1);
        request.url = QUrl(rest.isEmpty() ? base : base + u'/' + rest);
    }
    if (!request.url.isValid() || request.url.host().isEmpty()) {
        *error = QStringLiteral("%1 is not an address a request can be sent to").arg(request.url.toString());
        return {};
    }
    // The connection's headers, then the action's in place of any of the same name
    for (const QStringList &lines : {connection.headers, action.value("headers").toStringList()}) {
        for (const QString &line : lines) {
            const QPair<QByteArray, QByteArray> header = headerOf(line);
            if (header.first.isEmpty())
                continue;
            request.headers.erase(std::remove_if(request.headers.begin(), request.headers.end(), [&header](const auto &had) {
                return had.first.compare(header.first, Qt::CaseInsensitive) == 0;
            }), request.headers.end());
            request.headers.append(header);
        }
    }
    request.body = action.value("body").toString().toUtf8();
    // A body without a Content-Type of its own is said to be JSON if it looks like it,
    // and plain words otherwise (a GUESS at what such requests want most often: the
    // web services a church's production is likely to be told something by take JSON)
    const bool typed = std::any_of(request.headers.cbegin(), request.headers.cend(), [](const auto &header) {
        return header.first.compare("Content-Type", Qt::CaseInsensitive) == 0;
    });
    if (!request.body.isEmpty() && !typed) {
        const QByteArray trimmed = request.body.trimmed();
        const QByteArray type = trimmed.startsWith('{') || trimmed.startsWith('[') ? QByteArray("application/json")
                                                                                   : QByteArray("text/plain; charset=utf-8");
        request.headers.append(qMakePair(QByteArray("Content-Type"), type));
    }
    return request;
}

// ---- The actions

const QStringList &httpMethods()
{
    static const QStringList methods {QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"), QStringLiteral("DELETE")};
    return methods;
}

QVariantMap describe(const rv::data::Action &action)
{
    const Communication &communication = action.communication();
    const QString deviceName = text(communication.device_identification().parameter_name());
    QVariantMap map {
        {"kind", QStringLiteral("communication")},
        {"deviceId", text(communication.device_identification().parameter_uuid().string())},
        {"deviceName", deviceName},
        {"done", true},
    };
    const QString through = QStringLiteral(", to ") + quoted(deviceName);
    if (communication.has_midi_command()) {
        const Communication::MIDICommand &midi = communication.midi_command();
        const bool on = midi.state() == Communication::MIDICommand::STATE_ON;
        map.insert("command", QStringLiteral("midi"));
        map.insert("on", on);
        map.insert("channel", int(midi.channel()));
        map.insert("note", int(midi.note()));
        map.insert("intensity", int(midi.intensity()));
        map.insert("title", QStringLiteral("Send MIDI note %1 %2 (%3), velocity %4, channel %5%6")
                                .arg(on ? QStringLiteral("on") : QStringLiteral("off")).arg(midi.note()).arg(noteName(int(midi.note())))
                                .arg(midi.intensity()).arg(std::max(1u, midi.channel())).arg(through));
    } else if (isHttp(communication)) {
        const auto value = [&communication](const char *name) {
            const Communication::Command *command = commandNamed(communication, QLatin1String(name));
            return command ? text(command->value()) : QString();
        };
        QStringList headers;
        for (const Communication::Command &command : communication.commands()) {
            if (text(command.name()) == QLatin1String("Header"))
                headers.append(text(command.value()));
        }
        map.insert("command", QStringLiteral("http"));
        map.insert("method", value("Method"));
        map.insert("path", value("Path"));
        map.insert("body", value("Body"));
        map.insert("headers", headers);
        map.insert("title", QStringLiteral("Send an HTTP %1 to %2%3").arg(value("Method"), value("Path").isEmpty() ? QStringLiteral("/") : value("Path"), through));
    } else if (communication.has_global_cache_command() || communication.has_gvg100_command() || communication.has_sony_bvs_command()) {
        const QString name = text(action.name());
        map.insert("command", QStringLiteral("other"));
        map.insert("done", false);
        map.insert("title", (name.isEmpty() ? QStringLiteral("A command") : name) + through);
    } else {
        // A command of words, which a RossTalk device is sent as they are: the Custom
        // Control, or another of RossTalk's that ProPresenter made
        const Communication::Command *control = commandNamed(communication, customControlName);
        const QString name = text(action.name());
        map.insert("command", QStringLiteral("words"));
        map.insert("format", text(communication.format()));
        map.insert("description", text(communication.description()));
        map.insert("words", commandText(communication));
        map.insert("customControl", control != nullptr && text(communication.format()) == customControlFormat);
        if (control)
            map.insert("value", text(control->value()));
        map.insert("title", (name.isEmpty() ? QStringLiteral("Send") : name) + QStringLiteral(" ") + quoted(commandText(communication)) + through);
    }
    return map;
}

QString build(const QVariantMap &wanted, rv::data::Action *action)
{
    const QString command = wanted.value("command").toString();
    const QString deviceName = wanted.value("deviceName").toString();
    if (deviceName.isEmpty())
        return QStringLiteral("A communication action needs a device");
    if (action->uuid().string().empty())
        action->mutable_uuid()->set_string(QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString());
    action->set_isenabled(true);
    action->set_type(rv::data::Action::ACTION_TYPE_COMMUNICATION);
    Communication *communication = action->mutable_communication();
    rv::data::CollectionElementType *device = communication->mutable_device_identification();
    const QString deviceId = wanted.value("deviceId").toString();
    if (deviceId.isEmpty())
        device->clear_parameter_uuid();
    else
        device->mutable_parameter_uuid()->set_string(deviceId.toStdString());
    device->set_parameter_name(deviceName.toStdString());
    if (command == QLatin1String("midi")) {
        const int channel = wanted.value("channel").toInt();
        const int note = wanted.value("note").toInt();
        const int intensity = wanted.value("intensity").toInt();
        if (channel < 1 || channel > 16 || note < 0 || note > 127 || intensity < 0 || intensity > 127)
            return QStringLiteral("A MIDI note is on channel 1 to 16, with a note and a velocity of 0 to 127");
        const bool on = wanted.value("on", true).toBool();
        action->set_name((on ? noteOnName : noteOffName).toStdString());
        communication->clear_format();
        communication->clear_description();
        communication->clear_commands();
        Communication::MIDICommand *midi = communication->mutable_midi_command();
        midi->set_state(on ? Communication::MIDICommand::STATE_ON : Communication::MIDICommand::STATE_OFF);
        midi->set_channel(uint32_t(channel));
        midi->set_note(uint32_t(note));
        midi->set_intensity(uint32_t(intensity));
    } else if (command == QLatin1String("customControl")) {
        const QString value = wanted.value("value").toString().trimmed();
        if (value.isEmpty())
            return QStringLiteral("A Custom Control needs to say which");
        // As ProPresenter writes one: the value takes the place of the format's "x"
        action->set_name(customControlName.toStdString());
        communication->clear_midi_command();
        communication->set_format(customControlFormat.toStdString());
        communication->set_description(customControlDescription.toStdString());
        communication->clear_commands();
        Communication::Command *made = addCommand(communication, customControlName, value);
        made->mutable_replacement_range()->set_start(int(customControlFormat.indexOf(u'x')));
        made->mutable_replacement_range()->set_end(int(customControlFormat.indexOf(u'x')) + 1);
    } else if (command == QLatin1String("http")) {
        const QString method = wanted.value("method").toString().toUpper();
        if (!httpMethods().contains(method))
            return QStringLiteral("An HTTP request is a GET, a POST, a PUT or a DELETE");
        const QStringList headers = wanted.value("headers").toStringList();
        for (const QString &header : headers) {
            if (headerOf(header).first.isEmpty())
                return QStringLiteral("A header is written Name: value");
        }
        action->set_name(httpName.toStdString());
        communication->clear_midi_command();
        communication->set_format(httpFormat.toStdString());
        communication->set_description(httpDescription.toStdString());
        communication->clear_commands();
        Communication::Command *made = addCommand(communication, QStringLiteral("Method"), method);
        for (const QString &possible : httpMethods())
            made->add_possible_values(possible.toStdString());
        addCommand(communication, QStringLiteral("Path"), wanted.value("path").toString().trimmed());
        // (Only what there is: no body, and no headers, for a request that has none.)
        const QString body = wanted.value("body").toString();
        if (!body.isEmpty())
            addCommand(communication, QStringLiteral("Body"), body);
        for (const QString &header : headers)
            addCommand(communication, QStringLiteral("Header"), header.trimmed());
    } else {
        return QStringLiteral("That kind of command cannot be made here");
    }
    return {};
}

// ---- The MIDI Map

const QList<MidiCommand> &midiCommands()
{
    static const QString clear = QStringLiteral("Clear Commands");
    static const QString video = QStringLiteral("Video Controls");
    static const QString presentation = QStringLiteral("Presentation Actions");
    static const QString index = QStringLiteral("Select by Index");
    static const QList<MidiCommand> commands {
        {QStringLiteral("clearAll"), QStringLiteral("All"), clear, 0, false},
        {QStringLiteral("clearSlide"), QStringLiteral("Slide"), clear, 1, false},
        {QStringLiteral("clearBackground"), QStringLiteral("Background"), clear, 2, false},
        {QStringLiteral("clearAnnouncements"), QStringLiteral("Announcements"), clear, 30, false},
        {QStringLiteral("clearProps"), QStringLiteral("Props"), clear, 3, false},
        {QStringLiteral("clearMessages"), QStringLiteral("Messages"), clear, 28, false},
        {QStringLiteral("clearAudio"), QStringLiteral("Audio"), clear, 4, false},
        {QStringLiteral("bailToLogo"), QStringLiteral("Bail to Logo"), clear, 5, false},
        {QStringLiteral("goToBeginning"), QStringLiteral("Go to Beginning"), video, 6, false},
        {QStringLiteral("playPause"), QStringLiteral("Play/Pause"), video, 7, false},
        {QStringLiteral("play"), QStringLiteral("Play"), video, 8, false},
        {QStringLiteral("pause"), QStringLiteral("Pause"), video, 9, false},
        {QStringLiteral("nextPlaylistItem"), QStringLiteral("Next Playlist Item"), presentation, 10, false},
        {QStringLiteral("previousPlaylistItem"), QStringLiteral("Prev Playlist Item"), presentation, 11, false},
        {QStringLiteral("nextSlide"), QStringLiteral("Next Slide"), presentation, 12, false},
        {QStringLiteral("previousSlide"), QStringLiteral("Previous Slide"), presentation, 13, false},
        {QStringLiteral("startTimeline"), QStringLiteral("Start Timeline"), presentation, 14, false},
        {QStringLiteral("stopTimeline"), QStringLiteral("Stop Timeline"), presentation, 15, false},
        {QStringLiteral("rewindTimeline"), QStringLiteral("Rewind Timeline"), presentation, 16, false},
        {QStringLiteral("selectPlaylist"), QStringLiteral("Select Playlist"), index, 17, true},
        {QStringLiteral("selectPlaylistItem"), QStringLiteral("Select Playlist Item"), index, 18, true},
        {QStringLiteral("triggerSlide"), QStringLiteral("Trigger Slide"), index, 19, true},
        {QStringLiteral("selectMediaPlaylist"), QStringLiteral("Select Media Playlist"), index, 20, true},
        {QStringLiteral("triggerMedia"), QStringLiteral("Trigger Media"), index, 21, true},
        {QStringLiteral("selectAudioPlaylist"), QStringLiteral("Select Audio Playlist"), index, 22, true},
        {QStringLiteral("triggerAudio"), QStringLiteral("Trigger Audio"), index, 23, true},
        {QStringLiteral("selectPropCollection"), QStringLiteral("Select Prop Collection"), index, 32, true},
        {QStringLiteral("toggleProp"), QStringLiteral("Toggle Prop On/Off"), index, 24, true},
        {QStringLiteral("selectMacroCollection"), QStringLiteral("Select Macro Collection"), index, 31, true},
        {QStringLiteral("triggerMacro"), QStringLiteral("Trigger Macro"), index, 29, true},
        {QStringLiteral("startTimer"), QStringLiteral("Start Timer"), index, 25, true},
        {QStringLiteral("stopTimer"), QStringLiteral("Stop Timer"), index, 26, true},
        {QStringLiteral("resetTimer"), QStringLiteral("Reset Timer"), index, 27, true},
    };
    return commands;
}

QMap<QString, int> midiMap(const QVariantMap &stored)
{
    QMap<QString, int> map;
    for (const MidiCommand &command : midiCommands()) {
        bool number = false;
        const int note = stored.value(command.key).toInt(&number);
        map.insert(command.key, number && note >= -1 && note <= 127 ? note : command.note);
    }
    return map;
}

QString commandForNote(const QMap<QString, int> &map, int note)
{
    if (note < 0 || note > 127)
        return {};
    for (const MidiCommand &command : midiCommands()) {
        if (map.value(command.key, -1) == note)
            return command.key;
    }
    return {};
}

QMap<QString, int> autoFilled(int first)
{
    // ProPresenter's own notes are 0 to 32, one for each command, in the order Auto
    // Fill goes in (a GUESS: that it fills them in that order, from the note typed)
    QMap<QString, int> map;
    for (const MidiCommand &command : midiCommands()) {
        const int note = first + command.note;
        map.insert(command.key, note >= 0 && note <= 127 ? note : -1);
    }
    return map;
}

QString noteName(int note)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (note < 0 || note > 127)
        return {};
    return QString::fromLatin1(names[note % 12]) + QString::number(note / 12 - 2);
}

QVariantMap readMidiMap(const QString &file)
{
    return readFile(file).value("midiMap").toObject().toVariantMap();
}

QString writeMidiMap(const QString &file, const QMap<QString, int> &map)
{
    QJsonObject whole = readFile(file);
    QJsonObject kept;
    for (const MidiCommand &command : midiCommands()) {
        const int note = map.value(command.key, command.note);
        if (note != command.note)
            kept.insert(command.key, note);
    }
    if (kept.isEmpty())
        whole.remove("midiMap");
    else
        whole.insert("midiMap", kept);
    return writeFile(file, whole);
}

} // namespace communications
