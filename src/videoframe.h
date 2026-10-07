#pragma once

#include <QImage>
#include <QString>

// One picture from early in a video file, for a thumbnail, no wider than `width`. Null if
// the file cannot be read as video.
//
// Which picture: one from about five seconds in, or from half way through a video
// shorter than ten seconds. Not the first: a great many videos fade in from black, and
// a thumbnail of the first frame is then a black rectangle, which says nothing about
// which video it is. For the same reason a picture that is nearly black is passed over
// for one a little later, if there is one that is not, as far as fifteen seconds in.
//
// A video is a run of keyframes, each a whole picture, with the frames between them
// stored as changes from the frame before. To show the frame at some exact moment a
// player has to decode its way there from the last keyframe, and a thumbnail does not
// need the exact moment: any picture from around it will do. So this looks first for a
// keyframe at that moment or within ten seconds after it (and not beyond seven tenths
// of the way through, where a video is on its way out), and if there is one decodes
// that single frame and stops. For a 4K clip on a 2017 laptop that is about 40 ms. Of
// the hundred or so videos this was tried on, every one had such a keyframe.
//
// Where there is none, as in a clip encoded with a single keyframe at its start, there
// is nothing for it but to begin at the keyframe before the moment and decode every
// frame from there up to it. That is slower by as many times as there are frames to
// get through (a fifth of a second for five seconds of 720p, over a second for 4K),
// and it is given two and a half seconds; a video too large to get to its moment in
// that time gives the picture it had got to, which is at least well past its first.
//
// It is done with FFmpeg's libraries directly, the same ones Qt Multimedia plays video
// with, and on the CPU. Decoding on the graphics hardware was tried: it decodes a 4K
// keyframe in 4 ms against 40, but the picture then has to be copied back out of video
// memory to be scaled down and saved, which costs about 25 ms, and the Intel driver
// Ubuntu ships cannot do the scaling on the hardware. That came to 31 ms against 45 for
// a 4K clip and no gain at 1080p, which is not worth depending on a video driver for
// something done once for each file.
//
// Safe to call from any thread, and from several at once.
QImage grabVideoFrame(const QString &path, int width);
