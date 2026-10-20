[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Workspaces

Everything the app shows comes from a workspace: one folder holding the libraries, media
and playlists for a setup. Workspaces sit side by side in
`~/Documents/SimplePresenter/WorkSpaces`, and the picker at the left of the toolbar
switches between them, clearing the output and reloading everything from the one chosen.
The app opens the workspace used last, and remembers what was selected in each.

A workspace is laid out the way ProPresenter lays out its own folder:

```
Libraries/<library name>/*.pro    presentations, one flat folder per library
Media/...                         images and videos, in any depth of folders
Playlists/Library                 playlists and playlist folders, in ProPresenter's format
Playlists/Media                   media playlists and their folders, in ProPresenter's format
Themes/<theme name>/Theme         the themes, each in a folder of its name
Configuration/Timers              the timers
Configuration/Props               the props and their collections
Configuration/Stage               the stage layouts
Configuration/Macros              the macros and their collections
Configuration/Groups              the groups, with their colours and their hotkeys
Configuration/Workspace           the screens, and the looks
```

All of it is in ProPresenter's own formats. So a copy of a ProPresenter folder, dropped
into `WorkSpaces`, is a workspace. It will hold more than this (presets, the rest of its
configuration), which the app leaves alone. Changes made in the app (a new playlist, a
presentation added to one, an arrangement chosen, media dropped on a slide, a timer set,
a prop made, a chord put on a song) are written to the files in the workspace.

Files are found by their path relative to the workspace first, so a workspace keeps
working when it is moved or copied from another machine; then by the path recorded for
them; then, for media, by name anywhere under `Media`.

The media bin shows the media playlists, as ProPresenter's does, not the folders on
disk. The first time the app opens a workspace with no media playlists file, it writes
one that mirrors the folders under `Media`: a playlist for each folder of media.

An exported playlist is imported from the "+" beside Playlists. Its presentations go
into the library last browsed and its media under `Media`, keeping the folders it had
below ProPresenter's own `Media` folder; files already there are left as they are.
The "+" beside Libraries makes a new presentation in the library that is open, makes a
new library, which is a folder, and brings a song in from a ChordPro file
([Chords](chords.md)). **New Presentation…** asks for a name first, the name being the
file's and what playlists find it by, and gives the presentation one slide with nothing
on it, which the [editor](editing.md) then puts text boxes, shapes and pictures on. A
second of the same name gets a number after it. The file starts as ProPresenter's own
presentations do, with a slide written as ProPresenter writes an empty one; but that
ProPresenter opens a presentation that was made here is a guess, and has not been
tried.

A right click on a presentation has **Rename**, which renames it in place in the list,
as a library or a playlist is renamed: the file is renamed, the name inside it is
changed to match, and every row of a playlist that is that presentation is brought
along, so that it still finds it (nothing else of a row is touched, the system it was
written on and its arrangement least of all). A name the library has already is
refused. In a library the menu also has **Delete…**, which asks first and then moves
the file to the system's Trash, where the file manager has it to put back; nothing is
ever deleted outright, and a file that cannot be moved there is left where it is. A
playlist that had it says it is missing. Renaming and deleting a presentation that is
on the output are allowed for: renamed, the show goes on from the slide it was at;
deleted, its slide is taken off the output first. In a playlist the menu has Rename and
not Delete: there, only the row is ever removed. (Whether ProPresenter offers these in
the same places, and what it does with the rows of a deleted presentation, has not
been looked at: that is a guess.)

Under the list of presentations is a box that narrows it. What is typed there leaves
the presentations whose names have every word of it, in any order, as a word or part of
one, with capitals, accents and punctuation aside, as [Search](search.md) has them. It
works on whichever is being browsed, a library or a playlist; in a playlist a heading
stays for the rows under it that are left. The box says how many are left of how many.
**Enter** opens the first of them, **Esc** or the cross empties the box, and opening
another library or playlist empties it too. Only the list is narrowed: the arrow keys
still step through every presentation, in the order the library or the playlist has
them. (ProPresenter has such a box under its list; how it matches names, and what its
keys do, is a guess here.)

`--workspace <dir>` opens a particular workspace folder, wherever it is; the folders
beside it are then the ones the picker offers. `--help` lists the other options.
