#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace rv::data {
class Action;
class Presentation;
}

// ProPresenter's timeline: a presentation's own run of things to do at times, played
// from its start as a clock goes, for a song played to a track. Plain rules, on the
// presentation's file and on the times, with nothing of the windows in them, so that
// they can be tried by themselves (tests/unit/tst_timeline).
//
// What ProPresenter says of it (its blog, "Automate with Timeline in ProPresenter 7",
// and its help, "Using Timelines in ProPresenter"). It is opened by a button at the
// upper right of a presentation, and has four tracks: the slides triggered, the media
// triggered (backgrounds, which run on by themselves), the audio (a click track, a
// soundtrack) and the actions (a macro, say). Play triggers each at its time. The
// playhead can be put anywhere and played from there, for rehearsing. With Record on,
// whatever slide, media or action is clicked is put on the timeline where the playhead
// is, playing or not. Its length is typed or dragged; it can go round (Loop). A clear
// group can stop it, the presentation's and the announcements'.
//
// How a file of ProPresenter's keeps it (one made by ProPresenter 22 on a Mac, given by
// the maintainer on 2026-11-14, with five slides and a piece of audio on it). The
// presentation's `timeline` has a duration in seconds (300 where nothing has changed
// it, which is what ProPresenter gives every presentation), whether it loops, whether
// it follows timecode and from what offset, and a list of cues, each a time in seconds
// from the start (left out for 0), a name, and either a slide (its cue's id) or an
// action. A slide's name is "Cue" and its place among the presentation's slides as the
// groups store them (Master's order, from 1: the presentation's sixteenth slide is
// "Cue 16" whichever arrangement is shown); a piece of audio is an action that plays it,
// named by its file with its extension ("Josh Leake - New City.mp3"), with its length
// as the action's duration. The cues are in the order they were put on, not by time:
// the audio, put on last, came after the slides at later times.
//
// There are two lists of cues in the format, `cues` (1) and `cues_v2` (11). That file
// had its cues in the second and nothing in the first. The maintainer's reading
// (2026-11-14): older versions write the first, and newer ones the second and can still
// read the first. So both are read, the second wherever it has anything; and what is
// written goes in the second. A timeline that has only the first is copied into the
// second as it is first changed here, and the first is left as it was (A GUESS: what
// ProPresenter does with the old list when it changes such a timeline has not been
// seen).
//
// What is a GUESS in how it plays, ProPresenter's help saying nothing of it:
//   - started part way (the playhead put somewhere and played), the slide of the cue
//     before the playhead is triggered, and the audio and media whose time on the
//     timeline is under the playhead are started where the playhead is in them, so that
//     rehearsing from the second verse sounds and looks as it would have; actions
//     before the playhead are not run;
//   - a slide that comes up more than once in the arrangement being shown is taken at
//     the first place after the slide the show is at, round to the first if there is
//     none after: so that a timeline goes through a song's choruses in order;
//   - cues at the same time are done in the order they were put on.
namespace timeline {

// The length ProPresenter gives a timeline nobody has changed
constexpr double kUsualDuration = 300;

// A presentation's timeline as the operator window shows it and plays it:
//   duration (seconds), loop, timecode (whether it follows timecode), timecodeOffset,
//   cues, and oldList (its cues are in the old list and not the new one);
//   slides: what a slide is called on the timeline and the group it is in, by the
//     slide's id, for every slide of the presentation: { name ("Cue 3"), group,
//     groupColor ("#rrggbb" or "") }, for a cue put on here to be named as
//     ProPresenter names one, and drawn in its group's colour
// and each cue, in the file's order:
//   place (its place in the list, which is how it is changed: cues have no id),
//   time, name, track ("slide", "media", "audio" or "action"), length (seconds, for
//   audio and media: the action's duration, or the end its media says it has; 0 where
//   neither is known), and
//   for a slide: slideId, group, groupColor (of the group it is in, "" for none);
//   for anything else: action, as `describeAction` makes it (actions::describe, whose
//     kind says the track: "audio", "media", or anything else an action).
QVariantMap describe(const rv::data::Presentation &presentation,
                     const std::function<QVariantMap(const rv::data::Action &)> &describeAction);

// The places of the cues whose times are after `from` and up to `to`, in the order
// they are to be done: by time, and those at one time in the file's order. What a
// playhead passing from `from` to `to` triggers.
QList<int> passed(const QVariantList &cues, double from, double to);

// A cue that is in force where a timeline is started, and how far into it that is.
struct Entry
{
    int place = -1;
    double offset = 0;
    bool operator==(const Entry &other) const = default;
};
// What starting at `at` triggers (see the guesses above): the slide of the last slide
// cue at or before it; each piece of audio or media that has begun by then and has
// not ended (its offset how far in), or that begins there; and each action at exactly
// that time. In the order they are to be done, as passed() has them.
QList<Entry> entering(const QVariantList &cues, double at);

// The first time after `after` at which a cue is due; -1 for none.
double nextTime(const QVariantList &cues, double after);

// Which place of the slides shown (`ids`, the slides' ids in the arrangement's order)
// a slide cue for `slideId` triggers, with the show at place `after` of them (-1 for
// none): the first after it that is that slide, or failing that the first anywhere;
// -1 if the slide is not in the arrangement.
int slidePlace(const QStringList &ids, const QString &slideId, int after);

// ---- Changing it. Each changes the presentation in place, in its timeline and in
// nothing else, and says whether anything was changed; the presentation is then written
// back whole by whoever read it (see Catalog).

// A slide cue at `time`, named as ProPresenter names one; put after the cues there
// are, as ProPresenter puts them. False for a slide the presentation has not.
bool addSlide(rv::data::Presentation *presentation, double time, const QString &slideId);
// An action cue at `time`: `action` as made (actions::build), named `name`. Its
// length, where the action has none, is what its media says.
void addAction(rv::data::Presentation *presentation, double time, const rv::data::Action &action, const QString &name);
// The cue at `place` (as describe() numbers them) moved to `time`, or taken off.
bool move(rv::data::Presentation *presentation, int place, double time);
bool remove(rv::data::Presentation *presentation, int place);
// Its length, at least a second; and whether it goes round.
bool setDuration(rv::data::Presentation *presentation, double seconds);
bool setLoop(rv::data::Presentation *presentation, bool loop);

} // namespace timeline
