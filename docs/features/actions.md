[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Actions

A slide can do more when it is shown than show itself and its media. In ProPresenter's
files a slide's cue is a list of **actions**, and the app runs the ones it knows as
the slide goes live: the slide first, and then its actions, in the order the cue has
them. (So an action that clears the slide clears that slide, which is how a cue is
made that shows nothing of its own. Such a slide is still the one the show is at: it
is marked as the live one, and the arrow keys go on from it.)

- **Timer**: start, stop or reset a [timer](timers.md), or reset and start it, and
  optionally set it up anew first (as a countdown of so long, a countdown to a time of
  day, or an elapsed time).
- **Clear**: everything, the slide, the media, the props, the messages or the
  [announcements](announcements.md). (And the audio, by dragging its clear button onto
  the slide.)
- **Clear Group**: clear one of the workspace's [clear groups](clear-groups.md), which is
  to say those of its layers that there are here.
- **Stage**: give each stage screen one of the workspace's [stage layouts](stage-layouts.md),
  or leave it as it is.
- **Prop**: put a [prop](props.md) on (Trigger) or take it off (Clear).
- **Macro**: run a [macro](macros.md).
- **Audience Look**: make one of the workspace's [looks](looks.md) the live one.
- **Audio**: play a piece of the [audio bin](audio.md), music or a sound effect. It is
  made by dragging the piece onto the slide.
- **Media Playlist** and **Audio Playlist**: play a playlist of the [media bin](media.md)
  or of the [audio bin](audio.md) from its first row (see below).
- **Message**: show one of the workspace's [messages](messages.md), with words of the
  action's own for its tokens (see below).
- **Video Input**: put one of the workspace's [video inputs](video-inputs.md) on the video
  input layer.
- **Communication**: send a MIDI note, a RossTalk Custom Control or an HTTP request to
  one of the workspace's [devices](devices.md).

Each action a slide has is a small icon in the top left corner of its thumbnail, after
the icons for its hotkey and its media. A right click on a slide offers **Add Action**,
which leads to the kinds above: Clear, Clear Group, Prop, Macro, Audience Look, Media
Playlist, Video Input, Audio Playlist and Message to lists to pick from (a prop by its collection,
then Trigger or Clear; a macro by its collection; a playlist in its folders), Timer,
Stage and Communication to a small panel to fill in. Under it is **Remove Action**,
which lists the actions the slide has. A right click on an action's icon says what the
action is and offers to change it, for a timer, a stage or a message action, or to
remove it; for one that plays audio it has Stop or Loop, and Music or Sound Effect; for
one that plays a playlist, Always Retrigger; for a video input, the inputs to change to.

A row of a menu that leads to a menu of its own has an arrowhead at its right, and its
menu opens beside it when the pointer rests on the row (or on a click). The menu it
came from stays where it is, and each is kept inside the window.

Whatever is listed in the show controls can also be **dragged onto a slide**: a timer
or the stage screen, which bring up their panel; a prop, which asks whether it is to
be triggered or cleared; a macro, which needs nothing more; a piece of audio, which
needs nothing more either; and a message, out of the list on its tab. So can the
**clear buttons** beside the preview, each of which gives the slide the action that
clears its layer, or its group; a playlist, out of the list of either bin (a folder
of playlists is not taken); and a video input, out of the media bin's Video Inputs.
Each of those can be dropped on a macro as well.

**A playlist.** An action for a playlist of the media bin or of the audio bin plays it
as a click on its first row in the bin would: the first that is on this computer, or
the row the action names, if ProPresenter made it so. A playlist that is playing
already, which is to say whose media is on the media layer, or whose music is the
music, whichever of its rows that is, is left to play on when such an action is run
again. **Always Retrigger**, in the action's menu and off to begin with, starts it again
from its first row instead. Music played so goes on to the next of its playlist, as
music clicked in the bin does. The playlist is found by its id, or failing that by its
name: ProPresenter's own actions name it by both, and two of the maintainer's name a
playlist by its name and an id it no longer has.

**A message.** An action that shows a message gives words of its own to the message's
tokens that are given words (a Name, a Room), and the message is shown with those; a
token it gives nothing to shows what it was last given on the messages' tab, and so do
the timers and the clock. Picked from Add Action, or dropped on a slide, a message with
such tokens brings up a small panel with a box for each, starting from what each was
last given on the tab; one with none needs nothing more. The words are the action's:
the message's own, on its tab, are as they were.

What is a guess about these three, from ProPresenter's file format and its own actions
in a workspace of the maintainer's and not from ProPresenter running: that a media
playlist's action is written as an audio playlist's is, with the media bin's type (none
of the maintainer's files has one); that a playlist starts at its first item; that
"playing already" is the playlist and not the row it starts at; that what the file
calls Select Playlist (which none of ProPresenter's own that have been seen has) is
only which playlist the bin shows, which is not done here; and that a message shown by
an action leaves the words on its tab as they were. A sound effect at the head of an
audio playlist plays over the music, as any sound effect does. The pictures for the two
playlists are this app's own: ProPresenter's pack of its pictures has none.

ProPresenter has many more kinds of action than these (capture, and others for the
features this app does not have). A slide or a macro that has one keeps it: it is shown as a
fainter icon, said for what it is, written back untouched and not done. An action for
a timer, a prop, a macro, a layout, a look or a clear group that is not in the
workspace does nothing, and the [log](log.md) says so; so does one that plays a sound
file that is not on this computer.

A stage action names the stage screens it is for, and its panel has a line for each
stage screen of the workspace: the layout that screen is to be given, or that it is to
be left as it is. An action made here names every stage screen, as ProPresenter's do.
One that names no screen of this workspace at all (it was made where the screens were
others) is taken to be for the first stage screen.
