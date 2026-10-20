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
