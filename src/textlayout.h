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
TextLayoutResult layoutText(const RichText &text, qreal width);

// Puts the text into a document that lays out as layoutText() does. Every stretch of the
// document carries the whole format of its run, so text typed beside it takes that
// format and nothing of it is lost on the way back out. The document is for editing
// over the real drawing, not for showing: colours and decorations are not in it.
void toDocument(const RichText &text, QTextDocument *document);
RichText fromDocument(const QTextDocument *document);
