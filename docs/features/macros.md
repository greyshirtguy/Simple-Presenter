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
slide); **Remove Action**, which lists its actions by name; a row for each action that has
anything to change, leading to what it can be changed to, as a slide's Edit Action does
(another macro, look, prop, stage layout or thing to clear, picked from a list with the one
it is ticked; its panel, for a timer, a stage or a message action), and to Remove;
Rename; Colour; Duplicate; Move to another collection; and Remove. The `+` over the
list adds a macro or a collection. One of the clear buttons dragged onto a macro
gives it that clear action, a layer's or a group's; a piece out of the [audio
bin](audio.md) dragged onto one gives it the action that plays that piece; and a picture
or a video out of the [media bin](media.md), the action that shows it on the media
layer, as a click on it in the bin does. Held over the macros' tab, either brings the tab
forward; dropped on the strip along the bottom of the list, or on the list where there
is no macro, it is given a macro of its own, called what it is called. A macro can run
another macro; two that run each other are stopped after eight turns.

The macros are ProPresenter's own, in `Configuration/Macros`, read and written as
they are: a workspace ProPresenter has used comes with its macros, and the slides that
run them now do so here. A macro's picture is the M in brackets, or the letter or
digit it has in ProPresenter; the rest of ProPresenter's set of pictures, and a
picture of the user's own, are kept in the file and drawn as the M for now. Actions of
kinds not understood here are kept too.

**What is a guess, or not done:** that the action a picture or a video is given in a
macro is a copy of its row's own, as the media bin's file has it, called what the row
is called (no macro of ProPresenter's with media in it has been looked at); that the
macro shows it as a click in the bin does, leaving it to play on if it is playing
already; and that a macro made for it is called what its file is called, without the
ending. Its picture in the macro's row is this app's own, the pack of ProPresenter's
pictures here having none for media. Not done: there is no Media under Add Action,
and while a macro's video plays, its playback markers are gone by but cannot be
changed from the transport, there being no row of a bin for them to be kept in.
