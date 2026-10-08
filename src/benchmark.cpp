#include "benchmark.h"

#include "proconvert.h"
#include "workspacefiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QSysInfo>
#include <QTextStream>

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
    return writePresentation(library + "/Pictures.pro", "Pictures", 18,
                             [&](rv::data::Presentation *, rv::data::Cue *cue, rv::data::Slide *slide, int i) {
        addWords(slide, words(i, 2), 340, 400, 110);
        rv::data::Action *background = cue->add_actions();
        background->mutable_uuid()->set_string(workspace::newUuid());
        background->set_isenabled(true);
        background->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        background->mutable_media()->mutable_audio();
        *background->mutable_media()->mutable_element() = workspace::mediaElement(pictures.at(i % 4), folder);
        workspace::setMediaForeground(background, false);
    });
}
