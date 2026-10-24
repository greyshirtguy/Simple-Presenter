// When a screen playing a video draws what else moves on it (src/framepacing.h): how long a
// frame of the video lasts, from when they came, one late frame not counting; the moments
// after a frame for videos of the usual rates on a 60 Hz screen, and on a 50 Hz one; and
// when a video counts as stopped.

#include "framepacing.h"

#include <QTest>

using namespace framepacing;

namespace {

const double refresh60 = 1000.0 / 60;
const double refresh50 = 1000.0 / 50;

} // namespace

class TestFramePacing : public QObject
{
    Q_OBJECT

private slots:
    void interval()
    {
        QCOMPARE(videoInterval({}), 0.0);
        QCOMPARE(videoInterval({100}), 0.0);
        QCOMPARE(videoInterval({100, 133.3}), 133.3 - 100);
        // One frame late (and the one after it early) does not count
        const double interval = videoInterval({0, 33.3, 66.7, 110.0, 133.3, 166.7, 200.0});
        QVERIFY(qAbs(interval - 33.3) < 0.2);
    }

    void thirtyOnSixty()
    {
        const QList<double> moments = framesBetween(1000.0 / 30, refresh60);
        QCOMPARE(moments.size(), 1);
        QVERIFY(qAbs(moments[0] - refresh60) < 1e-9);
        // A little early or late, still one
        QCOMPARE(framesBetween(1000.0 / 30 - 0.8, refresh60).size(), 1);
        QCOMPARE(framesBetween(1000.0 / 30 + 0.8, refresh60).size(), 1);
    }

    void otherRates()
    {
        // Sixty and fifty: a frame at every refresh, or near enough, so none between
        QVERIFY(framesBetween(1000.0 / 60, refresh60).isEmpty());
        QVERIFY(framesBetween(1000.0 / 50, refresh60).isEmpty());
        // Twenty-five and twenty-four: one, the second refresh being too near the next frame
        QCOMPARE(framesBetween(1000.0 / 25, refresh60).size(), 1);
        QCOMPARE(framesBetween(1000.0 / 24, refresh60).size(), 1);
        // Fifteen: three
        QCOMPARE(framesBetween(1000.0 / 15, refresh60).size(), 3);
        // On a 50 Hz screen: twenty-five has one, fifty none
        QCOMPARE(framesBetween(1000.0 / 25, refresh50).size(), 1);
        QVERIFY(framesBetween(1000.0 / 50, refresh50).isEmpty());
        // Each a whole refresh before the next frame of the video
        for (const double rate : {15.0, 24.0, 25.0, 30.0}) {
            const double interval = 1000.0 / rate;
            for (const double moment : framesBetween(interval, refresh60))
                QVERIFY(moment <= interval - refresh60 + 1.0);
        }
        // Nothing for nothing
        QVERIFY(framesBetween(0, refresh60).isEmpty());
        QVERIFY(framesBetween(33.3, 0).isEmpty());
    }

    void stopped()
    {
        QVERIFY(!videoStopped(30, 1000.0 / 30));
        QVERIFY(!videoStopped(45, 1000.0 / 30));
        QVERIFY(videoStopped(60, 1000.0 / 30));
        QVERIFY(videoStopped(5, 0));
    }
};

QTEST_GUILESS_MAIN(TestFramePacing)
#include "tst_framepacing.moc"
