#pragma once

#include "pptxdeck.h"

#include <QFont>
#include <QList>
#include <QRectF>

// A text box of a deck (pptxdeck.h) laid out as ProPresenter lays it out, for measuring
// it: for Export to PowerPoint. A port of ProToPPTX's TextLayout, with Qt's text layout
// where it had macOS's own, and the faces this app draws slides in (fontFor, in
// textlayout.h), so that a box measures here as it is drawn here.
//
// What it is measured for: ProPresenter's "scale text down to fit" shrinks the words of a
// box until they fit, at the moment they are drawn, and does not save the size it drew
// them at. To give PowerPoint the same size (its "shrink text on overflow", with the
// factor written into the file), the words are laid out at their full size, and smaller,
// until they fit (shrinkFactor). And PowerPoint lays lines out otherwise than ProPresenter
// does, so where ProPresenter puts each line's baseline is worked out here, for the
// writer to put PowerPoint's lines in the same places (pptxwriter.cpp, TextFit).
//
// As ProToPPTX found ProPresenter lays text out (its docs/NOTES.md, KNOWN where measured):
// a line's height is the font's own (ascent, descent and leading, as this app lays its
// slides out); a line multiple over 1 puts the extra below each line, a multiple under 1
// makes the line shorter; superscripts are drawn at 0.6 of the size and do not make
// their line taller. Where the space of an exact or a least height goes, and whether the
// space before the first paragraph and after the last count, are GUESSES made here: at
// the top of the line, and not at all, as the writer's model of PowerPoint has them.

namespace pptx {

struct LaidLine
{
    // Which paragraph of the box it is in
    int paragraph = 0;
    // The largest font size on it, in the units laid out (the shrink factor applied)
    double largestSize = 0;
    // Where its baseline is, from the top of the text
    double baseline = 0;
};

struct LaidText
{
    QList<LaidLine> lines;
    double height = 0;
};

// The font a run of the deck is drawn in, at this size.
QFont fontOf(const RunStyle &style, double size);
// The words of a box laid out at this width, line by line, with every size multiplied by
// `fontScale`, and the height of the whole.
LaidText layoutLines(const TextBody &body, double width, double fontScale = 1);
// The height of one line of this font, as a multiple of its size: what ProPresenter's
// "single" line spacing is for it (about 1.2 for Helvetica).
double lineHeightRatio(const RunStyle &style);
// The factor (0...1) by which "scale text down to fit" would shrink the words of a box of
// this size: 1 if they fit as they are.
double shrinkFactor(const TextBody &body, const QRectF &frame);

} // namespace pptx
