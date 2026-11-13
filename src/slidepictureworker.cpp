#include "slidepictureworker.h"

#include "iconprovider.h"
#include "messagetext.h"
#include "prodocument.h"
#include "slidepictures.h"
#include "thumbnailprovider.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>

#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <deque>

namespace slidepictures {

namespace {

const char *const workerFlag = "--slide-picture-worker";
// How long a picture waits for what is in it to load before it is taken as it is
constexpr int mostWaitMs = 4000;
// The cache's size: pruned to the lower when it has grown past the higher
constexpr qint64 cacheMost = 200LL * 1024 * 1024;
constexpr qint64 cacheKept = 150LL * 1024 * 1024;

// The processor only when nothing else wants it, and the disk likewise (see the header).
// Before anything else, so that every thread made after takes it too.
void lowerPriority()
{
    sched_param param {};
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_IDLE, &param);
    setpriority(PRIO_PROCESS, 0, 19);
    // (ioprio_set: who 1 is a process, 0 this one; the class in the top bits, 3 idle)
    syscall(SYS_ioprio_set, 1, 0, 3 << 13);
}

void writeAll(const QByteArray &bytes)
{
    qsizetype done = 0;
    while (done < bytes.size()) {
        const ssize_t wrote = ::write(STDOUT_FILENO, bytes.constData() + done, size_t(bytes.size() - done));
        if (wrote <= 0)
            return;
        done += wrote;
    }
}

void say(const QString &what)
{
    const QByteArray line = "slide pictures: " + what.toUtf8() + '\n';
    [[maybe_unused]] const ssize_t ignored = ::write(STDERR_FILENO, line.constData(), size_t(line.size()));
}

QString cacheFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/slide-pictures");
}

// The oldest pictures let go of until the cache is back under its size (only its own
// pictures, by name, in its own folder)
void pruneCache()
{
    QDir folder(cacheFolder());
    if (!folder.exists())
        return;
    QFileInfoList files = folder.entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.png")}, QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const QFileInfo &file : std::as_const(files))
        total += file.size();
    if (total <= cacheMost)
        return;
    for (const QFileInfo &file : std::as_const(files)) {
        if (total <= cacheKept)
            break;
        total -= file.size();
        folder.remove(file.fileName());
    }
}

class Worker
{
public:
    Worker(QQuickView *view, const QString &root)
        : m_view(view)
        , m_root(root)
        , m_input(STDIN_FILENO, QSocketNotifier::Read)
    {
        QObject::connect(&m_input, &QSocketNotifier::activated, [this] { readInput(); });
        m_poll.setInterval(15);
        QObject::connect(&m_poll, &QTimer::timeout, [this] { checkDrawn(); });
    }

private:
    struct Job
    {
        qint64 id = 0;
        Asked asked;
        QString cacheName;
        QElapsedTimer waited;
    };

    void readInput()
    {
        char chunk[4096];
        const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof chunk);
        if (got <= 0) {
            // (The app has closed it, or gone)
            m_input.setEnabled(false);
            QCoreApplication::quit();
            return;
        }
        m_buffer.append(chunk, got);
        qsizetype end;
        while ((end = m_buffer.indexOf('\n')) >= 0) {
            const QByteArray line = m_buffer.left(end);
            m_buffer.remove(0, end + 1);
            Job job;
            if (readAsked(line, &job.id, &job.asked))
                m_queue.push_back(job);
        }
        next();
    }

    void answer(const Job &job, int status, const QByteArray &type = {}, const QByteArray &bytes = {}, const QString &error = {})
    {
        Answer made;
        made.id = job.id;
        made.status = status;
        made.type = type;
        made.length = bytes.size();
        made.error = error;
        writeAll(answerLine(made) + bytes);
    }

    void next()
    {
        while (!m_busy && !m_queue.empty()) {
            Job job = m_queue.front();
            m_queue.pop_front();
            start(job);
        }
    }

    // The slide found, and its picture taken from the cache, or put in the window to be drawn
    void start(Job job)
    {
        const QFileInfo file(job.asked.file);
        if (!file.isFile()) {
            answer(job, 404, {}, {}, QStringLiteral("no such file"));
            return;
        }
        const qint64 changed = file.lastModified().toMSecsSinceEpoch();
        const QString key = job.asked.file + u'\n' + QString::number(changed) + u'\n' + (job.asked.arrangement ? *job.asked.arrangement : QStringLiteral("\x01"));
        if (key != m_documentKey) {
            QString error;
            const ProDocument document = ProDocument::load(job.asked.file, m_root, job.asked.arrangement, &error);
            m_slides = error.isEmpty() ? document.slides : QVariantList();
            m_documentKey = key;
        }
        if (job.asked.index < 0 || job.asked.index >= m_slides.size()) {
            answer(job, 404, {}, {}, QStringLiteral("no such cue"));
            return;
        }
        const QVariantMap slide = m_slides.at(job.asked.index).toMap();
        const QVariantMap media = slide.value(QStringLiteral("media")).toMap();
        const QString mediaPath = media.value(QStringLiteral("path")).toString();
        const qint64 mediaChanged = mediaPath.isEmpty() ? 0 : QFileInfo(mediaPath).lastModified().toMSecsSinceEpoch();
        job.cacheName = cacheName(job.asked, changed, mediaPath, mediaChanged, QCoreApplication::applicationVersion());
        const QByteArray type = job.asked.png ? QByteArray("image/png") : QByteArray("image/jpeg");

        QFile kept(cacheFolder() + u'/' + job.cacheName);
        if (kept.open(QIODevice::ReadOnly)) {
            answer(job, 200, type, kept.readAll());
            return;
        }

        const QSize size = sizeFor(slide.value(QStringLiteral("width")).toDouble(), slide.value(QStringLiteral("height")).toDouble(), job.asked.quality);
        QQuickItem *root = m_view->rootObject();
        m_view->resize(size);
        root->setProperty("square", squareFor(size.width()));
        root->setProperty("media", media.isEmpty() ? QVariant() : QVariant(media));
        root->setProperty("slide", slide);
        m_busy = true;
        m_job = job;
        m_job.waited.start();
        m_poll.start();
    }

    // Taken once everything in it has loaded (the slide's pictures, the media's), and
    // after a turn of the loop at least, for the loading to begin
    void checkDrawn()
    {
        const bool ready = m_view->rootObject()->property("ready").toBool();
        if (!(ready && m_job.waited.elapsed() >= 30) && m_job.waited.elapsed() < mostWaitMs)
            return;
        m_poll.stop();
        if (!ready)
            say(QStringLiteral("cue %1 of %2 was taken before everything in it had loaded").arg(m_job.asked.index).arg(QFileInfo(m_job.asked.file).fileName()));
        QImage image = m_view->grabWindow();
        // (Should the desktop's scale have come in all the same, the picture is brought to
        // the size asked for)
        if (!image.isNull() && image.size() != m_view->size())
            image = image.scaled(m_view->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QByteArray type = m_job.asked.png ? QByteArray("image/png") : QByteArray("image/jpeg");
        if (image.isNull()) {
            answer(m_job, 500, {}, {}, QStringLiteral("the slide could not be drawn"));
        } else {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.convertToFormat(QImage::Format_RGB32).save(&buffer, m_job.asked.png ? "PNG" : "JPG", m_job.asked.png ? -1 : 85);
            QDir().mkpath(cacheFolder());
            QSaveFile kept(cacheFolder() + u'/' + m_job.cacheName);
            if (kept.open(QIODevice::WriteOnly)) {
                kept.write(bytes);
                kept.commit();
            }
            answer(m_job, 200, type, bytes);
        }
        m_busy = false;
        next();
    }

    QQuickView *m_view;
    QString m_root;
    QSocketNotifier m_input;
    QByteArray m_buffer;
    std::deque<Job> m_queue;
    bool m_busy = false;
    Job m_job;
    QTimer m_poll;
    // The presentation read last, and by what it is known
    QString m_documentKey;
    QVariantList m_slides;
};

}

bool isWorker(int argc, char *argv[])
{
    return argc >= 2 && qstrcmp(argv[1], workerFlag) == 0;
}

int runWorker(int argc, char *argv[])
{
    lowerPriority();
    // (A picture is of the size asked for in pixels, whatever the desktop's scale: a
    // window on a screen scaled by 2 would draw it at twice that)
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
    qunsetenv("QT_SCALE_FACTOR");
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("SimplePresenter");
    QGuiApplication::setApplicationName("SimplePresenter");
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const QString root = argc >= 3 ? QString::fromLocal8Bit(argv[2]) : QString();
    pruneCache();

    QQmlEngine engine;
    engine.addImageProvider("thumbnail", new ThumbnailProvider);
    engine.addImageProvider("icon", new IconProvider);
    engine.addImageProvider("token", new TokenPictures);
    QQuickView view(&engine, nullptr);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setColor(Qt::black);
    view.resize(400, 225);
    view.loadFromModule("SimplePresenterApp", "SlidePictureMaker");
    if (view.status() != QQuickView::Ready || !view.rootObject()) {
        for (const QQmlError &error : view.errors())
            say(error.toString());
        return 1;
    }
    Worker worker(&view, root);
    say(QStringLiteral("ready"));
    return app.exec();
}

}
