#pragma once

#include <QObject>
#include <QThreadPool>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Import PowerPoint, for the windows: a PowerPoint file (.pptx) made into a presentation
// of the workspace, slide for slide, its words as text that can be edited, its pictures
// as pictures, its slide backgrounds as the cues' background media (the maintainer's
// ask of 2026-11-09, and ProPresenter's own File > Import > PowerPoint as Presentation,
// which it has since version 21; what ProPresenter makes of each thing has not been
// looked at, and this is this app's own reading, see pptxmake.h). The reading
// (pptxread.h) and the making (pptxmake.h) run on a thread of their own, one import at a
// time, as the export does (pptxexport.h): a deck's pictures and videos are copied into
// the workspace, which is not for the thread that draws. The windows are told how far it
// has got, and at the end where the presentation is and what could not be carried over.
//
// The one option, leaving the slides' backgrounds out (as ProPresenter 22's import
// offers), is kept with the app's settings: it is this computer's, not the workspace's.
class PowerPointImport : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether an import is going on
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // How far it has got, from 0 to 1
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    // Whether the slides' backgrounds are to be left out, as last chosen
    Q_PROPERTY(bool leaveOutBackgrounds READ leaveOutBackgrounds WRITE setLeaveOutBackgrounds NOTIFY leaveOutBackgroundsChanged)

public:
    explicit PowerPointImport(QObject *parent = nullptr);
    ~PowerPointImport() override;

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    bool leaveOutBackgrounds() const;
    void setLeaveOutBackgrounds(bool leaveOut);

    // What a file holds, before it is imported: { name, slides, width, height, error }
    // (width and height in slide units, 1920 x 1080 for a 16:9 deck). Reads the file's
    // list of slides only, which is quick.
    Q_INVOKABLE QVariantMap describe(const QString &file) const;
    // Starts importing `file` (a path, or a file: address) into the library folder
    // `library` of `workspace`. False if an import is going on already.
    Q_INVOKABLE bool start(const QString &file, const QString &workspace, const QString &library);
    // Stops the import going on, which then finishes, having made nothing.
    Q_INVOKABLE void stop();

signals:
    void busyChanged();
    void progressChanged();
    void leaveOutBackgroundsChanged();
    // When an import is done, or has stopped: { ok, error, path, name, slides, notes },
    // `path` the presentation's file, `notes` a list of { message, slides } (slides as
    // "slides 2, 5 and 9")
    void finished(const QVariantMap &result);

private:
    void setProgress(double progress);

    QThreadPool m_pool;
    std::shared_ptr<std::atomic<bool>> m_stopped;
    bool m_busy = false;
    double m_progress = 0;
};
