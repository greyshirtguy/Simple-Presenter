# SimplePresenter

An experiment in vibe coding a simple, ProPresenter-compatible presentation app that
runs natively on Linux (Windows and macOS maybe later).

It reads and shows ProPresenter 7 `.pro` presentations, with a slide layer over a media
layer, shader transitions, a stage display and a media bin.

Built with Qt 6 (C++ and QML). The ProPresenter file format comes from the unofficial
protobuf definitions in [ProPresenter7-Proto](https://github.com/greyshirtguy/ProPresenter7-Proto),
included as a submodule.

## Building

Needs Qt 6.9 or newer. On Ubuntu 26.04:

```
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-shadertools-dev \
    qml6-module-qtquick-controls qml6-module-qtquick-effects qml6-module-qtmultimedia \
    protobuf-compiler libprotobuf-dev libfontconfig-dev
```

For hardware video decoding on Intel graphics, also `intel-media-va-driver`. Without a
VA-API driver video is decoded on the CPU.

```
git clone --recurse-submodules <this repository>
cd <this repository>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/SimplePresenter
```

## Content

The app reads from `~/Documents/SimplePresenter`, created on first run:

```
Libraries/<library name>/*.pro    presentations, one flat folder per library
Media/...                         images and videos, in any depth of folders
```

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
