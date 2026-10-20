#pragma once

#include "action.pb.h"

#include <QString>
#include <QVariantMap>

// Two kinds of action that start one of the workspace's playlists: a playlist of the
// media bin (ProPresenter's "Media Bin Playlist" action, type 21 in its file) and a
// playlist of the audio bin ("Audio Bin Playlist", type 20). Plain rules on the action
// as ProPresenter's file has it, with nothing of the windows in them
// (tests/unit/tst_playlistactions.cpp); the show plays what they name (show::State).
//
// What the file holds. Both are one message, PlaylistItemType: the playlist, by its id
// and its name; an item of it, by id and name; whether the playlist is to be selected
// (shown in its bin); and Always Retrigger. ProPresenter's own actions of the audio
// bin (six, in a presentation of the maintainer's, 2026-10-20) name the playlist alone:
// no item, nothing selected, Always Retrigger off and so not written. None of the
// maintainer's files has an action for a playlist of the media bin, so that it is
// written the same way, with the type of the media bin, is A GUESS from the file
// format, which has the one message for both.
//
// What is made here is that: the playlist by id and name, and Always Retrigger only
// when it is on. An item, and whether the playlist is selected, are read and kept as
// they are (an action ProPresenter made that has them keeps them when Always Retrigger
// is changed here); an item is let go of only when the action is given another
// playlist, which it would not belong to. What such an action plays, and when it
// leaves a playlist that is playing to play on, is the show's to say (show::State).
//
// Always Retrigger (the maintainer's words, 2026-10-20): "if it is already playing and
// a macro or slide runs this cue again - it WONT retrigger unless that option is on".
namespace playlistactions {

// Whether an action is one of these two kinds.
bool isPlaylist(const rv::data::Action &action);

// What such an action is, as a map for the rest of the app (see actions.h):
//   { kind: "mediaPlaylist" or "audioPlaylist", playlistId, playlistName, itemId,
//     itemName, selects, retrigger, title }
// `title` says it in words, as every action's map does. An action of neither kind is
// an empty map.
QVariantMap describe(const rv::data::Action &action);

// Makes `action` the one `wanted` describes, as describe() puts it: kind, playlistId,
// playlistName, and retrigger if it is to be changed. A new action is the playlist and
// that alone; one that is there keeps all else it has. Returns an error message, empty
// on success.
QString build(const QVariantMap &wanted, rv::data::Action *action);

} // namespace playlistactions
