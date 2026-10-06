#pragma once

#include <QImage>
#include <QString>

// One picture from early in a video file, for a thumbnail, no wider than `width`. Null if
// the file cannot be read as video.
//
// A video is a run of keyframes, each a whole picture, with the frames between them
// stored as changes from the frame before. To show the frame at some exact moment a
// player has to decode its way there from the last keyframe, and that is what a
// thumbnail does not need: any picture from around that moment will do. So this seeks
// to the keyframe at or before a point a little way in (the very start of a clip is
// often black, or a fade from it), decodes that one frame and stops. For a 4K clip on a
// 2017 laptop that is about 40 ms, where playing up to the same point through a media
// player took about 200.
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
