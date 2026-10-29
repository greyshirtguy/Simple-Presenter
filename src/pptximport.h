#pragma once

#include "pptxdeck.h"

#include <QImage>
#include <QString>

#include <functional>

namespace rv::data {
class Presentation;
class URL;
}

// A presentation read into a deck (pptxdeck.h), slide for slide as ProPresenter would
// show them, with the options the person chose: for Export to PowerPoint. A port of
// ProToPPTX's ProImporter, with its ElementMapper, MediaLayers and PlayOrder, each of
// which says below what it knows of ProPresenter, KNOWN where ProToPPTX measured or read
// it, A GUESS where it reasoned.
//
// For each slide, in play order, back to front:
//
//   1. The colour under everything: black, as ProPresenter's output is where nothing is
//      drawn; or the presentation's background colour, if it has one turned on.
//   2. The background media: the slide's own media cue's, or, with "keep background
//      media" on, the one left on screen by an earlier slide.
//   3. A slide's own background colour, if it draws one. It is part of the slide layer,
//      which is above the media in ProPresenter, so it covers the media; when there is
//      no media it simply becomes the slide's colour.
//   4. Foreground media of the slide's own cue.
//   5. The slide's elements, from the last the slide lists to the first: ProPresenter
//      lists them front to back.
//
// Notes go to the speaker notes. ProPresenter's groups become PowerPoint's sections,
// unless they say nothing: a sermon whose groups are all called "Group", or nothing, gets
// none.
//
// Three things ProPresenter can fill the letters of a text with that PowerPoint has no
// such fill for, and a fourth it has (KNOWN, from a presentation the maintainer made for
// the purpose, and ProPresenter's own pictures of its slides, 2026-10-28):
//
//   - a background effect, a blur (or an invert) of whatever is behind the letters, which
//     can fill a shape as well: the letters show the slide behind them blurred, or its
//     colours inverted;
//   - a cut-out: the letters are holes in the box's own fill, through which what is
//     behind the box shows (the demo's box filled the slide in black, and its letters
//     showed the background picture);
//   - a picture (media fill), stretched or fitted over the whole box as the picture says,
//     each letter showing its part of it.
//
// ProToPPTX leaves the first two out. Here each is put in as a picture filling the
// letters (or the shape), which PowerPoint can do: what is behind the element is drawn
// (the colour under everything, the background media or a video's still, the slide's
// colour, and the shapes and pictures behind it; not their words, which would take this
// app's whole drawing of text), blurred, inverted or as it is, and cut to the element's
// box; a media fill's picture is fitted to the box. The blur's size was measured against
// ProPresenter's picture of the demo's blurred words: its blur amount of 0.2648 is a
// Gaussian of 6.9 slide units (standard deviation; between 6.8 and 7.1, at a 1080p
// slide), so about 26 units for each whole of the amount, which is taken here to hold
// for every amount (A GUESS beyond the one measured). Its saturation, not written in the
// demo, left the colours as they are, so none written is 1; one that is written is taken
// as a multiple of the colours' own (A GUESS: none has been seen).

namespace pptx {

struct ImportResult
{
    Deck deck;
    QList<Note> notes;
    // The arrangement whose order was followed, if one was
    QString arrangement;
};

// What the importer is handed of the computer it runs on, so that it can be tried
// without it: the file a reference to a medium names here (empty if it cannot be found),
// and a picture of a video with its length in seconds (null where it cannot be read).
struct ImportContext
{
    std::function<QString(const rv::data::URL &)> findMedia;
    std::function<QImage(const QString &path, double *length)> videoStill;
};

// `arrangement` is the name of the arrangement to follow, as the presentation's row in
// the library or playlist has it ("" for Master: every group once, in its order).
// `fallbackName` is the title for a presentation with no name of its own.
ImportResult importPresentation(const rv::data::Presentation &presentation, const QString &fallbackName,
                                const QString &arrangement, const Options &options, const ImportContext &context);

// The picture a background effect becomes: `behind`, the part of the slide behind the
// element, blurred by a Gaussian of `sigma` of its pixels (its standard deviation) with its
// colours' saturation times `saturation`, or inverted. (For the tests.)
QImage backgroundEffect(const QImage &behind, double sigma, double saturation, bool invert);

} // namespace pptx
