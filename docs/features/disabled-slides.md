[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Disabled slides

A slide can be switched off without being deleted, as in ProPresenter: **Disable Slide**
in the slide's right-click menu, and **Enable Slide** to switch it on again. It is kept in
the presentation's file as ProPresenter keeps it (the cue's `isEnabled`), so a slide
disabled in ProPresenter is disabled here and the other way round.

A disabled slide stays in its place, faded, in the slides and in the editor's list. It is
passed over by everything that goes on from slide to slide:

- **Next** and **Previous**, by the arrow keys, a clicker, or anything else that steps;
- a **Go To Next** timer, the **Slide Show**, and the announcements going round;
- the next slide the operator is shown, and the stage's next slide.

It is shown only when it is asked for by itself: a click on it, or its number typed. Next
then goes on from it to the next slide that is not disabled.

(Before this, a slide ProPresenter had disabled was not listed here at all.)

## At a date

This app's own, not ProPresenter's: a slide can be set to be disabled by itself at a day
and a time. **Disable On…** in its right-click menu puts up a panel with the start of
tomorrow to begin from; the day and the time are typed as in the [Calendar](calendar.md)
(the day as the computer writes one, or 2026-10-30; the time as 9:00 am or 21:00), and
what they make is said under them. **Save** keeps it, **Remove Date** takes a date off,
**Cancel** or Esc changes nothing. Until then the slide has a calendar's page and the day
on it, and its menu's row says the date.

When the date comes the slide is disabled as **Disable Slide** would disable it, in its
file, and the date is done with. A date that comes while the app is not running, or while
another workspace is open, is carried out when the workspace is next opened, and one that
is typed as already past disables the slide at once.

Where the dates are kept: in the workspace's folder, in `SimplePresenter/Disable
Dates.json`, by the presentation's id and the slide's, so that they go where the workspace
goes. Not in the presentation's file: ProPresenter, saving a presentation, leaves out
anything in it that it does not know, as a test of it in ProPresenter 7 on the Mac showed,
while it keeps the ids. (A guess: that ProPresenter leaves a folder in its workspace that it
does not know alone.) A presentation moved or renamed since its slide was given a date is
not found where it was, and the date is kept and said in the log; it is tried again when
the workspace is next opened.
