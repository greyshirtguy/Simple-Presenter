[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Keys

| Key | Action |
|---|---|
| Right, Space / Left | Next / previous slide |
| Down / Up | Next / previous presentation |
| F1 | Clear the first [clear group](clear-groups.md), which to begin with is Clear All |
| F2 / F3 / F4 / F5 / F6 | Clear the slide layer / media layer / props / audio / messages |
| ~, held for most of a second | [Simple View](simple-view.md) on or off |
| A letter or a digit | The hotkey of a group, if a group has been given it: goes to the first slide of that group |
| A number, then Enter | Goes to the slide with that number |
| Ctrl+E | Edit mode: the editor, for the presentation being viewed |
| Ctrl+R | The [reflow editor](reflow.md), for the presentation being viewed |
| Ctrl+Alt+B | [Bible mode](bibles.md) on or off |
| Ctrl+S | Show mode: out of the editor, whichever one is up |
| Ctrl+F | [Search](search.md) |
| Ctrl+V | Show or hide the media bin |
| Ctrl+1 / Ctrl+2 | Switch the audience screens / the stage screens on or off |
| Ctrl+, | The settings, from the editor as well |
| Esc | Close a menu |

A group's hotkey is set on the settings screen, under Groups, where each group has its
colour: click the box after the group's name and press the letter or digit. Pressed
while showing, it puts the first slide of that group on the output, from where the
group first comes up in the presentation if it comes up more than once.

The hotkeys are kept with the workspace, in its list of groups (`Configuration/Groups`),
which is where ProPresenter keeps a group's hotkey beside its name and its colour. So a
workspace that ProPresenter has used comes with ProPresenter's own: **A** for the first
verse, **S** for the second, **C** for the chorus, **B** for the bridge and so on. Every
hotkey in that list works, whether or not its group is one of those on the settings
screen, and the settings screen says which others there are. A hotkey changed on the
settings screen is written into the list, and nothing else in the file is touched. A
workspace with no list of groups goes by **C** for the chorus, **V** for the verse
(which is "Verse 1", where the verses are numbered) and **B** for the bridge until a
hotkey is changed, which makes the list, of the groups on the settings screen.

ProPresenter has a second file, `Configuration/KeyMappings`, for the key mappings a
user has made beyond the ones it starts with. That file is only read: a plain letter
or digit mapped to a group there works as a hotkey too. Nothing is written to it. The
groups' names and colours on the settings screen are the app's own, and the same in
every workspace.

The slide a key goes to has the key in a small orange icon at its top left corner,
beside the icon for its media if it has any. How solid these icons are is set on the
settings screen, under Slides, from nearly gone (5%) to solid; they start at 80%, so
that the slide shows through them a little. That setting is the app's own.

**A slide by its number.** The slides wear their numbers in the grid, from 1. Typing
a digit while showing takes nothing live: it puts the number up over the slides, in a
panel that lets them show through. More digits go on with it, Enter takes the slide
with that number live, as a click on it would, and the panel fades away. A number left
for a second without a key goes away by itself, with nothing done.

That much is as it was asked for. The rest is this app's own, and a guess at what is
wanted: Esc gives a number up at once, and Backspace takes its last digit off; any other
key (an arrow, say) gives it up and then does what it always does; a number no slide
has, or one of more than four digits, does nothing; and a digit that is a group's hotkey
stays that hotkey and does not begin a number, though it is a digit like any other once
a number has been begun. ProPresenter's own hotkeys are all letters, so this only comes
up where someone has given a group a digit.

In the editor:

| Key | Action |
|---|---|
| Arrows | Move the picked element by one unit, or by ten with Shift; with nothing picked, change slide |
| Page Up / Page Down | Previous / next slide |
| Enter | Edit the picked element's text |
| Esc | Finish editing text; then, let go of the picked element |
| Tab / Shift+Tab | Pick the next / previous element |
| Delete, Backspace | Delete the picked element |
| Ctrl+D | Duplicate the picked element |
| Ctrl+B / Ctrl+I / Ctrl+U | Bold / italic / underline |
| Ctrl+V, Ctrl+Shift+V | Paste the words, in the format of the text where they go |
| Ctrl+Z / Ctrl+Shift+Z, Ctrl+Y | Undo / redo |
| Shift while dragging | Move in a straight line; resize from a corner in proportion |
| Ctrl while dragging | No snapping |
| F1 / F2 / F3 / F4 / F5 / F6 | The clears, as when showing |

The chord editor has keys of its own, which are listed with it: [Chords](chords.md).
