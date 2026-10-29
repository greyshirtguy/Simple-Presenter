#pragma once

#include "pptxdeck.h"

#include <QByteArray>
#include <QList>
#include <QStringList>

// The words of a ProPresenter text box, with their fonts, sizes, colours and paragraph
// settings, out of the RTF ProPresenter keeps them in, for Export to PowerPoint. A port
// of ProToPPTX's RTFReader.
//
// Why a second reader of RTF beside the one the slides are drawn from (rtf.h): that one
// reads what this app draws and writes back, and PowerPoint can show more of a text than
// this app draws: indents, the space before and after a paragraph, tab stops, lists,
// highlights, an underline's own colour. Those are kept here for the export alone, and
// the drawing goes on as it was.
//
// KNOWN (ProToPPTX's notes): what ProPresenter writes is the RTF of macOS's own text
// system ("Cocoa RTF": \cocoartf, \expandedcolortbl). What this reads was settled there
// by counting the control words in the maintainer's presentations (2026, ProPresenter
// 21.4), and it reads all of them: fonts and sizes (\f, \fs, \fsmilli), bold and
// italic, colours with alpha (\cf with \cssrgb, \csgray, \csgenericrgb), underline and
// strike, character spacing (\expnd, \expndtw), superscript and baseline (\super, \up),
// alignment, indents, line and paragraph spacing (\sl, \slmult, \slleading, \sa, \sb),
// tab stops, and lists (\ls, and the marker in {\listtext}). Anything else is passed over
// without harm: an unknown control word is ignored, and an unknown group marked \* is
// skipped whole.
//
// Units: RTF's twips are 1/20 of a point, and font sizes are half points. A point here
// is a unit of the ProPresenter slide (a pixel of a 1080p slide), because that is what
// ProPresenter's font sizes are; so twips / 20 is slide units, as the deck counts them.
//
// The text is counted as ProPresenter counts it (in UTF-16 units, a paragraph break
// being one, a list's marker being part of the text) so that the ranges ProPresenter
// keeps beside the RTF (capitalization, gradient fills) can be laid on it afterwards.

namespace pptx {

struct RtfDocument
{
    QList<Paragraph> paragraphs;
    // For each paragraph, how many UTF-16 units of list marker came before its runs:
    // counted in ProPresenter's ranges, but not part of the words (PowerPoint draws the
    // marker itself, as a bullet)
    QList<int> markerLengths;
    // The fonts the RTF names, in the order of its font table (PostScript names)
    QStringList fontNames;

    // Where each paragraph's runs start, in ProPresenter's count of the whole text
    QList<int> paragraphStarts() const;
};

// Reads RTF. Never fails: what cannot be read is left out, and text that is not RTF at
// all comes back as one paragraph of plain words a line.
RtfDocument readRtf(const QByteArray &rtf);

// The marker macOS wrote for a list item ("\t1.\t", "\t•\t") as a bullet PowerPoint can
// draw itself.
std::optional<Paragraph::Bullet> bulletFromMarker(const QString &marker);

} // namespace pptx
