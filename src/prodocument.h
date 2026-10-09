#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace rv::data {
class Cue;
class Slide;
}

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
//   - media: the file's name as `mediaName`, whether it is a foreground as
//     `mediaForeground`, whether it is a video as `mediaVideo` and, for a video, how
//     it plays on from its end as `mediaPlayback`, `mediaLoopCount` and
//     `mediaLoopSeconds`; and, where the file can be found here, the media itself as
//     `media`: { name, path, source, video, foreground, loops, retriggers, volume,
//     playback, loopCount, loopSeconds } (all but the first four as
//     workspace::MediaBehaviour describes);
//   - everything else: `actions`, a list of what the cue does besides, such as
//     starting a timer or running a macro, each a map as src/actions.h describes.
struct ProDocument
{
    QString name;
    QVariantList slides;
    // Names of the arrangements the document defines, and the one `slides` follows;
    // empty means every group in stored order.
    QStringList arrangements;
    QString arrangement;
    // Whether any slide has chords over its words, and the key they are written in
    // (see chords.h), "" if the file names none. `userKey` is the key the file says
    // they were last shown in, which is the original one if it names no other.
    bool hasChords = false;
    QString originalKey;
    QString userKey;

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
    // Sets how the video the cue triggers plays on from its end (see
    // workspace::MediaBehaviour), and writes the file back.
    static QString setCueMediaPlayback(const QString &path, const QString &cueId, int playback, int loopCount, double loopSeconds);
    // Gives the cue another action, changes one it has, by the action's id, and takes
    // one away. The action is a map as src/actions.h describes. Each writes the file
    // back and returns an error message, empty on success.
    static QString addCueAction(const QString &path, const QString &cueId, const QVariantMap &action);
    static QString changeCueAction(const QString &path, const QString &cueId, const QString &actionId, const QVariantMap &action);
    static QString removeCueAction(const QString &path, const QString &cueId, const QString &actionId);
    // Sets the key the presentation's chords are written in, as chords.h names keys.
    // The chords themselves are not changed: this says what key they are in, for
    // showing them in another. The key they are shown in is made the same, as it is
    // for a song that has never been shown in another. Writes the file back.
    static QString setOriginalKey(const QString &path, const QString &key);
    // Removes the media the cue triggers, if any, and writes the file back.
    static QString removeCueMedia(const QString &path, const QString &cueId);
    // Removes the cue with this id, and so its slide, from the presentation and from
    // every group that lists it (a group that comes up twice in an arrangement loses it
    // in both places). A group left with nothing in it is kept: arrangements name
    // their groups, and ProPresenter keeps an empty one too. Writes the file back.
    static QString removeCue(const QString &path, const QString &cueId);
    // Adds a slide for each of the image or video files, in the order given: a new cue
    // with nothing on its slide, named for the file, that triggers the file as a
    // foreground. That is what ProPresenter makes of media dropped between two slides,
    // and how a video is given a place of its own in the run of a presentation. They
    // go just before the cue with this id, or with `after` just after it, in the
    // presentation and in that cue's group; with no cue named, at the end. Writes the
    // file back; returns an error message, empty on success.
    static QString insertMediaCues(const QString &path, const QString &cueId, bool after, const QStringList &mediaPaths,
                                   const QString &workspace);
    // Adds a slide with nothing on it, placed as insertMediaCues places one, and gives
    // the id of its cue. Writes the file back.
    static QString insertBlankCue(const QString &path, const QString &cueId, bool after, QString *madeId);
    // A cue as it is in the file, to be pasted; empty, with *error set, if it is not there.
    static QByteArray copyCue(const QString &path, const QString &cueId, QString *error);
    // Adds a copy of a cue that copyCue gave, from this presentation or another, placed
    // as insertMediaCues places one, and gives the id of the copy. The copy and what is
    // on its slide have ids of their own. Writes the file back.
    static QString pasteCue(const QString &path, const QString &cueId, bool after, const QByteArray &copied, QString *madeId);

    // Dresses the slides of these cues in a theme slide (see themefile.h), or with no
    // cues named every slide of the presentation, and writes the file back. `dressed`
    // is given how many slides were.
    static QString dressCues(const QString &path, const QStringList &cueIds, const rv::data::Slide &theme,
                             const QSet<QString> &themeElements, int *dressed);
    // The slide of a cue as it would be dressed in a theme slide, as a map of the kind
    // `slides` holds for drawing: empty if the cue is not there. The file is not changed.
    static QVariantMap dressedSlide(const QString &path, const QString &cueId, const rv::data::Slide &theme,
                                    const QSet<QString> &themeElements);

private:
    // Reads the file, hands the cue with this id to `change`, and writes the file back
    // unless that gave an error.
    static QString changeCue(const QString &path, const QString &cueId, const std::function<QString(rv::data::Cue *)> &change);
};
