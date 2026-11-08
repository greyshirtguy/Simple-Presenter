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
below ProPresenter's own `Media` folder. Where some of its presentations are in the
library already, or a playlist here has its playlist's name, it asks first: **Replace
Them** puts the archive's presentations in place of the library's (each old file kept
first among the editor's backups, in the app's data folder) and its playlist in place of
the one of that name, where that one is; **Keep What Is Here** leaves them as they are
and adds the playlist beside the old one. Media files already there are always left as
they are, a file of the same name being taken for the same file. (A guess at what
ProPresenter asks here.)
The "+" beside Libraries makes a new presentation in the library that is open, makes a
new library, which is a folder, brings a song in from a ChordPro file
([Chords](chords.md)), and brings a song's words in from SongSelect (below).
**New Presentation…** asks for a name first, the name being the
file's and what playlists find it by, and gives the presentation one slide with nothing
on it, which the [editor](editing.md) then puts text boxes, shapes and pictures on. A
second of the same name gets a number after it. The file starts as ProPresenter's own
presentations do, with a slide written as ProPresenter writes an empty one; but that
ProPresenter opens a presentation that was made here is a guess, and has not been
tried.

### A song's words from SongSelect

CCLI's SongSelect lets a church download a song's words as a plain text file (the
"lyrics" download, `<Title>-lyrics.txt`): the title on the first line, each part's
words under its label (Verse 1, Chorus, Bridge), and at the foot the authors, the CCLI
song number, the copyright line with the publishers, and the church's licence number.
**Import Lyrics File…** takes such a file, and **Import Lyrics from Clipboard** the
same words copied from SongSelect's page, and puts up a panel that says what was found
(the title, the authors, the parts, the number) and asks:

- **Lines on a slide**, two unless changed, every time; the panel says how many slides
  that makes.
- **Theme**: a theme slide to dress every slide in as it comes in, from the workspace's
  themes as a slide's Theme menu has them, or None. The slides are dressed before the
  file is first written, so there is no earlier version of it to keep.
- **Library**: which library the song goes into.
- **Add to playlist**: a playlist to add it to the end of, or None.

The theme, the library and the playlist start as they were for the last song brought
into this workspace (this computer's settings), the library as the one that is open
until then; so a run of songs for a service comes in with a few clicks each. The song
is then opened in its library. Each labelled part is a group of that name; a block of
words with no label is a group with no name (a verse, no doubt, but naming it would be
a guess). The song's details go where ProPresenter keeps a song's from SongSelect, for
the [copyright display](copyright.md): the title, the authors on one line, the
publishers one a line, the year, and the song number, with the display on, as
ProPresenter's own files from SongSelect have them; the church's licence number is
the workspace's, not the song's, and is left out. A song with none of these, pasted
words say, has only its title there, and the display off.

Guesses: the labels a block may start with are the usual names of a song's parts, and
a label with nothing under it is left out; the year goes in the year field alone and
not at the start of the publisher as some of ProPresenter's own files from SongSelect
have it (the copyright display has a place of its own for the year); and whether
ProPresenter's own SongSelect window asks the same things has not been looked at. The
controls are as the maintainer asked for them.

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

### Rows of a playlist

A click on a row of a playlist opens its presentation, and that row is then the one
selected. Several can be selected: a click with **Ctrl** held selects a row as well as
those that are, or lets it go, and one with **Shift** held every row from the last one
selected to it, headings between them too; neither opens anything, so what is open
stays open. A heading can be selected by itself, as a row. **Delete** takes the
selected rows out of the playlist, without asking (a row is only a row: the
presentation stays in its library), and the menu's Remove does the same for all of
them; the row that took their place is then the one selected. [Search](search.md) adds
a presentation under the selected row. (How ProPresenter selects rows, and what it
shows after Delete, has not been looked at: a guess.)

Rows are copied and pasted as well: **Ctrl+C** copies the selected rows and **Ctrl+V**
pastes them under the row selected, while the rows are what was last clicked (a click
on a slide, or Esc, gives Ctrl+V back to the media bin); a row's menu has Copy and
Paste too, the paste going under that row. A pasted row is the row it was copied from,
its arrangement and all, and the copies can go into another playlist: open it from the
list, and Ctrl+V puts them under the row selected there, which is the presentation
opened with the playlist until a row is clicked. The copy is the app's own, not the
system clipboard's.

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
