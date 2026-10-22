#include "calendarevents.h"

#include "calendarfile.h"
#include "sessionlog.h"
#include "workspacefiles.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTimeZone>

#include <algorithm>

namespace {

long long nowSeconds()
{
    return QDateTime::currentSecsSinceEpoch();
}

QTimeZone localZone()
{
    return QTimeZone::systemTimeZone();
}

const QString what = QStringLiteral("the calendar");

} // namespace

CalendarEvents::CalendarEvents(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &CalendarEvents::look);
}

QString CalendarEvents::path() const
{
    return m_workspace.isEmpty() ? QString() : QDir(m_workspace).filePath(QStringLiteral("Configuration/Calendar"));
}

QStringList CalendarEvents::playlistIds() const
{
    QStringList ids;
    for (const QVariant &event : events()) {
        const QVariantMap action = event.toMap().value("action").toMap();
        if (action.value("kind").toString() == QLatin1String("playlist") && !ids.contains(action.value("playlistId").toString()))
            ids << action.value("playlistId").toString();
    }
    return ids;
}

QString CalendarEvents::open(const QString &workspace)
{
    m_workspace = workspace;
    m_lookedTo = nowSeconds();
    const QString error = reload();
    if (error.isEmpty() && !events().isEmpty())
        SessionLog::write("calendar", QStringLiteral("%1 event%2, the calendar %3").arg(events().size())
                                          .arg(events().size() == 1 ? QString() : QStringLiteral("s"))
                                          .arg(enabled() ? QStringLiteral("on") : QStringLiteral("off")));
    schedule();
    return error;
}

QString CalendarEvents::reload()
{
    rv::data::Calendar calendar;
    QString error;
    if (!path().isEmpty() && QFile::exists(path()))
        error = workspace::readMessage(path(), &calendar, what);
    m_calendar = calendarfile::describe(calendar);
    emit changed();
    return error;
}

QString CalendarEvents::edit(const std::function<QString(rv::data::Calendar *)> &change)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("There is no workspace open");
    rv::data::Calendar calendar;
    if (QFile::exists(path())) {
        const QString error = workspace::readMessage(path(), &calendar, what);
        if (!error.isEmpty())
            return error;
    }
    const std::string before = calendar.SerializeAsString();
    const QString error = change(&calendar);
    if (!error.isEmpty())
        return error;
    // (A change that changes nothing writes nothing.)
    if (calendar.SerializeAsString() != before || !QFile::exists(path())) {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        const QString failed = workspace::writeMessage(path(), calendar, what);
        if (!failed.isEmpty())
            return failed;
    }
    reload();
    schedule();
    return {};
}

QString CalendarEvents::setEnabled(bool on)
{
    // Switched on, nothing that came due while it was off is run.
    if (on && !enabled())
        m_lookedTo = nowSeconds();
    const QString error = edit([on](rv::data::Calendar *calendar) {
        calendarfile::setEnabled(calendar, on);
        return QString();
    });
    if (error.isEmpty())
        SessionLog::write("calendar", on ? QStringLiteral("the calendar switched on") : QStringLiteral("the calendar switched off"));
    return error;
}

QVariantMap CalendarEvents::add()
{
    QString made;
    const long long start = calendarfile::newStart(nowSeconds(), localZone());
    const QString error = edit([&](rv::data::Calendar *calendar) { return calendarfile::add(calendar, start, workspace::newUuid, &made); });
    return {{"id", error.isEmpty() ? made : QString()}, {"error", error}};
}

QString CalendarEvents::change(const QString &id, const QVariantMap &changes)
{
    return edit([&](rv::data::Calendar *calendar) { return calendarfile::change(calendar, id, changes); });
}

QString CalendarEvents::remove(const QString &id)
{
    return edit([&](rv::data::Calendar *calendar) { return calendarfile::remove(calendar, id); });
}

QVariantMap CalendarEvents::find(const QString &id) const
{
    for (const QVariant &event : events()) {
        if (event.toMap().value("id").toString() == id)
            return event.toMap();
    }
    return {};
}

QVariantList CalendarEvents::listing(bool recurring) const
{
    QVariantList rows = calendarfile::listing(events(), nowSeconds(), recurring, localZone(), 150);
    for (QVariant &row : rows) {
        QVariantMap map = row.toMap();
        map.insert("when", calendarfile::whenText(map.value("at").toLongLong(), localZone(), QLocale()));
        row = map;
    }
    return rows;
}

double CalendarEvents::now() const
{
    return double(nowSeconds());
}

QString CalendarEvents::dateText(double at) const
{
    return calendarfile::dateText((long long)at, localZone(), QLocale());
}

QString CalendarEvents::timeText(double at) const
{
    return calendarfile::timeText((long long)at, localZone(), QLocale());
}

QString CalendarEvents::whenText(double at) const
{
    return calendarfile::whenText((long long)at, localZone(), QLocale());
}

double CalendarEvents::withDate(double at, const QString &text) const
{
    return double(calendarfile::withDate((long long)at, text, localZone(), QLocale()));
}

double CalendarEvents::withTime(double at, const QString &text) const
{
    return double(calendarfile::withTime((long long)at, text, localZone(), QLocale()));
}

void CalendarEvents::schedule()
{
    m_timer.stop();
    if (!enabled() || events().isEmpty())
        return;
    const long long now = nowSeconds();
    const long long next = calendarfile::nextAfter(events(), std::max(m_lookedTo, now - calendarfile::graceSeconds), localZone());
    if (next < 0)
        return;
    // To the second it is due (and a little after, so that it is due when looked at),
    // and in any case within the hour, so that a clock put right, or a computer that
    // slept, is caught up with
    const long long wait = std::clamp<long long>((next - now) * 1000 + 50, 50, 3600LL * 1000);
    m_timer.start(int(wait));
}

void CalendarEvents::look()
{
    const long long now = nowSeconds();
    // (A clock put back starts again from where it now is.)
    if (now < m_lookedTo)
        m_lookedTo = now;
    if (enabled()) {
        for (const QVariant &entry : events()) {
            const QVariantMap event = entry.toMap();
            for (long long at : calendarfile::occurrences(event, m_lookedTo, now, localZone(), 100)) {
                if (calendarfile::stillDue(at, now)) {
                    emit due(event, double(at));
                } else {
                    SessionLog::write("calendar", QStringLiteral("the event \"%1\" came due at %2, while nothing could run it, and is missed")
                                                      .arg(event.value("name").toString(), calendarfile::whenText(at, localZone(), QLocale())));
                }
            }
        }
    }
    m_lookedTo = now;
    ++m_tick;
    emit ticked();
    schedule();
}
