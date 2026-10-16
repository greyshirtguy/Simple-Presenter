# Tests

Two kinds, for two kinds of mistake.

## Unit tests: `tests/unit/`

The parts of the app that are plain rules are written so that they can be tried by
themselves: no window, no workspace on disk, nothing drawn. A unit test sets such a part
up from plain values, does one thing to it and looks at what came of it. They take a few
milliseconds, so they can be run after every change.

```
cmake --build build          # they are built with the app
ctest --test-dir build       # add --output-on-failure to see what failed
```

So far there are twenty-eight:

| Test | What it tries |
| --- | --- |
| `tst_lookfile` | The looks in ProPresenter's set-up file (`src/lookfile.h`): what each gives each screen, the live look as a look of its own (made from a saved one, changed by itself, saved as that one, made again), a theme known by its place whatever computer named it, the messages and the announcements switched for one screen, a screen given a mask and having it taken away, and changes that leave the rest of the file alone |
| `tst_maskfile` | The masks in ProPresenter's set-up file (`src/maskfile.h`), on a file made as ProPresenter might have left one: the masks read in the file's order, each with its slide whole; a new one, with nothing on it and a name no other has; one renamed; one removed, which takes it from every look that gave it to a screen; a workspace with no file given one for its first mask; and after each, that the screens, the looks and the rest of the file are as they were |
| `tst_chords` | Chords (`src/chords.h`): a chord's parts, changing key, the four notations, a key's own chords, what a typed chord might become, the stretch of the words a chord is written over in the file (with words, and with none), ChordPro there and back, a song read from a ChordPro file; and which sharps and flats are drawn as ♯ and ♭, held to the cases the chord editors for the Mac and Windows are held to (`tests/unit/data/signs.json`, copied from the same place as `tst_rtfstandins`'s, whose `tools/signs.py` made it) |
| `tst_sheethints` | The line of hints under the chord sheet (`src/sheethints.h`): what it says for each thing the person can be about (typing, an emptied box on a chord, carrying a chord of the sheet's or of the strip's, Ctrl held, over the strip, on a chord, on a word, nobody at the sheet), in the words the Mac and Windows chord editors have, each beginning with a capital; and how many go into a width, the last left off, but the two on copying and pasting put one under the other and made smaller instead, and how tall the line then is |
| `tst_rtfstandins` | A line of chords with no words changed where it stands in an RTF (`src/rtfstandins.h`): held to the cases the chord editors for the Mac and Windows are held to (`tests/unit/data/`, copied from [Pro7 Chord Editor](https://github.com/greyshirtguy/Pro7ChordEditor)'s `testdata/vectors`, whose `tools/standins.py` made them and is the rule's reference), so that all three write the same bytes; and its reader of RTF to what the app's own reader makes of the same samples |
| `tst_thememath` | Which text box of a slide goes into which text box of a theme (`src/thememath.h`): by name, by size, in order |
| `tst_searchindex` | Finding presentations by their names and words (`src/searchindex.h`): capitals, punctuation and accents, the order of what is found, the line that is shown; and a list on the screen narrowed to the names that have what was typed, a playlist's headings with it |
| `tst_ndisetup` | Reading NDI's installer (`src/ndisetup.h`): finding the licence it shows and the archive it carries, and not being fooled by something that is not it |
| `tst_decklinkformats` | What a screen sent out through a Blackmagic DeckLink card is sent as (`src/decklinkformats.h`): the formats offered, in the menu's order, each with its size, its rate and the code Blackmagic's SDK gives it; and pictures made straight from premultiplied, rounded, never past white, and a pixel with nothing on it left alone |
| `tst_screenfile` | The list of a workspace's screens in ProPresenter's set-up file (`src/screenfile.h`): what a workspace with no file has, what adding, renaming and removing a screen write, and that the rest of a file ProPresenter wrote goes back as it came |
| `tst_groupedit` | Putting slides into another group as ProPresenter does (`src/groupedit.h`): the whole group from its first slide, a split from a later one, what becomes of the arrangements, and that nothing else of the presentation is touched |
| `tst_arrangementedit` | A presentation's arrangements (`src/arrangementedit.h`), on a presentation built as ProPresenter writes one: a new one of every group once in Master's order, a copy straight after what it is a copy of, a name made free with a number, renaming and the names that are refused, removing the one selected and another, a group put in at each kind of gap, every move of every place against the list worked the long way, one coming up of a group taken out; and after each, the groups, the slides and the other arrangements as they were to the byte |
| `tst_labelfile` | Slide labels (`src/labelfile.h`): a label put on a slide's cue and taken off again, and the workspace's list of labels kept ready read, added to, changed and shortened with the rest of the file kept |
| `tst_decodingchoice` | Who decodes video (`src/decodingchoice.h`): who has the say as the app starts (the command line, the setting of three values, the environment as it was found, the guard), what Qt's variable is then set to, and what is said of it |
| `tst_decodingguard` | The guard on hardware video decoding (`src/decodingguard.h`), on folders made up to look like a system's: which graphics chip video would be decoded on, what of nouveau's firmware is looked for and where, and which computers are then held back |
| `tst_slidenumber` | A slide asked for by its number (`src/slidenumber.h`): what the digits typed so far make, what Backspace leaves of them, and which slide of a presentation a number is, or that none is |
| `tst_elementorder` | Where an element dragged up or down the editor's list of a slide's elements lands (`src/elementorder.h`): the top of the list is the front and under its last row the back, a gap just over or just under the element changes nothing, and every element of a pile let go at every gap, against the list worked the long way |
| `tst_colortriggers` | A timer's Color Triggers (`src/colortriggers.h`): which is in force as a countdown comes down (ProPresenter's own example: orange at 1:00, yellow at 0:30, red for the last ten seconds and through an overrun; and its stage layouts' one red at nothing) and as an elapsed time goes up; two at one time; and a trigger's time written as ProPresenter writes it and read back as it is typed, and what is not a time |
| `tst_linejoin` | A text box's lines made one (`src/linejoin.h`), as Single Line and Replace Line Returns draw it: a verse's raised number first and red words, each run keeping its look; the delimiter between lines in the look of the run before it, a line break within a paragraph counting as a line's end; lines trimmed and empty ones left out; the first paragraph's alignment and spacing; and a text with nothing in it |
| `tst_bible` | Bibles (`src/bible.h`, `src/biblexml.h`), on Bibles made up in the test: the 66 books' names, short forms and ids; a passage as people type it (`john 3 16`, `Jn 3:16-18`, `Matt 1 2 3`, `Ps 23`, `Gen 1:1-2:3`, `Ruth 1-2`, a list of verses, a book of one chapter) and as it is written back; the verses of a passage, across chapters, cut at a chapter's end, and what is said for a book or chapter the Bible has not; the verse after and before a passage, across chapters and books; reading each of the four kinds of XML file, with its red letters, notes and titles left out, and books found by number, name or order; and the three Bibles that come with the app (`bibles/`), read with the app's reader, their books, chapters and verses counted and nothing of their words looked at |
| `tst_bibleflow` | How a passage is flowed across slides (`src/bibleflow.h`), on made-up words with a stand-in for the text box: a passage's words with their verses and red; what each place between two words costs; Fill as ProPresenter fills; Even making as many slides as Fill, never a scrap at the end, breaking at a full stop or a comma near the middle rather than between two words, not leaving a word or two of a phrase alone, and breaking a phrase longer than a slide; a word wider than the box on a slide of its own; Break on New Verse; and slides kept as they were, at the start, the end or the middle of a passage that has grown, the words either side flowed each by itself |
| `tst_apibible` | api.bible (`src/apibible.h`), on answers made in the test in api.bible's shape: the list of Bibles; the ids a passage is asked by; a passage's verses read from its JSON, the words of Jesus in red and notes left out; the verses wanted taken from whole chapters; what is said for each kind of refusal; and what FUMS is told |
| `tst_reflow` | The reflow editor's rules for a slide's words (`src/reflow.h`), on texts made in the test: where a slide is split for a caret, with the spaces around it going to neither slide and a line of chords alone not taken for space; a stretch taken with its look, a break taken with a line on either side of it; two texts joined; words typed over, a line break typed, everything deleted; and the chords of a split slide, of two joined, and of a split joined again |
| `tst_clearfile` | Clear groups (`src/clearfile.h`): the group a workspace with no file is taken to have, written as ProPresenter's own is; a file of ProPresenter's read; a layer taken out and put back where it was; a group made, renamed, moved and removed; its picture, its tint and whether its button is shown; and that the rest of the file, and what is not known of in it, is kept |
| `tst_showstate` | The rules of the show (`src/showstate.h`): what a slide going live puts on the output and in what order, when the media it brings is started and when what is playing is left to, what a slide's actions do, what clearing leaves, what a clear group clears, how props stack and give way, what a macro does, where a step takes the show, how the show follows a presentation that is read again, and one that is renamed under it; and the audio layer: music one at a time, sound effects over it and each other, what follows a piece that has ended, and what clears which; the messages: several at once, one that goes by itself after its time, what clears them, and an action that shows one; and what the stage is told; the announcements: a presentation sent there goes to their layer, with media of its own under its slide, and the show stays where it was, what clears them, and stepping from where they are; and going on by itself: a slide's Go To Next Timer and its presentation's Slide Show, which of them wins, what stops it, the show and the announcements each by their own time, and a presentation that would never stop being stopped |
| `tst_audiocue` | How a piece of audio plays (`src/audiocue.h`), on a row of the audio bin made as ProPresenter writes one: whether it stops, goes round or is followed by the next, whether it is music or a sound effect, how loud and how long; each of those changed, with a look at what else of the row is still as it was, to the byte; and what a new row is given |
| `tst_audiolevel` | The arithmetic of sound (`src/audiolevel.h`): the loudest sample of each channel of a stretch of sound, however its samples are written; how far up a meter that comes, by decibels, and how the meter rises and falls; and that two pieces crossing are together as loud all the way as either is alone |
| `tst_clocktext` | How a date and a time are written for a text box linked to the system clock (`src/clocktext.h`): each of the five ways ProPresenter names for each, the twelve and the twenty-four hour clock, both together, in the words of the locale it is given, and where in a pattern the clock goes |
| `tst_messagefile` | Messages (`src/messagefile.h`), on a file made as ProPresenter wrote the one that was looked at: a message read; a text as its words and its tokens in order; a message made, named, moved and removed; a text with tokens of each kind written as the file keeps them, a token the message has left as it is and one no longer in the text taken out; what a token is given; its theme; how it goes away; and after each, what else of the file is still as it was, to the byte |

**Adding one.** A part can be unit tested if it compiles without the windows: it takes
what it needs as values and says what is to happen as values, where the code round it
would reach for a window or a file. `src/showstate.h` says how that is done for the
show: the rules change a small struct and return a list of what is then to be done
outside, and the object QML talks to (`src/show.h`) carries the list out. Put the test
in `tests/unit/`, give `tests/unit/CMakeLists.txt` an executable for it built from the
part's sources alone, and add it with `add_test`.

## Scripted tests: `tests/ui/`

These work the whole app as someone would. Each is a QML script (`tests/ui/t/<name>.qml`)
that the app loads into its operator window and runs: it clicks, drags and types with
`testInput`, looks at what the windows then hold and show, and prints a line for each
thing it checks, `ok` or `FAILED`.

```
cmake -B build-tests -G Ninja -DSIMPLEPRESENTER_TEST_HOOK=ON
cmake --build build-tests
tests/ui/run.py                  # all of them: about a quarter of an hour
tests/ui/run.py props stage      # or some
tests/ui/run.py --list
```

**The door.** A script gets into the app by `--script <file>`, which is only there in a
build made with `-DSIMPLEPRESENTER_TEST_HOOK=ON` (`src/testhook.h`). The build that is
used and the one that is packaged do not have it; hence a build folder of its own.

**Nothing of a run touches the real desktop, settings or workspaces.**

- Every test gets a fresh copy of the workspace it runs on, fresh settings and a
  documents folder of its own, in a folder made for the run under the system's
  temporary folder.
- Most tests need the windows really drawn, by the graphics card, with a pointer and a
  keyboard that behave as real ones do. They get a desktop of their own: a GNOME Shell
  with no screen, on a session bus of its own, started for the run and stopped after
  it. Nothing of it shows anywhere. (The few that can do without are drawn into memory.)
- Sound goes into a private audio service with one sink that plays to nowhere
  (`tests/ui/audio/`): the real one and the sound card are not touched.

It needs `gnome-shell`, `pipewire`, `wireplumber` and `protoc` (which the build needs
anyway), all of which Ubuntu's desktop has.

One test, `screens`, has a desktop to itself with two displays, to send screens to. It
also sends a screen over NDI if NDI's library is there to be found (the main README says
where the app looks; `NDI_RUNTIME_DIR_V6=<folder> tests/ui/run.py screens` points it at
one), and checks what the app says without it if not. While it runs with the library, a
source called "Test Screen" is on the local network for a few seconds.

Another, `ndisetup`, tries getting NDI's library through the app: the panel that comes
up, NDI's licence shown and declined and agreed to, and the screen going on the network
once the library is in place. It does not download anything: it is pointed at a copy
of NDI's installer kept on this computer (`third_party/ndi-sdk/download/`, which is not
in the repository), and where there is no such copy it tries what the app says when
the download fails instead.

And `decklink` sends a screen out through a Blackmagic DeckLink card's output, with no
card: with `SP_TEST_DECKLINK` set (which `run.py` does for it), the app pretends two
outputs, one that can key as a linked pair of connectors can, and one that cannot, and
keeps each picture it would have handed the card for the test to look at. Everything
but the handing over is the app's own: the menu, the formats, the alpha settings, the
drawing, the reading back, starting and stopping. Whether a real card takes what it is
given is not tested here.

**What a run leaves.** A line for each test, and its failures:

```
props       60 ok, 0 failed, 0 QML errors
```

The run's folder has every test's full output (`logs/<name>.log`) and the pictures it
took (`frames/<name>/`). It is removed if every test passed, and kept, with its path
printed, if any did not or if `--keep` was given.

**The workspaces the tests run on are not in the repository.** They are in
`tests/ui/fixtures/`, which git is told to leave alone, because they are snapshots of
real workspaces: the presentations are songs and services, and their words are not this
repository's to publish. The scripts go by what is in them (the names of presentations,
the number of slides, the colour of a macro), so they cannot simply be pointed at any
other workspace. On a computer without that folder the scripted tests say so and stop;
the unit tests run anywhere.

What the folder holds:

| In `tests/ui/fixtures/` | What it is |
| --- | --- |
| `workspaces/Demo` | Presentations and playlists; its `Media` is a link to a real media folder, which is only read |
| `workspaces/ProPresenter MR` | Presentations, timers, props, stage layouts and playlists as ProPresenter itself wrote them |
| `workspaces/Act` | The same presentations with ProPresenter's macros, groups, its own account of the workspace (screens and looks) and its themes |
| `workspaces/Shapes` | One presentation and two pictures, for the editor |
| `songs/` | Two songs with chords, as Multitracks and ProPresenter wrote them, which the five `chords` tests copy into their workspaces |

The `bible` and `bible2` tests need nothing more of them (`bible` also finds a verse in the KJV that comes with the
app, and looks only at whether it was found): `run.py` writes a Bible of its
own making into the test's copy of the workspace (every verse is "Made up words of verse
…"), and for `bible` plays api.bible itself, on a port of this computer, with a made-up
key, a Bible of the same kind and FUMS. No Bible's words are in the tests, and the tests
ask nothing of the internet. `bible2` uses a theme the Act workspace has, whose Bible
slide's verse box is Single Line, which its slides keep.
| `made/` | Small videos and pictures made for the tests: one keyframe in twenty seconds, dark until nine seconds in, VP9, HEVC, one with sound |

**A trap in them: a playlist's row can lead out of the test.** A row of a playlist says
where its presentation is twice over, by the path from the workspace's folder and by
the file's whole address on the computer the row was made on, and a row is found by the
first of those that is there. The fixtures' rows were made on the maintainer's
computer, in a real workspace. So for as long as a test's copy of a presentation is
where the row says, the row finds the copy; but **a test that deletes or renames the
copy sends the row to the real file**, and whatever the test then does to that row it
does to somebody's song. A test that takes presentations away or renames them has to
give its workspace rows that lead nowhere real first: `prepare_library` in `run.py`
rewrites every address as ProPresenter writes one on a Mac.

**Writing one.** Start from a short one (`t/rapid.qml`). A script is a `QtObject` with a
list of steps; each step does something and says how many milliseconds to wait before
the next, so that the app can draw in between. Where the script has `//COMMON`, the
runner puts in the helpers every script shares (`t/common.txt`: `check()`, finding an
item by its `objectName`, clicking it); `t/lib.js` has more. Because the script is
loaded in the operator window's own scope, it calls the window's functions and reads its
properties by name. Then give it a line in the `TESTS` table of `run.py`, which says the
workspace it runs on and whether it has to be really drawn.

**Pictures.** The pictures of the chord editors in the main README were taken this way
and not by hand: a script that sets a scene and calls `testInput.grab("name")`, run with
`--keep`, leaves `frames/<test>/name.png` in the run's folder. For pictures at twice the
size, as a high-density display would show the app, give the test's line in `TESTS`
`env={"QT_SCALE_FACTOR": "2"}` and the test desktop a display large enough for the
window, with `SP_TEST_MONITOR=3840x2160 tests/ui/run.py <test> --keep`. Such a script
is not a test, and is not kept here.

## And two more, in the app itself

`SimplePresenter --selftest <dir>` saves pictures of a fixed run for comparing before
and after a change to anything that draws, and `benchmark/run.py` times a fixed run
against a baseline. The main README has both.
