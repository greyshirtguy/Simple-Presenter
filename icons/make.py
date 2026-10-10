#!/usr/bin/env python3
"""Makes the pictures in this folder from ProPresenter's icon pack.

    icons/make.py <the unpacked IconPack folder> [<where to write>]

See README.md beside this for where the pack comes from. It needs Pillow
(python3-pil).

Why the pictures are worked on at all, and not used as they come. A picture in the pack
is 72 pixels square with a drawing of about 26 pixels in the middle of it: the size
ProPresenter draws it at on a screen of ordinary density. This app also runs on screens
of twice that density, where a toolbar icon is some 30 to 40 real pixels high, and a
26-pixel drawing made bigger by the graphics chip is soft at the edges beside the text
next to it. So each drawing is made bigger here, once, with care:

  1. the middle 40 by 40 pixels are cut out. That holds every drawing in the pack, and
     cutting all of them alike keeps them the sizes they are beside each other;
  2. how solid the drawing is, which is all that most of them are (one colour, more or
     less see-through), is stretched to run from nothing to fully solid. The pack draws
     in white at about three quarters strength; here the app says what colour and how
     bright (see IconProvider), so the picture is just the shape;
  3. it is made eight times the size, smoothly, and then its edges are drawn tighter:
     the band over which an edge goes from nothing to solid, which the enlarging
     spreads over several pixels, is narrowed about its middle. That keeps the shape
     and gives a clean edge. It is not done so hard as to lose the half-tones that are
     part of some drawings (the ring round the cross of a clear button);
  4. it is brought down to four times the size, 160 by 160, and saved. The app scales
     that down to what it needs, which is always smaller.

A drawing with colours of its own (the green screen) keeps them; the others are saved
white, to be tinted.
"""
import os
import sys

from PIL import Image

# The pictures the app uses, by the name each has in the pack. Add a name here and run
# this again to have another.
NAMES = [
    "Clear", "ClearAnnouncements", "ClearAudio", "ClearMedia", "ClearMessages", "ClearPresentation", "ClearProps",
    "ClearVideoInput", "Countdown", "Looks", "Macro", "Message", "Prop", "Screens", "ScreensOn", "Stage",
    "Transport Pause", "Transport Play",
]
# The middle of a picture, which holds every drawing in the pack
BOX = (16, 16, 56, 56)
SIDE = BOX[2] - BOX[0]
WORKED_AT = 8
KEPT_AT = 4
# How much steeper an edge is made, about half solid
TIGHTER = 2.0


def coloured(picture):
    """Whether a picture is drawn in more than one colour (most are white throughout)."""
    seen = set()
    for red, green, blue, alpha in picture.get_flattened_data() if hasattr(picture, "get_flattened_data") else picture.getdata():
        if alpha > 200:
            seen.add((red // 32, green // 32, blue // 32))
    return len(seen) > 1


def made(path):
    cut = Image.open(path).convert("RGBA").crop(BOX)
    solid = cut.getchannel("A")
    most = max(solid.getextrema()[1], 1)
    big = (SIDE * WORKED_AT, SIDE * WORKED_AT)
    solid = solid.point(lambda value: min(255, round(value * 255 / most))).resize(big, Image.BICUBIC)
    solid = solid.point(lambda value: max(0, min(255, round((value / 255 - 0.5) * TIGHTER * 255 + 127.5))))
    if coloured(cut):
        # Colours are enlarged with how solid they are counted in, or the colour of what
        # is see-through (which is no colour at all) would creep into the edges.
        colours = cut.convert("RGBa").resize(big, Image.BICUBIC).convert("RGBA").convert("RGB")
    else:
        colours = Image.new("RGB", big, (255, 255, 255))
    whole = Image.merge("RGBA", (*colours.split(), solid))
    return whole.resize((SIDE * KEPT_AT, SIDE * KEPT_AT), Image.LANCZOS)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    pack = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))
    for name in NAMES:
        # (No spaces in a name the app asks for.)
        made(os.path.join(pack, name + ".png")).save(os.path.join(out, name.replace(" ", "") + ".png"), optimize=True)
    print("%d pictures written to %s" % (len(NAMES), out))


if __name__ == "__main__":
    main()
