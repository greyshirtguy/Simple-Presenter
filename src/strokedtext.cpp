#include "strokedtext.h"

#include "richtext.h"

#include <QFont>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRawFont>
#include <QTextLayout>
#include <QTextOption>

namespace {

QFont fontFor(const TextRun &run)
{
    QFont font(run.family);
    font.setPixelSize(qMax(1, qRound(run.size)));
    font.setBold(run.bold);
    font.setItalic(run.italic);
    // Hinting snaps outlines to the pixel grid, which distorts them once scaled.
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

struct RunOutline
{
    QPainterPath path;
    const TextRun *run;
};

} // namespace

StrokedText::StrokedText(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    const auto repaint = [this] { update(); };
    connect(this, &StrokedText::contentChanged, this, repaint);
    connect(this, &StrokedText::unitChanged, this, repaint);
    connect(this, &StrokedText::bleedChanged, this, repaint);
    connect(this, &StrokedText::verticalAlignmentChanged, this, repaint);
}

void StrokedText::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        update();
}

void StrokedText::paint(QPainter *painter)
{
    const RichText content = m_content.value<RichText>();
    if (content.paragraphs.isEmpty() || m_unit <= 0)
        return;

    const qreal boxWidth = width() / m_unit - 2 * m_bleed;
    const qreal boxHeight = height() / m_unit - 2 * m_bleed;
    if (boxWidth <= 0 || boxHeight <= 0)
        return;

    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);

    QList<RunOutline> outlines;
    qreal y = 0;
    for (const TextParagraph &paragraph : content.paragraphs) {
        QString text;
        QList<QTextLayout::FormatRange> formats;
        for (const TextRun &run : paragraph.runs) {
            QTextLayout::FormatRange range;
            range.start = text.size();
            range.length = run.text.size();
            range.format.setFont(fontFor(run));
            formats.append(range);
            text += run.text;
        }
        if (text.isEmpty()) {
            if (!paragraph.runs.isEmpty())
                y += QFontMetricsF(fontFor(paragraph.runs.first())).height();
            continue;
        }

        const qreal hFactor = paragraph.alignment & Qt::AlignHCenter ? 0.5
                            : paragraph.alignment & Qt::AlignRight ? 1.0 : 0.0;

        QTextLayout layout(text, fontFor(paragraph.runs.first()));
        layout.setTextOption(option);
        layout.setFormats(formats);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(boxWidth);
            line.setPosition(QPointF((boxWidth - line.naturalTextWidth()) * hFactor, y));
            y += line.height();
        }
        layout.endLayout();

        for (qsizetype i = 0; i < paragraph.runs.size(); ++i) {
            if (formats.at(i).length == 0)
                continue;
            // Overlapping glyphs (script faces, tight tracking) must not punch holes in each other.
            RunOutline outline;
            outline.run = &paragraph.runs.at(i);
            outline.path.setFillRule(Qt::WindingFill);
            const QList<QGlyphRun> glyphRuns = layout.glyphRuns(formats.at(i).start, formats.at(i).length);
            for (const QGlyphRun &glyphRun : glyphRuns) {
                const QRawFont rawFont = glyphRun.rawFont();
                const QList<quint32> glyphs = glyphRun.glyphIndexes();
                const QList<QPointF> positions = glyphRun.positions();
                for (qsizetype g = 0; g < glyphs.size(); ++g)
                    outline.path.addPath(rawFont.pathForGlyph(glyphs.at(g)).translated(positions.at(g)));
            }
            outlines.append(outline);
        }
    }

    const qreal vFactor = m_vAlign & Qt::AlignVCenter ? 0.5 : m_vAlign & Qt::AlignBottom ? 1.0 : 0.0;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->scale(m_unit, m_unit);
    painter->translate(m_bleed, m_bleed + (boxHeight - y) * vFactor);

    // Every stroke goes down before any fill, so one run's stroke never covers its
    // neighbour's fill. The stroke is centred on the outline, as Cocoa draws it.
    for (const RunOutline &outline : outlines) {
        if (outline.run->strokeWidth > 0) {
            painter->strokePath(outline.path, QPen(outline.run->stroke, outline.run->strokeWidth,
                                                   Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
    }
    for (const RunOutline &outline : outlines) {
        if (outline.run->fillVisible)
            painter->fillPath(outline.path, outline.run->fill);
    }
}
