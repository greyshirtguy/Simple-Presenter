[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Import PowerPoint

A PowerPoint file (`.pptx`) brought in as a presentation of the workspace, slide for slide,
with its words as text boxes that can be edited, its pictures as pictures, and its slide
backgrounds as the slides' background media. ProPresenter has this as File › Import ›
PowerPoint as Presentation (since version 21, with a switch for the backgrounds since 22);
how ProPresenter makes each thing over has not been looked at, and what is here is this
app's own reading of a PowerPoint file, done with what was learnt writing one
([Export to PowerPoint](export-powerpoint.md)).

The `+` beside Libraries has **Import PowerPoint…**: it asks for the file, and a panel
says what the file holds (its slides and its shape), has the one option, the library the
presentation goes into and the playlist it is added to (both as they were the last time
something came in), and **Import**. The import runs on a thread of its own, with how far
it has got and a **Stop**; at the end the panel says what was made and lists what could
not be carried over, on which slides. The presentation is opened in its library, and the
log says what came in.

A `.pptx` dragged in from the file manager and dropped on a library in the list, or on a
playlist, comes in at once, with nothing asked, as a drop in ProPresenter does: into that
library, or into the library the last import went into and added to that playlist, with
the switch for the backgrounds as it was last set. The panel comes up all the same, to
show how far it has got and then what came of it; several files dropped together come in
one after another, and it says what came of each.

## What comes over

- **Each slide**, in the file's order, as a cue with a slide of 1920 by 1080. A deck of
  another shape (4:3, 16:10) is fitted into the slide and centred, with black at the
  sides, as the output is where nothing is drawn. A hidden slide (PowerPoint's Hide
  Slide) comes in as a [disabled](disabled-slides.md) one.
- **Its words**, as text boxes: each run's font, size, bold, italic, underline, strike,
  colour, letter spacing, superscript and capitals, and an outline round the letters;
  each paragraph's alignment and line spacing; the box's insets, its vertical alignment,
  and whether PowerPoint shrank the words to fit it (they come in at the size PowerPoint
  drew them, set to shrink to fit as well). What a placeholder inherits from its layout,
  its master and the theme (its place, its size, its font, its colour, its bullets) is
  followed down, as PowerPoint follows it; a placeholder with nothing typed into it shows
  nothing, as in PowerPoint's slide show, and is left out. A bulleted or numbered
  paragraph gets its marker written into its words.
- **Where the lines fall.** PowerPoint and ProPresenter lay text out otherwise (see
  [Export to PowerPoint](export-powerpoint.md#what-goes-over)); the box's line spacing and
  top margin are set so that ProPresenter's lines fall where PowerPoint's do, by the same
  measurements the export uses, the other way round.
- **Shapes**: rectangles, rounded rectangles and ellipses as ProPresenter's own; triangles,
  diamonds, parallelograms, trapezoids, pentagons, hexagons, octagons, chevrons, arrows
  and stars as drawn outlines, straight-sided; a shape drawn by hand by its corners, its
  curves straightened. Their fills (a colour, a gradient, a picture), their lines and their
  shadows, those the shape has of its own and those its style takes from the theme.
- **Pictures**, cropped and stretched into their boxes as PowerPoint had them, flipped,
  faded; **videos** on a slide, playing when it comes up.
- **Backgrounds.** A slide's background colour (its own, its layout's or its master's,
  which is most often the theme's white) becomes the slide's background colour; a
  background picture or video becomes the cue's background media, on the media layer
  under the slide, where a look can hide it and the words stand alone; a gradient becomes
  a rectangle over the whole slide at the back. **Leave out the slides' backgrounds**, the
  panel's one switch, leaves all three out, for a deck whose words are to go on a theme
  of the workspace's own.
- **Speaker notes** become the slides' notes, and **sections** become groups, by name; a
  deck with no sections has one group with no name, which shows as plain slides.
- **Pictures and videos** are copied out of the file into the workspace, under
  `Media/Imported/<presentation name>/`, by the names they have in the file
  (`image1.png`). That ProPresenter's own import puts media under `Media/Imported` is a
  guess from the empty folder of that name in a workspace ProPresenter made.

## What does not come over

Said at the end of the import, with the slides it is on:

- **Tables, charts, SmartArt and embedded objects** are left out, and so are **lines and
  connectors** and **sound**.
- **Transitions and animations**: every object is shown as it is once its animations are
  over. (A slide whose objects were all to fly in shows them all.)
- **Fonts not installed here** are drawn in a stand-in, as any presentation's are; the
  report names them. PowerPoint's own fonts (Calibri, Cambria, Segoe) are seldom on a
  Linux computer; the Carlito and Caladea families stand in for the first two, with the
  same widths, where they are installed.
- **Lists**: the marker is written into the words, and a line that wraps does not hang
  under the first. The space PowerPoint puts before and after a paragraph is not kept.
- **Text set vertically** is shown horizontally; a **picture cut to a shape** is shown
  whole in its box; a **tiled** picture is stretched; a **patterned** fill is shown as its
  colour; words filled with a gradient are shown in its first colour, and highlights
  behind words are left out.
- **`.ppt`**, the old PowerPoint format, is not read; nor are files from Keynote or
  Google Slides unless they are saved as `.pptx`, which both can do.

## Guesses

Marked as such in the code (`src/pptxread.cpp`, `src/pptxmake.cpp`), where they are made:

- That ProPresenter's own import keeps media under `Media/Imported`, and that it makes a
  hidden slide a disabled one; that its "without slide backgrounds" leaves colours out as
  well as pictures.
- That a text box with no fill and no line of its own is a text element to ProPresenter,
  and a shape with words a shape; in ProPresenter's files the two are one kind of thing.
- How PowerPoint lightens a theme colour (its tint and shade), which the standard does not
  say in plain words: a straight blend towards white or black, close on screen.
- That PowerPoint draws a new rectangle's words in the theme's light colour though the
  master's text style says dark: the shape style's font colour is taken over the master's.
- That a presentation made here opens in ProPresenter. Every part of it is written by the
  functions the editor writes with, and none made from a PowerPoint file has been opened
  in ProPresenter yet.
- That a file dropped on a playlist goes into the library the last import went into, and
  that a drop asks nothing first: ProPresenter takes a PowerPoint file dropped on a
  playlist or a library, and what it does then has not been looked at.
