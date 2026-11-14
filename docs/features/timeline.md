[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Timeline

ProPresenter's timeline: a presentation's own run of things to do at set times, played
from its start as a clock goes, so that a song played to a track runs its slides, its
backgrounds and its macros by itself. It is the presentation's, kept in its file as
ProPresenter keeps it, so a timeline made in ProPresenter plays here, and one made here
plays there.

## Opening it

The **Timeline** button is at the right of the line over the slides, after the
Arrangements button, where ProPresenter has it: two blocks with a playhead between them.
It opens the timeline of the presentation being viewed under that line, and shuts it;
it is lit while the timeline is open, and whether it is open is kept from one start to
the next.

As in ProPresenter, the timeline has a ruler of minutes and seconds across the top, and
four tracks under it, each with its picture at the left:

- **Slides**: each slide on the timeline, as a block from its time to the next slide's,
  in its group's colour, with the group's name on the first of a run of it and the
  cue's own name under that ("Cue 3", as ProPresenter names one).
- **Media**: pictures and videos, each as long as it lasts.
- **Audio**: pieces of audio, each a green bar as long as it is, named by its file.
- **Actions**: macros, clears, looks and the rest, a block each.

The **playhead** is the white line down through them. Under the tracks are, at the left,
**Rewind**, **Play** (**Pause** while it plays), **Record** and **Loop**, and where the
playhead is; at the right the **Duration**, typed in seconds, and a slider that spreads
the time out or draws it in. The view keeps the playhead in sight as it plays, and the
wheel scrolls it along.

## Playing it

**Play** plays the timeline from the playhead: each cue is done as the playhead reaches
it, a slide going live as a click on it would take it, media and audio started, an
action run as a macro's actions are. At the end of its Duration it stops, or with
**Loop** on it goes round from the start. **Pause** stops it where it is, and the music
it started stops with it, to start again where the timeline is when it plays on.
**Rewind** puts the playhead at the start, and a click on the ruler, or on a track where
it has nothing, puts it there; dragging along drags it. A click on a cue puts the
playhead at it.

Played from part way, for rehearsing from the second verse, the timeline does what it
would have been doing there: the slide of the cue before the playhead goes live, and
the audio and the video whose time is under the playhead start where the playhead is in
them. Actions before the playhead are not run.

A slide that comes up more than once in the arrangement being shown (a chorus sung
three times) is taken at its next place after the slide that is live, so that the
timeline goes through the song in order.

Two timelines can play at once, as in ProPresenter: that of the presentation on the
slides and media, and that of a presentation sent to the announcements.

**What stops it**, besides Pause and its end:

- a clear group with **Stop Timeline** ticked for it, in the clear groups' settings
  (ProPresenter's own Clear All group, which a workspace starts with, stops both);
- everything cleared by a MIDI note's Clear All, or by F1 in a workspace with no clear
  groups, and another workspace opened;
- a slide of another presentation taken live on its layer. Opening another
  presentation, and looking at its slides, leaves it playing.

Stopped by any of those, what it started plays on, as anything cleared is cleared by
the clear itself.

## Recording

With **Record** on, whatever is triggered by hand is put on the timeline where the
playhead is, playing or not: a slide of the presentation clicked, a picture or a video
of the media bin, a piece of the audio bin, a macro. So a song is recorded by putting
its track on, starting it, pressing Play and clicking the slides along with it. What is
recorded is shown at once, a little paler, and is written to the presentation's file
all at once when Record is turned off (Record lit red means it is on: turn it off when
done, or every click goes on the timeline too, as ProPresenter's help warns).

## Changing it

- A **slide is carried up onto it** from the slides under it, as in ProPresenter, and
  let go where it is to go: it goes on the slides track at that time, wherever on the
  timeline it is let go. While it is carried, the block it will be (its group's colour,
  its group's name and its cue's) goes with the pointer, and the slides track is
  outlined; over the timeline, a white line and the time on the ruler say where it would
  land. Held near either end of what is in view, the timeline scrolls on, for a time out
  of sight. Let go anywhere else, nothing is done. A slide can be carried only while the
  timeline is open: with it shut, a slide is not dragged at all. (While it is open, a
  drag that starts on a slide carries the slide and does not scroll the slides; the
  wheel and the scroll bar do.)
- A **picture or a video of the media bin, or a piece of the audio bin, is dragged
  onto it** in the same way, and goes on the media track or the audio track at the time
  where it is let go, whichever track that is over, as the action that plays it. While
  it is dragged its track is outlined, and over the timeline the block it will be is
  drawn there, as long as the piece lasts (ten seconds for a picture or a video, whose
  length is not known until it is on).
- A cue is **dragged** along its track to another time.
- A cue's own menu (the right button) puts the playhead at it, or **Deletes** it.
- The right button on a track where it has nothing adds something there: on the
  actions track, any action, from the same menu as a slide's actions; on the media
  track and the audio track, a picture, a video or a piece of audio of the bins; on
  the slides track, a slide of the presentation.
- The **Duration** is typed; **Loop** is switched on and off.

**Against a drag by accident**, as everything dragged in this app's operator window, a
slide is carried, a cue moved, and a picture or a piece of audio dragged, only once the
pointer is twelve pixels from where it was pressed with the button held for 150 ms (this
app's own; the maintainer asked for a light guard that is not hard work on a trackpad).
Moving a little way first is how desktops tell a drag from a click, and twelve pixels is
a little more than most, for slides that are clicked all through a service. The moment
held is for a click made while the hand is still moving the pointer, which can cover any
distance in the tenth of a second a click lasts: that stays a click, on the slide if it
is let go on it, and nothing is carried. A drag made on purpose has held the button far
longer by then, on a trackpad as with a mouse, so neither is felt: nothing has to be
held still, and there is no long press.

Every change is written to the presentation's file at once, as ProPresenter writes
one: a slide by its id and named "Cue" and its place among the presentation's slides as
its groups store them; a piece of audio or media as the action that plays it, named by
its file, with its length. Nothing else in the file is touched.

## From elsewhere

- **The API**: ProPresenter's play, pause and rewind of a timeline, and how one is
  going, for the presentation that is live or open, by its UUID, by its place in a
  playlist, or the announcements' (see [The API](api.md)). ProPresenter Remote and
  Bitfocus Companion's module use these.
- **MIDI**: Start, Stop and Rewind Timeline in the MIDI Map work the timeline of the
  presentation open (see [Devices](devices.md)).

## Not here yet

- **Timecode.** ProPresenter's timeline can follow SMPTE timecode, from its Timecode
  box beside the Duration; that is to come, and the box is not shown until it does
  something. A timeline that follows timecode in ProPresenter plays by its own clock
  here, and what it says of its timecode is kept in the file as it is.
- The end of the timeline dragged to lengthen it: the Duration is typed.
- Actions dragged onto it (a macro, a clear button): they are put on by the right
  button and by recording.
- Carrying several selected slides at once: one is carried at a time.
- The playhead moves ten times a second as it plays, a few pixels at a time, so as not
  to have the operator window drawn at every frame for it: smoother than that is not
  worth the old laptop's time.

## How the files are read

ProPresenter's format has two lists of cues for a timeline. The file ProPresenter 22
wrote, which this was made from, had its cues in the newer one; the maintainer's reading
is that older versions write the older list, and newer ones the newer and still read the
older. So both are read, the newer wherever it has anything, and what is written goes in
the newer; a timeline that has only the older is copied into the newer as it is first
changed here, the older left as it was.

## What is a guess

ProPresenter's timeline was read of in its help and its blog, and seen in one
screenshot and one file of the maintainer's; how it behaves was not watched. So these
are guesses:

- what a timeline played from part way does (the slide before, the audio and video
  started part way, the actions before not run), and which place of a slide that
  comes up more than once is taken;
- that Play at the end of a timeline plays it from the start, and that Pause stops the
  music it started;
- that a slide of another presentation stops a timeline, and that MIDI's Clear All
  stops both;
- how media and actions are named on it, and the actions track's blocks;
- the menus of a cue and of an empty track, and where the clear groups' Stop Timeline
  ticks are;
- that what is let go over any of its tracks goes on its own (a slide on the slides
  track, a picture on the media track, audio on the audio track), and what a carried
  slide looks like (ProPresenter's is likely a picture of the slide);
- that the API's `has_timeline` means a timeline with something on it, and that MIDI's
  Start Timeline is the presentation open and not the one live, and its Stop a pause;
- what the old list is for, as said above.
