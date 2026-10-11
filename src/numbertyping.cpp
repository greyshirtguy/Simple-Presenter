#include "numbertyping.h"

#include "slidenumber.h"

QString NumberTyping::typed(const QString &soFar, const QString &key) const
{
    return slidenumber::typed(soFar, key);
}

QString NumberTyping::erased(const QString &soFar) const
{
    return slidenumber::erased(soFar);
}

int NumberTyping::slideFor(const QString &typed, int slides) const
{
    return slidenumber::slideFor(typed, slides);
}
