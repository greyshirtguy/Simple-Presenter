#include "colortriggers.h"

#include <QRegularExpression>
#include <QStringList>

#include <cmath>

namespace colortriggers {

int inForce(const QList<double> &times, double value, bool countsDown)
{
    // (A hair's leeway: a timer's seconds come from clocks, and 59.9999 is a minute.)
    constexpr double leeway = 1e-6;
    int found = -1;
    for (int i = 0; i < times.size(); ++i) {
        const double time = times.at(i);
        if (countsDown) {
            if (value <= time + leeway && (found < 0 || time < times.at(found)))
                found = i;
        } else {
            if (value + leeway >= time && (found < 0 || time > times.at(found)))
                found = i;
        }
    }
    return found;
}

QString written(double seconds)
{
    // In hundredths, rounded, so that 59.999 is written 1:00.00 and not 0:60.00
    const qint64 all = std::llround(std::max(0.0, seconds) * 100);
    const qint64 hundredths = all % 100;
    const qint64 wholeSeconds = (all / 100) % 60;
    const qint64 minutes = (all / 6000) % 60;
    const qint64 hours = all / 360000;
    const QString tail = QStringLiteral("%1.%2").arg(wholeSeconds, 2, 10, QLatin1Char('0')).arg(hundredths, 2, 10, QLatin1Char('0'));
    if (hours > 0)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(tail);
    return QStringLiteral("%1:%2").arg(minutes).arg(tail);
}

double read(const QString &typed)
{
    // Up to three parts, hours, minutes and seconds, the last as many as there are: each
    // part before the last a whole number (or nothing, for none), the last a number that
    // may have a fraction
    const QString text = typed.trimmed();
    const QStringList parts = text.split(u':');
    if (text.isEmpty() || parts.size() > 3)
        return -1;
    static const QRegularExpression whole(QStringLiteral("^\\d*$"));
    static const QRegularExpression number(QStringLiteral("^(\\d+(\\.\\d*)?|\\.\\d+)$"));
    double total = 0;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const QString &part = parts.at(i);
        const bool last = i == parts.size() - 1;
        if (!(last ? number : whole).match(part).hasMatch())
            return -1;
        total = total * 60 + (part.isEmpty() ? 0 : part.toDouble());
    }
    return total;
}

}
