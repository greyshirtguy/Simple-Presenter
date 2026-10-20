// Communication devices (src/communications.h): ProPresenter's list of devices read as
// connections, from a file laid out as ProPresenter wrote one; this app's own file of
// connections, a list for each workspace, and the MIDI Map beside them; what is sent
// (a command of words from its format, a MIDI note's bytes, an HTTP request); and the
// actions, a MIDI note and a Custom Control made exactly as ProPresenter makes them and
// an HTTP request in the same general form, each read back, changed, and with what this
// app does not know of them kept.

#include "communications.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <google/protobuf/unknown_field_set.h>

namespace {

using Communication = rv::data::Action::CommunicationType;

// A device as ProPresenter keeps it: its parser is XML, in UTF-16 with a byte-order mark,
// in base 64
QJsonObject proDevice(const QString &name, const QString &id, bool reconnect, const QString &xml)
{
    QByteArray utf16("\xff\xfe", 2);
    for (const QChar c : xml) {
        utf16.append(char(c.unicode() & 0xff));
        utf16.append(char(c.unicode() >> 8));
    }
    return {{"name", name}, {"reconnect", reconnect}, {"was_connected", false}, {"id", id}, {"parser", QString::fromLatin1(utf16.toBase64())}};
}

QByteArray proFile()
{
    const QString midi = QStringLiteral(
        "<RVProtocolParserMIDI bundleName=\"ProPresenter 21.4.app\" bundleIdentifier=\"com.renewedvision.propresenter\" behavior=\"2\">"
        "<RVMIDIHardwareCommunicator sources=\"Bus One\" destinations=\"Bus One|Bus Two\"></RVMIDIHardwareCommunicator></RVProtocolParserMIDI>");
    const QString ross = QStringLiteral(
        "<RVProtocolParserRossTalk bundleName=\"ProPresenter 21.4.app\" bundleIdentifier=\"com.renewedvision.propresenter\" behavior=\"2\">"
        "<RVTCPNetworkHardwareCommunicator interface=\"\" port=\"7788\" address=\"10.0.0.5\" mode=\"1\"></RVTCPNetworkHardwareCommunicator>"
        "</RVProtocolParserRossTalk>");
    const QString dmx = QStringLiteral("<RVProtocolParserDMX behavior=\"2\"></RVProtocolParserDMX>");
    QJsonArray list {proDevice("MIDI", "0C9FC689-168D-4B33-A8F2-FAC5CF89A30A", true, midi),
                     proDevice("Switcher", "3224F612-2D35-4821-9066-A3627C596F21", false, ross),
                     proDevice("Lights", "11111111-2222-3333-4444-555555555555", false, dmx)};
    return QJsonDocument(list).toJson();
}

// A MIDI Note On as ProPresenter writes one (seen in a presentation of its own)
rv::data::Action proNoteOn()
{
    rv::data::Action action;
    action.mutable_uuid()->set_string("2C9A9B91-1FA8-407E-901E-92EAAA499FD1");
    action.set_name("MIDI Note On");
    action.set_isenabled(true);
    action.set_type(rv::data::Action::ACTION_TYPE_COMMUNICATION);
    Communication *communication = action.mutable_communication();
    communication->mutable_device_identification()->mutable_parameter_uuid()->set_string("0C9FC689-168D-4B33-A8F2-FAC5CF89A30A");
    communication->mutable_device_identification()->set_parameter_name("MIDI");
    communication->mutable_midi_command()->set_channel(1);
    communication->mutable_midi_command()->set_note(6);
    communication->mutable_midi_command()->set_intensity(2);
    return action;
}

// A RossTalk Custom Control as ProPresenter writes one (seen likewise)
rv::data::Action proCustomControl()
{
    rv::data::Action action;
    action.mutable_uuid()->set_string("6BA29FEA-BA0A-412B-8FFE-42BACF6CC8F1");
    action.set_name("Custom Control");
    action.set_isenabled(true);
    action.set_type(rv::data::Action::ACTION_TYPE_COMMUNICATION);
    Communication *communication = action.mutable_communication();
    communication->mutable_device_identification()->mutable_parameter_uuid()->set_string("3224F612-2D35-4821-9066-A3627C596F21");
    communication->mutable_device_identification()->set_parameter_name("RossTalk");
    communication->set_format("CC x");
    communication->set_description("Executes the specified custom control.");
    Communication::Command *command = communication->add_commands();
    command->set_name("Custom Control");
    command->set_value("1/3/2");
    command->mutable_replacement_range()->set_start(3);
    command->mutable_replacement_range()->set_end(4);
    return action;
}

std::string bytesOf(const google::protobuf::MessageLite &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

communications::Connection http(const QString &base, const QStringList &headers = {})
{
    communications::Connection connection;
    connection.id = "H";
    connection.name = "Webhooks";
    connection.kind = "http";
    connection.baseUrl = base;
    connection.headers = headers;
    return connection;
}

QByteArray headerValue(const communications::Request &request, const QByteArray &name)
{
    for (const auto &header : request.headers) {
        if (header.first.compare(name, Qt::CaseInsensitive) == 0)
            return header.second;
    }
    return {};
}

}

class TestCommunications : public QObject
{
    Q_OBJECT

private slots:
    // ProPresenter's devices: its MIDI and RossTalk ones as connections, with their ids,
    // names, ports and Auto Reconnect, not connected; a kind this app has not, counted
    void readsProPresentersDevices()
    {
        int skipped = -1;
        const QList<communications::Connection> devices = communications::proDevicesFrom(proFile(), &skipped);
        QCOMPARE(skipped, 1);
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.at(0).kind, QStringLiteral("midi"));
        QCOMPARE(devices.at(0).id, QStringLiteral("0C9FC689-168D-4B33-A8F2-FAC5CF89A30A"));
        QCOMPARE(devices.at(0).name, QStringLiteral("MIDI"));
        QCOMPARE(devices.at(0).sources, QStringList({"Bus One"}));
        QCOMPARE(devices.at(0).destinations, QStringList({"Bus One", "Bus Two"}));
        QVERIFY(devices.at(0).autoReconnect);
        QVERIFY(!devices.at(0).connected);
        QCOMPARE(devices.at(1).kind, QStringLiteral("rosstalk"));
        QCOMPARE(devices.at(1).address, QStringLiteral("10.0.0.5"));
        QCOMPARE(devices.at(1).port, 7788);
        QVERIFY(!devices.at(1).autoReconnect);
        // An empty list, as ProPresenter writes one, and what is not JSON: nothing
        QVERIFY(communications::proDevicesFrom("[]").isEmpty());
        QVERIFY(communications::proDevicesFrom("not json").isEmpty());
    }

    // This app's file: a list for each workspace, each kept as it was when another is
    // written, and the MIDI Map beside them kept likewise
    void keepsAListForEachWorkspace()
    {
        QTemporaryDir folder;
        const QString file = folder.filePath("SimplePresenter/Devices.json");
        QVERIFY(communications::read(file, "/w/one").isEmpty());
        QList<communications::Connection> one = communications::proDevicesFrom(proFile());
        one[0].connected = true;
        QCOMPARE(communications::write(file, "/w/one", one), QString());
        QCOMPARE(communications::writeMidiMap(file, communications::autoFilled(40)), QString());
        const QList<communications::Connection> two {http("https://example.org/hooks", {"Authorization: Bearer x"})};
        QCOMPARE(communications::write(file, "/w/two", two), QString());
        QCOMPARE(communications::read(file, "/w/one"), one);
        QCOMPARE(communications::read(file, "/w/two"), two);
        QCOMPARE(communications::midiMap(communications::readMidiMap(file)).value("clearAll"), 40);
        // The map put back as ProPresenter has it leaves nothing of itself in the file
        QCOMPARE(communications::writeMidiMap(file, communications::midiMap({})), QString());
        QVERIFY(communications::readMidiMap(file).isEmpty());
        QCOMPARE(communications::read(file, "/w/two"), two);
    }

    void saysWhatIsWrongWithAConnection()
    {
        communications::Connection connection = communications::proDevicesFrom(proFile()).at(0);
        QCOMPARE(communications::problemWith(connection), QString());
        connection.sources.clear();
        connection.destinations.clear();
        QVERIFY(!communications::problemWith(connection).isEmpty());
        connection = communications::proDevicesFrom(proFile()).at(1);
        connection.address.clear();
        QVERIFY(!communications::problemWith(connection).isEmpty());
        QCOMPARE(communications::problemWith(http("http://192.168.1.20:8080/api")), QString());
        QVERIFY(!communications::problemWith(http("ftp://example.org")).isEmpty());
        QVERIFY(!communications::problemWith(http("https://example.org", {"no colon"})).isEmpty());
        connection.name = "  ";
        QVERIFY(!communications::problemWith(connection).isEmpty());
    }

    // A command of words: the format with each value in place of its range
    void putsValuesInTheirPlaces()
    {
        QCOMPARE(communications::commandText(proCustomControl().communication()), QStringLiteral("CC 1/3/2"));
        Communication two;
        two.set_format("KEY x ON y");
        Communication::Command *first = two.add_commands();
        first->set_value("1");
        first->mutable_replacement_range()->set_start(4);
        first->mutable_replacement_range()->set_end(5);
        Communication::Command *second = two.add_commands();
        second->set_value("ME:2");
        second->mutable_replacement_range()->set_start(9);
        second->mutable_replacement_range()->set_end(10);
        QCOMPARE(communications::commandText(two), QStringLiteral("KEY 1 ON ME:2"));
        // A range past the format's end is left out
        second->mutable_replacement_range()->set_start(40);
        second->mutable_replacement_range()->set_end(41);
        QCOMPARE(communications::commandText(two), QStringLiteral("KEY 1 ON y"));
    }

    void makesMidiMessages()
    {
        QCOMPARE(communications::midiMessage(true, 1, 6, 2), QByteArray("\x90\x06\x02", 3));
        QCOMPARE(communications::midiMessage(false, 16, 127, 64), QByteArray("\x8f\x7f\x40", 3));
        // Channel 0, as a file may have it, is the first
        QCOMPARE(communications::midiMessage(true, 0, 60, 100), QByteArray("\x90\x3c\x64", 3));
    }

    // An HTTP request: the base and the path joined, a path that is a whole address taken
    // as it is, the action's headers over the connection's, a body's type guessed
    void makesHttpRequests()
    {
        QString error;
        communications::Request request = communications::httpRequest(http("https://example.org/api/", {"Authorization: Bearer one", "X-Show: Sunday"}),
                                                                       {{"method", "post"}, {"path", "/v1/clear"}, {"body", "{\"all\": true}"},
                                                                        {"headers", QStringList({"authorization: Bearer two"})}}, &error);
        QCOMPARE(error, QString());
        QCOMPARE(request.method, QStringLiteral("POST"));
        QCOMPARE(request.url, QUrl("https://example.org/api/v1/clear"));
        QCOMPARE(headerValue(request, "Authorization"), QByteArray("Bearer two"));
        QCOMPARE(headerValue(request, "X-Show"), QByteArray("Sunday"));
        QCOMPARE(headerValue(request, "Content-Type"), QByteArray("application/json"));
        QCOMPARE(request.body, QByteArray("{\"all\": true}"));
        request = communications::httpRequest(http("http://10.0.0.9:8000"), {{"method", "PUT"}, {"path", "lights"}, {"body", "on"}}, &error);
        QCOMPARE(request.url, QUrl("http://10.0.0.9:8000/lights"));
        QCOMPARE(headerValue(request, "Content-Type"), QByteArray("text/plain; charset=utf-8"));
        request = communications::httpRequest(http("http://10.0.0.9"), {{"method", "GET"}, {"path", "https://other.example/ping"}}, &error);
        QCOMPARE(request.url, QUrl("https://other.example/ping"));
        QVERIFY(headerValue(request, "Content-Type").isEmpty());
        request = communications::httpRequest(http("http://10.0.0.9"), {{"method", "GET"}, {"path", ""}}, &error);
        QCOMPARE(request.url, QUrl("http://10.0.0.9"));
        communications::httpRequest(http("http://10.0.0.9"), {{"method", "PATCH"}, {"path", "/x"}}, &error);
        QVERIFY(!error.isEmpty());
    }

    // ProPresenter's own MIDI Note On, read; made here, it is the same to the byte; changed
    // to a note off, what else the action had is kept
    void midiActionsAreProPresentersOwn()
    {
        QVariantMap map = communications::describe(proNoteOn());
        QCOMPARE(map.value("kind").toString(), QStringLiteral("communication"));
        QCOMPARE(map.value("command").toString(), QStringLiteral("midi"));
        QCOMPARE(map.value("deviceName").toString(), QStringLiteral("MIDI"));
        QCOMPARE(map.value("on").toBool(), true);
        QCOMPARE(map.value("channel").toInt(), 1);
        QCOMPARE(map.value("note").toInt(), 6);
        QCOMPARE(map.value("intensity").toInt(), 2);
        QVERIFY(map.value("done").toBool());
        QVERIFY(map.value("title").toString().contains("F#-2"));
        rv::data::Action made;
        made.mutable_uuid()->set_string("2C9A9B91-1FA8-407E-901E-92EAAA499FD1");
        QCOMPARE(communications::build(map, &made), QString());
        QCOMPARE(bytesOf(made), bytesOf(proNoteOn()));
        // Something of the action's that this app knows nothing of is kept by a change
        rv::data::Action kept = proNoteOn();
        kept.GetReflection()->MutableUnknownFields(&kept)->AddVarint(900, 3);
        map.insert("on", false);
        map.insert("intensity", 0);
        QCOMPARE(communications::build(map, &kept), QString());
        QCOMPARE(kept.name(), std::string("MIDI Note Off"));
        QCOMPARE(kept.communication().midi_command().state(), Communication::MIDICommand::STATE_OFF);
        QCOMPARE(kept.GetReflection()->GetUnknownFields(kept).field_count(), 1);
        map.insert("channel", 17);
        QVERIFY(!communications::build(map, &kept).isEmpty());
    }

    // ProPresenter's own Custom Control, read and made again to the byte
    void customControlsAreProPresentersOwn()
    {
        const QVariantMap map = communications::describe(proCustomControl());
        QCOMPARE(map.value("command").toString(), QStringLiteral("words"));
        QVERIFY(map.value("customControl").toBool());
        QCOMPARE(map.value("value").toString(), QStringLiteral("1/3/2"));
        QCOMPARE(map.value("words").toString(), QStringLiteral("CC 1/3/2"));
        rv::data::Action made;
        made.mutable_uuid()->set_string("6BA29FEA-BA0A-412B-8FFE-42BACF6CC8F1");
        QVariantMap wanted = map;
        wanted.insert("command", "customControl");
        QCOMPARE(communications::build(wanted, &made), QString());
        QCOMPARE(bytesOf(made), bytesOf(proCustomControl()));
        wanted.insert("value", " ");
        QVERIFY(!communications::build(wanted, &made).isEmpty());
    }

    // An HTTP request in ProPresenter's general form: read back as it was made, with
    // nothing in it that is not asked for, and nothing ProPresenter does not define
    void httpActionsUseProPresentersFields()
    {
        rv::data::Action action;
        QCOMPARE(communications::build({{"command", "http"}, {"deviceId", "H"}, {"deviceName", "Webhooks"}, {"method", "post"},
                                        {"path", "/v1/go"}, {"body", "{}"}, {"headers", QStringList({"X-One: 1", "X-Two: 2"})}}, &action),
                 QString());
        QCOMPARE(action.type(), rv::data::Action::ACTION_TYPE_COMMUNICATION);
        QCOMPARE(action.name(), std::string("HTTP Request"));
        QVERIFY(!action.uuid().string().empty());
        QVERIFY(action.GetReflection()->GetUnknownFields(action).empty());
        const QVariantMap map = communications::describe(action);
        QCOMPARE(map.value("command").toString(), QStringLiteral("http"));
        QCOMPARE(map.value("method").toString(), QStringLiteral("POST"));
        QCOMPARE(map.value("path").toString(), QStringLiteral("/v1/go"));
        QCOMPARE(map.value("body").toString(), QStringLiteral("{}"));
        QCOMPARE(map.value("headers").toStringList(), QStringList({"X-One: 1", "X-Two: 2"}));
        // The method's possible values are ProPresenter's way of saying what it can be
        QCOMPARE(action.communication().commands(0).possible_values_size(), 4);
        // No body and no headers: no lines for them
        QCOMPARE(communications::build({{"command", "http"}, {"deviceName", "Webhooks"}, {"method", "GET"}, {"path", "/ping"}}, &action), QString());
        QCOMPARE(action.communication().commands_size(), 2);
        QVERIFY(!communications::build({{"command", "http"}, {"deviceName", "Webhooks"}, {"method", "GET"}, {"headers", QStringList({"bad"})}},
                                       &action).isEmpty());
        QVERIFY(!communications::build({{"command", "http"}, {"method", "GET"}}, &action).isEmpty());
    }

    // ProPresenter's MIDI Map: its notes, its order, a note to its command, Auto Fill
    void midiMapIsProPresenters()
    {
        const QList<communications::MidiCommand> &commands = communications::midiCommands();
        QCOMPARE(commands.size(), 33);
        QSet<int> notes;
        for (const communications::MidiCommand &command : commands)
            notes.insert(command.note);
        QCOMPARE(notes.size(), 33);
        QCOMPARE(*std::max_element(notes.cbegin(), notes.cend()), 32);
        const QMap<QString, int> map = communications::midiMap({{"nextSlide", 60}, {"clearAll", "x"}});
        QCOMPARE(map.value("nextSlide"), 60);
        QCOMPARE(map.value("clearAll"), 0);
        QCOMPARE(map.value("clearAnnouncements"), 30);
        QCOMPARE(communications::commandForNote(map, 60), QStringLiteral("nextSlide"));
        QCOMPARE(communications::commandForNote(map, 12), QString());
        QCOMPARE(communications::commandForNote(map, 30), QStringLiteral("clearAnnouncements"));
        QCOMPARE(communications::commandForNote(map, 128), QString());
        const QMap<QString, int> filled = communications::autoFilled(100);
        QCOMPARE(filled.value("clearAll"), 100);
        QCOMPARE(filled.value("nextSlide"), 112);
        QCOMPARE(filled.value("selectPropCollection"), -1);
        QCOMPARE(communications::noteName(0), QStringLiteral("C-2"));
        QCOMPARE(communications::noteName(30), QStringLiteral("F#0"));
        QCOMPARE(communications::noteName(60), QStringLiteral("C3"));
        QCOMPARE(communications::noteName(127), QStringLiteral("G8"));
    }
};

QTEST_APPLESS_MAIN(TestCommunications)
#include "tst_communications.moc"
