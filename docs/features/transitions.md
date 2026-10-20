[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Transitions

The transition is chosen at the left of the thin bar under the slides (the buttons for
the size of the thumbnails are at its right), and applies to every change on the
output, slides and media alike; the slider beside it is how long it takes. The
menu has them by category, as ProPresenter does: Dissolves, Wipes, Movements, Objects,
Color and Blurs, and then More, for the ones ProPresenter does not have.

Some transitions can be adjusted: the direction a wipe or a push travels, the colour of
a burn, the size of the squares. For those, the button with the sliders on it, beside
the transition, opens a panel with what there is to adjust: a slider for a number, a
swatch for a colour, and a pad of nine places for a direction, which is where what is
coming comes from. A change takes effect with the next transition and is remembered for
that transition; Reset puts it back as it came.

The names, the categories and what can be adjusted are ProPresenter's, so that what is
known from there is found here. The shaders are not: ProPresenter's are its own. Each
transition here is either written for this app to give the same kind of change, or
ported from [gl-transitions](https://gl-transitions.com), from which ProPresenter
adapted several of its own. So they are equivalents and not copies, and will not match
ProPresenter's frame for frame.

On the laptop in [Built for modest hardware](../how-it-is-made.md#built-for-modest-hardware), with a slide
and its media changing at once on a full-screen 1080p output, all of them keep sixty
frames a second but one, Cross Zoom, which manages fifty-seven.

A presentation can have a transition of its own, and so can one of its slides, as in
ProPresenter, and those are made in place of the one chosen here: the slide's own, or
failing that its presentation's, or failing that the one chosen. A slide's own is set
in the editor's Build tab (see [Builds](builds.md)); a presentation's is kept as
ProPresenter set it. The log says, for each change, whose transition it was.
