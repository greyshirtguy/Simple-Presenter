#include "awake.h"

#include "sessionlog.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>

namespace {

// The services that can be asked, in the order they are tried.
struct Service
{
    const char *name;
    const char *path;
    const char *interface;
    const char *release;
    // Whether Inhibit takes a window and a set of flags as well, as GNOME's does
    bool gnome;
};
const Service services[] = {
    {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", "UnInhibit", false},
    {"org.gnome.SessionManager", "/org/gnome/SessionManager", "org.gnome.SessionManager", "Uninhibit", true},
};
const int serviceCount = int(sizeof(services) / sizeof(services[0]));

const QString appId = QStringLiteral("io.github.greyshirtguy.SimplePresenter");
const QString reason = QStringLiteral("A presentation is on the output");
// GNOME's flag for "do not count the session as idle"
const uint gnomeIdle = 8;
// How long an answer is waited for, in milliseconds
const int patience = 3000;

}

Awake::~Awake()
{
    // (The desktop ends the asking of an app that has gone by itself; this is for
    // tidiness.)
    release();
}

void Awake::setWanted(bool wanted)
{
    if (m_wanted == wanted)
        return;
    m_wanted = wanted;
    emit wantedChanged();
    if (!m_wanted)
        release();
    else if (!m_held && !m_asking && !m_refused)
        ask(0);
}

void Awake::ask(int service)
{
    // Drawn into memory, for a test or a benchmark, there is no screen to keep awake.
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (service >= serviceCount || !bus.isConnected()) {
        m_refused = true;
        SessionLog::write("awake", QStringLiteral("The desktop could not be asked to keep the screens awake while the output is showing (%1). "
                                                  "If they go dark during a show, turn off blanking in the desktop's power settings.")
                                       .arg(bus.isConnected() ? QStringLiteral("it answers to none of the ways of asking")
                                                              : QStringLiteral("there is no session bus")));
        return;
    }
    const Service &asked = services[service];
    QDBusMessage message = QDBusMessage::createMethodCall(asked.name, asked.path, asked.interface, QStringLiteral("Inhibit"));
    if (asked.gnome)
        message << appId << uint(0) << reason << gnomeIdle;
    else
        message << appId << reason;
    m_asking = true;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, patience), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, service](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<uint> reply = *watcher;
        watcher->deleteLater();
        m_asking = false;
        if (reply.isError()) {
            ask(service + 1);
            return;
        }
        m_service = service;
        m_cookie = reply.value();
        m_held = true;
        if (!m_wanted) {
            // It stopped being wanted while the desktop was thinking about it.
            release();
            return;
        }
        SessionLog::write("awake", QStringLiteral("the screens are kept from sleeping while the output or the stage is showing (asked of %1)")
                                       .arg(QLatin1String(services[service].name)));
        emit heldChanged();
    });
}

void Awake::release()
{
    if (!m_held)
        return;
    const Service &asked = services[m_service];
    QDBusMessage message = QDBusMessage::createMethodCall(asked.name, asked.path, asked.interface, QLatin1String(asked.release));
    message << m_cookie;
    QDBusConnection::sessionBus().send(message);
    m_held = false;
    m_service = -1;
    m_cookie = 0;
    SessionLog::write("awake", QStringLiteral("neither the output nor the stage is showing: the screens may sleep again"));
    emit heldChanged();
}
