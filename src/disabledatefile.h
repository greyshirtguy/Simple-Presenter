#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QTimeZone>

// Disable dates: a slide set to be disabled at a date and time, this app's own (ProPresenter
// has no such thing). The plain rules of the file they are kept in, with nothing of the
// windows in them, so that they can be tried by themselves (tests/unit/tst_disabledatefile);
// DisableDates (src/disabledates.h) reads and writes the file and disables the slides.
//
// Why a file of the app's own, and in the workspace: ProPresenter keeps no field it does
// not know. A presentation with one in six places (a cue, its action, its slide, the
// presentation...) was opened, edited and saved by ProPresenter 7 on the Mac, and came back
// with none of them, every id in it kept (the maintainer's test, 2026-10-23). The ids are
// what survive, so a date is kept beside the presentation, by the presentation's uuid and
// the slide's (its cue's), in the workspace folder, so that it goes where the workspace
// goes (the maintainer's choice). When the date comes, the slide is disabled in its file as
// ProPresenter disables one (the cue's isEnabled), and the date is done with.
//
// The file is `<workspace>/SimplePresenter/Disable Dates.json`:
//
//     { "about": "...", "dates": [ { "presentation": "<uuid>", "path": "Libraries/Demo/x.pro",
//       "name": "x", "slide": "<uuid>", "at": "2026-10-30T09:00:00+11:00" } ] }
//
// "path" is where the presentation was, from the workspace's folder, to find it by; its uuid
// says whether what is there is still it. "name" is for whoever reads the file. "at" is the
// time, ISO 8601 with the offset from UTC of the clock it was set on. Every other key of the
// file and of each date is kept as it was when the file is written again.
//
// A GUESS: that ProPresenter leaves a folder in its workspace that it does not know alone.
namespace disabledatefile {

struct Entry {
    QString presentation;
    QString path;
    QString name;
    QString slide;
    long long at = 0;
};

// The file of the dates in a workspace's folder
QString pathIn(const QString &workspace);

// The file's dates: none for a missing or empty file. A file that is there but cannot be
// read as one says so in `error`, and gives none.
QList<Entry> read(const QByteArray &bytes, QString *error);
// `bytes` (the file as it is, or nothing) with its dates made `entries`, each written with
// its time on `zone`'s clock; every other key of it and of a date that is kept stays.
QByteArray write(const QByteArray &bytes, const QList<Entry> &entries, const QTimeZone &zone);

// `entries` with `entry` in them, in place of a date already set for that slide
QList<Entry> with(QList<Entry> entries, const Entry &entry);
// `entries` without the date of a slide
QList<Entry> without(QList<Entry> entries, const QString &presentation, const QString &slide);
// The date of a slide, or 0 for none
long long dateOf(const QList<Entry> &entries, const QString &presentation, const QString &slide);
// The dates that have come by `now`, the earliest first
QList<Entry> due(const QList<Entry> &entries, long long now);
// The first date after `now`, or -1 for none
long long next(const QList<Entry> &entries, long long now);

} // namespace disabledatefile
