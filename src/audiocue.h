#pragma once

#include "action.pb.h"
#include "cue.pb.h"
#include "url.pb.h"

#include <QString>
#include <QVariantMap>

#include <functional>
#include <string>

// How a piece of audio plays, as ProPresenter's files say it: read from a file, and
// changed in one. Plain rules on the file's own messages, with nothing of the windows
// or of the disk in them, so that they can be tried by themselves
// (tests/unit/tst_audiocue).
//
// Where audio is kept. The audio bin is one file of a workspace, Playlists/Audio: the
// same kind of file as the playlists of presentations and the media playlists
// (playlistfile.h), with a piece of audio for each row. A row is a cue, and the cue has
// one action, which is a media action whose media is a sound file. The same action,
// by itself, is what a slide or a macro has that plays a piece of audio: dragging a row
// of the bin onto a slide copies the row's action into the slide's cue.
//
// What says how it plays is in three places, as one file of ProPresenter's own showed
// (version 21.4 on a Mac, six rows: five that go on to the next and one that goes
// round):
//
//   - Whether it goes round again at its end is the MEDIA's: `playback_behavior` in
//     the transport of the action's media, where a video's is too.
//   - Whether the next row of the playlist follows it is the CUE's: its "completion
//     target" is the next cue. Every row of that file also has the "completion action"
//     that says this happens after the last of the cue's actions, whatever its target,
//     and that is left as it is found.
//   - Whether it is music or a sound effect is the ACTION's: `audio_type` beside the
//     media, which ProPresenter calls a tune and a sound. Music is one at a time, a
//     piece of music taking the place of the one that is playing; sound effects play
//     over whatever else is playing and over each other.
//
// So there are three ways a row can play on from its end, of which it has one:
//
//   stop   it plays once
//   loop   it goes round until it is cleared or something takes its place
//   next   it plays once, and then the row after it in its playlist is played
//
// A row whose media goes round is "loop" whatever its cue says is next, since it never
// comes to an end for anything to follow. An action on a slide or in a macro has no
// playlist, and so nothing that could be next: there it is "stop" or "loop".
//
// GUESSES, where the one file looked at does not say:
//   - The action has a second place for how it plays on (beside `audio_type`), which
//     that file leaves empty in every row, the one that goes round among them. It is
//     read (an action that says there that it goes round is taken to) and never
//     written.
//   - ProPresenter can also have media go round a number of times, or for a length of
//     time. Nothing here sets either; one that is found is told as "loop", with the
//     number or the time beside it for whoever plays it.
//   - What a new row is given is what that file's rows have, less what was plainly
//     the user's own doing there (which outputs each channel goes to): see make().
//     That a new row goes on to the next is taken from five of its six doing so.
namespace audiocue {

// Whether an action plays a piece of audio: a media action whose media is a sound file
bool isAudio(const rv::data::Action &action);
// The action of a cue that does, or null: the first, if it should have several
const rv::data::Action *audioOf(const rv::data::Cue &cue);
rv::data::Action *audioOf(rv::data::Cue *cue);

struct Behaviour
{
    // "stop", "loop" or "next"
    QString playback = QStringLiteral("stop");
    // A sound effect, and not music
    bool effect = false;
    // How loud, from 0 to 1, which is as loud as the file has it
    double volume = 1;
    // How long it is in seconds, as the file says; 0 if it does not
    double duration = 0;
    // For one that goes round only so many times, or only for so long: how many, and
    // how many seconds (0 for one that goes round for good, and for any other)
    int loopCount = 0;
    double loopSeconds = 0;

    // Adds to a row of a list what the row shows and what plays it: playback, effect,
    // volume, duration, loopCount, loopSeconds.
    void describe(QVariantMap *row) const;
};

// How an audio action plays. `cue` is the row's cue, which says whether the next row
// follows; null for an action that is not a row's (on a slide, in a macro).
Behaviour read(const rv::data::Action &action, const rv::data::Cue *cue);

// Makes it play on from its end one of the three ways ("stop", "loop", "next"), by
// changing only what says so: the media's going round, and the cue's target. Anything
// else leaves it as it is and answers false. With no cue, "next" is "stop".
bool setPlayback(rv::data::Action *action, rv::data::Cue *cue, const QString &playback);
// Makes it a sound effect, or music.
void setEffect(rv::data::Action *action, bool effect);

// Fills in a new row's cue for a sound file: `name` is what it is called, `file` the
// reference to it (see workspace::recordFile), `format` what kind of file it is as its
// name ends, and `seconds` how long it is, or 0 if that is not known. `newId` makes an
// id each time it is called. See GUESSES above.
void make(rv::data::Cue *cue, const QString &name, const rv::data::URL &file, const QString &format, double seconds,
          const std::function<std::string()> &newId);

} // namespace audiocue
