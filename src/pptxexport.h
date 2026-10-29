#pragma once

#include <QObject>
#include <QThreadPool>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Export to PowerPoint, for the windows: a presentation of the workspace written as a
// PowerPoint file (.pptx), slide for slide as ProPresenter shows it, with its words as
// text that can be edited. It is this app's own, and not ProPresenter's (which has no such
// export); it is the maintainer's ProToPPTX for the Mac (pptxdeck.h says how the two are
// related), brought into the app as a right click on a presentation.
//
// The presentation is read (pptximport.h) and written (pptxwriter.h) on a thread of its
// own, one export at a time: reading media and fonts, laying out text to fit it, and
// copying videos into the file are not for the thread that draws. The windows are told
// how far it has got, and at the end what could not be carried over (the report's
// notes), and where the file is.
//
// The options are ProToPPTX's (pptx::Options), kept with the app's settings, as is the
// folder last exported into: they are this computer's and this app's, not the workspace's.
class PptxExport : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether an export is going on
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // How far it has got, from 0 to 1
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit PptxExport(QObject *parent = nullptr);
    ~PptxExport() override;

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }

    // The options as last chosen, by name: carryBackgroundForward, skipDisabledSlides,
    // includeNotes, groupsAsSections and followArrangement (true or false),
    // backgroundPlacement ("slideBackground" or "pictureAtBack") and videos ("embed",
    // "stillOnly" or "leaveOut")
    Q_INVOKABLE QVariantMap options() const;
    Q_INVOKABLE void setOption(const QString &name, const QVariant &value);
    // Where a presentation of this name is offered to be exported to: its name as a
    // .pptx (with what a file's name cannot have put as "-"), in the folder last exported
    // into, or Documents
    Q_INVOKABLE QString suggestedFile(const QString &presentationName) const;
    // Starts exporting the presentation `file`, of `workspace`, following the arrangement
    // of that name ("" for Master), into `destination` (a path or a file: address), which
    // is given ".pptx" if it has no ending. False if an export is going on already.
    Q_INVOKABLE bool start(const QString &file, const QString &workspace, const QString &arrangement, const QString &destination);
    // Stops the export going on, which then finishes, not having written its file.
    Q_INVOKABLE void stop();

signals:
    void busyChanged();
    void progressChanged();
    // When an export is done, or has stopped: { ok, error, path, name, slides, notes },
    // `notes` a list of { message, slides } (slides as "slides 2, 5 and 9")
    void finished(const QVariantMap &result);

private:
    void setProgress(double progress);

    QThreadPool m_pool;
    std::shared_ptr<std::atomic<bool>> m_stopped;
    bool m_busy = false;
    double m_progress = 0;
};
