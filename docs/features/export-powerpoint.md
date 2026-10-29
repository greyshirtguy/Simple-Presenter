[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Export to PowerPoint

This app's own, not ProPresenter's: a presentation written as a PowerPoint file (`.pptx`)
that looks as it does here, slide for slide, with its words as text boxes that can be
edited, not pictures of slides. A right click on a presentation, in a library or in a
playlist, has **Export to PowerPoint…**: a panel puts up the options, **Export…** asks
where the file is to go (the presentation's name as a `.pptx`, in the folder exported into
last, or Documents), and the export runs, with how far it has got and a **Stop**. At the
end the panel says how many slides went where, and lists what could not be carried over,
on which slides; **Show in Folder** opens the folder. The log says what was exported, and
where.

It is the maintainer's [ProToPPTX](https://github.com/greyshirtguy/ProToPPTX), a Mac app
that does the same, brought into the app: its reading of ProPresenter's files, its writing
of PowerPoint's, and what it found out by measuring ProPresenter's exported slide pictures
against PowerPoint's own pictures of its decks (where text starts in a box, how far apart
lines are in each, how PowerPoint names a font's faces), all of it ported (see
`src/pptxdeck.h`). Media is found, and fonts are drawn, as the rest of this app finds and
draws them.

## What goes over

- **Each slide ProPresenter would show**, in its order: the groups as Master has them, or
  the arrangement the presentation's row shows (a playlist's item can have its own), a
  group as often as the arrangement repeats it. A slide no group lists is left out, as
  ProPresenter leaves it out.
- **Its words**, as text boxes with their fonts, sizes, colours, outlines, shadows,
  alignment, line spacing, indents, tab stops and lists, placed where ProPresenter puts
  them: PowerPoint lays lines out otherwise, and each box is set so that its lines fall
  where ProPresenter's do. Text set to shrink to fit its box is given PowerPoint's
  "shrink text on overflow", at the size it is drawn at here. Title and start case, which
  PowerPoint has no setting for, are put into the words.
- **Its shapes and pictures**: rectangles, rounded rectangles, ellipses and other shapes
  (as straight-sided ones), their fills (colours, gradients, pictures), lines, shadows and
  soft edges, pictures cropped and fitted as ProPresenter fits them, and videos, which play
  when their slide comes up, looping if they loop, each with a still of its own (PowerPoint
  wants one).
- **Media cues**: a slide's picture or video behind it. In ProPresenter a background stays
  up while the slides after it, with none of their own, are clicked through; PowerPoint
  has no such thing, so by default each such slide is given the background that would be
  on screen. Foreground media goes over its own slide.
- **Notes** go to the speaker notes, and **groups** become PowerPoint's sections (unless
  every group is called "Group", or nothing).

## The options

ProToPPTX's, kept for the next time.

| Option | To begin with | What it does |
| --- | --- | --- |
| Keep background media on following slides | on | Gives each slide the background that would be on screen when the slides are clicked through in order |
| Leave out slides turned off | on | A slide [disabled](disabled-slides.md) cannot be shown in ProPresenter; with this off it goes over all the same, and the report says so |
| Put slide notes into speaker notes | on | |
| Make groups into PowerPoint sections | on | |
| Follow the chosen arrangement | on | Off, every group once, in Master's order |
| Background pictures | As the slide's background | As the slide's background (Format Background), where it cannot be moved by mistake while the words are edited; or as a picture at the back, which can be selected and replaced. A picture whose shape is not the slide's, and videos, always go in as a picture at the back |
| Videos | Put the video in | The video copied into the file; a still only (a smaller file); or left out |

## Words filled with what is behind them

ProPresenter can fill a text's letters with things PowerPoint has no such fill for, which
ProToPPTX leaves out. Here each becomes a picture filling the letters, which PowerPoint can
do, made of what is behind the text box on its slide (its background colour and media, a
video's still, and the shapes and pictures behind it; not the words of other boxes):

- **Background blur**: the letters show the slide behind them blurred. How much was
  measured against ProPresenter's own picture of a slide made for the purpose: its blur of
  0.26 is a blur of about 7 slide units (a Gaussian's standard deviation, on a 1080p
  slide), and that is taken to grow in proportion for more or less. A shape filled with a
  background blur is the same, filled with the picture.
- **Background invert**: the letters show what is behind them with its colours inverted.
- **Cut out**: the letters are holes in their box's own fill, through which what is
  behind the box shows; the box keeps its fill, and the letters are filled with what is
  behind it.
- **Media fill**, which PowerPoint has: the letters filled with a picture, fitted to the
  whole box as ProPresenter fits it (a video's still, for a video).

The report says which slides have such words. LibreOffice draws letters filled with a
picture in their colour; PowerPoint draws the picture.

## Fonts

PowerPoint names a font by a family and its bold and italic switches, and its families
have four faces at most (Regular, Bold, Italic, Bold Italic); ProPresenter names a font by
the PostScript name of its face. Where the face is installed here, its own name table says
what Office calls it; any other face, such as Helvetica Neue's Condensed Black, is named by
its full name, which is how PowerPoint finds it. A face that is not installed here (most
of a Mac's own fonts, on Linux) is named all the same, by its family as the presentation
gives it and its style spelt out ("Helvetica Neue Condensed Black"), and the report says
it is not installed: PowerPoint shows it where it is.

## What does not go over

Said in the report where it happens:

- **Macros**: what one does is kept in the workspace, not the presentation; one that
  changes the background cannot be followed, and its slides are listed to be checked.
- **Sound**: audio cues are not put in.
- **Chords**: only the words go over.
- **Text set to show on one line, or a word at a time**: shown as written.
- **Text set to grow to fill its box**: shown at its own size.
- Media that cannot be found here, by its file's name.

Not said, and not carried over: slide transitions and builds, a media cue's Media Inspector
settings (its crop, effects and the rest), live video and web content, timers, and the
words of other text boxes behind a box whose letters are filled with what is behind them.

## Guesses

ProToPPTX's, where it marked them so (in the code, where they are made), and these here:

- That PowerPoint stretches a picture filling letters over the text's box, as it does a
  shape's; the pictures are blurred, or are what is behind the box, so a little out of
  place they look the same.
- That a blur grows in proportion to its amount, from the one measured; and that a
  saturation written for it multiplies the colours' own (none was written in the slide
  measured, and the colours were as they are).
- A face not installed here named by its family and its style spelt out.
- Where lines fall is measured with this app's layout of ProPresenter's text, which is
  not macOS's, as ProToPPTX's was: the same fonts' metrics, not the same pictures.
