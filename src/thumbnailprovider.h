#pragma once

#include <QQuickAsyncImageProvider>
#include <QStringList>

// Serves "image://thumbnail/<percent-encoded file path>": a small picture of an image
// file, or of a frame from early in a video file. Anything after a "?" in the id is
// ignored: callers put a counter there and change it to make the views ask again once
// thumbnails have been discarded.
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
