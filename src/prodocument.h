#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

#include <optional>

// A presentation, read for showing.
//
// How ProPresenter stores one. A .pro file is a `Presentation` message holding
//   - cues: one for each slide. A cue is a list of actions, the things that happen when
//     it is triggered: usually one that shows a slide, and often one that starts a
//     media file under it;
//   - cue groups: named, coloured runs of cues (Verse 1, Chorus), each a list of cue ids;
//   - arrangements: named orders of the groups, in which a group may come up several
//     times (Verse 1, Chorus, Verse 2, Chorus); and which of them is selected.
// With no arrangement selected the slides run in the order the groups are stored, which
// ProPresenter calls Master.
//
// What this makes of it: a presentation flattened into what the QML slide renderer
// consumes, a list of slides in display order. Each is a map of `id` (its cue's, so the
// same slide can be found again after rearranging), size, background, label, plain text
// and a list of elements, plus its group: `group` (name), `groupColor` (the document's
// own colour for it as "#rrggbb", or empty) and `groupStart` (true on the first slide of
// each run of the group). Element text is a RichText value. All geometry is in slide
// units.
//
// What else a slide's cue does when it is triggered goes with the slide:
//   - media: the file's name as `mediaName` and whether it is a foreground as
//     `mediaForeground`, and, where the file can be found here, the media itself as
//     `media`: { name, path, source, video, foreground, loops, retriggers, volume }
//     (the last four as workspace::MediaBehaviour describes);
//   - timers: `timerActions`, a list of what to do to which timer, each a map as
//     Timers::act() takes.
struct ProDocument
{
    QString name;
    QVariantList slides;
    // Names of the arrangements the document defines, and the one `slides` follows;
    // empty means every group in stored order.
    QStringList arrangements;
    QString arrangement;

    // Reads a ProPresenter 7 .pro file. `arrangement` names the arrangement to follow,
    // "" (or a name the file does not have) being every group in stored order; without
    // it the arrangement the file has selected is followed. On failure returns an empty
    // document and sets *error. `workspace` is the folder the presentation's library,
    // media and playlists live under: media is looked for relative to it, then at its
    // recorded path, then by file name under its Media folder.
    static ProDocument load(const QString &path, const QString &workspace,
                            const std::optional<QString> &arrangement, QString *error);
    // Just the arrangement names and which is selected ("" for none). False if unreadable.
    static bool arrangementsOf(const QString &path, QStringList *names, QString *selected);
    // Selects the named arrangement, or none for "", and writes the file back. Returns
    // an error message, empty on success.
    // The id of the arrangement of that name, or empty.
    static QString arrangementId(const QString &path, const QString &name);
    static QString setArrangement(const QString &path, const QString &name);
    // Makes the cue with this id trigger the given image or video file as its media, as
    // a background or a foreground (see workspace::MediaBehaviour), replacing the media
    // it triggered before if any, and writes the file back. The media is recorded
    // relative to `workspace` as well as by its path, if it is inside. Returns an error
    // message, empty on success.
    static QString setCueMedia(const QString &path, const QString &cueId, const QString &mediaPath, bool foreground,
                               const QString &workspace);
    // Makes the media the cue triggers a background or a foreground, and writes the
    // file back.
    static QString setCueMediaForeground(const QString &path, const QString &cueId, bool foreground);
    // Removes the media the cue triggers, if any, and writes the file back.
    static QString removeCueMedia(const QString &path, const QString &cueId);
};
