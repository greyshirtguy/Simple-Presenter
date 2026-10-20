[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Themes

A **theme** is a set of slides built to show how slides should look: where the words go,
in what font and colour, over what shapes and pictures. They are ProPresenter's own, kept
under the workspace's `Themes` folder, each in a folder of its name (a folder there with
no theme in it is a folder of themes).

The toolbar's **Themes** button lets them down to be browsed: folders as folders, a theme
as its first slide; a click on a theme shows its slides. A theme is used in three ways.

- **On a presentation.** With a presentation open, a click on a theme slide dresses
  every slide of the presentation in it.
- **On one slide.** A slide's menu has **Theme**, which leads through the folders and the
  themes (each by the look of its first slide) to the theme's slides.
- **On a screen, for as long as a look says** ([Looks](looks.md)), with the presentation
  left alone.

The first two change the presentation's file; a copy of it as it was is kept first,
beside the editor's backups.

**What dressing a slide does.** ProPresenter does not say how it decides, so this is
this app's reading of it, and may differ from ProPresenter's in the corners.

- The slide's text boxes that have words are matched with the theme slide's text boxes
  (the things on it with words in them, which stand for the words to come: "Verse",
  "Lyrics"; and those with no words named as one of the slide's text boxes is, such as
  a "Name" and a "Role" made to bring a name in): **by name** first, whatever the
  capitals; then **by size**, the largest into the largest, since a theme moves boxes
  about but the big one is the main one in both; and boxes much of a size **in the
  order** they come.
- A matched text box takes everything from the theme's: its name, place, size, fill,
  outline, shadow, and the one format the theme's text is in (font, size, colour,
  alignment, capitals, how it fits its box). It keeps its words, and the chords over
  them. (The name is worth having: a [stage layout](stage-layouts.md) can show only the
  text boxes of a given name, and a song whose boxes were called something else, or
  nothing, is brought into line by being dressed in a theme, however its boxes were
  matched.)
- What else the theme slide has (shapes, pictures, text boxes that get no words) comes
  with it, the text boxes empty.
- **Words are never thrown away**: a text box of the slide that has no box in the theme
  stays as it was.
- Things of the slide's own that are not words (a picture someone put on it) stay.
  Things an earlier theme brought are taken away again, so that going from one theme
  to another leaves nothing of the first. (They are known by having the ids of things
  in the workspace's themes; what a theme put on a slide in ProPresenter itself is not
  known that way, and stays.)
- The slide's background colour becomes the theme slide's. The media and the other
  actions of its cue are not its look, and are not touched.
- On a screen a look gives the theme, the slide comes on as the theme says: the theme's
  [builds](builds.md) are run there, in the theme's order, the text boxes that took the
  theme's look having the builds of the theme's boxes in place of their own. So a name
  can be wiped in and out on the stream and come on plainly in the room. A theme put on
  slides in their file leaves their builds as they were.

**Making and changing themes.** In the Themes panel, **+ New Theme** makes one, with a
slide to start from; inside a theme, **+ Slide** adds another, **Edit** opens the theme in
[the editor](editing.md), which edits its slides as it edits any (a right click on a theme
slide opens it there, or deletes it), its builds included, and **Delete Theme…**
removes it. A theme's text is
meant to be in one format: where a theme slide's text has several, the first is the one
slides are dressed in.

Not yet: making folders of themes here (ProPresenter's are shown, and a theme can be
put in one in the file manager), the pictures a theme keeps in its own folder being
found from there, and choosing a theme for a new slide.
