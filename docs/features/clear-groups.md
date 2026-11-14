[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Clear groups

A **clear group** is a named set of layers that are cleared together, at one click.
They are ProPresenter's, kept in the workspace where ProPresenter keeps them
(`Configuration/ClearGroups`), and a workspace starts with the one ProPresenter starts
it with: **Clear All**, which clears every layer. A workspace that has no such file is
shown that group too, and the file is not written until something is changed.

Each group has a button, in a column of its own beside the preview, next to the column
of the layers' clears, in the order the groups are in. The buttons share the column's
height between them, as ProPresenter's do, so the one group a workspace starts with has
a button as high as the preview's area; each has its picture and no words. A button is red while any of the
group's layers has something on it, and a click clears them. **F1 clears the first
group**, which is Clear All unless the list has been put in another order. A group's
button can be dragged onto a slide or a macro, as the clears of the layers can, to give
it the action that clears that group: see [Actions](actions.md).

A right click on any of the clear buttons has **Clear to Logo** (below) and
**Configure Clear Groups…**, which puts up the window they are set up in: the groups
down the left, and on the right what the one picked has.

- **The layers it clears.** ProPresenter's eight, in its order: audio, audio effects,
  messages, props, announcements, the slide, the media and video inputs, each ticked
  and unticked. The audio is the whole of the [audio](audio.md) layer, its music and
  its sound effects; the audio effects are the sound effects alone, so that a group
  can stop those and leave the music playing. The video inputs are the
  [video input](video-inputs.md) layer.
- **Stop Timeline**, for the presentation and for the announcements: whether the group
  stops the [timeline](timeline.md) playing there, whatever layers it clears. Clear All
  stops both, as ProPresenter's does.
- **The picture** on its button, one of the thirty-two ProPresenter has to choose from.
  For the two that ProPresenter's icon pack has (the one called All, and the message)
  it is ProPresenter's own picture; the digits are digits; and the rest are this app's
  own drawings of what each is called (a bell, a star, a cupcake), not ProPresenter's.
  A group that was given a picture of the user's own in ProPresenter shows that
  picture and keeps it; one cannot be given here.
- **A tint**, switched on or off, and its colour, chosen from the app's colour picker.
  The picture is drawn in it, fainter while the group has nothing to clear.
- **Whether its button is shown.** A group kept out of sight has no button and is
  otherwise as it was: an action that clears it still does, and so does F1 if it is
  the first.

The **+** makes a group, which clears nothing until it is given layers. A right click
on a group in the list renames it, moves it up or down the list, or deletes it; a
double click renames it too. Every change is written to the file as it is made, and a
change of mind (a layer unticked and ticked again, a tint switched on and off) leaves
the file exactly as it was.

What is a guess, ProPresenter's own window for this not having been looked at: how
that window is laid out; the order the layers are listed in, which is the order
ProPresenter's own group has them in; that its "audio" stops the sound effects as well
as the music; where its Stop Timeline is, which its help puts under Presentation and
under Announcements; that F1 is for the first group of the list; what
a new group is given (here a name, no layers, and for its picture the first digit that
no group has); and that any group, the one a workspace starts with among them, can be
renamed, moved and removed. What it rests on is one file of ProPresenter's own, with
its one group in it, and one presentation with an action that clears that group.

## Clear to Logo

**F12**, or **Clear to Logo** in the clear buttons' menu, clears everything, as Clear
All does, the audio included, and puts up the **logo**: a picture or a video, chosen in
**Settings → Screens** (Logo, Choose…). It is put on the media layer as media is, by
the media's transition, on the screens that show the media, and the media's clear
button is red while it is up. It stays only while nothing else is on: a slide, media,
a prop, a message, an announcement or a video input that comes on takes it off again,
and clearing the media clears it too. Audio is the exception: music or a sound played
leaves the logo up, so that music can play under it. A slide or a macro whose action is
ProPresenter's clear to the logo does the same as F12.

With no logo chosen, F12 clears everything all the same and says where one is chosen;
a logo whose file has gone is said to be missing, and nothing is put up.

ProPresenter keeps its logo in its own settings, on that computer, not in a workspace,
and so does this: it is the app's own setting. What is a guess: that ProPresenter's
logo is shown as its media is (on the screens a look gives the media, fitted, by the
media's transition), and that its Clear to Logo clears the audio, as Clear All does.
That anything coming on but audio takes the logo off is the maintainer's rule.

