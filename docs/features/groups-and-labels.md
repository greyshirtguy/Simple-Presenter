[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Groups and labels

A right click on a slide has two rows for what the slide is called and belongs to.

**Group** lists the workspace's groups, which are ProPresenter's list of them
(`Configuration/Groups`), each with its colour, and the slide's own ticked. In a
workspace that has no such list it offers the groups set up on the settings screen.
Choosing one works as it does in ProPresenter: it sets the group of that slide **and of
the slides after it that are in its group now**. So on the first slide of a group the
whole group becomes the one chosen (it is still the same group of the presentation,
and keeps its place in every arrangement); on a later slide the group is split there,
the slides before staying as they were and the slide and those after it becoming a new
group straight after. The new group is written as ProPresenter writes one given from
its list: its name, its colour, and the id of the list's group.

Two things there are guesses. When a group is split, the new group is put after the old
one wherever an arrangement has it, so that every arrangement goes on showing the same
slides in the same order. A new group that no arrangement had would vanish from all of
them. And a slide that is in a group of the chosen name already is left as it is, with
nothing written. Whether ProPresenter does either has not been looked at.

**Label** gives the slide a label, which is a few words and, if wanted, a colour, shown
in the slide's caption: plain words after the group's name, or, if the label has a
colour, a tag of that colour at the end of the caption. Its menu is ProPresenter's:

- **Edit…** puts up a panel with a box for the words and the colours to pick from: one
  for no colour, eighteen ready, and **Other…** for the system's own colour picker.
- **None** takes the label off, and out of the file.
- The **labels the workspace keeps ready**, which are ProPresenter's list of them
  (`Configuration/Labels`): a click gives the slide that label, words and colour.

The labels kept ready are looked after in the settings, under **Labels**: made, changed
and removed. Changing one there does not change the slides that were given it, each of
which has its own copy of the words and the colour. A workspace that has no list gets
the file with the first label made.

A label is kept where ProPresenter keeps it, on the slide's action in its cue, and
written as ProPresenter writes one. A guess: how ProPresenter draws a label's colour on
a thumbnail (the tag is this app's own), and which colours its swatch has.
