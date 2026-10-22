#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace rv::data {
class Calendar;
}

// The workspace's calendar, for QML, and its clock: ProPresenter's Configuration/Calendar
// (see calendarfile.h for what it holds and the rules), read when a workspace is opened
// and written as it is changed from the Calendar window; and the time kept, so that as
// an event comes due it is said (`due`), for the operator window to run its action.
//
// The file is the truth: every change reads it as it is now, makes the one change and
// writes it back, so that what ProPresenter wrote since, and what this app knows nothing
// of, is kept. A workspace with no file has an empty calendar that is off; the file is
// made when something is first changed.
//
// The clock. Nothing looks at the time while nothing is due: a timer is set for the next
// time an event comes due (and, in case the computer's clock is put right or the computer
// sleeps, for no more than an hour on), and when it goes off, what has come due since the
// last look is said. What came due longer ago than calendarfile::graceSeconds is missed,
// noted in the log and not run: a workspace opened, or the calendar switched on, after an
// event's time does not run it late (the maintainer's choice), nor does a computer that
// slept through it. While the calendar is off nothing is due.
//
// Called CalendarEvents and not Calendar, which is the name of a type of Qt's own.
class CalendarEvents : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // ProPresenter's "Calendar Enabled"
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    // The events, as calendarfile::describeEvent gives them, in the file's order
    Q_PROPERTY(QVariantList events READ events NOTIFY changed)
    // The ids of the playlists an event goes live from, for the library's calendar mark
    Q_PROPERTY(QStringList playlistIds READ playlistIds NOTIFY changed)
    // Goes up each time the clock is looked at, so that what says whether an event has
    // come due is asked again
    Q_PROPERTY(int tick READ tick NOTIFY ticked)

public:
    explicit CalendarEvents(QObject *parent = nullptr);

    bool enabled() const { return m_calendar.value("enabled").toBool(); }
    QVariantList events() const { return m_calendar.value("events").toList(); }
    QStringList playlistIds() const;
    int tick() const { return m_tick; }

    // Reads a workspace folder's calendar, and starts its clock from now. Returns an
    // error message, empty on success.
    Q_INVOKABLE QString open(const QString &workspace);
    // The changes, as calendarfile's are, each written at once: an error message back,
    // empty on success. Switched on, the calendar's clock starts from now.
    Q_INVOKABLE QString setEnabled(bool on);
    // A new event: { id, error }
    Q_INVOKABLE QVariantMap add();
    Q_INVOKABLE QString change(const QString &id, const QVariantMap &changes);
    Q_INVOKABLE QString remove(const QString &id);
    // One event, as `events` has it; empty for none
    Q_INVOKABLE QVariantMap find(const QString &id) const;

    // The list the window shows (calendarfile::listing), each row with `when`, its time
    // as the list writes it
    Q_INVOKABLE QVariantList listing(bool recurring) const;
    // Now, and times as the window writes and reads them, on this computer's clock and
    // in its locale (calendarfile's dateText and the rest): seconds since 1970, -1 for
    // text that is no date or time
    Q_INVOKABLE double now() const;
    Q_INVOKABLE QString dateText(double at) const;
    Q_INVOKABLE QString timeText(double at) const;
    Q_INVOKABLE QString whenText(double at) const;
    Q_INVOKABLE double withDate(double at, const QString &text) const;
    Q_INVOKABLE double withTime(double at, const QString &text) const;

signals:
    void changed();
    void ticked();
    // An event has come due, at `at`, for its action to be run
    void due(const QVariantMap &event, double at);

private:
    QString path() const;
    // Reads the file again into what is shown
    QString reload();
    // Writes `change` made to the file as it is now
    QString edit(const std::function<QString(rv::data::Calendar *)> &change);
    // Sets the timer for the next time something is due
    void schedule();
    // Says what has come due since the last look
    void look();

    QString m_workspace;
    QVariantMap m_calendar;
    // Up to when it has been looked at: seconds since 1970
    long long m_lookedTo = 0;
    QTimer m_timer;
    int m_tick = 0;
};
