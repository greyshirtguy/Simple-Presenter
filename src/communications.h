#pragma once

#include "action.pb.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// Communication devices: things outside the app that a slide or a macro tells something
// (ProPresenter's Devices), and what one says back. Three kinds:
//
//   - MIDI, as ProPresenter has it: notes sent to the ports a connection names as its
//     destinations, and notes heard from its sources, which work the show by the MIDI
//     Map (see MidiMap below).
//   - RossTalk, as ProPresenter has it, to a Ross switcher over the network: for now its
//     Custom Control alone (the maintainer's choice, 2026-10-20).
//   - HTTP Requests: this app's own, which ProPresenter does not have. A connection is a
//     named base address; an action sends a GET, POST, PUT or DELETE to a path under it,
//     with a body and headers if it has them.
//
// Plain rules, with nothing of the network or of MIDI in them (tests/unit/
// tst_communications.cpp); Devices (devices.h) does the talking.
//
// Where things are kept, and why:
//
//   - The connections are this app's, in a file of its own in its settings folder
//     (Devices.json), a list for each workspace, as ProPresenter keeps a list for each.
//     ProPresenter's list is in the workspace's Configuration/CommunicationDevices, as
//     JSON whose every device has a `parser`: XML, in UTF-16, in base 64, naming
//     ProPresenter's own classes and its app bundle (seen in a file of the maintainer's,
//     2026-10-20). That file is read, never written: its devices are offered as
//     connections here with the same ids and names, so that ProPresenter's own actions,
//     which name a device by its id and its name, find them.
//   - The actions are ProPresenter's: a communication action, which names its device
//     and says what to send. A MIDI note is written as ProPresenter writes one
//     (`midi_command`); a Custom Control as ProPresenter writes one, in the general form
//     its communication actions have for commands of words: a `format` ("CC x") and
//     named values each with the part of the format it takes the place of. An HTTP
//     request is written in that same general form, with nothing ProPresenter does not
//     define: to ProPresenter it is a communication action for a device it does not
//     have, as one whose device has been deleted is (the maintainer's decision,
//     2026-10-20, over a field or a kind of action of this app's own, which
//     ProPresenter's handling of could not be known).
namespace communications {

// ---- Connections

struct Connection
{
    QString id;
    QString name;
    // "midi", "rosstalk" or "http"
    QString kind;
    // Whether it is to be connected (its Connect button), which it is again when the
    // app next opens the workspace; and whether a connection that drops, or a port that
    // goes, is tried again until it is back (ProPresenter's Auto Reconnect). Not for an
    // HTTP connection, which has nothing to keep connected.
    bool connected = false;
    bool autoReconnect = false;
    // MIDI: the ports, by their names, that notes are heard from and sent to
    QStringList sources;
    QStringList destinations;
    // RossTalk: the switcher's address and port (7788 is RossTalk's)
    QString address;
    int port = 7788;
    // HTTP: the address every request's path goes under, and headers every request
    // has (an Authorization, say), "Name: value" each
    QString baseUrl;
    QStringList headers;

    bool operator==(const Connection &other) const = default;
};

QVariantMap toMap(const Connection &connection);
Connection fromMap(const QVariantMap &map);

// What is wrong with a connection as it would be saved, "" for nothing: a name, and
// what its kind needs (a port to send to or hear from, an address, a base address
// that is http or https).
QString problemWith(const Connection &connection);

// This app's file: { "workspaces": { <workspace folder>: [connection, ...] },
// "midiMap": { <command>: note } }. A file that is not there, or cannot be read, has
// nothing in it. Writing keeps every other workspace's list, and the map, as they were.
QList<Connection> read(const QString &file, const QString &workspace);
QString write(const QString &file, const QString &workspace, const QList<Connection> &connections);

// ProPresenter's devices for a workspace (Configuration/CommunicationDevices), as
// connections: its MIDI and RossTalk devices, with their ids, names, ports (a MIDI
// device's sources and destinations, a RossTalk one's port and address, where the file
// has them) and whether they reconnect. Not connected: that is this computer's to say.
// `skipped` is told how many of its devices are of kinds this app does not have.
QList<Connection> proDevices(const QString &workspace, int *skipped = nullptr);
// The same from the file's contents, for the tests
QList<Connection> proDevicesFrom(const QByteArray &json, int *skipped = nullptr);

// ---- What is sent

// A command of words, as a communication action of ProPresenter's has one: its
// `format`, with each named value put in place of the part of it its replacement range
// says ("CC x", the value "1/3/2" for 3 to 4: "CC 1/3/2"). A value whose range is not
// within the format is left out.
QString commandText(const rv::data::Action::CommunicationType &communication);

// A MIDI note on or off, as the three bytes it is sent as: channel 1 to 16 (as
// ProPresenter's files count them; 0 is taken for 1), note and velocity 0 to 127.
QByteArray midiMessage(bool on, int channel, int note, int velocity);

// An HTTP request: what an action of the kind says, under its connection's base
// address. The URL is the base and the path joined with one "/" between; a path that
// is a whole address of its own is taken as it is. The headers are the connection's,
// then the action's, which win for a name both have.
struct Request
{
    QString method;
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
};
Request httpRequest(const Connection &connection, const QVariantMap &action, QString *error);

// ---- The actions

// What a communication action is, for actions::describe: kind "communication", with
// deviceId and deviceName, and `command`: "midi" (with on, channel, note, intensity),
// "customControl" (with value, and format and description as the file has them),
// "http" (with method, path, body, headers), or "other" for a command of a device this
// app does not have (Global Cache, GVG 100, Sony BVS, or words of another device's).
// `title` says what it sends; `done` whether this app sends it.
QVariantMap describe(const rv::data::Action &action);

// Makes an action the communication action the map says (as describe() gives one, of
// the first three commands), as ProPresenter makes one: a new one, or one that is
// there, whose id and whatever else it holds are kept. Returns an error message, empty
// on success.
QString build(const QVariantMap &wanted, rv::data::Action *action);

// The methods an HTTP request can have, in the order the menu has them
const QStringList &httpMethods();

// ---- The MIDI Map

// ProPresenter's MIDI Map: a note for each of 33 commands, which a note heard from a
// connection's sources sets going, on any channel and with any velocity but none (a
// note on with no velocity is a note off). For those "by index", the velocity, 1 to
// 127, is which one: the 4th playlist, the 2nd prop. As ProPresenter's window has
// them, in its order, under its headings (seen in a picture of it, 2026-10-20).
struct MidiCommand
{
    QString key;
    QString label;
    QString group;
    int note = 0;
    bool byIndex = false;
};
const QList<MidiCommand> &midiCommands();

// The notes as they are set: every command's, the map's where it has one and
// ProPresenter's own otherwise
QMap<QString, int> midiMap(const QVariantMap &stored);
// The command a note sets going, "" for none (where two have the same note, the first
// of ProPresenter's order)
QString commandForNote(const QMap<QString, int> &map, int note);
// The notes made one after another from `first`, in the order Auto Fill gives them
// (ProPresenter's own order of its defaults, 0 to 32); notes past 127 are not given
QMap<QString, int> autoFilled(int first);
// A note's name as ProPresenter shows it: 0 is C-2, 60 is C3
QString noteName(int note);

// The map as this app's file keeps it (only the notes that are not ProPresenter's own),
// and kept there
QVariantMap readMidiMap(const QString &file);
QString writeMidiMap(const QString &file, const QMap<QString, int> &map);

} // namespace communications
