#include "thememath.h"

#include <algorithm>

namespace thememath {

namespace {

// Places in a list of boxes, the largest box first; those much of a size (within a
// tenth) in the order they came in.
QList<int> bySize(const QList<Box> &boxes, const QList<int> &places)
{
    QList<int> ordered = places;
    const auto area = [&boxes](int at) { return boxes.at(at).width * boxes.at(at).height; };
    std::stable_sort(ordered.begin(), ordered.end(), [&area](int a, int b) {
        const double larger = std::max(area(a), area(b));
        if (larger <= 0 || std::abs(area(a) - area(b)) <= larger * 0.1)
            return false;
        return area(a) > area(b);
    });
    return ordered;
}

}

QList<int> match(const QList<Box> &slide, const QList<Box> &theme)
{
    QList<int> into(theme.size(), -1);
    QList<bool> taken(slide.size(), false);
    // By name
    for (int t = 0; t < theme.size(); ++t) {
        if (theme.at(t).name.trimmed().isEmpty())
            continue;
        for (int s = 0; s < slide.size(); ++s) {
            if (!taken.at(s) && slide.at(s).name.trimmed().compare(theme.at(t).name.trimmed(), Qt::CaseInsensitive) == 0) {
                into[t] = s;
                taken[s] = true;
                break;
            }
        }
    }
    // What is left, by size and then by order
    QList<int> themeLeft;
    QList<int> slideLeft;
    for (int t = 0; t < theme.size(); ++t) {
        if (into.at(t) < 0)
            themeLeft << t;
    }
    for (int s = 0; s < slide.size(); ++s) {
        if (!taken.at(s))
            slideLeft << s;
    }
    const QList<int> themeOrdered = bySize(theme, themeLeft);
    const QList<int> slideOrdered = bySize(slide, slideLeft);
    for (int i = 0; i < themeOrdered.size() && i < slideOrdered.size(); ++i)
        into[themeOrdered.at(i)] = slideOrdered.at(i);
    return into;
}

}
