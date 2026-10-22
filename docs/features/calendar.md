[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Calendar

Events at a date and a time that run something by themselves, as ProPresenter's Calendar
does: a playlist's presentation going live for a service, or a macro that switches a
look, a stage layout or a lighting cue, once or every week on chosen days.

The **Calendar** button in the toolbar, right of Bible, opens its window (and puts it
away again); so does **Ctrl+Alt+C**, ProPresenter's key for it. It is a window of its
own, as ProPresenter's is, and stays in front of the operator window.

## The window

On the left, **Calendar Enabled**, and the list of events. Nothing runs while the
calendar is not enabled. At the foot of the list:

- **+** adds an event, called New Event, due once at the next whole hour, with no action.
- The **pin** takes the list to now.
- The **arrows going round** switch between each event listed once, at its first time,
  and each time an event comes due: under **Current**, the last time each came due, and
  under **Upcoming**, the times still to come, for a little over a year. Lit blue, each
  time is listed.

An event whose time has come is red; the one picked is blue. A right click on an event
has **Delete Event…**, and so has Delete, with an event picked.

On the right, the event picked:

- its name, and a **Description**;
- **Schedule**: **starts**, a date and a time of day, typed as the list writes them (a
  time can be typed as `7 pm`, `7:30pm` or `19:30` too); **repeat**, the days of the week it
  comes due on, every week, Sun to Sat, at the time it starts; and **ends on**, for an
  event that repeats: ticked, a year after it starts, and then as typed;
- **Action**: **None**; a playlist from **Playlists**, the whole of it or one of its
  presentations; or a macro from **Macros**. **Test** runs it now.

A playlist that an event goes live from has a calendar beside it in the list of
playlists, as ProPresenter marks one.

## When an event comes due

At its time, an event's action runs, and the log says so:

- a presentation of a playlist goes live on its first slide: the playlist is opened in
  the operator window, at that presentation, as if its first slide had been clicked. For
  the whole playlist, its first presentation;
- a macro runs as it does from the show controls.

An event runs only while the app has its workspace open and the calendar enabled. One
whose time passed while the app was closed, the workspace was not open, the calendar was
off or the computer was asleep is skipped, not run late; the log says when one is missed.

Everything is kept in ProPresenter's own file of the workspace, Configuration/Calendar,
as ProPresenter writes it, with each event's action in both of the forms the file has
for it.

## Guesses, and what is not here

- A repeating event comes due at the same time on the clock through the change to and
  from summer time, and on the chosen days only: the day it starts counts only if it is
  one of them.
- "Ends on" counts, and is for an event that repeats; ticked, it starts a year after the
  event does, as the file looked at had it.
- The Action menu's words, "The Whole Playlist" among them, and where Delete is.
- An event is late by up to a minute and still run (a busy computer); after that it is
  missed.
- ProPresenter's **Capture** is shown and not offered: this app does not record. An event
  that has it is kept, and does nothing here.
- A date ProPresenter leaves out of a repeating event is honoured, but nothing here makes
  one; no file of ProPresenter's with one has been seen.
