#pragma once

#include <QList>
#include <QString>

// ProPresenter's Color Triggers: on a text box linked to a timer (or to how long a video
// has left), a list of times, each with a colour, at which the time's words change to
// that colour. ProPresenter keeps them on the link itself (DataLink.TimerText's and
// DataLink.VideoCountdown's color_triggers: a time, a double, and a colour), so they are
// read and written with the rest of the link (proconvert.cpp), and the colour is put on
// the words where they are drawn (SlideElement.qml, StrokedText).
//
// ProPresenter's help ("Using Timers on Stage Screens") says that for a countdown "the
// colors will change as the time goes down to zero": orange at 1:00, yellow at 0:30 and
// red for the last ten seconds, in its example; and that "if the timer is set to allow
// overrun, the final color ... will remain the timer's color". So for a time that counts
// down, the trigger in force is the one with the smallest time the time shown has come
// down to: none while the time is above them all, the one at 0:00 (ProPresenter's own
// stage layouts have a red one there) from zero on and through an overrun.
//
// GUESSES, the help saying nothing of them:
//   - A timer that counts up (an elapsed time) changes as it goes up: the trigger in
//     force is the one with the largest time it has reached.
//   - The time is in seconds. ProPresenter's own layouts have a trigger only at zero,
//     which is the same in any unit; the timers' own durations are in seconds.
//   - A time that is exactly a trigger's has reached it.
// The order of the list is not what decides: each time is compared. The list's order is
// the person's own (it can be changed by dragging, which ProPresenter cannot do), and is
// kept as it is in the file.
namespace colortriggers {

// Which trigger is in force for a time showing `value` seconds, given the triggers'
// times: its place in the list, or -1 for none.
int inForce(const QList<double> &times, double value, bool countsDown);

// A trigger's time as the inspector shows it, as ProPresenter writes it: minutes,
// seconds and hundredths ("1:00.00", "0:30.00"), with hours before them where there
// are any ("1:01:00.00"). Never negative.
QString written(double seconds);

// A time typed for a trigger, read back: "1:00.00", "1:00", "0:30.5", ":30", "1:01:00",
// or a number of seconds alone ("90"). -1 for what is not a time.
double read(const QString &typed);

}
