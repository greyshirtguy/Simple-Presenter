#pragma once

#include <QColor>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

// What the HTTP API's answers are made of (see httpapi.h, httpapiserver.h): the app's own
// maps turned into ProPresenter's JSON, in plain C++, with unit tests. The shapes are the
// ones ProPresenter's documentation gives and a ProPresenter 21.4 was seen to answer
// with; where the app has nothing to fill a field with, what ProPresenter gives for
// nothing is given, and the comment says so. (What has to be read from ProPresenter's
// files is in httpapifiles.h.)
namespace httpapijson {

// ProPresenter's index for "not in a list": the largest number it writes there
// (4294967295, the active presentation's own index in its id as it was seen)
constexpr qint64 noIndex = 4294967295LL;

// An id, as nearly everything is named: {uuid, name, index}
QVariantMap id(const QString &uuid, const QString &name, qint64 index);

// A colour, as ProPresenter gives one: red, green, blue and opacity, each 0 to 1
QVariantMap color(const QColor &color);

// status/slide: the text, notes and cue UUID of the slide that is live and of the one
// that would come next (each as a slide map, see proconvert; an empty map for none)
QVariantMap slideStatus(const QVariantMap &current, const QVariantMap &next);

// presentation/slide_index's and announcement/slide_index's answer: {key: {index,
// presentation_id, total_cues, remaining_cues}}, or {key: null} with no presentation
// (index < 0). The presentation by its UUID, its name and its place in its list.
QVariantMap slideIndex(const QString &key, const QString &uuid, const QString &name, qint64 listIndex, int index, int total);

// The libraries, as libraries gives them: [{uuid, name, index}]. ProPresenter's own
// UUIDs for its libraries are not in the workspace's folder, so each is given one made
// from its name, the same every time (and so the same on every computer); ProPresenter's
// documentation says a library is better asked for by its name anyway, its UUID being
// "deprecated" for that.
QVariantList libraries(const QStringList &names);
QString libraryUuid(const QString &name);

// The playlists, as playlists gives them: a tree of [{id, field_type ("group" for a
// folder, "playlist"), children}], each id's index its place among those beside it.
// From the flat list the app has of them ({name, path, parent, folder}: see
// PlaylistFile::nodes), whose path is a playlist's UUID.
QVariantList playlistTree(const QVariantList &nodes);

// A timer's time as timers/current gives it: hours, minutes and seconds, two figures
// each ("00:01:39"), with a minus before a countdown that has run past nothing (a GUESS:
// none that had was seen)
QString timerTime(double seconds);
// How a timer stands, in ProPresenter's words: "running", "overrunning" (a countdown
// running past nothing), "stopped", "complete" (stopped where it ends), or "overran"
// (stopped past nothing). From a timer of Timers' list and its state (Timers::state).
QString timerState(const QVariantMap &timer, const QVariantMap &state);
// A timer as timers gives it: its id, whether it may overrun, and what kind it is with
// its times: countdown {duration}, count_down_to_time {time_of_day, period} or elapsed
// {start_time, end_time if it has one}. (A countdown to a time is given as seconds from
// midnight on a 24-hour clock: a GUESS at how ProPresenter writes its own.)
QVariantMap timer(const QVariantMap &timer, int index);
// timers/current's entry for it: {id, time, state}
QVariantMap timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index);

// A look as looks and look/current give it: {id, screens}, a screen for each of the
// audience screens in their order (Looks' lines: see looks.h), each saying which
// layers it shows, the theme ("presentation") and the mask, by UUID
QVariantMap look(const QVariantMap &look, const QStringList &screenIds, const QString &name, int index);

// A prop as props gives it: {id, is_active, auto_clear_enabled, auto_clear_duration,
// transition}. (The app has no clearing of a prop by itself, nor a transition of a
// prop's own: off, and none.)
QVariantMap prop(const QVariantMap &prop, int index, bool active);

// A message as messages gives it: {id, message (its words with its tokens in them as
// ${name}), tokens, theme, visible_on_network, is_active}. A token: {name, text: {text}},
// {name, timer: {...}} or {name, clock: {...}}, the timer's and clock's formats as far
// as the app keeps them.
QVariantMap message(const QVariantMap &message, int index, bool active);

// A clear group as clear/groups gives it: {id, icon, tint, layers, ...}, its layers and
// its picture by ProPresenter's names for them (see clearfile.h)
QVariantMap clearGroup(const QVariantMap &group, int index);

// A macro as macros gives it: {id, color, image_type, actions: [{type}]}, each action's
// type by ProPresenter's name for its kind (see actions.h)
QVariantMap macro(const QVariantMap &macro, int index);

// The media playlists, or the audio bin's, as media/playlists and audio/playlists give
// them: the same tree as the playlists' but with "type" ("group" or "playlist") where
// those have "field_type"
QVariantList binTree(const QVariantList &nodes);
// A row of one (PlaylistFile's), as media/playlist/{id} and audio/playlist/{id} give it:
// {id, type ("video", "image" or "audio"), artist, duration in whole seconds}
QVariantMap binItem(const QVariantMap &row, int index, bool audio);

// timer/video_countdown: the time left of the video playing, as hours, minutes and whole
// seconds ("0:00:01", as ProPresenter was seen to write it; its documentation's example
// has two figures of hours). Nothing playing is no time left.
QString videoCountdown(double seconds);


}
