#pragma once

#include "richtext.h"

#include <QFont>
#include <QList>
#include <QPainterPath>
#include <QRectF>

class QTextDocument;

// How styled text is laid out, in one place, so that what is drawn on a slide and what a
// text editor laid over it believes about where every character is are the same thing.
// Everything is in slide units.

// The font a run is drawn in.
QFont fontFor(const TextRun &run);

struct TextLayoutResult
{
    // The glyphs of one run, with its underline and strikethrough if it has them.
    struct Outline
    {
        QPainterPath path;
        TextRun format;
    };

    QList<Outline> outlines;
    // One for each line that has text: where it is, and how wide its text is.
    QList<QRectF> lines;
    // Which way each of those lines is aligned in the box: 0 left, 0.5 centred, 1 right.
    QList<qreal> lineAlignments;
    qreal height = 0;
};

// Lays the text out in a box `width` wide, with its top at 0. Lines are stacked the way
// QTextDocument stacks them, which is what a TextEdit shows.
//
// Two things are remembered, because the same work comes round again and again. The
// outline of each glyph is kept by font and glyph: getting outlines from a font is most
// of the cost of a layout, and since layout is always in slide units, every slide of a
// presentation asks for the same letters of the same font at the same size. And whole
// layouts are kept by text and width, the most recently used few hundred: one slide is
// laid out for its thumbnail, for the preview and for the output, and again whenever a
// list it is in is rebuilt. Safe to call from several threads at once, which happens:
// each window draws on a thread of its own.
TextLayoutResult layoutText(const RichText &text, qreal width);

// Puts the text into a document that lays out as layoutText() does. Every stretch of the
// document carries the whole format of its run, so text typed beside it takes that
// format and nothing of it is lost on the way back out. The document is for editing
// over the real drawing, not for showing: colours and decorations are not in it.
void toDocument(const RichText &text, QTextDocument *document);
RichText fromDocument(const QTextDocument *document);
