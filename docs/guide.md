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

- **Simple View.** Hold the ~ key for most of a second, or click Simple View in the
  toolbar, and everything round the slides gets out of the way, so that as many of them
  as will fit can be seen at once. The same again brings it all back.
- **Two chord editors.** In the editor, **Chords** is a sheet of the whole song, the
  chords put on with the mouse and the keys 1 to 7; **ChordPro** is the same song as
  ChordPro text. ProPresenter shows the chords a song came with, and has no way to put
  one on by hand.
- **Importing a ChordPro file** as a new song, chords and all: Import ChordPro File… in
  the menu of the + beside Libraries.
- **A Bible passage shared out evenly.** In Bible mode the words can be shared out across
  the slides, broken at a full stop or a comma where there is one near, with no slide
  left holding a scrap.
- **HTTP Requests**, a kind of device (Settings → Devices): a slide or a macro sends a
  GET, POST, PUT or DELETE, with a body and headers, to a web address.
- **How a slide's video plays on, at a glance.** A slide with a video has an icon for
  whether it stops at its end, loops, or loops for a count or a time. A right click on
  the icon changes it.
- **Playing a slide's media by itself.** The Media caption in a slide's menu plays the
  slide's media and leaves the slide layer as it is.
- **Dragging the clear buttons.** A clear button dropped on a slide or a macro gives it
  the action that clears that layer, or that clear group.
- **How solid the icons on the slides are** is a setting (Settings → Slides), from
  nearly gone to solid.
- **A slide disabled at a date.** Disable On… in a slide's menu sets a day and a time for
  the slide to be disabled by itself. Until then it is marked with the day.

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
| Right, Space / Left | Next / previous slide |
| Down / Up | Next / previous presentation |
| F1 | Clear the first clear group, which to begin with is Clear All |
| F2 / F3 / F4 / F5 / F6 | Clear the slide layer / media layer / props / audio / messages |
| ~, held for most of a second | Simple View on or off |
| A letter or a digit | The hotkey of a group: its first slide |
| A number, then Enter | The slide with that number |
| Ctrl+E / Ctrl+S | The editor / back to the show |
| Ctrl+R | The reflow editor |
| Ctrl+F | Search |
| Ctrl+click / Shift+click on a slide | Select it, or let it go / select every slide from the last one selected, for the Text button |
| Esc | Put the Text button's popover away; then let go of the slides selected |
| Ctrl+Alt+B | Bible mode on or off |
| Ctrl+Alt+C | The Calendar |
| Ctrl+V | Show or hide the media bin |
| Ctrl+1 / Ctrl+2 | The audience screens / the stage screens on or off |
| Ctrl+, | The settings |

## Where things are kept

- The workspaces, and a log of each run, in `~/Documents/SimplePresenter/`. The log says
  what was done and what went wrong, written to be read, or handed to an AI model.
- The app's own settings in `~/.config/SimplePresenter/`.
- A copy of each presentation as it was before it was first edited, and Bibles that were
  imported, in `~/.local/share/SimplePresenter/SimplePresenter/`.
- The disable dates, in the workspace's folder, in `SimplePresenter/Disable Dates.json`.
