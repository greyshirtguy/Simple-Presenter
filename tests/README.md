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

So far there are three:

| Test | What it tries |
| --- | --- |
| `tst_ndisetup` | Reading NDI's installer (`src/ndisetup.h`): finding the licence it shows and the archive it carries, and not being fooled by something that is not it |
| `tst_screenfile` | The list of a workspace's screens in ProPresenter's set-up file (`src/screenfile.h`): what a workspace with no file has, what adding, renaming and removing a screen write, and that the rest of a file ProPresenter wrote goes back as it came |
| `tst_showstate` | The rules of the show (`src/showstate.h`): what a slide going live puts on the output and in what order, when the media it brings is started and when what is playing is left to, what a slide's actions do, what clearing leaves, how props stack and give way, what a macro does, where a step takes the show, and how the show follows a presentation that is read again |

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
| `workspaces/Act` | The same presentations with ProPresenter's macros, groups and its own account of the workspace |
| `workspaces/Shapes` | One presentation and two pictures, for the editor |
| `made/` | Small videos and pictures made for the tests: one keyframe in twenty seconds, dark until nine seconds in, VP9, HEVC, one with sound |

**Writing one.** Start from a short one (`t/rapid.qml`). A script is a `QtObject` with a
list of steps; each step does something and says how many milliseconds to wait before
the next, so that the app can draw in between. Where the script has `//COMMON`, the
runner puts in the helpers every script shares (`t/common.txt`: `check()`, finding an
item by its `objectName`, clicking it); `t/lib.js` has more. Because the script is
loaded in the operator window's own scope, it calls the window's functions and reads its
properties by name. Then give it a line in the `TESTS` table of `run.py`, which says the
workspace it runs on and whether it has to be really drawn.

## And two more, in the app itself

`SimplePresenter --selftest <dir>` saves pictures of a fixed run for comparing before
and after a change to anything that draws, and `benchmark/run.py` times a fixed run
against a baseline. The main README has both.
