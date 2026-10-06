#include "strokedtext.h"

#include "richtext.h"
#include "textlayout.h"

#include <QPainter>
#include <QPen>

StrokedText::StrokedText(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    const auto repaint = [this] { update(); };
    connect(this, &StrokedText::contentChanged, this, repaint);
    connect(this, &StrokedText::unitChanged, this, repaint);
    connect(this, &StrokedText::bleedChanged, this, repaint);
    connect(this, &StrokedText::verticalAlignmentChanged, this, repaint);
    connect(this, &StrokedText::insetsChanged, this, repaint);
    connect(this, &StrokedText::lineFillChanged, this, repaint);
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

    const qreal boxWidth = width() / m_unit - 2 * m_bleed - m_insetLeft - m_insetRight;
    const qreal boxHeight = height() / m_unit - 2 * m_bleed - m_insetTop - m_insetBottom;
    if (boxWidth <= 0 || boxHeight <= 0)
        return;

    const TextLayoutResult layout = layoutText(content, boxWidth);
    const qreal vFactor = m_vAlign & Qt::AlignVCenter ? 0.5 : m_vAlign & Qt::AlignBottom ? 1.0 : 0.0;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->scale(m_unit, m_unit);
    painter->translate(m_bleed + m_insetLeft, m_bleed + m_insetTop + (boxHeight - layout.height) * vFactor);

    // The bars behind the lines go down first.
    if (m_lineFill.alpha() > 0) {
        qreal widest = 0;
        for (const QRectF &line : layout.lines)
            widest = qMax(widest, line.width());
        for (qsizetype i = 0; i < layout.lines.size(); ++i) {
            QRectF bar = layout.lines.at(i);
            if (m_lineFillStyle == 0) {
                bar.setLeft(0);
                bar.setWidth(boxWidth);
            } else if (m_lineFillStyle == 2) {
                bar.setLeft((boxWidth - widest) * layout.lineAlignments.at(i));
                bar.setWidth(widest);
            }
            bar.adjust(-m_lineFillWidthOffset / 2, -m_lineFillHeightOffset / 2,
                       m_lineFillWidthOffset / 2, m_lineFillHeightOffset / 2);
            bar.translate(m_lineFillHorizontalOffset, m_lineFillVerticalOffset);
            if (bar.width() > 0 && bar.height() > 0)
                painter->fillRect(bar, m_lineFill);
        }
    }

    // Every stroke goes down before any fill, so one run's stroke never covers its
    // neighbour's fill. The stroke is centred on the outline, as Cocoa draws it.
    for (const TextLayoutResult::Outline &outline : layout.outlines) {
        if (outline.format.strokeWidth > 0) {
            painter->strokePath(outline.path, QPen(outline.format.stroke, outline.format.strokeWidth,
                                                   Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
    }
    for (const TextLayoutResult::Outline &outline : layout.outlines) {
        if (outline.format.fillVisible)
            painter->fillPath(outline.path, outline.format.fill);
    }
}
