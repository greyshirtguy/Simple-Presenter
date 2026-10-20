[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# NDI

[NDI](https://ndi.video) sends video between programs and computers on a local network.
A screen set to **NDI** (Settings → Screens) is announced on the network under the name
given it there, after the computer's name, and anything that takes NDI can pick it: a
vision mixer, OBS, NDI's own Studio Monitor on another computer. It is the screen as a
display would show it, at the size and the frames a second set for it; it needs no
display, and a screen can go nowhere else.

The screen is drawn out of sight by the graphics card, read back and handed to NDI's
library to compress and send, all on this computer, which is work for it: 1920 × 1080 at
30 frames a second is a fair place to start, and on a modest laptop a good place to
stay. The work is only done while something is taking the source. With nothing taking
it, one picture goes out a second, enough for it to be found and looked at. The
settings say, for each such screen, whether it is on the network and how many are
taking it.

**NDI's library is not part of this app**, and does not come with it. It is NDI's own
and is not open, though it is free to have, under NDI's licence, which whoever has it
has to have agreed to themselves. Everything else in the app works without it.

**Getting it.** The first time a screen is set to NDI without the library, a panel comes
up with the two ways:

- **Let the app fetch it.** *Download NDI's Library* downloads NDI's own installer for
  Linux from NDI's site (about 60 MB), shows NDI's licence, and, if you agree to it,
  takes the library out and puts it where the app looks. Nothing is put anywhere if you
  do not. The screen is then on the network without the app being started again.
- **By hand**, on a computer that is not on the internet, or if you would rather:
  1. Get the *NDI SDK for Linux* from
     [ndi.video](https://ndi.video/for-developers/ndi-sdk/) and run the installer it
     gives you (`sh Install_NDI_SDK_v6_Linux.sh`), which shows NDI's licence and
     unpacks a folder.
  2. Copy `lib/x86_64-linux-gnu/libndi.so.6.x.x` from that folder to
     `~/.local/share/SimplePresenter/SimplePresenter/ndi/libndi.so.6` (the panel names
     the folder exactly, and has a button that opens it).
  3. Press *Look Again* in the panel, or start the app again.

A screen still without the library has a *Get NDI's Library…* button on its line in
the settings, which brings the panel back.

The app looks for `libndi.so.6` in the folder the environment variable
`NDI_RUNTIME_DIR_V6` names, which is what NDI's own installers set; then in its own
folder above; then wherever the system keeps its libraries.

The few header files that say how the library is talked to are in
`third_party/ndi/include`; those are NDI's under the MIT licence, as each says at its top.

Plain NDI is what is sent (full frames, lightly compressed, a good deal of network), not
NDI|HX. There is no sound in it yet, and nothing is received.

NDI® is a registered trademark of Vizrt NDI AB.
