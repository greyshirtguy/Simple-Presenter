#include "slideselectionbridge.h"

#include "slideselection.h"

namespace {

QList<int> places(const QVariantList &list)
{
    QList<int> out;
    for (const QVariant &value : list)
        out << value.toInt();
    return out;
}

QVariantList listed(const QList<int> &places)
{
    QVariantList out;
    for (int place : places)
        out << place;
    return out;
}

}

QVariantList SlideSelection::toggled(const QVariantList &selected, int active, int clicked) const
{
    return listed(slideselection::toggled(places(selected), active, clicked));
}

QVariantList SlideSelection::extended(const QVariantList &selected, int last, int active, int clicked) const
{
    return listed(slideselection::extended(places(selected), last, active, clicked));
}

QVariantList SlideSelection::targets(const QVariantList &selected, int count) const
{
    return listed(slideselection::targets(places(selected), count));
}
