#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// The text of a slide element.
//
// Why it is drawn this way. Slide text is big, often outlined, and has to look the same
// on a 4K output as in a thumbnail. The usual way of drawing text in Qt Quick, from a
// texture of pre-rendered glyphs, cannot outline it. So this takes the outline of every
// glyph as a path (see textlayout.h), strokes the paths and then fills them, on the
// CPU, into an image that becomes a texture. Underline and strikethrough are added to
// the paths, so they are stroked and filled like the letters.
//
// Why that is cheap enough. It happens once, when the text or the size of the item
// changes; from then on the text is a texture like any picture, and showing it costs
// the GPU one rectangle. Nothing is drawn again while a slide just sits on the output.
//
// Why line breaks never move. The layout is always done in slide units (the slide's own
// coordinates, usually 1920 by 1080) and the painter is scaled by `unit` to whatever
// size the item really is. A line that breaks after a word on the output breaks after
// the same word in a thumbnail a tenth of the size.
class StrokedText : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    // A RichText value, as produced by ProDocument.
    Q_PROPERTY(QVariant content MEMBER m_content NOTIFY contentChanged)
    // Output pixels per slide unit.
    Q_PROPERTY(qreal unit MEMBER m_unit NOTIFY unitChanged)
    // Slide units by which the item extends beyond the text box on every side, so strokes
    // on glyphs at the edge of the box are not clipped.
    Q_PROPERTY(qreal bleed MEMBER m_bleed NOTIFY bleedChanged)
    // Qt.AlignTop / Qt.AlignVCenter / Qt.AlignBottom
    Q_PROPERTY(int verticalAlignment MEMBER m_vAlign NOTIFY verticalAlignmentChanged)
    // Slide units the text keeps clear of each edge of its box.
    Q_PROPERTY(qreal insetLeft MEMBER m_insetLeft NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetTop MEMBER m_insetTop NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetRight MEMBER m_insetRight NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetBottom MEMBER m_insetBottom NOTIFY insetsChanged)
    // A colour to fill in behind each line of text, for a shape whose fill is masked to
    // its text's lines; transparent for none. `lineFillStyle` says how wide each line's
    // bar is: 0 the whole box, 1 its own text, 2 the widest line's text. The offsets,
    // in slide units, add to a bar's width and height about its centre and move it.
    Q_PROPERTY(QColor lineFill MEMBER m_lineFill NOTIFY lineFillChanged)
    Q_PROPERTY(int lineFillStyle MEMBER m_lineFillStyle NOTIFY lineFillChanged)
    Q_PROPERTY(qreal lineFillWidthOffset MEMBER m_lineFillWidthOffset NOTIFY lineFillChanged)
    Q_PROPERTY(qreal lineFillHeightOffset MEMBER m_lineFillHeightOffset NOTIFY lineFillChanged)
    Q_PROPERTY(qreal lineFillHorizontalOffset MEMBER m_lineFillHorizontalOffset NOTIFY lineFillChanged)
    Q_PROPERTY(qreal lineFillVerticalOffset MEMBER m_lineFillVerticalOffset NOTIFY lineFillChanged)

public:
    explicit StrokedText(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

signals:
    void contentChanged();
    void unitChanged();
    void bleedChanged();
    void verticalAlignmentChanged();
    void insetsChanged();
    void lineFillChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QVariant m_content;
    qreal m_unit = 1;
    qreal m_bleed = 0;
    int m_vAlign = Qt::AlignVCenter;
    qreal m_insetLeft = 0;
    qreal m_insetTop = 0;
    qreal m_insetRight = 0;
    qreal m_insetBottom = 0;
    QColor m_lineFill = Qt::transparent;
    int m_lineFillStyle = 0;
    qreal m_lineFillWidthOffset = 0;
    qreal m_lineFillHeightOffset = 0;
    qreal m_lineFillHorizontalOffset = 0;
    qreal m_lineFillVerticalOffset = 0;
};
