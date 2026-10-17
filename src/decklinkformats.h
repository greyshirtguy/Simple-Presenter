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
// and a frame from now if it was not (it comes after a while of nothing)
qint64 nextAfter(const Waiting &screen, qint64 now);

}
