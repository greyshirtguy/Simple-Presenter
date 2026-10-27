[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Editing

The window is in one of two modes, and the two buttons at the left of the toolbar say
which: **Show** (the triangle) and **Edit** (the pencil). **Edit**, or Ctrl+E, or Edit
in the right-click menu of a presentation or of a slide, swaps the slides for the
editor, at that slide if it was a slide's menu. **Show**, or Ctrl+S, or Edit again,
goes back to showing, from whichever editor is up. The output carries on as it was
while a presentation is edited, and shows a slide as edited the next time that slide
is shown.

![The editor: the presentation's slides down the left, with the elements of the one being worked on listed under them; that slide in the middle, a text box on it picked and showing its handles; and on the right the panel of what can be set for the box, which is its name, position, size and opacity, its fill, stroke and shadow, and the rules for when it shows](../editor.png)

On the left are the presentation's slides and, under them, the elements of the slide
being worked on, the one in front first. In the middle is the slide. On the right are
the properties of the element that is picked, in parts, as ProPresenter has them:
**Shape** and **Text**, and for a presentation's slides **Build**, which has the
element's builds, or with nothing picked the slide's (see [Builds](builds.md)).

The same editor works on the workspace's [props](props.md), [stage layouts](stage-layouts.md),
[themes](themes.md) and [masks](masks.md), which are slides too: the list on the left is
then of those, and has a `+` to add one and a right-click menu to rename, copy or
remove one.

A click on the name under one of them in that list renames it where it stands (Enter
keeps the name typed, Esc gives it up). Under a slide of a presentation the same click
is for the slide's **label**, which is what stands there: typed, it is the slide's
label, in the colour the label had, and typing nothing takes the words away. **Rename**
in a slide's right-click menu does the same, and it is undone like any other change.

With a presentation in the editor, three buttons at the right of the editor's toolbar
choose between three ways of working. **Slides** is everything described here.
**Chords** and **ChordPro** are for a song's chords, and have a section of their own:
[Chords](chords.md).

- **Picking and arranging.** Click an element on the slide or in the list. Drag it to
  move it, or drag a handle to resize it; both snap to the slide's edges and middle and
  to the other elements, and show the line they have snapped to. The eye and the
  padlock in the list hide and lock an element, a double click there renames it, and a
  right click, there or on the slide, offers the rest: duplicate, delete, bring forward
  and send back.
- **Which is in front.** Where elements overlap, the one higher in the list is the one
  seen: the list is the pile, its top the front. A row of it is dragged up or down to
  another place, and a line between two rows shows where it would land; let go there,
  the element is at that place in the pile, on the slide and in the file. Held near the
  top or the bottom of a list too long to see whole, it scrolls the list. Let go where
  it was, or off to one side of the list, nothing is moved. The right-click menu's
  **Bring to Front**, **Bring Forward**, **Send Backward** and **Send to Back** do the
  same without dragging. (How it looks while a row is held is this app's own, after its
  other lists.)
- **Text.** Double-click an element to edit its text where it stands, drawn as it will
  be shown. Font, size, colour, bold, italic, underline, capitals, spacing and outline
  apply to the text that is selected, or to all of the text when none is. A paste is a
  paste and match style, whether by Ctrl+V or Ctrl+Shift+V: only the words of what was
  copied come in, in the format of the text where they go, each new line a paragraph
  like the one the caret was in. (There is no paste that keeps the copied text's own
  formatting.)
- **Adding.** The first three buttons over the slide add a text box (the T), a shape
  from a short list (a rectangle, a rounded rectangle, an ellipse or an arrow, each
  filled with a plain colour to begin with), and a picture or video from a file. The
  buttons are glyphs; what the one under the pointer does is said in words along the
  bottom of the editor. Underneath, the three are one kind of thing, as they are in
  ProPresenter's files: any of them can be given words by double-clicking it, and any
  of them any fill.
- **Turning.** An element is turned about its middle by the **Rotation** field, in
  degrees clockwise, or by dragging one of its corner handles with Ctrl held, as
  ProPresenter has it with the Command key. Dragged, it settles on upright and on the
  quarter turns when it is near one, and with Shift goes by fifteen degrees at a time.
  The frame and the handles turn with the element, and a turned element is resized
  along its own sides. Over a corner handle the pointer says which a drag would do: the
  arrows for resizing, or with Ctrl down a curved arrow for turning. (Ctrl held from the
  start of a drag is what turns; to resize by a corner without snapping, press Ctrl
  once the drag is under way.)
- **Shape.** Position and size, opacity, fill, stroke and shadow. A fill is a colour
  (optionally only behind the lines of the text), a gradient from one colour to another
  along an angle, a picture, which is made to fit inside the element, to fill it and
  be cut off at its edges, or stretched to it (and turned, flipped, cropped and given
  effects in its [Media Inspector](media.md#the-media-inspector), behind **Inspector…**), or a
  [Screen Preview](stage-layouts.md#screen-preview) of
  one of the workspace's screens, chosen under it, or one of the workspace's
  [video inputs](video-inputs.md#as-the-fill-of-a-shape), live, chosen and scaled under it. A rounded rectangle has one handle more
  than the others, a round one on its top edge, which is dragged to make its corners
  more round or less. A rectangle, a rounded rectangle and an ellipse can have their
  edges feathered, which fades them out. A picture is referred to where it is on disk,
  not copied; a video can be chosen as a fill, and is kept in the file, but only
  pictures are drawn so far.
- **Slides.** The `+` over the list on the left adds a slide with nothing on it after
  the one being worked on. A right click on a slide in the list offers **New Slide**,
  which does the same after that slide; **Copy** and **Paste**, the copy
  going after the slide whose menu Paste is chosen from, in that presentation or
  another; and **Delete Slide…**. A slide's menu while showing has Copy, Paste and
  Delete Slide… too. A pasted slide is a slide of its own, and so is everything on it.
  None of these can be undone with Undo, and each takes with it what could be undone
  before; deleting asks first.
- **Text style.** A slide's menu, in the editor's list and while showing, has **Copy
  Text Style** and **Paste Text Style**: the look of one slide's text boxes put on
  another's, in this presentation or another. It is the copied slide taken as a
  [theme](themes.md) would be, for its text boxes alone. The text boxes of the two that
  have words are matched as a slide's are with a theme's (by name, then by size, then
  in order), and each of the slide's takes everything of its match's: its name, place,
  size, fill, outline, shadow, and the one format of its text. It keeps its words and
  the chords over them. Nothing else of the slide changes, and nothing else of the
  copied one comes with it: not its shapes or pictures, not a text box left over, not
  its background. In the editor it is a step that can be undone; while showing it is
  for good, and the file as it was is kept with the editor's backups first, as for a
  theme. (What "text style" covers is a choice of this app's, from taking the copied
  slide as a theme: ProPresenter's own has not been looked at.) **Apply This Text Style
  to All**, under Copy Text Style, puts the slide's on every other slide of its
  presentation at once (this app's own: see [Text](text-format.md#apply-this-text-style-to-all)).
- **Visibility.** An element can be set to show only when other elements of the slide
  have text, or have none: all, any or none of a list of conditions.
- **Linked text.** A text box can show something other than text of its own, in its
  own font and colour. What it shows is picked from the one button beside **Shows**,
  which opens a menu, as ProPresenter's does; where there is more to say about the
  thing picked, that is set under the button.
  - **Another Element** leads to the slide's other elements: the box shows that one's
    text, as it is or run together onto one line, or broken into a word or a letter to
    a line.
  - **Current Slide** and **Next Slide** show something of the slide that is live, or
    of the one after it, which is what the boxes of a stage layout mostly do. A picker
    under the button says what: **Text**, all of the slide's words; **Notes**, the
    slide's notes, with a tick box for keeping their formatting (their own fonts,
    sizes and colours, in place of the box's); or **Match Element Name**, the words of
    the slide's elements of the name typed under it.
  - **Timer** leads to the workspace's [timers](timers.md): the box shows that one's
    time. How the time is written is set as ProPresenter sets it, a part at a time:
    the hours, the minutes, the seconds and the hundredths of a second are each
    hidden, or shown as one digit or as two, or shown that way but hidden while they
    are nothing. A part that is hidden is counted in the next one shown, so seconds
    alone count past sixty. Under it are the timer's **Color Triggers**, the colours
    its words change to as the time comes to given times (see [Timers](timers.md)); a
    video's time left has them too.
  - **System Clock** shows the time of day, the date, or both. Each is written one of
    five ways, by the names ProPresenter has for them (None, Short, Medium, Long,
    Full), in the words and the order this computer is set to; a tick box has the
    time on the 24 hour clock.
  - **Video Countdown** shows how long the video that is playing on the output has
    left, written a part at a time as a timer is, and nothing (`00:00`, say) while no
    video plays. A tick box has videos that loop left out.
  - **Stage Message** shows what the stage is being told
    ([Stage layouts](stage-layouts.md)), and nothing until it is told something. A tick
    box has the box flash when a message is shown, in the colour beside it: to begin
    with, the red ProPresenter's own layouts flash.
  - **Playback Marker** shows a [playback marker](playback-markers.md)'s **Marker Name**
    or the **Time to Marker**: of the presentation's media, the announcements' or the
    audio's (**Of**), which marker (**First**, **Previous**, **Next**, **Last**, or **By
    Name**, with the name typed), the time a part at a time as a timer's is, and a tick
    box for the marker's colour.
  - **Copyright** shows the copyright line of the presentation that is live, as the
    [copyright display](copyright.md) makes it, and nothing while there is none.

  A presentation has a fourth tab, **Copyright**: its own copyright details, for the
  [copyright display](copyright.md) (its Title, Artist, Author, Publisher, Year, CCLI
  Song Number and Album, and Display Copyright), changed as a step that can be undone.

  A linked box has a yellow outline in the editor, and in small yellow print at its
  foot what it is linked to. Some of this is a guess at ProPresenter, whose own menu
  was described and not looked at: the order and the names of the menu's rows; how a
  date and a time are joined when both are shown; what a video's time is when no video
  plays, and that a looping video that is left out counts as none; and that notes
  shown with their formatting kept are drawn at their own sizes, which on a layout as
  big as a screen are small unless the notes were written large.
- **Notes.** A slide of a presentation has notes: words for whoever runs the show or
  stands on the stage, which the audience never sees. **Notes**, at the bottom right
  of the editor, puts up a pane for them under the slide, and puts it away again; the
  pane's top edge is dragged to make it taller or shorter. It is a plain box of
  formatted text with no controls of its own, as ProPresenter's is: Ctrl+B, Ctrl+I
  and Ctrl+U make what is selected bold, italic or underlined (or all of the notes,
  with nothing selected), and a right click puts up a small panel with a font, a size
  and a colour. What is typed is saved a moment after the typing stops, and when the
  pane is left or another slide turned to; Undo takes it back. Notes start out as
  ProPresenter's do, in Helvetica Neue at 24 points, in white. They are kept in the
  presentation's file where ProPresenter keeps them, so each program shows what the
  other wrote; and notes typed onto a slide that had none and taken out again leave
  the file as it was. Props, stage layouts and themes have no notes, and their
  editors have neither the button nor the pane. (That the keys are these three and
  the panel has these three things is a guess at ProPresenter.)
- **Scale.** Text can be set, as in ProPresenter, to be made smaller until it fits its
  box, larger until it fills it, or either. Presentations that have this set in
  ProPresenter are drawn that way here.

Every change is saved to the presentation file as it is made, and Undo takes it back
out. Only what was changed is touched: everything else in the file, including whatever
this app does not understand, is written back as ProPresenter wrote it. The first
change made to a presentation in an editing session is preceded by a copy of the file
as it was, in `~/.local/share/SimplePresenter/SimplePresenter/Edit Backups`, where the
twenty most recent for each presentation are kept.

Fonts are recorded by name, as ProPresenter records them: each face by its PostScript
name (`CMGSans-Light`), with the family (`CMG Sans`) beside it. Text is drawn in the very
face named, so a family's Light, Medium, Black or Condensed is drawn as itself, not as
its Regular or Bold. A family that is installed from two makers (fonts name theirs, a
foundry, which Qt then shows after the family, as `CMG Sans [ULA]`) is called by its own
name, listed once, and written by its own name. Where the face named is not installed
here, the family's face of the same style is used if there is one (`Light` of
`CMGSans-Light`: a GUESS at what ProPresenter does, which keeps both names); text set
in a font that is not installed at all keeps its font unless another is chosen for it,
the editor marks the font as missing, and a substitute is used to draw it.

## Quick Edit

**Quick Edit**, beside Edit in a slide's right-click menu, is for putting a slide's
words right without the editor, as ProPresenter's is: for the typo seen in the middle of
a service. It puts the slide up on its own, over its thumbnail and about twice its size,
with a border of a pixel and a slim line along its top, so as to hide as little of the
other slides as it can (ProPresenter's is the size of the thumbnail itself). The slide
is drawn as the editor draws it, with its first text box being typed in. A click on a
text box types in it, where the click was; **Tab** goes on to the next one (and
Shift+Tab back). Nothing can be moved, sized or formatted there, and the keys that would
do those do nothing: it is for the words alone, and they keep the formatting they had.
Ctrl+Z undoes typing. **Esc**, the **✕** at its top right or a click anywhere outside it
put it away, keeping what was typed.

What is typed is saved to the file as the editor saves it, the file as it was being
kept with the editor's backups first. Nothing of it goes to the output: the slide is
not shown by being edited, and one that is live stays as it is on the screens until it
is next shown, as after the editor. A text box that is locked, or linked to something
else (a timer, the slide that is live), is not one to type in there.

A GUESS: how ProPresenter's looks, beyond its being a small editor of the slide's words
the size of its thumbnail; Tab; and that a click outside it keeps what was typed.
