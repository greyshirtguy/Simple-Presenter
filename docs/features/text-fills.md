[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Text fills

ProPresenter can fill a text's letters with more than a colour, over all of its words or
over some of them, and fill a box with what is behind it. The slides here draw each as
ProPresenter does, on the screens, in the operator's preview, in the thumbnails and in the
editor:

- **A gradient** across the letters it fills, from the first of them to the last, at its
  angle; or across the whole box, where it is set to stretch over it.
- **A picture** (a media fill): the picture fitted to the whole box as its media says
  (Scale to Fit, Scale to Fill or Stretch), each letter showing the part of it that is
  there, with what its Media Inspector sets.
- **A blur** of what is behind the letters, as much as ProPresenter blurs it, with its
  saturation.
- **An invert** of what is behind them.
- **A cut-out**: the letters are holes in their box's colour, and show what is behind it.
- **A box filled with a blur or an invert** of what is behind it, in its own shape, under
  its outline and its words.

What is behind is what is under the slide: the media and the video input, a video at each
of its frames; for a prop, an announcement, a message or a mask, the slide too. The slide's
own background colour, where it has one, is drawn over the media, as it is here, so it is
part of what is behind.

The words of such a text can be edited. Letters typed take the fill of where they are
typed, as they take its font, and the fills go with the words into the file, everything
ProPresenter wrote of them kept.

## Setting them

In the editor's Text tab, **Fill**, after Capitals, is ProPresenter's menu, with its names:
**Text Color**, **Text Gradient**, **Text Cut Out**, **Media Fill**, **Background Invert**
and **Background Blur**. It sets the fill of the words selected, or of all of them, as the
tab's other formats do, and shows the fill where the caret is. Under it, as ProPresenter
has them:

- **Text Color**: the colour, beside the menu.
- **Text Gradient**: its two colours, with the button between them that swaps them; its
  angle; and **Use object bounds for gradient**, for the gradient to run across the whole
  box and not across its letters.
- **Media Fill**: the file, with ProPresenter's gear beside it for another and its
  magnifier to show it in the file manager; **Inspector…** for the Media Inspector on it
  (cropping, turning, effects and the rest, for the words' picture alone); and its scale.
- **Background Blur**: **Saturation** and **Blur**, with ProPresenter's own note under
  them: *Blur percentage is shared among all objects on a slide.* So it is: setting the
  blur of one sets it for every blur on the slide, of words and of shapes alike; each
  keeps its own saturation.
- **Text Cut Out** and **Background Invert**: nothing more.

A box's own blur or invert of what is behind it is set in the Shape tab, as ProPresenter
has it: **Background Blur** and **Background Invert** in its Fill menu, a blur with the same
**Saturation** and **Blur**, its amount shared with every other blur on the slide.

A fill set over all of the words is written as ProPresenter writes it, as the text's own
fill and as a range over the words; over some of them, as a range. A fill read from the
file and changed keeps everything else ProPresenter wrote of it.

## How it is known

From a presentation the maintainer made for the purpose, of the five fills over a picture,
and ProPresenter's own pictures of its slides, set against this app's of them on a screen
of the same size. ProPresenter's blur of 0.26 is a blur of about 7 slide units, measured,
and one of 1 taken to be about four times that; this app's blur is within a fraction of
a pixel of it there. A gradient over one line ran across its own letters, not across the
box.

## Guesses

- That a blur grows in proportion to its amount, from the one measured, and that its
  saturation multiplies the colours' own.
- The order of the Fill menu (the order ProPresenter's pictures of it were taken in), and
  where Background Blur and Background Invert are in the Shape tab's (at its end); the
  Saturation slider's range, 0 to 2, its middle the colours as they are (ProPresenter's
  picture had it in the middle for a blur with none set); and how a new fill starts: a
  gradient from the words' colour to that colour darker, upwards, as a shape's does here;
  a blur at the amount of one already on the slide, or half; a picture Scale to Fill, as
  ProPresenter's picture showed one.
- With some of the words filled and some not, the text's own fill is written as the colour
  of those that are not: ProPresenter's file for that has not been seen.
- That a gradient stretched over the box (`stretch_to_document_bounds` in the file) runs
  across the text's whole box, inside its margins: the presentation measured has no such
  gradient.
- That what is behind is the layers under the element's own, and not the elements of its
  slide behind it: an element over another blurs what is under both. ProPresenter likely
  blurs the elements behind as well.
- A turned element takes what is under its box as if it were not turned.
- Where a slide is drawn over nothing (a thumbnail of a slide with no media, a stage
  layout), a blur of nothing is nothing, and an invert of it white, as on a screen with
  nothing on it. In the editor, where nothing is under the slide, letters filled with a
  blur are drawn half see-through white, so that they can be seen and placed: this app's
  own.

## Not yet

- ProPresenter's angle has a dial beside its number, which is not here.
- A cut-out of a box filled with a gradient or a picture leaves its letters unfilled,
  showing the box's fill, and not holes in it.
- Words with chords over them on the stage, or scrolling, have letters filled with a
  picture, a blur, an invert or a cut-out drawn in their own colour.
- A shadow of the words is not drawn under letters filled with a picture, a blur, an
  invert or a cut-out.
