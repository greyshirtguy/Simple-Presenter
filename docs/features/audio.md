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

**A piece can have [playback markers](playback-markers.md)**: points in it whose actions
run as it plays them. **Inspector** in its right-click menu has them, in the Media
Inspector's Playback Markers tab; its other tab there, **Audio**, has where its sound
goes and how loud each of its channels is (see [The app's channels](#the-apps-channels),
below).

**The music has a [transport](media.md#the-transport)**, the last of the three tabs over
the transport under the preview, with the audio bin's notes: how far in the music is and
how much is left, a slider to move it, and buttons to go back to its start, to pause it
and play it again, and to skip fifteen seconds. Sound effects have none: they play to
their end over whatever else is playing.

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
the left and the right, there while anything is sounding (a piece of audio, a video
with its sound, or a test tone) and gone when nothing is. They go by decibels as a sound
desk's do: green, yellow for the top 12, red for the top 3. They show what the app sends
the sound device: the left bar its odd-numbered channels, the right its even-numbered (a
stereo pair's two sides, wherever on the device the pair is), the loudest of each. So
sound sent to channels that go nowhere on the device is not on them, as it is not heard.
The system's own volume, and whatever comes after the app, is not in them.

**Where sound goes** is in the settings, under **Audio**: the system's own choice, which
is wherever this computer sends sound, or one of the devices it has. It is for
everything the app plays, the audio bin and a video's sound alike, and is this
computer's, kept with the app's own settings; ProPresenter's file of the workspace
names a device too, one of the computer it was written on, and that is left as it is.
A device that was chosen and is not there now (unplugged, say) is said to be gone, and
sound goes wherever the system sends it until it is back. What is playing goes over to
a newly chosen device at once.

## The app's channels

As in ProPresenter ([Audio Routing in
ProPresenter](https://support.renewedvision.com/hc/en-us/articles/360052696094-Audio-Routing-in-ProPresenter)),
the app has **channels of its own**, between what plays and the sound device: up to
sixteen, two to begin with. Everything that plays is sent to them, and they go on to
the device's own channels. That is how a video's third channel reaches a device's fifth,
how music and a video's sound go to different outputs of an interface, and how a
workspace set up for a computer with an interface of six outputs is played on a laptop
with two.

**How many** is the **Channels** box in the settings, under Audio, beside **Channel
Routing**. Channels taken off are the last; those added are called Channel 5, Channel 6
and so on.

**Channel Routing** opens a window with the app's channels down the left and the
device's across the top, by their numbers, with a cell where each meets each: a cell
lit sends the app's channel to the device's. A click lights it or puts it out, and a
channel can go to as many of the device's as are lit, or to none. Beside each of the
app's channels:

- its **name**, which a click on it lets be changed (Enter keeps it, Esc does not);
- **M**, to mute it; **S**, to solo it: while any is soloed, only those are heard;
- **T**, a test tone on it, a steady note, until it is clicked again.

Over them all, **M** mutes every channel (and, with all muted, unmutes them all), **S**
takes every solo off, and **Mapping** has **Auto**, each channel to the device's of the
same number, and **Clear**, none anywhere. Until the matrix is changed it is Auto.

**What is kept where.** How many channels there are, their names, and whether each is
muted or soloed are the workspace's, kept in ProPresenter's file of how the workspace is
set up (`Configuration/Workspace`), as ProPresenter keeps them; a workspace from
ProPresenter opens with its channels as they were set there. Everything else of how
ProPresenter is set up for sound there (the output device it names, its own matrix, its
SDI and NDI output) is the computer's it was set up on, and is left as it is. Where the
app's channels go on the device is this computer's, and is kept with the app's own
settings for each device it has, so that each device keeps its own. A tone is only
until it is switched off, and is never kept.

**Each piece of media** says where its own channels go, in the **Audio** tab of its
[Media Inspector](media.md#the-media-inspector): a video's (when its file has sound) and a
piece of the audio bin's. The tab has:

- **Ramp In** and **Ramp Out**, in seconds, none unless set: the sound comes in over
  the first, from where it starts to play, and goes out over the last before where it
  stops (its out point, or the end of its file). One that goes round has no ramp out.
- **Channels**: the media's own channels down the left (with what each is, where the
  file says: FL and FR, LFE and so on), the app's across the top, a cell lit where the
  one goes to the other, as in Channel Routing; **Mapping** has Auto and Clear. Until a
  cell is clicked it is Auto: the media's first channel to the app's first, its second
  to the second, and so on, round again from the first for any beyond the app's last;
  and sound of one channel to the first two.
- For each of the media's channels, how loud it is, in decibels (0 is as the file has
  it), and **M** to mute it.

These are ProPresenter's fields of the media, and are kept where ProPresenter keeps
them. A change taken back (a cell clicked and clicked again, a ramp set and set to none
again), while the app is running, leaves the file exactly as it was, ProPresenter's own
entries and all. What is playing is left as it is: a change is heard the next time the
media is played.

**The app mixes its own sound**, all of it but a video input's, so that it can send any
channel anywhere (Qt's players, which the app plays media with, send a file's first
channels to a device's first, and do nothing else). Each player's sound is taken as it is
decoded, on the player's own thread, brought to the device's rate and kept in step with
the device's clock, and mixed on a thread of its own that nothing else uses, so that a
busy moment in the operator window, a slide change that takes long to draw, does not make
a gap in the sound. The device is opened when something is first to be heard, and let go
of half a minute after the last sound. The cost is a little delay: sound is heard after
its picture by as much as the mixer keeps in hand, which is as much as a player hands
over at once and the device takes at once, and a little more, and then by what Qt, the
system's sound service and the card hold. The log says how much the app's share is for
each piece of media, and so do the screens'
[performance statistics](screens.md#performance-statistics): on the tests' sound device
it was 79 ms for a video's sound (49 kept by the mixer and up to 30 by Qt; the device
there takes sound 21 ms at a time), and what the sound service and the card hold is not
told to the app. A screen's [video delay](screens.md#video-delay) can hold the picture
back to meet it.

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
  they fall; and that its meter is of what goes to the device.
- Of the app's channels: what ProPresenter calls the Channels box, and what it starts a
  workspace with (here two, called Channel 1 and Channel 2, which are written to the file
  only when they are first changed); that a channel that is muted is not heard even when
  it is soloed; what the M over them all does when all are muted; the test tone's note
  (1 kHz) and how loud it is (20 dB below full); that ProPresenter's Auto for media
  sends channels beyond the app's last round again from its first (as its files have
  it, where it has written a mapping for a file of sixteen channels in a workspace of
  two), and sound of one channel to the first two; that a channel's volume is in
  decibels; what a ramp is (here the sound alone, coming in from where it starts and
  going out before where it stops, on the curve of the fades), and that one that goes
  round has no ramp out; how the window and the tab are laid out, and the letters'
  colours.
- That a video input's sound is not sent through the app's channels: it goes to the
  device by Qt's own way, its first two channels to the device's first two.

Not done: see [TODO](../todo.md).
