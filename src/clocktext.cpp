#include "clocktext.h"

#include <QRegularExpression>
#include <QTimeZone>

namespace clocktext {

namespace {

// How this place writes a date in full, as a pattern ("dddd, MMMM d, yyyy"), and the
// same with the day of the week taken out, and then with the month's name cut short.
QString longPattern(const QLocale &locale)
{
    return locale.dateFormat(QLocale::LongFormat);
}

QString withoutWeekday(QString pattern)
{
    // The day's name and whatever stands between it and its neighbour
    static const QRegularExpression leading(QStringLiteral("^dddd[^A-Za-z']*"));
    static const QRegularExpression trailing(QStringLiteral("[^A-Za-z']*dddd$"));
    static const QRegularExpression inside(QStringLiteral("dddd[^A-Za-z']*"));
    pattern.replace(leading, QString());
    pattern.replace(trailing, QString());
    pattern.replace(inside, QString());
    return pattern;
}

QString date(const QDate &day, int style, const QLocale &locale)
{
    switch (style) {
    case Short:
        return locale.toString(day, QLocale::ShortFormat);
    case Medium:
        return locale.toString(day, withoutWeekday(longPattern(locale)).replace(QLatin1String("MMMM"), QLatin1String("MMM")));
    case Long:
        return locale.toString(day, withoutWeekday(longPattern(locale)));
    case Full:
        return locale.toString(day, longPattern(locale));
    default:
        return {};
    }
}

QString time(const QDateTime &moment, int style, bool twentyFourHour, const QLocale &locale)
{
    if (style <= None || style > Full)
        return {};
    // (The halves of the day in the case this place writes them in.)
    const QString hours = twentyFourHour ? QStringLiteral("HH:mm") : QStringLiteral("h:mm");
    const QString half = twentyFourHour ? QString() : QStringLiteral(" Ap");
    QString text = locale.toString(moment.time(), hours + (style >= Medium ? QStringLiteral(":ss") : QString()) + half);
    if (style == Long)
        text += u' ' + moment.timeZoneAbbreviation();
    else if (style == Full)
        text += u' ' + moment.timeZone().displayName(moment, QTimeZone::LongName, locale);
    return text;
}

} // namespace

QString written(const QDateTime &moment, int dateStyle, int timeStyle, bool twentyFourHour, const QLocale &locale)
{
    const QString day = date(moment.date(), dateStyle, locale);
    const QString hour = time(moment, timeStyle, twentyFourHour, locale);
    if (day.isEmpty() || hour.isEmpty())
        return day + hour;
    return day + (dateStyle == Short ? QStringLiteral(", ") : QStringLiteral(" at ")) + hour;
}

QString linked(const QDateTime &moment, int dateStyle, int timeStyle, bool twentyFourHour, const QString &pattern, const QLocale &locale)
{
    const QString clock = written(moment, dateStyle, timeStyle, twentyFourHour, locale);
    const QString placeholder = QStringLiteral("${clock}");
    return pattern.contains(placeholder) ? QString(pattern).replace(placeholder, clock) : clock;
}

} // namespace clocktext
