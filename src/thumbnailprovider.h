#pragma once

#include <QQuickAsyncImageProvider>
#include <QStringList>

// Serves "image://thumbnail/<percent-encoded file path>": a small picture of an image
// file, or of a frame from early in a video file. After a "?" in the id come a counter,
// which callers change to make the views ask again once thumbnails have been discarded,
// and then how the piece of media is set to look in its Media Inspector (see
// mediasettings.h), if it has been, each part only if it is set:
//
//   &t=<seconds>          a video's picture taken at that moment (see videoframe.h)
//   &c=<l>,<r>,<t>,<b>    cropped by so many pixels of the media at its own size from its
//                         left, right, top and bottom
//   &r=<90|180|270>       turned clockwise by that much
//   &f=<h|v|hv>           flipped, across or up and down, as it is shown once turned
//
// so that a thumbnail shows the media as it is set to be shown. How it is fitted (which
// depends on what it is shown in), its blur and its effects are drawn over it where it
// is shown (qml/MediaPicture.qml). Each different set of these has a picture of its own
// in the cache.
//
// How it works. Every view that shows a media file as a small picture (the media bin,
// the slide grid, the preview, the editor) is an Image with such a url. Making the
// picture can take a while (a 4K video is some 40 ms; a large PNG has to be read whole),
// so none of it happens on the thread that draws the window:
//
//   - A request is handed to a small pool of worker threads and returns at once; the
//     view shows the picture when it arrives.
//   - A worker first looks in the cache on disk, where every picture made is kept as a
//     small JPEG (or PNG, if it has transparency) under a name worked out from the
//     file's path and time of last change. Most requests end there, in a millisecond or
//     two; a file that has been changed has a new name, and so gets a new picture.
//   - Otherwise it makes the picture (see videoframe.h for video), stores it and
//     hands it over.
//   - A request whose view has gone before a worker got to it (the list was scrolled
//     on, another playlist opened) is dropped without doing the work.
//
// The workers are few: half as many as the processor has threads, so that a folder of
// videos neither opens a decoder for every file at once nor takes the whole machine
// from a show that is running.
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    // Forgets the cached thumbnails of these files, so the next request makes them afresh.
    static void discard(const QStringList &paths);
};
