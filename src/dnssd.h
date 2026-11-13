#pragma once

#include <QByteArrayList>
#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

// Advertising a service on the local network by DNS-SD over multicast DNS (what Apple
// calls Bonjour), so that a phone on the same network finds it by itself: here, so that
// ProPresenter Remote finds the app as it finds ProPresenter (see proremote.h).
//
// How: through Avahi, the daemon that answers multicast DNS for the whole computer on
// Linux, by its D-Bus interface on the system bus (Qt's D-Bus module, which the app has
// already). The app makes an entry group of Avahi's, adds the service to it and commits
// it; Avahi announces it, answers the network's questions about it, and says goodbye
// for it when the group is reset, freed, or let go by the app's leaving the bus (which
// it is when the app quits, however it quits). Every call is made without waiting for
// its answer, so the app's thread never stops for Avahi. With no Avahi (not installed,
// not running) nothing is advertised and problem() says so: a client can still be told
// the computer's address.
//
// A name another computer is advertising the same service under is a collision: Avahi
// says so, and the app takes the name Avahi offers in its place ("Name #2"), as Avahi's
// own programs do.
class DnsSd : public QObject
{
    Q_OBJECT

public:
    struct Service
    {
        // The name people see ("Mac-Studio"), the type ("_proapiv1ws._tcp"), the port,
        // and the text record's strings ("path=/v1/ws")
        QString name;
        QString type;
        int port = 0;
        QByteArrayList text;

        bool operator==(const Service &other) const = default;
    };

    explicit DnsSd(QObject *parent = nullptr);

    // Advertises the service, in place of what was advertised before; nothing if its
    // name or type is empty
    void advertise(const Service &service);
    void withdraw() { advertise({}); }

    // Whether Avahi says the service is out there, under which name, and if it cannot be,
    // why
    bool advertised() const { return m_established; }
    QString nameInUse() const { return m_nameInUse; }
    QString problem() const { return m_problem; }

signals:
    void stateChanged();

private slots:
    // Avahi's word on the entry group: 2 established, 3 a collision, 4 a failure
    void groupState(int state, const QString &error);

private:
    // Brings Avahi to what is wanted, one call at a time
    void apply();
    void call(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments,
              void (DnsSd::*then)(QDBusPendingCallWatcher *));
    void made(QDBusPendingCallWatcher *watcher);
    void reset(QDBusPendingCallWatcher *watcher);
    void added(QDBusPendingCallWatcher *watcher);
    void committed(QDBusPendingCallWatcher *watcher);
    void renamed(QDBusPendingCallWatcher *watcher);
    bool failed(QDBusPendingCallWatcher *watcher);
    void setState(bool established, const QString &problem);

    Service m_wanted;
    // What is in Avahi's entry group now (and what is on its way there), and the name it
    // goes by there
    Service m_applied;
    Service m_adding;
    QString m_nameInUse;
    QString m_group;
    bool m_busy = false;
    bool m_established = false;
    QString m_problem;
};
