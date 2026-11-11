#include "powerpointimport.h"

#include "pptxmake.h"
#include "pptxread.h"
#include "sessionlog.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <QSettings>
#include <QUrl>

namespace {

const QString leaveOutKey = QStringLiteral("pptxImport/leaveOutBackgrounds");

QString localPath(const QString &given)
{
    return given.startsWith(QLatin1String("file:")) ? QUrl(given).toLocalFile() : given;
}

} // namespace

PowerPointImport::PowerPointImport(QObject *parent) : QObject(parent), m_stopped(std::make_shared<std::atomic<bool>>(false))
{
    m_pool.setMaxThreadCount(1);
}

PowerPointImport::~PowerPointImport()
{
    // (An import going on when the app closes is stopped, and waited for: it may be
    // writing into the workspace, and cleans up after itself)
    m_stopped->store(true);
    m_pool.waitForDone();
}

bool PowerPointImport::leaveOutBackgrounds() const
{
    return QSettings().value(leaveOutKey, false).toBool();
}

void PowerPointImport::setLeaveOutBackgrounds(bool leaveOut)
{
    if (leaveOut == leaveOutBackgrounds())
        return;
    QSettings().setValue(leaveOutKey, leaveOut);
    emit leaveOutBackgroundsChanged();
}

QVariantMap PowerPointImport::describe(const QString &file) const
{
    const pptx::Glance glance = pptx::glanceAt(localPath(file));
    return {
        {"name", glance.name},
        {"slides", glance.slides},
        {"width", glance.size.width()},
        {"height", glance.size.height()},
        {"error", glance.error},
    };
}

QStringList PowerPointImport::filesAmong(const QList<QUrl> &files) const
{
    QStringList paths;
    for (const QUrl &file : files) {
        const QString path = file.toLocalFile();
        if (pptx::isPowerPointFile(path) && QFileInfo(path).isFile())
            paths << path;
    }
    return paths;
}

void PowerPointImport::setProgress(double progress)
{
    if (qFuzzyCompare(progress + 1, m_progress + 1))
        return;
    m_progress = progress;
    emit progressChanged();
}

bool PowerPointImport::start(const QString &given, const QString &workspace, const QString &library)
{
    if (m_busy)
        return false;
    const QString file = localPath(given);
    pptx::MakeOptions options;
    options.leaveOutBackgrounds = leaveOutBackgrounds();
    m_stopped = std::make_shared<std::atomic<bool>>(false);
    m_busy = true;
    m_progress = 0;
    emit busyChanged();
    emit progressChanged();

    const std::shared_ptr<std::atomic<bool>> stopped = m_stopped;
    const QPointer<PowerPointImport> self(this);
    m_pool.start([self, stopped, file, workspace, library, options]() {
        QElapsedTimer timer;
        timer.start();
        const QString fileName = QFileInfo(file).fileName();
        QVariantMap result {{"ok", false}, {"name", QFileInfo(file).completeBaseName()}, {"path", QString()}, {"slides", 0}};
        const auto report = [self](const std::function<void(PowerPointImport *)> &call) {
            // (Through the thread the windows run on, where `self` lives, and only if it
            // still does)
            if (self)
                QMetaObject::invokeMethod(self.data(), [self, call]() {
                    if (self)
                        call(self.data());
                }, Qt::QueuedConnection);
        };
        // Reading the file is the first part of the work, making the presentation and
        // copying its media the second
        pptx::ReadResult read = pptx::readDeck(file, [&](int done, int total) {
            report([done, total](PowerPointImport *i) { i->setProgress(0.5 * done / std::max(1, total)); });
            return !stopped->load();
        });
        QString error = read.error;
        pptx::NoteCollector notes;
        for (const pptx::Note &note : read.notes) {
            if (note.slides.isEmpty())
                notes.add(note.kind, note.message);
            for (int slide : note.slides)
                notes.add(note.kind, note.message, slide);
        }
        if (error.isEmpty()) {
            const pptx::Made made = pptx::makePresentation(read.deck, read.deck.title, library, workspace, options, &notes, [&](int done, int total) {
                report([done, total](PowerPointImport *i) { i->setProgress(0.5 + 0.5 * done / std::max(1, total)); });
                return !stopped->load();
            });
            error = made.error;
            if (error.isEmpty()) {
                result.insert("ok", true);
                result.insert("path", made.path);
                result.insert("name", made.name);
                result.insert("slides", made.slides);
            }
        }
        if (stopped->load() && error.isEmpty())
            error = QStringLiteral("The import was stopped");
        QVariantList list;
        for (const pptx::Note &note : notes.all())
            list << QVariantMap {{"message", note.message}, {"slides", note.slidesText()}};
        result.insert("notes", list);
        result.insert("error", error);
        if (error.isEmpty()) {
            SessionLog::write("import", QStringLiteral("%1 came in from PowerPoint as \"%2\": %3 slides, %4 notes, in %5 ms")
                                            .arg(fileName, result.value("name").toString())
                                            .arg(result.value("slides").toInt())
                                            .arg(list.size())
                                            .arg(timer.elapsed()));
        } else {
            SessionLog::write("import", QStringLiteral("%1 was not imported from PowerPoint: %2").arg(fileName, error));
        }
        report([result](PowerPointImport *i) {
            i->m_busy = false;
            i->setProgress(1);
            emit i->busyChanged();
            emit i->finished(result);
        });
    });
    return true;
}

void PowerPointImport::stop()
{
    m_stopped->store(true);
}
