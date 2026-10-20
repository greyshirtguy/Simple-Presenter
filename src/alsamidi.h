#pragma once

#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QSocketNotifier;
typedef struct _snd_seq snd_seq_t;

// MIDI through ALSA's sequencer, which is how a Linux desktop has MIDI: every MIDI port
// there is, a USB keyboard's, a program's (a DAW, a show controller), and those that
// network MIDI makes (rtpmidid), is a port of a client of the sequencer, known by the
// client's name and its own. This app is one more client, "SimplePresenter", with a port
// it hears on and one it sends from.
//
// A port is asked for by a name, as a device of ProPresenter's names its sources and
// destinations (on a Mac, CoreMIDI's names): the name is matched against each port's
// "client:port", then its own name, then its client's, the first that matches being the
// port. Notes heard are said with the name of the port they came from, so that whatever
// listens can tell whose they are.
//
// Everything is on the thread it is made on: the sequencer is read when its file says it
// has something, which is a handful of bytes a note, never a file of the disk's. The
// sequencer's own announcements are listened to, so that a port that comes or goes is
// known at once (portsChanged), which is what has a connection reconnect.
class AlsaMidi : public QObject
{
    Q_OBJECT

public:
    struct Port
    {
        int client = -1;
        int port = -1;
        QString clientName;
        QString portName;
        // Whether notes can be heard from it, and sent to it
        bool source = false;
        bool destination = false;
    };

    explicit AlsaMidi(QObject *parent = nullptr);
    ~AlsaMidi() override;

    // Opens this app's client, if it is not open: false, and `error` says why, if the
    // sequencer cannot be had (no /dev/snd/seq, or no leave to use it)
    bool open(QString *error);
    bool isOpen() const { return m_seq != nullptr; }

    // Every other client's ports, as they are now
    QList<Port> ports() const;
    // The names to show for them: a port's own name, and its client's where it needs it
    // to be told apart ("Midi Through Port-0")
    static QString nameOf(const Port &port);

    // Hears notes from these ports, and from no others: says which names were not found
    QStringList listenTo(const QStringList &names);
    // Sends a note on or off to these ports: says which names were not found
    QStringList send(const QStringList &names, bool on, int channel, int note, int velocity);

signals:
    // A note heard: the name of the port it came from (as nameOf has it), its channel
    // (1 to 16), note and velocity, and whether it is a note on (a note on with no
    // velocity is a note off, as MIDI has it)
    void noteHeard(const QString &source, int channel, int note, int velocity, bool on);
    // A port has come or gone, or a client
    void portsChanged();

private:
    void readEvents();
    const Port *find(const QList<Port> &ports, const QString &name, bool source) const;

    snd_seq_t *m_seq = nullptr;
    int m_client = -1;
    int m_in = -1;
    int m_out = -1;
    QList<QSocketNotifier *> m_notifiers;
    // The ports heard from now, as client and port
    QSet<QPair<int, int>> m_listening;
};
