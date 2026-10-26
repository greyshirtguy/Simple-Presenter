[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Blackmagic DeckLink

**Tried so far on one card.** What follows was built from Blackmagic's SDK and from what
ProPresenter does, and tested against outputs the app pretends, on a computer with no
card. It has since been tried on one computer with a card (a DeckLink Duo 2, with
Desktop Video 15.0), sending three screens through its outputs; keying has not been
seen to work yet.

A screen can go out through an output of a Blackmagic DeckLink card (a DeckLink Duo 2 is
the one it was made for), as an SDI signal, for a vision mixer, a recorder or a monitor.
Video only, for now.

**The outputs** are the ones Blackmagic's *Desktop Video Setup* has made of the card's
connectors, by the names it gives them; the app does not change how they are arranged,
and nor does ProPresenter. A pair of connectors linked there for fill and key is one
output, and the connector it takes for the key is not offered by itself while it is.
In a screen's menu of where it goes (Settings → Screens), each output is under
**Blackmagic**, and is a menu of the formats it can send: 1080p at 24, 25, 29.97, 30,
50, 59.94 and 60 frames a second, and 720p at 50, 59.94 and 60. Choosing a format is
choosing the output. The outputs are looked for again each time that menu is opened,
so a card arranged otherwise in Desktop Video Setup is seen without the app being
started again.

**Keying.** An output that can key, which a linked pair can, has ProPresenter's alpha
settings on the screen's line:

| Setting | What it does |
| --- | --- |
| **Alpha: None** | The screen goes out as it is, on black, as on any display. What an output that cannot key always sends |
| **Alpha: Premultiplied** | What nothing is drawn on is see-through, and the colours are as the app draws them, already darkened by how see-through they are |
| **Alpha: Straight** | The same, with the colours as they would be if they were not darkened: for a mixer that wants them so |
| **Key: Internal** | The card itself lays the screen over the video coming into it, and sends the two together |
| **Key: External** | The card sends the screen (the fill) out of one connector, and its opacity in black and white (the key) out of the other, for a vision mixer to key with |
| **Blend** | How strongly the card lays the screen over, from 0% to 100% |

Which alpha type a mixer wants is the mixer's to say; ProPresenter's help on alpha
keying explains the difference. That the app asks the card for the key after starting
its output, and the blend as a share of 255, is how Blackmagic's SDK reads, and has not
been seen to work.

**How it is sent.** The screen is drawn out of sight by the graphics card, as for NDI,
read back into a frame the DeckLink card made, and handed to it. Only when something
has changed: the card goes on showing the last frame it was given, so a still slide
costs nothing, and a moving one costs a picture read back for each frame that changes.
A frame goes out on the card's next frame after it is ready; with the card as the
Clock Source (below), it is made ready a little before that frame.

**Together.** Every screen going out through a card is drawn in the same passes: each
that has changed is drawn, then each is read back, then all are handed to their cards
at once. (Drawn one after another, each read back before the next was drawn, three
outputs on an older computer were seen to change one by one.) That they then go out on
the same frame of the cards depends on the cards' frames being lined up, which is not
known. Blackmagic's SDK can line outputs up exactly, but only for frames sent a few
frames ahead, at every frame, which would cost a delay and work on a still slide: that
is not done. The log's line for a screen that stops says how long its drawing took.

**The Clock Source.** A card sends its frames by a clock of its own (its crystal, or what
it is locked to), and the app draws by its own, at the format's rate: the two drift apart
a little at every frame, so that while a screen moves, now and then a frame is shown
twice or one is skipped. ProPresenter can have one of its outputs be the clock its
drawing keeps to, and so can this: **Clock Source**, under the screens in Settings →
Screens once a card's outputs have been found, is **Automatic**, the app's own clock, or
one of the outputs. With an output chosen, and while a screen is going out through it,
every screen going out through a card at its rate is drawn by its frames: once in each of
them, a little before it begins (by the longest the drawing has taken lately, and 8 ms
more for the frame to be handed over, which is a guess at what a card needs), and what
moves on the screens moves by them. The card's frames are read from the card itself, on
the thread that hands it its frames. A screen at another rate keeps the app's clock, and
so does every display, which is drawn at its own refreshes whatever this is set to. The
settings say what each screen is drawn by. Which output it is is this computer's, kept
with the app's settings, as ProPresenter keeps its clock source in its preferences; what
ProPresenter calls it is not known here, and the name is that of its preferences' field.
It has been tried against the outputs the app pretends for its tests, whose frames are
made two percent longer than their format's, and not yet on a card.

**Blackmagic's library is not part of this app.** It comes with *Desktop Video*, the
drivers for the card, from Blackmagic Design, which a computer with a card has installed
already; version 14.3 or later. The app looks for it the first time a screen's menu is
opened or a screen is set to go out through a card, and never otherwise, so on a
computer with no card it costs nothing. Without it, the settings say so.

The header files the app is built against are in `third_party/decklink/include`, under
Blackmagic's licence at the top of each.

DeckLink and Desktop Video are Blackmagic Design's. This app has nothing to do with
them, or with Renewed Vision.
