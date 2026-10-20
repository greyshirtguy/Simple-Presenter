[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Reflow

Getting a song's words spread well over its slides (two lines a slide, a verse to a
slide) is most of the work of setting it up. The **reflow editor** is for that alone.
It is the editor in another of its ways of working: the **Reflow** button in the
toolbar, straight after Edit (a T over a wave), or **Ctrl+R**, brings it up on the
presentation being viewed, from the show or from the editor proper; the button again
goes back to the show, and Edit leads to the editor proper on the same presentation.

At the left the slides' words are a table, a row to a slide, in the order the
presentation has its slides: each row has its slide's number, its group's colour down
its edge and the group's name where the group begins, and the words, which are typed in
as a page is. At the right are the slides themselves, as they will be shown, the one
whose row the caret is in ringed; the slider under them sets their size.

- **Ctrl+Enter**, or **Insert Slide Break** over the table, splits the slide where the
  caret is. What is after the caret goes to a new slide straight after, in the same
  group. The new slide is the slide over again, the same text boxes and everything
  else in the same places and set the same, with those words in it. The spaces or the
  line break the caret was among go to neither slide.
- **Backspace** at the very start of a slide's words joins the slide to the one before
  it: its words go after that one's, on a new line. **Delete** at the very end of a
  slide's words joins the next slide to it the same way. The slide that was joined is
  taken out of the presentation, with whatever else it had.
- **Enter** is a line break within the slide, and words are typed and corrected as
  anywhere. What is typed is saved a moment after the typing stops, and the slides at
  the right follow.
- **Up** and **Down** go from the first and last line of a row to the row before and
  after, and Tab to the next row. A click on a slide at the right puts the caret in
  its row.
- **Undo** (Ctrl+Z) takes back a split, a join or what was typed, a step at a time, and
  Redo does it again.

The words keep their fonts, sizes and colours through all of it, and a song's chords
stay over their words, though none of that is shown or set here: the table is in one
plain face that is easy to read. ProPresenter's reflow editor shows the words in their
own fonts, and can set them; here that is left to the editor proper, and this is for
reflowing.

What a row is: the slide's first text box that has words of its own, or, on a slide
whose boxes are all empty, the first of those. A slide with several text boxes that
have words is a row for the first, and the others go with the slide as they are. A
slide with no text box of its own (a picture, or text that is another element's or a
timer's) is a row that says so and takes no typing.

It is for presentations only: the props, the stage layouts and the themes are not
reflowed.

What was a guess when it was made, ProPresenter's own having been described and not
looked at: what becomes of the spaces at a split; that joined words begin a new line;
that the new slide of a split does not take the first slide's label, its notes, or
whatever else its cue did when it was shown (media it started, a timer), which would
otherwise be done twice; which text box a slide with several is a row for; and the keys
that go from row to row. The maintainer, who knows ProPresenter, has since said that
each is as wanted, and that the third is as ProPresenter does, as far as they know.
