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

So far there are forty-two:

| Test | What it tries |
| --- | --- |
| `tst_lookfile` | The looks in ProPresenter's set-up file (`src/lookfile.h`): what each gives each screen, the live look as a look of its own (made from a saved one, changed by itself, saved as that one, made again), a theme known by its place whatever computer named it, the messages and the announcements switched for one screen, a screen given a mask and having it taken away, the video input layer switched for a screen, and changes that leave the rest of the file alone |
| `tst_videoinputfile` | The workspace's video inputs in ProPresenter's set-up file (`src/videoinputfile.h`): the list read as ProPresenter writes it; an input added last, renamed and removed, with all else in the file kept, and a workspace with no such file given one; ProPresenter's Video Input Action read, one made as it writes one, and one there already given another input with the rest kept; a shape's fill as an input by its place; the mode Automatic takes, the largest picture up to 1920 by 1080 at its fastest; and a device's own sound found among the audio inputs by its name |
| `tst_maskfile` | The masks in ProPresenter's set-up file (`src/maskfile.h`), on a file made as ProPresenter might have left one: the masks read in the file's order, each with its slide whole; a new one, with nothing on it and a name no other has; one renamed; one removed, which takes it from every look that gave it to a screen; a workspace with no file given one for its first mask; and after each, that the screens, the looks and the rest of the file are as they were |
| `tst_chords` | Chords (`src/chords.h`): a chord's parts, changing key, the four notations, a key's own chords, what a typed chord might become, the stretch of the words a chord is written over in the file (with words, and with none), ChordPro there and back, a song read from a ChordPro file; and which sharps and flats are drawn as ♯ and ♭, held to the cases the chord editors for the Mac and Windows are held to (`tests/unit/data/signs.json`, copied from the same place as `tst_rtfstandins`'s, whose `tools/signs.py` made it) |
| `tst_sheethints` | The line of hints under the chord sheet (`src/sheethints.h`): what it says for each thing the person can be about (typing, an emptied box on a chord, carrying a chord of the sheet's or of the strip's, Ctrl held, over the strip, on a chord, on a word, nobody at the sheet), in the words the Mac and Windows chord editors have, each beginning with a capital; and how many go into a width, the last left off, but the two on copying and pasting put one under the other and made smaller instead, and how tall the line then is |
| `tst_rtfstandins` | A line of chords with no words changed where it stands in an RTF (`src/rtfstandins.h`): held to the cases the chord editors for the Mac and Windows are held to (`tests/unit/data/`, copied from [Pro7 Chord Editor](https://github.com/greyshirtguy/Pro7ChordEditor)'s `testdata/vectors`, whose `tools/standins.py` made them and is the rule's reference), so that all three write the same bytes; and its reader of RTF to what the app's own reader makes of the same samples |
| `tst_thememath` | Which text box of a slide goes into which text box of a theme (`src/thememath.h`): by name, by size, in order |
| `tst_searchindex` | Finding presentations by their names and words (`src/searchindex.h`): capitals, punctuation and accents, the order of what is found, the line that is shown; and a list on the screen narrowed to the names that have what was typed, a playlist's headings with it |
| `tst_ndisetup` | Reading NDI's installer (`src/ndisetup.h`): finding the licence it shows and the archive it carries, and not being fooled by something that is not it |
| `tst_decklinkformats` | What a screen sent out through a Blackmagic DeckLink card is sent as (`src/decklinkformats.h`): the formats offered, in the menu's order, each with its size, its rate and the code Blackmagic's SDK gives it; pictures made straight from premultiplied, rounded, never past white, and a pixel with nothing on it left alone; and the beat the screens are drawn on together: when a pass is due, which screens it takes, when each is next, and, played through a second of a transition, three outputs of one format drawn in every pass together, one that fell out of step back with them at the next, and outputs of two rates each drawn no oftener than its own frames come |
| `tst_httpapi` | The rules of the HTTP API (`src/httpapi.h`): requests read as they come, whole or in pieces, with their query and body, and what is not HTTP refused; responses, JSON with CR LF CR LF after it, and chunks written as ProPresenter writes them; paths matched to ProPresenter's routes, words before parts; what `status/updates` takes, every stream Companion's module asks for among it; an id found by UUID, name or index in ProPresenter's order |
| `tst_mediasettings` | What a piece of media's Media Inspector sets (`src/mediasettings.h`, `src/mediaeffects.h`), on media laid out as ProPresenter writes it with fields the app does not know: read as the inspector shows it; a change that changes the fields it names and nothing else, byte for byte; a value that cannot be changing nothing at all; trimming, and an out point at the end being none; effects kept by their ids, added from the catalogue and taken off, and added and taken off again leaving the media as it was; what a new effect is and what its shader is given; ProPresenter's forty, each once; another file keeping what still makes sense |
| `tst_httpapijson` | What the HTTP API's answers are made of (`src/httpapijson.h`): ids and colours; the slides now and next; where the show is, and nowhere; times as ProPresenter writes them; how a timer stands, and timers of each kind; a look for each audience screen; props and messages with their tokens; clear groups and macros in ProPresenter's words; libraries' UUIDs made from their names; playlist trees, and the media and audio ones |
| `tst_screenfile` | The list of a workspace's screens in ProPresenter's set-up file (`src/screenfile.h`): what a workspace with no file has, what adding, renaming and removing a screen write, and that the rest of a file ProPresenter wrote goes back as it came |
| `tst_groupedit` | Putting slides into another group as ProPresenter does (`src/groupedit.h`): the whole group from its first slide, a split from a later one, what becomes of the arrangements, and that nothing else of the presentation is touched |
| `tst_arrangementedit` | A presentation's arrangements (`src/arrangementedit.h`), on a presentation built as ProPresenter writes one: a new one of every group once in Master's order, a copy straight after what it is a copy of, a name made free with a number, renaming and the names that are refused, removing the one selected and another, a group put in at each kind of gap, every move of every place against the list worked the long way, one coming up of a group taken out; and after each, the groups, the slides and the other arrangements as they were to the byte |
| `tst_labelfile` | Slide labels (`src/labelfile.h`): a label put on a slide's cue and taken off again, and the workspace's list of labels kept ready read, added to, changed and shortened with the rest of the file kept |
| `tst_decodingchoice` | Who decodes video (`src/decodingchoice.h`): who has the say as the app starts (the command line, the setting of three values, the environment as it was found, the guard), what Qt's variable is then set to, and what is said of it |
| `tst_decodingguard` | The guard on hardware video decoding (`src/decodingguard.h`), on folders made up to look like a system's: which graphics chip video would be decoded on, what of nouveau's firmware is looked for and where, and which computers are then held back |
| `tst_slidenumber` | A slide asked for by its number (`src/slidenumber.h`): what the digits typed so far make, what Backspace leaves of them, and which slide of a presentation a number is, or that none is |
| `tst_builds` | ProPresenter's builds (`src/builds.h`) and its transitions as its files know them (`src/transitionkinds.h`), on a lower third laid out as ProPresenter wrote one, with fields the app does not know: the builds read in the file's order, and those it does not name after them; when each runs, With Slide, With Previous and After Previous with their delays, and the steps On Click makes; a build added as ProPresenter makes one, taken off, changed (its start, its delay, its direction, another transition altogether, and put back as ProPresenter has it to begin with), and moved in the order, with nothing else of the slide changed, byte for byte, and moved back to the slide as it came; the slide's own transition set and taken off again, leaving the slide as it came; what cannot be, changing nothing; With Previous timed from when the build before was set going, before its delay (ProPresenter's lower third: the underline 0.2 s after the name); a slide dressed in a theme for a screen taking the theme's builds and its order; ProPresenter's forty-three, each once |
| `tst_communications` | Communication devices (`src/communications.h`): ProPresenter's list of devices read as connections from a file laid out as ProPresenter writes one (JSON, each device's parser XML in UTF-16 in base 64), its MIDI and RossTalk devices with their ids, names, ports and Auto Reconnect, and a kind this app has not counted; this app's own file, a list for each workspace, each kept when another is written, and the MIDI Map beside them, which leaves nothing of itself when it is ProPresenter's own again; what is wrong with a device; a command of words from its format and its values' ranges; a MIDI note's bytes; an HTTP request's address, headers and body; a MIDI Note On and a Custom Control made byte for byte as ProPresenter wrote them, read back, and changed with what else the action had kept; an HTTP request in ProPresenter's general form, with nothing in it ProPresenter does not define; ProPresenter's MIDI Map, its 33 notes, a note to its command, Auto Fill, and notes' names |
| `tst_playlistactions` | The actions that play a playlist of the media bin or of the audio bin (`src/playlistactions.h`): ProPresenter's Audio Bin Playlist read as ProPresenter wrote one (the playlist by id and name and nothing else), and one with all the file allows (an item, Select Playlist, Always Retrigger); another kind with a playlist in it not taken for one; a new one made as ProPresenter makes its own, byte for byte but for the type; Always Retrigger changed with the item, Select Playlist, the action's label and delay and a field this app knows nothing of all kept; another playlist letting the item go; and one with no playlist not made |
| `tst_elementorder` | Where an element dragged up or down the editor's list of a slide's elements lands (`src/elementorder.h`): the top of the list is the front and under its last row the back, a gap just over or just under the element changes nothing, and every element of a pile let go at every gap, against the list worked the long way |
| `tst_colortriggers` | A timer's Color Triggers (`src/colortriggers.h`): which is in force as a countdown comes down (ProPresenter's own example: orange at 1:00, yellow at 0:30, red for the last ten seconds and through an overrun; and its stage layouts' one red at nothing) and as an elapsed time goes up; two at one time; and a trigger's time written as ProPresenter writes it and read back as it is typed, and what is not a time |
| `tst_linejoin` | A text box's lines made one (`src/linejoin.h`), as Single Line and Replace Line Returns draw it: a verse's raised number first and red words, each run keeping its look; the delimiter between lines in the look of the run before it, a line break within a paragraph counting as a line's end; lines trimmed and empty ones left out; the first paragraph's alignment and spacing; and a text with nothing in it |
| `tst_bible` | Bibles (`src/bible.h`, `src/biblexml.h`), on Bibles made up in the test: the 66 books' names, short forms and ids; a passage as people type it (`john 3 16`, `Jn 3:16-18`, `Matt 1 2 3`, `Ps 23`, `Gen 1:1-2:3`, `Ruth 1-2`, a list of verses, a book of one chapter) and as it is written back; the verses of a passage, across chapters, cut at a chapter's end, and what is said for a book or chapter the Bible has not; the verse after and before a passage, across chapters and books; reading each of the four kinds of XML file, with its red letters, notes and titles left out, and books found by number, name or order; and the three Bibles that come with the app (`bibles/`), read with the app's reader, their books, chapters and verses counted and nothing of their words looked at |
| `tst_bibleflow` | How a passage is flowed across slides (`src/bibleflow.h`), on made-up words with a stand-in for the text box: a passage's words with their verses and red; what each place between two words costs; Fill as ProPresenter fills; Even making as many slides as Fill, never a scrap at the end, breaking at a full stop or a comma near the middle rather than between two words, not leaving a word or two of a phrase alone, and breaking a phrase longer than a slide; a word wider than the box on a slide of its own; Break on New Verse; and slides kept as they were, at the start, the end or the middle of a passage that has grown, the words either side flowed each by itself |
| `tst_apibible` | api.bible (`src/apibible.h`), on answers made in the test in api.bible's shape: the list of Bibles; the ids a passage is asked by; a passage's verses read from its JSON, the words of Jesus in red and notes left out; the verses wanted taken from whole chapters; what is said for each kind of refusal; and what FUMS is told |
| `tst_reflow` | The reflow editor's rules for a slide's words (`src/reflow.h`), on texts made in the test: where a slide is split for a caret, with the spaces around it going to neither slide and a line of chords alone not taken for space; a stretch taken with its look, a break taken with a line on either side of it; two texts joined; words typed over, a line break typed, everything deleted; and the chords of a split slide, of two joined, and of a split joined again |
| `tst_clearfile` | Clear groups (`src/clearfile.h`): the group a workspace with no file is taken to have, written as ProPresenter's own is; a file of ProPresenter's read; a layer taken out and put back where it was; a group made, renamed, moved and removed; its picture, its tint and whether its button is shown; and that the rest of the file, and what is not known of in it, is kept |
| `tst_showstate` | The rules of the show (`src/showstate.h`): what a slide going live puts on the output and in what order, when the media it brings is started and when what is playing is left to, what a slide's actions do, what clearing leaves, what a clear group clears, how props stack and give way, what a macro does, where a step takes the show, Next and a click on the slide running its On Click builds before it goes on, a slide's communication actions handed out to be sent, how the show follows a presentation that is read again, and one that is renamed under it; and the audio layer: music one at a time, sound effects over it and each other, what follows a piece that has ended, and what clears which; the messages: several at once, one that goes by itself after its time, what clears them, and an action that shows one; and what the stage is told; the announcements: a presentation sent there goes to their layer, with media of its own under its slide, and the show stays where it was, what clears them, and stepping from where they are; and going on by itself: a slide's Go To Next Timer and its presentation's Slide Show, which of them wins, what stops it, the show and the announcements each by their own time, and a presentation that would never stop being stopped; and an action that plays a playlist of either bin: from its first row that is here, or the one it names, found by its id or its name, left to play on when the playlist is playing (whichever of its rows), started again from the first with Always Retrigger, music so played followed by the next; a message shown with the words an action gives its tokens; and a video of a media playlist that has played to its end followed as its Next Cue says (the next with a file, the first, a row it names, one at random), only where it was played from its playlist and is still the one on the layer; and disabled slides: passed over by Next and Previous, a Go To Next Timer and the announcements going round, round to the first that is not disabled, and shown when asked for by themselves |
| `tst_audiocue` | How a piece of audio plays (`src/audiocue.h`), on a row of the audio bin made as ProPresenter writes one: whether it stops, goes round or is followed by the next, whether it is music or a sound effect, how loud and how long; each of those changed, with a look at what else of the row is still as it was, to the byte; and what a new row is given |
| `tst_mediacue` | What follows a row of a media playlist when its video ends, ProPresenter's Next Cue (`src/mediacue.h`): a row as ProPresenter writes one read as followed by nothing; each kind read as the cue says it, a cue to go to by its id; anything to follow making the video stop at its end, with the rest of the row as it was; a cue to go to named and taken away again; nothing written that is as asked, or no such thing; and a picture, which has no going round to stop |
| `tst_playbackmarkers` | ProPresenter's Playback Markers (`src/playbackmarkers.h`), on a media action as ProPresenter writes one: a video or audio takes them and a picture does not; a new one's colour, at random, one its media's others have not; added in the order of their times, and taken away to the byte; renamed, recoloured and moved; their actions added, changed and taken off; a duplicate with ids of its own; and which marker a stage link means (first, previous, next, last, by name) |
| `tst_ccli` | The copyright display (`src/ccli.h`): the settings in ProPresenter's CCLI file read and changed with the rest kept; what a workspace with none has; a presentation's details; the line its details make by the layout, a detail it has not leaving its words out; and which slides show it |
| `tst_calendarfile` | ProPresenter's Calendar (`src/calendarfile.h`), on a calendar made as ProPresenter wrote the one looked at: read; each change (a name, a description, a start, the days, "ends on", the calendar on and off) changing that and nothing else, to the byte; an action written in both of the file's forms as ProPresenter writes them, another action the event had kept; events added and deleted; when events come due, once, every week, through the end of summer time on Sydney's clock, up to "ends on" and not on a day left out; how late one may still run; the window's list both ways; and dates and times written and typed |
| `tst_textscroller` | ProPresenter's Scrolling Text (`src/textscroller.h`): the two boxes of ProPresenter's that were looked at, read; each change of the editor's (the way the words go, the speed, repeating, the gap, starting off the box, the feathering, switched off) changing that and nothing else, to the byte, a field ProPresenter's descriptions do not name kept; a box switched to scrolling given ProPresenter's settings for a new one, or keeping its own; how fast a Speed is; and where the words are as they go, once and over and over |
| `tst_disabledatefile` | Disable dates (`src/disabledatefile.h`), this app's own: a file of them with keys the app does not know, read, and one that says too little left out; a date set, changed and taken out, with the rest of the file kept, its unknown keys included; a file that is not one said to be so; where the file is; and which dates have come and which is next |
| `tst_audiolevel` | The arithmetic of sound (`src/audiolevel.h`): the loudest sample of each channel of a stretch of sound, however its samples are written; how far up a meter that comes, by decibels, and how the meter rises and falls; and that two pieces crossing are together as loud all the way as either is alone |
| `tst_clocktext` | How a date and a time are written for a text box linked to the system clock (`src/clocktext.h`): each of the five ways ProPresenter names for each, the twelve and the twenty-four hour clock, both together, in the words of the locale it is given, and where in a pattern the clock goes |
| `tst_messagefile` | Messages (`src/messagefile.h`), on a file made as ProPresenter wrote the one that was looked at: a message read; a text as its words and its tokens in order; a message made, named, moved and removed; a text with tokens of each kind written as the file keeps them, a token the message has left as it is and one no longer in the text taken out; what a token is given; its theme; how it goes away; and after each, what else of the file is still as it was, to the byte; and what an action that shows a message gives its tokens, read as ProPresenter writes it (a timer token's setting among them), and words given to its text tokens with all else it says kept |

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
tests/ui/run.py                  # all of them: about half an hour
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
also sends a screen over NDI if NDI's library is there to be found ([docs/features/ndi.md](../docs/features/ndi.md) says
where the app looks; `NDI_RUNTIME_DIR_V6=<folder> tests/ui/run.py screens` points it at
one), and checks what the app says without it if not. While it runs with the library, a
source called "Test Screen" is on the local network for a few seconds. It also checks
that a screen set to a display fills that display the first time it is shown, which a
desktop that puts a new window where it likes had once stopped.

Another, `ndisetup`, tries getting NDI's library through the app: the panel that comes
up, NDI's licence shown and declined and agreed to, and the screen going on the network
once the library is in place. It does not download anything: it is pointed at a copy
of NDI's installer kept on this computer (`third_party/ndi-sdk/download/`, which is not
in the repository), and where there is no such copy it tries what the app says when
the download fails instead.

And `httpapi` switches the HTTP API on in the settings and is a client of it on the
network, as Companion would be, writing its requests and reading what comes back as
bytes: the head and body as ProPresenter writes them, a stream of several things at
once and a chunked GET, what a slide taken live through it draws on the output against
what a click on it drew, and a presentation, a timer, the stage message and the
audience screens worked through it. Switched off, it lets its clients go.

And `quickedit` puts a slide's words right with Quick Edit: the panel up from the slide's
menu, over the slide's thumbnail at about twice its size and the window behind not
darkened, with its text box being typed in, words typed where the caret was put, Ctrl+B
doing nothing, a drag not moving the box, and Esc keeping it all, with the file holding
the words in the box's own formatting and nothing else of the slide changed, while the
output keeps what it had; a click outside writing nothing; and between boxes, Delete
and the arrows doing nothing and Tab going back to typing. And `mediainspector` works
the Media Inspector on a picture of four colours, made for it, and on a short video:
each setting (flipped, turned, cropped, stretched, an effect) told by the colours at
known points of its preview, in the file at once, then on the output and in the
operator's preview when the slide is shown again, and in the thumbnails, effect and
all; every one of
the effects drawn; another file in its place; a video's trim and picture set from the
inspector's player, and the video then starting at its in point and stopping at its out
point; a row of the media bin; and an element's picture fill turned in the editor,
drawn so there, and undone. And `builds` runs ProPresenter's builds, on a presentation
of blocks of colour made for it: on the output and in the operator's preview, each
block not there before its Build In and there after it, the other way round for a Build
Out, and a wipe half there half way through, at moments worked out from the times the
slide's steps were given; the presentation's own transition and a slide's own made in
place of the one chosen; Next running a slide's On Click builds one at a time before it
goes on, and a click on the slide doing the same; the thumbnails whole throughout. Then
the editor's Build tab: a build added, given another start, a delay, another
transition and a direction, a second added, the two dragged into another order and that
undone, one taken off and the other reset, the slide's own transition taken off; and
the file then holding just that, read with `protoc`. And `themebuilds` runs a theme's
builds on the screen a look dresses in it: the maintainer's "L3rd Name - Animated",
given the output by the look that the Names Library's slides make live by their macro,
the name not there while the slide comes in, wiped in, and the whole lower third
wiped out again some five seconds later, the underline (With Previous with 5.2 s)
0.2 s after the name (After Previous with 5 s) as in ProPresenter, on the output and
in the operator's preview, from when the look made it so, and on the next slide from
the slide; and the theme's slide in the editor's Build tab, with no Build In for the
slide.

And `devices` works communication devices on a workspace given ProPresenter's own
devices file and a presentation with ProPresenter's own MIDI Note On and Custom Control:
ProPresenter's two devices offered and added in Settings → Devices, connected (the
RossTalk one to a stand-in switcher `run.py` keeps on this computer), an HTTP Requests
device added, all kept in this app's file; an HTTP request added to a slide through Add
Action → Communication. Then the show, end to end: the first slide's note sent to Midi
Through, the kernel's own MIDI loop, heard back from it, and taking the next slide live
by the MIDI Map, whose Custom Control reaches the switcher; the next slide's request
reaching a stand-in web server with its headers and its body; the MIDI Map's Auto Fill
moving the note away, after which it does nothing. ALSA's own `aseqdump` listens on
Midi Through from outside the app, and the files are read with `protoc` after. (So the
test sends notes on this computer's Midi Through: anything else listening there hears
them.)

And `playacts` works the actions that play a playlist of either bin and the one that
shows a message, on a media bin, an audio bin, two messages and a presentation of
blocks of the test's own (the pictures and the tones in the test's folder): a media
playlist from Add Action, in its folder, and given Always Retrigger from its icon's
menu; an audio playlist, a message and a media playlist dragged out of their lists onto
slides and a macro (a folder of playlists not taken); the panel for a message's tokens,
and a message with none needing nothing more. Then the show: the output and the
operator's preview showing the playlist's first picture round the slide's block, left
to play on when it is playing, started again with Always Retrigger; music from the
audio playlist followed by the next; a message shown with the action's words and then
from its tab with its own; and ProPresenter's own two actions, one finding its playlist
by name. The files are read with `protoc` after: each action as ProPresenter writes its
kind, ProPresenter's own to the byte, and the messages file untouched.

And `videoinputs` works video inputs on a workspace whose ProPresenter has one, with no
camera: with `SP_TEST_VIDEOINPUT` set (which `run.py` does for it), the app has a
pretend device more, whose picture is one colour the test changes. ProPresenter's own
input read from its file, one added and renamed in Settings → Inputs, the pretend device
chosen, its preview and a thumbnail taken of it; the media bin's Video Inputs, an input
clicked onto the layer and seen on the output and in the operator's preview, following
the device's colour, taken off one screen by a look and given back, cleared by its
button, and let go once nothing shows it; ProPresenter's HTTP API listing the inputs,
triggering one by name, saying the layer is on and clearing it; Video Input from Add
Action, an input dragged out of the bin onto a slide, the input changed from the
action's menu, the slide shown and Clear All; and the fill: Video Input chosen in the
editor, drawn live there, another input and a scale chosen, all undone to the byte, and
the slide on the output with the input in its shape, its thumbnail showing the input's
own thumbnail and holding nothing. The files are read with `protoc` after:
ProPresenter's input as it was, the added one with nothing of this computer's in it,
the actions and the fill as ProPresenter writes them, and the rest as it was.

And `transports` works the tabs over the transport and the media bin's Next Cue, on a
media bin, an audio bin and a presentation of the test's own (three videos of three
seconds and a long one, made with ffmpeg in the test's folder, and a long tone): Next
Cue's menu and its choices, a still having none; a run of the three videos on the output
and in the operator's preview, each going on as its row says (the next, a row it names,
the first), and Random; the media's tab working the long video, paused and played; a
presentation sent to the announcements with the long video under its slide, the
announcements' tab working that video and not the other; the long tone from the audio
bin, its tab working it, a sound effect over it not taking its place, paused and played
on from where it was; and with everything cleared, nothing to work. The media bin is
read with `protoc` after: each row's Next Cue as ProPresenter keeps it, and the rest as
it was.

And `markers` works playback markers, on a media bin and an audio bin of the test's own
(a video made with ffmpeg in the test's folder, a still, a tone): the Media Inspector's
Playback Markers tab for the video and none for the still; markers added at the player
and by a right click on the bar, each a colour the other has not, renamed, given a
Clear and a prop's Trigger, coloured from the menu and with Other…'s picker, duplicated,
deleted, dragged along the bar by its flag (the player following) and moved to the
player, and a flag clicked; the video played over a live slide, the
first marker clearing the slide and the second putting the prop on as playback reaches
them; the transport's Marker menu, and its flags: on the top of the handle, a click on
the bar under one still the slider's, one dragged (shown as it goes, the live video left
where it is) and kept, and run at its new time as the video plays on, its right-click
menu and Rename; the audio tab's flag; a stage layout's box linked in the
editor to the previous marker's name in its colour, empty before it and drawn red after;
and the tone's Inspector, with that tab alone, and its marker run as it plays. The bins
are read with `protoc` after.

And `copyright` works the copyright display, on a workspace with no file of it:
Settings → Copyright, ProPresenter's layout and its example, the display enabled (which
makes the file), a licence number, the first and last slides, a chip taken out and one
added; Edit Template, the editor on the template and its box moved; a presentation's
details typed in the editor's Copyright tab; and the line on the first and last slides
on the output and in the operator's preview, drawn, and on no other. The files are read
with `protoc` after.

And `calendar` works the Calendar, on a workspace with no calendar and a playlist the test
makes of two of its presentations: the toolbar's button opening its window; an event added,
named, given a time a few seconds on and a macro from the Action menu, the calendar
enabled, and the macro run at its time; a second given one presentation of the playlist,
the playlist marked in the library, and the presentation live on its first slide at its
time, on the output too; Test; the days of the week, the list each way, and "ends on"; an
event whose time passes while the calendar is off not run when it is switched on; one
deleted from its menu; the list's colours as drawn; the window put away by the button and
by its own close, and opened again; and the app quitting with it open. The file is read
with `protoc` after.

`disabled` works disabled slides, on "It Is Well" of the Demo workspace, which has a slide
ProPresenter disabled: that slide listed and faded, in the slides and the editor's list;
Next and Previous passing over it; a click showing it, and Next going on from it; Enable
Slide and Disable Slide from a slide's menu, kept in the file; a Go To Next Timer passing
over a disabled slide; and the next slide the operator is shown being the next that is
not disabled. The file is read with `protoc` after: three cues changed, and nothing else.

`scrolling` works Scrolling Text, on the same presentation, whose slides have thirteen
boxes ProPresenter made to scroll, behind the lyrics: on the output the box's words going
by one shader, at the speed its slider says, and drawn moving; going in the operator's
preview and still in the thumbnail and the editor; the editor's Text tab with
ProPresenter's settings, changed (going right, from off the box, quicker, feathered at one
end, once), shown again so, and gone once across; switched off and on again; and set
going down, its words then as wide as the box. The file is read with `protoc` after.

`disabledates` works disable dates, on "Holy Holy Holy" of the Demo workspace, with a file
of dates the runner leaves in the workspace before the app starts, one of them long past:
at open that slide disabled and the date gone from the file; then from a slide's menu,
Disable On…, the panel with the start of tomorrow, a time a few seconds on typed and said in
words, one that cannot be read, saved; the slide marked in the grid, its menu saying the
date, the file having it; at that time the slide disabled and its mark gone; a date set
and taken off with Remove Date; and a time that has passed, disabling a slide at once. The
presentation is read with `protoc` after (three cues disabled, each in that alone), and the
file of dates (none left, its own key kept).

And `decklink` sends a screen out through a Blackmagic DeckLink card's output, with no
card: with `SP_TEST_DECKLINK` set (which `run.py` does for it), the app pretends two
outputs, one that can key as a linked pair of connectors can, and one that cannot, and
keeps each picture it would have handed the card for the test to look at. Everything
but the handing over is the app's own: the menu, the formats, the alpha settings, the
drawing, the reading back, starting and stopping, and two screens at once, drawn in the
same passes and handed over together, halfway through a dissolve as much as after a
change. Whether a real card takes what it is given is not tested here.

**What a run leaves.** A line for each test, and its failures:

```
props       60 ok, 0 failed, 0 QML errors
```

A test whose app has not quit by its time limit has failed too, though every check
passed: an app that does not quit when it is told to is a fault (a window that would not
close once kept it running with no windows).

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
item by its `objectName`, clicking it, the panel it is in scrolled to it first if it is out
of sight there, as a person would); `t/lib.js` has more. Because the script is
loaded in the operator window's own scope, it calls the window's functions and reads its
properties by name. Then give it a line in the `TESTS` table of `run.py`, which says the
workspace it runs on and whether it has to be really drawn.

**Pictures.** The pictures of the chord editors and the show controls in the README and
in [docs/features](../docs/features) were taken this way and not by hand: a script that
sets a scene and calls `testInput.grab("name")`, run with `--keep`, leaves
`frames/<test>/name.png` in the run's folder. For pictures at twice the
size, as a high-density display would show the app, give the test's line in `TESTS`
`env={"QT_SCALE_FACTOR": "2"}` and the test desktop a display large enough for the
window, with `SP_TEST_MONITOR=3840x2160 tests/ui/run.py <test> --keep`. Such a script
is not a test, and is not kept here.

## And two more, in the app itself

`SimplePresenter --selftest <dir>` saves pictures of a fixed run for comparing before
and after a change to anything that draws, and `benchmark/run.py` times a fixed run
against a baseline. [docs/how-it-is-made.md](../docs/how-it-is-made.md) has both.
