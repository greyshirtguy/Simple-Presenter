[Simple Presenter](../README.md)

# Installing

Being packaged does not make it a product. The warning at the top of the [README](../README.md) holds for
the package as much as for the code: it is an experiment, shared as an example of what
vibe coding can do and passed on as it is, not something made for use. The package is
for seeing it run without building it first.

There is a package for Ubuntu 26.04 on ordinary (64-bit Intel or AMD) computers, which
is what the app is made and tested on, with the standard desktop. Download
`simplepresenter_0.102_amd64.deb` from the
[Releases](https://github.com/greyshirtguy/Simple-Presenter/releases) page and install it:

```
sudo apt install ./simplepresenter_0.102_amd64.deb
```

That also installs what it needs, from Ubuntu's own packages, and puts Simple Presenter
among the applications. `sudo apt remove simplepresenter` takes it off again.

The package holds only this program, and is about 3 MB. Qt, FFmpeg and the video
drivers are the system's own, which is why it is small, why video is decoded by whatever
the machine's drivers can do (see [Hardware video decoding](#hardware-video-decoding)),
and also why it is tied to one release: it is built against the Qt that Ubuntu 26.04
ships (6.10), and will not install where Qt is older, as on Ubuntu 24.04, or has moved
on. Anywhere else, build it from source, which takes a few minutes.

## The first run

The app keeps everything in workspaces, under `~/Documents/SimplePresenter/WorkSpaces`,
and the first time it starts it makes an empty one called `Default`. To give it
something to show, either

- copy your ProPresenter folder (`Documents/ProPresenter` on a Mac or on Windows) into
  `WorkSpaces`, and pick it from the workspace picker at the left of the toolbar (a
  copy, always: the app changes what it works on); or
- put some `.pro` files into a folder of their own inside `WorkSpaces/Default/Libraries`,
  and some images or videos into `WorkSpaces/Default/Media`.

The app works on that copy and saves its changes into it; [Workspaces](features/workspaces.md) has
the details.

## Building from source

Nothing here is specific to Ubuntu except the names of the packages. It needs Qt 6.9
or newer.

**1. Install the tools and the libraries.**

```
sudo apt install git cmake ninja-build g++ pkg-config \
    qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-shadertools-dev \
    qml6-module-qtquick-controls qml6-module-qtquick-effects qml6-module-qtmultimedia \
    qml6-module-qtquick-dialogs qml6-module-qtquick-shapes qt6-image-formats-plugins \
    protobuf-compiler libprotobuf-dev libfontconfig-dev zlib1g-dev libxcb1-dev \
    libavformat-dev libavcodec-dev libswscale-dev libavutil-dev libasound2-dev
```

| These | are for |
|---|---|
| `git`, `cmake`, `ninja-build`, `g++`, `pkg-config` | Fetching the code and building it |
| `qt6-…-dev` | Qt 6, which the app is written with: windows, drawing, video playback, and the tool that compiles the transition shaders |
| `qml6-module-…` | The parts of Qt Quick that the app loads when it starts, such as buttons and effects |
| `qt6-image-formats-plugins` | Reading pictures in the formats Qt does not have built in, WebP among them |
| `protobuf-compiler`, `libprotobuf-dev` | ProPresenter's files are Protocol Buffers. The build turns the descriptions of the format into C++ that reads and writes it |
| `libfontconfig-dev` | Finding fonts by the names ProPresenter knows them by |
| `zlib1g-dev` | Unpacking exported playlists, which are zip archives |
| `libav…-dev`, `libswscale-dev` | FFmpeg, for taking a frame from a video as its thumbnail |
| `libxcb1-dev` | X11's own library, for keeping the output and stage windows out of Alt+Tab when the app is run through X11 |
| `libasound2-dev` | ALSA, which a Linux desktop has MIDI through, for MIDI devices |

**2. Get the code.**

```
git clone --recurse-submodules https://github.com/greyshirtguy/Simple-Presenter.git
cd Simple-Presenter
```

`--recurse-submodules` also fetches
[ProPresenter7-Proto](https://github.com/greyshirtguy/ProPresenter7-Proto), the
unofficial descriptions of ProPresenter's file formats that the app is built on. If the
build complains that `.proto` files are missing, that step was skipped:
`git submodule update --init` does it afterwards.

**3. Build it.**

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

The first line checks that everything in step 1 is there and sets the build up in a
folder called `build`; the second compiles. It takes a few minutes the first time, most
of it spent on the code generated for ProPresenter's formats. The result is one file,
`build/SimplePresenter`, with the interface and the shaders compiled into it.

A key for [api.bible](features/bibles.md) is not in the repository: a key is for whoever
registered for it, and its use is counted against them. To build one in, so that
api.bible's Bibles are there with nothing to set up, put the key in the file
`packaging/api-bible-key.txt` (git leaves that file alone), or give it as
`-DSIMPLEPRESENTER_API_BIBLE_KEY=…` on the first line above, and build again. It can
be read out of the program by anyone who has it, and everyone who has the program
shares its allowance of requests. Without one, a key of one's own can be typed in the
settings instead.

**4. Run it.**

```
./build/SimplePresenter
```

See [The first run](#the-first-run) for giving it something to show, and
`./build/SimplePresenter --help` for its options. Video plays with or without a driver
for decoding it on the graphics hardware; [Hardware video decoding](#hardware-video-decoding)
says how to get one.

**5. Look around.** [How it works](how-it-is-made.md#how-it-works) is the short version, and
`src/main.cpp` opens with a tour of the code that says where everything is. The sources
are written to be read: each file starts by saying what it is for and why it is the way
it is. To browse or change them in an editor that understands the project, open
`CMakeLists.txt` in Qt Creator. After a change, `cmake --build build` again rebuilds
only what the change touched, and the [self-test](how-it-is-made.md#self-test) shows whether anything
that is drawn has moved.

**6. Make the package,** if you want one to install or to pass on:

```
cd build && cpack
```

writes the same `.deb` as on the Releases page. The program in it has had its names
taken out, which is most of what keeps it small, so a crash's account of where the app
was (see [Log](features/log.md)) gives the places inside the app as numbers. The list that turns
those numbers back into names is made from the program as it was built, and belongs
with the package it was made for:

```
nm -C -n --defined-only SimplePresenter | xz > simplepresenter_0.102_symbols.txt.xz
```

A place such as `SimplePresenter(+0x8ae5ac)` is in the function on the last line of
that list whose number is not greater than `8ae5ac`.

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
| NVIDIA, open-source nouveau driver | Nothing: also part of Mesa | Limited: only some older cards (up to the GTX 780), and it needs firmware that has to be taken out of NVIDIA's own driver. Expect CPU decoding. Where the firmware is missing, the trying itself can hang the graphics chip, so the app does not try: see [the processor alone](#decoding-by-the-processor-alone), below. |

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

The app's own [log](features/log.md) has a line for each video that is played which says how its
frames arrive: as textures, which is a video decoded by the graphics chip, or in
memory, which is one decoded by the processor.

The lines FFmpeg prints at startup about VDPAU or Vulkan failing are it trying methods
the machine does not have, and are harmless.

### Decoding by the processor alone

Which decoder a video gets is Qt's choice, made anew for every video that is opened: a
hardware decoder if the graphics driver offers one, and FFmpeg's own, on the processor,
if not. A driver can offer one that does not work, and then it is the trying that does
the harm. The case this was added for: an NVIDIA card of 2012 (a GTX 670) on the
nouveau driver, whose video decoder wants firmware the system does not have. The first
video played, the next showed nothing, and the graphics chip then stopped answering;
the kernel's log spoke of the missing firmware, of a `CTXSW_TIMEOUT`, and of killing
the channel that FFmpeg's H.264 decoder had opened. With no hardware decoder tried,
none of that happened.

**The app looks for that case by itself.** As it starts, before anything is played, it
looks at the graphics chip a hardware decoder would be looked for on (the first of the
system's render nodes, which is the one FFmpeg takes). If that chip is run by nouveau,
is one nouveau has a video decoder for (the cards of 2007 to 2014, up to the GTX 780),
and the firmware that decoder needs is not in the system's firmware folders, no
hardware decoder is tried: every video is decoded by the processor. The first time
that happens a notice says so, once, with a button that leads to the settings. The
[log](features/log.md) says near its start what was found and what was decided.

**Settings, Video** has the choice, of three:

- **Automatically**, which is the above: the graphics chip where its driver offers a
  decoder, and the processor on a computer where that decoder is known to hang.
- **By the processor only**: no hardware decoder is tried, on any computer. For one on
  which a video shows nothing, or which hangs when a video starts or is changed for
  another, whatever its graphics: switch to this, start the app again, and see whether
  that goes away.
- **By the graphics chip, whatever the app makes of this computer**: hardware decoders
  are tried even where Automatically would leave them alone. For a computer the app is
  wrong about.

A change takes effect when the app is next started, and the settings say how the run
that is up is decoding, and what the app makes of the computer. On the command line,
`--software-decoding` and `--hardware-decoding` do the same as the second and the third
for one run.

Nothing else changes, whichever it is. The pictures of a video are still drawn by the
graphics chip, through OpenGL, as textures, with the same transitions; only the working
out of them moves to the processor, which costs its time: a little over one core for a
4K video and under half of one for 1080p, measured on the 2017 laptop this is made on.

How it is done: the variable Qt's FFmpeg backend reads for the hardware decoders it may
try, `QT_FFMPEG_DECODING_HW_DEVICE_TYPES`, is given a list that names none, before
anything is played. Whoever starts the app can set that variable themselves, to nothing
or to the decoders they want tried (`vaapi`, say); the command line and the setting
come before it, and where it is set the app's own looking is left out of it. So a
version of the app that has no such setting can be tried the same way:

```
QT_FFMPEG_DECODING_HW_DEVICE_TYPES=, SimplePresenter
```

The log's line for each video says how its pictures arrived. With hardware decoding not
tried it says outright that the processor decoded it. Otherwise "in memory" leaves that
open, and the format beside it settles it for the usual kind of video: `NV12` in memory
is a video the graphics chip decoded and handed back, `YUV420P` one the processor
decoded.

**To have such a card decode after all**, its firmware has to be put in, which only
NVIDIA's own driver comes with: the nouveau project's page on
[video acceleration](https://nouveau.freedesktop.org/VideoAcceleration.html) says how
it is taken out of that driver and where it goes (`/lib/firmware/nouveau`). With it in
place the app tries the decoder again without anything being changed. That has not been
tried here, and nouveau's own page speaks of pictures that come out wrong on some
videos: if it goes badly, By the processor only is the way back.

What is a guess: that the firmware being there makes the decoder safe to try; the
names of the firmware for the oldest of those cards; and how the driver is asked which
chip it runs (if that goes unanswered, the guard still holds). It was worked out from
one computer's logs and from reading, on a laptop that has no such card.
