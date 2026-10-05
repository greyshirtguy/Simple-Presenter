# Simple Presenter

![The Simple Presenter operator window: libraries and playlists at the top left with the selected playlist's presentations below them, a grid of slide thumbnails framed in their group colours, output and stage previews on the right, and the media bin along the bottom](docs/screenshot.png)

As a fun experiment in vibe coding, I decided to make a simple, ProPresenter-compatible
desktop application for Linux. It is a native app, not Electron, so it performs well even
on older computers.

It reads and displays ProPresenter 7 `.pro` presentations, with a slide layer over a media
layer; there are no props, messages or announcements. Transitions are shaders, which keeps
them cheap: a dissolve and a ripple for now, plus a plain cut. It has two outputs, an
audience output and a stage display, each in its own window, and a media bin. It can also
import a playlist that has been exported from ProPresenter.

## TODO

- [ ] **Improve File Compatibility**: render more of what a `.pro` file can hold, such
      as gradients, shapes other than rectangles, image fills and text that scales to fit.
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

## Content

The app reads from `~/Documents/SimplePresenter`, created on first run:

```
Libraries/<library name>/*.pro    presentations, one flat folder per library
Media/...                         images and videos, in any depth of folders
Playlists/Library                 playlists and playlist folders, in ProPresenter's format
```

This is the layout of ProPresenter's own folder, so `--root <dir>` can point the app at
a copy of one. Changes made in the app (a new playlist, a presentation added to one, an
arrangement chosen, media dropped on a slide) are written to the files in that folder.

An exported playlist is imported from the "+" beside Playlists. Its presentations go
into the library last browsed and its media under `Media`, keeping the folders it had
below ProPresenter's own `Media` folder; files already there are left as they are.

`--root <dir>` points it at a different folder. `--help` lists the other options.

## Keys

| Key | Action |
|---|---|
| Right, Space / Left | Next / previous slide |
| Down / Up | Next / previous presentation |
| F1 / F2 / F3 | Clear all / slide layer / media layer |
| Ctrl+V | Show or hide the media bin |
| Ctrl+1 / Ctrl+2 | Show or hide the output / stage window |

## Self-test

```
./build/SimplePresenter --selftest <dir>
```

drives the app through a fixed sequence (a slide, a transition, the clears, a simulated
trackpad swipe), saves frames from each window into `<dir>` as PNGs, and quits. It
neither reads nor changes saved settings.

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | Startup, command-line options, the self-test |
| `src/prodocument.*` | Reads and writes `.pro` files |
| `src/playlistfile.*` | Reads and writes the playlists file |
| `src/playlistimport.*`, `src/zipreader.*` | Imports exported `.proplaylist` archives |
| `src/rtf.*` | Parser for the RTF that slide text is stored in |
| `src/strokedtext.*` | Draws slide text with stroke and fill |
| `src/catalog.*` | Libraries and media found on disk |
| `src/thumbnailprovider.*` | Cached image and video thumbnails |
| `src/framerelay.*` | Feeds the preview from the output's video frames |
| `src/fontresolver.*` | Finds fonts by PostScript name through fontconfig |
| `qml/Main.qml` | The operator window |
| `qml/Output.qml`, `qml/Stage.qml` | The output and stage windows |
| `qml/TransitionLayer.qml` | One output layer with shader transitions |
| `shaders/` | The transitions |

## Licence

SimplePresenter is free software, licensed under the GNU Lesser General Public License
version 3. See `COPYING.LESSER`, and `COPYING` for the GNU General Public License it
builds on.

It uses, under their own licences:

- [Qt](https://www.qt.io) 6, under the LGPL version 3, linked dynamically.
- [ProPresenter7-Proto](https://github.com/greyshirtguy/ProPresenter7-Proto), under the
  MIT licence.
- The ripple transition in `shaders/ripple.frag`, ported from
  [gl-transitions](https://gl-transitions.com), under the MIT licence.
- Protocol Buffers, FFmpeg (through Qt Multimedia) and fontconfig, as provided by the
  system.

ProPresenter is a trademark of Renewed Vision. This project is not affiliated with or
endorsed by them.
