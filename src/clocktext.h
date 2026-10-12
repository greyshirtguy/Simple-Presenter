#pragma once

#include <QDateTime>
#include <QLocale>
#include <QString>

// The time of day as a text box linked to the system's clock shows it: plain rules, with
// nothing of the windows in them and no clock of their own, so that they can be tried
// by themselves on a moment that is given (tests/unit/tst_clocktext).
//
// How ProPresenter's files say it. A link to the clock (a `ClockText`) has how the date
// is written and how the time is, each one of five ways that are numbered as Apple's
// date formatter numbers them, since that is what ProPresenter on a Mac writes them
// with; whether the time is on the twenty-four hour clock; and a pattern, text with
// "${clock}" where the date and time go. One file of ProPresenter's own had three such
// links, all alike: no date, the time the short way, twenty-four hours, and the
// pattern "${clock}" alone.
//
// The five ways, as that formatter writes them in English as the United States has it:
//
//            a date                          a time
//   None     nothing                         nothing
//   Short    11/23/37                        3:30 PM
//   Medium   Nov 23, 1937                    3:30:32 PM
//   Long     November 23, 1937               3:30:32 PM PST
//   Full     Tuesday, November 23, 1937      3:30:32 PM Pacific Standard Time
//
// Here they are written with this computer's own words for the months, the days and the
// halves of the day, and its own order for the parts of a date, as the system's
// settings have them.
//
// GUESSES, a clock so linked not having been looked at in ProPresenter in every one of
// its ways: what stands between a date and a time when both are shown (a comma and a
// space after a short date, " at " after any other, which is what that formatter puts
// in English); and that a pattern with no "${clock}" in it shows the clock alone, as a
// timer's does.
namespace clocktext {

enum Style { None = 0, Short = 1, Medium = 2, Long = 3, Full = 4 };

// The moment written those ways, in the words and the order of `locale`.
QString written(const QDateTime &moment, int dateStyle, int timeStyle, bool twentyFourHour, const QLocale &locale);
// That, put into `pattern` where the pattern has "${clock}".
QString linked(const QDateTime &moment, int dateStyle, int timeStyle, bool twentyFourHour, const QString &pattern, const QLocale &locale);

} // namespace clocktext
