# Simple Presenter

![The Simple Presenter operator window: libraries and playlists at the top left with the selected playlist's presentations below them, a grid of slide thumbnails framed in their group colours, output and stage previews on the right, and the media bin along the bottom](docs/screenshot.png)

As a fun experiment in vibe coding, I decided to make a simple, ProPresenter-compatible
desktop application for Linux. It is a native app, not Electron, so it performs well even
on older computers.

It reads and displays ProPresenter 7 `.pro` presentations, with a slide layer over a media
layer; there are no props, messages or announcements. Transitions are shaders, which keeps
them cheap: a dissolve, a plain cut, and twenty-one ported from
[gl-transitions](https://gl-transitions.com), such as wipes, warps, zooms and a ripple. It has two outputs, an
audience output and a stage display, each in its own window, and a media bin. It can also
import a playlist that has been exported from ProPresenter, and it has a simple
[editor](#editing) for the text boxes on a slide.

## TODO

- [ ] **Improve File Compatibility**: render more of what a `.pro` file can hold, such
      as gradients, shapes other than rectangles, image fills and text that scales to fit.
      Drawn so far: text with its fonts, colours, outline, shadow, capitals, underline
      and spacing; plain fills, including one that is only behind the lines of the text;
      strokes and shadows; elements that show only when another has text; and text
      linked from another element.
- [ ] **Editor**: text boxes can be added, moved, resized and removed, and their text
      and looks changed. Still to do: other kinds of element, adding and removing
      slides, picking several elements at once, rotating, lists and scrolling text.
- [ ] **Playlist Support**: build and run an ordered list of presentations and media
      for a service. Playlists and folders can be created, renamed, rearranged, removed
      and run, and their rows added, reordered and removed; still to do are headers and
      media rows.
- [x] **Import Playlists**: read ProPresenter's exported `.proplaylist` files, bringing
      in the playlist, its presentations and, when the export included it, its media.

Built with Qt 6 (C++ and QML). The ProPresenter file format comes from the unofficial
protobuf definitions in [ProPresenter7-Proto](https://github.com/greyshirtguy/ProPresenter7-Proto),
included as a submodule.

## Building

Needs Qt 6.9 or newer. On Ubuntu 26.04:

```
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-shadertools-dev \
    qml6-module-qtquick-controls qml6-module-qtquick-effects qml6-module-qtmultimedia \
    qml6-module-qtquick-dialogs qml6-module-qtquick-shapes \
    protobuf-compiler libprotobuf-dev libfontconfig-dev zlib1g-dev
```

Video plays without any of the drivers below, decoded on the CPU; see
[Hardware video decoding](#hardware-video-decoding) to move that work to the graphics
hardware.

```
git clone --recurse-submodules <this repository>
cd <this repository>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/SimplePresenter
```

## Hardware video decoding

Qt Multimedia decodes video through FFmpeg and uses the graphics hardware when a driver
for it is installed. Without one it falls back to the CPU, which works but costs more,
and more so with 4K media or several videos at once. Only the Intel case has been
tested with this app; the AMD and NVIDIA notes are what should apply.

| Graphics | What to install on Ubuntu | Notes |
|---|---|---|
| Intel (Broadwell, 2014, or newer) | `intel-media-va-driver` | Tested. Decodes through VA-API. Older Intel chips use `i965-va-driver`. |
| AMD | Nothing: the driver is part of Mesa (`mesa-libgallium`), installed with the desktop | Decodes through VA-API. On older Ubuntu releases it is the separate `mesa-va-drivers` package. |
| NVIDIA, proprietary driver | The driver's own decode library, `libnvidia-decode-<version>`, which the `nvidia-driver-<version>` package pulls in | Decodes through VDPAU or NVDEC, not VA-API. |
| NVIDIA, open-source nouveau driver | Nothing: also part of Mesa | Limited: only some older cards, and it needs firmware. Expect CPU decoding. |

To see what a machine can decode, install `vainfo` and run it: it lists the codecs the
VA-API driver offers, or fails if there is no driver. That does not apply to NVIDIA's
proprietary driver.

To see what the app itself chose, run it with Qt's multimedia logging on and play a
video:

```
QT_LOGGING_RULES="qt.multimedia.ffmpeg*=true" ./build/SimplePresenter 2>&1 | grep hwaccel
```

`Checking HW context: vaapi` followed by `Using above hw context` means that method is
available, and `Selected format ... for hw` means a video is being decoded with it.
`Could not create hw context` for every method means CPU decoding.

The lines FFmpeg prints at startup about VDPAU or Vulkan failing are it trying methods
the machine does not have, and are harmless.

## Workspaces

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
```

So a copy of a ProPresenter folder, dropped into `WorkSpaces`, is a workspace. It will
hold more than this (themes, presets, configuration), which the app leaves alone.
Changes made in the app (a new playlist, a presentation added to one, an arrangement
chosen, media dropped on a slide) are written to the files in the workspace.

Files are found by their path relative to the workspace first, so a workspace keeps
working when it is moved or copied from another machine; then by the path recorded for
them; then, for media, by name anywhere under `Media`.

The media bin shows the media playlists, as ProPresenter's does, not the folders on
disk. The first time the app opens a workspace with no media playlists file, it writes
one that mirrors the folders under `Media`: a playlist for each folder of media.

An exported playlist is imported from the "+" beside Playlists. Its presentations go
into the library last browsed and its media under `Media`, keeping the folders it had
below ProPresenter's own `Media` folder; files already there are left as they are.

`--workspace <dir>` opens a particular workspace folder, wherever it is; the folders
beside it are then the ones the picker offers. `--help` lists the other options.

## Editing

**Edit** in the toolbar, or **Edit** in a presentation's right-click menu, swaps the
slides for the editor; **Done**, or **Edit** again, goes back to showing. The output
carries on as it was while a presentation is edited, and shows a slide as edited the
next time that slide is shown.

On the left are the presentation's slides and, under them, the elements of the slide
being worked on, the one in front first. In the middle is the slide. On the right are
the properties of the element that is picked, in two parts, as ProPresenter has them:
**Shape** and **Text**.

- **Picking and arranging.** Click an element on the slide or in the list. Drag it to
  move it, or drag a handle to resize it; both snap to the slide's edges and middle and
  to the other elements, and show the line they have snapped to. The eye and the
  padlock in the list hide and lock an element, a double click there renames it, and a
  right click, there or on the slide, offers the rest: duplicate, delete, bring forward
  and send back.
- **Text.** Double-click an element to edit its text where it stands, drawn as it will
  be shown. Font, size, colour, bold, italic, underline, capitals, spacing and outline
  apply to the text that is selected, or to all of the text when none is.
- **Shape.** Position and size, opacity, fill (a colour, optionally only behind the
  lines of the text), stroke and shadow.
- **Visibility.** An element can be set to show only when other elements of the slide
  have text, or have none: all, any or none of a list of conditions.
- **Linked text.** A text box can show the text of another element of the slide, in
  its own font and colour, as it is or run together onto one line, or broken into a
  word or a letter to a line.

Every change is saved to the presentation file as it is made, and Undo takes it back
out. Only what was changed is touched: everything else in the file, including whatever
this app does not understand, is written back as ProPresenter wrote it. The first
change made to a presentation in an editing session is preceded by a copy of the file
as it was, in `~/.local/share/SimplePresenter/SimplePresenter/Edit Backups`, where the
twenty most recent for each presentation are kept.

Fonts are recorded by name, so text set in a font that is not installed here keeps its
font unless another is chosen for it; the editor marks such a font as missing, and a
substitute is used to draw it.

## Keys

| Key | Action |
|---|---|
| Right, Space / Left | Next / previous slide |
| Down / Up | Next / previous presentation |
| F1 / F2 / F3 | Clear all / slide layer / media layer |
| Ctrl+V | Show or hide the media bin |
| Ctrl+1 / Ctrl+2 | Show or hide the output / stage window |
| Esc | Close a menu |

In the editor:

| Key | Action |
|---|---|
| Arrows | Move the picked element by one unit, or by ten with Shift; with nothing picked, change slide |
| Page Up / Page Down | Previous / next slide |
| Enter | Edit the picked element's text |
| Esc | Finish editing text; then, let go of the picked element |
| Tab / Shift+Tab | Pick the next / previous element |
| Delete, Backspace | Delete the picked element |
| Ctrl+D | Duplicate the picked element |
| Ctrl+B / Ctrl+I / Ctrl+U | Bold / italic / underline |
| Ctrl+Z / Ctrl+Shift+Z, Ctrl+Y | Undo / redo |
| Shift while dragging | Move in a straight line; resize from a corner in proportion |
| Ctrl while dragging | No snapping |
| F1 / F2 / F3 | The clears, as when showing |

## Self-test

```
./build/SimplePresenter --selftest <dir>
```

drives the app through a fixed sequence (a slide, a transition, the clears, a simulated
trackpad swipe, then the editor brought up on a presentation with an element picked and
its text being edited), saves frames from each window into `<dir>` as PNGs, and quits.
It changes nothing in the workspace, neither reads nor changes saved settings, and opens
the first workspace unless `--workspace` names one.

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | Startup, command-line options, the self-test |
| `src/prodocument.*` | Reads `.pro` files for showing, and makes the changes show mode can |
| `src/proconvert.*` | Turns a slide in a `.pro` file into what is drawn, and changes back into the file's terms |
| `src/presentationeditor.*` | A presentation open in the editor: its changes, undo, saving and backups |
| `src/playlistfile.*` | Reads and writes the playlists file |
| `src/playlistimport.*`, `src/zipreader.*` | Imports exported `.proplaylist` archives |
| `src/richtext.*` | Styled text as the app works with it, and formatting part of it |
| `src/rtf.*`, `src/rtfwriter.*` | Reads and writes the RTF that slide text is stored in |
| `src/textlayout.*` | Lays text out, the same for drawing it and for editing it in place |
| `src/strokedtext.*` | Draws slide text with stroke and fill |
| `src/richtextbridge.*` | Lets a text box on the slide be typed into |
| `src/catalog.*` | The open workspace: its libraries, playlists and media |
| `src/thumbnailprovider.*` | Cached image and video thumbnails |
| `src/framerelay.*` | Feeds the preview from the output's video frames |
| `src/fontresolver.*` | Finds fonts by PostScript name through fontconfig |
| `qml/Main.qml` | The operator window |
| `qml/Editor.qml`, `qml/EditorCanvas.qml`, `qml/EditorInspector.qml` | The editor: its lists, the slide being worked on, and the properties panel |
| `qml/Slide.qml`, `qml/SlideElement.qml` | Draw a slide and one element of it |
| `qml/Output.qml`, `qml/Stage.qml` | The output and stage windows |
| `qml/TransitionLayer.qml` | One output layer with shader transitions |
| `shaders/` | The transitions; `shaders/gl-transitions` holds the ones ported from gl-transitions |

## Licence

SimplePresenter is free software, licensed under the GNU Lesser General Public License
version 3. See `COPYING.LESSER`, and `COPYING` for the GNU General Public License it
builds on.

It uses, under their own licences:

- [Qt](https://www.qt.io) 6, under the LGPL version 3, linked dynamically.
- [ProPresenter7-Proto](https://github.com/greyshirtguy/ProPresenter7-Proto), under the
  MIT licence.
- Transitions ported from [gl-transitions](https://gl-transitions.com), under the MIT
  licence: `shaders/ripple.frag` and everything in `shaders/gl-transitions`, where the
  licence text is. Each file credits its author.
- Protocol Buffers, FFmpeg (through Qt Multimedia) and fontconfig, as provided by the
  system.

ProPresenter is a trademark of Renewed Vision. This project is not affiliated with or
endorsed by them.
