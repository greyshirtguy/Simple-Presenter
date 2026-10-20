#pragma once

#include "alsamidi.h"
#include "communications.h"

#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QTcpSocket;
class QTimer;

// The workspace's communication devices, live: ProPresenter's Devices, as Settings →
// Devices sets them up (communications.h has what they are, how they are kept, and
// what is sent to each). It connects them, keeps them connected where they are to be,
// sends what a slide's or a macro's action says (Show hands it out: see
// show::Effect::Communicate), and hears MIDI notes, which work the show by the MIDI Map
// (midiCommand, which the operator window does: see MidiControl.qml).
//
// What a connection is, as it runs:
//   - MIDI: this app's client of the sequencer (alsamidi.h) hears from the connection's
//     sources and sends to its destinations. It is connected when each port it names is
//     there; one that is not is said, and the connection goes on with the others.
//   - RossTalk: a TCP connection to the switcher, kept open; a command is a line of
//     words, ended with a carriage return and a line feed (a GUESS from RossTalk's
//     published form, which ProPresenter's help does not give).
//   - HTTP: nothing to keep open. A request is sent when it is asked for, and the log is
//     told what came back.
//
// Auto Reconnect, as ProPresenter has it: a connection that drops, or whose port goes,
// is tried again (every few seconds for RossTalk, and for MIDI as soon as the port is
// back), and one that was connected when the workspace was last left is connected
// again when it is next opened. Without it a connection that drops stays down, and
// starts down. (A GUESS at what ProPresenter's Auto Reconnect covers, from its help:
// "so ProPresenter reconnects to MIDI devices automatically".)
//
// Nothing is kept where `remember` is off (the self-test, the benchmark): the
// connections are then ProPresenter's devices, unconnected, and the MIDI Map its own.
class Devices : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The connections, each as communications::toMap has it, with `state` ("connected",
    // "connecting", "off", or "ready" for an HTTP one, which has nothing to connect) and
    // `problem` (what is wrong, "" for nothing)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)
    // ProPresenter's devices for the workspace that are not connections here (by id),
    // as connections, to be added
    Q_PROPERTY(QVariantList offered READ offered NOTIFY changed)
    // The MIDI ports there are, by name, that can be heard from and sent to; and why there
    // are none, if MIDI cannot be had
    Q_PROPERTY(QStringList midiSources READ midiSources NOTIFY portsChanged)
    Q_PROPERTY(QStringList midiDestinations READ midiDestinations NOTIFY portsChanged)
    Q_PROPERTY(QString midiProblem READ midiProblem NOTIFY portsChanged)
    // The MIDI Map: [{ key, label, group, note, noteName, byIndex }], in ProPresenter's order
    Q_PROPERTY(QVariantList midiMap READ midiMap NOTIFY midiMapChanged)
    // The last note heard, as words ("C-1 (12), velocity 100, channel 1, from …"), for the
    // map's window to show, as ProPresenter's does in its corner
    Q_PROPERTY(QString lastNote READ lastNote NOTIFY lastNoteChanged)
    Q_PROPERTY(bool remember MEMBER m_remember NOTIFY rememberChanged)

public:
    explicit Devices(QObject *parent = nullptr);
    ~Devices() override;

    QVariantList connections() const;
    QVariantList offered() const;
    QStringList midiSources() const;
    QStringList midiDestinations() const;
    QString midiProblem() const { return m_midiProblem; }
    QVariantList midiMap() const;
    QString lastNote() const { return m_lastNote; }

    // The workspace's connections, as kept for it (or, for one nothing is kept for, its
    // ProPresenter devices, unconnected), and those to be connected connected
    Q_INVOKABLE void open(const QString &workspace);
    // A new connection of a kind ("midi", "rosstalk", "http"), named for its kind (and a
    // number, if that is taken), or a copy of one ProPresenter has (`proId`): { id, error }
    Q_INVOKABLE QVariantMap add(const QString &kind, const QString &proId = QString());
    // Any of a connection's settings, as communications::toMap names them; returns why
    // not, "" for done
    Q_INVOKABLE QString change(const QString &id, const QVariantMap &changes);
    Q_INVOKABLE QString remove(const QString &id);
    // Its Connect button: connected, or not
    Q_INVOKABLE void setConnected(const QString &id, bool on);
    // The connection an action names, by its id, or failing that by its name: as
    // connections() has it, or an empty map for none
    Q_INVOKABLE QVariantMap find(const QString &id, const QString &name) const;
    // Sends what a communication action says (see actions::describe)
    Q_INVOKABLE void send(const QVariantMap &action);

    Q_INVOKABLE void setMidiNote(const QString &key, int note);
    Q_INVOKABLE void autoFillMidiMap(int first);
    Q_INVOKABLE QString noteName(int note) const { return communications::noteName(note); }

signals:
    void changed();
    void portsChanged();
    void midiMapChanged();
    void lastNoteChanged();
    void rememberChanged();
    // A note heard from a connection's sources asks for this command of the MIDI Map,
    // with this index (the note's velocity) for those by index
    void midiCommand(const QString &key, int index);
    // A thing sent and how it went, for the log and the tests: `ok` and what it says
    void sentOut(const QString &device, bool ok, const QString &said);

private:
    struct Live;
    QString file() const;
    void save();
    Live &liveOf(const QString &id);
    void connectOne(const communications::Connection &connection);
    void disconnectOne(const QString &id);
    void connectRossTalk(const communications::Connection &connection);
    void listenToMidi();
    void noteHeard(const QString &source, int channel, int note, int velocity, bool on);
    void tell(const QString &device, bool ok, const QString &said);
    int indexOf(const QString &id) const;

    bool m_remember = true;
    QString m_workspace;
    QList<communications::Connection> m_list;
    QList<communications::Connection> m_pro;
    QHash<QString, Live *> m_live;
    AlsaMidi m_midi;
    QString m_midiProblem;
    QMap<QString, int> m_midiMap;
    QString m_lastNote;
    QNetworkAccessManager *m_network = nullptr;
};
