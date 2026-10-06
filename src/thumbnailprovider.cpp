#include "thumbnailprovider.h"

#include "videoframe.h"
#include "workspacefiles.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QUrl>

namespace {

// Wide enough for the largest thumbnail the views show, on a screen scaled to 150%.
const int thumbnailWidth = 480;

// Set as the app closes: whatever has not been started is then not worth starting.
QAtomicInt closing;

class Response : public QQuickImageResponse
{
public:
    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    // The view no longer wants the picture. Called from another thread than the one
    // doing the work, which looks at this before it starts.
    void cancel() override { m_cancelled.storeRelaxed(1); }
    bool cancelled() const { return m_cancelled.loadRelaxed() != 0; }

    // May be called from any thread, once. The response is deleted soon afterwards.
    void finish(const QImage &image)
    {
        m_image = image;
        emit finished();
    }

private:
    QImage m_image;
    QAtomicInt m_cancelled;
};

// Thumbnails are kept on disk under the user's cache folder, keyed by the file's full
// path and modification time: an edited or replaced file gets a new key and so a new
// thumbnail. Stale entries are never removed.
QString cacheFileFor(const QString &path)
{
    static const QString directory = [] {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
        QDir().mkpath(dir);
        return dir;
    }();
    const QFileInfo info(path);
    const QString key = info.absoluteFilePath() + u'|' + QString::number(info.lastModified().toMSecsSinceEpoch())
        + u'|' + QString::number(thumbnailWidth);
    return directory + u'/' + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
}

// JPEG is small and fast but has no alpha, so thumbnails with transparency go to PNG.
QImage readCached(const QString &path)
{
    const QString base = cacheFileFor(path);
    QImage image(base + ".jpg");
    if (image.isNull())
        image.load(base + ".png");
    return image;
}

void writeCached(const QString &path, const QImage &image)
{
    if (image.isNull())
        return;
    const QString base = cacheFileFor(path);
    if (image.hasAlphaChannel())
        image.save(base + ".png");
    else
        image.save(base + ".jpg", "JPG", 85);
}

QImage readImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && size.width() > thumbnailWidth)
        reader.setScaledSize(size.scaled(thumbnailWidth, thumbnailWidth, Qt::KeepAspectRatio));
    return reader.read();
}

// The threads that make thumbnails: as many as half the processor's threads, so that
// the window, and a video that is playing, always have the other half.
//
// They run at ordinary priority. Running them at idle priority, to be sure of never
// taking time from a show, was tried and is a trap: Qt scales large images on a pool of
// threads of its own, a thread started from an idle-priority thread is idle-priority
// too, and Qt keeps those threads for the window's own image work, which then crawls
// whenever the machine is busy.
QThreadPool &workers()
{
    static QThreadPool pool;
    static const bool ready = [] {
        pool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 4));
        // Closing the app must not wait for a queue of thumbnails. What is waiting is
        // dropped, and the few being made are let finish while their requests are
        // still there to be answered.
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] {
            closing.storeRelaxed(1);
            pool.clear();
            pool.waitForDone();
        }, Qt::DirectConnection);
        return true;
    }();
    Q_UNUSED(ready)
    return pool;
}

} // namespace

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &)
{
    const QString path = QUrl::fromPercentEncoding(id.section(u'?', 0, 0).toUtf8());
    auto *response = new Response;
    workers().start([path, response] {
        QImage image;
        if (!response->cancelled() && !closing.loadRelaxed()) {
            image = readCached(path);
            if (image.isNull()) {
                image = workspace::isVideo(path) ? grabVideoFrame(path, thumbnailWidth) : readImage(path);
                writeCached(path, image);
            }
        }
        response->finish(image);
    });
    return response;
}

void ThumbnailProvider::discard(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QString base = cacheFileFor(path);
        QFile::remove(base + ".jpg");
        QFile::remove(base + ".png");
    }
}
