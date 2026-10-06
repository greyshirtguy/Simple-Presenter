#include "strokedtext.h"

#include "richtext.h"

#include <QPainter>
#include <QPen>
#include <QtMath>

namespace {

// Whether a run laid out glyph by glyph can be drawn that way and look as it would drawn
// whole. It can if nothing of it is seen through: where two glyphs overlap, or one
// glyph's stroke runs over its neighbour, drawing them one after another lays colour on
// colour, which only shows if the colour is not solid.
bool drawnByGlyph(const TextLayoutResult::Outline &outline)
{
    const TextRun &format = outline.format;
    return !outline.pieces.isEmpty() && (!format.fillVisible || format.fill.alpha() == 255)
        && (format.strokeWidth <= 0 || format.stroke.alpha() == 255);
}

} // namespace

StrokedText::StrokedText(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    // Anything but the replacement changing means all of it is drawn again, and that
    // what was worked out for the replacement no longer holds.
    const auto repaint = [this] {
        m_liveKnown = false;
        update();
    };
    connect(this, &StrokedText::contentChanged, this, repaint);
    connect(this, &StrokedText::unitChanged, this, repaint);
    connect(this, &StrokedText::bleedChanged, this, repaint);
    connect(this, &StrokedText::verticalAlignmentChanged, this, repaint);
    connect(this, &StrokedText::fitChanged, this, repaint);
    connect(this, &StrokedText::insetsChanged, this, repaint);
    connect(this, &StrokedText::lineFillChanged, this, repaint);
    connect(this, &StrokedText::replacementChanged, this, &StrokedText::replace);
}

void StrokedText::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        m_liveKnown = false;
        update();
    }
}

// What there is to draw: the content, or the replacement in the content's style.
RichText StrokedText::shown() const
{
    const RichText content = m_content.value<RichText>();
    if (m_replacement.typeId() != QMetaType::QString)
        return content;
    return RichText::plain(m_replacement.toString(), content.firstRun(),
                           content.paragraphs.isEmpty() ? Qt::AlignHCenter : content.paragraphs.first().alignment);
}

// What there is to draw at the size it is drawn at: as it is, or made smaller or larger
// to suit the box, if the box is set to do that.
//
// The size is found for the shape of the text and not for the text itself: with every
// digit taken for a nought. A timer's time then keeps one size while it runs, and does
// not shiver as a 1 gives way to a 0; it changes size only when it gains or loses a
// digit. (It also means the search is made once for a running timer, and not thirty
// times a second for one that shows its hundredths.)
RichText StrokedText::fitted() const
{
    const RichText text = shown();
    const QSizeF room = box();
    if (m_fit < 2 || m_fit > 4 || room.width() <= 0 || room.height() <= 0)
        return text;

    RichText shape = text;
    for (TextParagraph &paragraph : shape.paragraphs) {
        for (TextRun &run : paragraph.runs) {
            for (QChar &character : run.text) {
                if (character.isDigit())
                    character = u'0';
            }
        }
    }
    if (m_fitKind != m_fit || m_fitRoom != room || !(m_fitShape == shape)) {
        m_fitScale = fittingScale(shape, room, m_fit);
        m_fitShape = shape;
        m_fitRoom = room;
        m_fitKind = m_fit;
    }
    return m_fitScale == 1 ? text : scaledText(text, m_fitScale);
}

// The box the text is laid out in, in slide units: empty if there is no room for any.
QSizeF StrokedText::box() const
{
    if (m_unit <= 0)
        return {};
    return QSizeF(width() / m_unit - 2 * m_bleed - m_insetLeft - m_insetRight,
                  height() / m_unit - 2 * m_bleed - m_insetTop - m_insetBottom);
}

// Where the layout's own top left corner is in the item, in slide units.
QPointF StrokedText::origin(const TextLayoutResult &layout) const
{
    const qreal vFactor = m_vAlign & Qt::AlignVCenter ? 0.5 : m_vAlign & Qt::AlignBottom ? 1.0 : 0.0;
    return QPointF(m_bleed + m_insetLeft, m_bleed + m_insetTop + (box().height() - layout.height) * vFactor);
}

// The replacement has changed. If the one before it is known, and both are drawn glyph
// by glyph, only what has changed between them is drawn again.
void StrokedText::replace()
{
    const bool live = m_replacement.typeId() == QMetaType::QString && box().width() > 0 && box().height() > 0;
    const TextLayoutResult before = m_live;
    const bool known = m_liveKnown;
    m_live = live ? layoutText(fitted(), box().width(), true) : TextLayoutResult();
    m_liveKnown = live;

    const QRect changed = known && live ? changedPart(before, m_live) : QRect();
    if (changed.isNull())
        update();
    else
        update(changed);
}

// What of the item has to be drawn again to turn one layout of the replacement into
// another: the glyphs that are not the same glyph in the same place in both, with what
// their strokes add. Null if that cannot be said, and all of it has to be.
QRect StrokedText::changedPart(const TextLayoutResult &before, const TextLayoutResult &after) const
{
    // A fill behind the lines goes by how wide the lines are.
    if (m_lineFill.alpha() > 0 || before.height != after.height || before.outlines.size() != after.outlines.size())
        return {};

    QRectF changed;
    qreal reach = 0;
    for (qsizetype i = 0; i < after.outlines.size(); ++i) {
        const TextLayoutResult::Outline &was = before.outlines.at(i);
        const TextLayoutResult::Outline &is = after.outlines.at(i);
        if (!drawnByGlyph(was) || !drawnByGlyph(is))
            return {};
        reach = qMax(reach, qMax(was.format.strokeWidth, is.format.strokeWidth) / 2);
        for (qsizetype g = 0; g < qMax(was.pieces.size(), is.pieces.size()); ++g) {
            const bool same = g < was.pieces.size() && g < is.pieces.size() && was.pieces.at(g).glyph == is.pieces.at(g).glyph
                              && was.pieces.at(g).position == is.pieces.at(g).position;
            if (same)
                continue;
            if (g < was.pieces.size())
                changed |= was.pieces.at(g).bounds;
            if (g < is.pieces.size())
                changed |= is.pieces.at(g).bounds;
        }
    }
    if (changed.isEmpty())
        return {};

    // In the item's own pixels, with a pixel to spare for the soft edges.
    const QPointF at = origin(after);
    const QRectF placed = changed.adjusted(-reach, -reach, reach, reach).translated(at);
    const QRect pixels = QRectF(placed.topLeft() * m_unit, placed.bottomRight() * m_unit).toAlignedRect().adjusted(-2, -2, 2, 2);
    return pixels & QRect(0, 0, qCeil(width()), qCeil(height()));
}

void StrokedText::paint(QPainter *painter)
{
    const QSizeF room = box();
    if (room.width() <= 0 || room.height() <= 0)
        return;
    // A replacement's layout is worked out when it is set, where it is needed to know
    // what has changed; if that is not to hand, it is done here.
    const bool live = m_replacement.typeId() == QMetaType::QString;
    const TextLayoutResult layout = live && m_liveKnown ? m_live : layoutText(fitted(), room.width(), live);
    if (layout.outlines.isEmpty() && layout.lines.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->scale(m_unit, m_unit);
    painter->translate(origin(layout));

    // The bars behind the lines go down first.
    if (m_lineFill.alpha() > 0) {
        qreal widest = 0;
        for (const QRectF &line : layout.lines)
            widest = qMax(widest, line.width());
        for (qsizetype i = 0; i < layout.lines.size(); ++i) {
            QRectF bar = layout.lines.at(i);
            if (m_lineFillStyle == 0) {
                bar.setLeft(0);
                bar.setWidth(room.width());
            } else if (m_lineFillStyle == 2) {
                bar.setLeft((room.width() - widest) * layout.lineAlignments.at(i));
                bar.setWidth(widest);
            }
            bar.adjust(-m_lineFillWidthOffset / 2, -m_lineFillHeightOffset / 2,
                       m_lineFillWidthOffset / 2, m_lineFillHeightOffset / 2);
            bar.translate(m_lineFillHorizontalOffset, m_lineFillVerticalOffset);
            if (bar.width() > 0 && bar.height() > 0)
                painter->fillRect(bar, m_lineFill);
        }
    }

    // What is being drawn again, if not all of it: a glyph clear of that is left alone.
    const QRectF clip = painter->hasClipping() ? painter->clipBoundingRect() : QRectF();
    const auto untouched = [&clip](const QRectF &bounds, qreal reach) {
        return !clip.isNull() && !bounds.adjusted(-reach, -reach, reach, reach).intersects(clip);
    };

    // Every stroke goes down before any fill, so one run's stroke never covers its
    // neighbour's fill. The stroke is centred on the outline, as Cocoa draws it.
    for (const TextLayoutResult::Outline &outline : layout.outlines) {
        if (outline.format.strokeWidth <= 0)
            continue;
        const QPen pen(outline.format.stroke, outline.format.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (!drawnByGlyph(outline)) {
            painter->strokePath(outline.path, pen);
            continue;
        }
        for (const TextLayoutResult::Outline::Piece &piece : outline.pieces) {
            if (!untouched(piece.bounds, outline.format.strokeWidth / 2))
                painter->strokePath(piece.path, pen);
        }
    }
    for (const TextLayoutResult::Outline &outline : layout.outlines) {
        if (!outline.format.fillVisible)
            continue;
        if (!drawnByGlyph(outline)) {
            painter->fillPath(outline.path, outline.format.fill);
            continue;
        }
        for (const TextLayoutResult::Outline::Piece &piece : outline.pieces) {
            if (!untouched(piece.bounds, 0))
                painter->fillPath(piece.path, outline.format.fill);
        }
    }
}
