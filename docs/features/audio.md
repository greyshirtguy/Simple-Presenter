[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Audio

The **audio bin** is ProPresenter's: playlists of sound files, for the music before a
service, a track to sing to, a sound effect. It is a tab of the show controls, the one
with two joined notes, and what it shows is read from the file ProPresenter keeps it in
(`Playlists/Audio`), where what is changed here is saved.

At the top, on the lighter ground, are the playlists and the folders they are kept in;
under them, on the darker, the pieces of the playlist picked. The line between the two
is dragged to share out the height. Over the pieces it says how many there are, and at
the right of that a **+** browses for sound files to add. Sound files can also be
dragged in from the file manager, onto a playlist or among the pieces. They are
referred to where they are on disk, not copied. The **+** under the tabs adds a
playlist or a folder; a right click on one renames or removes it, and one can be
dragged onto another to move it.

**A click on a piece plays it.** What it is says how:

- A piece is **music** (two joined notes at its left) or a **sound effect** (a jagged
  line). Music is one at a time: a piece of music takes the place of the one that is
  playing. Sound effects do not stop anything: they play over the music and over each
  other, as many at once as are clicked, the same one twice if it is clicked twice.
- At its end a piece **stops** (a square at its right), **loops** (an arrow going
  round) until it is cleared or other music takes its place, or is followed by the
  **next** of its playlist (an arrowhead against a bar), so that a playlist of music
  plays through. A piece whose file is not on this computer says "missing", is not
  played, and is passed over by the one before it. It is music that the next follows:
  a sound effect set to Next plays once, and nothing follows it.

A right click on a piece sets those: **Stop**, **Loop** or **Next**, and **Music** or
**Sound Effect**. They are the piece's own, kept in ProPresenter's file as ProPresenter
keeps them. What is playing is marked orange, and the music counts down what it has
left.

**All of it is the audio layer**, which has a clear button of its own beside those of
the slide, the media and the props, with ProPresenter's picture of clearing the audio:
red while anything is playing, and **F5** does the same. Clearing everything clears it
too, and a [clear group](clear-groups.md) can have it among its layers.

Along the bottom of the bin:

- a switch, two lines that cross, for **crossing** one piece of music into the next.
  On, music that takes the place of music comes in as the other goes out, and a piece
  that the next follows hands over a fade's length before its end, so that the two
  cross. Off, the one stops and the other starts.
- a box that **narrows the list** to the pieces whose names have what is typed in it.
  Esc empties it.
- **how long a fade is**, as a slider and as a number of seconds, a second unless it is
  changed. It is how long a crossing takes, and how long the audio takes to go out
  when it is cleared. With no length, clearing stops it at once.

**A piece can be dragged** onto a slide, which gives the slide an [action](actions.md)
that plays it when the slide goes live; and onto a [macro](macros.md). To get to the
macros with a piece in hand, hold it over their tab: the tab comes forward after a
moment. There it is dropped on a macro, which is given the action, or on the strip
along the bottom, which makes a macro of its own for it, called what the piece is
called. An action that plays audio is a copy of the piece as the bin had it then, and
has its own Stop or Loop and Music or Sound Effect, in its menu; it has no playlist, so
nothing follows it.

**A playlist can be dragged** too, out of the list at the top, onto a slide or onto a
macro: that gives it an action that plays the playlist from its first piece, which is
followed by the next as music clicked here is. Run again while the playlist is playing,
it leaves it to play on, unless its Always Retrigger is on. It can be picked from Add
Action as well, under Audio Playlist. See [Actions](actions.md).

**How loud it is** is shown in the operator's preview: two bars up its left edge,
the left and the right, there while anything is sounding (a piece of audio, or a video
with its sound) and gone when nothing is. They go by decibels as a sound desk's do:
green, yellow for the top 12, red for the top 3. They show the sound as the app plays
it; the system's own volume, and whatever comes after the app, is not in them.

**Where sound goes** is in the settings, under **Audio**: the system's own choice, which
is wherever this computer sends sound, or one of the devices it has. It is for
everything the app plays, the audio bin and a video's sound alike, and is this
computer's, kept with the app's own settings; ProPresenter's file of the workspace
names a device too, one of the computer it was written on, and that is left as it is.
A device that was chosen and is not there now (unplugged, say) is said to be gone, and
sound goes wherever the system sends it until it is back.

What is a guess, ProPresenter's own audio bin having been described and not looked at,
and one file of its own having been read (version 21.4, six pieces):

- How the bin is laid out is as it was described by the person this was made for.
- A file added here is set to be followed by the next, as five of that file's six
  pieces are, and is given what those pieces have but for which outputs their channels
  go to, which was plainly that user's own doing.
- Where the file says a piece goes round is known from the one piece of that file that
  does; that the next following a piece is the cue's "completion target" is known from
  the five that have it. An action has a second place that could say it goes round,
  which that file never uses: it is read if it is there, and never written.
- That ProPresenter stops at a playlist's last piece and does not begin again at its
  first.
- That nothing follows a sound effect. ProPresenter may well play the next after one
  that is set to Next, as it does after music: that has not been looked at, and here
  it is not done.
- What ProPresenter's fade applies to and how long its crossfade is. Here one length
  does for both, and a new piece of music cuts in unless crossing is on.
- That clearing the audio stops the sound effects as well as the music, and that the
  "audio effects" a clear group can name are the sound effects.
- That F5 is ProPresenter's key for clearing the audio. It is believed to be, and has
  not been checked there.
- What ProPresenter gives a slide's or a macro's action that plays audio: here it is a
  copy of the bin's own action for the piece, with the piece's name on it.
- How many bars ProPresenter's meters have, where their colours change and how fast
  they fall.

Not done: see [TODO](../todo.md).
