#include "pptxexport.h"

#include "pptximport.h"
#include "pptxwriter.h"
#include "sessionlog.h"
#include "videoframe.h"
#include "workspacefiles.h"

#include "presentation.pb.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

const QString prefix = QStringLiteral("pptx/");

pptx::Options optionsFromSettings()
{
    QSettings settings;
    pptx::Options options;
    options.carryBackgroundForward = settings.value(prefix + "carryBackgroundForward", options.carryBackgroundForward).toBool();
    options.skipDisabledSlides = settings.value(prefix + "skipDisabledSlides", options.skipDisabledSlides).toBool();
    options.includeNotes = settings.value(prefix + "includeNotes", options.includeNotes).toBool();
    options.groupsAsSections = settings.value(prefix + "groupsAsSections", options.groupsAsSections).toBool();
    options.followArrangement = settings.value(prefix + "followArrangement", options.followArrangement).toBool();
    options.backgroundPlacement = settings.value(prefix + "backgroundPlacement").toString() == u"pictureAtBack"
                                      ? pptx::Options::PictureAtBack
                                      : pptx::Options::SlideBackground;
    const QString videos = settings.value(prefix + "videos").toString();
    options.videos = videos == u"stillOnly"   ? pptx::Options::StillOnly
                   : videos == u"leaveOut" ? pptx::Options::LeaveOut
                                           : pptx::Options::Embed;
    return options;
}

QString localPath(const QString &given)
{
    return given.startsWith(QLatin1String("file:")) ? QUrl(given).toLocalFile() : given;
}

} // namespace

PptxExport::PptxExport(QObject *parent) : QObject(parent), m_stopped(std::make_shared<std::atomic<bool>>(false))
{
    m_pool.setMaxThreadCount(1);
}

PptxExport::~PptxExport()
{
    // (An export going on when the app closes is stopped, and waited for: it may be
    // writing beside its destination, and cleans up after itself)
    m_stopped->store(true);
    m_pool.waitForDone();
}

QVariantMap PptxExport::options() const
{
    const pptx::Options options = optionsFromSettings();
    return {
        {"carryBackgroundForward", options.carryBackgroundForward},
        {"skipDisabledSlides", options.skipDisabledSlides},
        {"includeNotes", options.includeNotes},
        {"groupsAsSections", options.groupsAsSections},
        {"followArrangement", options.followArrangement},
        {"backgroundPlacement", options.backgroundPlacement == pptx::Options::PictureAtBack ? QStringLiteral("pictureAtBack")
                                                                                           : QStringLiteral("slideBackground")},
        {"videos", options.videos == pptx::Options::StillOnly ? QStringLiteral("stillOnly")
                   : options.videos == pptx::Options::LeaveOut ? QStringLiteral("leaveOut")
                                                               : QStringLiteral("embed")},
    };
}

void PptxExport::setOption(const QString &name, const QVariant &value)
{
    if (!options().contains(name))
        return;
    QSettings().setValue(prefix + name, value);
}

QString PptxExport::suggestedFile(const QString &presentationName) const
{
    QString safe = presentationName.trimmed();
    for (const QChar bad : {u'/', u':', u'\\'})
        safe.replace(bad, u'-');
    if (safe.startsWith(u'.'))
        safe[0] = u'-';
    if (safe.isEmpty())
        safe = QStringLiteral("Presentation");
    QString folder = QSettings().value(prefix + "folder").toString();
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(folder).filePath(safe + QStringLiteral(".pptx"));
}

void PptxExport::setProgress(double progress)
{
    if (qFuzzyCompare(progress + 1, m_progress + 1))
        return;
    m_progress = progress;
    emit progressChanged();
}

bool PptxExport::start(const QString &file, const QString &workspace, const QString &arrangement, const QString &destination)
{
    if (m_busy)
        return false;
    QString path = localPath(destination);
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".pptx");
    QSettings().setValue(prefix + "folder", QFileInfo(path).absolutePath());
    const pptx::Options options = optionsFromSettings();
    m_stopped = std::make_shared<std::atomic<bool>>(false);
    m_busy = true;
    m_progress = 0;
    emit busyChanged();
    emit progressChanged();

    const std::shared_ptr<std::atomic<bool>> stopped = m_stopped;
    const QPointer<PptxExport> self(this);
    m_pool.start([self, stopped, file, workspace, arrangement, path, options]() {
        QElapsedTimer timer;
        timer.start();
        QVariantMap result {{"ok", false}, {"path", path}, {"name", QFileInfo(file).completeBaseName()}};
        const auto report = [self](const std::function<void(PptxExport *)> &call) {
            // (Through the thread the windows run on, where `self` lives, and only if it
            // still does)
            if (self)
                QMetaObject::invokeMethod(self.data(), [self, call]() {
                    if (self)
                        call(self.data());
                }, Qt::QueuedConnection);
        };
        QString error;
        rv::data::Presentation presentation;
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("%1 could not be read: %2").arg(QFileInfo(file).fileName(), in.errorString());
        } else {
            const QByteArray bytes = in.readAll();
            if (!presentation.ParseFromArray(bytes.constData(), int(bytes.size())))
                error = QStringLiteral("%1 is not a presentation this app can read").arg(QFileInfo(file).fileName());
        }
        if (error.isEmpty()) {
            // Media found as the app finds it, by one finder for the whole presentation
            auto finder = std::make_shared<workspace::FileFinder>(workspace, QStringLiteral("Media"));
            pptx::ImportContext context;
            context.findMedia = [finder](const rv::data::URL &url) { return finder->find(url); };
            context.videoStill = [](const QString &video, double *length) {
                return grabVideoFrame(video, 1920, -1, nullptr, length);
            };
            const pptx::ImportResult imported = importPresentation(presentation, QFileInfo(file).completeBaseName(), arrangement, options, context);
            report([](PptxExport *e) { e->setProgress(0.2); });
            pptx::NoteCollector notes;
            for (const pptx::Note &note : imported.notes) {
                if (note.slides.isEmpty())
                    notes.add(note.kind, note.message);
                for (int slide : note.slides)
                    notes.add(note.kind, note.message, slide);
            }
            const bool written = !stopped->load()
                                 && pptx::writeDeck(imported.deck, path, options, &notes, &error, [&](int done, int total) {
                                        report([done, total](PptxExport *e) { e->setProgress(0.2 + 0.8 * done / std::max(1, total)); });
                                        return !stopped->load();
                                    });
            if (stopped->load() && error.isEmpty())
                error = QStringLiteral("The export was stopped");
            result.insert("ok", written);
            result.insert("slides", int(imported.deck.slides.size()));
            QVariantList list;
            for (const pptx::Note &note : notes.all())
                list << QVariantMap {{"message", note.message}, {"slides", note.slidesText()}};
            result.insert("notes", list);
            if (written) {
                SessionLog::write("export", QStringLiteral("\"%1\" exported to PowerPoint as %2: %3 slides, %4 notes, in %5 ms")
                                                .arg(QFileInfo(file).completeBaseName(), path)
                                                .arg(imported.deck.slides.size())
                                                .arg(list.size())
                                                .arg(timer.elapsed()));
            }
        }
        result.insert("error", error);
        if (!error.isEmpty())
            SessionLog::write("export", QStringLiteral("\"%1\" was not exported to PowerPoint: %2").arg(QFileInfo(file).completeBaseName(), error));
        report([result](PptxExport *e) {
            e->m_busy = false;
            e->setProgress(1);
            emit e->busyChanged();
            emit e->finished(result);
        });
    });
    return true;
}

void PptxExport::stop()
{
    m_stopped->store(true);
}
