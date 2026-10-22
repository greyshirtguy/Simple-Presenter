#include "textscroller.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace textscroller {

namespace {

QString directionName(Scroller::Direction direction)
{
    switch (direction) {
    case Scroller::DIRECTION_RIGHT:
        return QStringLiteral("right");
    case Scroller::DIRECTION_UP:
        return QStringLiteral("up");
    case Scroller::DIRECTION_DOWN:
        return QStringLiteral("down");
    default:
        return QStringLiteral("left");
    }
}

double share(const QVariant &value)
{
    return qBound(0.0, value.toDouble(), 1.0);
}

} // namespace

QVariantMap describe(const Scroller &scroller)
{
    return {
        {"direction", directionName(scroller.scrolling_direction())},
        {"rate", scroller.scroll_rate()},
        {"repeat", scroller.should_repeat()},
        {"distance", scroller.repeat_distance()},
        {"offScreen", scroller.starts_off_screen()},
        {"fadeLeft", scroller.fade_left()},
        {"fadeRight", scroller.fade_right()},
    };
}

bool asks(const QVariantMap &changes)
{
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        if (it.key().startsWith(QLatin1String("scroll")))
            return true;
    }
    return false;
}

void change(Scroller *scroller, const QVariantMap &changes)
{
    if (changes.contains("scrollOn")) {
        const bool on = changes.value("scrollOn").toBool();
        // (A box that has never had them: ProPresenter's settings for a new one)
        if (on && scroller->scroll_rate() == 0 && scroller->repeat_distance() == 0 && !scroller->should_repeat()) {
            scroller->set_scroll_rate(0.5);
            scroller->set_should_repeat(true);
            scroller->set_repeat_distance(0.05);
        }
        scroller->set_should_scroll(on);
    }
    if (changes.contains("scrollDirection")) {
        const QString direction = changes.value("scrollDirection").toString();
        scroller->set_scrolling_direction(direction == QLatin1String("right") ? Scroller::DIRECTION_RIGHT
                                          : direction == QLatin1String("up")  ? Scroller::DIRECTION_UP
                                          : direction == QLatin1String("down") ? Scroller::DIRECTION_DOWN
                                                                               : Scroller::DIRECTION_LEFT);
    }
    if (changes.contains("scrollRate"))
        scroller->set_scroll_rate(share(changes.value("scrollRate")));
    if (changes.contains("scrollRepeat"))
        scroller->set_should_repeat(changes.value("scrollRepeat").toBool());
    if (changes.contains("scrollDistance"))
        scroller->set_repeat_distance(share(changes.value("scrollDistance")));
    if (changes.contains("scrollOffScreen"))
        scroller->set_starts_off_screen(changes.value("scrollOffScreen").toBool());
    if (changes.contains("scrollFadeLeft"))
        scroller->set_fade_left(share(changes.value("scrollFadeLeft")));
    if (changes.contains("scrollFadeRight"))
        scroller->set_fade_right(share(changes.value("scrollFadeRight")));
}

double speed(double rate)
{
    return 20.0 * std::pow(25.0, qBound(0.0, rate, 1.0));
}

double start(bool offScreen, double room)
{
    return offScreen ? room : 0.0;
}

QList<double> positions(double travelled, double start, double length, double gap, double room, bool repeat, int most)
{
    QList<double> found;
    if (most <= 0 || length <= 0)
        return found;
    if (!repeat) {
        const double at = start - travelled;
        if (at + length > 0 && at < room)
            found << at;
        return found;
    }
    // Each going is `cycle` after the one before it; the first that can still be seen
    // is the first whose far edge is not yet past the start of the box.
    const double cycle = std::max(1.0, length + gap);
    const double first = std::max(0.0, std::floor((travelled - start - length) / cycle) + 1);
    for (double i = first; found.size() < most; i += 1) {
        const double at = start + i * cycle - travelled;
        if (at >= room)
            break;
        if (at + length > 0)
            found << at;
    }
    return found;
}

} // namespace textscroller
