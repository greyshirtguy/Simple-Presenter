#include "benchmark.h"

#include "proconvert.h"
#include "screenfile.h"
#include "stage.pb.h"
#include "workspacefiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQuickWindow>
#include <QScreen>
#include <QSysInfo>
#include <QTextStream>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWindow>

#include <algorithm>
#include <ctime>
#include <unistd.h>

namespace {

using proconvert::applyChanges;

// A field of /proc/self/status, in megabytes ("VmRSS", "VmHWM")
double statusMegabytes(const QByteArray &field)
{
    QFile status("/proc/self/status");
    if (!status.open(QIODevice::ReadOnly))
        return 0;
    for (const QByteArray &line : status.readAll().split('\n')) {
        if (line.startsWith(field + ':'))
            return line.mid(field.size() + 1).trimmed().split(' ').value(0).toDouble() / 1024;
    }
    return 0;
}

// A picture for the benchmark's slides: a wash of colour with rings on it, so that it
// has both smooth parts and edges, as a photograph has.
bool writePicture(const QString &path, int hue)
{
    QImage image(1920, 1080, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient wash(0, 0, 1920, 1080);
    wash.setColorAt(0, QColor::fromHsv(hue, 200, 230));
    wash.setColorAt(1, QColor::fromHsv((hue + 70) % 360, 220, 70));
    painter.fillRect(image.rect(), wash);
    for (int i = 0; i < 40; ++i) {
        painter.setPen(QPen(QColor::fromHsv((hue + i * 9) % 360, 120, 255, 150), 6));
        painter.drawEllipse(QPointF(160 + (i * 397) % 1600, 120 + (i * 211) % 840), 40 + i * 6, 40 + i * 6);
    }
    painter.end();
    return image.save(path, "JPG", 90);
}

// A new element's id
QString idOf(const rv::data::Slide::Element &element)
{
    return QString::fromStdString(element.element().uuid().string());
}

// Adds a text box of these words to a slide, across the slide at this height.
void addWords(rv::data::Slide *slide, const QString &words, double y, double height, double size)
{
    rv::data::Slide::Element made = proconvert::makeTextElement(*slide, nullptr);
    TextRun format = proconvert::readText(made.element().text()).firstRun();
    format.size = size;
    format.fill = Qt::white;
    proconvert::writeText(made.mutable_element()->mutable_text(), RichText::plain(words, format, Qt::AlignHCenter));
    const QString id = idOf(made);
    *slide->add_elements() = made;
    applyChanges(slide, id, {{"x", 120.0}, {"y", y}, {"width", 1680.0}, {"height", height}});
}

// Adds a ticker to a slide: a text box of these words along its foot, scrolling to the
// left over and over (Scrolling Text: see textscroller.h), at ProPresenter's middle speed.
void addTicker(rv::data::Slide *slide, const QString &words)
{
    addWords(slide, words, 950, 90, 56);
    const QString id = idOf(slide->elements(slide->elements_size() - 1));
    applyChanges(slide, id, {{"scrollOn", true}, {"scrollDirection", "left"}, {"scrollRate", 0.5}});
}

// Adds a shape to a slide, in this box, changed as asked.
void addShape(rv::data::Slide *slide, const QString &shape, const QRectF &box, QVariantMap changes = {})
{
    const rv::data::Slide::Element made = proconvert::makeShapeElement(*slide, shape);
    const QString id = idOf(made);
    *slide->add_elements() = made;
    changes.insert("x", box.x());
    changes.insert("y", box.y());
    changes.insert("width", box.width());
    changes.insert("height", box.height());
    applyChanges(slide, id, changes);
}

struct Group
{
    const char *name;
    const char *color;
};

// Writes a presentation of `count` slides, each filled in by `fill`, in groups of six
// under the names a song's groups have.
QString writePresentation(const QString &path, const QString &name, int count,
                          const std::function<void(rv::data::Presentation *, rv::data::Cue *, rv::data::Slide *, int)> &fill)
{
    static const Group groups[] = {{"Verse 1", "#1e88e5"}, {"Chorus", "#d81b60"}, {"Verse 2", "#1e88e5"},
                                   {"Bridge", "#8e24aa"},  {"Tag", "#fb8c00"},    {"Ending", "#6d4c41"}};
    rv::data::Presentation presentation;
    presentation.mutable_uuid()->set_string(workspace::newUuid());
    presentation.set_name(name.toStdString());
    rv::data::Presentation::CueGroup *group = nullptr;
    for (int i = 0; i < count; ++i) {
        if (i % 6 == 0) {
            const Group &named = groups[(i / 6) % 6];
            group = presentation.add_cue_groups();
            group->mutable_group()->mutable_uuid()->set_string(workspace::newUuid());
            group->mutable_group()->set_name(named.name);
            proconvert::setColor(group->mutable_group()->mutable_color(), QColor(named.color));
            group->mutable_group()->mutable_hotkey();
        }
        rv::data::Cue *cue = proconvert::addBlankCue(&presentation, std::string(), QSizeF(1920, 1080));
        group->add_cue_identifiers()->set_string(cue->uuid().string());
        fill(&presentation, cue, cue->mutable_actions(0)->mutable_slide()->mutable_presentation()->mutable_base_slide(), i);
    }
    return proconvert::writePresentation(path, presentation);
}

} // namespace

Benchmark::Benchmark(const QString &reportPath, QObject *parent)
    : QObject(parent), m_reportPath(reportPath)
{
}

double Benchmark::sinceStart()
{
    // When the process started is kept by the kernel, in clock ticks since the machine
    // was started: the 22nd field of /proc/self/stat, counting from after the program's
    // name, which is in brackets and may itself have spaces in it.
    static const double started = [] {
        QFile stat("/proc/self/stat");
        if (!stat.open(QIODevice::ReadOnly))
            return 0.0;
        const QByteArray all = stat.readAll();
        const QList<QByteArray> fields = all.mid(all.lastIndexOf(')') + 2).split(' ');
        return fields.value(19).toDouble() * 1000.0 / double(sysconf(_SC_CLK_TCK));
    }();
    timespec now;
    clock_gettime(CLOCK_BOOTTIME, &now);
    return double(now.tv_sec) * 1000.0 + double(now.tv_nsec) / 1e6 - started;
}

VideoTiming::VideoTiming(QQuickWindow *window, QVideoSink *sink, QObject *parent)
    : QObject(parent), m_window(window), m_sink(sink)
{
    // (Each of these is run on the thread that sends it: the player's, and the output's
    // drawing thread. The VideoOutput connected itself to its sink when it was made,
    // before this was, so its own queued call is made before this one's and run first.)
    connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &) {
        const double now = Benchmark::sinceStart();
        qsizetype index;
        {
            QMutexLocker locked(&m_lock);
            index = m_arrived.size();
            m_arrived.append(now);
            m_seen.append(-1);
        }
        QMetaObject::invokeMethod(this, [this, index] {
            const double now = Benchmark::sinceStart();
            QMutexLocker locked(&m_lock);
            if (index < m_seen.size())
                m_seen[index] = now;
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeSynchronizing, this, [this] {
        const double now = Benchmark::sinceStart();
        QMutexLocker locked(&m_lock);
        m_syncFrom.append(now);
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::afterSynchronizing, this, [this] {
        const double now = Benchmark::sinceStart();
        QMutexLocker locked(&m_lock);
        m_syncTo.append(now);
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this, [this] {
        const double now = Benchmark::sinceStart();
        QMutexLocker locked(&m_lock);
        m_swapped.append(now);
    }, Qt::DirectConnection);
}

QVariantMap VideoTiming::summary()
{
    if (m_sink)
        disconnect(m_sink, nullptr, this, nullptr);
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    QMutexLocker locked(&m_lock);
    const QVector<double> &arrived = m_arrived;
    const QVector<double> &seen = m_seen;
    const qsizetype frames = arrived.size();
    QVariantMap result{{"frames", int(frames)}};
    if (frames < 3)
        return result;

    // How long a frame of the video lasts: the commonest gap between two coming
    std::vector<double> gaps;
    for (qsizetype i = 1; i < frames; ++i)
        gaps.push_back(arrived[i] - arrived[i - 1]);
    std::nth_element(gaps.begin(), gaps.begin() + gaps.size() / 2, gaps.end());
    const double interval = gaps[gaps.size() / 2];

    // Which frame each sync took: the newest that had come before it began, if the
    // app's thread had got to one since the sync before (the calls are run in the order
    // they were made, so `seen` only goes up)
    const qsizetype syncs = m_syncFrom.size();
    QVector<qsizetype> taken(syncs, -1);
    qsizetype seenUpTo = 0;
    qsizetype cameUpTo = 0;
    qsizetype shown = -1;
    for (qsizetype k = 0; k < syncs; ++k) {
        bool marked = false;
        while (seenUpTo < frames && seen[seenUpTo] >= 0 && seen[seenUpTo] < m_syncFrom[k]) {
            ++seenUpTo;
            marked = true;
        }
        while (cameUpTo < frames && arrived[cameUpTo] < m_syncFrom[k])
            ++cameUpTo;
        if (marked)
            shown = std::max(shown, cameUpTo - 1);
        taken[k] = shown;
    }
    // The frames the output drew for something else: that took no new frame of the video
    // (and when each began, for reading them against what was done)
    int others = 0;
    QVariantList othersAt;
    for (qsizetype k = 1; k < syncs; ++k) {
        if (taken[k] == taken[k - 1]) {
            ++others;
            othersAt.append(m_syncFrom[k]);
        }
    }

    // When each frame was first on the screen: at the first swap after a sync that took
    // it, or one after it; -1 for a frame no swap showed
    QVector<double> onScreen(frames, -1);
    qsizetype sync = -1;
    qsizetype highest = -1;
    int notShown = 0;
    for (const double swap : std::as_const(m_swapped)) {
        while (sync + 1 < syncs && m_syncFrom[sync + 1] < swap)
            ++sync;
        if (sync < 0 || taken[sync] <= highest)
            continue;
        notShown += int(taken[sync] - highest - 1);
        highest = taken[sync];
        onScreen[highest] = swap;
    }

    // How long each was on the screen: until the next one shown came
    int held = 0;
    double toScreen = 0;
    qsizetype last = -1;
    for (qsizetype i = 0; i < frames; ++i) {
        if (onScreen[i] < 0)
            continue;
        toScreen = std::max(toScreen, onScreen[i] - arrived[i]);
        if (last >= 0 && onScreen[i] - onScreen[last] > interval * 1.25)
            ++held;
        last = i;
    }
    double waitForThread = 0;
    for (qsizetype i = 0; i < frames; ++i) {
        if (seen[i] >= 0)
            waitForThread = std::max(waitForThread, seen[i] - arrived[i]);
    }
    double longestSync = 0;
    for (qsizetype k = 0; k < std::min(syncs, m_syncTo.size()); ++k)
        longestSync = std::max(longestSync, m_syncTo[k] - m_syncFrom[k]);

    result.insert("others", others);
    result.insert("othersAt", othersAt);
    result.insert("notShown", notShown);
    result.insert("held", held);
    result.insert("interval", interval);
    result.insert("waitForThread", waitForThread);
    result.insert("sync", longestSync);
    result.insert("toScreen", toScreen);
    return result;
}

MotionTiming::MotionTiming(QQuickWindow *window, QObject *item, const QByteArray &property, QObject *parent)
    : QObject(parent), m_window(window), m_item(item), m_property(property)
{
    connect(window, &QQuickWindow::beforeSynchronizing, this, [this] {
        // (The app's thread is waiting for the sync, so the item stands still.)
        const double where = m_item ? m_item->property(m_property.constData()).toDouble() : 0;
        QMutexLocker locked(&m_lock);
        m_where.append(where);
    }, Qt::DirectConnection);
}

QVariantMap MotionTiming::summary()
{
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    QMutexLocker locked(&m_lock);
    std::vector<double> steps;
    for (qsizetype i = 1; i < m_where.size(); ++i)
        steps.push_back(m_where[i] - m_where[i - 1]);
    QVariantMap result{{"frames", int(m_where.size())}};
    if (steps.size() < 10)
        return result;
    std::vector<double> sorted = steps;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double middle = sorted[sorted.size() / 2];
    int uneven = 0;
    int stopped = 0;
    for (const double step : steps) {
        if (qAbs(step - middle) > qAbs(middle) * 0.25)
            ++uneven;
        if (step == 0 && middle != 0)
            ++stopped;
    }
    result.insert("uneven", uneven);
    result.insert("stopped", stopped);
    return result;
}

void Benchmark::watchMotion(QObject *window, QObject *item, const QString &property)
{
    delete m_motion;
    m_motion = nullptr;
    if (auto *quickWindow = qobject_cast<QQuickWindow *>(window); quickWindow && item)
        m_motion = new MotionTiming(quickWindow, item, property.toUtf8(), this);
}

QVariantMap Benchmark::motionTiming()
{
    if (!m_motion)
        return {};
    const QVariantMap result = m_motion->summary();
    m_motion->deleteLater();
    m_motion = nullptr;
    return result;
}

void Benchmark::watchVideo(QObject *window, QObject *sink)
{
    delete m_video;
    m_video = nullptr;
    auto *quickWindow = qobject_cast<QQuickWindow *>(window);
    auto *videoSink = qobject_cast<QVideoSink *>(sink);
    if (quickWindow && videoSink)
        m_video = new VideoTiming(quickWindow, videoSink, this);
}

QVariantMap Benchmark::videoTiming(const QVariantList &marks)
{
    if (!m_video)
        return {};
    const QVariantMap result = m_video->summary();
    const QString folder = qEnvironmentVariable("SP_BENCHMARK_VIDEO_TRACE");
    if (!folder.isEmpty()) {
        static int traces = 0;
        QFile trace(QStringLiteral("%1/video-%2.txt").arg(folder).arg(++traces));
        if (trace.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream out(&trace);
            const auto line = [&out](const char *name, const QVector<double> &values) {
                out << name;
                for (const double value : values)
                    out << " " << QString::number(value, 'f', 3);
                out << "\n";
            };
            QVector<double> marked;
            for (const QVariant &mark : marks)
                marked.append(mark.toDouble());
            line("marks", marked);
            line("arrived", m_video->arrivedTimes());
            line("seen", m_video->seenTimes());
            line("syncFrom", m_video->syncFromTimes());
            line("syncTo", m_video->syncToTimes());
            line("swapped", m_video->swappedTimes());
        }
    }
    m_video->deleteLater();
    m_video = nullptr;
    return result;
}

QObject *Benchmark::window(const QString &name) const
{
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *candidate : windows) {
        if (candidate->objectName() == name)
            return candidate;
    }
    return nullptr;
}

double Benchmark::cpu() const
{
    timespec used;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &used);
    return double(used.tv_sec) * 1000.0 + double(used.tv_nsec) / 1e6;
}

double Benchmark::memory() const
{
    return statusMegabytes("VmRSS");
}

double Benchmark::peakMemory() const
{
    return statusMegabytes("VmHWM");
}

void Benchmark::record(const QString &name, double value, const QString &unit)
{
    const QString line = QStringLiteral("%1\t%2\t%3").arg(name, QString::number(value, 'f', value < 100 ? 1 : 0), unit);
    m_lines.append(line);
    QTextStream(stdout) << line << "\n";
}

void Benchmark::finish()
{
    QFile report(m_reportPath);
    if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&report);
        out << "# SimplePresenter " << QCoreApplication::applicationVersion() << ", Qt " << qVersion() << ", "
            << QSysInfo::prettyProductName() << ", " << QGuiApplication::platformName() << "\n";
        for (const QScreen *screen : QGuiApplication::screens()) {
            out << "# screen " << screen->name() << " " << screen->size().width() << "x" << screen->size().height() << " at "
                << qRound(screen->refreshRate()) << " Hz, scale " << screen->devicePixelRatio() << "\n";
        }
        for (const QString &line : std::as_const(m_lines))
            out << line << "\n";
    } else {
        qWarning("Could not write %s", qPrintable(m_reportPath));
    }
    QCoreApplication::quit();
}

QString Benchmark::makeWorkspace(const QString &folder)
{
    const QString library = folder + "/Libraries/Benchmark";
    const QString media = folder + "/Media";
    if (!QDir().mkpath(library) || !QDir().mkpath(media))
        return QStringLiteral("Could not make the benchmark's workspace in %1").arg(folder);
    QStringList pictures;
    for (int i = 0; i < 4; ++i) {
        pictures.append(QStringLiteral("%1/Picture %2.jpg").arg(media).arg(i + 1));
        if (!writePicture(pictures.last(), i * 85))
            return QStringLiteral("Could not write %1").arg(pictures.last());
    }

    // Words: what most slides are. Three or four lines of large text.
    static const char *const lines[] = {"The quick brown fox jumps", "over the lazy dog again", "and every good boy",
                                        "deserves fruit in the morning", "while pack my box", "with five dozen jugs",
                                        "how vexingly quick", "daft zebras jump today"};
    const auto words = [](int slide, int count) {
        QStringList chosen;
        for (int i = 0; i < count; ++i)
            chosen.append(QString::fromLatin1(lines[(slide * 3 + i) % 8]));
        return chosen.join(QChar('\n'));
    };
    QString error = writePresentation(library + "/Words.pro", "Words", 36,
                                      [&](rv::data::Presentation *, rv::data::Cue *, rv::data::Slide *slide, int i) {
        addWords(slide, words(i, 3 + i % 2), 190, 700, 96);
    });
    if (!error.isEmpty())
        return error;

    // Shapes: one of each kind the app draws, with each kind of fill, and a line of
    // words. This is the slide that costs most to draw.
    error = writePresentation(library + "/Shapes.pro", "Shapes", 18,
                              [&](rv::data::Presentation *, rv::data::Cue *, rv::data::Slide *slide, int i) {
        const double shift = (i % 6) * 20;
        addShape(slide, "rectangle", QRectF(80 + shift, 80, 420, 300));
        addShape(slide, "roundedRectangle", QRectF(560 + shift, 80, 520, 300),
                 {{"fillKind", "gradient"}, {"fillGradientFrom", QColor("#ffe061")}, {"fillGradientTo", QColor("#02c4fa")},
                  {"fillGradientAngle", 315.0}});
        addShape(slide, "ellipse", QRectF(1140 + shift, 80, 420, 300), {{"featherOn", true}, {"featherRadius", 0.12}});
        addShape(slide, "arrow", QRectF(80 + shift, 440, 420, 220));
        addShape(slide, "roundedRectangle", QRectF(560 + shift, 440, 520, 300),
                 {{"fillMediaPath", pictures.at(i % 4)}, {"fillMediaScale", 1}});
        addShape(slide, "ellipse", QRectF(1140 + shift, 440, 420, 300),
                 {{"fillMediaPath", pictures.at((i + 1) % 4)}, {"fillMediaScale", 1}, {"featherOn", true}, {"featherRadius", 0.08}});
        addWords(slide, words(i, 1), 800, 200, 80);
    });
    if (!error.isEmpty())
        return error;

    // Pictures: words over a picture that the slide brings with it as its background.
    const auto background = [&folder](rv::data::Cue *cue, const QString &file) {
        rv::data::Action *action = cue->add_actions();
        action->mutable_uuid()->set_string(workspace::newUuid());
        action->set_isenabled(true);
        action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        action->mutable_media()->mutable_audio();
        *action->mutable_media()->mutable_element() = workspace::mediaElement(file, folder);
        workspace::setMediaForeground(action, false);
    };
    error = writePresentation(library + "/Pictures.pro", "Pictures", 18,
                              [&](rv::data::Presentation *, rv::data::Cue *cue, rv::data::Slide *slide, int i) {
        addWords(slide, words(i, 2), 340, 400, 110);
        background(cue, pictures.at(i % 4));
    });
    if (!error.isEmpty())
        return error;

    // Scrolling: words with a ticker along the foot of the slide, what a notice or a
    // lower third that scrolls is
    error = writePresentation(library + "/Scrolling.pro", "Scrolling", 6,
                              [&](rv::data::Presentation *, rv::data::Cue *, rv::data::Slide *slide, int i) {
        addWords(slide, words(i, 2), 340, 400, 110);
        addTicker(slide, words(i + 1, 1) + QStringLiteral("  ·  ") + words(i + 2, 1) + QStringLiteral("  ·  ") + words(i + 3, 1));
    });
    if (!error.isEmpty())
        return error;

    // Video: words over a video that the first slide brings with it as its background,
    // and that goes on playing, round and round, under the rest. Only where
    // benchmark/run.py has made a video and said where it is: the app has no way of
    // making one itself (Qt's own recorder would not, on the computer this is made on).
    // It is put beside Media and not in it: what is in Media is in the media bin, whose
    // thumbnail of a video would be made while the app starts, and change what the
    // start-up lines measure.
    const QString video = qEnvironmentVariable("SP_BENCHMARK_VIDEO");
    if (!video.isEmpty() && QFileInfo(video).isFile()) {
        const QString copied = folder + "/Video.mp4";
        if (!QFile::copy(video, copied))
            return QStringLiteral("Could not copy %1 to %2").arg(video, copied);
        error = writePresentation(library + "/Video.pro", "Video", 12,
                                  [&](rv::data::Presentation *, rv::data::Cue *cue, rv::data::Slide *slide, int i) {
            addWords(slide, words(i, 2), 340, 400, 110);
            if (i == 0)
                background(cue, copied);
        });
        if (!error.isEmpty())
            return error;
    }

    // A stage layout of one shape, filled with a Screen Preview of the output: as big as
    // most of the stage screen, as ProPresenter's own stage layouts have theirs, so that
    // what a picture of the output costs is not made to look less than it is.
    rv::data::Stage::Document stage;
    rv::data::Stage::Layout *layout = stage.add_layouts();
    layout->mutable_uuid()->set_string(workspace::newUuid());
    layout->set_name("Screen Preview");
    rv::data::Slide *slide = layout->mutable_slide();
    slide->mutable_uuid()->set_string(workspace::newUuid());
    slide->set_draws_background_color(true);
    proconvert::setColor(slide->mutable_background_color(), Qt::black);
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    const screenfile::Screen output = screenfile::defaults().first();
    addShape(slide, "rectangle", QRectF(0, 180, 1440, 810),
             {{"fillKind", "screen"}, {"fillScreenId", output.id}, {"fillScreenName", output.name}, {"fillOn", true}});
    if (!QDir().mkpath(folder + "/Configuration"))
        return QStringLiteral("Could not make the benchmark's Configuration folder in %1").arg(folder);
    return workspace::writeMessage(folder + "/Configuration/Stage", stage, QStringLiteral("the stage layouts"));
}
