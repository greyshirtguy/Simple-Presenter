#pragma once

#include "presentation.pb.h"

#include <QString>

#include <functional>

// Putting slides of a presentation into another group, as ProPresenter does it from a
// slide's menu. Plain rules on the presentation as the file has it, with nothing of the
// windows in them, so that they can be tried by themselves (tests/unit/tst_groupedit).
//
// How a presentation keeps its groups. It has a list of them in the order its slides
// run in, which ProPresenter calls Master: each is a group (an id of its own, a name,
// a colour, and the id of the group in the workspace's list of groups that it was
// given from) and the ids of its slides' cues. An arrangement is a list of the ids of
// groups, in which a group may come up several times.
//
// What ProPresenter does, as the maintainer describes it from using it: choosing a
// group in a slide's menu sets the group of that slide and of all the slides after it
// that were in the same group. So:
//   - on the first slide of a group, the whole group becomes the chosen one. It is
//     still the same group of the presentation, with the id it had and the place it
//     had in every arrangement: what it is called, its colour and which of the
//     workspace's groups it is are what change.
//   - on a later slide the group is split there. The slides before stay the group
//     they were, and the slide and those after it become a new group, of the chosen
//     kind, straight after the old one.
// A slide that is already in a group of the chosen name is left as it is, and nothing
// is written. (That one is this app's own choice and a GUESS at ProPresenter's: what
// it does when a slide is given the group it is in already has not been looked at.)
//
// A GUESS, where a group is split: what becomes of the arrangements. A new group that
// no arrangement has would vanish, with its slides, from every arrangement there is,
// the one being viewed among them. So here it is put after the old group wherever
// that comes up, in every arrangement, and each arrangement shows the slides it did,
// in the order it did. Whether ProPresenter does that has not been looked at.
//
// And how the chosen group is written, which was looked at in the 98 groups, of 98
// presentations, that ProPresenter had given from its list: an id of its own, its
// name, its colour, a hotkey (empty), and the id of the list's group beside them. A
// colour that has no red, green or blue is written as its alpha alone, the file
// leaving out what is nought.
namespace groupedit {

// The group that was chosen
struct Choice
{
    QString name;
    // "#rrggbb", or empty for one that has no colour to give
    QString color;
    // Its id in the workspace's list of groups, or empty if it is not from that list
    QString id;
};

enum class Outcome {
    Unchanged,   // the slide is in a group of that name already
    Whole,       // its group, the slide being the first of it, is now the chosen one
    Split,       // its group was split at it, and the slides from it on are a new group
    NoSuchSlide, // no group of the presentation has a slide with that cue
};

// Sets the group of the slide whose cue is `cueId`, and of the slides after it in its
// group. `newId` gives an id for a new group when one is wanted.
Outcome setGroupFrom(rv::data::Presentation *presentation, const std::string &cueId, const Choice &choice,
                     const std::function<std::string()> &newId);

} // namespace groupedit
