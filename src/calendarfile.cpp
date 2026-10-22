#include "calendarfile.h"

#include "action.pb.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace calendarfile {

namespace {

using Event = rv::data::Calendar::Event;
using DayOfWeek = rv::data::Calendar_Event_DayOfWeek;

const QString notThere = QStringLiteral("The calendar no longer has that event");

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

Event *find(rv::data::Calendar *calendar, const QString &id)
{
    for (Event &event : *calendar->mutable_events()) {
        if (text(event.uuid().string()) == id)
            return &event;
    }
    return nullptr;
}

// Qt counts the days of the week from Monday, 1, to Sunday, 7; ProPresenter from
// Sunday, 1, to Saturday, 7.
int proDay(const QDate &date)
{
    return date.dayOfWeek() % 7 + 1;
}

QDateTime onClock(long long at, const QTimeZone &zone)
{
    return QDateTime::fromSecsSinceEpoch(at, zone);
}

// Where in `actions` the ordinary action is that says what the calendar's own action
// says: the first of the kind that has the id of what it runs (or, for an event whose
// action is Capture, the first Capture action); -1 for none
int mirrorOf(const Event &event)
{
    const auto &own = event.action();
    std::string id;
    auto type = rv::data::Action::ACTION_TYPE_CAPTURE;
    if (own.type() == playlistType && own.has_playlist()) {
        id = own.playlist().playlist_uuid().string();
        type = rv::data::Action::ACTION_TYPE_LIBRARY_PLAYLIST;
    } else if (own.type() == macroType && own.has_macro()) {
        id = own.macro().identification().parameter_uuid().string();
        type = rv::data::Action::ACTION_TYPE_MACRO;
    }
    for (int i = 0; i < event.actions_size(); ++i) {
        const rv::data::Action &ordinary = event.actions(i);
        if (ordinary.type() == type && (type == rv::data::Action::ACTION_TYPE_CAPTURE || ordinary.uuid().string() == id))
            return i;
    }
    return -1;
}

// The calendar's action, as describeEvent names it
QVariantMap describeAction(const Event &event)
{
    QVariantMap action {{"kind", QStringLiteral("none")}, {"name", QString()}, {"playlistId", QString()}, {"itemId", QString()},
                        {"macroId", QString()}, {"macroName", QString()}, {"collectionId", QString()}, {"collectionName", QString()}};
    const int mirror = mirrorOf(event);
    // The name the file gives what it runs: its ordinary action's
    if (mirror >= 0)
        action.insert("name", text(event.actions(mirror).name()));
    const auto &own = event.action();
    if (own.type() == playlistType && own.has_playlist()) {
        action.insert("kind", QStringLiteral("playlist"));
        action.insert("playlistId", text(own.playlist().playlist_uuid().string()));
        action.insert("itemId", text(own.playlist().playlist_item_uuid().string()));
    } else if (own.type() == macroType && own.has_macro()) {
        const rv::data::CollectionElementType &macro = own.macro().identification();
        action.insert("kind", QStringLiteral("macro"));
        action.insert("macroId", text(macro.parameter_uuid().string()));
        action.insert("macroName", text(macro.parameter_name()));
        action.insert("collectionId", text(macro.parent_collection().parameter_uuid().string()));
        action.insert("collectionName", text(macro.parent_collection().parameter_name()));
        if (action.value("name").toString().isEmpty())
            action.insert("name", text(macro.parameter_name()));
    } else if (mirror >= 0) {
        action.insert("kind", QStringLiteral("capture"));
    } else if (event.has_action() || event.actions_size() > 0) {
        action.insert("kind", QStringLiteral("other"));
    }
    return action;
}

// An event's action changed: the calendar's own, and the ordinary one beside it, in
// the one's place, or first where there was none
QString setAction(Event *event, const QVariantMap &asked)
{
    const QString kind = asked.value("kind").toString();
    const int at = mirrorOf(*event);
    if (kind == QLatin1String("none")) {
        event->clear_action();
        if (at >= 0)
            event->mutable_actions()->DeleteSubrange(at, 1);
        return {};
    }
    rv::data::Action made;
    made.set_isenabled(true);
    auto *own = event->mutable_action();
    if (kind == QLatin1String("playlist")) {
        const std::string playlist = asked.value("playlistId").toString().toStdString();
        const std::string item = asked.value("itemId").toString().toStdString();
        if (playlist.empty())
            return QStringLiteral("A playlist is needed");
        own->set_type(playlistType);
        auto *target = own->mutable_playlist();
        target->mutable_playlist_uuid()->set_string(playlist);
        if (item.empty())
            target->clear_playlist_item_uuid();
        else
            target->mutable_playlist_item_uuid()->set_string(item);
        made.mutable_uuid()->set_string(playlist);
        made.set_name(asked.value("playlistName").toString().toStdString());
        made.set_type(rv::data::Action::ACTION_TYPE_LIBRARY_PLAYLIST);
        made.mutable_playlist_item()->mutable_playlist_uuid()->set_string(playlist);
        if (!item.empty())
            made.mutable_playlist_item()->mutable_item_uuid()->set_string(item);
    } else if (kind == QLatin1String("macro")) {
        const std::string macro = asked.value("macroId").toString().toStdString();
        if (macro.empty())
            return QStringLiteral("A macro is needed");
        rv::data::CollectionElementType identification;
        identification.mutable_parameter_uuid()->set_string(macro);
        identification.set_parameter_name(asked.value("macroName").toString().toStdString());
        identification.mutable_parent_collection()->mutable_parameter_uuid()->set_string(asked.value("collectionId").toString().toStdString());
        identification.mutable_parent_collection()->set_parameter_name(asked.value("collectionName").toString().toStdString());
        own->set_type(macroType);
        *own->mutable_macro()->mutable_identification() = identification;
        made.mutable_uuid()->set_string(macro);
        made.set_name(identification.parameter_name());
        made.set_type(rv::data::Action::ACTION_TYPE_MACRO);
        *made.mutable_macro()->mutable_identification() = identification;
    } else {
        return QStringLiteral("An event's action is a playlist, a macro or none");
    }
    if (at >= 0) {
        *event->mutable_actions(at) = made;
    } else {
        *event->add_actions() = made;
        // (The calendar's one goes first, as ProPresenter has it.)
        for (int i = event->actions_size() - 1; i > 0; --i)
            event->mutable_actions()->SwapElements(i, i - 1);
    }
    return {};
}

} // namespace

QVariantMap describeEvent(const rv::data::Calendar::Event &event)
{
    QVariantList days;
    QList<int> numbers;
    for (int i = 0; i < event.recurrence_days_size(); ++i) {
        const int day = int(event.recurrence_days(i));
        if (day >= 1 && day <= 7 && !numbers.contains(day))
            numbers << day;
    }
    std::sort(numbers.begin(), numbers.end());
    for (int day : std::as_const(numbers))
        days << day;
    QVariantList excluded;
    for (const rv::data::Timestamp &date : event.recurrence_excluded_dates())
        excluded << qint64(date.seconds());
    return {
        {"id", text(event.uuid().string())},
        {"name", text(event.name())},
        {"description", text(event.description())},
        {"start", qint64(event.date().seconds())},
        {"days", days},
        {"ends", event.has_recurrence_limit_date() ? qint64(event.recurrence_limit_date().seconds()) : qint64(0)},
        {"excluded", excluded},
        {"action", describeAction(event)},
    };
}

QVariantMap describe(const rv::data::Calendar &calendar)
{
    QVariantList events;
    for (const rv::data::Calendar::Event &event : calendar.events())
        events << describeEvent(event);
    return {{"enabled", calendar.active()}, {"events", events}};
}

QString add(rv::data::Calendar *calendar, long long start, const std::function<std::string()> &newId, QString *made)
{
    Event *event = calendar->add_events();
    event->mutable_uuid()->set_string(newId());
    event->set_name("New Event");
    event->mutable_date()->set_seconds(start);
    if (made)
        *made = text(event->uuid().string());
    return {};
}

QString change(rv::data::Calendar *calendar, const QString &id, const QVariantMap &changes)
{
    Event *event = find(calendar, id);
    if (!event)
        return notThere;
    if (changes.contains("name"))
        event->set_name(changes.value("name").toString().toStdString());
    if (changes.contains("description"))
        event->set_description(changes.value("description").toString().toStdString());
    if (changes.contains("start")) {
        bool number = false;
        const long long start = changes.value("start").toLongLong(&number);
        if (!number || start <= 0)
            return QStringLiteral("That is not a time");
        if (event->date().seconds() != start) {
            event->mutable_date()->set_seconds(start);
            event->mutable_date()->clear_nanos();
        }
    }
    if (changes.contains("days")) {
        QList<int> days;
        for (const QVariant &day : changes.value("days").toList()) {
            const int number = day.toInt();
            if (number < 1 || number > 7)
                return QStringLiteral("A day of the week is from 1, Sunday, to 7, Saturday");
            if (!days.contains(number))
                days << number;
        }
        std::sort(days.begin(), days.end());
        QList<int> had;
        for (int i = 0; i < event->recurrence_days_size(); ++i)
            had << int(event->recurrence_days(i));
        if (had != days) {
            event->clear_recurrence_days();
            for (int day : std::as_const(days))
                event->add_recurrence_days(DayOfWeek(day));
        }
    }
    if (changes.contains("ends")) {
        const long long ends = changes.value("ends").toLongLong();
        if (ends <= 0)
            event->clear_recurrence_limit_date();
        else if (!event->has_recurrence_limit_date() || event->recurrence_limit_date().seconds() != ends)
            event->mutable_recurrence_limit_date()->set_seconds(ends);
    }
    if (changes.contains("action"))
        return setAction(event, changes.value("action").toMap());
    return {};
}

QString remove(rv::data::Calendar *calendar, const QString &id)
{
    for (int i = 0; i < calendar->events_size(); ++i) {
        if (text(calendar->events(i).uuid().string()) == id) {
            calendar->mutable_events()->DeleteSubrange(i, 1);
            return {};
        }
    }
    return notThere;
}

void setEnabled(rv::data::Calendar *calendar, bool on)
{
    calendar->set_active(on);
}

QList<long long> occurrences(const QVariantMap &event, long long after, long long until, const QTimeZone &zone, int most)
{
    QList<long long> found;
    const long long start = event.value("start").toLongLong();
    const long long ends = event.value("ends").toLongLong();
    QSet<int> days;
    for (const QVariant &day : event.value("days").toList())
        days.insert(day.toInt());
    QSet<QDate> excluded;
    for (const QVariant &date : event.value("excluded").toList())
        excluded.insert(onClock(date.toLongLong(), zone).date());
    if (most <= 0 || until <= after)
        return found;
    if (days.isEmpty()) {
        if (start > after && start <= until && !excluded.contains(onClock(start, zone).date()))
            found << start;
        return found;
    }
    // Day by day on the clock, from the later of the first day and the day `after` is,
    // to the earlier of `until` and "ends on"
    const QDateTime first = onClock(start, zone);
    const QTime timeOfDay = first.time();
    const long long last = ends > 0 ? std::min(until, ends) : until;
    const QDate lastDay = onClock(last, zone).date();
    for (QDate day = std::max(first.date(), onClock(after, zone).date()); day <= lastDay && found.size() < most; day = day.addDays(1)) {
        if (!days.contains(proDay(day)) || excluded.contains(day))
            continue;
        const long long at = QDateTime(day, timeOfDay, zone).toSecsSinceEpoch();
        if (at >= start && at > after && at <= last)
            found << at;
    }
    return found;
}

long long nextAfter(const QVariantList &events, long long after, const QTimeZone &zone)
{
    long long next = -1;
    for (const QVariant &event : events) {
        const QList<long long> times = occurrences(event.toMap(), after, after + 400LL * 86400, zone, 1);
        if (!times.isEmpty() && (next < 0 || times.first() < next))
            next = times.first();
    }
    return next;
}

bool stillDue(long long at, long long now)
{
    return now >= at && now - at <= graceSeconds;
}

long long newStart(long long now, const QTimeZone &zone)
{
    const QDateTime clock = onClock(now, zone);
    const QDateTime hour(clock.date(), QTime(clock.time().hour(), 0), zone);
    return hour.addSecs(3600).toSecsSinceEpoch();
}

QVariantList listing(const QVariantList &events, long long now, bool recurring, const QTimeZone &zone, int most)
{
    struct Row
    {
        QString id;
        QString name;
        long long at;
        bool past;
        QString section;
    };
    QList<Row> rows;
    for (const QVariant &entry : events) {
        const QVariantMap event = entry.toMap();
        const QString id = event.value("id").toString();
        const QString name = event.value("name").toString();
        const long long start = event.value("start").toLongLong();
        if (!recurring) {
            rows << Row {id, name, start, start <= now, QString()};
            continue;
        }
        // Its last time, which for one that repeats is in the week before now, or before
        // it ended
        const long long ends = event.value("ends").toLongLong();
        const long long upTo = ends > 0 ? std::min(now, ends) : now;
        const QList<long long> past = occurrences(event, std::max(start - 1, upTo - 8LL * 86400), upTo, zone, 1000);
        if (!past.isEmpty())
            rows << Row {id, name, past.last(), true, QStringLiteral("current")};
        for (long long at : occurrences(event, now, now + 400LL * 86400, zone, most))
            rows << Row {id, name, at, false, QStringLiteral("upcoming")};
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.section != b.section)
            return a.section == QLatin1String("current");
        return a.at < b.at;
    });
    QVariantList list;
    int upcoming = 0;
    for (const Row &row : std::as_const(rows)) {
        if (row.section == QLatin1String("upcoming") && ++upcoming > most)
            break;
        list << QVariantMap {{"id", row.id}, {"name", row.name}, {"at", row.at}, {"past", row.past}, {"section", row.section}};
    }
    return list;
}

namespace {

// The locale's short date, with the year whole
QString dateFormat(const QLocale &locale)
{
    QString format = locale.dateFormat(QLocale::ShortFormat);
    if (!format.contains(QLatin1String("yyyy")))
        format.replace(QLatin1String("yy"), QLatin1String("yyyy"));
    return format;
}

bool twelveHours(const QLocale &locale)
{
    return locale.timeFormat(QLocale::ShortFormat).contains(QLatin1Char('a'), Qt::CaseInsensitive);
}

} // namespace

QString dateText(long long at, const QTimeZone &zone, const QLocale &locale)
{
    return locale.toString(onClock(at, zone).date(), dateFormat(locale));
}

QString timeText(long long at, const QTimeZone &zone, const QLocale &locale)
{
    return locale.toString(onClock(at, zone).time(), twelveHours(locale) ? QStringLiteral("h:mm:ss ap") : QStringLiteral("HH:mm:ss"));
}

QString whenText(long long at, const QTimeZone &zone, const QLocale &locale)
{
    return locale.toString(onClock(at, zone).date(), QStringLiteral("d MMM yyyy")) + QStringLiteral(" at ") + timeText(at, zone, locale);
}

long long withDate(long long at, const QString &typed, const QTimeZone &zone, const QLocale &locale)
{
    const QString text = typed.trimmed();
    QDate date = locale.toDate(text, dateFormat(locale));
    if (!date.isValid())
        date = QDate::fromString(text, Qt::ISODate);
    if (!date.isValid())
        return -1;
    return QDateTime(date, onClock(at, zone).time(), zone).toSecsSinceEpoch();
}

long long withTime(long long at, const QString &typed, const QTimeZone &zone, const QLocale &locale)
{
    Q_UNUSED(locale)
    static const QRegularExpression pattern(QStringLiteral("^(\\d{1,2})(?::(\\d{2}))?(?::(\\d{2}))?\\s*([ap])?\\.?\\s*(?:m\\.?)?$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(typed.trimmed());
    if (!match.hasMatch())
        return -1;
    int hour = match.captured(1).toInt();
    const int minute = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    const int second = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    const QString half = match.captured(4).toLower();
    if (!half.isEmpty()) {
        if (hour < 1 || hour > 12)
            return -1;
        hour = hour % 12 + (half == QLatin1String("p") ? 12 : 0);
    }
    const QTime time(hour, minute, second);
    if (!time.isValid())
        return -1;
    return QDateTime(onClock(at, zone).date(), time, zone).toSecsSinceEpoch();
}

} // namespace calendarfile
