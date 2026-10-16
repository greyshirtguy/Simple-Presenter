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
    // What the arrangement editor over the slides shows (qml/ArrangementBar.qml). The
    // presentation's groups in the order it stores them, which is Master, each
    // { id, name, color, slides }: its own id in this presentation, its name ("" for a
    // group with none), its colour as "#rrggbb" or "", and how many slides it has. And
    // the arrangements in the file's order, each { id, name, groups }, where `groups`
    // is the ids of the groups it goes through, one for each time a group comes up,
    // exactly as the file has them: a place in that list is a place in the file's. (So
    // an id that is no group's any more is in it too, though the slides leave it out.)
    QVariantList groups;
    QVariantList arrangementList;
    // Whether any slide has chords over its words, and the key they are written in
    // (see chords.h), "" if the file names none. `userKey` is the key the file says
    // they were last shown in, which is the original one if it names no other.
    bool hasChords = false;
    QString originalKey;
    QString userKey;
    // Where the presentation is sent when one of its slides is triggered: "" for the
    // slide and media layers, as all are unless they say otherwise, or "announcements"
    // for the announcements layer (ProPresenter's Destination, see setDestination()).
    QString destination;
    // ProPresenter's Slide Show: the seconds each slide stays before the next is
    // triggered, going round to the first after the last; 0 for none. (A slide with a
    // Go To Next Timer of its own, which is in its map as "goToNext", goes by that.)
    double slideShow = 0;

    // Reads a ProPresenter 7 .pro file. `arrangement` names the arrangement to follow,
    // "" (or a name the file does not have) being every group in stored order; without
    // it the arrangement the file has selected is followed. On failure returns an empty
    // document and sets *error. `workspace` is the folder the presentation's library,
    // media and playlists live under: media is looked for relative to it, then at its
    // recorded path, then by file name under its Media folder.
    static ProDocument load(const QString &path, const QString &workspace,
                            const std::optional<QString> &arrangement, QString *error);
    // Just the arrangement names and which is selected ("" for none). False if unreadable.
    // `ids`, if asked for, is the id of each, in step with the names.
    // `destination`, if asked for, is where the presentation is sent (see `destination`
    // above), which every row of a list is told as well.
    static bool arrangementsOf(const QString &path, QStringList *names, QString *selected, QStringList *ids = nullptr,
                               QString *destination = nullptr);
    // Records where the presentation is sent: "announcements", or "" for the slide and
    // media layers (which, being what a file means when it says nothing, takes the
    // field out of the file again). Writes the file back unless it says so already.
    // Returns an error message, empty on success.
    //
    // ProPresenter records the same on every row of a playlist that is this
    // presentation, which is PlaylistFile::setRowsDestination(); Catalog does both.
    static QString setDestination(const QString &path, const QString &destination);
    // The presentation's Slide Show, in seconds; 0 for none, which takes it out of the
    // file. Writes the file back unless it says so already.
    static QString setSlideShow(const QString &path, double seconds);
    // A slide's Go To Next Timer: with `on`, after `seconds` the next slide is
    // triggered, or with `loop` the first ("Loop to Beginning"); without, it has none.
    //
    // In the file it is what the slide's cue does when it is done: its target (the
    // next cue, or the first), "after a time", and the time. Taken off, the cue is
    // left as ProPresenter leaves one it has taken a timer off: no target, and done
    // "last", with the time still there (a GUESS, from its files, where cues without
    // a timer have times). But a timer put on in this run of the app and taken off
    // again leaves the cue exactly as it was before, to the byte.
    static QString setCueGoToNext(const QString &path, const QString &cueId, bool on, double seconds, bool loop);
    // Selects the named arrangement, or none for "", and writes the file back. Returns
    // an error message, empty on success.
    // The id of the arrangement of that name, or empty.
    static QString arrangementId(const QString &path, const QString &name);
    static QString setArrangement(const QString &path, const QString &name);
    // Making and changing arrangements, as the arrangement editor over the slides does
    // (arrangementedit.h has the rules, and what of them is a guess). An arrangement is
    // named by its id. Each writes the file back, unless nothing is changed, and
    // returns an error message, empty on success.
    //
    // A new one, of every group once in Master's order; and a copy of one. `*name` is
    // the name it was given.
    static QString addArrangement(const QString &path, QString *name);
    static QString duplicateArrangement(const QString &path, const QString &id, QString *name);
    static QString renameArrangement(const QString &path, const QString &id, const QString &name);
    // Takes one out; if it was the one selected in the file, none is.
    static QString removeArrangement(const QString &path, const QString &id);
    // A group put into an arrangement at a landing (the place it goes before, or the
    // number of places for after the last); the group at a place moved to a landing;
    // and the group at a place taken out.
    static QString insertArrangementGroup(const QString &path, const QString &id, const QString &groupId, int landing);
    static QString moveArrangementGroup(const QString &path, const QString &id, int from, int landing);
    static QString removeArrangementGroup(const QString &path, const QString &id, int place);
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
    // Makes a new presentation in the library folder `library`, called `name` (with a
    // number after it if the library has one of that name already), and gives its
    // file's path. It has one slide with nothing on it, of an HD screen's size, in a
    // group with no name, which is how ProPresenter keeps slides that are in no
    // group. One slide and not none: a presentation is opened to be worked on, and
    // there is then a slide to put a text box or a picture on. (A GUESS: that
    // ProPresenter starts a new presentation with one slide too. It has not been
    // looked at.) Returns an error message, empty on success.
    static QString create(const QString &library, const QString &name, QString *made);
    // Gives a presentation another name: its file's, and the name inside the file,
    // which ProPresenter keeps the same as the file's. The name is taken as
    // proconvert::presentationName() has it. Nothing else in the file is changed, and
    // nothing is changed at all if the library has a presentation of that name
    // already (it is not given a number, as a new one would be: a rename that came
    // out as another name than was typed would be a surprise). `*renamed` is where
    // the file then is. What points at the file by its name is not seen to here: the
    // rows of playlists are the catalog's to bring along. Returns an error message,
    // empty on success.
    static QString rename(const QString &path, const QString &name, QString *renamed);
    // Gives a slide a label, words and a colour ("#rrggbb", or empty for none), or
    // with neither takes its label off (see labelfile.h for where a label is kept and
    // how ProPresenter writes one).
    static QString setCueLabel(const QString &path, const QString &cueId, const QString &text, const QString &color);
    // Puts a slide, and the slides after it in its group, into another group, as
    // ProPresenter does from a slide's menu: `name`, `color` ("#rrggbb", or empty) and
    // `listId` (the group's id in the workspace's list of groups, or empty) are the
    // group that was chosen. The rules are groupedit's, which says what is known of
    // ProPresenter's and what is a guess. `*changed` says whether anything was written
    // (a slide in a group of that name already is left as it is).
    static QString setGroupFrom(const QString &path, const QString &cueId, const QString &name, const QString &color,
                                const QString &listId, bool *changed);
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
    // The slide a cue shows, as the file has it, to take the look of its text from
    // (Copy Text Style): empty, with *error set, if it is not there or has no text box
    // with words in it.
    static QByteArray slideOfCue(const QString &path, const QString &cueId, QString *error);
    // Gives the slides of these cues the look of that slide's text boxes (see
    // themefile::dressText) and writes the file back, where any text box took it.
    // `restyled` is given how many text boxes did.
    static QString dressCuesText(const QString &path, const QStringList &cueIds, const rv::data::Slide &style, int *restyled);

private:
    // Reads the file, hands the cue with this id to `change`, and writes the file back
    // unless that gave an error.
    static QString changeCue(const QString &path, const QString &cueId, const std::function<QString(rv::data::Cue *)> &change);
};
