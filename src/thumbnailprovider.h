#pragma once

#include <QQuickAsyncImageProvider>

// Serves "image://thumbnail/<percent-encoded file path>": a small preview of an image
// file, or a frame from early in a video file, cached on disk between runs. Never blocks
// the UI; a file that cannot be read yields an empty image.
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
