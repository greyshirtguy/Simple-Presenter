[Simple Presenter](../README.md)

# How it is made

For anyone who wants to build the app, change it, or see how it was done. How to build it
is under [Installing](installing.md#building-from-source); what an AI model is told before it
works on the code is in [AGENTS.md](../AGENTS.md).

## Built for modest hardware

The machine all of this is measured on is a 2017 Dell laptop: a two-core Core i5-7300U
with Intel HD 620 graphics. On it, when each of these was last measured:

| | |
|---|---|
| Starting, to the first frame on screen | about a second |
| Sitting with a still slide on the output | next to nothing: three hundredths of a percent of one processor core, which is the app being asked once a second whether it is still answering (see [Log](features/log.md)) |
| A 4K video under lyrics, full screen at 1080p | the graphics chip a third busy (it is a quarter busy with only the desktop on screen); 5 to 7% of one processor core |
| Showing the slides of a presentation just picked | about 80 ms |
| Going into [Simple View](features/simple-view.md) | a third of a second: the panes slide off in a fifth, at sixty frames a second, and the slides fill the window a tenth later |
| Thumbnails for 112 videos, 47 of them 4K, the first time they are seen | 2.5 seconds, while the window stays responsive |
| Putting a still on the output, even one of 8000 by 4500 | read in the background; the window is not held up |
| A 1080p video under the words, with a [Screen Preview](features/stage-layouts.md#screen-preview) of the output on the stage | a quarter of one processor core, against a tenth without the Screen Preview; every frame on both screens, none late (measured by the benchmark, in a desktop of its own) |
| [Scrolling text](features/scrolling-text.md) on the output, a ticker along the foot of the slide | a quarter of one processor core, the output drawn at each of its sixty frames a second, the words moving on by the same in each; a third with a Screen Preview of the output on the stage, scrolling it too; no frame late (the benchmark) |
| Slides changed over a video of thirty frames a second, 1080p or 4K, the stage off | by a cut, a frame of the video a refresh late at about one cut in twelve (one in three before it was mended); by a dissolve, at about one in two, mostly as it begins (one or more each before); none while nothing is done. Words with a shadow are drawn through layers of Qt's that draw two frames more after a change, and a cut to them costs the video a frame. With a Screen Preview of the output on the stage, a cut costs the video a frame about one time in two (a little better than before): the output then takes nine thousandths of a second to take in a slide's change, against two without, for a reason not yet found |
| Memory, with a workspace open | about 290 MB, of which 160 MB is what Qt needs for any window; up to 450 MB while a 4K video plays |

What gets it there:

- **Nothing is drawn twice.** A slide's text is turned into a picture once, when the
  slide is shown, and from then on costs the graphics chip one rectangle. Nothing is
  drawn while nothing changes.
- **A transition costs only while it runs.** It is one shader blending two textures.
  Between transitions nothing is blended and the output is drawn directly; going
  through the blend all the time, as the app once did, kept the graphics chip twice as
  busy for the same picture.
- **Video is decoded once, by the hardware if it can.** The graphics chip decodes it
  when a driver is installed, and the preview in the operator window borrows a few of
  the output's frames a second, and a Screen Preview every one, instead of decoding the
  file again. Drawing them is then what costs: a window that shows a moving video draws
  thirty frames a second, whatever it borrows them from.
- **Media files are never read on the thread that runs the windows.** Thumbnails are
  made on spare processor threads, one keyframe from each video, and kept on disk. A
  still put on the output is read on another thread, at no more than the size it can be
  shown at, and brought in when it is ready, so a large picture does not make a
  transition or a playing video stutter.
- **The small pictures are honest but plain.** Thumbnails are the real slides drawn
  small, without their shadows, which cannot be seen at that size and are the dear part.
- **Lists are only rebuilt when they change.** The app notices when a workspace changes
  on disk, but rebuilds what is on screen only if what it shows is different.
- **What follows something moving is drawn with it, and seldom.** A player says where
  its video has got to twenty times a second. A transport drawn straight from that
  redrew the operator window twenty times a second, and cost over half as much again as
  playing a 4K video did. So the transport looks at the player a few times a second,
  and does it when the preview beside it is about to be redrawn for a new frame: the
  window is drawn once for both, and the transport costs nothing that can be measured.
  A timer that is stopped costs nothing, and one that is running a redraw a second: a
  third of a percent of a core, or half a percent when a slide on the output shows it.
- **Text that changes fast is drawn a digit at a time.** A timer set to show its
  hundredths changes thirty times a second, which is not what slide text is drawn for:
  drawing all of it again that often, large, on a full-screen output, took over a
  quarter of a core. Such text is laid out glyph by glyph, and only the glyphs that
  have changed are cleared, drawn and sent to the graphics chip again, which brings it
  to about a tenth of a core. In the thumbnails, where hundredths cannot be read, it
  is five times a second.
- **What moves keeps to the screen.** A transition, scrolling text and a build are moved
  on once at each frame of the screen they are on, by that screen's own refreshes, and
  on a screen playing a video in the refreshes the video leaves free (see
  `src/frameclock.h`). Qt's own animations, which these were, are stepped by a timer of
  sixty-two and a half a second while two windows are showing, as here: scrolling words
  jumped twice as far in one frame in twenty-five, and the step that came twice in a
  refresh took the turn of the video's next frame, which stayed on a refresh too long.
  And a slide changed by a cut is drawn once: it was drawn a second time a moment later,
  for the textures a transition would be drawn from, and on a screen playing a video that
  frame took the video's turn too.
- **The log is written when something happens, never when something is drawn.** A line
  of it costs four millionths of a second and is handed to the system at once: nothing
  waits for the disk, and nothing is written for a frame. See [Log](features/log.md).

## How it works

```
  a workspace on disk                    C++ (src/)                      QML (qml/)
  -------------------          ---------------------------       --------------------------
  Libraries/*/*.pro    --->    ProDocument, proconvert    --->   Main.qml: what is open,
  Playlists/Library            PlaylistFile, Timers,             what a key or a click
  Playlists/Media              Props, StageLayouts,              does
  Configuration/Timers,        Macros, Screens, Looks,                  |
    Props, Stage, Macros,      Themes                                   | goLive(), showMedia(),
    Groups, Workspace          (parse, flatten into                     | toggleProp()
  Themes/..., Media/...        lists and maps)                          |
        ^                                                               v
        |                      Show, showstate            <---   asks for a change
        +--- changes are       (what is live, and the     --->   is told what to show
             written back      rules it changes by)                     |
             into the files                                             v
                               StrokedText, textlayout           OutputScene.qml, for each
                               (text laid out and drawn   <---   audience screen: a media
                               with its outline, once)           layer and a slide layer,
                                                                 each a TransitionLayer,
                                                                 and the props over them
                               ThumbnailProvider, videoframe
                               (small pictures, cached)   --->   thumbnails in the lists
```

The app is a window, some screens and a folder. The folder is the workspace. The operator
window is where the show is run from; an audience screen is what the audience sees, a media
layer with a slide layer over it, the props over both and the messages over all three,
in a window, on a display or sent over the network ([Screens](features/screens.md)); a stage screen is what the
people on stage see.

The code is in two halves. The C++ in `src/` does files and pixels: it reads and writes
ProPresenter's documents, parses the RTF their text is kept in, lays text out and draws
it with its outline, and makes thumbnails. The QML in `qml/` is everything on screen:
the windows, and what a click or a key does in them. What passes between them is plain
data: a presentation crosses over as a list of slides, each a map of everything needed
to draw it, so the QML never sees a file format.

Between the two is the show itself. **What is live, and the rules by which it changes,
are in one place with nothing of the windows in it** (`src/showstate.*`): which slide
the show is at, what is on the media layer, which props are on; and what happens to
those when a slide goes live, media is put on, a layer is cleared or a macro is run.
When is a background video left to play on? What does a slide that clears itself leave
marked? What does a macro that runs a macro do? Each is a few lines there, tried by
tests that need no window ([Tests](#tests)). The operator window only asks for a change
(through `Show`, which QML can talk to) and is told what the output is then to be
handed. Anything that decides what the audience sees belongs there, and not in a
window's script.

Three rules run through all of it.

- **The files are ProPresenter's, and stay that way.** They hold far more than this app
  understands. Every change is made by parsing the whole file, altering only the fields
  the change is about, and writing the whole thing back in one step, so that whatever
  the app does not know about goes back exactly as it came.
- **One drawing of a slide.** The same component draws a slide on the output, in a
  thumbnail, in the preview and in the editor, from the same data. Layout is done in
  the slide's own coordinates and scaled, so a line of text breaks at the same word at
  every size.
- **One keeper of what is live.** Nothing but `Show` changes it, and everything that
  marks or shows it reads it from there.

Most of what a slide shows is settled when its file is read. The exception is text that
changes while the slide is on show: a timer's time, the words or the notes of the
slide that is live, the time of day, or the time a video has left. An element linked to
a timer is drawn by asking `Timers`, the one object that holds the timers and keeps
them running, what the time is now, and is drawn again when the answer changes; one
linked to the live slide asks `Show`, which is what keeps what is live; and one linked
to the clock or to a video asks `Clocks`, which goes round once a second, and is told
where the video has got to, only while some box is showing it. A stage layout is
nothing more than a slide made of such
boxes, and a prop nothing more than a slide laid over the others, so both are drawn by
what draws every slide and edited by what edits every slide.

A transition is a small fragment shader that is handed the outgoing and incoming
pictures and a number that goes from 0 to 1; `shaders/dissolve.frag` explains the
pattern. Adding one is a shader file, a line for it in the build, and an entry in
`qml/TransitionCatalogue.qml` that names it and lists what can be adjusted about it.

`src/main.cpp` opens with a longer tour, and the header of each file says how that part
works.

## Benchmark

```
benchmark/run.py
```

runs the app several times over on a workspace of its own making, times what an
operator waits for (starting up, opening a presentation, a slide reaching the output
with the output full screen, the frames of a transition, a change in the editor
reaching the screen, what it costs standing by, and memory), and compares the times
with a baseline kept in `benchmark/baseline.txt`, marking whatever has got worse by
more than a fifth. It is for seeing that a change has not made the app slower. Nothing
of yours is read or changed by it. See [benchmark/README.md](../benchmark/README.md).

## Self-test

```
./build/SimplePresenter --selftest <dir>
```

drives the app through a fixed sequence (a slide, a transition, the clears, a simulated
trackpad swipe, then the editor brought up on a presentation with an element picked and
its text being edited), saves frames from each window into `<dir>` as PNGs, and quits.
It changes nothing in the workspace, neither reads nor changes saved settings, and opens
the first workspace unless `--workspace` names one. Its [log](features/log.md) goes into `<dir>`
with the pictures, and not into Documents. The timers and the transport are
held still for it, so that what they show does not depend on when it is run. Run it
before and after a change to anything that draws, and compare the frames.

With `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` in front of it, it runs
without putting windows on the screen, and every run gives the same frames; that way of
drawing leaves out shadows, transitions and video, so it checks layout and text, not
those.

## Tests

There are two kinds, and both are in `tests/` ([tests/README.md](../tests/README.md) has
the whole of it).

**Unit tests** try the parts of the app that are plain rules, by themselves, with no
window and no workspace: the rules of the show (`src/showstate.*`), chords, the screens,
the looks and the masks in ProPresenter's file, how a piece of audio plays and how loud it is,
where sound goes (the app's channels in ProPresenter's file, a media's, and the app's own
mixer, a stretch at a time),
search, how a date and a time are written, the matching of themes, and Bibles, from
reading a passage as it is typed to sharing it out across slides. They are
built with the app and take a few milliseconds:

```
ctest --test-dir build
```

**Scripted tests** work the whole app as someone would, with the mouse and the keyboard,
and look at what its windows then hold and show (and, for sound, what a sound device of
eight channels was sent, recorded): seventy-six scripts and some three
thousand checks. They run on a desktop of their own that shows nowhere, on
copies of workspaces, with settings of their own, so nothing of a run touches the real
desktop, settings or workspaces. The app is built for them with a door the scripts come
in by, which a release does not have:

```
cmake -B build-tests -G Ninja -DSIMPLEPRESENTER_TEST_HOOK=ON
cmake --build build-tests
tests/ui/run.py
```

The workspaces those scripts run on are not in the repository (they are not the
repository's to publish), so on another computer only the unit tests run as they are.

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | Startup and command-line options; begins with a tour of the code |
| `src/catalog.*` | The open workspace: its libraries, playlists and media, and every change to them |
| `src/workspacefiles.*` | How documents refer to files, and how those files are found again |
| `src/prodocument.*` | Reads `.pro` files for showing, and makes the changes show mode can |
| `src/proconvert.*` | Turns a slide in a `.pro` file into what is drawn, and changes back into the file's terms |
| `src/presentationeditor.*` | A presentation, the props, the stage layouts or a theme open in the editor: its changes (a song's chords among them), undo, saving and backups |
| `src/playlistfile.*` | Reads and writes the three playlists files: of presentations, of media, and of the audio bin |
| `src/audiocue.*` | How a piece of audio plays, as ProPresenter's files say it: read from a row of the audio bin or an action, and changed there |
| `src/audiolayer.*` | What plays the audio layer: the music and the sound effects, their fades and crossing, and the choice of the device sound goes to |
| `src/audiometer.*`, `src/audiolevel.*` | The meter of what is being played, and the arithmetic of sound: how loud a stretch of it is, what a meter shows of that, how a fade goes |
| `src/audiorouting.*` | Where sound goes, as plain rules: ProPresenter's Auto for media and for a device, matrices clicked, Mute and Solo, a channel's decibels, the ramps, and what a player is told of a media's sound |
| `src/audiosetupfile.*` | The app's channels in ProPresenter's file of how the workspace is set up: how many, their names, Mute and Solo |
| `src/audiomix.*` | The app's own mixer, with nothing of a device or a player in it: each player's sound brought to the device's rate and kept in step with it, mixed into the app's channels and on to the device's |
| `src/audiomixer.*` | The mixer's device, on a thread of its own, the app's channels and where they go on this computer's devices; and a player's sound handed to it (AudioFeed) |
| `src/mediachannels.*` | How many channels of sound a file has, and what they are, read off the operator window's thread, for the Media Inspector's Audio tab |
| `src/audioprobe.*` | How long a sound file is, read from it as it is added to the audio bin |
| `src/timers.*` | The workspace's timers: their file, their running, and what a text box linked to one shows |
| `src/clocks.*`, `src/clocktext.*` | The time of day and the time a video has left, for text boxes linked to them, kept going only while one is shown; and how a date and a time are written |
| `src/groupkeys.*` | The hotkeys of groups: read from and written to the workspace's list of groups, where ProPresenter keeps them |
| `src/slidenumber.*`, `src/numbertyping.*` | A slide asked for by its number, typed: what the digits make and which slide that is, and what QML reaches those rules by |
| `src/groupedit.*` | Putting a slide, and those after it in its group, into another group, as ProPresenter does |
| `src/arrangementedit.*` | A presentation's arrangements made, copied, renamed and removed, and a group put into one, moved along it and taken out |
| `src/labelfile.*`, `src/labels.*` | Slide labels: a slide's own, and the labels a workspace keeps ready, in ProPresenter's list of them |
| `src/clearfile.*`, `src/cleargroups.*` | The workspace's clear groups, in ProPresenter's file of them: the layers each clears, its picture and tint, and adding to, changing and removing them |
| `src/messagefile.*`, `src/messages.*` | The workspace's messages, in ProPresenter's file of them: a message's words and tokens, its theme and how it goes away, read and changed; a message as a slide in its theme; and the words one is showing |
| `src/reflow.*` | The reflow editor's rules for a slide's words: where a slide is split for a caret, a stretch of words taken with its look, two lots joined, words typed over, and where the chords go through each |
| `src/elementorder.*` | Where an element lands when its row is dragged up or down the editor's list of a slide's elements: from a gap in the list, which has the front one first, to a place in the order drawn |
| `src/messagetext.*` | A message's words in a box they are typed in, each token a small label among them, and the pictures of those labels |
| `src/decodingchoice.*`, `src/videodecoding.*` | Who decodes video, the graphics chip or the processor alone: the rule by which that is settled as the app starts, and what QML reads it by |
| `src/decodingguard.*` | The guard on hardware video decoding: which computers have a graphics driver whose decoder is not to be tried (nouveau without its decoder's firmware) |
| `src/cursors.*` | The pointer for turning an element in the editor, and whether Ctrl is held |
| `src/actions.*` | What a slide's cue or a macro does besides: turns ProPresenter's actions into plain data and back |
| `src/macros.*` | The workspace's macros and their collections: the file, and adding to, changing and removing them |
| `src/props.*`, `src/stagelayouts.*` | The workspace's props and their collections, and its stage layouts: their files, and adding to, renaming and removing them |
| `src/showstate.*` | What is live and the rules by which it changes, with nothing of the windows in it: what a slide going live, media, a piece of audio, a clear, a prop or a macro does; the announcements, which are at a slide of their own; and a slide going on to the next by itself |
| `src/show.*` | The one keeper of what is live: the above as QML talks to it, with signals for what the output is to be handed; and the words of the live slide and the next, for the text boxes that show them |
| `src/awake.*` | Asks the desktop to keep the screens awake while the output or the stage is showing |
| `src/screenfile.*` | The list of a workspace's screens in ProPresenter's set-up file: reading it, and adding to, renaming in and removing from it |
| `src/lookfile.*`, `src/looks.*` | The workspace's looks, in ProPresenter's set-up file: the saved ones and the live look, which layers each audience screen gets, in what theme, and under which mask |
| `src/maskfile.*`, `src/masks.*` | The workspace's masks, in the same file: each a slide and a name, read, made, renamed and removed |
| `src/themefile.*`, `src/thememath.*`, `src/themes.*` | The workspace's themes: reading them, which text box of a slide goes into which of a theme's, and dressing slides in a theme, for good or for a screen |
| `src/chords.*`, `src/chordsbridge.*` | Chords: what one is, changing key, the four notations, ChordPro text; and the same for QML to call |
| `src/songimport.*` | A new presentation made of a ChordPro file |
| `src/bible.*`, `src/biblexml.*` | A Bible: its books, chapters and verses, the books' names, a passage as it is typed and as it is written; and reading one from an XML file of the four kinds |
| `src/bibleflow.*` | How a passage's words are shared out across slides: Fill, and Even at natural pauses, and where that fails; and round slides kept as they were |
| `bibles/` | The three public-domain Bibles that come with the app, as Zefania files, the list the app reads, and the script that made them from eBible.org's (its README) |
| `src/colortriggers.*` | A timer's Color Triggers: which is in force as its time comes down or goes up, and a trigger's time written and read as typed |
| `src/linejoin.*` | A text box's lines made one, each run keeping its look: ProPresenter's Single Line and Replace Line Returns |
| `src/apibible.*` | api.bible: what is asked of it and how its answers are read |
| `src/bibles.*`, `qml/BiblePanel.qml` | Bible mode: the Bibles imported and api.bible's, a passage looked up, and slides made of it in a theme; and the bar over the slides |
| `src/searchindex.*`, `src/search.*` | Search: the finding, and the reading of the libraries for it in the background |
| `src/screens.*` | The workspace's screens and what each is sent out through on this computer: a window, a display, NDI or nothing |
| `src/ndi.*`, `third_party/ndi/include/` | A screen sent over the network as an NDI source: drawn out of sight, read back, and handed to NDI's library, which is looked for when the app runs, and fetched for whoever asks. The folder has NDI's header files |
| `src/ndisetup.*`, `qml/NdiSetup.qml` | Getting NDI's library: reading NDI's installer for its licence and its archive, and the panel that offers to fetch it or says how to by hand |
| `src/playlistimport.*`, `src/zipreader.*` | Imports exported `.proplaylist` archives |
| `src/richtext.*` | Styled text as the app works with it, and formatting part of it |
| `src/rtf.*`, `src/rtfwriter.*` | Reads and writes the RTF that slide text is stored in |
| `src/textlayout.*` | Lays text out, the same for drawing it and for editing it in place, and finds the size at which text fits a box |
| `src/strokedtext.*` | Draws slide text with stroke and fill |
| `src/richtextbridge.*` | Lets a text box on the slide be typed into |
| `src/thumbnailprovider.*`, `src/videoframe.*` | Thumbnails of images and videos, made on worker threads and cached, at the moment and in the crop, turn and flip a Media Inspector sets |
| `src/mediasettings.*`, `src/mediaeffects.*` | What a piece of media's Media Inspector sets, read from and written to ProPresenter's fields, and ProPresenter's effects |
| `src/framerelay.*` | Feeds the preview from the output's video frames |
| `src/firstframe.*` | Says when a video has its first picture, so that it is not put on the output before |
| `src/fontresolver.*` | Finds fonts by PostScript name through fontconfig |
| `src/selftest.*` | The self-test |
| `src/benchmark.*`, `qml/Benchmark.qml`, `benchmark/` | The benchmark: what it works on and measures with, its run, and the script that repeats it and compares it with the baseline |
| `src/sessionlog.*` | The log of a run: what the app is running on, what it did, a crash's last lines, and the watch for the app not answering |
| `src/testhook.*` | The door the scripted tests come in by; only in a build made for testing |
| `tests/unit/` | The unit tests, run by `ctest` |
| `tests/ui/` | The scripted tests, the scripts' shared helpers, and the program that runs them on a desktop of their own |
| `qml/Main.qml` | The operator window: what is open, and what a click or a key does |
| `qml/Toolbar.qml`, `Sidebar.qml`, `SlideGrid.qml`, `PreviewPanel.qml`, `MediaBin.qml` | The parts of the operator window |
| `qml/AudioPanel.qml`, `qml/AudioGlyph.qml` | The audio bin, a tab of the show controls; and its pictures, which are drawn here |
| `qml/SlideNotes.qml` | A slide's notes, in a pane under the slide in the editor |
| `qml/SimpleViewToggle.qml` | The button that floats over the slides in Simple View, and leads back out of it |
| `qml/TypedNumber.qml` | The panel over the slides that shows a slide's number while it is typed |
| `qml/Transport.qml`, `ShowControl.qml`, `TimersPanel.qml`, `PropsPanel.qml`, `StagePanel.qml`, `MacrosPanel.qml` | Under the preview: the transport for the video that is playing, and the show controls with their tabs of timers, props, stage screens and macros |
| `qml/ActionDialog.qml`, `qml/PopupMenu.qml` | The panel a timer action or a stage action is set up in, and the menus, which open one out of another |
| `qml/SettingsScreen.qml` | The settings: groups, slides, Bibles, screens, looks, video, audio, windows, and what the app is |
| `qml/Editor.qml`, `EditorCanvas.qml`, `EditorInspector.qml` | The editor: its lists and its three ways of working, the slide being worked on, and the properties panel |
| `qml/OutputScene.qml`, `qml/StageScene.qml`, `qml/StageView.qml` | What an audience screen and a stage screen show, whatever they are sent out through; and the plain view a stage screen has without a layout |
| `qml/Output.qml`, `qml/Stage.qml`, `qml/AuxWindow.qml` | A screen in a window: floating, or filling a display |
| `qml/LooksPanel.qml` | The Looks window: the saved looks and the live one, and what each gives each screen |
| `icons/`, `src/iconprovider.*`, `qml/ProIcon.qml` | ProPresenter's own pictures for what this app has too, where they are from and how the files were made (`icons/README.md`, `icons/make.py`), and how one is drawn in any colour at any size |
| `qml/ThemesPanel.qml` | The themes, let down from the toolbar's button |
| `qml/SearchPanel.qml` | The search window |
| `src/rtfstandins.*` | A line of chords with no words given more stand-in characters or fewer, where it stands in a text box's RTF |
| `qml/ChordSheet.qml`, `qml/ChordProEditor.qml` | The two chord editors: the song as a sheet with its chords in bubbles, and as ChordPro text |
| `qml/EditorModes.qml` | The switch between the editor's three ways of working, in the middle of its toolbar |
| `qml/ReflowEditor.qml` | The reflow editor, a fourth way: the slides' words as a table that is typed in, split and joined, with the slides beside it |
| `qml/ChordedText.qml`, `qml/chordlayout.js` | Words with chords over them, as a stage screen draws them; and where a chord stands over its line |
| `qml/ImportSong.qml` | The panel a ChordPro file is imported from |
| `qml/NewPresentation.qml` | The panel a new presentation is named in |
| `qml/SlideLabel.qml` | The panel a label is made in: its words and its colour |
| `qml/ArrangementBar.qml`, `qml/ArrangementsPanel.qml` | The arrangement editor over the slides: Master's groups and the arrangement being viewed, as labels that are dragged; and the window that lists a presentation's arrangements |
| `qml/ClearGroupsPanel.qml` | The window the clear groups are set up in: the groups, and the layers, picture and tint of the one picked |
| `qml/DecodingNotice.qml` | The notice, shown once, that hardware video decoding is being held back on this computer |
| `qml/ClearGroupGlyph.qml`, `qml/clearglyphs.js` | The picture on a clear group's button: ProPresenter's own where the pack has it, and this app's drawings of the rest |
| `qml/ScreensSettings.qml` | The Screens section of the settings |
| `src/windowlists.*` | Keeps the windows of the screens out of Alt+Tab where the app can see to that itself (through X11) |
| `qml/TransitionLayer.qml`, `qml/MediaContent.qml` | One output layer with shader transitions, and what the media layer shows on it |
| `qml/MediaLook.qml`, `qml/MediaEffects.qml` | A piece of media drawn as its Media Inspector sets it, and its effects |
| `qml/MediaInspector.qml`, `qml/QuickEdit.qml` | The Media Inspector, and Quick Edit of a slide's words |
| `qml/AudioRouting.qml`, `qml/RoutingMatrix.qml` | The Channel Routing window of the settings, and the matrix it and the Media Inspector's Audio tab draw |
| `qml/PropsLayer.qml` | The props that are on, over the other layers; and, over those, the messages that are shown |
| `qml/AnnouncementLayer.qml` | The announcements layer of a screen: a slide over media of its own |
| `qml/GoToNextPanel.qml` | The panel the seconds of a Go To Next Timer, or of a Slide Show, are typed in |
| `qml/MessagesPanel.qml` | The messages, a tab of the show controls |
| `qml/TransitionCatalogue.qml`, `qml/TransitionControls.qml`, `qml/TransitionOptions.qml` | The transitions there are and what can be adjusted about each, the controls that choose one, and the panel for adjusting it |
| `qml/Slide.qml`, `qml/SlideElement.qml` | Draw a slide and one element of it |
| `shaders/` | The transitions: those written for this app, and in `shaders/gl-transitions` those ported from gl-transitions; and in `shaders/effects` the effects media can be given, written for this app |
| `packaging/` | The launcher, icon and description that an installed copy has, and the GNOME Shell extension that keeps the output and stage windows out of Alt+Tab through Wayland |
| `third_party/ProPresenter7-Proto` | The descriptions of ProPresenter's file formats, as a submodule |
| `AGENTS.md`, `CLAUDE.md`, `docs/agent-playbook.md` | What an AI coding agent is told before it works here, and its guide to helping someone who uses the app |
