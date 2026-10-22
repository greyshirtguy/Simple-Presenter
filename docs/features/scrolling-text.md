[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Scrolling text

A text box's words can go across it, left or right, or up or down it, as ProPresenter's
Scrolling Text does: a ticker along the foot of the announcements, credits rising up the
screen, or, as in a presentation of ProPresenter's own, rows of a song's words drifting
behind it in both directions.

It is set in the editor, in the **Text** tab of a text box, under **Scrolling**:

- **Scrolling**, ticked, makes the box's words scroll. A box that has never scrolled is
  given ProPresenter's settings for a new box (half speed, over and over, a small gap).
- **Direction**: the four arrows.
- **Start**: **Automatic**, the words starting where they are in the box, or
  **Off-Screen**, the words coming in from the edge of the box they go away from.
- **Speed**: the slider, slow at the left.
- **Feather**: how far in from each end of the box the words fade, the start (left, or
  the top) and the end (right, or the bottom). With the button between the two sliders
  lit, the two move together.
- **Repeat**, ticked, has the words come round again and again, with a gap after each
  going of them that the slider beside it sets. Not ticked, they go across once and are
  gone.

The words scroll on the screens, on the stage, and in the operator's preview of a screen,
from when the slide is shown. In the slide's thumbnail and in the editor they stand still
where they are. Words going across are one line, as long as they are; words going up or
down are as wide as the box. A box that shows another's text, or the slide's, scrolls
that.

Everything is ProPresenter's own fields of the box (`text_scroller`), read and written as
ProPresenter has them, and nothing else of the file is touched.

**What is a guess.** The file says where ProPresenter's sliders were, not what they mean,
and the speed and the rest were worked out from two of ProPresenter's own presentations
and not measured against it:

- how fast a Speed is: from 20 slide units a second at the left of the slider to 500 at
  the right, each tenth about a third faster than the last; at the middle a 1920-wide
  screen is crossed in about 20 seconds;
- that the gap is a share of the box, along the way the words go;
- that the feathering is a share of the box at each end, and that for words going up or
  down the left's is the top's and the right's the bottom's;
- that words going across are one line and words going up or down as wide as the box,
  and that words that do not repeat go once and are gone;
- the order and the words of the controls, which were taken from ProPresenter's help
  and not from its window.

How it is drawn: the words are drawn once, and each frame then moves them in one pass
over the box, however many times they come round in it; a slide of ProPresenter's with
thirteen scrolling boxes on it is thirteen such passes a frame. Words longer than 8192
pixels on the screen are drawn a little soft.
