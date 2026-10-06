#pragma once

#include <QQuickAsyncImageProvider>
#include <QStringList>

// Serves "image://thumbnail/<percent-encoded file path>": a small preview of an image
// file, or a frame from early in a video file, cached on disk between runs. Never blocks
// the UI; a file that cannot be read yields an empty image.
// Anything after a "?" in the id is ignored: callers put a counter there and change it
// to make the views ask again once thumbnails have been discarded.
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    // Forgets the cached thumbnails of these files, so the next request makes them afresh.
    static void discard(const QStringList &paths);
};
