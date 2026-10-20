[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Looks

A **look** says which layers of the show each audience screen gets. The room can have
the slides over the media while a stream has the slides alone, to lie over a camera's
picture; a lobby screen can have the props and nothing else. A workspace has as many
looks saved as are wanted, by name. They are ProPresenter's own, and work as its do.

**The live look.** What the screens are showing is a look of its own, the *live look*,
apart from the saved ones. Making a saved look live copies it into the live look, which
takes its name. After that the two go their own ways: the live look can be changed by
itself (a layer switched off for this service only) without the saved look knowing, and
a saved look can be changed without the screens changing until it is made live again.
The toolbar's **Looks** button has the live look's name under it.

A saved look is made live in three ways: from the menu a click on the **Looks** button
gives, where the look the live one came from is ticked; with **Make Live** in the Looks
window; or by an **Audience Look** action on a slide or in a macro, which is how a
service moves from one to another by itself. Making the same look live again puts the
live look back as that look has it. When the look changes, each layer comes or goes
over the time the look says. (An action that goes over to the look that is live
already, and unchanged, costs nothing; one that changes the look is on the screens at
once, and in the file a moment after, so that it never holds a slide up.)

> [!NOTE]
> That an action puts the live look back is worth knowing before changing the live
> look by hand. Songs whose first slide runs a macro that goes over to a look will undo
> a change made to the live look as soon as the next song starts; what was meant to
> last should be saved (below). The log says when a change went this way.

The preview at the top right of the operator window shows one of the screens, the first
audience screen unless another is picked under it (see [Screens](screens.md)). Of an
audience screen it shows what the live look gives that screen: a layer the look keeps
from it is not in the preview either, and its slide is in that screen's theme.

**The Looks window** (the Looks button, then **Edit Looks…**; its ✕, Esc or a click
outside it puts it away) is laid out as ProPresenter's is. On the left are the looks: the live look first, and under it
the saved ones, with a green dot beside the one the live look was made from. On the
right is what the look that is picked gives each audience screen, as a table: the
screens across the top, and the layers down the side in the order they lie on a screen,
the top one first.

| Row | What it is |
| --- | --- |
| Masks | Not a tick box but which [mask](masks.md) the screen is given: a circle with a line through it for none, or the mask's name. A click gives the masks to choose from, and the way to the editor they are made in |
| Messages, Props, Announcements, Slide, Media | A tick box for each screen: whether that screen gets the layer |
| Video Input | ProPresenter's layer that this app has not yet. It is shown greyed, as the look has it in the file, cannot be changed here, and is kept as it is |
| Presentation | Not a layer but the **theme** that screen's slides are dressed in as they are shown: a circle with a line through it for none, or a theme slide, chosen from the same menu of themes a slide's own menu has |

Picking a look in the list shows it, to be read and changed; it does not make it live.
At the top right is what is done with the look that is picked. For a saved look that is
**Make Live**. The live look is live already; a change to it is seen on the screens at
once, the list then says it has been "changed" (it is no longer what the look it was
made from is), and in the same place there is **Save**, which keeps the live look as
the saved look it was made from. The **+** makes a new saved look as a copy of the one
that is picked, so that picking the live look and pressing it keeps the live look, as
it has been changed, as a saved look of its own instead. A double click on a saved look renames it, and a right click offers Make
Live, Rename, Duplicate and Delete. The live look cannot be removed: there is always
something the screens are showing.

**A theme for a screen.** A look can give a screen a theme slide, and every slide shown
on that screen is then dressed in it as it is shown: the same words, large and central
in the room and a line across the foot of the stream. Nothing about the presentation is
changed, and the next look can dress it another way.

The looks, and the live look with them, are the workspace's, the ones ProPresenter has
for it, and are read from and written to its file of how the workspace is set up
(`Configuration/Workspace`), with the screens. So a workspace opened here shows what it
was showing in ProPresenter, and one taken back there shows what it was showing here.
