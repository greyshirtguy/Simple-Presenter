// What plays a timeline (src/timelineplayer.h): that its cues are said due at their
// times, in their order, from where it is started; that it stops at its end or goes
// round; and what pausing, moving the playhead, being left and recording do. Short
// timelines, of fractions of a second, on the real clock.

#include "timelineplayer.h"

#include <QSignalSpy>
#include <QTest>

namespace {

QVariantMap cue(int place, double time, const char *track, double length = 0)
{
    return {{"place", place}, {"time", time}, {"track", QString::fromLatin1(track)}, {"length", length},
            {"name", QStringLiteral("Cue %1").arg(place + 1)}};
}

QVariantMap timelineOf(const QVariantList &cues, double duration, bool loop = false)
{
    return {{"duration", duration}, {"loop", loop}, {"cues", cues}};
}

// The places said due, in order, and with what offsets
QList<int> placesOf(const QSignalSpy &spy)
{
    QList<int> places;
    for (const QList<QVariant> &call : spy)
        places << call.at(0).toMap().value("place").toInt();
    return places;
}

} // namespace

class TimelinePlayerTest : public QObject
{
    Q_OBJECT

private slots:
    void playsItsCuesAtTheirTimesAndStopsAtItsEnd()
    {
        TimelinePlayer player;
        QSignalSpy due(&player, &TimelinePlayer::cueDue);
        player.load("a.pro", "a", "", timelineOf({cue(0, 0, "slide"), cue(1, 0.15, "action"), cue(2, 0.1, "slide")}, 0.3));
        QVERIFY(!player.playing());
        player.play();
        QVERIFY(player.playing());
        // At once, what is at the start
        QCOMPARE(placesOf(due), QList<int> {0});
        QTRY_COMPARE_WITH_TIMEOUT(placesOf(due), (QList<int> {0, 2, 1}), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!player.playing(), 1000);
        // Stopped at its end, which is where the playhead stays
        QCOMPARE(player.position(), 0.3);
        // Played again from there, it starts from the start (A GUESS at ProPresenter)
        due.clear();
        player.play();
        QCOMPARE(placesOf(due), QList<int> {0});
        player.pause();
    }

    void goesRoundWithLoop()
    {
        TimelinePlayer player;
        QSignalSpy due(&player, &TimelinePlayer::cueDue);
        player.load("a.pro", "a", "", timelineOf({cue(0, 0, "slide"), cue(1, 0.05, "slide")}, 0.12, true));
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(due.size() >= 4, 1500);
        QCOMPARE(placesOf(due).mid(0, 4), (QList<int> {0, 1, 0, 1}));
        QVERIFY(player.playing());
        player.stop();
        QVERIFY(!player.playing());
    }

    // Started part way: the slide before, and the audio under the playhead, how far in
    void startsWhereThePlayheadIs()
    {
        TimelinePlayer player;
        QSignalSpy due(&player, &TimelinePlayer::cueDue);
        QSignalSpy paused(&player, &TimelinePlayer::paused);
        player.load("a.pro", "a", "", timelineOf({cue(0, 0, "audio", 10), cue(1, 1, "slide"), cue(2, 2, "slide"), cue(3, 1.5, "action")}, 10));
        player.seek(1.75);
        QCOMPARE(player.position(), 1.75);
        QVERIFY(due.isEmpty());
        player.play();
        QCOMPARE(placesOf(due), (QList<int> {0, 1}));
        QCOMPARE(due.at(0).at(1).toDouble(), 1.75);
        QCOMPARE(due.at(1).at(1).toDouble(), 0.0);
        // Moved while it plays: paused (its audio is to stop), and started again there
        due.clear();
        player.seek(5);
        QCOMPARE(paused.size(), 1);
        QVERIFY(player.playing());
        QCOMPARE(placesOf(due), (QList<int> {0, 2}));
        QVERIFY(qAbs(due.at(0).at(1).toDouble() - 5) < 0.01);
        // Rewound
        due.clear();
        player.rewind();
        QCOMPARE(placesOf(due), QList<int> {0});
        QCOMPARE(due.at(0).at(1).toDouble(), 0.0);
        // Paused by hand: where it was, and said to be
        player.pause();
        QCOMPARE(paused.size(), 3);
        QVERIFY(!player.playing());
        QVERIFY(player.position() < 0.5);
    }

    // Stopped by another presentation's slide on its layer, and not by its own
    void stopsWhenAnotherPresentationTakesTheLayer()
    {
        TimelinePlayer player;
        QSignalSpy paused(&player, &TimelinePlayer::paused);
        player.load("a.pro", "a", "list", timelineOf({cue(0, 5, "slide")}, 10));
        player.play();
        player.wentLive("a", "list");
        QVERIFY(player.playing());
        player.wentLive("a", "");
        QVERIFY(!player.playing());
        // (Stopped, not paused: what it started is left to play)
        QCOMPARE(paused.size(), 0);
    }

    // Read again: the same presentation goes on where it is; another's starts afresh
    void followsItsPresentationReadAgain()
    {
        TimelinePlayer player;
        QSignalSpy due(&player, &TimelinePlayer::cueDue);
        player.load("a.pro", "a", "", timelineOf({cue(0, 0.2, "slide")}, 10));
        player.seek(0.1);
        player.play();
        QVERIFY(due.isEmpty());
        // A cue moved to before where it is is not done; one moved after, is
        player.load("a.pro", "a", "", timelineOf({cue(0, 0.05, "slide"), cue(1, 0.25, "action")}, 10));
        QVERIFY(player.playing());
        QTRY_COMPARE_WITH_TIMEOUT(placesOf(due), QList<int> {1}, 1000);
        player.load("b.pro", "b", "", timelineOf({}, 10));
        QVERIFY(!player.playing());
        QCOMPARE(player.position(), 0.0);
        QCOMPARE(player.path(), QStringLiteral("b.pro"));
    }

    // Recording: kept and shown, not done, until taken to be written
    void recordsWhereThePlayheadIs()
    {
        TimelinePlayer player;
        QSignalSpy due(&player, &TimelinePlayer::cueDue);
        player.record(QVariantMap {{"track", "slide"}, {"slideId", "A"}, {"name", "Cue 1"}});
        QCOMPARE(player.recordedCount(), 0);
        player.load("a.pro", "a", "", timelineOf({cue(0, 0, "slide")}, 10));
        player.setRecording(true);
        player.seek(2.5);
        player.record(QVariantMap {{"track", "slide"}, {"slideId", "A"}, {"name", "Cue 1"}});
        player.record(QVariantMap {{"track", "action"}, {"name", "Opening"}, {"wanted", QVariantMap {{"kind", "macro"}, {"macroId", "M"}}}});
        QCOMPARE(player.recordedCount(), 2);
        const QVariantList shown = player.timeline().value("cues").toList();
        QCOMPARE(shown.size(), 3);
        QCOMPARE(shown.at(1).toMap().value("recorded").toBool(), true);
        QCOMPARE(shown.at(1).toMap().value("place").toInt(), -1);
        QCOMPARE(shown.at(1).toMap().value("time").toDouble(), 2.5);
        // Not done by the timeline before it is written
        player.seek(2.4);
        player.play();
        QTest::qWait(250);
        QCOMPARE(placesOf(due), QList<int> {0});
        player.pause();
        const QVariantList taken = player.takeRecorded();
        QCOMPARE(taken.size(), 2);
        QCOMPARE(taken.at(0).toMap(), (QVariantMap {{"time", 2.5}, {"name", "Cue 1"}, {"slideId", "A"}}));
        QCOMPARE(taken.at(1).toMap().value("wanted").toMap().value("macroId").toString(), QStringLiteral("M"));
        QCOMPARE(player.recordedCount(), 0);
        // Another presentation given it drops what was recorded and stops recording
        player.record(QVariantMap {{"track", "slide"}, {"slideId", "B"}});
        player.load("b.pro", "b", "", timelineOf({}, 10));
        QCOMPARE(player.recordedCount(), 0);
        QVERIFY(!player.recording());
    }

    // The playhead is told as it moves only while something shows it
    void saysWhereItIsOnlyWhileWatched()
    {
        TimelinePlayer player;
        player.load("a.pro", "a", "", timelineOf({}, 10));
        QSignalSpy moved(&player, &TimelinePlayer::positionChanged);
        player.play();
        QTest::qWait(350);
        QCOMPARE(moved.size(), 0);
        player.setWatched(true);
        QTRY_VERIFY_WITH_TIMEOUT(moved.size() >= 2, 1000);
        player.pause();
    }
};

QTEST_GUILESS_MAIN(TimelinePlayerTest)
#include "tst_timelineplayer.moc"
