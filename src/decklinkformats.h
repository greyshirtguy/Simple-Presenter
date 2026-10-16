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

}
