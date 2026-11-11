[Simple Presenter](../README.md)

# Simple Presenter, in short

*This page is also what the ? button at the right of the toolbar shows, inside the app.*

Simple Presenter runs a show from a ProPresenter 7 folder as it is, reading and writing
ProPresenter's own files. It is shared as an example of what vibe coding can do in the
hands of a hobby programmer: it is not a product for use, and nobody supports it. It
changes the files it opens, so give it a copy of your ProPresenter folder, never your
only one.

Where ProPresenter has a feature, this app does it as ProPresenter does, as far as that
is known. Where that is a guess, the page for that part says so. Below are the features
that are its own, what it does not do, its keys, and where it keeps things. Each part has
a page of its own, linked from the
[README](https://github.com/greyshirtguy/Simple-Presenter#readme).

## Its own features

These are not ProPresenter's.

- **Simple View.** Hold the ~ key for about half a second, or click Simple View in the
  toolbar, and everything round the slides gets out of the way, so that as many of them
  as will fit can be seen at once. The same again brings it all back. How long the key
  is held is a setting (Settings → Simple View), down to a plain press.
- **Two chord editors.** In the editor, **Chords** is a sheet of the whole song, the
  chords put on with the mouse and the keys 1 to 7; **ChordPro** is the same song as
  ChordPro text. ProPresenter shows the chords a song came with, and has no way to put
  one on by hand.
- **Importing a ChordPro file** as a new song, chords and all: Import ChordPro File… in
  the menu of the + beside Libraries.
- **Importing a song's words from SongSelect**: Import Lyrics File… and Import Lyrics
  from Clipboard in the same menu take the lyrics file SongSelect gives out, or its
  words copied, and ask the lines on a slide, a theme, the library and a playlist; the
  song's details go where the copyright display looks.
- **A Bible passage shared out evenly.** In Bible mode the words can be shared out across
  the slides, broken at a full stop or a comma where there is one near, with no slide
  left holding a scrap.
- **HTTP Requests**, a kind of device (Settings → Devices): a slide or a macro sends a
  GET, POST, PUT or DELETE, with a body and headers, to a web address.
- **How a slide's video plays on, at a glance.** A slide with a video has an icon for
  whether it stops at its end, loops, or loops for a count or a time. A right click on
  the icon changes it.
- **A background video's sound.** A background is silent here. A slide with a
  background video has a speaker icon, struck through while it is muted; a click on it
  has the background heard, and another mutes it again, at once if it is playing.
- **Playing a slide's media by itself.** The Media caption in a slide's menu plays the
  slide's media and leaves the slide layer as it is.
- **Dragging the clear buttons.** A clear button dropped on a slide or a macro gives it
  the action that clears that layer, or that clear group.
- **How solid the icons on the slides are** is a setting (Settings → Slides), from
  nearly gone to solid.
- **When the slides follow the live one** is a setting there too: **Early**, where a
  slide that goes live in the bottom row in view brings the next row into view, or
  **Just in Time**, where the slides move only once the live one is out of view. Either
  way they move smoothly, starting as the slide changes, and easing in and out of the
  move unless that is switched off: a step to the next slide or the one before takes as
  long as is set there, and a jump to a slide out of view (by a hotkey, a typed number or
  the network) a tenth of a second. Next held down, they keep up with the slides as they
  go. And coming back to the presentation that is live, after looking at another, puts
  its live slide in view at once, as the setting would have brought it there.
- **The meters beside the preview**: how many of the app's channels have one, from two
  to sixteen, and how solid they are, are settings (Settings → Audio).
- **A slide disabled at a date.** Disable On… in a slide's menu sets a day and a time for
  the slide to be disabled by itself. Until then it is marked with the day.
- **Apply This Text Style to All.** In a slide's menu, under Copy Text Style: the slide's
  text style put on every other slide of its presentation.
- **Export to PowerPoint.** A right click on a presentation: a PowerPoint file of it,
  slide for slide as it is shown, its words as text that can be edited, its media behind
  them, its notes as speaker notes. What could not be carried over is listed at the end.
- **Import PowerPoint.** The + beside Libraries, or a `.pptx` dropped on a library or a
  playlist: a PowerPoint file brought in as a presentation, its words as text boxes that
  can be edited, its pictures as pictures, its
  slide backgrounds as the slides' background media (or left out, by the panel's switch),
  its notes as slide notes and its sections as groups. What could not be carried over
  (tables, charts, SmartArt, animations) is listed at the end.
- **A look and a layer changing together.** A slide or a macro that goes over to a look
  and changes a layer in the same moment: a layer the look gives back to a screen comes
  back with what is being put on it, which comes in from nothing, and the slide before
  is not seen there; a layer the look fades from a screen goes with what it had.

## What it does not do

ProPresenter does a great deal that this does not. Among it: timelines; capture and
recording; video in from a DeckLink card or over NDI; timecode; reordering slides in
the editor; lists in text, and a text built a part at a time; importing PowerPoint; and
much else. The
[list of what is not there yet](https://github.com/greyshirtguy/Simple-Presenter/blob/main/docs/todo.md)
is longer, and not even it is the whole of it.

## Keys

| Key | What it does |
| --- | --- |
| Right, Space / Left | Next / previous slide; after the slide is cleared, the one after / before the slide cleared |
| Down / Up | Next / previous presentation |
| F1 | Clear the first clear group, which to begin with is Clear All |
| F2 / F3 / F4 / F5 / F6 | Clear the slide layer / media layer / props / audio / messages |
| ~, held for about half a second (a setting) | Simple View on or off |
| A letter or a digit | The hotkey of a group: its first slide |
| A number, then Enter | The slide with that number |
| Ctrl+E / Ctrl+S | The editor / back to the show |
| Ctrl+R | The reflow editor |
| Ctrl+F | Search |
| Ctrl+click / Shift+click on a slide | Select it, or let it go / select every slide from the last one selected, for the Text button |
| Ctrl+click / Shift+click on a row of a playlist | Select it as well, or let it go / select every row from the last one selected; a heading can be selected too. What is open stays open |
| Delete | Take the selected rows out of the playlist |
| Ctrl+C / Ctrl+V | Copy the selected rows of a playlist / paste them under the row selected, while the rows are what was last clicked; a click on a slide, or Esc, and Ctrl+V is the media bin again |
| Esc | Put the Text button's popover away; then let go of the slides selected; then leave the playlist's rows |
| Ctrl+Alt+B | Bible mode on or off |
| Ctrl+Alt+C | The Calendar |
| Ctrl+V | Show or hide the media bin (unless rows are being pasted, above) |
| Ctrl+1 / Ctrl+2 | The audience screens / the stage screens on or off |
| Ctrl+= / Ctrl+- | Make the thumbnails larger / smaller: the media bin's where the pointer is over the bin, the slides' otherwise |
| Shift, held while a transition's duration slider is dragged | The slider moves a tenth as far as the pointer, for a value between its marks |
| Ctrl+, | The settings |

## Where things are kept

- The workspaces, and a log of each run, in `~/Documents/SimplePresenter/`. The log says
  what was done and what went wrong, written to be read, or handed to an AI model.
- The app's own settings in `~/.config/SimplePresenter/`.
- A copy of each presentation as it was before it was first edited, and Bibles that were
  imported, in `~/.local/share/SimplePresenter/SimplePresenter/`.
- The disable dates, in the workspace's folder, in `SimplePresenter/Disable Dates.json`;
  and the slides whose background video is heard, in `SimplePresenter/Background Sound.json`.
