# Helping someone who uses Simple Presenter

This is for an AI coding agent that a person has asked for help with Simple Presenter,
on that person's own computer. Read [AGENTS.md](../AGENTS.md) first: it has the rules
this page leans on, above all that **the person's workspaces and settings are never
experimented on, only copies of them**.

You are the only help there is. The app is one person's hobby, nobody supports it, and
the person in front of you may be running a service from it on Sunday. So: find out
what is really wrong before changing anything, change as little as you can, say plainly
what you did and what you did not check, and never leave their set-up worse than you
found it.

## First, find out what happened

1. **Ask**, if you do not know: what they did, what they expected, what they saw, and
   whether it used to work. Which workspace, and whether it came from ProPresenter.
2. **Read the log.** Each run of the app writes one to
   `~/Documents/SimplePresenter/Logs/`, named for when the app was started; the newest
   is the last run. It begins with what the app was running on (its version, Qt, the
   system, the processor, the screens, the graphics chip and its driver) and then has a
   line for everything that was done or that happened. Lines whose second column is in
   capitals are what went wrong: `WARNING` and `ERROR` (Qt or a library complaining),
   `PROBLEM` (something the app could not do, including every error it showed),
   `STALLED` (it stopped answering for two seconds) and `CRASH` (with where it was).
   The start of a log also says if the run before it crashed or was killed.
   `README.md`, under Log, has the whole of it.
3. **See what they are running.**

   ```
   SimplePresenter --version          # or ./build/SimplePresenter --version
   dpkg -l simplepresenter            # the installed package, if there is one
   echo $XDG_SESSION_TYPE $XDG_CURRENT_DESKTOP
   ```

   The package is built for Ubuntu 26.04 and its Qt (6.10) and will not install
   elsewhere; anywhere else the app is built from source, which needs Qt 6.9 or newer.
   Check that what they have is not simply older than the fix they need: the Releases
   page of the repository says what each version brought.

## Then decide which kind of problem it is

**The computer.** By far the most likely. It happens in one place and not another, or
it started when something on the computer changed. Nothing in this repository changes:
you fix the computer, or tell the person what would. The table below has the ones that
are known.

**One file.** A presentation, a theme or a layout that ProPresenter shows one way and
this app another, or that this app will not open. This app does not draw everything
ProPresenter can (`README.md`, under TODO, lists what is and is not drawn), so first
see whether it is a thing the app is known not to do: that is not a fault. If it is
not, you may have found one. Work on a copy of the file.

**The app.** It does the wrong thing with any workspace, on any computer. Make sure of
that before believing it: build `main` from source, run it on a copy of the workspace
with fresh settings (below), and see the fault there.

## Known problems that are the computer's

| What they see | What it usually is | What to do |
| --- | --- | --- |
| Video stutters, or the fans run, most of all with 4K | No driver for decoding video on the graphics chip, so the processor does it. The log's line for each video says whether its frames arrive "as textures" (the chip) or "in memory" (the processor) | `README.md`, Hardware video decoding: `vainfo` says what the chip can decode; on Intel install `intel-media-va-driver` |
| The package will not install | It is for Ubuntu 26.04 only | Build from source (`README.md`) |
| Words are in the wrong font | The font the presentation names is not installed. The log names every such font | Install the font, or choose another in the editor. `fc-list` shows what is installed |
| A slide's video or picture is missing | The workspace was copied without its media, or the media is somewhere the files do not say. The log has a `PROBLEM` line naming the file | Media is looked for by its path inside the workspace, then by the path recorded for it, then by name anywhere under the workspace's `Media` folder: put it there |
| The output or stage window comes back in the wrong place | A Wayland desktop does not let an application place its windows | Settings, Screens: send the screen to a display by name. Or Settings, Windows: run through X11 |
| The output window is in Alt+Tab | Only GNOME can leave it out, with the extension that comes with the app | `README.md`, Screens |
| The screens go dark during a sermon | The desktop is not one that answers when asked to stay awake (GNOME and KDE do). The log says so once | Turn blanking off in the desktop's own settings |
| An NDI screen is not seen by a receiver | NDI's library is not installed (Settings, Screens says so and offers to fetch it), or the network does not pass NDI's discovery | The panel in Settings; then the network |
| A foreground video has no sound | The desktop's sound is going somewhere else, or is muted. A background video is silent on purpose | The desktop's sound settings |
| A layer switched off (or a theme chosen) in the live look is back as it was at the next song | The live look was changed by hand, and then a slide or a macro ran an action that goes over to a look, which puts the live look back as that saved look has it. Songs often have such a macro on their first slide. (It is taken to be what ProPresenter does too, making a look live being a copying of it; that has not been tried in ProPresenter.) The log has a `looks` line each time ("the live look had been changed: it is put back…"), and one for every change made to a look | In the Looks window, **Save** keeps the live look as the saved look it came from; or change the saved look and make it live |
| A stage text box is empty for some songs | The box is set to show only the text of elements with a certain name, and those songs' text boxes are called something else. That is ProPresenter's behaviour and is kept on purpose | Rename the song's text box in the editor, or dress the song in a theme, which gives matched text boxes the theme's names |
| The app is slow to start, or to open a big library | Thumbnails being made for the first time, or a slow disk | See whether a second start is quick. The log's `health` lines say what it is using |

Add to this table when you find another, if the person wants to send the addition
back (see Pull requests in `AGENTS.md`): it is the most useful thing here.

## Looking closer, without touching anything of theirs

**A copy of the workspace, and settings of its own.** This runs the app as if it had
never been run, on a copy:

```
mkdir -p /tmp/sp-try/home
cp -r ~/Documents/SimplePresenter/WorkSpaces/<name> /tmp/sp-try/workspace
XDG_CONFIG_HOME=/tmp/sp-try/home/config XDG_DATA_HOME=/tmp/sp-try/home/data \
XDG_CACHE_HOME=/tmp/sp-try/home/cache \
    ./build/SimplePresenter --workspace /tmp/sp-try/workspace
```

A workspace's `Media` can be many gigabytes. Where it is, copy everything but that
folder and link it instead: media is only ever read.

```
rsync -a --exclude Media ~/Documents/SimplePresenter/WorkSpaces/<name>/ /tmp/sp-try/workspace/
ln -s ~/Documents/SimplePresenter/WorkSpaces/<name>/Media /tmp/sp-try/workspace/Media
```

The log of such a run still goes to `~/Documents/SimplePresenter/Logs/`, with the
others.

**What the app is told about video.**

```
QT_LOGGING_RULES="qt.multimedia.ffmpeg*=true" ./build/SimplePresenter 2>&1 | grep hwaccel
```

**What is in one of ProPresenter's files.** They are Protocol Buffers, and the
descriptions of them are in the repository (get them with
`git submodule update --init`). Use the folder `autogen-proto`: it is taken from
ProPresenter's own program, it is what the app is built from, and it has a name for
every field. The folder `proto` beside it is older and leaves many fields as bare
numbers. This prints a presentation as text:

```
P=third_party/ProPresenter7-Proto/autogen-proto
protoc -I $P --decode rv.data.Presentation $P/presentation.proto < "Some Song.pro" | less
```

`protoc` warns that an import is unused; that is nothing.

| File in a workspace | `--decode` | from |
| --- | --- | --- |
| `Libraries/<library>/<name>.pro` | `rv.data.Presentation` | `presentation.proto` |
| `Playlists/Library`, `Playlists/Media` | `rv.data.PlaylistDocument` | `propresenter.proto` |
| `Themes/<theme>/Theme` | `rv.data.Template.Document` | `template.proto` |
| `Configuration/Workspace` (screens, looks) | `rv.data.ProPresenterWorkspace` | `proworkspace.proto` |
| `Configuration/Stage` | `rv.data.Stage.Document` | `stage.proto` |
| `Configuration/Props` | `rv.data.PropDocument` | `propDocument.proto` |
| `Configuration/Timers` | `rv.data.TimersDocument` | `timers.proto` |
| `Configuration/Macros` | `rv.data.MacrosDocument` | `macros.proto` |
| `Configuration/Groups` | `rv.data.ProGroupsDocument` | `groups.proto` |

Text in a slide is RTF inside the file, with more beside it (capitals, chords);
`src/proconvert.h` says how it is read.

**Whether a file survives the app.** The promise is that a file the app changes still
has everything in it that the app did not mean to change. To see it for one file:
decode it as above before and after, on a copy, and `diff` the two. What differs should
be only what was changed. If more does, that is a fault of the important kind.

**A crash.** The log's last lines say where the app was. Places inside the app are
numbers in a released package; `simplepresenter_<version>_symbols.txt.xz`, beside the
package on the Releases page, turns them into names (`README.md`, Building from source).
A build from source has the names already; where the system keeps what a crashed program
leaves behind (`coredumpctl list`), `coredumpctl gdb SimplePresenter` opens the last
crash in a debugger.

**Pictures of what it draws.** `--selftest <folder>` runs a fixed sequence and saves
pictures; with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` in front it needs
no screen. It is on whatever workspace `--workspace` names, changes nothing in it, and
reads no saved settings.

## If the app has to be changed

Read `AGENTS.md` again, the part called What must stay true, and then:

- Build from source and make the change there. The person then runs their own build;
  say clearly that they are now running something nobody else has, and keep a note of
  what you changed and why in a file beside it, so that the next agent (or you, with no
  memory of this) can see it.
- Change one thing. Do not tidy, rename or "improve" on the way.
- If the change is to how a file is written, try it on a copy of their workspace and
  look at the file before and after as above. A file that ProPresenter will no longer
  open is the worst thing you can do to this person.
- Run what there is to run and say what it was: the unit tests, the self-test before
  and after, and the benchmark before and after if the change is anywhere near drawing
  or slide changes.

## What you must not do

- Run the app, or anything else, on the person's real workspace to "see what happens".
- Delete or overwrite anything of theirs without showing them what it is first.
- Send any of their files, or any part of one, anywhere: not to an issue, not to a
  pull request, not to a paste site, not as a "minimal example".
- Switch off, skip or loosen a test to get past it.
- Tell them it is fixed when you have only seen that it builds.
- Install system packages, change drivers or edit system files without saying exactly
  what and why, and letting them decide.

## What is not here yet

Things that would make this easier and safer, which the maintainer means to add:

- A workspace that can be published, made of nothing anyone owns, so that the scripted
  tests can be run by anyone, and by you.
- A test that a whole workspace survives being read and written back unchanged, file by
  file, which is the project's main promise and is at present checked by hand.
- One command that runs every check there is and writes down what it found.
- The checks run by themselves on every pull request.

Until they are, be more careful than they would let you be.
