#pragma once

#include "action.pb.h"

#include <QColor>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <string>

// ProPresenter's Playback Markers: points in a video or a piece of audio, each with a
// name, a colour and actions that are run when playback reaches it (or when it is
// picked from the transport's Marker menu). Read from a file, and changed in one. Plain
// rules on the file's own messages, with nothing of the windows or of the disk in them,
// so that they can be tried by themselves (tests/unit/tst_playbackmarkers).
//
// Where they are kept: on the media ACTION, beside the media it plays (its MediaType's
// `markers`), so that they are the place's that plays the media and not the file's: a
// slide's video, a row of the media bin, a piece of the audio bin each have their own,
// as each has its own way of playing. A marker is { uuid, time, color, name, actions },
// the actions being actions of every kind a slide's are, kept as a slide keeps its own.
// What runs them is the show's (show::State::runMarker), as it runs a macro's.
//
// ProPresenter's help (support.renewedvision.com, "Using Playback Markers in
// ProPresenter", 7.10 and later) says they are for video and audio files, and not for
// pictures or media used as a slide element's fill: `takes` says so.
//
// GUESSES, no file of ProPresenter's with a marker in it having been seen: that `time`
// is in seconds from the start of the file, as the files' other times are (an in point,
// a cue's completion time); the colours offered (see palette), the one a new marker is
// given (see newColor), and the one a marker with none in its file is shown in; and
// that the markers are kept in the order of their times, which is how they are listed.
namespace playbackmarkers {

// Whether a media action can have markers: one of a video or a piece of audio
bool takes(const rv::data::Action &media);

// A media action's markers, as QML is handed them, in the order of their times:
// { id, time (seconds), color ("#rrggbb"), name, actions }, `actions` each as
// `describeAction` describes it (actions::describe, in the app). Empty for none.
QVariantList describe(const rv::data::Action &media, const std::function<QVariantMap(const rv::data::Action &)> &describeAction);

// The changes, each returning an error message, empty on success. A marker is found by
// its id; `newId` makes ids for what is new.
//
// Adds a marker at `time` (kept to no less than nothing), with no name and the colour
// newColor gives for `roll`; *made is its id.
QString add(rv::data::Action *media, double time, const std::function<std::string()> &newId, QString *made = nullptr,
            quint32 roll = 0);
// Changes a marker's `name`, `color` or `time`, as `changes` has them, and keeps the
// markers in the order of their times.
QString change(rv::data::Action *media, const QString &id, const QVariantMap &changes);
QString remove(rv::data::Action *media, const QString &id);
// A copy of a marker, at the same time, with new ids for it and for each of its
// actions; *made is its id.
QString duplicate(rv::data::Action *media, const QString &id, const std::function<std::string()> &newId, QString *made = nullptr);
// A marker's actions: one added, as it is made (actions::build, in the app); one
// changed by `build`, which is handed the action there is; one taken off.
QString addAction(rv::data::Action *media, const QString &id, const rv::data::Action &action);
QString changeAction(rv::data::Action *media, const QString &id, const QString &actionId,
                     const std::function<QString(rv::data::Action *)> &build);
QString removeAction(rv::data::Action *media, const QString &id, const QString &actionId);

// The colours the marker menu offers by name, as [{ name, color "#rrggbb" }], in the
// order of the rainbow and white last (any other is picked with Other…)
QVariantList palette();
// The colour a new marker of `media` is given: one of the palette's, but for white, that
// none of its markers has yet (any of them, once every one is taken), picked by `roll`,
// which the app makes at random. So markers added one after another are told apart
// without being coloured by hand.
QColor newColor(const rv::data::Action &media, quint32 roll);
// The colour a marker that has none in its file is shown in: the palette's blue
QColor unsetColor();

// Which of `markers` (as describe gives them, in the order of their times) a text box
// linked to a playback marker means, with playback at `position` seconds, as the link's
// identifier says: 0 the first, 1 the previous (the one most lately reached: at or
// before the position), 2 the next (the first after it), 3 the last, 4 the first one
// called `name`. Its place, or -1 for none.
int pick(const QVariantList &markers, double position, int which, const QString &name);

} // namespace playbackmarkers
