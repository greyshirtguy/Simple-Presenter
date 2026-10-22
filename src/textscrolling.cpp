#include "textscrolling.h"

#include "richtext.h"
#include "textlayout.h"
#include "textscroller.h"

#include <algorithm>

double TextScrolling::speed(double rate) const
{
    return textscroller::speed(rate);
}

double TextScrolling::start(bool offScreen, double room) const
{
    return textscroller::start(offScreen, room);
}

QVariantList TextScrolling::positions(double travelled, double start, double length, double gap, double room, bool repeat) const
{
    QVariantList list;
    // (Enough for any box: the words of each going are at least a slide unit long.)
    for (double at : textscroller::positions(travelled, start, length, gap, room, repeat, 64))
        list << at;
    return list;
}

QSizeF TextScrolling::naturalSize(const QVariant &content, const QVariant &replacement, double width) const
{
    // (As StrokedText::shown: a replacement with a style of its own as it is, words
    // alone in the content's style)
    RichText words = content.value<RichText>();
    if (replacement.userType() == qMetaTypeId<RichText>()) {
        words = replacement.value<RichText>();
    } else if (replacement.typeId() == QMetaType::QString) {
        words = RichText::plain(replacement.toString(), words.firstRun(),
                                words.paragraphs.isEmpty() ? Qt::AlignHCenter : words.paragraphs.first().alignment);
    }
    if (words.isEmpty())
        return {};
    const TextLayoutResult laid = measureText(words, width);
    double widest = 0;
    for (const QRectF &line : laid.lines)
        widest = std::max(widest, line.width());
    return {widest, laid.height};
}
