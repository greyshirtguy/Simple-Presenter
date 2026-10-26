#pragma once

#include <QList>

// The rules of a screen's video delay, ProPresenter's "video frame delay" (its file keeps
// it with each output, as a number of frames: OutputDisplay.video_delay): everything the
// screen shows is shown so many of its frames late, so that its picture can be put level
// with sound that is heard late (the app's own mixer keeps some in hand: audiomix.h), or
// with another screen whose picture is late for the projector or the switcher it goes
// through. Plain rules on lists of times, with nothing of the windows in them, so that
// they can be tried by themselves (tests/unit/tst_delayline); FrameDelay (framedelay.h)
// follows them.
//
// How it goes. Each time what the screen shows is drawn anew, a copy of it is held, with
// when it was drawn; and at each of the screen's frames the one shown is the newest of
// them drawn at least the delay before. Pictures are drawn at the screen's frames, each a
// little before or after its moment, so one drawn so many frames ago counts as drawn then
// to within half a frame either way: shown at exactly that frame, not a frame early or late.
// What is older than the one shown is let go of; until any is old enough, the oldest is
// shown (as when the delay is first set).
namespace delayline {

// The most frames a screen can be delayed by. A GUESS at ProPresenter's: its file has room
// for any number. Each frame of delay can mean a picture of the screen held on the
// graphics chip (8 MB for 1080p), and fifteen is a quarter of a second at sixty a second.
constexpr int kMostFrames = 15;

// Which of the pictures held is to be shown at `now` (each by when it was drawn, oldest
// first; milliseconds on one clock), `delay` milliseconds late on a screen of frames
// `frame` milliseconds long: the newest drawn at least `delay` before it, to within half a
// frame; the oldest if none is yet; -1 if none is held.
int shown(const QList<double> &drawnAt, double now, double delay, double frame);

// When the next of them after the one shown is due: the earliest `now` at which shown()
// gives it; -1 if there is none
double nextDue(const QList<double> &drawnAt, int shown, double delay, double frame);

// How many pictures may have to be held at once for a delay: one for each frame of it,
// the one shown, and the one being drawn
int most(double delay, double frame);

} // namespace delayline
