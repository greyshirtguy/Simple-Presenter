[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Timecode

ProPresenter's Timecode: a [timeline](timeline.md) that follows the timecode a playback
rig or a desk sends, so that a song's slides, and a video, keep to a track that is played
somewhere else, and go wherever in it the rig is sent. What ProPresenter's help says of
it ("Timecode in ProPresenter") is what is here; what its help does not say is guessed,
and listed at the foot.

## What is listened to

Linear timecode (LTC, SMPTE's), which is timecode as sound: a rig sends it down an audio
cable, and it comes into one channel of a sound input of the computer. That is what
ProPresenter takes too: its settings ask for a sound device and a channel of it. The
four formats are ProPresenter's: 24, 25, 29.97 and 30 frames a second.

It is read out of the sound by the app itself. A signal that is turned over, quiet,
sitting off centre or with hiss on it is still read, and so is a rig running a little
fast or slow; nothing has to be told how loud it is.

## A timeline that follows it

Under a presentation's timeline, at the right, where ProPresenter has them:

- the **Timecode** box, ticked for a timeline that follows timecode;
- beside it the timecode the timeline **starts** at, written as ProPresenter writes one,
  `01:00:00;00` (hours, minutes, seconds, frames). Click it and type another, and press
  Enter: `1:00:00` is an hour, `3;12` three seconds and twelve frames, and digits alone
  are read in pairs from the right (`01000000` is an hour). What is no timecode is said,
  and the start left as it was.

Both are kept in the presentation's file, where ProPresenter keeps them, so a timeline
set up there follows timecode here and one set up here does there.

Beside the start is a small **clock**, which is this app's own: it opens the Timecode
window (ProPresenter opens its own from its View menu). Its hand is green while timecode
is running. And while the timecode has the timeline, **following timecode** is said
beside the time under the tracks.

## The Timecode window

- **The timecode coming in**, in large figures.
- Under it ProPresenter's word for how it is: **Playing** while timecode runs,
  **Stopped** while it does not, and **Not Engaged** while nothing is listened for and
  nothing is triggered. The word is clicked to stop listening, and clicked again to
  listen. It starts at Not Engaged, and stays so until it is clicked: until then no sound
  device is opened. Whether it listens is then kept from one start of the app to the next.
- **Playlist**: the playlist the timecode triggers. Its presentations are listed under
  it, each whose timeline follows timecode with the timecode it starts at, and the one
  the timecode is in now lit.
- The **cog** shows the settings: the **Input Device**, the **Channel** of it, the
  **Format**, and an **Offset**, in seconds, which is added to the timecode that comes
  in (a minus number takes off). The **speaker**, held, plays the signal to the
  computer's own sound device, quietly, to hear that it is coming in.

Under the settings a line says what the signal looks like, which is this app's own: how
loud it is, and how many frames a second it is going at, which is what to set the Format
to (it is said in orange if the Format is another).

## What following is

While timecode is listened for and a playlist is chosen, each presentation of that
playlist whose timeline has its Timecode box ticked is the timecode's to play: for as
long as the timecode is within its timeline (from its start, for the timeline's
Duration), the timeline is where the timecode says it is.

- **The timecode starts**, or **jumps**: the timeline it is now in goes to where the
  timecode is in it, and does what is in force there, as a timeline played from part way
  does: its slide goes live, its audio starts where the timeline is in it, and its video
  starts there; or, if that video is the one playing already, the video is sent there
  (see below). A slide that is live already is not triggered again.
- **While it runs**, the timeline's cues are done at their times, and twice a second
  the timeline is kept to the timecode, so that the two clocks cannot drift apart.
- **The timecode stops**: the timeline pauses, its audio stops, and its video is held
  on the frame it is at. Started again from where it stopped, they go on from there.
- **The timecode goes somewhere no timeline is**, or into another presentation's
  timeline: the one it left pauses (its video plays on, as when a timeline is paused by
  hand), and the one it is now in, if any, follows.

A start or a jump is believed at its second frame, and a stop three and a half frames
after the last: so a single frame read wrongly down a poor cable does nothing. With the
sound system handing the sound over in pieces of a fiftieth or a twenty-fifth of a
second, a jump is acted on about a tenth of a second after the rig made it. Where the
timeline is then put is where the timecode is at that moment, not where it was when it
jumped: the tenth of a second is not lost from then on.

**What is done by hand is left alone** until the timecode next starts or jumps: a slide
of another presentation taken live, or a clear, stops the timeline, as it stops one
played by hand, and the timecode going on as it was does not start it again. The
operator's own view is not changed by any of this: the presentation the timecode is in
is not opened over what is being looked at.

At **29.97** frames a second, where a second of timecode is not quite a second, a
timeline's place is counted in frames from its start and turned into time at the frames'
real rate, by whichever count the signal says it uses (dropping frames, or not): so a
video on the timeline keeps to the timecode for as long as it runs.

## A video on the timeline

A video on a timeline that follows timecode goes where the timecode goes. When the
timecode jumps, the video that is playing is not taken off and put on again: it is sent
to the new place as it plays, and the picture that was showing stays until the new one
comes.

What that costs is the video's own doing. Most videos keep a whole picture only every
so often (every eight seconds or so) and, for the frames between, only what has changed;
to show a frame the computer has to go back to the whole picture before it and work
forward. So the new picture comes a moment after the jump, and how long a moment
depends on how the video was made. Measured on the 2017 laptop this is made on, with
its graphics chip decoding, ten jumps to each:

| The video | The new picture came after |
| --- | --- |
| 1080p, a whole picture every 250 frames (as most programs make one) | 13 to 312 ms |
| 1080p, a whole picture every 30 frames | 9 to 27 ms |
| 4K, a whole picture every 250 frames | 14 to 933 ms |
| A 4K motion background as they are sold (20 Mbit/s, a whole picture every 25 frames) | 16 to 52 ms |

The short times are for places just after a whole picture, the long ones for places just
before the next. The player makes the moment up: a quarter of a second after its
picture came, every one of those forty jumps had the video back in step with its
timeline to within three thousandths of a second. So a video with a whole picture every
second or so (which motion backgrounds mostly have, and which any video can be made
with) goes where the timecode goes in a frame or two; one with a whole picture every
eight seconds stands on its old picture for up to a third of a second first, or up to a
second at 4K.

Each jump is written in the log (`~/Documents/SimplePresenter/Logs/`): how long the
picture took, and how far out the video was once it had settled. If it was more than a
frame and a half out, it is put right: held on its picture if it was ahead, sent again
if it was behind. And every few seconds while the timecode runs the video is looked at,
and sent again if it has come two frames from where the timeline is (a rig's clock and
the computer's are never quite one).

The same is done for a timeline whose playhead is moved by hand while it plays.

## Trying it with no timecode

The Input Device list has, first, **Made here (to try it)**: a timecode of the app's
own, which is not ProPresenter's. With it chosen, the window has a row to **Play** it,
**Stop** it, and send it to a timecode typed (**Go to**), running or stopped. What
follows it is what follows a rig's, by the same way: so a timeline, and a video on one,
can be seen following and jumping on a computer with nothing plugged in.

## On a stage layout

A text box can be set to show ProPresenter's **Timecode** (the timecode coming in) or
**Timecode Status** (Playing, Stopped, Not Engaged): in the editor, under what the box
shows, as its other links are. A layout made in ProPresenter with such a box shows the
same here.

## From elsewhere

The [extended API](extended-api.md) gives a presentation's timeline its `timecode` and
`timecode_start`, and takes them. ProPresenter's own API has nothing of timecode.

## Where it is kept

- A timeline's Timecode box and its start: in the presentation's file, as ProPresenter
  keeps them.
- The input device, its channel, the format, the offset and whether timecode is listened
  for: in the app's own settings, as this computer's. (ProPresenter keeps its own in a
  file of its own, which names a sound device of the Mac or PC it is on; that file has
  not been seen, and is neither read nor written.)
- The playlist chosen: in the app's settings, for each workspace.

## Not here

- Timecode by MIDI (MTC) or over a network: ProPresenter's settings have a sound device
  and a channel, and so has this.
- Timecode sent out.
- A piece of audio on a timeline is started again where the timeline is at a jump, and
  not sent there as a video is: there is a short gap in it.
- A video on the announcements' timeline is started again at a jump, likewise.

## What is a guess

ProPresenter's help says that the playhead is controlled by timecode, that a playlist
is chosen whose slides are then triggered at their times, and what its window and its
settings have. No picture of its window was seen, and nothing of how it behaves was
watched. So these are guesses:

- how the Timecode window is laid out, and its words where the help does not give them;
- that a timeline's start is kept in its file in seconds, and that the Offset is in
  seconds;
- that a timeline is the timecode's only while its presentation is in the playlist
  chosen, and that the timecode leaving a timeline pauses it;
- that a jump is believed at its second frame, and what is done at 29.97;
- that what is done by hand is left until the timecode next starts or jumps;
- that a video is held when the timecode stops, and everything about how one is sent
  somewhere at a jump: ProPresenter's is known only by how it looks, which is that no
  frame is seen to go missing;
- where Timecode and Timecode Status stand in the menu of what a text box shows.
