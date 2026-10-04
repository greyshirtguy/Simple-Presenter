#pragma once

#include <QQuickPaintedItem>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// Styled text rasterised once on the CPU into a texture: each run's glyph outlines are
// laid out with QTextLayout, stroked, then filled. Layout happens in slide units and the
// painter is scaled by `unit`, so line breaks are identical at every output size.
// Repaints only when a property or the item size changes.
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

public:
    explicit StrokedText(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

signals:
    void contentChanged();
    void unitChanged();
    void bleedChanged();
    void verticalAlignmentChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QVariant m_content;
    qreal m_unit = 1;
    qreal m_bleed = 0;
    int m_vAlign = Qt::AlignVCenter;
};
