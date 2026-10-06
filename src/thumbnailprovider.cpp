#include "thumbnailprovider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMediaPlayer>
#include <QPointer>
#include <QQueue>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace {

const int thumbnailWidth = 480;
const QStringList videoSuffixes = {"mp4", "mov", "m4v", "mkv", "webm", "avi"};

class Response : public QQuickImageResponse
{
public:
    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    // May be called from any thread, once.
    void finish(const QImage &image)
    {
        m_image = image;
        emit finished();
    }

private:
    QImage m_image;
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

// Decodes video thumbnails on the main thread's event loop, a couple at a time: each one
// opens a decoder, so a folder full of videos must not start them all at once.
class VideoThumbnailer : public QObject
{
public:
    static VideoThumbnailer *instance()
    {
        static VideoThumbnailer *thumbnailer = new VideoThumbnailer(QCoreApplication::instance());
        return thumbnailer;
    }

    void enqueue(const QString &path, Response *response)
    {
        m_queue.enqueue({path, response});
        startNext();
    }

private:
    struct Request
    {
        QString path;
        QPointer<Response> response;
    };

    using QObject::QObject;

    void startNext()
    {
        while (m_active < maxActive && !m_queue.isEmpty()) {
            const Request request = m_queue.dequeue();
            if (request.response)
                start(request);
        }
    }

    void start(const Request &request)
    {
        ++m_active;
        auto *job = new QObject(this);
        auto *player = new QMediaPlayer(job);
        auto *sink = new QVideoSink(job);
        auto *timeout = new QTimer(job);
        player->setVideoSink(sink);

        // `job` owns everything, so deleting it disconnects all of this; done at most once.
        const auto finish = [this, job, player, path = request.path, response = request.response](const QImage &image) {
            if (job->property("finished").toBool())
                return;
            job->setProperty("finished", true);
            player->stop();
            job->deleteLater();
            writeCached(path, image);
            if (response)
                response->finish(image);
            --m_active;
            startNext();
        };

        // Skip a little way in: the very first frame of a clip is often black or a fade-in.
        connect(player, &QMediaPlayer::mediaStatusChanged, job, [job, player](QMediaPlayer::MediaStatus status) {
            if (status != QMediaPlayer::LoadedMedia || job->property("started").toBool())
                return;
            job->setProperty("started", true);
            player->setPosition(qMin<qint64>(player->duration() / 10, 5000));
            player->play();
        });
        connect(sink, &QVideoSink::videoFrameChanged, job, [job, finish](const QVideoFrame &frame) {
            if (!job->property("started").toBool() || !frame.isValid())
                return;
            const QImage image = frame.toImage();
            if (!image.isNull())
                finish(image.scaledToWidth(thumbnailWidth, Qt::SmoothTransformation));
        });
        connect(player, &QMediaPlayer::errorOccurred, job, [finish] { finish({}); });
        connect(timeout, &QTimer::timeout, job, [finish] { finish({}); });

        timeout->setSingleShot(true);
        timeout->start(10000);
        player->setSource(QUrl::fromLocalFile(request.path));
    }

    static constexpr int maxActive = 2;
    QQueue<Request> m_queue;
    int m_active = 0;
};

} // namespace

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &)
{
    const QString path = QUrl::fromPercentEncoding(id.section(u'?', 0, 0).toUtf8());
    auto *response = new Response;

    const bool isVideo = videoSuffixes.contains(QFileInfo(path).suffix().toLower());
    QThreadPool::globalInstance()->start([path, isVideo, response] {
        QImage image = readCached(path);
        if (image.isNull() && isVideo) {
            // Video is decoded on the main thread's event loop.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [path, response] {
                VideoThumbnailer::instance()->enqueue(path, response);
            }, Qt::QueuedConnection);
            return;
        }
        if (image.isNull()) {
            image = readImage(path);
            writeCached(path, image);
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
