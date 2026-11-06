#include "strokedtext.h"

#include "frameclock.h"
#include "richtext.h"
#include "textshadow.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QPen>
#include <QQuickWindow>
#include <QSGNode>
#include <QMutex>
#include <QThreadPool>
#include <QtMath>

#include <cstring>
#include <sys/mman.h>

namespace {

// Whether a run laid out glyph by glyph can be drawn that way and look as it would drawn
// whole. It can if nothing of it is seen through: where two glyphs overlap, or one
// glyph's stroke runs over its neighbour, drawing them one after another lays colour on
// colour, which only shows if the colour is not solid.
bool drawnByGlyph(const TextLayoutResult::Outline &outline)
{
    const TextRun &format = outline.format;
    // (Nor letters with a fill of their own: a gradient goes across all of them)
    return !outline.pieces.isEmpty() && (!format.fillVisible || format.fill.alpha() == 255)
        && (format.strokeWidth <= 0 || format.stroke.alpha() == 255) && format.fillEffect < 0;
}

// The brush of a gradient fill (textfills.h) across `box`, the letters it fills, as a
// shape's gradient is laid across its box (SlideElement): its angle anticlockwise from
// pointing right, its first stop at the tail.
QBrush gradientBrush(const QVariantMap &fill, const QRectF &box)
{
    QGradientStops stops;
    for (const QVariant &stop : fill.value("stops").toList()) {
        const QVariantMap s = stop.toMap();
        stops.append({std::clamp(s.value("position").toDouble(), 0.0, 1.0), s.value("color").value<QColor>()});
    }
    if (stops.isEmpty())
        return QBrush(Qt::white);
    if (fill.value("radial").toBool()) {
        QRadialGradient radial(box.center(), std::max(box.width(), box.height()) / 2);
        radial.setStops(stops);
        return QBrush(radial);
    }
    const qreal along = qDegreesToRadians(fill.value("angle").toDouble());
    const qreal reach = qAbs(box.width() / 2 * qCos(along)) + qAbs(box.height() / 2 * qSin(along));
    const QPointF c = box.center();
    QLinearGradient linear(QPointF(c.x() - qCos(along) * reach, c.y() + qSin(along) * reach),
                           QPointF(c.x() + qCos(along) * reach, c.y() - qSin(along) * reach));
    linear.setStops(stops);
    return QBrush(linear);
}

// The threads pictures are painted ahead on. Two: the laptop this is made on has two
// cores, which the video's decoding and every window's drawing share with them, and two
// let a slide of several boxes be painted in about the time of its largest. Waited for
// as the app ends, so that none is left painting with fonts that are going.
QThreadPool *painters()
{
    static QThreadPool *pool = [] {
        auto *made = new QThreadPool(qApp);
        made->setObjectName(QStringLiteral("text pictures"));
        made->setMaxThreadCount(2);
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, made, [made] {
            made->clear();
            made->waitForDone();
        });
        return made;
    }();
    return pool;
}

// The pictures painted ahead, kept for texts that come to be drawn as they were painted:
// those of the slides likely to be shown next (PaintAhead), and of every text an audience
// screen has shown since, for going back to it. The least lately wanted is let go first,
// past 32 MB: some twenty pictures of a slide's words on a 1080p screen, cut to what is
// not see-through of them (cropped), five on a 4K one. (Only on the app's thread.)
struct Kept
{
    StrokedText::Look look;
    qreal ratio;
    StrokedText::Picture picture;
    quint64 wanted;
};

QList<Kept> &kept()
{
    static QList<Kept> pictures;
    return pictures;
}

quint64 nextWant()
{
    static quint64 wants = 0;
    return ++wants;
}

const qsizetype keptBytes = 32 * 1024 * 1024;

Kept *findKept(const StrokedText::Look &look, qreal ratio)
{
    for (Kept &one : kept()) {
        if (one.ratio == ratio && one.look.sameAs(look))
            return &one;
    }
    return nullptr;
}

void keep(const StrokedText::Look &look, qreal ratio, const StrokedText::Picture &picture)
{
    if (Kept *there = findKept(look, ratio)) {
        there->wanted = nextWant();
        return;
    }
    kept().append({look, ratio, picture, nextWant()});
    qsizetype bytes = 0;
    for (const Kept &one : std::as_const(kept()))
        bytes += one.picture.image.sizeInBytes();
    while (bytes > keptBytes && kept().size() > 1) {
        qsizetype oldest = 0;
        for (qsizetype i = 1; i < kept().size(); ++i) {
            if (kept().at(i).wanted < kept().at(oldest).wanted)
                oldest = i;
        }
        bytes -= kept().at(oldest).picture.image.sizeInBytes();
        kept().removeAt(oldest);
    }
}

// What is being painted, and the texts waiting for each picture (the text that asked for
// it, and any that came to want the same while it was being painted)
struct Painting
{
    StrokedText::Look look;
    qreal ratio;
    QList<QPointer<StrokedText>> waiting;
};

QList<Painting> &painting()
{
    static QList<Painting> under;
    return under;
}

Painting *findPainting(const StrokedText::Look &look, qreal ratio)
{
    for (Painting &one : painting()) {
        if (one.ratio == ratio && one.look.sameAs(look))
            return &one;
    }
    return nullptr;
}

// A picture whose pixels are memory mapped for it by the system, and given back to the
// system as the picture goes. (Pictures made on the painters' threads, and let go on the
// app's, were kept by the C library for those threads to make the next, past any need:
// 64 MB more held by the end of the benchmark, 2026-10-31. Only these pictures go so: the
// rest of the app's memory is the library's to keep.) The memory comes cleared, which for
// such a picture is see-through.
struct Mapping
{
    void *memory;
    size_t length;
};

QImage mappedPicture(QSize pixels)
{
    const qsizetype stride = qsizetype(pixels.width()) * 4;
    const size_t length = size_t(stride) * size_t(pixels.height());
    void *memory = length > 0 ? mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) : MAP_FAILED;
    if (memory == MAP_FAILED) {
        QImage made(pixels, QImage::Format_ARGB32_Premultiplied);
        made.fill(Qt::transparent);
        return made;
    }
    return QImage(static_cast<uchar *>(memory), pixels.width(), pixels.height(), stride, QImage::Format_ARGB32_Premultiplied,
                  [](void *info) {
                      const auto *mapping = static_cast<Mapping *>(info);
                      munmap(mapping->memory, mapping->length);
                      delete mapping;
                  },
                  new Mapping {memory, length});
}

// The part of a picture that is not see-through, and where it is (none, for a picture of
// nothing): a slide's words fill a part of their box, often a small one, and only that
// part is kept, and copied to the screen's frame when they are shown. (A row is looked
// along from its start to its first pixel that is not see-through, which in a row of
// words is soon found, and from its end to its last, only as far as the furthest found.)
StrokedText::Picture cropped(const QImage &whole)
{
    const int width = whole.width();
    const int height = whole.height();
    int top = -1;
    int bottom = -1;
    int left = width;
    int right = -1;
    for (int y = 0; y < height; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(whole.constScanLine(y));
        int x = 0;
        while (x < width && (line[x] >> 24) == 0)
            ++x;
        if (x == width)
            continue;
        left = std::min(left, x);
        int end = width - 1;
        while (end > right && (line[end] >> 24) == 0)
            --end;
        right = std::max(right, end);
        if (top < 0)
            top = y;
        bottom = y;
    }
    if (top < 0)
        return {};
    // (Most of it: kept as it is, rather than copied)
    const QRect part(left, top, right - left + 1, bottom - top + 1);
    if (qint64(part.width()) * part.height() * 10 > qint64(width) * height * 9)
        return {whole, QPoint(0, 0)};
    QImage image = mappedPicture(part.size());
    image.setDevicePixelRatio(whole.devicePixelRatio());
    for (int y = 0; y < part.height(); ++y)
        memcpy(image.scanLine(y), whole.constScanLine(part.top() + y) + part.left() * 4, size_t(part.width()) * 4);
    return {image, part.topLeft()};
}

} // namespace

PicturesKept *StrokedText::picturesKept()
{
    static PicturesKept *news = new PicturesKept(qApp);
    return news;
}

// A picture painted, on the app's thread: kept, and handed to the texts waiting for it
void picturePainted(const StrokedText::Look &look, qreal ratio, const StrokedText::Picture &picture)
{
    keep(look, ratio, picture);
    emit StrokedText::picturesKept()->kept();
    QList<QPointer<StrokedText>> waiting;
    for (qsizetype i = 0; i < painting().size(); ++i) {
        if (painting().at(i).ratio == ratio && painting().at(i).look.sameAs(look)) {
            waiting = painting().takeAt(i).waiting;
            break;
        }
    }
    // (Each takes it if it still wants it: what it is drawn from may have changed since)
    for (const QPointer<StrokedText> &text : std::as_const(waiting)) {
        if (text && text->paintingAhead() && text->m_pending)
            text->takeKept();
    }
}

namespace {

// The pictures waiting to be painted, in the order the painters come to them: a text's
// own, and the words of a slide asked for (paintFirst), before those painted ahead of being
// wanted. A queue of the app's own, and not the pool's: a picture painted ahead that comes
// to be wanted while it waits is moved up (hurry). (Under the lock: the painters take from
// it on their threads.)
struct Job
{
    StrokedText::Look look;
    qreal ratio;
    bool soon;
};

QMutex &jobsLock()
{
    static QMutex lock;
    return lock;
}

QList<Job> &jobs()
{
    static QList<Job> waiting;
    return waiting;
}

// (Behind the others wanted soon, before the rest)
void queue(Job job)
{
    qsizetype at = jobs().size();
    if (job.soon) {
        at = 0;
        while (at < jobs().size() && jobs().at(at).soon)
            ++at;
    }
    jobs().insert(at, std::move(job));
}

// A picture painted ahead, wanted now: up the queue, if it is waiting there still
void hurry(const StrokedText::Look &look, qreal ratio)
{
    QMutexLocker locked(&jobsLock());
    for (qsizetype i = 0; i < jobs().size(); ++i) {
        if (!jobs().at(i).soon && jobs().at(i).ratio == ratio && jobs().at(i).look.sameAs(look)) {
            Job job = jobs().takeAt(i);
            job.soon = true;
            queue(std::move(job));
            return;
        }
    }
}

// Paints the first picture of the queue, on a painter's thread, to be kept (picturePainted)
void paintNext()
{
    Job job;
    {
        QMutexLocker locked(&jobsLock());
        if (jobs().isEmpty())
            return;
        job = jobs().takeFirst();
    }
    const StrokedText::Look &look = job.look;
    const qreal ratio = job.ratio;
    const QSize pixels(qCeil(look.width * ratio), qCeil(look.height * ratio));
    {
        StrokedText::Picture picture;
        if (!pixels.isEmpty()) {
            QImage whole = mappedPicture(pixels);
            whole.setDevicePixelRatio(ratio);
            {
                QPainter painter(&whole);
                look.paint(&painter, nullptr);
            }
            look.castShadow(whole);
            picture = cropped(whole);
        }
        // (`look` with what painting it worked out: the fit)
        QMetaObject::invokeMethod(qApp, [look, ratio, picture] { picturePainted(look, ratio, picture); }, Qt::QueuedConnection);
    }
}

// Paints `look`, to be kept (picturePainted): `soon`, before those painted ahead of being
// wanted. (A job for the pool for each picture queued, which takes the first of the queue.)
void startPainting(const StrokedText::Look &look, qreal ratio, bool soon)
{
    painting().append({look, ratio, {}});
    {
        QMutexLocker locked(&jobsLock());
        queue({look, ratio, soon});
    }
    painters()->start(paintNext);
}

} // namespace

StrokedText::StrokedText(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    // Anything but the replacement changing means all of it is drawn again, and that
    // what was worked out for the replacement no longer holds.
    connect(this, &StrokedText::contentChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::unitChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::bleedChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::verticalAlignmentChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::fitChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::insetsChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::lineFillChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::replacementColorChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::maskOfChanged, this, &StrokedText::changed);
    connect(this, &StrokedText::shadowChanged, this, [this] {
        // (Drawn here only where it paints ahead: see `shadow`)
        if (paintingAhead())
            changed();
    });
    connect(this, &StrokedText::replacementChanged, this, &StrokedText::replace);
    connect(this, &StrokedText::tickingChanged, this, &StrokedText::replace);
}

StrokedText::~StrokedText()
{
    setPending(false);
}

void StrokedText::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        changed();
}

void StrokedText::componentComplete()
{
    QQuickPaintedItem::componentComplete();
    findGroup();
    if (paintingAhead())
        changed();
}

// (The first look, made as this is read, says nothing of it: it is being read)
bool StrokedText::isPaintingAhead()
{
    if (!m_groupKnown)
        findGroup(false);
    return paintingAhead();
}

// (Looked for again as the item is put into a window: an element a Repeater makes is
// completed before it is put inside the slide, and only then can its slide be found)
void StrokedText::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickPaintedItem::itemChange(change, value);
    if ((change == ItemParentHasChanged || change == ItemSceneChange) && isComponentComplete()) {
        const bool before = paintingAhead();
        findGroup();
        if (paintingAhead() && !before)
            changed();
    }
}

// Painted ahead and the picture not in yet, nothing is done with the window's picture of
// the text: it is not painted here, nor sent to the graphics chip, and what was on show
// stays as it was until the picture is in. (Qt asks for a new text's picture the first
// time the window is drawn after it is made, out of sight or not: a slide's words were
// painted there, as the screen waited, though the slide was not to be shown until they
// had been painted ahead.)
QSGNode *StrokedText::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    if (paintingAhead() && m_pictureGeneration != m_generation)
        return node;
    return QQuickPaintedItem::updatePaintNode(node, data);
}

StrokedText::Look StrokedText::look() const
{
    Look l;
    l.content = m_content;
    l.replacement = m_replacement;
    l.replacementColor = m_replacementColor;
    l.width = width();
    l.height = height();
    l.unit = m_unit;
    l.bleed = m_bleed;
    l.vAlign = m_vAlign;
    l.fit = m_fit;
    l.insetLeft = m_insetLeft;
    l.insetTop = m_insetTop;
    l.insetRight = m_insetRight;
    l.insetBottom = m_insetBottom;
    l.lineFill = m_lineFill;
    l.lineFillStyle = m_lineFillStyle;
    l.lineFillWidthOffset = m_lineFillWidthOffset;
    l.lineFillHeightOffset = m_lineFillHeightOffset;
    l.lineFillHorizontalOffset = m_lineFillHorizontalOffset;
    l.lineFillVerticalOffset = m_lineFillVerticalOffset;
    l.maskOf = m_maskOf;
    l.layeredFills = m_layeredFills;
    if (const QVariantList shadow = m_shadow.toList(); shadow.size() >= 4) {
        l.shadowColor = shadow.at(0).value<QColor>();
        l.shadowOffsetX = shadow.at(1).toReal();
        l.shadowOffsetY = shadow.at(2).toReal();
        l.shadowRadius = shadow.at(3).toReal();
    }
    l.fitShape = m_fitShape;
    l.fitRoom = m_fitRoom;
    l.fitKind = m_fitKind;
    l.fitScale = m_fitScale;
    return l;
}

// What a drawing of the text found the size of fitted text to be, kept for the next
void StrokedText::keepFit(const Look &look) const
{
    m_fitShape = look.fitShape;
    m_fitRoom = look.fitRoom;
    m_fitKind = look.fitKind;
    m_fitScale = look.fitScale;
}

RichText StrokedText::shown() const
{
    return look().shown();
}

RichText StrokedText::fitted() const
{
    const Look l = look();
    const RichText text = l.fitted();
    keepFit(l);
    return text;
}

QSizeF StrokedText::box() const
{
    return look().box();
}

QPointF StrokedText::origin(const TextLayoutResult &layout) const
{
    return look().origin(layout);
}

// What there is to draw: the content, or the replacement in the content's style; or a
// replacement that is text with a style of its own, as it is.
RichText StrokedText::Look::shown() const
{
    if (replacement.userType() == qMetaTypeId<RichText>())
        return replacement.value<RichText>();
    const RichText text = content.value<RichText>();
    if (replacement.typeId() != QMetaType::QString)
        return text;
    TextRun format = text.firstRun();
    if (const QColor color = replacementColor.value<QColor>(); replacementColor.isValid() && color.isValid())
        format.fill = color;
    RichText words = RichText::plain(replacement.toString(), format,
                                     text.paragraphs.isEmpty() ? Qt::AlignHCenter : text.paragraphs.first().alignment);
    // (The fill the first run has, by its number, is the content's)
    words.fillEffects = text.fillEffects;
    return words;
}

// What there is to draw at the size it is drawn at: as it is, or made smaller or larger
// to suit the box, if the box is set to do that.
//
// The size is found for the shape of the text and not for the text itself: with every
// digit taken for a nought. A timer's time then keeps one size while it runs, and does
// not shiver as a 1 gives way to a 0; it changes size only when it gains or loses a
// digit. (It also means the search is made once for a running timer, and not thirty
// times a second for one that shows its hundredths.)
RichText StrokedText::Look::fitted() const
{
    const RichText text = shown();
    const QSizeF room = box();
    if (fit < 2 || fit > 4 || room.width() <= 0 || room.height() <= 0)
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
    if (fitKind != fit || fitRoom != room || !(fitShape == shape)) {
        fitScale = fittingScale(shape, room, fit);
        fitShape = shape;
        fitRoom = room;
        fitKind = fit;
    }
    return fitScale == 1 ? text : scaledText(text, fitScale);
}

// The box the text is laid out in, in slide units: empty if there is no room for any.
QSizeF StrokedText::Look::box() const
{
    if (unit <= 0)
        return {};
    return QSizeF(width / unit - 2 * bleed - insetLeft - insetRight, height / unit - 2 * bleed - insetTop - insetBottom);
}

// Where the layout's own top left corner is in the item, in slide units.
QPointF StrokedText::Look::origin(const TextLayoutResult &layout) const
{
    const qreal vFactor = vAlign & Qt::AlignVCenter ? 0.5 : vAlign & Qt::AlignBottom ? 1.0 : 0.0;
    return QPointF(bleed + insetLeft, bleed + insetTop + (box().height() - layout.height) * vFactor);
}

// The replacement has changed. If the one before it is known, and both are drawn glyph
// by glyph, only what has changed between them is drawn again. On a screen that is playing
// a video, not now but as the video's next frame comes, to be drawn in the frame the
// video's own is drawn in and in none of its own, which took the video's turn as often as
// not (see the top); changed again before the frame comes, it is the latest that is drawn
// then. Where no video plays, at once, as it always was.
void StrokedText::replace()
{
    const bool live = m_replacement.typeId() == QMetaType::QString && box().width() > 0 && box().height() > 0;
    sayIfAheadChanged();
    // (Painted ahead, all of it: what changes now and then, see `ticking`)
    if (!live || paintingAhead()) {
        changed();
        return;
    }
    // (A timer's running text is never painted ahead: see above)
    setPending(false);
    if (m_ticking && waitForVideoFrame())
        return;
    relayout();
}

// Whether the change is to wait for the next frame of a video playing on the window this is
// in (FrameClock::waitForVideoFrame: false, and nothing to wait for, where none is), to be
// made as it comes
bool StrokedText::waitForVideoFrame()
{
    if (!window())
        return false;
    if (!m_pace) {
        // (Made first and put inside this after: a child made with its parent is told of the
        // window while it is being made, before it is a FrameClock that can take it in. Found
        // by the frames test, 2026-11-06: nothing waited, and nothing failed to.)
        m_pace = new FrameClock;
        m_pace->setParent(this);
        m_pace->setParentItem(this);
        connect(m_pace, &FrameClock::videoFrameCame, this, &StrokedText::relayout);
    }
    return m_pace->waitForVideoFrame();
}

// Lays the replacement out, and has what differs from the last drawn again
void StrokedText::relayout()
{
    // (Gone from a string to the content's own words while this waited: drawn as those)
    if (m_replacement.typeId() != QMetaType::QString || box().width() <= 0 || box().height() <= 0)
        return;
    const TextLayoutResult before = m_live;
    const bool known = m_liveKnown;
    m_live = layoutText(fitted(), box().width(), true);
    m_liveKnown = true;

    const QRect part = known ? changedPart(before, m_live) : QRect();
    if (part.isNull())
        update();
    else
        update(part);
}

// What of the item has to be drawn again to turn one layout of the replacement into
// another: the glyphs that are not the same glyph in the same place in both, with what
// their strokes add. Null if that cannot be said, and all of it has to be.
QRect StrokedText::changedPart(const TextLayoutResult &before, const TextLayoutResult &after) const
{
    // A fill behind the lines goes by how wide the lines are.
    if (m_lineFill.alpha() > 0 || before.height != after.height || before.outlines.size() != after.outlines.size())
        return {};

    QRectF part;
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
                part |= was.pieces.at(g).bounds;
            if (g < is.pieces.size())
                part |= is.pieces.at(g).bounds;
        }
    }
    if (part.isEmpty())
        return {};

    // In the item's own pixels, with a pixel to spare for the soft edges.
    const QPointF at = origin(after);
    const QRectF placed = part.adjusted(-reach, -reach, reach, reach).translated(at);
    const QRect pixels = QRectF(placed.topLeft() * m_unit, placed.bottomRight() * m_unit).toAlignedRect().adjusted(-2, -2, 2, 2);
    return pixels & QRect(0, 0, qCeil(width()), qCeil(height()));
}

// Something the text is drawn from has changed: all of it is drawn again, now or ahead.
// Painted ahead, the picture there was stays on show until the new one is in (the screen
// does not show a slide until its pictures are: see Slide.ready).
void StrokedText::changed()
{
    m_liveKnown = false;
    ++m_generation;
    // (Until it is complete, what it is made with is still being set: componentComplete)
    if (!isComponentComplete())
        return;
    if (!paintingAhead()) {
        setPending(false);
        update();
        return;
    }
    // (A picture to hand is taken now: the slide is ready at once)
    if (takeKept())
        return;
    setPending(true);
    if (!m_queued) {
        // (Once all that is set at a time has been: a new slide sets much at once)
        m_queued = true;
        QMetaObject::invokeMethod(this, &StrokedText::paintPicture, Qt::QueuedConnection);
    }
}

// Whether this paints ahead: inside an item that asks it to, and not text that changes
// often, as a running timer's does (see above)
bool StrokedText::paintingAhead() const
{
    return m_group && (m_replacement.typeId() != QMetaType::QString || !m_ticking);
}

// The item that asks for painting ahead: the nearest round this with a `paintsAhead`
// property (a Slide), if that is true. (The search stops at the first that has one, so
// that a text costs no more than a few looks up the tree, as it is made, and again only
// if it is moved.)
void StrokedText::findGroup(bool tell)
{
    QQuickItem *found = nullptr;
    for (QQuickItem *around = parentItem(); around; around = around->parentItem()) {
        const QVariant asks = around->property("paintsAhead");
        if (asks.isValid()) {
            if (asks.toBool())
                found = around;
            break;
        }
    }
    m_groupKnown = true;
    if (found == m_group.data()) {
        sayIfAheadChanged(tell);
        return;
    }
    const bool pending = m_pending;
    setPending(false);
    disconnect(m_groupShown);
    m_group = found;
    setPending(pending && found);
    // A picture painted ahead while its slide was out of sight (waiting to be shown) is
    // handed to the window as the slide comes into sight, in the frame that shows it. (The
    // slide's and not the text's own sight: the letters a text fill is drawn through are
    // a text never seen itself, whose picture is used as it is shown: TextFills.)
    if (m_group) {
        m_groupShown = connect(m_group, &QQuickItem::visibleChanged, this, [this] {
            if (m_unshown && m_group && m_group->isVisible()) {
                m_unshown = false;
                update();
            }
        });
    }
    sayIfAheadChanged(tell);
}

void StrokedText::sayIfAheadChanged(bool tell)
{
    const bool ahead = paintingAhead();
    if (ahead == m_saidAhead)
        return;
    m_saidAhead = ahead;
    if (tell)
        emit paintingAheadChanged();
}

// Counted, or no longer counted, on the item's `picturesPending`
void StrokedText::setPending(bool pending)
{
    if (pending == m_pending)
        return;
    m_pending = pending;
    if (m_group)
        m_group->setProperty("picturesPending", m_group->property("picturesPending").toInt() + (pending ? 1 : -1));
}

qreal StrokedText::ratio() const
{
    return window() ? window()->effectiveDevicePixelRatio() : qGuiApp->devicePixelRatio();
}

// A picture of what the text is drawn from, if there is one to hand, taken at once: none
// needed for no words (an element that is a shape or a picture has its text too), or one
// kept (painted ahead, or for another text drawn the same). True if it was.
bool StrokedText::takeKept()
{
    const Look now = look();
    if (now.shown().isEmpty() || width() <= 0 || height() <= 0) {
        acceptPicture(m_generation, {}, now);
        return true;
    }
    if (const Kept *found = findKept(now, ratio())) {
        acceptPicture(m_generation, found->picture, found->look);
        return true;
    }
    return false;
}

// Paints the picture ahead, from a copy of all it is drawn from, on one of the threads
// kept for that (startPainting), or waits for the same picture being painted already;
// what is painted is taken only if nothing has changed since (picturePainted).
void StrokedText::paintPicture()
{
    m_queued = false;
    if (!paintingAhead()) {
        setPending(false);
        update();
        return;
    }
    if (!m_pending || takeKept())
        return;
    const Look snapshot = look();
    const qreal at = ratio();
    if (!findPainting(snapshot, at))
        startPainting(snapshot, at, true);
    findPainting(snapshot, at)->waiting.append(this);
}

std::optional<StrokedText::Look> StrokedText::lookOf(const QVariantMap &element, qreal unit, bool effects)
{
    if (!element.value("hasText").toBool() || element.value("linkKind").toString() != QLatin1String("none")
        || element.contains("scroll"))
        return std::nullopt;
    Look l;
    l.content = element.value("displayText");
    const bool shadow = effects && element.value("textShadowEnabled").toBool();
    // (SlideElement's textBleed, for words that draw their own shadow)
    l.bleed = 60 + (shadow ? qAbs(element.value("textShadowOffset").toReal()) + 2 * element.value("textShadowRadius").toReal() : 0);
    l.unit = unit;
    l.width = element.value("width").toReal() * unit + 2 * l.bleed * unit;
    l.height = element.value("height").toReal() * unit + 2 * l.bleed * unit;
    l.vAlign = element.value("verticalAlignment").toInt();
    l.fit = element.value("textScale").toInt();
    l.insetLeft = element.value("marginLeft").toReal();
    l.insetTop = element.value("marginTop").toReal();
    l.insetRight = element.value("marginRight").toReal();
    l.insetBottom = element.value("marginBottom").toReal();
    l.lineFill = element.value("fillEnabled").toBool() && element.value("fillLinesOnly").toBool()
        ? element.value("fillColor").value<QColor>() : QColor(Qt::transparent);
    l.lineFillStyle = element.value("lineMaskStyle").toInt();
    l.lineFillWidthOffset = element.value("lineMaskWidthOffset").toReal();
    l.lineFillHeightOffset = element.value("lineMaskHeightOffset").toReal();
    l.lineFillHorizontalOffset = element.value("lineMaskHorizontalOffset").toReal();
    l.lineFillVerticalOffset = element.value("lineMaskVerticalOffset").toReal();
    if (shadow) {
        l.shadowColor = element.value("textShadowColor").value<QColor>();
        l.shadowOffsetX = element.value("textShadowOffsetX").toReal();
        l.shadowOffsetY = element.value("textShadowOffsetY").toReal();
        l.shadowRadius = element.value("textShadowRadius").toReal();
    }
    return l;
}

void StrokedText::paintAhead(const QList<Look> &looks, qreal ratio)
{
    // (Those kept, wanted again from the last to the first: the least lately wanted are
    // let go first, and so the best last. Those painted now are wanted later still.)
    for (auto look = looks.crbegin(); look != looks.crend(); ++look) {
        if (Kept *there = findKept(*look, ratio))
            there->wanted = nextWant();
    }
    for (const Look &look : looks) {
        if (!look.shown().isEmpty() && look.width > 0 && look.height > 0 && !findKept(look, ratio) && !findPainting(look, ratio))
            startPainting(look, ratio, false);
    }
}

bool StrokedText::paintFirst(const QList<Look> &looks, qreal ratio)
{
    bool all = true;
    for (const Look &look : looks) {
        if (look.shown().isEmpty() || look.width <= 0 || look.height <= 0)
            continue;
        if (Kept *there = findKept(look, ratio)) {
            there->wanted = nextWant();
            continue;
        }
        all = false;
        if (findPainting(look, ratio))
            hurry(look, ratio);
        else
            startPainting(look, ratio, true);
    }
    return all;
}

bool StrokedText::allKept(const QList<Look> &looks, qreal ratio)
{
    for (const Look &look : looks) {
        if (!look.shown().isEmpty() && look.width > 0 && look.height > 0 && !findKept(look, ratio))
            return false;
    }
    return true;
}

namespace {

// Whether two numbers are the same but for how they were worked out (a sum in another
// order: QML's anchors and the sum in lookOf)
bool near(qreal a, qreal b)
{
    return qAbs(a - b) <= 1e-6 * std::max({qreal(1), qAbs(a), qAbs(b)});
}

} // namespace

bool StrokedText::Look::sameAs(const Look &other) const
{
    if (!near(width, other.width) || !near(height, other.height) || !near(unit, other.unit) || !near(bleed, other.bleed)
        || vAlign != other.vAlign || fit != other.fit || !near(insetLeft, other.insetLeft) || !near(insetTop, other.insetTop)
        || !near(insetRight, other.insetRight) || !near(insetBottom, other.insetBottom) || lineFill != other.lineFill
        || lineFillStyle != other.lineFillStyle || !near(lineFillWidthOffset, other.lineFillWidthOffset)
        || !near(lineFillHeightOffset, other.lineFillHeightOffset) || !near(lineFillHorizontalOffset, other.lineFillHorizontalOffset)
        || !near(lineFillVerticalOffset, other.lineFillVerticalOffset) || maskOf != other.maskOf || layeredFills != other.layeredFills
        || shadowColor != other.shadowColor || !near(shadowOffsetX, other.shadowOffsetX) || !near(shadowOffsetY, other.shadowOffsetY)
        || !near(shadowRadius, other.shadowRadius))
        return false;
    if (replacement.isValid() != other.replacement.isValid() || replacementColor.isValid() != other.replacementColor.isValid()
        || (replacement.isValid() && replacement != other.replacement)
        || (replacementColor.isValid() && replacementColor != other.replacementColor))
        return false;
    return content.value<RichText>() == other.content.value<RichText>();
}

void StrokedText::acceptPicture(quint64 generation, const Picture &picture, const Look &painted)
{
    // (A later change is being painted: this one is not wanted)
    if (generation != m_generation)
        return;
    m_picture = picture;
    m_pictureGeneration = generation;
    keepFit(painted);
    setPending(false);
    // (Not handed to the window while its slide is out of sight: that would be a frame of
    // the window's for nothing to be seen. See findGroup.)
    if (!m_group || m_group->isVisible()) {
        m_unshown = false;
        update();
    } else {
        m_unshown = true;
    }
}

void StrokedText::paint(QPainter *painter)
{
    // Painted ahead: the picture copied, as it is, where it is (see updatePaintNode; the
    // rest of the frame's picture of the item is cleared, as it is made)
    if (paintingAhead()) {
        if (!m_picture.image.isNull()) {
            painter->setCompositionMode(QPainter::CompositionMode_Source);
            painter->drawImage(QPointF(m_picture.at) / m_picture.image.devicePixelRatio(), m_picture.image);
        }
        return;
    }
    const Look l = look();
    const bool live = m_replacement.typeId() == QMetaType::QString;
    l.paint(painter, live && m_liveKnown ? &m_live : nullptr);
    keepFit(l);
}

void StrokedText::Look::paint(QPainter *painter, const TextLayoutResult *live) const
{
    const QSizeF room = box();
    if (room.width() <= 0 || room.height() <= 0)
        return;
    // A replacement's layout is worked out when it is set, where it is needed to know
    // what has changed; if that is not to hand, it is done here.
    const bool byGlyph = replacement.typeId() == QMetaType::QString;
    const TextLayoutResult layout = live ? *live : layoutText(fitted(), room.width(), byGlyph);
    if (layout.outlines.isEmpty() && layout.lines.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing);
    painter->scale(unit, unit);
    painter->translate(origin(layout));

    // Only the letters of one fill, in white, as a mask for the graphics chip
    // (TextFills.qml): no bars, no outlines.
    if (maskOf >= 0) {
        for (const TextLayoutResult::Outline &outline : layout.outlines) {
            if (outline.format.fillEffect == maskOf && outline.format.fillVisible)
                painter->fillPath(outline.path, Qt::white);
        }
        return;
    }

    // The text's fills other than a colour: a gradient's brush across the letters it
    // fills, or the text's whole box where it is stretched over it (textfills.h); none for
    // the fills the graphics chip draws, whose letters are left empty.
    // (Looked into only for text that has such letters: the text is not copied again for
    // any other)
    QHash<int, QBrush> brushes;
    QHash<int, QRectF> boxes;
    for (const TextLayoutResult::Outline &outline : layout.outlines) {
        if (outline.format.fillEffect >= 0)
            boxes[outline.format.fillEffect] |= outline.path.boundingRect();
    }
    if (!boxes.isEmpty()) {
        const RichText text = shown();
        for (auto box = boxes.cbegin(); box != boxes.cend(); ++box) {
            const QVariantMap fill = text.fillEffects.value(box.key()).toMap();
            const QRectF whole(QPointF(bleed + insetLeft, bleed + insetTop) - origin(layout), room);
            if (fill.value("kind") == QLatin1String("gradient"))
                brushes.insert(box.key(), gradientBrush(fill, fill.value("stretch").toBool() ? whole : box.value()));
        }
    }

    // The bars behind the lines go down first.
    if (lineFill.alpha() > 0) {
        qreal widest = 0;
        for (const QRectF &line : layout.lines)
            widest = qMax(widest, line.width());
        for (qsizetype i = 0; i < layout.lines.size(); ++i) {
            QRectF bar = layout.lines.at(i);
            if (lineFillStyle == 0) {
                bar.setLeft(0);
                bar.setWidth(room.width());
            } else if (lineFillStyle == 2) {
                bar.setLeft((room.width() - widest) * layout.lineAlignments.at(i));
                bar.setWidth(widest);
            }
            bar.adjust(-lineFillWidthOffset / 2, -lineFillHeightOffset / 2, lineFillWidthOffset / 2, lineFillHeightOffset / 2);
            bar.translate(lineFillHorizontalOffset, lineFillVerticalOffset);
            if (bar.width() > 0 && bar.height() > 0)
                painter->fillRect(bar, lineFill);
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
        if (outline.format.fillEffect >= 0) {
            if (brushes.contains(outline.format.fillEffect))
                painter->fillPath(outline.path, brushes.value(outline.format.fillEffect));
            else if (!layeredFills)
                painter->fillPath(outline.path, outline.format.fill);
            continue;
        }
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

// The shadow of what the picture holds, put under it as the element's Shadow draws one
// everywhere else (SlideElement: Qt's MultiEffect over the text's layer), from the same
// numbers: a blur of the radius over 32, at most 1, of a blurMax of 64, which MultiEffect
// blurs as much as a Gaussian of the sigma measured (textshadow.h). In the picture's own
// pixels, for a screen whose pixels are a ratio of the item's (MultiEffect's blur taken to
// be in the item's, A GUESS for such a screen).
void StrokedText::Look::castShadow(QImage &picture) const
{
    const qreal ratio = picture.devicePixelRatio();
    textshadow::cast(picture, shadowColor, QPointF(shadowOffsetX, shadowOffsetY) * unit * ratio,
                     textshadow::multiEffectSigma(std::min(64.0, 2 * shadowRadius * unit)) * ratio);
}
