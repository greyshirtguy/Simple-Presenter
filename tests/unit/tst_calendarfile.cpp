// ProPresenter's Calendar (src/calendarfile.h): a calendar made as ProPresenter wrote the
// one that was looked at, read; each change made to it with a look at what else is still
// as it was, to the byte; actions written in both of the file's forms as ProPresenter
// writes them; when events come due, on Sydney's clock, through summer time; the list
// the window shows; and dates and times as they are written and typed.

#include "calendarfile.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using namespace calendarfile;
using rv::data::Action;
using rv::data::Calendar;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

const QTimeZone sydney("Australia/Sydney");
const QLocale australian(QLocale::English, QLocale::Australia);

// Sunday 18 October 2026, 7 pm in Sydney (08:00 UTC), as the file has it; and so on
constexpr long long sunday18 = 1792310400;
constexpr long long sunday25 = 1792915200;
constexpr long long tuesday27 = 1793088000;
constexpr long long wednesday27NextYear = 1824624000;
constexpr long long day = 86400;
constexpr long long week = 7 * day;

rv::data::CollectionElementType macroNamed(const char *id, const char *name)
{
    rv::data::CollectionElementType macro;
    macro.mutable_parameter_uuid()->set_string(id);
    macro.set_parameter_name(name);
    macro.mutable_parent_collection()->mutable_parameter_uuid()->set_string("C0C0C0C0-1111-2222-3333-444444444444");
    macro.mutable_parent_collection()->set_parameter_name("Default Collection");
    return macro;
}

// An event whose action is a macro, in both of the file's forms, as ProPresenter wrote it
void giveMacro(Calendar::Event *event, const char *id, const char *name)
{
    event->mutable_action()->set_type(102);
    *event->mutable_action()->mutable_macro()->mutable_identification() = macroNamed(id, name);
    Action *ordinary = event->add_actions();
    ordinary->mutable_uuid()->set_string(id);
    ordinary->set_name(name);
    ordinary->set_isenabled(true);
    ordinary->set_type(Action::ACTION_TYPE_MACRO);
    *ordinary->mutable_macro()->mutable_identification() = macroNamed(id, name);
}

void givePlaylist(Calendar::Event *event, const char *playlist, const char *name, const char *item)
{
    event->mutable_action()->set_type(101);
    event->mutable_action()->mutable_playlist()->mutable_playlist_uuid()->set_string(playlist);
    if (*item)
        event->mutable_action()->mutable_playlist()->mutable_playlist_item_uuid()->set_string(item);
    Action *ordinary = event->add_actions();
    ordinary->mutable_uuid()->set_string(playlist);
    ordinary->set_name(name);
    ordinary->set_isenabled(true);
    ordinary->set_type(Action::ACTION_TYPE_LIBRARY_PLAYLIST);
    ordinary->mutable_playlist_item()->mutable_playlist_uuid()->set_string(playlist);
    if (*item)
        ordinary->mutable_playlist_item()->mutable_item_uuid()->set_string(item);
}

// The calendar as ProPresenter 21 wrote the one looked at (the names and ids made up):
// one event every Sunday at 7 pm running a macro; one once, on the next Sunday, taking
// a presentation of a playlist live; one every Tuesday until a year on, running another
// macro. And one field the descriptions of ProPresenter's files have no name for, as a
// later version's might be.
Calendar proPresentersOwn()
{
    Calendar calendar;
    Calendar::Event *weekly = calendar.add_events();
    weekly->mutable_uuid()->set_string("E1E1E1E1-1111-2222-3333-444444444444");
    weekly->set_name("New Event");
    weekly->mutable_date()->set_seconds(sunday18);
    weekly->add_recurrence_days(rv::data::Calendar_Event_DayOfWeek_DAY_OF_WEEK_SUNDAY);
    giveMacro(weekly, "A1A1A1A1-1111-2222-3333-444444444444", "Lights Up");
    Calendar::Event *once = calendar.add_events();
    once->mutable_uuid()->set_string("E2E2E2E2-1111-2222-3333-444444444444");
    once->set_name("Once");
    once->mutable_date()->set_seconds(sunday25);
    givePlaylist(once, "B1B1B1B1-1111-2222-3333-444444444444", "Evening Service", "B2B2B2B2-1111-2222-3333-444444444444");
    Calendar::Event *year = calendar.add_events();
    year->mutable_uuid()->set_string("E3E3E3E3-1111-2222-3333-444444444444");
    year->set_name("Every Tuesday for a year");
    year->mutable_date()->set_seconds(tuesday27);
    year->add_recurrence_days(rv::data::Calendar_Event_DayOfWeek_DAY_OF_WEEK_TUESDAY);
    year->mutable_recurrence_limit_date()->set_seconds(wednesday27NextYear);
    giveMacro(year, "A2A2A2A2-1111-2222-3333-444444444444", "Stream On");
    calendar.set_active(true);
    calendar.GetReflection()->MutableUnknownFields(&calendar)->AddVarint(900, 7);
    return calendar;
}

std::function<std::string()> ids()
{
    auto next = std::make_shared<int>(0);
    return [next] { return "EVENT-" + std::to_string(++*next); };
}

QVariantMap eventAt(const Calendar &calendar, int index)
{
    return describe(calendar).value("events").toList().value(index).toMap();
}

QVariantList events(const Calendar &calendar)
{
    return describe(calendar).value("events").toList();
}

int hourOf(long long at)
{
    return QDateTime::fromSecsSinceEpoch(at, sydney).time().hour();
}

} // namespace

class TestCalendarFile : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!sydney.isValid())
            QSKIP("this computer has no time zone database to try Sydney's clock with");
    }

    void readAsProPresenterWroteIt()
    {
        const Calendar calendar = proPresentersOwn();
        const QVariantMap all = describe(calendar);
        QVERIFY(all.value("enabled").toBool());
        QCOMPARE(all.value("events").toList().size(), 3);
        const QVariantMap weekly = eventAt(calendar, 0);
        QCOMPARE(weekly.value("name").toString(), QStringLiteral("New Event"));
        QCOMPARE(weekly.value("start").toLongLong(), sunday18);
        QCOMPARE(weekly.value("days").toList(), QVariantList {1});
        QCOMPARE(weekly.value("ends").toLongLong(), 0LL);
        const QVariantMap macro = weekly.value("action").toMap();
        QCOMPARE(macro.value("kind").toString(), QStringLiteral("macro"));
        QCOMPARE(macro.value("macroId").toString(), QStringLiteral("A1A1A1A1-1111-2222-3333-444444444444"));
        QCOMPARE(macro.value("name").toString(), QStringLiteral("Lights Up"));
        QCOMPARE(macro.value("collectionName").toString(), QStringLiteral("Default Collection"));
        const QVariantMap playlist = eventAt(calendar, 1).value("action").toMap();
        QCOMPARE(playlist.value("kind").toString(), QStringLiteral("playlist"));
        QCOMPARE(playlist.value("playlistId").toString(), QStringLiteral("B1B1B1B1-1111-2222-3333-444444444444"));
        QCOMPARE(playlist.value("itemId").toString(), QStringLiteral("B2B2B2B2-1111-2222-3333-444444444444"));
        QCOMPARE(playlist.value("name").toString(), QStringLiteral("Evening Service"));
        QVERIFY(eventAt(calendar, 1).value("days").toList().isEmpty());
        QCOMPARE(eventAt(calendar, 2).value("ends").toLongLong(), wednesday27NextYear);
        QCOMPARE(eventAt(calendar, 2).value("days").toList(), QVariantList {3});
    }

    void aChangeChangesItsFieldAndNothingElse()
    {
        const QString id = QStringLiteral("E3E3E3E3-1111-2222-3333-444444444444");
        const auto changed = [&](const QVariantMap &changes, const std::function<void(Calendar::Event *)> &expect) {
            Calendar calendar = proPresentersOwn();
            Calendar wanted = proPresentersOwn();
            QVERIFY(change(&calendar, id, changes).isEmpty());
            expect(wanted.mutable_events(2));
            QCOMPARE(encoded(calendar), encoded(wanted));
        };
        changed({{"name", "Tuesdays"}}, [](Calendar::Event *e) { e->set_name("Tuesdays"); });
        changed({{"description", "Before the band"}}, [](Calendar::Event *e) { e->set_description("Before the band"); });
        changed({{"start", tuesday27 + 1800}}, [](Calendar::Event *e) { e->mutable_date()->set_seconds(tuesday27 + 1800); });
        changed({{"days", QVariantList {6, 2, 3, 2}}}, [](Calendar::Event *e) {
            e->clear_recurrence_days();
            e->add_recurrence_days(rv::data::Calendar_Event_DayOfWeek_DAY_OF_WEEK_MONDAY);
            e->add_recurrence_days(rv::data::Calendar_Event_DayOfWeek_DAY_OF_WEEK_TUESDAY);
            e->add_recurrence_days(rv::data::Calendar_Event_DayOfWeek_DAY_OF_WEEK_FRIDAY);
        });
        changed({{"days", QVariantList {}}}, [](Calendar::Event *e) { e->clear_recurrence_days(); });
        changed({{"ends", 0}}, [](Calendar::Event *e) { e->clear_recurrence_limit_date(); });
        changed({{"ends", wednesday27NextYear + week}}, [](Calendar::Event *e) { e->mutable_recurrence_limit_date()->set_seconds(wednesday27NextYear + week); });
        // What is as asked already changes nothing.
        changed({{"days", QVariantList {3}}, {"ends", wednesday27NextYear}, {"start", tuesday27}}, [](Calendar::Event *) {});
        // The calendar switched off, and what is not there
        Calendar calendar = proPresentersOwn();
        setEnabled(&calendar, false);
        Calendar wanted = proPresentersOwn();
        wanted.set_active(false);
        QCOMPARE(encoded(calendar), encoded(wanted));
        QVERIFY(!change(&calendar, "NOT-THERE", {{"name", "x"}}).isEmpty());
        QVERIFY(!change(&calendar, id, {{"days", QVariantList {8}}}).isEmpty());
        QVERIFY(!change(&calendar, id, {{"start", "soon"}}).isEmpty());
    }

    void anActionIsWrittenAsProPresenterWritesIt()
    {
        const QString id = QStringLiteral("E1E1E1E1-1111-2222-3333-444444444444");
        // A macro in place of the macro: both forms, in the same place
        Calendar calendar = proPresentersOwn();
        QVERIFY(change(&calendar, id, {{"action", QVariantMap {{"kind", "macro"}, {"macroId", "A3A3A3A3-1111-2222-3333-444444444444"},
                                                               {"macroName", "Walk In"}, {"collectionId", "C0C0C0C0-1111-2222-3333-444444444444"},
                                                               {"collectionName", "Default Collection"}}}}).isEmpty());
        Calendar wanted = proPresentersOwn();
        wanted.mutable_events(0)->clear_action();
        wanted.mutable_events(0)->clear_actions();
        giveMacro(wanted.mutable_events(0), "A3A3A3A3-1111-2222-3333-444444444444", "Walk In");
        QCOMPARE(encoded(calendar), encoded(wanted));
        // A presentation of a playlist, and the playlist alone
        QVERIFY(change(&calendar, id, {{"action", QVariantMap {{"kind", "playlist"}, {"playlistId", "B1B1B1B1-1111-2222-3333-444444444444"},
                                                               {"playlistName", "Evening Service"}, {"itemId", "B3B3B3B3-1111-2222-3333-444444444444"}}}}).isEmpty());
        wanted.mutable_events(0)->clear_action();
        wanted.mutable_events(0)->clear_actions();
        givePlaylist(wanted.mutable_events(0), "B1B1B1B1-1111-2222-3333-444444444444", "Evening Service", "B3B3B3B3-1111-2222-3333-444444444444");
        QCOMPARE(encoded(calendar), encoded(wanted));
        QVERIFY(change(&calendar, id, {{"action", QVariantMap {{"kind", "playlist"}, {"playlistId", "B1B1B1B1-1111-2222-3333-444444444444"},
                                                               {"playlistName", "Evening Service"}, {"itemId", ""}}}}).isEmpty());
        wanted.mutable_events(0)->clear_action();
        wanted.mutable_events(0)->clear_actions();
        givePlaylist(wanted.mutable_events(0), "B1B1B1B1-1111-2222-3333-444444444444", "Evening Service", "");
        QCOMPARE(encoded(calendar), encoded(wanted));
        QVERIFY(eventAt(calendar, 0).value("action").toMap().value("itemId").toString().isEmpty());
        // None takes both away; an action the calendar's own does not say is kept, and
        // stays after the calendar's own when one is given again.
        Action other;
        other.mutable_uuid()->set_string("D1D1D1D1-1111-2222-3333-444444444444");
        other.set_type(Action::ACTION_TYPE_CLEAR);
        *calendar.mutable_events(0)->add_actions() = other;
        QVERIFY(change(&calendar, id, {{"action", QVariantMap {{"kind", "none"}}}}).isEmpty());
        QVERIFY(!calendar.events(0).has_action());
        QCOMPARE(calendar.events(0).actions_size(), 1);
        QCOMPARE(eventAt(calendar, 0).value("action").toMap().value("kind").toString(), QStringLiteral("other"));
        QVERIFY(change(&calendar, id, {{"action", QVariantMap {{"kind", "macro"}, {"macroId", "A1A1A1A1-1111-2222-3333-444444444444"},
                                                               {"macroName", "Lights Up"}, {"collectionId", "C0C0C0C0-1111-2222-3333-444444444444"},
                                                               {"collectionName", "Default Collection"}}}}).isEmpty());
        QCOMPARE(calendar.events(0).actions_size(), 2);
        QCOMPARE(calendar.events(0).actions(0).type(), Action::ACTION_TYPE_MACRO);
        QCOMPARE(calendar.events(0).actions(1).uuid().string(), std::string("D1D1D1D1-1111-2222-3333-444444444444"));
        // Back as it came, but for the action kept
        calendar.mutable_events(0)->mutable_actions()->DeleteSubrange(1, 1);
        QCOMPARE(encoded(calendar), encoded(proPresentersOwn()));
        // What cannot be
        QVERIFY(!change(&calendar, id, {{"action", QVariantMap {{"kind", "macro"}}}}).isEmpty());
        QVERIFY(!change(&calendar, id, {{"action", QVariantMap {{"kind", "capture"}}}}).isEmpty());
        // A Capture event is told as one, and given a macro in its Capture's place
        Calendar capture = proPresentersOwn();
        capture.mutable_events(1)->clear_action();
        capture.mutable_events(1)->clear_actions();
        capture.mutable_events(1)->mutable_action()->set_type(103);
        capture.mutable_events(1)->add_actions()->set_type(Action::ACTION_TYPE_CAPTURE);
        QCOMPARE(eventAt(capture, 1).value("action").toMap().value("kind").toString(), QStringLiteral("capture"));
        QVERIFY(change(&capture, "E2E2E2E2-1111-2222-3333-444444444444", {{"action", QVariantMap {{"kind", "macro"}, {"macroId", "A1"}, {"macroName", "M"}}}}).isEmpty());
        QCOMPARE(capture.events(1).actions_size(), 1);
        QCOMPARE(capture.events(1).actions(0).type(), Action::ACTION_TYPE_MACRO);
    }

    void anEventIsAddedAndTakenAway()
    {
        Calendar calendar = proPresentersOwn();
        QString made;
        QVERIFY(add(&calendar, sunday25 + 3600, ids(), &made).isEmpty());
        QCOMPARE(made, QStringLiteral("EVENT-1"));
        const QVariantMap added = eventAt(calendar, 3);
        QCOMPARE(added.value("name").toString(), QStringLiteral("New Event"));
        QCOMPARE(added.value("start").toLongLong(), sunday25 + 3600);
        QCOMPARE(added.value("action").toMap().value("kind").toString(), QStringLiteral("none"));
        QVERIFY(remove(&calendar, made).isEmpty());
        QCOMPARE(encoded(calendar), encoded(proPresentersOwn()));
        QVERIFY(!remove(&calendar, made).isEmpty());
        // A new one is first due at the next whole hour.
        QCOMPARE(newStart(sunday18 - 35 * 60, sydney), sunday18);
        QCOMPARE(newStart(sunday18, sydney), sunday18 + 3600);
    }

    void whenEventsComeDue()
    {
        const Calendar calendar = proPresentersOwn();
        const QVariantMap weekly = eventAt(calendar, 0);
        const QVariantMap once = eventAt(calendar, 1);
        const QVariantMap year = eventAt(calendar, 2);
        // Once: at its time, and only then
        QCOMPARE(occurrences(once, sunday25 - 1, sunday25, sydney, 10), QList<long long> {sunday25});
        QVERIFY(occurrences(once, sunday25, sunday25 + week, sydney, 10).isEmpty());
        // Every Sunday at 7 pm, from its first
        QCOMPARE(occurrences(weekly, sunday18 - week, sunday18 + 2 * week, sydney, 10),
                 (QList<long long> {sunday18, sunday18 + week, sunday18 + 2 * week}));
        QCOMPARE(occurrences(weekly, sunday18 + 1, sunday18 + 2 * week, sydney, 1), QList<long long> {sunday18 + week});
        // Summer time ends on the first Sunday of April 2027: still 7 pm on the clock,
        // an hour later by UTC.
        const QList<long long> autumn = occurrences(weekly, sunday18 + 23 * week - day, sunday18 + 24 * week + day, sydney, 10);
        QCOMPARE(autumn.size(), 2);
        QCOMPARE(hourOf(autumn.at(0)), 19);
        QCOMPARE(hourOf(autumn.at(1)), 19);
        QCOMPARE(autumn.at(1) - autumn.at(0), week + 3600);
        // Every Tuesday until "ends on", a Wednesday: its last the day before that
        const QList<long long> tuesdays = occurrences(year, tuesday27 - 1, wednesday27NextYear + week, sydney, 100);
        QCOMPARE(tuesdays.first(), tuesday27);
        QCOMPARE(QDateTime::fromSecsSinceEpoch(tuesdays.last(), sydney).date(), QDate(2027, 10, 26));
        QCOMPARE(tuesdays.size(), 53);
        // "Ends on" counts when it is a time it is due at.
        QVariantMap exactly = year;
        exactly.insert("ends", tuesdays.at(2));
        QCOMPARE(occurrences(exactly, tuesday27 - 1, tuesday27 + 5 * week, sydney, 10).size(), 3);
        // A day left out, and days that do not include the first: from the next chosen one
        QVariantMap skipping = weekly;
        skipping.insert("excluded", QVariantList {sunday25});
        QCOMPARE(occurrences(skipping, sunday18, sunday18 + 2 * week, sydney, 10), QList<long long> {sunday18 + 2 * week});
        QVariantMap weekdays = weekly;
        weekdays.insert("days", QVariantList {2, 4});
        QCOMPARE(occurrences(weekdays, sunday18 - 1, sunday18 + week, sydney, 10), (QList<long long> {sunday18 + day, sunday18 + 3 * day}));
        // The next across them all, and how late one may still be run
        QCOMPARE(nextAfter(events(calendar), sunday18, sydney), sunday25);
        QCOMPARE(nextAfter(events(calendar), sunday25, sydney), tuesday27);
        QVERIFY(stillDue(sunday25, sunday25));
        QVERIFY(stillDue(sunday25, sunday25 + graceSeconds));
        QVERIFY(!stillDue(sunday25, sunday25 + graceSeconds + 1));
        QVERIFY(!stillDue(sunday25, sunday25 - 1));
    }

    void theListAsTheWindowShowsIt()
    {
        const Calendar calendar = proPresentersOwn();
        const long long now = sunday18 + 4 * day; // Thursday 22 October
        // Each event once, by its first time, the ones that have come due marked
        const QVariantList plain = listing(events(calendar), now, false, sydney, 50);
        QCOMPARE(plain.size(), 3);
        QCOMPARE(plain.at(0).toMap().value("name").toString(), QStringLiteral("New Event"));
        QVERIFY(plain.at(0).toMap().value("past").toBool());
        QVERIFY(!plain.at(1).toMap().value("past").toBool());
        // Each time: the last time each came due, then those to come, in order
        const QVariantList all = listing(events(calendar), now, true, sydney, 5);
        QCOMPARE(all.size(), 6);
        QCOMPARE(all.at(0).toMap().value("section").toString(), QStringLiteral("current"));
        QCOMPARE(all.at(0).toMap().value("at").toLongLong(), sunday18);
        QStringList upcoming;
        for (int i = 1; i < all.size(); ++i) {
            QCOMPARE(all.at(i).toMap().value("section").toString(), QStringLiteral("upcoming"));
            upcoming << all.at(i).toMap().value("name").toString();
        }
        QCOMPARE(upcoming, (QStringList {"New Event", "Once", "Every Tuesday for a year", "New Event", "Every Tuesday for a year"}));
    }

    void datesAndTimesWrittenAndTyped()
    {
        QCOMPARE(dateText(tuesday27, sydney, australian), QStringLiteral("27/10/2026"));
        QCOMPARE(timeText(tuesday27, sydney, australian), QStringLiteral("7:00:00 pm"));
        QCOMPARE(whenText(tuesday27, sydney, australian), QStringLiteral("27 Oct 2026 at 7:00:00 pm"));
        const QLocale british(QLocale::English, QLocale::UnitedKingdom);
        QCOMPARE(timeText(tuesday27, sydney, british), QStringLiteral("19:00:00"));
        // A date typed keeps the time of day; a time keeps the date.
        QCOMPARE(withDate(tuesday27, QStringLiteral("3/11/2026"), sydney, australian), tuesday27 + week);
        QCOMPARE(withDate(tuesday27, QStringLiteral(" 2026-11-03 "), sydney, australian), tuesday27 + week);
        QCOMPARE(withDate(tuesday27, QStringLiteral("31/2/2026"), sydney, australian), -1LL);
        QCOMPARE(withDate(tuesday27, QStringLiteral("soon"), sydney, australian), -1LL);
        for (const QString &typed : {QStringLiteral("7:30:00 pm"), QStringLiteral("7:30pm"), QStringLiteral("19:30"), QStringLiteral("7:30 PM")})
            QCOMPARE(withTime(tuesday27, typed, sydney, australian), tuesday27 + 1800);
        QCOMPARE(withTime(tuesday27, QStringLiteral("7 pm"), sydney, australian), tuesday27);
        QCOMPARE(withTime(tuesday27, QStringLiteral("19:00:30"), sydney, australian), tuesday27 + 30);
        QCOMPARE(withTime(tuesday27, QStringLiteral("12:15 am"), sydney, australian), tuesday27 - 19 * 3600 + 15 * 60);
        for (const QString &typed : {QStringLiteral("13 pm"), QStringLiteral("25:00"), QStringLiteral("7:61"), QStringLiteral("seven")})
            QCOMPARE(withTime(tuesday27, typed, sydney, australian), -1LL);
    }
};

QTEST_GUILESS_MAIN(TestCalendarFile)
#include "tst_calendarfile.moc"
