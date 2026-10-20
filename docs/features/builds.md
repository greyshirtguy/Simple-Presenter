[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Builds

A **build** is ProPresenter's way of bringing the things on a slide on one after
another: a transition for one element of the slide, as it comes in (its **Build In**) or
as it goes (its **Build Out**), with the slide already on the screens. A lower third's
name wiped in with the slide, its two lines a moment later, and all of it wiped away
again five seconds on, is three elements' builds. They are read from the
presentation's file and written to it as ProPresenter keeps them, and the transitions
are those of [Transitions](transitions.md), drawn over the element's own box.

**When each runs.** The slide comes in first, with its transition, and brings with it
every element that has no Build In (one with only a Build Out is there from the start).
Then the builds go in their order, each as its **Start** says:

- **With Slide**: as the slide's transition ends;
- **After Previous**: when the build before it ends;
- **On Click**: when **Next** is pressed (or the slide on show is clicked), and the
  builds after it are timed from then;
- **With Previous**: with the build before it, as that one was set going: a name wiped
  out After Previous with a delay of five seconds, and its underline With Previous
  with 5.2, go 0.2 seconds apart. After a build that is itself With Previous, from
  when that one began.

Each is set going so, and then waits its **Delay**. So a slide with On Click builds takes a Next for each
of them before Next goes on to the next slide, as in ProPresenter; the log says "build 1
of 2 of slide 3" for each. A slide shown again starts its builds again. When the next
slide is shown, builds that have not run are not run: the next slide's transition takes
everything away. Every screen, and the operator's preview, runs the builds by the same
clock, so they keep together. Thumbnails, the editor and Quick Edit show the slide
whole.

**The Build tab**, the third of the editor's inspector, beside Shape and Text, has them
as ProPresenter's has them:

- With an element picked, its **Build In** and its **Build Out**: each its transition,
  with **Change** for another and **✕** to take the build off; what can be adjusted
  about the transition (a direction, a colour, a size, as in
  [Transitions](transitions.md)), its **Duration**, and **Reset to Default Parameters**,
  which puts the transition back as ProPresenter has it to begin with. An element
  without one has **Add Build** there instead, which gives it a dissolve, On Click.
- With nothing picked, the slide's own **Build In**: the transition the slide comes in
  with, and every element of it without a Build In of its own. ProPresenter has no
  Build Out for a whole slide. A slide with none says what it comes in with instead.
- Under either, the **Build Order**: every build of the slide, ins and outs, in the
  order they run, numbered, the picked element's lit. A row is dragged up or down to
  put it elsewhere in the order, and a click on one picks its element. Under the list,
  **Start** and **Delay** for the build picked there, or the picked element's first.

Each change is saved at once, and is a step to undo, as any change in the editor.

**A theme's builds.** A theme's slides have builds as a presentation's do, set in the
same Build tab when the theme is edited (a theme's slide has no transition of its own,
so there is no Build In for the whole slide there). They are run on a screen that the
live look dresses in the theme ([Themes](themes.md)): a slide of names, plain in the
room, can have its name wiped in and out again on the stream by a look that gives the
stream such a theme. When the look gives a screen the theme while a slide is on show,
as a macro of the slide's own may, the theme's builds run there from then, as if the
slide had come on in it; on the next slide, with the look already live, they are timed
from the slide as everywhere else. Next goes by the On Click builds of the slide
itself, not by a theme's (see below).

What a build costs: nothing at all for a slide that has none, and for one that has,
nothing while nothing is building. While a build runs, its element is drawn into a
picture of its own for the transition, which is a pass over the element's box for each
frame, as long as the build lasts. The log says how many frames each took.

What is a GUESS, or not done:

- That With Slide starts as the slide's transition ends rather than as it starts; that
  a With Previous build after another With Previous goes from when that one began, its
  delay and all (the only reading under which ProPresenter's own lower third keeps its
  role up); that a build's transition goes over the element's own box (a wipe starts at the element's
  edge, not the screen's) and eases in and out; that a click on the slide on show runs
  its next On Click build rather than showing the slide again; that a build whose
  transition this app does not have is drawn as a dissolve of its length; that a
  look's theme brings its builds, in place of the slide's own for the text boxes it
  dresses, and starts them afresh on a screen it is given to while the slide is on
  show; and that a theme's empty text box takes the words of the slide's box of its
  name.
- Not done: ProPresenter's **Delivery**, which builds a text a part at a time (its
  words, its lines), and the transitions that are for that alone (Blur Reveal and the
  rest); the favourite transitions (the star) and the pictures of each; ProPresenter's
  **Show**, to watch the builds in the editor. A file's builds with any of those are
  kept as they are. Builds on the announcements' slides, and on the stage's pictures of
  the slides, are not run: those show the slide whole. Next steps the On Click builds
  of the slide itself and not those of a look's theme, which are not run (and an
  element whose Build In is one is not seen): whether ProPresenter's Next steps a
  theme's is not known.
