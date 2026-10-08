#pragma once

#include "action.pb.h"

#include <QString>
#include <QVariantMap>

// The actions of a slide's cue and of a macro: what else happens when it is triggered.
//
// What an action is. In ProPresenter's files a cue is a list of actions, and showing a
// slide is one kind of them; so is triggering media. The rest are things done
// alongside: start a timer, clear a layer, give a stage screen a layout, turn a prop
// on or off, run a macro, and a good many more. A macro is nothing but a named list of
// such actions.
//
// What is done with them here. Five kinds are understood, which is to say shown, made,
// changed and run: those to do with timers, clearing, the stage, props and macros.
// Every other kind is shown for what it is and left alone. This is the one place that
// turns an action into the plain map QML works with, and such a map back into an
// action, for a slide's cue and for a macro alike.
//
// The map has, for every action:
//   id, kind ("timer", "clear", "stage", "prop", "macro" or "other"), title (a few
//   words saying what it does), done (whether this app runs it)
// and for each kind:
//   timer:  action (a Timers::Action), timerId, timerName, amount (the seconds an
//           Increment adds), and, if the action also sets the timer up, set (true),
//           configuration (that part of the file, in base 64, as Timers::act takes
//           it), and setKind, setDuration, setTimeOfDay, setStartTime, setEndTime,
//           setHasEndTime, setOverrun (as a timer's own are, see Timers)
//   clear:  layer (as the file numbers the layers: 0 everything, 2 the media behind
//           the slides, 4 the props, 5 the slide)
//   stage:  assignments, a list of { screenId, screenName, layoutId, layoutName }, the
//           layout's two empty for a screen that is left as it is
//   prop:   propId, propName, collectionId, collectionName, clear (whether it takes
//           the prop off, where otherwise it puts it on)
//   macro:  macroId, macroName, collectionId, collectionName
namespace actions {

// Whether an action is one of those a cue has beside its slide and its media, which
// are listed with it.
bool listed(const rv::data::Action &action);

QVariantMap describe(const rv::data::Action &action);

// Makes an action what the map says, of the five kinds: a new one, given an id, or
// one that is there, whose id and whatever else it holds are kept. Returns an error
// message, empty on success.
QString build(const QVariantMap &wanted, rv::data::Action *action);

} // namespace actions
