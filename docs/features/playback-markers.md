[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Playback markers

A playback marker is a point in a video or a piece of audio with actions that run when
playback reaches it, as in ProPresenter: lights at the start of a video, a stage
layout with a countdown partway through, a lower third at a set moment, "Enter Stage
Now" for the speaker near the end.

They are on the video of a slide, on a video of the media bin, and on a piece of the
audio bin. Each place has its own: the same file on a slide and in the bin has the
markers of each. A picture has none, and nor has media that fills a shape.

## Making them

In the **Media Inspector** (Inspector in the right-click menu of the slide, of the row
of the media bin, or of the piece of the audio bin), the **Playback Markers** tab lists
the markers in the order of their times: each with its colour, its time, its name and
the icons of its actions. A piece of audio has this tab and no other.

- **Add Marker** puts one where the Inspector's player is. A right click on the bar
  under the picture puts one where it was clicked (**Add Marker Here**).
- A click on a marker takes the player to it.
- A right click on a marker has:
  - **Rename**: its name, typed where it stands;
  - **Color**: red, orange, yellow, green, blue, purple, pink or white, or **Other…**
    for any colour, picked with the system's colour picker;
  - **Add Action**: the kinds of action a slide can have (see [Actions](actions.md)),
    as Add Action has them for a slide;
  - **Remove Action**: one of its actions taken off;
  - **Duplicate**: another at the same time, with the same actions;
  - **Move to Playhead**: to where the player is, for fine timing;
  - **Delete**.

A new marker is a colour, picked at random, that the media's other markers have not
(any of the seven but white, once each has one), so that markers added one after
another are told apart.

Over the bar, each marker is a flag of its colour, a little shield whose point at the
foot is the marker's place, sitting on the top of the slider's handle so that it covers
neither the handle nor the bar. A flag is **dragged** along the bar to move its marker,
the player following it to show where it is going, and the time it is going to shown
over it; let go, the marker is kept there. A click on a flag takes the player to it,
and a right click has its menu.

Every change is kept at once, in the file the media is in: the presentation, or the
bin's playlist file, as ProPresenter keeps them, on the action that plays the media.

## When they run

As playback reaches a marker its actions run, as a macro's do, and the log says so.
That holds for the video on the media layer, the video under an announcement's slide,
and the audio bin's pieces. A video that goes round runs its markers again each time
round.

The [transport](media.md#the-transport) draws the markers of what it works as the same
flags, smaller, over its bar, where a click on the slider anywhere but on a flag is the
slider's as ever. Under the pointer a flag shows its time and name. A flag is dragged to
move its marker, kept where it is let go; the media, being live, is not moved with it.
A right click has the same menu as in the Media Inspector, its **Rename** typed over the
media's name. That holds for media played from a slide, from the media bin or from the
audio bin, which is where its markers are kept; media played any other way has its
flags drawn and not changed.

A marker changed while its media plays, on the transport or in the Media Inspector, is
where that media runs it from then on, and where the next showing of it has it.

The transport also has a **Marker** menu at the right of its buttons, listing them by
time and name: one picked takes the media there and runs its actions, once.

## On the stage

A text box can show a marker's name or the time to it, as ProPresenter's Linked Text
"Playback Marker" does (see [Stage layouts](stage-layouts.md) and
[Editing](editing.md)): **Marker Name** or **Time to Marker**, of the presentation's
media, the announcements' or the audio's, for the **first**, **previous**, **next** or
**last** marker, or the one with a given name, optionally drawn in the marker's colour.
The time is set out as a timer's is.

## Guesses, and what is not here

- No file of ProPresenter's with a marker in it has been seen: that a marker's time is in
  seconds from the start of the file is a reading of its file format.
- A jump of more than a second (the transport moving the media) passes the markers in
  between without running them; a jump back to the first second, as going round is,
  runs those from the start again.
- The time to a marker that has been passed counts up from it.
- The colours offered, a new marker's at random, and blue for a marker that has none in
  its file, are this app's.
- ProPresenter's help moves a marker with Move to Playhead; dragging its flag, in the
  Inspector and on the transport, is this app's as well, and so is the flags' shape.
- Not here yet: ProPresenter's Cut, Copy and Paste of markers; dragging an action, a
  show control or a file onto a marker; dropping an action under the list for a marker
  of its own; and the link that colours a shape by a marker.
