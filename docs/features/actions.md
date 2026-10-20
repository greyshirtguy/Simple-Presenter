[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Actions

A slide can do more when it is shown than show itself and its media. In ProPresenter's
files a slide's cue is a list of **actions**, and the app runs the ones it knows as
the slide goes live: the slide first, and then its actions, in the order the cue has
them. (So an action that clears the slide clears that slide, which is how a cue is
made that shows nothing of its own. Such a slide is still the one the show is at: it
is marked as the live one, and the arrow keys go on from it.)

- **Timer**: start, stop or reset a [timer](timers.md), or reset and start it, and
  optionally set it up anew first (as a countdown of so long, a countdown to a time of
  day, or an elapsed time).
- **Clear**: everything, the slide, the media, the props, the messages or the
  [announcements](announcements.md). (And the audio, by dragging its clear button onto
  the slide.)
- **Clear Group**: clear one of the workspace's [clear groups](clear-groups.md), which is
  to say those of its layers that there are here.
- **Stage**: give each stage screen one of the workspace's [stage layouts](stage-layouts.md),
  or leave it as it is.
- **Prop**: put a [prop](props.md) on (Trigger) or take it off (Clear).
- **Macro**: run a [macro](macros.md).
- **Audience Look**: make one of the workspace's [looks](looks.md) the live one.
- **Audio**: play a piece of the [audio bin](audio.md), music or a sound effect. It is
  made by dragging the piece onto the slide.
- **Communication**: send a MIDI note, a RossTalk Custom Control or an HTTP request to
  one of the workspace's [devices](devices.md).

Each action a slide has is a small icon in the top left corner of its thumbnail, after
the icons for its hotkey and its media. A right click on a slide offers **Add Action**,
which leads to the kinds above: Clear, Clear Group, Prop, Macro and Audience Look to
lists to pick from (a prop by its collection, then Trigger or Clear; a macro by its
collection), Timer, Stage and Communication to a small panel to fill in. Under it is **Remove Action**,
which lists the actions the slide has. A right click on an action's icon says what the
action is and offers to change it, for a timer or a stage action, or to remove it;
for one that plays audio it has Stop or Loop, and Music or Sound Effect.

A row of a menu that leads to a menu of its own has an arrowhead at its right, and its
menu opens beside it when the pointer rests on the row (or on a click). The menu it
came from stays where it is, and each is kept inside the window.

Whatever is listed in the show controls can also be **dragged onto a slide**: a timer
or the stage screen, which bring up their panel; a prop, which asks whether it is to
be triggered or cleared; a macro, which needs nothing more; a piece of audio, which
needs nothing more either. So can the **clear
buttons** under the previews, each of which gives the slide the action that clears its
layer, or its group; and those can be dropped on a macro as well.

An action that shows a [message](messages.md), which a slide or a macro may have from
ProPresenter, is run too: the message is shown, with what its tokens were last given
here. One cannot be made here yet.

ProPresenter has many more kinds of action than these (capture, and others for the
features this app does not have). A slide or a macro that has one keeps it: it is shown as a
fainter icon, said for what it is, written back untouched and not done. An action for
a timer, a prop, a macro, a layout, a look or a clear group that is not in the
workspace does nothing, and the [log](log.md) says so; so does one that plays a sound
file that is not on this computer.

A stage action names the stage screens it is for, and its panel has a line for each
stage screen of the workspace: the layout that screen is to be given, or that it is to
be left as it is. An action made here names every stage screen, as ProPresenter's do.
One that names no screen of this workspace at all (it was made where the screens were
others) is taken to be for the first stage screen.
