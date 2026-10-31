#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>

// The shadow of words, drawn into their own picture, as Qt's MultiEffect draws a shadow
// over a layer (SlideElement's Shadow, which draws the words' shadow wherever they are not
// painted ahead: see StrokedText): what the picture covers, blurred, in the shadow's colour,
// moved by its offset, and put under it. Plain pictures, with nothing of the windows in
// them, so that it can be tried by itself (tests/unit/tst_textshadow).
//
// For speed, as it is drawn for a slide's words before they are shown, on a thread of
// their own. A blurred shadow has nothing finer in it than its blur, so it is worked out at
// a fraction of the picture's size where the blur is wide (a half, a quarter, an eighth: a
// quarter of the pixels or fewer) and stretched to the picture, smoothly, as it is put
// under it; and three box blurs, which cost the same however wide they are, stand for the
// Gaussian. About 5 ms for a slide's words on a 1080p screen (1844 by 564 pixels, a sigma
// of 2.4), on the laptop this is made on, where painting the words took about 6: 1.5 to
// find what the words cover, 1.1 to blur it, 0.2 to colour it and 2.2 to put it under them
// (measured 2026-10-31, the cover then kept in eight bits, as sixteen now are: a little
// more to blur).
//
// How near it comes to MultiEffect's (2026-10-31: the same slide drawn both ways on the
// tests' desktop, over black): as dark on the whole, 17.8 of 255 against MultiEffect's 17.7
// where no letter covers it, and no pixel more than a quarter different but on the
// letters' own soft edges, which MultiEffect draws a pixel aside. MultiEffect's blur also
// leaves a faint tail further out than three of its sigmas, at most a sixteenth of the
// shadow's strength, which this does not draw. And a wide, soft shadow comes out a little
// lighter than it should: a pixel's opacity is a whole number of 0 to 255, and Qt's
// smooth stretching of the shadow to the picture rounds a faint one down, so that the
// shadow of a stroke twelve pixels broad at a sigma of 12 has nine tenths of its strength
// (at the sigmas of 2 or 3 of a lyric's shadow, within a twentieth). tests/unit/tst_textshadow.
namespace textshadow {

// How much Qt's MultiEffect blurs, as a Gaussian's sigma in pixels, at a "radius" (its blur
// times its blurMax): measured at the radii of its table, between which it is read off in
// a straight line, and no more than at the last (TextFills.qml's measuredBlurs, which says
// how it was measured: 2026-10-29)
double multiEffectSigma(double radius);

// Puts under what `picture` holds the shadow of it: what it covers, blurred as a Gaussian of
// `sigma` blurs, in `color` (whose opacity is the shadow's), moved by `offset`; both in the
// picture's own pixels. Nothing for a colour with no opacity. What is beyond the picture's
// edges, the shadow's included, is cut off: the picture is to have room round what it
// holds for the shadow (SlideElement's textBleed).
void cast(QImage &picture, const QColor &color, QPointF offset, double sigma);

} // namespace textshadow
