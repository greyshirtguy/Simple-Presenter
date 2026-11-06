#pragma once

#include "textlayout.h"

#include <QColor>
#include <QImage>
#include <optional>

// Says, on the app's thread, that a picture painted ahead has been kept (StrokedText's): for
// what waits for the pictures of a slide before it is given to be shown (PaintAhead)
class FrameClock;

class PicturesKept : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void kept();
};
#include <QPointer>
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
//
// Text that suits its size to its box. A box can be set to make its text smaller until
// it fits, larger until it fills the box, or either (`fit`). What a box of a stage layout
// shows is whatever is live, a line of a song or six, so such boxes are mostly set that
// way; a slide's own text can be too. The size that suits is found by laying the text
// out at one size after another, which is done once for each text the box is given.
//
// Letters filled with something other than a colour (textfills.h). A gradient is drawn
// here, with the letters, across the box of the letters it fills. A picture, a cut-out of
// the box, and a blur or an invert of what is behind, are the graphics chip's to draw:
// their letters are left unfilled here (their outlines are drawn), and the same text drawn
// again with `maskOf` set to the fill's number is only those letters, in white, for the
// chip to draw the fill through (TextFills.qml). Where nothing is drawn over the text
// (`layeredFills` false: Scrolling Text, whose words are a texture moved by a shader),
// such letters are filled with their own colour, as the RTF has it.
//
// The one text that is not drawn once and left. A timer that shows its hundredths
// changes thirty times a second, and drawing all of it again that often, large, on a
// full-screen output, took over a quarter of a processor core. Such text arrives as a
// `replacement`, and is laid out glyph by glyph: when it changes, the glyphs that are
// the same glyph in the same place as before are left as they are, and only the part of
// the image under the others is cleared, drawn and sent to the graphics chip again,
// which for a running clock is its last digit or two.
//
// Painted ahead, on a thread of its own, for an audience screen's slides. Drawing the
// words is most of what a new slide costs the screen: 6 or 7 ms for a slide's words at
// 1080p on the laptop this is made on, in the sync of the frame that first shows them,
// while the app's own thread and the screen's drawing both wait; and a video playing
// under them misses a frame of its own for it (measured 2026-10-31: the video hitch at
// slide changes). So a text inside an item whose `paintsAhead` is true (a Slide of an
// audience screen's slide layer, or of a picture of a screen: see Slide) paints its
// picture on one of two threads kept for that, from a copy of everything it is drawn from
// (`Look`), whenever any of that changes, its shadow with it (castShadow), where a layer
// of Qt's draws it everywhere else; the sync then only copies the picture. Meanwhile it
// counts itself on that item's `picturesPending`, which the screen waits on before the
// slide is shown (Slide.ready, TransitionLayer). The pictures are kept, about 32 MB of
// them, each cut to what is not see-through of it, for a text drawn the same again: going
// back to a slide, the same slide on another screen of the same size, the slides likely
// next, painted before they are asked for, and a slide asked for, painted before it is given
// to be shown (PaintAhead); a text that finds its picture kept is ready at once. Not text
// that changes often, a running timer's (`ticking`), which keeps the drawing above; and
// anywhere else, as it always was.
//
// A timer's text on a screen playing a video changes as the video's frames come. It
// changes by the second, or thirty times in one for its hundredths, at moments of its own,
// and the frame of the screen drawn for each change came in the refresh before a frame of
// the video as often as not, which put that frame on the screen a refresh late (see
// src/framepacing.h: twenty-nine frames a second of the output's own, measured over a video
// with a timer's hundredths on it, 2026-11-06). So on such a screen the change waits for the
// video's next frame and is drawn in it (replace, by FrameClock::waitForVideoFrame), a
// thirtieth of a second late at most; anywhere else, at once.
class StrokedText : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    // A RichText value, as produced by ProDocument.
    Q_PROPERTY(QVariant content MEMBER m_content NOTIFY contentChanged)
    // Words to draw in place of the content's own, as a string, or undefined to draw
    // the content. They are set in the style the content starts in, so that an element
    // whose words come from elsewhere (a timer's time, say) still looks as it was made
    // to look. Or text that has its own fonts, sizes and colours (a RichText), which
    // is then drawn as it is, in place of the content and with nothing of the
    // content's style: a slide's notes, shown as they were written.
    Q_PROPERTY(QVariant replacement MEMBER m_replacement NOTIFY replacementChanged)
    // The colour the replacement's words are filled with, in place of the content's, or
    // undefined for the content's own: a timer's colour by its Color Triggers
    // (colortriggers.h). It changes seldom, so a change of it draws all of the text again.
    Q_PROPERTY(QVariant replacementColor MEMBER m_replacementColor NOTIFY replacementColorChanged)
    // Output pixels per slide unit.
    Q_PROPERTY(qreal unit MEMBER m_unit NOTIFY unitChanged)
    // Slide units by which the item extends beyond the text box on every side, so strokes
    // on glyphs at the edge of the box are not clipped.
    Q_PROPERTY(qreal bleed MEMBER m_bleed NOTIFY bleedChanged)
    // Qt.AlignTop / Qt.AlignVCenter / Qt.AlignBottom
    Q_PROPERTY(int verticalAlignment MEMBER m_vAlign NOTIFY verticalAlignmentChanged)
    // Whether the text's size is changed to suit the box, as ProPresenter's files number
    // it: 2 smaller if it does not fit, 3 larger if there is room, 4 either; anything
    // else, drawn at the size it has.
    Q_PROPERTY(int fit MEMBER m_fit NOTIFY fitChanged)
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
    // The number of one of the text's fills (RichText::fillEffects), to draw only the
    // letters it fills, in white, and nothing else; -1, as it begins, for the text
    Q_PROPERTY(int maskOf MEMBER m_maskOf NOTIFY maskOfChanged)
    // Whether letters filled with a picture, a cut-out, a blur or an invert are left
    // unfilled, for TextFills to draw over them (true, as it begins), or filled with
    // their own colour (false: see above)
    Q_PROPERTY(bool layeredFills MEMBER m_layeredFills NOTIFY maskOfChanged)
    // The text's shadow, as [colour, offset across, offset down, radius] (an element's
    // textShadow values, in slide units), or nothing for none: drawn here only where the
    // text paints ahead, into its picture (see above and castShadow), the layer that draws
    // it everywhere else being switched off there (SlideElement)
    Q_PROPERTY(QVariant shadow MEMBER m_shadow NOTIFY shadowChanged)
    // Whether a replacement that is text changes often, as a running timer's does, or the
    // clock's: it is then drawn as it changes, a glyph at a time (see above), and never
    // painted ahead. True, as it begins; an element linked to what changes only now and then
    // (a slide's words, the stage's message) says false, and its words are painted ahead
    // where the rest are (SlideElement)
    Q_PROPERTY(bool ticking MEMBER m_ticking NOTIFY tickingChanged)
    // Whether it paints ahead (see above)
    Q_PROPERTY(bool paintingAhead READ isPaintingAhead NOTIFY paintingAheadChanged)

public:
    explicit StrokedText(QQuickItem *parent = nullptr);
    ~StrokedText() override;

    void paint(QPainter *painter) override;
    // (Looked into first as its bindings are first worked out: by then the slide it is in
    // is round it, and an element's shadow is never set up for the layer only to be taken
    // down again as it is completed)
    bool isPaintingAhead();

    // Everything the text is drawn from, as the properties and the item's size say, and
    // what was worked out from it last (the size fitted text was found to suit): a copy is
    // drawn on another thread as the item itself is drawn.
    struct Look
    {
        QVariant content;
        QVariant replacement;
        QVariant replacementColor;
        qreal width = 0;
        qreal height = 0;
        qreal unit = 1;
        qreal bleed = 0;
        int vAlign = Qt::AlignVCenter;
        int fit = 0;
        qreal insetLeft = 0;
        qreal insetTop = 0;
        qreal insetRight = 0;
        qreal insetBottom = 0;
        QColor lineFill = Qt::transparent;
        int lineFillStyle = 0;
        qreal lineFillWidthOffset = 0;
        qreal lineFillHeightOffset = 0;
        qreal lineFillHorizontalOffset = 0;
        qreal lineFillVerticalOffset = 0;
        int maskOf = -1;
        bool layeredFills = true;
        QColor shadowColor = Qt::transparent;
        qreal shadowOffsetX = 0;
        qreal shadowOffsetY = 0;
        qreal shadowRadius = 0;
        // The last text a size was found for, the box it was found for, and the size, as
        // a multiple of the text's own: see fitted()
        mutable RichText fitShape;
        mutable QSizeF fitRoom;
        mutable int fitKind = 0;
        mutable qreal fitScale = 1;

        RichText shown() const;
        RichText fitted() const;
        QSizeF box() const;
        QPointF origin(const TextLayoutResult &layout) const;
        // Draws the text with `painter`, which draws in the item's own coordinates; `live`
        // is the replacement's layout where that is known (see replace())
        void paint(QPainter *painter, const TextLayoutResult *live) const;
        // Puts the shadow of what `picture` holds under it, if there is a shadow
        void castShadow(QImage &picture) const;
        // Whether a text drawn from this and one drawn from `other` look the same: all they
        // are drawn from but what was worked out last (the fit), the words last
        bool sameAs(const Look &other) const;
    };

    // A picture painted ahead: the part of the text's picture that is not see-through, and
    // where that part is in it, in the picture's own pixels (none, for a picture of nothing)
    struct Picture
    {
        QImage image;
        QPoint at;
    };

    // The look an element map gives the words of an audience screen's slide (SlideElement's
    // StrokedText, which this follows: the two are to be kept in step), drawn at `unit`
    // pixels to a slide unit, with or without shadows (Slide's `effects`); none for an
    // element whose words are not drawn so on a screen: none, a link to something that
    // changes (a timer, the clock, another slide's words...), or words that scroll
    static std::optional<Look> lookOf(const QVariantMap &element, qreal unit, bool effects);
    // Has `looks` painted ahead, `ratio` of the pictures' pixels to the items', and kept,
    // for texts that come to be drawn so (PaintAhead): the best first, and the best kept
    // longest; those kept already are kept on, and those being painted are not painted again
    static void paintAhead(const QList<Look> &looks, qreal ratio);
    // The same, before any painted ahead of being wanted, for a slide that is to be shown
    // (PaintAhead::paintFirst): true if they are all kept already. Those waiting to be
    // painted ahead are moved up.
    static bool paintFirst(const QList<Look> &looks, qreal ratio);
    // Whether the pictures of all of `looks` are kept
    static bool allKept(const QList<Look> &looks, qreal ratio);
    // What says that a picture has been kept
    static PicturesKept *picturesKept();

signals:
    void contentChanged();
    void replacementChanged();
    void replacementColorChanged();
    void unitChanged();
    void bleedChanged();
    void verticalAlignmentChanged();
    void fitChanged();
    void insetsChanged();
    void lineFillChanged();
    void maskOfChanged();
    void shadowChanged();
    void tickingChanged();
    void paintingAheadChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;

private:
    Look look() const;
    void keepFit(const Look &look) const;
    RichText shown() const;
    RichText fitted() const;
    QSizeF box() const;
    QPointF origin(const TextLayoutResult &layout) const;
    void replace();
    bool waitForVideoFrame();
    void relayout();
    QRect changedPart(const TextLayoutResult &before, const TextLayoutResult &after) const;
    // Painting ahead (see above)
    void changed();
    bool paintingAhead() const;
    void findGroup(bool tell = true);
    void setPending(bool pending);
    void paintPicture();
    void acceptPicture(quint64 generation, const Picture &picture, const Look &painted);
    void sayIfAheadChanged(bool tell = true);
    qreal ratio() const;
    bool takeKept();
    friend void picturePainted(const Look &look, qreal ratio, const Picture &picture);

    QVariant m_content;
    QVariant m_replacement;
    QVariant m_replacementColor;
    // The replacement as it was last laid out, and whether that still holds
    TextLayoutResult m_live;
    bool m_liveKnown = false;
    qreal m_unit = 1;
    qreal m_bleed = 0;
    int m_vAlign = Qt::AlignVCenter;
    int m_fit = 0;
    // The last text a size was found for, the box it was found for, and the size, as
    // a multiple of the text's own: see fitted()
    mutable RichText m_fitShape;
    mutable QSizeF m_fitRoom;
    mutable int m_fitKind = 0;
    mutable qreal m_fitScale = 1;
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
    int m_maskOf = -1;
    bool m_layeredFills = true;
    // Painting ahead: the item that asks for it (a Slide with `paintsAhead`), if any;
    // whether this is counted on its `picturesPending`; whether a painting is to be
    // started; which change of what the text is drawn from this is (counted), and which
    // the picture painted ahead is of; and whether that came while the text could not be
    // seen, and is to be handed to the window when it can
    QPointer<QQuickItem> m_group;
    bool m_pending = false;
    bool m_queued = false;
    quint64 m_generation = 0;
    quint64 m_pictureGeneration = 0;
    Picture m_picture;
    bool m_unshown = false;
    QVariant m_shadow;
    bool m_ticking = true;
    // What waits for the video's next frame, on a screen playing one, for a change of the
    // replacement to be drawn in (see replace); made when first wanted
    FrameClock *m_pace = nullptr;
    // Whether the item that asks for painting ahead has been looked for, and whether this
    // was said last to be painting ahead
    bool m_groupKnown = false;
    bool m_saidAhead = false;
    QMetaObject::Connection m_groupShown;
};
