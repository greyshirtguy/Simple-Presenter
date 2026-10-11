#include "slidenumber.h"

namespace slidenumber {

namespace {

// Whether a key is one digit, of the ten there are on a keyboard here. (Not what
// QChar calls a digit, which takes in the digits of other scripts: a slide's number is
// shown in these.)
bool isDigit(const QString &key)
{
    return key.size() == 1 && key.at(0) >= u'0' && key.at(0) <= u'9';
}

} // namespace

QString typed(const QString &soFar, const QString &key)
{
    if (!isDigit(key))
        return soFar;
    // A nought that leads is no part of a number: the digit after it takes its place.
    if (soFar == QLatin1String("0"))
        return key;
    if (soFar.size() >= kMostDigits)
        return soFar;
    return soFar + key;
}

QString erased(const QString &soFar)
{
    return soFar.chopped(qMin(1, int(soFar.size())));
}

int slideFor(const QString &typed, int slides)
{
    if (typed.isEmpty() || typed.size() > kMostDigits)
        return -1;
    for (const QChar c : typed) {
        if (c < u'0' || c > u'9')
            return -1;
    }
    const int number = typed.toInt();
    return number >= 1 && number <= slides ? number - 1 : -1;
}

} // namespace slidenumber
