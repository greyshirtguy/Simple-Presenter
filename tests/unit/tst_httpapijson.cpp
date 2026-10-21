// What the HTTP API's answers are made of (src/httpapijson.h): the app's own maps turned
// into ProPresenter's JSON, as its documentation gives it and a ProPresenter 21.4 was seen
// to answer with.

#include "httpapijson.h"

#include <QRegularExpression>
#include <QTest>

using namespace httpapijson;

class TestHttpApiJson : public QObject
{
    Q_OBJECT

private slots:
    void idsAndColours()
    {
        const QVariantMap made = id(QStringLiteral("8C6A1658-69C6-455A-A356-6D09D46CEA4E"), QStringLiteral("Move Of God"), 4);
        QCOMPARE(made.value("uuid").toString(), QStringLiteral("8C6A1658-69C6-455A-A356-6D09D46CEA4E"));
        QCOMPARE(made.value("name").toString(), QStringLiteral("Move Of God"));
        QCOMPARE(made.value("index").toLongLong(), 4LL);
        QCOMPARE(id(QString(), QString(), noIndex).value("index").toLongLong(), 4294967295LL);
        const QVariantMap orange = color(QColor(QStringLiteral("#ff9300")));
        QCOMPARE(orange.value("red").toDouble(), 1.0);
        QCOMPARE(orange.value("blue").toDouble(), 0.0);
        QVERIFY(qAbs(orange.value("green").toDouble() - 0x93 / 255.0) < 1e-6);
        QCOMPARE(orange.value("alpha").toDouble(), 1.0);
    }

    void theSlidesNowAndNext()
    {
        const QVariantMap current {{"plainText", QStringLiteral("We've come for a move of God")}, {"notesText", QStringLiteral("Slow")},
                                   {"id", QStringLiteral("D3252A24-B2FB-4D6D-B4B5-50A97F582496")}, {"elements", QVariantList {}}};
        const QVariantMap made = slideStatus(current, {});
        const QVariantMap now = made.value("current").toMap();
        QCOMPARE(now.value("text").toString(), QStringLiteral("We've come for a move of God"));
        QCOMPARE(now.value("notes").toString(), QStringLiteral("Slow"));
        QCOMPARE(now.value("uuid").toString(), QStringLiteral("D3252A24-B2FB-4D6D-B4B5-50A97F582496"));
        QCOMPARE(now.size(), 3);
        // None next: empty words, as ProPresenter gives nothing
        QCOMPARE(made.value("next").toMap(), (QVariantMap {{"text", QString()}, {"notes", QString()}, {"uuid", QString()}}));
    }

    void whereTheShowIs()
    {
        // As it was seen: slide 1 of 43, 41 left
        const QVariantMap at = slideIndex(QStringLiteral("presentation_index"), QStringLiteral("8C6A1658-69C6-455A-A356-6D09D46CEA4E"),
                                          QStringLiteral("Move Of God"), noIndex, 1, 43).value("presentation_index").toMap();
        QCOMPARE(at.value("index").toInt(), 1);
        QCOMPARE(at.value("total_cues").toInt(), 43);
        QCOMPARE(at.value("remaining_cues").toInt(), 41);
        QCOMPARE(at.value("presentation_id").toMap().value("index").toLongLong(), noIndex);
        // At none: null
        const QVariantMap none = slideIndex(QStringLiteral("announcement_index"), QString(), QString(), 0, -1, 0);
        QVERIFY(none.contains("announcement_index"));
        QCOMPARE(none.value("announcement_index").metaType(), QMetaType::fromType<std::nullptr_t>());
        // The last: none left
        QCOMPARE(slideIndex(QStringLiteral("k"), {}, {}, 0, 42, 43).value("k").toMap().value("remaining_cues").toInt(), 0);
    }

    void timesAsProPresenterWritesThem()
    {
        QCOMPARE(videoCountdown(1.8), QStringLiteral("0:00:01"));
        QCOMPARE(videoCountdown(0), QStringLiteral("0:00:00"));
        QCOMPARE(videoCountdown(-3), QStringLiteral("0:00:00"));
        QCOMPARE(videoCountdown(3725), QStringLiteral("1:02:05"));
        QCOMPARE(timerTime(99), QStringLiteral("00:01:39"));
        QCOMPARE(timerTime(0), QStringLiteral("00:00:00"));
        QCOMPARE(timerTime(36000 + 59), QStringLiteral("10:00:59"));
        QCOMPARE(timerTime(-5.4), QStringLiteral("-00:00:05"));
    }

    void howATimerStands()
    {
        const QVariantMap countdown {{"kind", QStringLiteral("countdown")}, {"duration", 180}};
        QCOMPARE(timerState(countdown, {{"running", true}, {"seconds", 99.0}}), QStringLiteral("running"));
        QCOMPARE(timerState(countdown, {{"running", true}, {"seconds", -4.0}}), QStringLiteral("overrunning"));
        QCOMPARE(timerState(countdown, {{"running", false}, {"seconds", 180.0}}), QStringLiteral("stopped"));
        QCOMPARE(timerState(countdown, {{"running", false}, {"seconds", 0.0}}), QStringLiteral("complete"));
        QCOMPARE(timerState(countdown, {{"running", false}, {"seconds", -4.0}}), QStringLiteral("overran"));
        const QVariantMap elapsed {{"kind", QStringLiteral("elapsed")}, {"startTime", 0}, {"hasEndTime", true}, {"endTime", 300}};
        QCOMPARE(timerState(elapsed, {{"running", true}, {"seconds", 10.0}}), QStringLiteral("running"));
        QCOMPARE(timerState(elapsed, {{"running", false}, {"seconds", 0.0}}), QStringLiteral("stopped"));
        QCOMPARE(timerState(elapsed, {{"running", false}, {"seconds", 300.0}}), QStringLiteral("complete"));
        QCOMPARE(timerCurrent(countdown, {{"running", true}, {"seconds", 99.0}}, 2).value("time").toString(), QStringLiteral("00:01:39"));
    }

    void timersOfEachKind()
    {
        const QVariantMap countdown = timer({{"id", QStringLiteral("9212EC2F-00DD-4A94-A017-D89A1521D6AA")}, {"name", QStringLiteral("Timer")},
                                             {"kind", QStringLiteral("countdown")}, {"duration", 180.0}, {"overrun", true}}, 0);
        QCOMPARE(countdown.value("allows_overrun").toBool(), true);
        QCOMPARE(countdown.value("countdown").toMap().value("duration").toLongLong(), 180LL);
        QVERIFY(!countdown.contains("elapsed"));
        const QVariantMap elapsed = timer({{"kind", QStringLiteral("elapsed")}, {"startTime", 300.0}, {"hasEndTime", false}}, 2);
        QCOMPARE(elapsed.value("elapsed").toMap(), (QVariantMap {{"start_time", 300LL}}));
        const QVariantMap ended = timer({{"kind", QStringLiteral("elapsed")}, {"startTime", 0.0}, {"hasEndTime", true}, {"endTime", 60.0}}, 2);
        QCOMPARE(ended.value("elapsed").toMap().value("end_time").toLongLong(), 60LL);
        const QVariantMap to = timer({{"kind", QStringLiteral("countdownTo")}, {"timeOfDay", 36000.0}}, 1);
        QCOMPARE(to.value("count_down_to_time").toMap().value("time_of_day").toLongLong(), 36000LL);
        QCOMPARE(to.value("count_down_to_time").toMap().value("period").toString(), QStringLiteral("24_hour"));
    }

    void aLookForEachAudienceScreen()
    {
        // The second screen shows only the slide and the props, in a theme; the third has
        // nothing in the look, and so gets everything
        const QVariantMap saved {{"id", QStringLiteral("C8A3A745-FE2D-49BF-8599-2B1BCE02C891")},
                                 {"screens", QVariantMap {{"a", QVariantMap {{"slide", true}, {"media", true}, {"props", true}, {"messages", true},
                                                                             {"announcements", true}, {"videoInput", true}, {"theme", QString()}, {"mask", QString()}}},
                                                          {"b", QVariantMap {{"slide", true}, {"media", false}, {"props", true}, {"messages", false},
                                                                             {"announcements", false}, {"videoInput", false},
                                                                             {"theme", QStringLiteral("0DFD5F45-3A7D-4340-9090-6A2EA8955FA3")},
                                                                             {"mask", QString()}}}}}};
        const QVariantMap made = look(saved, {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}, QStringLiteral("Lyrics L3rd"), 0);
        const QVariantList screens = made.value("screens").toList();
        QCOMPARE(screens.size(), 3);
        const QVariantMap second = screens[1].toMap();
        QCOMPARE(second.value("media").toBool(), false);
        QCOMPARE(second.value("slide").toBool(), true);
        QCOMPARE(second.value("video_input").toBool(), false);
        QCOMPARE(second.value("presentation").toString(), QStringLiteral("0DFD5F45-3A7D-4340-9090-6A2EA8955FA3"));
        QCOMPARE(screens[2].toMap().value("media").toBool(), true);
        QCOMPARE(made.value("id").toMap().value("name").toString(), QStringLiteral("Lyrics L3rd"));
    }

    void propsAndMessages()
    {
        const QVariantMap made = prop({{"id", QStringLiteral("2908F7CF-E18F-4FE4-A509-E481D5A7E3A9")}, {"name", QStringLiteral("Service Countdown")}}, 0, true);
        QCOMPARE(made.value("is_active").toBool(), true);
        QCOMPARE(made.value("auto_clear_enabled").toBool(), false);
        QCOMPARE(made.value("transition").metaType(), QMetaType::fromType<std::nullptr_t>());
        // A message's words with its tokens in them by name
        const QVariantMap message {{"id", QStringLiteral("M1")}, {"name", QStringLiteral("Parking")}, {"theme", QStringLiteral("Notices")},
                                   {"pieces", QVariantList {QVariantMap {{"token", false}, {"text", QStringLiteral("Car ")}},
                                                            QVariantMap {{"token", true}, {"text", QStringLiteral("t1")}},
                                                            QVariantMap {{"token", false}, {"text", QStringLiteral(" in ")}},
                                                            QVariantMap {{"token", true}, {"text", QStringLiteral("t2")}}}},
                                   {"tokens", QVariantList {QVariantMap {{"id", QStringLiteral("t1")}, {"kind", QStringLiteral("text")},
                                                                         {"name", QStringLiteral("Plate")}, {"value", QStringLiteral("ABC123")}},
                                                            QVariantMap {{"id", QStringLiteral("t2")}, {"kind", QStringLiteral("timer")},
                                                                         {"name", QStringLiteral("Countdown")}, {"timerId", QStringLiteral("T9")}}}}};
        const QVariantMap shown = httpapijson::message(message, 3, false);
        QCOMPARE(shown.value("message").toString(), QStringLiteral("Car ${Plate} in ${Countdown}"));
        const QVariantList tokens = shown.value("tokens").toList();
        QCOMPARE(tokens[0].toMap().value("text").toMap().value("text").toString(), QStringLiteral("ABC123"));
        QVERIFY(tokens[1].toMap().contains("timer"));
        QCOMPARE(shown.value("theme").toMap().value("name").toString(), QStringLiteral("Notices"));
        QCOMPARE(shown.value("is_active").toBool(), false);
    }

    void clearGroupsInProPresentersWords()
    {
        const QVariantMap group {{"id", QStringLiteral("0B290A4C-E9A3-492D-B6C9-DF4D3F609ABB")}, {"name", QStringLiteral("Clear All")},
                                 {"layers", QStringList {QStringLiteral("audio"), QStringLiteral("audioEffects"), QStringLiteral("messages"),
                                                         QStringLiteral("props"), QStringLiteral("announcements"), QStringLiteral("slide"),
                                                         QStringLiteral("media"), QStringLiteral("videoInput")}},
                                 {"picture", QStringLiteral("All")}, {"tinted", false}, {"tint", QStringLiteral("#ffffff")}};
        const QVariantMap made = clearGroup(group, 0);
        // As the one seen: every layer by ProPresenter's names, in its order
        QCOMPARE(made.value("layers").toStringList(), QStringList({QStringLiteral("music"), QStringLiteral("audio_effects"), QStringLiteral("messages"),
                                                                   QStringLiteral("props"), QStringLiteral("announcements"), QStringLiteral("presentation"),
                                                                   QStringLiteral("presentation_media"), QStringLiteral("video_input")}));
        QCOMPARE(made.value("icon").toString(), QStringLiteral("All"));
        QCOMPARE(made.value("tint").metaType(), QMetaType::fromType<std::nullptr_t>());
        QCOMPARE(clearGroup({{"picture", QStringLiteral("7")}}, 1).value("icon").toString(), QStringLiteral("Seven"));
        QCOMPARE(clearGroup({{"picture", QStringLiteral("Megaphone")}}, 1).value("icon").toString(), QStringLiteral("Megahorn"));
        QCOMPARE(clearGroup({{"picture", QStringLiteral("Star")}, {"image", QStringLiteral("data:...")}}, 1).value("icon").toString(),
                 QStringLiteral("Custom"));
        QCOMPARE(clearGroup({{"tinted", true}, {"tint", QStringLiteral("#ff0000")}}, 1).value("tint").toMap().value("red").toDouble(), 1.0);
    }

    void macrosActionsByType()
    {
        const QVariantMap made = macro({{"id", QStringLiteral("AD63C796-8C26-45EF-B494-E9B9726DEA88")}, {"name", QStringLiteral("Singing")},
                                        {"color", QStringLiteral("#ffff00")},
                                        {"actions", QVariantList {QVariantMap {{"kind", QStringLiteral("stage")}}, QVariantMap {{"kind", QStringLiteral("look")}},
                                                                  QVariantMap {{"kind", QStringLiteral("clear")}}, QVariantMap {{"kind", QStringLiteral("other")}}}}},
                                       0);
        QStringList types;
        for (const QVariant &action : made.value("actions").toList())
            types << action.toMap().value("type").toString();
        QCOMPARE(types, QStringList({QStringLiteral("stage_layout"), QStringLiteral("audience_look"), QStringLiteral("clear"), QStringLiteral("unknown")}));
        QCOMPARE(made.value("image_type").toString(), QStringLiteral("Default"));
    }

    void videoInputsByTheirIdsAndAVideoInputActionIsMedia()
    {
        const QVariantMap made = videoInput({{"id", QStringLiteral("6859FACB-8EAE-470D-A0F2-AE8BE864393B")}, {"name", QStringLiteral("Input 1")},
                                             {"device", QStringLiteral("/dev/video0")}}, 2);
        QCOMPARE(made.keys(), QStringList({QStringLiteral("id")}));
        const QVariantMap id = made.value("id").toMap();
        QCOMPARE(id.value("uuid").toString(), QStringLiteral("6859FACB-8EAE-470D-A0F2-AE8BE864393B"));
        QCOMPARE(id.value("name").toString(), QStringLiteral("Input 1"));
        QCOMPARE(id.value("index").toInt(), 2);
        const QVariantMap action = macro({{"actions", QVariantList {QVariantMap {{"kind", QStringLiteral("videoInput")}}}}}, 0);
        QCOMPARE(action.value("actions").toList().value(0).toMap().value("type").toString(), QStringLiteral("media"));
    }

    void librariesByNameMadeUuids()
    {
        const QVariantList made = libraries({QStringLiteral("MainHall"), QStringLiteral("Demo")});
        QCOMPARE(made.size(), 2);
        const QString uuid = made[0].toMap().value("uuid").toString();
        // The same every time, written as ProPresenter writes UUIDs, and one name's own
        QCOMPARE(uuid, libraryUuid(QStringLiteral("MainHall")));
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9A-F]{8}-[0-9A-F]{4}-5[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$")).match(uuid).hasMatch());
        QVERIFY(uuid != made[1].toMap().value("uuid").toString());
        QCOMPARE(made[1].toMap().value("index").toInt(), 1);
    }

    void playlistTrees()
    {
        // As seen: Folder 2 holding Playlist3, then Sunday Morning, then an empty folder
        const QVariantList nodes {QVariantMap {{"name", QStringLiteral("Folder 2")}, {"path", QStringLiteral("F2")}, {"parent", QString()}, {"folder", true}},
                                  QVariantMap {{"name", QStringLiteral("Playlist3")}, {"path", QStringLiteral("P3")}, {"parent", QStringLiteral("F2")}, {"folder", false}},
                                  QVariantMap {{"name", QStringLiteral("Sunday Morning")}, {"path", QStringLiteral("SM")}, {"parent", QString()}, {"folder", false}},
                                  QVariantMap {{"name", QStringLiteral("Folder 1")}, {"path", QStringLiteral("F1")}, {"parent", QString()}, {"folder", true}}};
        const QVariantList made = playlistTree(nodes);
        QCOMPARE(made.size(), 3);
        const QVariantMap folder = made[0].toMap();
        QCOMPARE(folder.value("field_type").toString(), QStringLiteral("group"));
        const QVariantList inside = folder.value("children").toList();
        QCOMPARE(inside.size(), 1);
        QCOMPARE(inside[0].toMap().value("id").toMap().value("name").toString(), QStringLiteral("Playlist3"));
        QCOMPARE(inside[0].toMap().value("id").toMap().value("index").toInt(), 0);
        QCOMPARE(made[1].toMap().value("field_type").toString(), QStringLiteral("playlist"));
        QCOMPARE(made[1].toMap().value("id").toMap().value("index").toInt(), 1);
        QCOMPARE(made[2].toMap().value("children").toList().size(), 0);
        // The media playlists' and the audio bin's say "type" instead
        const QVariantList bins = binTree(nodes);
        QCOMPARE(bins[0].toMap().value("type").toString(), QStringLiteral("group"));
        QVERIFY(!bins[0].toMap().contains("field_type"));
    }

    void binRows()
    {
        const QVariantMap video = binItem({{"id", QStringLiteral("259A53F4")}, {"name", QStringLiteral("Hopeful Horizon Access - 4K")},
                                           {"video", true}, {"duration", 30.4}}, 0, false);
        QCOMPARE(video.value("type").toString(), QStringLiteral("video"));
        QCOMPARE(video.value("duration").toLongLong(), 30LL);
        QCOMPARE(binItem({{"video", false}}, 1, false).value("type").toString(), QStringLiteral("image"));
        QCOMPARE(binItem({{"duration", 280.0}}, 0, true).value("type").toString(), QStringLiteral("audio"));
    }
};

QTEST_APPLESS_MAIN(TestHttpApiJson)
#include "tst_httpapijson.moc"
