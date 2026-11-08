#pragma once

#include "backgroundsoundfile.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Backgrounds played with their sound, for QML: a slide's background video that is to be
// heard, this app's own (see backgroundsoundfile.h for why, and the file). Read when a
// workspace is opened, and written as a slide's background has its sound played or muted.
//
// The file is the truth: each change reads it as it is now, makes the one change and
// writes it back, so that what else is in it is kept.
//
// What it holds is asked by whatever reads a presentation (ProDocument::load, through
// backgroundsound::played below), so that a slide's map says it (`mediaSound`) and its
// media's volume is the volume its file gives it, as a foreground's is, rather than none:
// what plays the media goes by that alone, as it always has (see workspace::MediaBehaviour).
// A presentation may be read on another thread than this one's, so what that asks is
// kept apart behind a lock.
class BackgroundSound : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Goes up with every change, so that what shows it is asked again
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit BackgroundSound(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // Reads a workspace folder's file. Returns an error message, empty on success.
    Q_INVOKABLE QString open(const QString &workspace);
    // Whether the background a slide of a presentation (by their uuids) brings is played
    // with its sound
    Q_INVOKABLE bool played(const QString &presentation, const QString &slide) const;
    // The background of a slide of the presentation at `path` (a file of this workspace)
    // played with its sound, or muted. Returns an error message, empty on success.
    Q_INVOKABLE QString set(const QString &presentation, const QString &path, const QString &name, const QString &slide, bool on);

signals:
    void changed();

private:
    QString file() const;
    QString reload();
    QString save();

    QString m_workspace;
    QList<backgroundsoundfile::Entry> m_entries;
    int m_revision = 0;
};

namespace backgroundsound {

// Whether the background a slide (by its presentation's uuid and its cue's) brings is
// played with its sound, in the workspace open: what BackgroundSound last read or wrote.
// Safe to ask from any thread.
bool played(const QString &presentation, const QString &slide);

} // namespace backgroundsound
