#include "dnssd.h"

#include "sessionlog.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace {

const QString avahi = QStringLiteral("org.freedesktop.Avahi");
const QString serverInterface = QStringLiteral("org.freedesktop.Avahi.Server");
const QString groupInterface = QStringLiteral("org.freedesktop.Avahi.EntryGroup");
// Avahi's "any": every network interface, IPv4 and IPv6 both
constexpr qint32 anyInterface = -1;
constexpr qint32 anyProtocol = -1;

}

DnsSd::DnsSd(QObject *parent)
    : QObject(parent)
{
}

void DnsSd::advertise(const Service &service)
{
    const Service wanted = service.name.isEmpty() || service.type.isEmpty() ? Service {} : service;
    if (wanted == m_wanted)
        return;
    // (A new name is the name used, until it collides)
    if (wanted.name != m_wanted.name)
        m_nameInUse = wanted.name;
    m_wanted = wanted;
    apply();
}

void DnsSd::apply()
{
    if (m_busy || m_wanted == m_applied)
        return;
    if (m_group.isEmpty()) {
        if (m_wanted.name.isEmpty())
            return;
        m_busy = true;
        call(QStringLiteral("/"), serverInterface, QStringLiteral("EntryGroupNew"), {}, &DnsSd::made);
        return;
    }
    // What is there is taken away (Avahi says goodbye for it), and what is wanted put in
    m_busy = true;
    call(m_group, groupInterface, QStringLiteral("Reset"), {}, &DnsSd::reset);
}

void DnsSd::call(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments,
                 void (DnsSd::*then)(QDBusPendingCallWatcher *))
{
    QDBusMessage message = QDBusMessage::createMethodCall(avahi, path, interface, method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, then](QDBusPendingCallWatcher *done) {
        done->deleteLater();
        (this->*then)(done);
    });
}

bool DnsSd::failed(QDBusPendingCallWatcher *watcher)
{
    if (!watcher->isError())
        return false;
    const QDBusError error = watcher->error();
    const bool absent = error.type() == QDBusError::ServiceUnknown || error.name() == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner");
    const QString why = absent ? QStringLiteral("Avahi, which advertises it on the network, is not running on this computer")
                               : QStringLiteral("Avahi could not advertise it: %1").arg(error.message());
    m_busy = false;
    m_group.clear();
    m_applied = {};
    if (why != m_problem)
        SessionLog::write("network", QStringLiteral("%1 is not advertised on the network: %2").arg(m_wanted.type, why));
    setState(false, why);
    return true;
}

void DnsSd::made(QDBusPendingCallWatcher *watcher)
{
    if (failed(watcher))
        return;
    m_group = QDBusPendingReply<QDBusObjectPath>(*watcher).value().path();
    QDBusConnection::systemBus().connect(avahi, m_group, groupInterface, QStringLiteral("StateChanged"), this,
                                         SLOT(groupState(int, QString)));
    m_busy = false;
    m_applied = {};
    apply();
}

void DnsSd::reset(QDBusPendingCallWatcher *watcher)
{
    if (failed(watcher))
        return;
    m_applied = {};
    setState(false, {});
    if (m_wanted.name.isEmpty()) {
        m_busy = false;
        return;
    }
    // (The text record as an array of byte arrays, D-Bus's "aay")
    QDBusArgument text;
    text.beginArray(QMetaType::fromType<QByteArray>());
    for (const QByteArray &one : std::as_const(m_wanted.text))
        text << one;
    text.endArray();
    // (Applied once Avahi has it: a change meanwhile is seen as one by apply)
    m_adding = m_wanted;
    call(m_group, groupInterface, QStringLiteral("AddService"),
         {anyInterface, anyProtocol, QVariant::fromValue(quint32(0)), m_nameInUse, m_wanted.type, QString(), QString(), QVariant::fromValue(quint16(m_wanted.port)),
          QVariant::fromValue(text)},
         &DnsSd::added);
}

void DnsSd::added(QDBusPendingCallWatcher *watcher)
{
    if (failed(watcher))
        return;
    call(m_group, groupInterface, QStringLiteral("Commit"), {}, &DnsSd::committed);
}

void DnsSd::committed(QDBusPendingCallWatcher *watcher)
{
    if (failed(watcher))
        return;
    m_applied = m_adding;
    m_busy = false;
    apply();
}

void DnsSd::groupState(int state, const QString &error)
{
    if (state == 2) {
        SessionLog::write("network", QStringLiteral("advertised on the network as \"%1\" (%2, port %3)").arg(m_nameInUse, m_applied.type).arg(m_applied.port));
        setState(true, {});
    } else if (state == 3) {
        // Another has the name: Avahi's alternative in its place ("Name #2"), and again
        SessionLog::write("network", QStringLiteral("\"%1\" is taken on the network by another %2: another name is chosen").arg(m_nameInUse, m_applied.type));
        m_busy = true;
        call(QStringLiteral("/"), serverInterface, QStringLiteral("GetAlternativeServiceName"), {m_nameInUse}, &DnsSd::renamed);
    } else if (state == 4) {
        const QString why = QStringLiteral("Avahi could not advertise it%1").arg(error.isEmpty() ? QString() : QStringLiteral(": ") + error);
        SessionLog::write("network", QStringLiteral("%1 is not advertised on the network: %2").arg(m_applied.type, why));
        setState(false, why);
    }
}

void DnsSd::renamed(QDBusPendingCallWatcher *watcher)
{
    if (failed(watcher))
        return;
    m_nameInUse = QDBusPendingReply<QString>(*watcher).value();
    m_applied = {};
    m_busy = false;
    apply();
}

void DnsSd::setState(bool established, const QString &problem)
{
    if (established == m_established && problem == m_problem)
        return;
    m_established = established;
    m_problem = problem;
    emit stateChanged();
}
