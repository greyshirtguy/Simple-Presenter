[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Timers

Under the transport are the show controls: a row of tabs, pictures and not words, of
which the one showing is blue. Timers are the first; [props](props.md), the
[stage screens](stage-layouts.md), the [macros](macros.md), the [audio bin](audio.md) and the
[messages](messages.md) are the other five. The `+`
under the tabs adds to whichever is showing.

<img src="../timers.png" width="336" alt="The timers tab, under the row of the show controls' six tabs (timers, props, stage, macros, audio and messages): a countdown of five minutes with its settings open, a countdown to a time of day, and an elapsed timer that is running">

A workspace's timers are ProPresenter's, and a workspace with none starts with one, a
five-minute countdown. There are three kinds:

- a **countdown** runs from a length of time down to nothing;
- a **countdown to a time** runs down to a time of day. It needs no starting: it shows
  how long it is until then, and stopping it holds it where it is. A time that went by
  less than six hours ago counts as gone by; any other is the next time it is that time;
- an **elapsed time** counts up, from nothing or from a time, to an end if it is given
  one.

Each stops when it gets there, unless it is set to overrun. Then it runs on past its
end: a countdown carries on below nothing, as a negative time, in red.

A timer is a row: its name, its time, and buttons to put it back at its start and to
start or stop it. A click on the row opens it, to change its name, its kind, its time
and whether it overruns. The `+` adds a timer;
Remove, or a right click, takes one away. Lengths of time are typed as hours, minutes
and seconds and read from the right, so `90`, `1:30` and `0:01:30` are all a minute
and a half; a time of day is typed on the 24-hour clock, as `10:30`.

The audience sees a timer through a text box that is [linked to it](editing.md): the box
shows the timer's time, in the box's own font and colour, on the output, in the
thumbnails and in the editor. The link finds its timer by the id ProPresenter gave it
and, failing that, by its name, which is what still holds when a presentation was made
on another machine.

**Color Triggers.** A text box linked to a timer, or to how long a video has left,
can change colour as the time comes to given times: ProPresenter's Color Triggers,
under the link in the editor's Text tab. Each is a time and a colour. The time's words
take a trigger's colour when a countdown comes down to it (a countdown to a time
likewise), keep the colour of the last one through an overrun, and are in the box's
own colour while the time is above them all; the colour of the one the time has come
to last is the one shown. ProPresenter's own example is orange at 1:00, yellow at 0:30
and red for the last ten seconds, and its stage layouts come with a red one at 0:00.

- **+** adds one: the first at nothing, in red, each after it half a minute before the
  latest, in yellow. A time is typed as ProPresenter writes it, minutes, seconds and
  hundredths (`1:00.00`), with hours before them if wanted (`1:01:00.00`); `1:00`,
  `90` and `:30` are read too, and what is not a time is not kept. The colour is
  picked from its swatch, and **×** takes a trigger away.
- The handle at a row's left drags it to another place in the list, which
  ProPresenter cannot do. The order does not change which colour is shown (each time
  is compared); it is kept in the file as it is.
- **⋯** has **Save as Default**, which keeps the list in the app's own settings, the
  same in every workspace, and **Recall Default**, which puts that list on the box
  being edited in place of its own: a quick way to give any new box the usual ones.

They are kept on the link in ProPresenter's file, a stage layout's or a
presentation's, where ProPresenter keeps them, and whatever else is there stays as it
was. What is a guess, its help saying nothing of it: that an elapsed time changes
colour as it goes up past each time; that the time is kept in seconds; and what +
adds. An audio countdown's link has triggers in the file too, which are kept but not
shown or set here, the app having no such link of its own yet.

A slide can also work a timer when it is triggered: start it, stop it, put it back,
and set it up first, which is how one timer is a minute's countdown on one slide and
three on another. Presentations made in ProPresenter that do this, such as a slide
that starts its own countdown, do it here too. What a slide sets a timer up as is not
written to the workspace until the timers are next changed by hand. Such an action is
given to a slide, and changed, as any [action](actions.md) is.

An action finds its timer as a link does: by id, and failing that by name. One that
finds none either way does nothing, and a text box whose timer is not there shows a
time of nothing; neither is treated as an error.
