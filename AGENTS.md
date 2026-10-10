# For an AI model working in this repository

This file is for you, the AI coding agent, whichever one you are. Read it before doing
anything here. It is short on purpose; the rest is where it points.

There are two reasons you may be here. Someone who **uses** Simple Presenter has a
problem and has asked you to help: read this file, then
[docs/agent-playbook.md](docs/agent-playbook.md), which is written for exactly that. Or
you are **changing** the app: this file is the brief.

## What this is

Simple Presenter is a presenter for Linux (Qt 6, C++20, QML) that runs a show from a
ProPresenter 7 folder as it is, reading and writing ProPresenter's own files. It is one
person's hobby, written entirely by describing it to an AI model. Nobody supports it.
`README.md` says what it does, part by part, and then how it is made; `src/main.cpp`
opens with a tour of the code; every source file starts by saying what it is for and
why it is the way it is. Read the header of a file before changing the file.

## What must stay true

These are not style. A change that breaks one of them is wrong however well it works.

1. **ProPresenter's files stay ProPresenter's.** A workspace must open in ProPresenter
   after this app has worked on it. Every write parses the whole file, changes only the
   fields the change is about, and writes the whole thing back, so that everything this
   app does not understand goes back exactly as it came. Never rebuild a message from
   the parts you know. Never drop a field, an entry or a file because the app has no use
   for it. What ProPresenter has a file for is kept in that file; only what is this
   app's alone, or this computer's alone, goes in the app's own settings.
2. **Do as ProPresenter does.** Where ProPresenter has a feature, its behaviour, its
   names and its controls are the specification, including the awkward corners (a stage
   text box that looks for a name and finds none shows nothing, on purpose). If you do
   not know what ProPresenter does, say that you are guessing, in the code and to the
   person, and do not present a guess as a fact.
3. **It has to be quick on an old laptop.** It is developed on a 2017 two-core laptop
   with integrated graphics. Nothing is drawn twice, nothing is drawn while nothing
   changes, and no file is read on the thread that draws. A feature that costs every
   slide change a few milliseconds is a regression. `benchmark/run.py` measures it.
4. **One keeper of what is live.** What the audience sees is decided in
   `src/showstate.*` (plain rules, with unit tests) and changed only through `Show`. Do
   not decide it in a window's script.
5. **The person's files are not yours to risk.** See the next section.

## The person's files

- Never work on a real workspace. Workspaces are folders under
  `~/Documents/SimplePresenter/WorkSpaces`; they hold a church's songs, media and
  set-up, and the app changes what it opens. Make a copy somewhere temporary and point
  the app at the copy with `--workspace <folder>`.
- Never change the app's real settings (`~/.config/SimplePresenter/`) to try something.
  Run it with `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` set to temporary
  folders instead.
- Reading is fine. The log of each run (`~/Documents/SimplePresenter/Logs/`) was written
  to be handed to you: start there.
- Songs are other people's copyright, and a workspace says a good deal about the people
  who use it. Do not copy any of it into this repository, an issue, a pull request, a
  paste site or anywhere else off the computer. Names of files are in the log; words of
  songs are not.
- Before deleting anything, look at what it is. Do not delete by a path built from a
  variable that could be empty.

## Building and checking

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build                 # the unit tests: a second
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    ./build/SimplePresenter --workspace <a copy> --selftest <an empty folder>
benchmark/run.py                       # times a fixed run against a baseline
```

`README.md` (Building from source) has what to install first. The self-test saves
pictures of a fixed run; run it before and after a change to anything that draws and
compare them. Anything its log says with `qrc:` in it is a QML error: there must be none.

**What you cannot run.** The scripted tests (`tests/ui/run.py`, about thirty scripts
that work the whole app) need workspaces that are not in this repository, because they
are not the repository's to publish. So on anyone's computer but the maintainer's, the
unit tests, the self-test and the benchmark are all there is. Say so when you report
what you checked; do not say "the tests pass" and mean a part of them.

## Changing the code

- Match what is there: the naming, the idiom, and above all the comments. Every new
  file opens by saying what it is, how it works and why; a guess about ProPresenter, a
  choice made for speed, and anything in a file that is deliberately left alone are
  said in a comment where they happen. A comment that has become untrue is a bug.
- Rules go in plain C++ with nothing of the windows in it, with a unit test
  (`tests/unit/`), and are handed to QML as plain data. `tests/README.md` says how.
- A change to what a file holds needs a test that reads a file ProPresenter might have
  written, makes the change, and looks at what else is still there.
- No new dependency without the maintainer agreeing to it first. NDI's library and its
  SDK are never added to the repository or to a package (`third_party/ndi/include` holds
  the few headers its licence allows).
- Words the user reads say plainly what a thing does. They never promise help, and
  never call the app a product.
- A thing ProPresenter has a picture for is shown with ProPresenter's picture, not a
  new drawing: `icons/README.md` has what there is, where it is from and how to add
  one. Only what the pack has no picture for is drawn here.
- A test that a thing is shown looks at what is drawn (a pixel, a picture), and at
  what the operator sees of it in the operator window, not only at the property that
  is meant to bring it about. A layer that a look had taken off a screen was once
  still in the operator's preview, and every test passed.

## Helping someone, and pull requests

Most problems are one computer's: a missing video driver, a font, a Wayland quirk, a
workspace that was moved. Those are fixed on that computer and are **not** changes to
this repository. [docs/agent-playbook.md](docs/agent-playbook.md) has how to tell which
kind you have, what to look at, and the known ones.

A pull request is only for a fault that anyone would have. Before suggesting one:

- it happens on a clean build of `main`, with files you can describe without copying
  anybody's songs;
- you can say what the cause is, not only what makes the symptom go away;
- there is a test that fails without the change and passes with it, where the thing
  can be tested;
- the unit tests pass, the self-test's pictures differ only where they were meant to,
  and the benchmark is no worse, on the same computer, before and after;
- it is one fix, as small as it can be, with nothing else tidied on the way;
- the person you are helping has read it, understands it, and wants it sent. It goes in
  their name. Say in it that an AI model wrote it, which one, and what you could not
  check.

Nobody is promised that a pull request is read, answered or merged. Do not open issues
or pull requests on your own initiative, and do not send one to get a local fix "into
the next version" for the person: build it for them from source instead.

## Things you will be asked about

- **It is not ProPresenter**, and has nothing to do with Renewed Vision. Much that
  ProPresenter does is not here: `README.md` has the list under TODO.
- **Where things are kept**: workspaces and logs under `~/Documents/SimplePresenter/`;
  settings in `~/.config/SimplePresenter/SimplePresenter.conf`; copies of files as they
  were before an edit, and NDI's library if it was fetched, under
  `~/.local/share/SimplePresenter/SimplePresenter/`.
- **The file formats** are in `third_party/ProPresenter7-Proto/autogen-proto` (a
  submodule): the set taken from ProPresenter itself, which the app is built from, and
  not the older folder `proto` beside it. `protoc --decode` with those turns any of
  ProPresenter's files into text you can read: the playbook has the commands.
