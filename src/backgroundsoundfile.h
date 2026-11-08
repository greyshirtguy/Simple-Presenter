#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// Backgrounds played with their sound: a slide's background video that is to be heard,
// this app's own (see workspace::MediaBehaviour: a background is silent here, whatever
// sound its file has, unless its slide is in this file). The plain rules of the file
// they are kept in, with nothing of the windows in them, so that they can be tried by
// themselves (tests/unit/tst_backgroundsoundfile); BackgroundSound (src/backgroundsound.h)
// reads and writes the file, and says what is in it to whatever reads a presentation.
//
// Asked for by the maintainer on 2026-11-08, after a service: a video dropped on a slide
// as a background had no sound, and nothing said so. A slide's background video now has
// an icon that says its sound is muted, and a click on it has the sound played, and
// another mutes it again, the video staying a background throughout.
//
// Why a file of the app's own, kept as the disable dates are (disabledatefile.h): that a
// background is silent is this app's rule, and ProPresenter's files have nothing that
// says a background is to be heard (their media has a volume, which is how loud it is
// when it is heard, and which a foreground has here too). Nor would ProPresenter keep a
// field of the app's own in its files: it drops what it does not know. So it is kept
// beside the presentations, by the presentation's uuid and the slide's (its cue's), in
// the workspace's folder, so that it goes where the workspace goes.
//
// The file is `<workspace>/SimplePresenter/Background Sound.json`:
//
//     { "about": "...", "played": [ { "presentation": "<uuid>", "path": "Libraries/Demo/x.pro",
//       "name": "x", "slide": "<uuid>" } ] }
//
// A slide is in it only while its background's sound is played: muted, which is what a
// background is, it is taken out. "path" is where the presentation was, from the
// workspace's folder, and "name" its name, for whoever reads the file; the uuids are
// what it goes by. Every other key of the file and of each slide in it is kept as it
// was when the file is written again.
namespace backgroundsoundfile {

struct Entry {
    QString presentation;
    QString path;
    QString name;
    QString slide;
};

// The file in a workspace's folder
QString pathIn(const QString &workspace);

// The file's slides: none for a missing or empty file. A file that is there but cannot
// be read as one says so in `error`, and gives none.
QList<Entry> read(const QByteArray &bytes, QString *error);
// `bytes` (the file as it is, or nothing) with its slides made `entries`; every other
// key of it and of a slide that is kept stays.
QByteArray write(const QByteArray &bytes, const QList<Entry> &entries);

// `entries` with `entry` in them, in place of the same slide if it was there already
QList<Entry> with(QList<Entry> entries, const Entry &entry);
// `entries` without a slide
QList<Entry> without(QList<Entry> entries, const QString &presentation, const QString &slide);
// Whether a slide is in them: whether its background is played with its sound
bool has(const QList<Entry> &entries, const QString &presentation, const QString &slide);

} // namespace backgroundsoundfile
