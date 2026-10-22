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
