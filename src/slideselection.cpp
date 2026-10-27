#include "slideselection.h"

#include <algorithm>

namespace slideselection {

namespace {

// In the list's order, each once
QList<int> tidied(QList<int> places)
{
    std::sort(places.begin(), places.end());
    places.erase(std::unique(places.begin(), places.end()), places.end());
    return places;
}

}

QList<int> toggled(const QList<int> &selected, int active, int clicked)
{
    QList<int> now = selected;
    if (now.isEmpty() && active >= 0)
        now << active;
    if (now.contains(clicked))
        now.removeAll(clicked);
    else
        now << clicked;
    return tidied(now);
}

QList<int> extended(const QList<int> &selected, int last, int active, int clicked)
{
    const int anchor = last >= 0 ? last : active >= 0 ? active : 0;
    QList<int> now = selected;
    if (now.isEmpty() && active >= 0)
        now << active;
    for (int place = qMin(anchor, clicked); place <= qMax(anchor, clicked); ++place)
        now << place;
    return tidied(now);
}

QList<int> targets(const QList<int> &selected, int count)
{
    QList<int> chosen;
    for (int place : tidied(selected)) {
        if (place >= 0 && place < count)
            chosen << place;
    }
    if (!chosen.isEmpty())
        return chosen;
    for (int place = 0; place < count; ++place)
        chosen << place;
    return chosen;
}

} // namespace slideselection
