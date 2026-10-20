[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Macros

A **macro** is a named list of actions, of the kinds a slide can have. Running it does
them all, in order. So something many slides should do (give the stage the singing
layout and clear the props, say) is set up once, as a macro, and each of those slides
has the one action that runs it.

The fourth tab of the show controls, the **[M]**, lists the workspace's macros by
collection. A macro has its picture at the left, on a rounded block of its colour
(royal blue unless it has been given another), as ProPresenter draws one, and two
lines beside it: its name, and under that a small icon for each of its actions, in
their order, so that what it does can be seen at a glance. A click on a macro runs it.

Everything else is in its menu, on a right click: Run; **Add Action** (as for a
slide); **Remove Action**, which lists its actions; a row for each action that has
anything to change (a timer action, a stage action), leading to Edit and Remove;
Rename; Colour; Duplicate; Move to another collection; and Remove. The `+` over the
list adds a macro or a collection. One of the clear buttons dragged onto a macro
gives it that clear action, a layer's or a group's; a piece out of the [audio
bin](audio.md) dragged onto one gives it the action that plays that piece. A macro can run another macro; two that run each other
are stopped after eight turns.

The macros are ProPresenter's own, in `Configuration/Macros`, read and written as
they are: a workspace ProPresenter has used comes with its macros, and the slides that
run them now do so here. A macro's picture is the M in brackets, or the letter or
digit it has in ProPresenter; the rest of ProPresenter's set of pictures, and a
picture of the user's own, are kept in the file and drawn as the M for now. Actions of
kinds not understood here are kept too.
