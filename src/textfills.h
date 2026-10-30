#pragma once

#include "richtext.h"

#include <QList>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace rv::data {
class Media;
class Slide;
// (Graphics::Text::Attributes, as protoc names the nested message)
class Graphics_Text_Attributes;
}

// What ProPresenter can fill a text's letters with other than a colour, read from the file
// into the text the slides are drawn from (RichText: each run's `fillEffect`, and the
// text's `fillEffects`).
//
// ProPresenter keeps a text's fill beside its RTF, in the text's attributes: a `fill` for
// the whole text, and the same over ranges of it (custom attributes), as it keeps
// capitalization. Its fills, and what ProPresenter draws for each (KNOWN, from a
// presentation the maintainer made for the purpose and ProPresenter's own pictures of
// its five slides, 2026-10-28):
//
//   - a colour (`text_solid_fill`), which is the RTF's own for each run: not read here;
//   - a gradient (`text_gradient_fill`), across the letters it fills: the demo's gradient
//     over one line, at an angle of 245 degrees, ran from one end of the line to the
//     other, so it is laid over the box of the letters of its range, not over the text
//     box (which would have shown a quarter of it across the line);
//   - a picture (`media_fill`), over the whole text box, fitted as its media says (the
//     demo's picture, Scale to Fill, showed in each letter the part of it that is there),
//     drawn as a picture fill of a shape is, its Media Inspector settings and all;
//   - a cut-out (`cut_out_fill`): the letters are holes in the box's own fill, through
//     which what is behind the box shows (the demo's box filled the slide in black, and
//     its letters showed the background picture);
//   - a background effect (`background_effect`): a blur of what is behind the letters
//     (`backgroundBlur`: its `blur_amount` of 0.2648 was a blur of a standard deviation of
//     about 6.9 slide units, measured against ProPresenter's picture, about 26 for each
//     whole of it; its `saturation`, not written there, left the colours as they are), or
//     an invert of it (`backgroundInvert`).
//
// Each fill is described as a map, in the text's `fillEffects`, by the number its runs'
// `fillEffect` refer to:
//
//   { kind: "gradient", angle, radial, stops: [{ color, position }], stretch (over the
//     text's whole box, and not its letters) }
//   { kind: "media", source (a file: url, empty if the file is not found), scale (0 fit,
//     1 fill, 2 stretch), settings (the Media Inspector's, only where it sets more:
//     mediasettings.h) }
//   { kind: "cutOut" }
//   { kind: "blur", amount, sigma (its standard deviation in slide units: blurSigma),
//     saturation (1 for as they are) }
//   { kind: "invert" }
//
// A fill read from ranges also says which of them it was first read from, `from` (its
// number among the attributes' custom attributes); one read from the text's own fill
// says `whole`. They are what write() makes the file's fills again from, as copies of
// what the file had, everything in them kept. A fill the editor sets says neither, and is
// written afresh; one it changes says what it was read from, and is written as a copy of
// that with the change made. For writing, a media fill may also say `file`, a file for
// it to be filled with, and `mediaSettings`, what the Media Inspector changed of it.
//
// Ranges count characters through the whole text, a paragraph break counting as one, as
// capitalization's do (proconvert.cpp). Where the file has ranges of such fills, they
// are what is read; where it has none, the whole text's fill. (KNOWN, the demo:
// ProPresenter writes such a fill over the whole text and over a range of all of it
// alike.)

namespace textfills {

// Lays the fills the attributes give onto `text`, read from the RTF: each run covered by
// a fill gets its number. `mediaFile` finds the file a media fill names, or empty.
void apply(RichText *text, const rv::data::Graphics_Text_Attributes &attributes,
           const std::function<QString(const rv::data::Media &)> &mediaFile);

// Makes a media file into media, for a media fill (proconvert: workspace::mediaElement)
using MakeMedia = std::function<rv::data::Media(const QString &path)>;

// Writes the text's fills into `attributes` as `text`'s runs have them (proconvert's
// writeText, for a text that carries its fills): its ranges, added to its custom
// attributes (which hold none of fills when this is called), and its own fill. `before`
// is the attributes as the file had them.
//
// Each stretch of words with one fill is a range of it: a copy of the range the fill was
// read from, its numbers kept where it still covers the same words (ProPresenter's range
// of a whole text runs one past its end), with only what differs from the fill set; or
// the text's own fill as a range, where the fill was that and covers only some of the
// words now; or a range made afresh. So a text not changed is written as it was, byte for
// byte, and one changed keeps all the file had that this app knows nothing of.
//
// The text's own fill: with no words filled, the colour of the words (as it was always
// written); with all of them filled with one fill, that fill, as well as its range over
// them all, as ProPresenter writes it (KNOWN: its demo's whole texts had both); where the
// file had that fill as the text's own and no range, the text's own alone, as it was; with
// some filled and some not, the colour of those that are not (A GUESS: ProPresenter's own
// file was not seen for that); with all filled, with more than one fill, as it was
// (ProPresenter's, for a text of a gradient and an invert, had none).
void write(const rv::data::Graphics_Text_Attributes &before, rv::data::Graphics_Text_Attributes *attributes,
           const RichText &text, const MakeMedia &makeMedia);

// Sets the amount of every blur of what is behind on a slide to `amount`: its texts'
// fills and its shapes'. ProPresenter's editor says, under its Blur slider, "Blur
// percentage is shared among all objects on a slide" (KNOWN, from its Text tab): the
// editor does as it says, when a blur is changed. (Its saturation is each one's own.)
void shareBlur(rv::data::Slide *slide, double amount);

// The standard deviation, in slide units, of ProPresenter's blur of this amount
double blurSigma(double amount);

// Whether a text has fills of the kinds drawn over the letters by the graphics chip
// (all but a gradient, which is drawn with the letters): the fills, each with its number
// as `index`, for the slide's drawing to make a layer for (SlideElement, TextFills)
QVariantList layered(const RichText &text);

} // namespace textfills
