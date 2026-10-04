#pragma once

#include <QColor>
#include <QList>
#include <QMetaType>
#include <QString>

// Styled text as the renderer consumes it. All lengths are in slide units (the
// coordinate space of the slide's own size, typically 1920x1080), not output pixels.

struct TextRun
{
    QString text;
    QString family;
    qreal size = 12;
    bool bold = false;
    bool italic = false;
    QColor fill = Qt::black;
    bool fillVisible = true;
    QColor stroke = Qt::black;
    qreal strokeWidth = 0;
};

// A paragraph with no text still carries one empty run, so its line height is known.
struct TextParagraph
{
    Qt::Alignment alignment = Qt::AlignLeft;
    QList<TextRun> runs;
};

struct RichText
{
    QList<TextParagraph> paragraphs;

    QString plainText() const;
};

Q_DECLARE_METATYPE(RichText)
