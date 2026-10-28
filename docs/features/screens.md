[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Screens

A **screen** is somewhere the show is drawn for. An **audience** screen gets the slides,
the media and the props; a **stage** screen gets a stage layout. A workspace can have
sixteen, of both kinds together, and the pair of buttons at the right of the toolbar
(or Ctrl+1 and Ctrl+2) switch all the screens of a kind on and off together.

The screens are set up under **Settings → Screens**: a line for each, with its name and
what it is sent out through, and **+ Add** for another of either kind. Where a screen
goes is chosen from a menu, as in ProPresenter, and is one of five things:

| It is sent to | What that is |
| --- | --- |
| **A window of its own** | A small window that floats over the operator window, and can be made to fill whatever display it is on (see below). What the one output and the one stage display always were, and what the first screen of each kind is until it is set to something else |
| **A display** | One of the computer's displays, by its name (`HDMI-A-1`, say), filled. While that display is not plugged in the screen is not shown, and the settings say so; plugged in again, it is back |
| **NDI** | The local network, as an NDI source: see [NDI](ndi.md) |
| **A Blackmagic output** | An output of a DeckLink card, as Blackmagic's Desktop Video Setup names it, in a format chosen from its own menu, and keyed or not: see [Blackmagic DeckLink](decklink.md) |
| **Nothing** | The screen is there for things to name (a stage action, say), and is not drawn. What a screen ProPresenter made is until it is set to something here |

Two things are known of a screen, and they are kept in two places.

- **What it is** (its name, its kind, and the id other things know it by) is the
  workspace's, and is in ProPresenter's own file of how the workspace is set up
  (`Configuration/Workspace`). So the screens ProPresenter has for a workspace are the
  screens here, a stage action made there finds the screen it names, and a screen added
  here is in the file for ProPresenter to find, written the way it writes one that is
  connected to nothing. (That last has not been tried in ProPresenter itself.)
- **What it is sent out through** is this computer's, and is in the app's own settings.
  The same workspace on another computer has the same screens and other things plugged
  in. What ProPresenter says each screen is connected to (a display, a video card) is
  left in its file exactly as it was found, and not gone by here.

Every audience screen shows the same thing unless the [look](looks.md) that is live says
otherwise (a look gives each screen its layers, and can dress its slides in a theme),
and every stage screen shows what is live through its own layout (each has its own,
chosen on the Stage tab of the show controls; a stage action gives any of them one).
Each screen is drawn by itself, at its own size. A video is still played only once
however many screens show it, by the first audience screen, and the others are handed
its frames, so they cannot drift apart and its sound is heard once.

**The preview.** The operator window has one preview, at its top right, of one screen
at a time: the first audience screen to begin with, or whichever is picked from the
name under it, which lists the audience screens and then the stage screens, as
ProPresenter's picker under its preview does. What is picked is kept for the workspace;
a screen that is no longer there gives way to the first audience screen again. A stage
screen is previewed with its layout, Screen Previews and all, so a stage screen that is
sent to **Nothing**, with a layout of shapes filled with Screen Previews of other
screens, is a way to look at several screens at once. (Until 0.103 there were two
previews, of the first audience screen and of the first stage screen.)

The line under the preview and its picker can be dragged up or down, to make the
preview's area taller or shorter, as the panes are made wider by their edges: a tall
layout of two or three Screen Previews one over another then has the room it wants.
Dragged, the preview fills its area, as wide as the clears leave it, and the screen is
fitted in it; the clears stretch to its height, and it stops where the show controls
still have room for two timers. A double click on the line gives the preview back its
16:9. How tall it was left is kept with the app's settings.

## Combined screens

An audience screen can be two outputs side by side drawn as one picture, as
ProPresenter's **Combined** arrangement is: two 1920x1080 outputs make one screen of
3840x1080, the left half going out through one and the right half through the other.
Under **Settings → Screens**, a screen's **Arrangement** is **Single** or **Combined:
two outputs side by side**, and a combined screen has a line under its own for its
second output. Each output goes wherever a screen can go, and each has its own settings
there (a card output and its format, NDI's name and size, a video delay); the second
goes nowhere until it is given somewhere. Back to Single, the screen is its first output
again and the second's settings go.

- **Out of a card.** Every screen going out through a card is drawn in the same passes,
  on one clock, and the card is handed them together (see
  [Blackmagic DeckLink](decklink.md)): so are the two halves of a combined screen sent to
  two outputs of a card, such as a DeckLink Duo 2's. Whether the card then sends them
  out on the same frame is the card's to say: the app hands them over together.
- **On displays, in windows, over NDI.** Each output is drawn by itself, as two screens
  are, from the same things: two displays refresh each in its own time, so while
  something moves across the middle the halves can be a frame apart.
- What is drawn is one picture of the whole: the media is fitted to the whole screen, a
  slide is drawn as on any screen of another shape than its own (a 1920x1080 slide
  takes the left half of a 3840x1080 screen; a presentation made at 3840x1080 fills it),
  and the operator's preview and Screen Previews show the whole screen in its own
  proportions. A video is played once, as for any screens, and each output is handed
  its frames.
- The arrangement is the workspace's, and is in ProPresenter's file, as the screen is:
  so many columns and rows of outputs, and a record of each, its size and its place in
  the whole. An output added is written as ProPresenter 7 wrote the second of two it had
  added side by side (`Placeholder - 2`, connected to nothing), compared with a file it
  wrote. A combined screen ProPresenter made, of more than two outputs, is drawn as it
  is, each output its share. What each output is sent out through is this computer's.

What is a GUESS: the names of the choices in the settings; how ProPresenter writes
outputs in a second row (only one row has been seen); and where it puts a slide of
another shape than the screen, as here at the top left. Edge-blended screens, and a
stage screen of several outputs, are not drawn as such: as their first output.

## Video delay

**Video Delay**, under each audience screen in Settings → Screens, shows all of that
screen so many of its own frames late, from none to fifteen, as ProPresenter's video
frame delay does for an output. It is for when the picture comes before the sound: the
app's sound is heard a little after it is played (see
[The app's channels](audio.md#the-apps-channels)), and a vision mixer, a recorder or a
stream can hold back the sound or the picture further. The settings say what the delay
comes to in milliseconds, by the screen's own frames: a refresh of the display it fills
(of the one its window is on), or a frame of the rate it goes out over NDI or of its
card's format. The operator's preview shows the screen as it is now, not delayed, and so
does a Screen Preview of it on a stage layout or a slide, which is drawn again from what
the screen is told (what ProPresenter's do is not known here: a guess, made because a
stage is level with the sound in the room). The delay is this computer's,
and is kept with what the screen is sent out through. ProPresenter keeps one with each of
its own outputs, in the workspace's file, and that is left as it was.

How it is done: the screen is drawn as it always is, but out of sight, and the graphics
card keeps a copy each time it changes; at each of the screen's frames, the copy from so
many frames before is what is shown. A screen with no delay is drawn as ever, at no cost.
One with a delay costs a copy of the screen each time it changes, a copy onto the screen
at each frame it draws, one frame more for each change, when its copy comes to be shown,
and on the graphics card a picture of the screen for each frame of the delay and three
more at most (8 MB a picture at 1920 × 1080: some 150 MB for fifteen frames while a
video plays). It needs OpenGL, which is what the app draws with; where it draws
otherwise (the self-test's drawing in software), the setting says so and is shut.

## Performance statistics

**Show Performance Statistics On Screen**, in Settings → Screens (ProPresenter's setting,
by its name), shows at the top left of every audience screen how it is doing: its name
and size; how many times it was drawn in the last second, and the longest time between
two of those; how often its own frames come (its display's refresh, or the rate it goes
out at); what it is drawn by (its display's refreshes, the app's clock, or a card's own
frames: see [Blackmagic DeckLink](decklink.md)); its video delay; and how late its sound
is heard, as far as the app can see.

The app draws a screen only when something on it changes. So a screen standing still is
drawn once a second (for the statistics' own numbers), and one playing a video or a
transition at each of its frames: one that should be drawn at every frame, and is drawn
at fewer, is one the computer is not keeping up with.

The sound line is the app's own share of the sound's delay: what its mixer keeps in hand,
and what Qt holds for the device. The system's sound service and the sound card hold more
after that, which Qt does not ask them about, so the whole delay is longer by an amount
the app does not know (with PipeWire, `pw-top` shows its own share as the quantum). What
the eye and the ear make of it, a clap filmed with the screen in shot, is the measure to
set a video delay by.

The statistics are drawn over the screen, and not delayed with it; the operator's
preview leaves them out. They are this computer's, as ProPresenter keeps the setting in
its preferences. What ProPresenter's show is not known here beyond its name and its frame
rates, and these lines are the app's own. Stage screens do not show them, for now.

## A screen in a window

The small window with a slim title bar that floats over the operator window, or
fills a screen. The small window is moved by dragging any part of it, and resized by
its edges; double-click its title bar to have it fill the screen it is on, and move
the mouse over it there for the control that brings it back. The output starts
out filling a second screen if there is one (`--screen`, with a screen's number or
name, says which; `--list-screens` says what there are), and each window comes back
the way it was left.

A Wayland desktop does not let an application place its own windows, so these windows
come back wherever the desktop puts them. The settings screen has a switch, under
Windows, to run the app through X11 instead, where their places are remembered; it says
what that costs.

**Alt+Tab.** The output window and the stage window are there to be looked at, not
switched to, so they are kept out of the desktop's window switcher, its overview and its
dock, and switching to Simple Presenter always lands on the window that works the show.
How that is done depends on what the app is run through:

- **Through X11** the app sees to it itself, and there is nothing to do.
- **Through Wayland**, which is the default, an application cannot: the desktop alone
  decides what is in its lists. GNOME leaves the two windows out when a small extension
  of its own, which comes with the app, is switched on. The package puts the extension
  where GNOME looks for it; log out and in again once so that GNOME finds it, and then
  switch it on:

  ```
  gnome-extensions enable simple-presenter-windows@greyshirtguy.github.io
  ```

  From a copy built from source, put it in place first:

  ```
  mkdir -p ~/.local/share/gnome-shell/extensions
  cp -r packaging/gnome-shell-extension/simple-presenter-windows@greyshirtguy.github.io \
      ~/.local/share/gnome-shell/extensions/
  ```

  The extension is written for GNOME 50, the one Ubuntu 26.04 has, and does only this:
  it finds the two windows by their application and their titles and tells GNOME to
  leave them out. On other desktops run through Wayland, and on GNOME without it, the
  two windows are in Alt+Tab as any window is. (So, through Wayland, is the window of
  any screen after the first of its kind: the extension knows only the first two.)

## Kept awake

**The screens are kept awake.** A desktop left alone blanks its screens after a few
minutes, and a presenter's computer is left alone for as long as a sermon lasts. So
while the audience screens or the stage screens are switched on, the app asks the
desktop not to count the time as idle, the way a video player does while a film plays;
with both switched off, or the app closed, the desktop goes back to its own settings.
The [log](log.md) says when the asking starts and ends, and says so once if the desktop
does not answer to it (GNOME and KDE do).
