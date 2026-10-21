[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Media

Media is triggered with a slide, when the slide's cue has some, or by a click in the
media bin. How it then behaves is one of two things, as in ProPresenter:

- A **background** stays. It plays on under whatever slides come next, and a video
  starts again when it reaches its end. Triggering it while it is already what is
  playing leaves it playing; it is not started again. So every slide of a song can
  carry the song's background. (That holds for a video that is going round. One set
  to stop at its end is started again each time it is triggered, there being nothing
  of it to play on; and so is one that has been set to play another way since it was
  started, which is how such a change takes effect.)
- A **foreground** is for the moment. A video plays once and stops on its last frame,
  and the next slide that is triggered takes it off, whether or not that slide has
  media of its own.

Either gives way to the next media that is triggered, of either kind.

A click on a slide with **Alt** held shows the slide, and does its actions, without
the media it brings, as in ProPresenter; while Alt is held the thumbnails are drawn
without their media, to say so. And the other way about, which is the app's own: the
**Media** caption in a slide's right-click menu can be clicked, where the slide has
media, and plays that media by itself, as a click on the file in the media bin would,
leaving the slide layer as it is.

Which of the two it is belongs to the place the media is used, so a slide's media and
the same file in the media bin are set separately. A right click on the slide, or on
the file in the bin, sets it, and the icon on the thumbnail shows which it is: two
layers, the one behind solid for a background, the one in front for a foreground.

**How a video plays on from its end** is a setting of its own, as it is in
ProPresenter: it **stops** on its last frame, or **loops** for good, or loops for a
**play count**, or loops for a **length of time**, after which it stops where it is. A
slide with a video has a second icon for this under the first: a square for stop, an
arrow going round for loop, with the count or the time after it. Media dropped on a
slide starts out as ProPresenter would have it, a background looping and a foreground
stopping.

Each of the two icons is its own thing to right-click, and its menu changes what it
shows, with the way it is now ticked: Background or Foreground for the one; Stop,
Loop, Loop for Play Count or Loop for Time for the other, the last two leading to a
few counts and times to pick from. Changing the one leaves the other as it is. A
plain click on an icon is a click on the slide, as anywhere else on it. (This is the
app's own shortcut. In ProPresenter these are set in the Media Inspector, which is
here too: see below.)

In the files this is what ProPresenter keeps: the layer a media action is on, and how
its video plays on. Media set up there behaves here as it was set there.

**Sound.** A foreground video is played with its sound, through the system's audio
output or the device chosen for sound in the settings (see [Audio](audio.md)), at the
volume ProPresenter has for it (full, unless it was turned down there).
A background video is silent, whatever sound its file has: it is there to be looked at
behind the words. The sound follows the picture through a transition, fading in or
out with a dissolve and starting or stopping at once with a cut. There is nowhere yet
to set this for one piece of media (see the TODO list); until there is, making a video
a foreground or a background is what gives it its sound or takes it away.

**Bringing media in.** Media gets onto a slide by being dragged there, out of the media
bin or, as files, straight out of the file manager. Where it is dropped settles what it
becomes, whichever of the two it came from:

- **On a slide**, it is the media that slide triggers, as a background: something to go
  behind the slide's words. If the slide already triggers media, the new file takes its
  place and plays as the old one did, so this week's video dropped on last week's is
  still the foreground that was.
- **Between two slides** (or before the first, or after the last), it gets a slide of
  its own there, with nothing on it, that triggers it as a foreground. That is how a
  video takes its turn in the run of a presentation, and it is what ProPresenter makes
  of media dropped between slides. Several files dropped together get a slide each.

A white outline on the slide, or a white line in the gap, shows which it will be.
Files dragged from the file manager into the media bin are added to a media playlist:
the one they are dropped on in the list, or the one being browsed, at the place among
its thumbnails where they are dropped. Files are referred to where they are on disk,
never copied or moved, and anything in a drag that is not an image or a video the app
can show is left out.

First in the bin's list is **Video Inputs**, as in ProPresenter: the workspace's
[video inputs](video-inputs.md), which a click puts on a layer of their own under the
media.

A **media playlist** can be dragged out of the bin's list onto a slide or onto a macro,
or picked from Add Action, under Media Playlist: that gives it an action that plays the
playlist from its first row. Run again while the playlist is playing, it leaves it to
play on, unless its Always Retrigger is on. See [Actions](actions.md).

<a id="next-cue"></a>**Next Cue.** A video in a media playlist can be followed by another of
its playlist when it has played to its end, as in ProPresenter: **Next Cue** in its
right-click menu leads to **None** (it stays on its last frame), **Next** (the row after
it), **Random** (any other row), **First** (the playlist's first row), and, under a line,
each of the playlist's other rows by name; what it has now is ticked. Anything but None
makes it stop at its end, since a video that goes round never gets there for anything to
follow. It holds only where the video was played from its playlist, by a click in the bin
or an action that plays the playlist: the same file triggered by a slide has no playlist
to go on in. A picture has no Next Cue, never coming to an end. It is kept in the media
playlists file as ProPresenter keeps it, the row's cue's "completion target".

Guesses, none of these having been seen in ProPresenter: that a row chosen by name is
named in the file by the id of its cue; that Next stops at the playlist's last row and
does not go round to its first; that Random never picks the row that has just played,
where there is another; and that a row with no file to play is passed over, and one named
that has none is nothing to go to.

A video's thumbnail is a picture from about five seconds into it (or from the middle of
one shorter than ten seconds), and a little later than that if the picture there is
nearly black: a great many videos fade in from black, and their first frame says
nothing about which video it is. Unless a moment has been chosen for it, in its
[Media Inspector](#the-media-inspector): then it is the picture from that moment. A
thumbnail also shows the media as it is set to be shown: cropped, turned and flipped,
fitted to the thumbnail as to the screen, blurred, and with its effects, those that move
by themselves held still.

Beside the preview, against the window's right edge, are the **clears**, in two columns
as ProPresenter has them, each button a picture and no words. The first column is the
workspace's [clear groups](clear-groups.md), a button each, sharing the column's height,
the first of which is what F1 clears: to begin with that is Clear All, as high as the
preview. The second is the layers, in ProPresenter's order down it: the audio (F5), the
messages (F6), the props (F4), the announcements, the slide (F2), the media (F3) and the
[video input](video-inputs.md), each red while there is something there for it to clear.
Under the preview is the **transport**, for the video on the output: how far in it is and
how much is left, a slider that can be dragged to move it, and buttons to go back to the
start, to play or pause, and to skip fifteen seconds back or on.

## The Media Inspector

**Inspector** in the right-click menu of a slide that has media (and in the menu of its
media icon), or of a row of the media bin, brings up ProPresenter's Media Inspector over
the window, for that piece of media; for an element filled with a picture it is the
**Inspector…** button in the editor's Fill section. On the left is the media as it will
be shown, in the shape of the screen (or of the element), with every setting as it is
set, effects and all; for a video, under it, a player of its own, silent, with play and
pause (and the space bar), a bar to move through the video on, the time gone and the
time left, the part of it that plays marked in blue under the bar and the moment its
picture is taken from as a white mark over it. On the right are three tabs:

- **Information**: where the file is, with **Show in Folder** (which opens the folder in
  the desktop's file manager, the file picked out where the file manager can do that)
  and **Choose…**, for another file in its place; for a video, its thumbnail, with
  **Set to Current Frame** to take it from where the player is and **Automatic** to
  leave it to the app; and what the file is.
- **Properties**: **Scaling** (Scale to Fit, Scale to Fill, Stretch to Fill, Scale +
  Blur, as [ProPresenter names
  them](https://support.renewedvision.com/hc/en-us/articles/360011694174-Media-Scaling-Options):
  fitted whole, filling the screen and cut off at its edges, stretched to it, or fitted
  on a blurred copy of itself that fills it); **Rotation**, a quarter at a time;
  **Flip**, across and up and down; **Cropping**, by so many pixels of the media at its
  own size from each edge. For a video, **Playback** (Stop or Loop; a loop for a count
  or for a time is set from the slide's playback icon, and is a Loop here), and
  **Trim**: the in point and the out point, each typed or set from where the player is,
  with **Reset Trim**. A trimmed video plays from its in point to its out point, which
  are its start and its end: it loops between them, or stops at the out point, and the
  Video Countdown counts to it.
- **Effects**: the media's effects, in order, each switched on and off, taken off, and
  adjusted (a slider for each number, a swatch for each colour, and **Reset Effect**).
  **Add Effect** lists ProPresenter's, by its names: Adjust Color, Blur, Color Blobs,
  CRT Monitor, Kaleidoscope, Old Film, Ripple Center, Sepia, Vignette and the rest,
  thirty-eight of them. Each is a pass over the media's pixels as it is drawn, which
  costs only media that has one; those that move by themselves (ripples, stars, an old
  film's flicker) are drawn at every frame while they are shown.

Every change is saved to the file as it is made, as the menus' changes are, and is in
the fields ProPresenter keeps it in, so the media looks the same in ProPresenter (and
media set up in ProPresenter's inspector looks here as it was set there). In the editor
the inspector's changes are the editor's, undone as the rest are. What is on the screens
is left as it is: a slide or a row shows its media as now set the next time it is shown,
as after the editor. **Done**, Esc or a click outside put it away.

The names, what each effect can be adjusted by, and the ids the files know them by are
ProPresenter's. The drawing is not: as with the [transitions](transitions.md), each effect
is drawn by a shader written for this app to give the same kind of change, so they are
equivalents and not copies, and will not match ProPresenter's pixel for pixel.

**What is a guess, or not done:**

- How the panel is laid out and what its buttons say; that its player is silent; and
  that what is on the screens is not changed as the media is set (ProPresenter's may).
- That Scale + Blur is kept as Scale to Fit with the blur switched on, the field beside
  it in the file; that a crop is of the media as it is in its file, before it is turned
  or flipped; that the flips are of the media as it is shown, once turned; that the
  sizes of the effects that are sizes in pixels are of a 1080-line screen; and that an
  effect a file does not say is on is off (the user's own files have such effects).
- A moment of nought chosen for a video's picture is no moment chosen: the very first
  frame cannot be its picture. ProPresenter writes -1 for none, and Automatic does too.
- Not there: how loud a piece of media is, its alignment, delay and play rate, its own
  transition, effect presets, Background Blur and Background Invert (known, so that a
  file keeps them, but not drawn: they work on what is behind the media), the audio tab,
  playback markers, the inspector for the audio bin, and several pieces of media at
  once. The editor's own thumbnails show media cropped, turned and flipped, but without
  its effects.
- Another file in its place keeps how the media was fitted, turned and flipped, its
  effects and how a video plays on, but not its crop, its trim or its picture's moment,
  which were the other file's.
