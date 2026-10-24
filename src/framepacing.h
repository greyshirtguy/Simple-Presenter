#pragma once

#include <QList>

// When a screen that is playing a video is to draw the frames of something else that
// moves on it (a slide's transition, scrolling text, a build): plain rules, with nothing
// of the windows in them, so that they can be tried by themselves (tests/unit/tst_framepacing).
// FrameClock (frameclock.h) follows them.
//
// Why there are rules for it. A screen is a window, and on Wayland Qt hands the desktop a
// window's next picture only once the desktop has taken the last: one picture a refresh
// of the screen, sixty a second. A video of thirty frames a second on a screen that
// draws only when something changes has a picture of its own every other refresh, each
// drawn the moment the frame comes; the refreshes between are free. If anything else
// draws a picture in the refresh in which the next frame of the video comes, before it
// comes, the frame has to wait for the refresh after: the frame before it stays on the
// screen for three refreshes and it for one, which is a stutter. So whatever else moves
// is drawn in the free refreshes, a refresh after each frame of the video, and never in
// the last refresh before the next one. (A picture drawn at a refresh's length after a
// frame of the video is always in the refresh after the frame's own, whatever the phase
// of the screen's refreshes, which the app cannot know: two moments a refresh apart
// always have a refresh between them.)
//
// The same reasoning has a picture of the screen's own time drawn at every refresh where
// there is no video (see FrameClock): it is the video, where there is one, that is the
// clock the screen keeps to.
namespace framepacing {

// How long a frame of the video lasts, in milliseconds, from the times its last frames
// came, in order: the middle one of the gaps between them, so that one frame that was
// late does not count. 0 for fewer than two.
double videoInterval(const QList<double> &arrivals);

// The moments after a frame of the video comes, in milliseconds, at which the screen is
// to draw what else moves before the next frame comes: a refresh after it, two, and so
// on, for as long as a whole refresh is left before the next frame (and a millisecond to
// spare, for the frames' coming a little early or late). For thirty frames a second on a
// 60 Hz screen, one; for twenty-five or twenty-four, one; for fifteen, three; for sixty,
// none, every refresh having a frame of the video.
QList<double> framesBetween(double videoInterval, double refresh);

// Whether the video has stopped coming (paused, at its end, or waiting for the file): no
// frame for half again as long as one lasts. The screen then draws by its own refreshes.
bool videoStopped(double sinceLastFrame, double videoInterval);

} // namespace framepacing
