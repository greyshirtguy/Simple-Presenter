#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

// What a screen sent out through a Blackmagic DeckLink card is sent as (see decklink.h):
// the video formats offered for one, and what is done to its pictures on the way.
//
// The formats. ProPresenter offers a long list under each card output; these are the
// ones the maintainer asked for, as the card is asked for them: 1080 lines at 24, 25,
// 29.97, 30, 50, 59.94 and 60 frames a second, and 720 at 50, 59.94 and 60, all
// progressive. (720 at 25 and 30 were asked for too; Blackmagic's SDK has no such
// formats, so they are not here.) Each is known by the name the settings show
// ("1080p29.97"), which is also what is kept in the app's settings, and has the code the
// SDK gives it, which is written out here, as the SDK writes it, so that these rules do
// not need the SDK to be built or tested (decklink.cpp checks that the two agree).
//
// The pictures. The app draws a screen with the colours of what is see-through already
// multiplied by how see-through it is ("premultiplied"), which is what a vision mixer
// keying a fill over a picture mostly wants, and what ProPresenter calls Premultiplied.
// Straight is the colours as they would be if they were not: each pixel's colour divided
// by its opacity, which unpremultiplied() does, on the four bytes a pixel the card is
// handed (blue, green, red and opacity, in that order). A pixel nothing is drawn on has
// no colour either way.
//
// The beat. Every screen going out through a card is drawn in the same passes, so that
// outputs change together rather than one after another (see decklink.h); these are the
// rules of when. A screen is drawn when something in it has changed, at most once a
// frame of its format, on the beat of the frames drawn before it, and a pass is due when
// the soonest of the screens waiting to be drawn is. It takes with it every other screen
// waiting that would be due within half a frame of its own: drawn a little early and in
// step, rather than on its own a moment later. Screens of one format share their beat
// from their first pass together, and so are always drawn together; the half frame is
// for formats of different rates. One drawn early keeps its own beat, so it is never
// drawn oftener than its format's frames come. And a screen that has changed but whose
// card has not yet given back a frame to draw into (the card keeps one while it is
// handed over, which can take it up to a frame) is waited for, half a frame at most,
// so as to be drawn with the others rather than alone a moment after them: once one
// screen is drawn on its own, it stays out of step with the rest until the next pass
// that waits for it. (Seen with a stand-in for the card that was slow to give frames
// back: the screens fell into alternate passes.)
//
// The card's frames as the clock. That beat is the app's own clock's, at the format's
// rate, and a card sends its frames by a clock of its own (its crystal, or what it is
// locked to): the two drift apart, a little each frame, so that while a screen moves the
// card now and then shows a frame twice, or a frame comes too late for its turn and the
// one after it is skipped. ProPresenter lets one of its outputs be the "clock source" for
// its drawing (in its Screens settings: its words for that are not known here, and the
// setting is called Clock Source after the field of its preferences that keeps it), and
// so can this app, for its screens going out through a card. While one of the card's
// outputs is that, and is sending, every screen going out through a card at the same
// rate is drawn by its frames: each is drawn `lead` before one of the card's frames
// begins, as read from the card (CardClock), and never twice in one of its frames. A
// screen of another rate keeps the app's beat. (A window on a display is drawn by the
// display's own refreshes, whatever this is set to: see FrameClock.)
namespace decklinkformats {

struct Format
{
    QString name;
    int width = 0;
    int height = 0;
    // Frames a second, as a fraction (30000 over 1001 for 29.97)
    int rateNumerator = 0;
    int rateDenominator = 1;
    // The SDK's code for it (its BMDDisplayMode)
    quint32 mode = 0;
};

// Every format offered, in the order the menu offers them
const QList<Format> &formats();

// The format of that name, or null
const Format *formatNamed(const QString &name);

// Turns pixels of blue, green, red and opacity, premultiplied, into straight ones, in
// place: `pixels` of them, four bytes each.
void unpremultiplied(uchar *pixels, qsizetype count);

// A frame of the format, in nanoseconds (33,366,666 for 29.97)
qint64 period(const Format &format);

// A screen waiting to be drawn (something in it has changed): when it may next be, how
// long a frame of its format is (in nanoseconds, on one clock for all), and whether its
// card has a frame free to draw it into
struct Waiting
{
    qint64 next = 0;
    qint64 period = 0;
    bool ready = true;
    // Kept to a card's frames (onCard): drawn only at its moment, `next`, and not with
    // another a little early
    bool onCard = false;
};

// When the next pass is due: when the soonest of these that has a frame free may be
// drawn, or, if one that would be drawn with it has no frame free yet, half a frame of
// that one's later (when its frame comes back sooner, it is due again at once); -1 if
// none has a frame free
qint64 passDue(const QList<Waiting> &waiting);

// Whether a pass at `now` draws this one: it is due, or would be within half a frame
// (and has a frame to be drawn into)
bool drawnInPass(const Waiting &screen, qint64 now);

// When a screen drawn at `now` may next be drawn: a frame on from when it was due, if it
// was drawn within a frame of that (on the beat of the frames before, or a little early),
// and a frame from now if it was not (it comes after a while of nothing). One kept to a
// card's frames, half a frame from now: so that its next moment is in the card's next
// frame, whichever way the card's frames have drifted from the format's.
qint64 nextAfter(const Waiting &screen, qint64 now);

// A card's own frames, as read from it at a moment of the passes' clock (`at`): how far
// into one of its frames it was then, and how long its frames are (by its own clock: the
// format's length, near enough, and what keeps the reading true for a while), all in
// nanoseconds
struct CardClock
{
    qint64 at = 0;
    qint64 inFrame = 0;
    qint64 period = 0;
};

// How late a pass can be woken and still be the one for its moment: a timer's lateness,
// and the GUI thread finishing what it was doing (the lead has room for it)
constexpr qint64 kLate = 3'000'000;

// A screen waiting, kept to the card's frames: due `lead` before the first of the card's
// frames that is to begin at or after when it may be drawn (and a moment ago, up to
// kLate, is still now), and marked as kept to them
Waiting onCard(Waiting screen, const CardClock &card, qint64 lead, qint64 now);

// How long before a card's frame begins its pass is to be: the longest a pass has taken
// lately, and 8 ms for handing the frame over, which with a 1080 frame is 8 MB on its
// way to the card (a GUESS at what a card needs: the SDK's guide does not say how long
// before its frame a frame handed over synchronously must come); no less than 4 ms, and
// no more than three quarters of a frame
qint64 leadFor(qint64 longestPass, qint64 period);

}
