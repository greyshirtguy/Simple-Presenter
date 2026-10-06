#include "textlayout.h"

#include <QCache>
#include <QDataStream>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QHash>
#include <QIODevice>
#include <QMutex>
#include <QRawFont>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

namespace {

// Where each stretch of a document keeps the format of its run.
const int runProperty = QTextFormat::UserProperty + 1;
// Where each paragraph of a document keeps its line spacing.
const int lineHeightProperty = QTextFormat::UserProperty + 2;
const int lineHeightIsMultipleProperty = QTextFormat::UserProperty + 3;

QTextCharFormat layoutFormat(const TextRun &run)
{
    QTextCharFormat format;
    format.setFont(fontFor(run));
    if (run.superscript != 0)
        format.setVerticalAlignment(run.superscript > 0 ? QTextCharFormat::AlignSuperScript
                                                        : QTextCharFormat::AlignSubScript);
    return format;
}

// The height QTextDocument gives a line before any line spacing is applied.
qreal naturalHeight(const QTextLine &line)
{
    return qCeil(line.ascent() + line.descent() + line.leading());
}

// How far a line of this paragraph advances, given its natural height.
qreal lineAdvance(const TextParagraph &paragraph, qreal natural)
{
    if (paragraph.lineHeight == 0)
        return natural;
    if (paragraph.lineHeightIsMultiple)
        return natural * paragraph.lineHeight / 240.0;
    const qreal height = qAbs(paragraph.lineHeight) / 20.0;
    return paragraph.lineHeight > 0 ? qMax(natural, height) : height;
}

QTextOption textOption()
{
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    // Advances as the font's design has them, not rounded to whole pixels: the layout
    // is scaled to whatever size it is shown at, and a TextEdit lays out the same way.
    option.setUseDesignMetrics(true);
    return option;
}

// The outlines of glyphs, kept by font and glyph.
//
// Text is drawn from its glyphs' outlines, so that it can be stroked, and asking a font
// for an outline is the dearest part of drawing a slide's text: around two thirds of
// the time. But the layout is always done in slide units, whatever size the slide ends
// up on screen, so the same few dozen outlines of the same font at the same size are
// asked for again and again: by every slide of a presentation, and by each of the
// thumbnail, the preview and the output that show the same slide.
class GlyphOutlines
{
public:
    QPainterPath outline(const QRawFont &font, const QString &fontKey, quint32 glyph)
    {
        {
            const QMutexLocker lock(&m_mutex);
            const auto ofFont = m_outlines.constFind(fontKey);
            if (ofFont != m_outlines.constEnd()) {
                const auto found = ofFont->constFind(glyph);
                if (found != ofFont->constEnd())
                    return *found;
            }
        }
        const QPainterPath made = font.pathForGlyph(glyph);
        const QMutexLocker lock(&m_mutex);
        // A presentation uses a handful of fonts; if it ever comes to thousands of
        // outlines, something unusual is going on, and starting again is simplest.
        if (m_count >= limit) {
            m_outlines.clear();
            m_count = 0;
        }
        m_outlines[fontKey].insert(glyph, made);
        ++m_count;
        return made;
    }

    // What tells one font from another here: its family, style and size.
    static QString keyFor(const QRawFont &font)
    {
        return font.familyName() + u'|' + font.styleName() + u'|' + QString::number(font.pixelSize(), 'f', 2);
    }

private:
    static constexpr int limit = 6000;
    QMutex m_mutex;
    QHash<QString, QHash<quint32, QPainterPath>> m_outlines;
    int m_count = 0;
};

GlyphOutlines &glyphOutlines()
{
    static GlyphOutlines outlines;
    return outlines;
}

// Everything about a text that its layout depends on, and the width, as bytes: two
// texts with the same key lay out the same.
QByteArray layoutKey(const RichText &text, qreal width)
{
    QByteArray key;
    QDataStream stream(&key, QIODevice::WriteOnly);
    stream << width;
    for (const TextParagraph &paragraph : text.paragraphs) {
        stream << int(paragraph.alignment) << paragraph.lineHeight << paragraph.lineHeightIsMultiple
               << qint32(paragraph.runs.size());
        for (const TextRun &run : paragraph.runs) {
            stream << run.text << run.family << run.size << run.bold << run.italic << run.underline << run.strikethrough
                   << run.kerning << run.superscript << run.capitalization << quint64(run.fill.rgba64()) << run.fillVisible
                   << quint64(run.stroke.rgba64()) << run.strokeWidth;
        }
    }
    return key;
}

// Layouts already worked out, the most recently used kept. The same slide is laid out
// many times over: for its thumbnail, for the preview and for the output, and again
// every time a list it is in is rebuilt.
struct LayoutCache
{
    QMutex mutex;
    QCache<QByteArray, TextLayoutResult> layouts {400};
};

LayoutCache &layoutCache()
{
    static LayoutCache cache;
    return cache;
}

QTextCharFormat documentFormat(const TextRun &run)
{
    QTextCharFormat format = layoutFormat(run);
    TextRun kept = run;
    kept.text.clear();
    format.setProperty(runProperty, QVariant::fromValue(kept));
    return format;
}

QTextBlockFormat documentFormat(const TextParagraph &paragraph)
{
    QTextBlockFormat format;
    format.setAlignment(paragraph.alignment);
    format.setProperty(lineHeightProperty, paragraph.lineHeight);
    format.setProperty(lineHeightIsMultipleProperty, paragraph.lineHeightIsMultiple);
    if (paragraph.lineHeight != 0) {
        // Extra height goes above the text, which a minimum height does and a
        // proportional one does not; a minimum height is not proportional, so it is
        // worked out for the paragraph's first font.
        const QFontMetricsF metrics(fontFor(paragraph.runs.isEmpty() ? TextRun() : paragraph.runs.first()));
        const qreal natural = qCeil(metrics.ascent() + metrics.descent() + metrics.leading());
        const qreal advance = lineAdvance(paragraph, natural);
        if (advance >= natural)
            format.setLineHeight(advance, QTextBlockFormat::MinimumHeight);
        else
            format.setLineHeight(advance / natural * 100, QTextBlockFormat::ProportionalHeight);
    }
    return format;
}

// The format a stretch of a document stands for. Text that arrived without one (it
// should not, but something pasted by a route that is not plain text could) is read
// from how it looks.
TextRun runOf(const QTextCharFormat &format)
{
    const QVariant kept = format.property(runProperty);
    if (kept.canConvert<TextRun>())
        return kept.value<TextRun>();
    TextRun run;
    run.family = format.fontFamilies().toStringList().value(0);
    run.size = format.hasProperty(QTextFormat::FontPixelSize) ? format.intProperty(QTextFormat::FontPixelSize) : 60;
    run.bold = format.fontWeight() >= QFont::DemiBold;
    run.italic = format.fontItalic();
    run.underline = format.fontUnderline();
    run.strikethrough = format.fontStrikeOut();
    run.fill = Qt::white;
    return run;
}

} // namespace

QFont fontFor(const TextRun &run)
{
    QFont font(run.family);
    font.setPixelSize(qMax(1, qRound(run.size)));
    font.setBold(run.bold);
    font.setItalic(run.italic);
    // Hinting snaps outlines to the pixel grid, which distorts them once scaled.
    font.setHintingPreference(QFont::PreferNoHinting);
    if (run.kerning != 0)
        font.setLetterSpacing(QFont::AbsoluteSpacing, run.kerning);
    switch (run.capitalization) {
    case TextRun::AllCaps:
        font.setCapitalization(QFont::AllUppercase);
        break;
    case TextRun::SmallCaps:
        font.setCapitalization(QFont::SmallCaps);
        break;
    case TextRun::TitleCase:
    case TextRun::StartCase:
        font.setCapitalization(QFont::Capitalize);
        break;
    default:
        break;
    }
    return font;
}

TextLayoutResult layoutText(const RichText &content, qreal width)
{
    // Text is drawn on the render thread of whichever window shows it, so several
    // threads can be here at once.
    LayoutCache &cache = layoutCache();
    const QByteArray key = layoutKey(content, width);
    {
        const QMutexLocker lock(&cache.mutex);
        if (const TextLayoutResult *known = cache.layouts.object(key))
            return *known;
    }

    TextLayoutResult result;
    const QTextOption option = textOption();

    qreal y = 0;
    for (const TextParagraph &paragraph : content.paragraphs) {
        if (paragraph.runs.isEmpty())
            continue;
        QString text;
        QList<QTextLayout::FormatRange> formats;
        for (const TextRun &run : paragraph.runs) {
            QTextLayout::FormatRange range;
            range.start = text.size();
            range.length = run.text.size();
            range.format = layoutFormat(run);
            formats.append(range);
            text += run.text;
        }

        const qreal hFactor = paragraph.alignment & Qt::AlignHCenter ? 0.5
                            : paragraph.alignment & Qt::AlignRight ? 1.0 : 0.0;

        // A paragraph with no text still makes one line, as tall as its font.
        QTextLayout layout(text, fontFor(paragraph.runs.first()));
        layout.setTextOption(option);
        layout.setFormats(formats);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLeadingIncluded(true);
            line.setLineWidth(width);
            // Aligned by what its characters advance, which leaves out the space a line
            // was broken at; a line too wide for the box starts at its left edge. Extra
            // height goes above the text, as it does in Cocoa's layout.
            const qreal advance = lineAdvance(paragraph, naturalHeight(line));
            line.setPosition(QPointF(qMax<qreal>(0, (width - line.horizontalAdvance()) * hFactor),
                                     y + qMax<qreal>(0, advance - line.height())));
            if (line.textLength() > 0 && line.naturalTextWidth() > 0) {
                result.lines.append(QRectF(line.position(), QSizeF(line.naturalTextWidth(), line.height())));
                result.lineAlignments.append(hFactor);
            }
            y += advance;
        }
        layout.endLayout();

        for (qsizetype i = 0; i < paragraph.runs.size(); ++i) {
            if (formats.at(i).length == 0)
                continue;
            // Overlapping glyphs (script faces, tight tracking) must not punch holes in each other.
            TextLayoutResult::Outline outline;
            outline.format = paragraph.runs.at(i);
            outline.format.text.clear();
            outline.path.setFillRule(Qt::WindingFill);
            const QList<QGlyphRun> glyphRuns = layout.glyphRuns(formats.at(i).start, formats.at(i).length);
            for (const QGlyphRun &glyphRun : glyphRuns) {
                const QRawFont rawFont = glyphRun.rawFont();
                const QString fontKey = GlyphOutlines::keyFor(rawFont);
                const QList<quint32> glyphs = glyphRun.glyphIndexes();
                const QList<QPointF> positions = glyphRun.positions();
                for (qsizetype g = 0; g < glyphs.size(); ++g) {
                    outline.path.addPath(glyphOutlines().outline(rawFont, fontKey, glyphs.at(g))
                                             .translated(positions.at(g)));
                }

                // Underline and strikethrough run the width of the glyphs, at the
                // positions and thickness the font asks for.
                const QRectF bounds = glyphRun.boundingRect();
                if ((outline.format.underline || outline.format.strikethrough) && bounds.isValid()
                    && !positions.isEmpty()) {
                    const qreal baseline = positions.first().y();
                    const qreal thickness = qMax<qreal>(1, rawFont.lineThickness());
                    if (outline.format.underline)
                        outline.path.addRect(bounds.left(), baseline + rawFont.underlinePosition() - thickness / 2,
                                             bounds.width(), thickness);
                    if (outline.format.strikethrough)
                        outline.path.addRect(bounds.left(), baseline - rawFont.xHeight() / 2 - thickness / 2,
                                             bounds.width(), thickness);
                }
            }
            result.outlines.append(outline);
        }
    }
    result.height = y;

    const QMutexLocker lock(&cache.mutex);
    cache.layouts.insert(key, new TextLayoutResult(result));
    return result;
}

void toDocument(const RichText &text, QTextDocument *document)
{
    document->clear();
    document->setDocumentMargin(0);
    document->setDefaultTextOption(textOption());

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    bool first = true;
    for (const TextParagraph &paragraph : text.paragraphs) {
        // An empty paragraph still has a format, for what is typed into it.
        const QTextCharFormat lead = documentFormat(paragraph.runs.isEmpty() ? TextRun() : paragraph.runs.first());
        if (first) {
            cursor.setBlockFormat(documentFormat(paragraph));
            cursor.setBlockCharFormat(lead);
            cursor.setCharFormat(lead);
        } else {
            cursor.insertBlock(documentFormat(paragraph), lead);
        }
        first = false;
        for (const TextRun &run : paragraph.runs) {
            if (!run.text.isEmpty())
                cursor.insertText(run.text, documentFormat(run));
        }
    }
    cursor.endEditBlock();
    document->clearUndoRedoStacks();
    document->setModified(false);
}

RichText fromDocument(const QTextDocument *document)
{
    RichText text;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        TextParagraph paragraph;
        const Qt::Alignment alignment = block.blockFormat().alignment();
        paragraph.alignment = alignment & Qt::AlignHCenter ? Qt::AlignHCenter : alignment & Qt::AlignRight ? Qt::AlignRight
                            : alignment & Qt::AlignJustify ? Qt::AlignJustify : Qt::AlignLeft;
        paragraph.lineHeight = block.blockFormat().intProperty(lineHeightProperty);
        paragraph.lineHeightIsMultiple = block.blockFormat().boolProperty(lineHeightIsMultipleProperty);
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.text().isEmpty())
                continue;
            TextRun run = runOf(fragment.charFormat());
            run.text = fragment.text();
            if (!paragraph.runs.isEmpty() && paragraph.runs.last().sameFormat(run))
                paragraph.runs.last().text += run.text;
            else
                paragraph.runs.append(run);
        }
        if (paragraph.runs.isEmpty())
            paragraph.runs.append(runOf(block.charFormat()));
        text.paragraphs.append(paragraph);
    }
    return text;
}
