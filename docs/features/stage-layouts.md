[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Stage layouts

The third tab lists the workspace's stage [screens](screens.md), each with what it
shows: either the plain view the app has of its own (the words of the live slide over
those of the next) or one of the workspace's **stage layouts**, chosen from the
drop-down in its row. The choices are remembered for each workspace.

<img src="../stage-layouts.png" width="336" alt="The stage tab: the stage screen, with the layout it shows, Band, picked from a list and a button to edit it, and under it the box for a message to the stage">

A stage layout is a slide whose boxes are linked to what is going on, and it is made
and changed in the same [editor](editing.md) as a slide. A text box of one can show:

- the words of the slide that is live, or of the one after it: all of the slide's
  text boxes that show, one after another in the order the slide has them (the back
  one first, which is the reverse of the editor's list), as plain words in the box's
  own font and colour;
- those words with the song's chords over them, in the key picked for it
  ([Chords](chords.md));
- the notes of the slide that is live, or of the one after it: their words in the
  box's own font and colour, or the notes as they were written;
- the time of a [timer](timers.md);
- the time of day, and the date;
- how long the video that is playing has left;
- a [playback marker](playback-markers.md)'s name, or the time to it;
- the copyright of the song that is live ([Copyright display](copyright.md));
- what the stage is being told: the stage message.

[Editing](editing.md) has how each is set, under Linked text.

## Screen Preview

A shape on a stage layout can be filled with a **Screen Preview**: a picture of what one
of the workspace's screens is showing, kept up to date as it changes. Of an audience
screen it is everything the screen shows (the media, with a video moving in it as it
plays, the slide, the announcements, the props, the messages and the mask, in what the
live look gives that screen); of a stage screen, the layout it has. It is chosen in the
editor's Fill menu, with the screen under it, and slides and props can have one too.
ProPresenter's own stage layouts come with some, which are drawn as they are.

The picture is not a copy of the screen's pixels, which one window cannot read from
another without a trip through the computer's memory that would cost more than
drawing it again. It is the screen drawn again, at the picture's size, from what the
screen is told: the slide's words again, a still picture at the picture's size, and a
video's frames borrowed from the one decoder that plays it for the screens, every one
of them. Each borrowed frame is a frame the stage screen draws as well, which is the
whole of the cost. On the laptop this is made on, with a 1080p video playing under the
words and the stage showing a picture of the output most of its size, the app used
about a quarter of one of the processor's cores, where with the stage showing its plain
view it used a tenth: a stage drawing every frame of a video costs about what the
output itself does. Nothing else was held up, and no frame of the output was late. A
picture that took fifteen frames a second instead, tried for comparison, used a sixth.
(The benchmark measures this: see [Built for modest hardware](../how-it-is-made.md#built-for-modest-hardware).)

Guesses at ProPresenter, and choices of this app's own: the screen is fitted into the
shape whole, on black where it shows nothing, and the shape is empty around it; a slide
coming or going is a cut in the picture, without the transition the screen makes; its
slides have shadows only where the shape is a quarter of the screen's height or more;
a picture's own slides have no pictures in them, so that a screen with a picture of
itself shows it once and not without end; a screen the link names that the workspace
does not have shows nothing; and the thumbnails show nothing for it, nor does the
editor, where the shape has the fill ProPresenter keeps beside the link (black, in its
own) and a yellow mark that says which screen. ProPresenter's stage editor also adds a
Screen Preview with its `+`; here a shape is added and its fill made one.

Along the bottom of the tab is the **stage message**: a box to type in what the people
on the stage are to be told, and a button that shows it and hides it. Enter in the box
shows it too. Shown, it is what every text box linked to the stage message shows, on
whichever layout has one; ProPresenter's own layouts have one, which is there only
while a message is. Such a box can be set to **flash** when a message is shown: its
fill is the colour chosen for that, on and off four times a second, for two seconds.
Words typed while a message is shown wait to be shown, and the button says Show again
for them. What is in the box is kept from one start of the app to the next; that a
message is being shown is not. (Guesses at ProPresenter: that the box is one line and
Enter shows it; the beat of the flash; and that a box which ProPresenter's file marks
hidden, with a rule that it shows when it has text, is shown while a message is.)

The `+` makes a layout to start from, with a box for each of the two slides, gives it
to the first stage screen and opens it in the editor. The editor's own list of layouts has a `+`
too, and a right click there renames, copies or removes one.

ProPresenter's own layouts, in `Configuration/Stage`, are read as they are and can be
given to the stage. What they have that is shown here: boxes for the words of the live
and the next slide, including those that take only the text of the slide's elements of
a given name (and nothing at all of a slide that has no element of that name, which is
how ProPresenter has it and is made use of), and with a song's chords over them where a
box has those switched on; a slide's notes; timers; the clock; the time left of a
video; the stage message; and boxes that show only while a timer is running, or has
run out; and Screen Previews (above). What they have that is not shown yet (pictures of
the slide that is live or of the next) is left empty on the stage, and in the editor is
marked with what it is; the links themselves are kept, so the layouts still work in
ProPresenter.
