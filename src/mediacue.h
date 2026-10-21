#pragma once

#include "action.pb.h"
#include "cue.pb.h"

#include <QString>
#include <QVariantMap>

// What follows a row of a media playlist when its video has played to its end, which is
// ProPresenter's Next Cue: read from a file, and changed in one. Plain rules on the
// file's own messages, with nothing of the windows or of the disk in them, so that they
// can be tried by themselves (tests/unit/tst_mediacue). Which row then plays is the
// show's to decide: see State::mediaFinished (showstate.h).
//
// Where it is kept. A row of a media playlist (playlistfile.h) is a cue, with the media
// action that plays it; what follows it is the CUE's "completion target", as it is for
// a row of the audio bin that goes on to the next (audiocue.h). The target is one of
//
//   nothing   the video stays on its last frame, as it always did here
//   next      the row after it in its playlist
//   random    any other row of its playlist
//   first     the first row of its playlist
//   a cue     a row of its own choosing, named by the id of that row's cue
//
// It is followed only where the row was played from its playlist: a click on it in the
// media bin, or an action that plays the playlist. The same media triggered by a slide
// has no playlist to go on in.
//
// A row that goes round never comes to an end for anything to follow, so a row given
// anything to follow is made to stop at its end; and the target is the cue's whatever
// the media does, so a row that is later made to go round again keeps it, unfollowed.
//
// GUESSES, no file of ProPresenter's with one of these having been seen: that a chosen
// row is named by its CUE's id (a cue's target is another cue, and a row's cue has an id
// of its own, apart from the row's). The show's guesses are with State::mediaFinished.
namespace mediacue {

struct Next
{
    // "" for nothing, "next", "random", "first" or "cue"
    QString kind;
    // For a cue: the id of that row's cue
    QString target;
};

// What follows a row, as its cue says
Next read(const rv::data::Cue &cue);
// For the rows the bin is handed: `cueId`, and, for a row that is followed by
// anything, `next` (as Next::kind) and `nextCue` (as Next::target, for a cue).
void describe(const rv::data::Cue &cue, QVariantMap *row);

// Sets what follows a row (`kind` as Next has it; `target`, for a cue, the id of the
// cue of the row to go to). Anything to follow makes the row's video stop at its end,
// through `media`, the row's media action (which may be null). Only what is to be other
// than it is is written. False, and nothing changed, for a kind there is no such thing
// as, or a cue with no target.
bool set(rv::data::Cue *cue, rv::data::Action *media, const QString &kind, const QString &target);

} // namespace mediacue
