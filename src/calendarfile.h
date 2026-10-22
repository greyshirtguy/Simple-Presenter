#pragma once

#include "calendar.pb.h"

#include <QList>
#include <QLocale>
#include <QString>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <string>

// ProPresenter's Calendar: events at a date and time, once or every week on chosen
// days, each with an action that runs as it comes due (a playlist's presentation going
// live, or a macro). Plain rules on ProPresenter's own message of it, and on the times
// it gives, with nothing of the windows or of the disk in them, so that they can be
// tried by themselves (tests/unit/tst_calendarfile). The object the windows talk to,
// which reads and writes the file and keeps the time, is CalendarEvents.
//
// Where ProPresenter keeps it. A workspace's Configuration/Calendar, a file of its own
// (rv.data.Calendar): whether the calendar is on (`active`, the window's "Calendar
// Enabled") and the events, each { uuid, name, description, date, recurrence_days,
// recurrence_limit_date, recurrence_excluded_dates, action, actions }. (The set-up
// file's own calendar field is not what ProPresenter writes, and is left alone.) One
// file of ProPresenter's was looked at (the maintainer's, made for this, ProPresenter
// 21 on a Mac), with three events, and it says:
//   - `date` is when the event first comes due, in seconds since 1970 in UTC: 7 pm in
//     Sydney in October is 08:00 UTC.
//   - `recurrence_days` are the days of the week it comes due on, every week, at the
//     time of day of `date` (ProPresenter's Sun to Sat buttons, "repeat"); none for an
//     event that comes due once. ProPresenter's numbers run from Sunday, 1, to
//     Saturday, 7.
//   - `recurrence_limit_date` is "ends on", a date and a time; none for one that never
//     ends.
//   - the action is written twice over. `action` is the calendar's own: its `type` is
//     101 for a playlist (the playlist's id, and the id of one of its rows where one is
//     chosen) and 102 for a macro (its id and name, and its collection's). And `actions`
//     holds the same as an action of ProPresenter's ordinary kind, as a slide's are: a
//     Library Playlist action ({ playlist id, row id }) or a Macro action, whose id is
//     the playlist's or the macro's own and whose name is theirs. Both are written here
//     as ProPresenter wrote them.
//
// GUESSES, where the one file does not say:
//   - what ProPresenter writes for an action of None: nothing at all, here (no `action`
//     and none in `actions`); and for Capture, which this app has nothing of and does
//     not offer: an event that has one, or anything else, is kept as it is and shown
//     as what it is.
//   - that a repeating event comes due at the same time of day on the clock wherever
//     summer time begins or ends between two of its days (7 pm stays 7 pm), as a
//     calendar does; and on the chosen days only, so that the first day itself counts
//     only if it is one of them.
//   - that "ends on" counts: an event due at exactly that moment comes due.
//   - that a date in `recurrence_excluded_dates` leaves out what is due that day (on
//     the clock where the app is); no file with one has been seen, and nothing here
//     makes one.
//   - what a new event is: named "New Event", as the file's first is, due once at the
//     next whole hour, with no action.
//
// Every change is made to the message given, changing the one thing and nothing else,
// so that what else is in it, and anything this does not know of, is kept.
namespace calendarfile {

// The kinds of an event's action in the file (Calendar.Event.Action.type)
constexpr unsigned playlistType = 101;
constexpr unsigned macroType = 102;

// How late an event may still be run, in seconds: one that came due longer ago than
// that (while the computer slept, say) is missed, and not run late. (The maintainer's
// choice, 2026-10-22: a missed event is skipped.)
constexpr long long graceSeconds = 60;

// The calendar as the windows are handed it: { enabled, events }, each event as
// describeEvent gives it, in the file's order
QVariantMap describe(const rv::data::Calendar &calendar);
// { id, name, description, start (seconds since 1970), days (ProPresenter's numbers,
// Sunday 1 to Saturday 7, in order), ends (seconds; 0 for none), excluded (seconds),
// action }, where `action` is { kind: "none", "playlist", "macro", "capture" or
// "other", name (the playlist's or the macro's, as the file names it), playlistId,
// itemId, macroId, macroName, collectionId, collectionName }
QVariantMap describeEvent(const rv::data::Calendar::Event &event);

// The changes, each returning an error message, empty on success. An event is found by
// its id; `newId` makes ids for what is new.
//
// A new event (see the GUESSES), last, due at `start`; *made is its id.
QString add(rv::data::Calendar *calendar, long long start, const std::function<std::string()> &newId, QString *made = nullptr);
// Changes an event's `name`, `description`, `start`, `days`, `ends` (0 for none) or
// `action`, as `changes` has them. An action is { kind: "none" } or { kind: "playlist",
// playlistId, playlistName, itemId (empty for none) } or { kind: "macro", macroId,
// macroName, collectionId, collectionName }; it is written in both of the file's
// forms, and what else the event's `actions` held is kept.
QString change(rv::data::Calendar *calendar, const QString &id, const QVariantMap &changes);
QString remove(rv::data::Calendar *calendar, const QString &id);
void setEnabled(rv::data::Calendar *calendar, bool on);

// The times an event (as describeEvent gives it) comes due after `after` and up to
// `until`, both in seconds since 1970, on the clock of `zone`: at most `most` of them,
// in order
QList<long long> occurrences(const QVariantMap &event, long long after, long long until, const QTimeZone &zone, int most);
// The first time any of `events` comes due after `after`, looking a little over a year
// ahead; -1 for none
long long nextAfter(const QVariantList &events, long long after, const QTimeZone &zone);
// Whether what came due at `at` is still to be run at `now`: not yet missed
bool stillDue(long long at, long long now);
// When a new event is first due: the next whole hour after `now`
long long newStart(long long now, const QTimeZone &zone);

// The calendar's list, as ProPresenter's window has it. With `recurring` each time an
// event comes due is a row of its own: under "current", the last time each event came
// due, if it has; under "upcoming", the times still to come, in order, a little over a
// year ahead and at most `most`. Without it, each event is one row, at its first time.
// Each row is { id, name, at, past, section }, `past` being whether it has come due.
// (A GUESS at what ProPresenter's "Current" holds: in its window it held the one
// event whose time had passed, in red.)
QVariantList listing(const QVariantList &events, long long now, bool recurring, const QTimeZone &zone, int most);

// Times as the window writes them, in `locale`, on the clock of `zone`: the date
// ("27/10/2026": the locale's short date, with the year whole), the time of day
// ("7:00:00 pm", or "19:00:00" where the locale has a 24-hour clock), and the two
// together as the list writes them ("27 Oct 2026 at 7:00:00 pm")
QString dateText(long long at, const QTimeZone &zone, const QLocale &locale);
QString timeText(long long at, const QTimeZone &zone, const QLocale &locale);
QString whenText(long long at, const QTimeZone &zone, const QLocale &locale);
// The moment `at` with its date, or its time of day, put as typed: -1 where the text is
// no date (as dateText writes one, or 2026-10-27) or no time ("7:00:00 pm", "7 pm",
// "7:30pm", "19:00", "19:00:30")
long long withDate(long long at, const QString &text, const QTimeZone &zone, const QLocale &locale);
long long withTime(long long at, const QString &text, const QTimeZone &zone, const QLocale &locale);

} // namespace calendarfile
