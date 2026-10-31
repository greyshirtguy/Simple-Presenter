#include "paintahead.h"

#include "strokedtext.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QtMath>

#include <limits>

namespace {

// How much of the pictures kept may be asked for at once, as the pictures would be uncut:
// most of them (StrokedText keeps 32 MB, cut to what is not see-through, which is smaller),
// the rest left for the slides on the screens
const qsizetype askedBytes = 24 * 1024 * 1024;

} // namespace

PaintAhead::PaintAhead(QQuickItem *parent)
    : QQuickItem(parent)
{
    connect(StrokedText::picturesKept(), &PicturesKept::kept, this, &PaintAhead::kept);
}

qreal PaintAhead::ratio() const
{
    return window() ? window()->effectiveDevicePixelRatio() : qGuiApp->devicePixelRatio();
}

bool PaintAhead::paintFirst(const QVariantMap &slide)
{
    m_first = looksOf({slide}, std::numeric_limits<qsizetype>::max());
    if (StrokedText::paintFirst(m_first, ratio())) {
        m_first.clear();
        return true;
    }
    return false;
}

// A picture has been kept: if it was the last of the first slide's, that is said
void PaintAhead::kept()
{
    if (m_first.isEmpty() || !StrokedText::allKept(m_first, ratio()))
        return;
    m_first.clear();
    emit firstPainted();
}

void PaintAhead::setSlides(const QVariantList &slides)
{
    m_slides = slides;
    emit slidesChanged();
    // (Once all that is set at a time has been)
    if (!m_queued) {
        m_queued = true;
        QMetaObject::invokeMethod(this, &PaintAhead::paint, Qt::QueuedConnection);
    }
}

void PaintAhead::paint()
{
    m_queued = false;
    StrokedText::paintAhead(looksOf(m_slides, askedBytes), ratio());
}

QList<StrokedText::Look> PaintAhead::looksOf(const QVariantList &slides, qsizetype budget) const
{
    QList<StrokedText::Look> looks;
    if (width() <= 0 || height() <= 0)
        return looks;
    const qreal ratio = this->ratio();
    qsizetype bytes = 0;
    for (const QVariant &one : slides) {
        const QVariantMap slide = one.toMap();
        // (As Slide: in slide units, fitted to the size it is drawn at)
        const qreal slideWidth = slide.contains("width") ? slide.value("width").toReal() : 1920;
        const qreal slideHeight = slide.contains("height") ? slide.value("height").toReal() : 1080;
        if (slideWidth <= 0 || slideHeight <= 0)
            continue;
        const qreal unit = std::min(width() / slideWidth, height() / slideHeight);
        for (const QVariant &element : slide.value("elements").toList()) {
            const std::optional<StrokedText::Look> look = StrokedText::lookOf(element.toMap(), unit, m_effects);
            if (!look)
                continue;
            bytes += qsizetype(qCeil(look->width * ratio)) * qCeil(look->height * ratio) * 4;
            if (bytes > budget)
                break;
            looks.append(*look);
        }
        if (bytes > budget)
            break;
    }
    return looks;
}
