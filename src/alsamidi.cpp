#include "alsamidi.h"

#include <QSocketNotifier>
#include <QTimer>

#include <alsa/asoundlib.h>
#include <poll.h>

AlsaMidi::AlsaMidi(QObject *parent)
    : QObject(parent)
{
}

AlsaMidi::~AlsaMidi()
{
    qDeleteAll(m_notifiers);
    if (m_seq)
        snd_seq_close(m_seq);
}

bool AlsaMidi::open(QString *error)
{
    if (m_seq)
        return true;
    snd_seq_t *seq = nullptr;
    const int opened = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
    if (opened < 0) {
        *error = QStringLiteral("MIDI cannot be used on this computer: %1").arg(QString::fromLocal8Bit(snd_strerror(opened)));
        return false;
    }
    snd_seq_set_client_name(seq, "SimplePresenter");
    const int in = snd_seq_create_simple_port(seq, "SimplePresenter in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                              SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    const int out = snd_seq_create_simple_port(seq, "SimplePresenter out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                               SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (in < 0 || out < 0) {
        *error = QStringLiteral("MIDI cannot be used on this computer: %1").arg(QString::fromLocal8Bit(snd_strerror(in < 0 ? in : out)));
        snd_seq_close(seq);
        return false;
    }
    m_seq = seq;
    m_client = snd_seq_client_id(seq);
    m_in = in;
    m_out = out;
    // The sequencer's announcements of ports and clients that come and go
    snd_seq_connect_from(m_seq, m_in, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    const int count = snd_seq_poll_descriptors_count(m_seq, POLLIN);
    QList<pollfd> fds(count);
    snd_seq_poll_descriptors(m_seq, fds.data(), unsigned(count), POLLIN);
    for (const pollfd &fd : std::as_const(fds)) {
        auto *notifier = new QSocketNotifier(fd.fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, &AlsaMidi::readEvents);
        m_notifiers.append(notifier);
    }
    return true;
}

QList<AlsaMidi::Port> AlsaMidi::ports() const
{
    QList<Port> found;
    if (!m_seq)
        return found;
    snd_seq_client_info_t *client;
    snd_seq_port_info_t *port;
    snd_seq_client_info_alloca(&client);
    snd_seq_port_info_alloca(&port);
    snd_seq_client_info_set_client(client, -1);
    while (snd_seq_query_next_client(m_seq, client) >= 0) {
        const int id = snd_seq_client_info_get_client(client);
        // Not this app's own ports, nor the sequencer's (its timer and announcements)
        if (id == m_client || id == SND_SEQ_CLIENT_SYSTEM)
            continue;
        snd_seq_port_info_set_client(port, id);
        snd_seq_port_info_set_port(port, -1);
        while (snd_seq_query_next_port(m_seq, port) >= 0) {
            const unsigned caps = snd_seq_port_info_get_capability(port);
            if (caps & SND_SEQ_PORT_CAP_NO_EXPORT)
                continue;
            Port made;
            made.client = id;
            made.port = snd_seq_port_info_get_port(port);
            made.clientName = QString::fromLocal8Bit(snd_seq_client_info_get_name(client));
            made.portName = QString::fromLocal8Bit(snd_seq_port_info_get_name(port));
            made.source = (caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_SUBS_READ);
            made.destination = (caps & SND_SEQ_PORT_CAP_WRITE) && (caps & SND_SEQ_PORT_CAP_SUBS_WRITE);
            if (made.source || made.destination)
                found.append(made);
        }
    }
    return found;
}

QString AlsaMidi::nameOf(const Port &port)
{
    return port.portName;
}

const AlsaMidi::Port *AlsaMidi::find(const QList<Port> &ports, const QString &name, bool source) const
{
    const auto matching = [&](auto test) -> const Port * {
        for (const Port &port : ports) {
            if ((source ? port.source : port.destination) && test(port))
                return &port;
        }
        return nullptr;
    };
    if (const Port *port = matching([&](const Port &p) { return p.clientName + u':' + p.portName == name; }))
        return port;
    if (const Port *port = matching([&](const Port &p) { return p.portName == name; }))
        return port;
    return matching([&](const Port &p) { return p.clientName == name; });
}

QStringList AlsaMidi::listenTo(const QStringList &names)
{
    QStringList missing;
    if (!m_seq)
        return names;
    const QList<Port> all = ports();
    QSet<QPair<int, int>> wanted;
    for (const QString &name : names) {
        if (const Port *port = find(all, name, true))
            wanted.insert({port->client, port->port});
        else
            missing.append(name);
    }
    for (const QPair<int, int> &had : std::as_const(m_listening)) {
        if (!wanted.contains(had))
            snd_seq_disconnect_from(m_seq, m_in, had.first, had.second);
    }
    for (const QPair<int, int> &now : std::as_const(wanted)) {
        if (!m_listening.contains(now))
            snd_seq_connect_from(m_seq, m_in, now.first, now.second);
    }
    m_listening = wanted;
    return missing;
}

QStringList AlsaMidi::send(const QStringList &names, bool on, int channel, int note, int velocity)
{
    QStringList missing;
    if (!m_seq)
        return names;
    const QList<Port> all = ports();
    for (const QString &name : names) {
        const Port *port = find(all, name, false);
        if (!port) {
            missing.append(name);
            continue;
        }
        snd_seq_event_t event;
        snd_seq_ev_clear(&event);
        snd_seq_ev_set_source(&event, m_out);
        snd_seq_ev_set_dest(&event, port->client, port->port);
        snd_seq_ev_set_direct(&event);
        const unsigned char lane = static_cast<unsigned char>(std::clamp(channel, 1, 16) - 1);
        if (on)
            snd_seq_ev_set_noteon(&event, lane, std::clamp(note, 0, 127), std::clamp(velocity, 0, 127));
        else
            snd_seq_ev_set_noteoff(&event, lane, std::clamp(note, 0, 127), std::clamp(velocity, 0, 127));
        if (snd_seq_event_output_direct(m_seq, &event) < 0)
            missing.append(name);
    }
    return missing;
}

void AlsaMidi::readEvents()
{
    if (!m_seq)
        return;
    bool changed = false;
    snd_seq_event_t *event = nullptr;
    // (Non-blocking: it says -EAGAIN when there is nothing more.)
    while (snd_seq_event_input(m_seq, &event) >= 0 && event) {
        switch (event->type) {
        case SND_SEQ_EVENT_NOTEON:
        case SND_SEQ_EVENT_NOTEOFF: {
            QString source;
            for (const Port &port : ports()) {
                if (port.client == event->source.client && port.port == event->source.port) {
                    source = nameOf(port);
                    break;
                }
            }
            const bool on = event->type == SND_SEQ_EVENT_NOTEON && event->data.note.velocity > 0;
            emit noteHeard(source, event->data.note.channel + 1, event->data.note.note, event->data.note.velocity, on);
            break;
        }
        case SND_SEQ_EVENT_CLIENT_START:
        case SND_SEQ_EVENT_CLIENT_EXIT:
        case SND_SEQ_EVENT_CLIENT_CHANGE:
        case SND_SEQ_EVENT_PORT_START:
        case SND_SEQ_EVENT_PORT_EXIT:
        case SND_SEQ_EVENT_PORT_CHANGE:
            changed = true;
            break;
        default:
            break;
        }
    }
    // A port that has gone is not heard from any more: forgotten, so that it is
    // listened to again if it comes back
    if (changed) {
        const QList<Port> all = ports();
        QSet<QPair<int, int>> still;
        for (const Port &port : all) {
            if (m_listening.contains({port.client, port.port}))
                still.insert({port.client, port.port});
        }
        m_listening = still;
        emit portsChanged();
    }
}
