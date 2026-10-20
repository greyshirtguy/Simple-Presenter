[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Go To Next timers and the Slide Show

Two things of ProPresenter's make a show go on by itself.

A slide's **Go To Next Timer** is in its menu (a right click on a slide, **Go To Next
Timer…**), which puts up a small panel: the number of seconds, and a tick box, **Loop
to Beginning**. A slide that has one triggers the next slide when its seconds are up,
or with the box ticked the first slide of the presentation. Its thumbnail then has a
small badge with the seconds, an arrowhead against a bar for the next and an arrow
going round for the first, and a right click on that badge opens the panel too, where
**Remove Timer** takes it off again.

A presentation's **Slide Show** gives every one of its slides the same time, without a
timer on any of them: each goes on to the next, and the last to the first, round and
round. It is the small button with the arrow going round over the slides, at the left
of the arrangement's name, which opens ProPresenter's menu of them: **None**, 5, 10, 15
or 20 seconds, and **Custom…** for any other time. With a Slide Show the button is lit
and says the seconds.

- **A slide's own timer wins.** In a presentation with a Slide Show, a slide that has a
  Go To Next Timer goes by its own: a longer or a shorter stay, or, on the last slide
  with no Loop to Beginning, a place to stop.
- **A slide has to be triggered.** Neither starts anything by being set. The time
  starts when a slide goes live, by a click, a key, an action or the timer before it.
- **Clearing stops it.** Clear the slide (or the announcements, or everything) and the
  show stays where it is. So does triggering another slide by hand, which starts that
  slide's time instead.
- **The show and the announcements each go on by their own time**, so a set of notices
  can go round on the [announcements](announcements.md) layer while the songs are
  worked by hand under it.
- **No seconds at all** is allowed for a slide's timer: it goes on at once, which is
  how a slide is made that only leads back to the first. A presentation all of whose
  slides did that would never stop, so one that has gone the whole way round without
  any time passing is stopped, and the log says so.

Both are kept in the presentation's file as ProPresenter keeps them, the timer as what
the slide's cue does when it is done and the Slide Show as the presentation's time for
it, so they are there in ProPresenter too, and ones made there work here.

What is a guess, or not done:

- That a timer of no seconds goes on at once in ProPresenter too is read from its
  files, not seen.
- A timer taken off leaves the cue as ProPresenter's files have a cue that has had
  one, with its time still written in it and nothing to act on it; one that was put on
  in the same run of the app and taken off again leaves the file exactly as it was.
- The words of the Slide Show's menu, and the five seconds a new timer starts at, are
  this app's.
- ProPresenter's files can also send a slide on to a random slide or a named one, and
  when a piece of media ends. Those are kept in the file and not acted on.
