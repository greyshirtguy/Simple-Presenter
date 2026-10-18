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
#include <QTransform>
#include <QUrl>
#include <QUrlQuery>

namespace {

// Wide enough for the largest thumbnail the views show, on a screen scaled to 150%.
const int thumbnailWidth = 480;
// Which way of choosing a video's picture the cached thumbnails were made by: to be
// changed whenever grabVideoFrame() would give a different picture for the same file.
// (The first way, with no mark at all, took the very first frame.)
const QLatin1StringView videoFrameChoice("|five seconds in");

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
// thumbnail. A video's key also says which way of choosing its picture made it (see
// videoframe.h), so that when that changes the pictures made the old way are not the
// ones shown. Stale entries are never removed. A picture of the media as it is set to
// look in its Media Inspector (see the header) goes under the same name with a dash
// and a key of the look after it, so that discarding a file's pictures finds them all.
QString cacheFileFor(const QString &path, const QString &look)
{
    static const QString directory = [] {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails";
        QDir().mkpath(dir);
        return dir;
    }();
    const QFileInfo info(path);
    const QString key = info.absoluteFilePath() + u'|' + QString::number(info.lastModified().toMSecsSinceEpoch())
        + u'|' + QString::number(thumbnailWidth) + (workspace::isVideo(path) ? videoFrameChoice : QLatin1StringView());
    const QString base = directory + u'/' + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
    return look.isEmpty() ? base
                          : base + u'-' + QString::fromLatin1(QCryptographicHash::hash(look.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

// JPEG is small and fast but has no alpha, so thumbnails with transparency go to PNG.
QImage readCached(const QString &path, const QString &look)
{
    const QString base = cacheFileFor(path, look);
    QImage image(base + ".jpg");
    if (image.isNull())
        image.load(base + ".png");
    return image;
}

void writeCached(const QString &path, const QString &look, const QImage &image)
{
    if (image.isNull())
        return;
    const QString base = cacheFileFor(path, look);
    if (image.hasAlphaChannel())
        image.save(base + ".png");
    else
        image.save(base + ".jpg", "JPG", 85);
}

QImage readImage(const QString &path, QSize *upright)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && size.width() > thumbnailWidth)
        reader.setScaledSize(size.scaled(thumbnailWidth, thumbnailWidth, Qt::KeepAspectRatio));
    // (The size as stored; a photograph whose camera says it was held sideways is
    // turned upright as it is read.)
    *upright = size.isValid() && (reader.transformation() & QImageIOHandler::TransformationRotate90) ? size.transposed() : size;
    return reader.read();
}

// How the media is set to look, from what follows the counter in a thumbnail's id (see
// the header): the moment for a video's picture, the crop, the turn and the flips.
struct Look
{
    double at = -1;
    double left = 0, right = 0, top = 0, bottom = 0;
    int rotation = 0;
    bool flipAcross = false;
    bool flipDown = false;
    // The same, as it goes into the cache's key: "" for media set to nothing
    QString key;
};

Look lookOf(const QString &query)
{
    Look look;
    const QUrlQuery parts(query);
    if (parts.hasQueryItem("t"))
        look.at = parts.queryItemValue("t").toDouble();
    const QStringList crop = parts.queryItemValue("c").split(u',');
    if (crop.size() == 4) {
        look.left = qMax(0.0, crop.at(0).toDouble());
        look.right = qMax(0.0, crop.at(1).toDouble());
        look.top = qMax(0.0, crop.at(2).toDouble());
        look.bottom = qMax(0.0, crop.at(3).toDouble());
    }
    const int rotation = parts.queryItemValue("r").toInt();
    look.rotation = rotation == 90 || rotation == 180 || rotation == 270 ? rotation : 0;
    const QString flips = parts.queryItemValue("f");
    look.flipAcross = flips.contains(u'h');
    look.flipDown = flips.contains(u'v');
    for (const char *key : {"t", "c", "r", "f"}) {
        if (parts.hasQueryItem(key))
            look.key += QLatin1String(key) + u'=' + parts.queryItemValue(key) + u';';
    }
    return look;
}

// The picture made smaller, cropped, turned and flipped as the look says: the crop in
// pixels of the media at its own size (`upright`), measured on the picture made from it.
QImage looking(const QImage &image, const QSize &upright, const Look &look)
{
    if (image.isNull())
        return image;
    QImage result = image;
    if ((look.left > 0 || look.right > 0 || look.top > 0 || look.bottom > 0) && upright.width() > 0 && upright.height() > 0) {
        const double sx = image.width() / double(upright.width());
        const double sy = image.height() / double(upright.height());
        const QRect kept = QRectF(look.left * sx, look.top * sy, image.width() - (look.left + look.right) * sx,
                                  image.height() - (look.top + look.bottom) * sy).toAlignedRect().intersected(image.rect());
        if (kept.width() > 0 && kept.height() > 0)
            result = result.copy(kept);
    }
    if (look.rotation != 0)
        result = result.transformed(QTransform().rotate(look.rotation));
    if (look.flipAcross || look.flipDown)
        result = result.flipped((look.flipAcross ? Qt::Horizontal : Qt::Orientations()) | (look.flipDown ? Qt::Vertical : Qt::Orientations()));
    return result;
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
    const Look look = lookOf(id.section(u'?', 1));
    auto *response = new Response;
    workers().start([path, look, response] {
        QImage image;
        if (!response->cancelled() && !closing.loadRelaxed()) {
            image = readCached(path, look.key);
            if (image.isNull()) {
                QSize upright;
                const bool video = workspace::isVideo(path);
                image = video ? grabVideoFrame(path, thumbnailWidth, look.at, &upright) : readImage(path, &upright);
                image = looking(image, upright, look);
                writeCached(path, look.key, image);
            }
        }
        response->finish(image);
    });
    return response;
}

void ThumbnailProvider::discard(const QStringList &paths)
{
    // The pictures of the media as it is, and of it set to look otherwise.
    for (const QString &path : paths) {
        const QString base = cacheFileFor(path, {});
        QFile::remove(base + ".jpg");
        QFile::remove(base + ".png");
        const QFileInfo info(base);
        for (const QString &looked : QDir(info.absolutePath()).entryList({info.fileName() + "-*"}, QDir::Files))
            QFile::remove(info.absolutePath() + u'/' + looked);
    }
}
