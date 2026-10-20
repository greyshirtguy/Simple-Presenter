[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Masks

A **mask** is a slide laid over everything else an audience screen shows: the top
layer of all, over the messages. Whatever is drawn on it covers what is under it, and
where it has nothing the output shows through. A mask of black shapes round the edges
gives a screen another outline; one black box keeps a projector off a doorway.

Masks are made, and a screen is given one, in the [Looks](looks.md) window, as in
ProPresenter. The **Masks** row there has, for each screen, the mask the look gives
it, and a click on it has the menu: **None**, the workspace's masks by name, **Add New
Mask** and **Edit Masks…**. Choosing one for the live look puts it on that screen at
once; a saved look keeps its own choice, for when it is made live. The operator's
preview of an audience screen has that screen's mask over it, as the screen has.

**Add New Mask** and **Edit Masks…** bring up the [editor](editing.md) on the masks,
which are edited as slides are: the list at the left is of the masks, each with its
name under it, and the `+` over it makes another. A shape added to a mask starts
black, and a mask is drawn on grey there so that what is black on it can be seen.
A click on the name under a mask renames it, as **Rename** in its right-click menu
does; **Remove…** takes the mask away, and with it every look's giving of it to a
screen.

There is no clear button for a mask and no clear takes one off, Clear All included:
it is part of how a screen is set up, not of the show.

Masks are ProPresenter's, kept where it keeps them, in the workspace's set-up file
(`Configuration/Workspace`) with the screens and the looks.

What is a guess, or not done. No workspace this was made with had a mask in it, so
more of this than usual is read from the file's format and not seen in a file:

- That a look names a mask by the id of the mask's slide, which is the only id a mask
  has.
- That what is drawn on a mask covers the output in the colour it is drawn in.
  ProPresenter's own words are that the layer "is used to mask parts of the
  presentation, creating a custom shape or transparent areas".
- What a new mask is given: a slide of 1920 by 1080 with nothing on it. ProPresenter
  lets a mask's size be set, to match its screen; that is not here yet.
- That removing a mask takes it out of the looks, and that a new shape on one is
  black.
- A mask comes and goes at once, not over the look's time as the layers do.
