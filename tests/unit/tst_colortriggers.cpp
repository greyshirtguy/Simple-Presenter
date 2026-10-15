// A timer's Color Triggers (src/colortriggers.h): which is in force as a countdown goes
// down and as an elapsed time goes up, ProPresenter's example of its help among them;
// and a trigger's time written as the inspector shows it and read back as it is typed.

#include "colortriggers.h"

#include <QTest>

using namespace colortriggers;

class TestColorTriggers : public QObject
{
    Q_OBJECT

private slots:
    // ProPresenter's example: orange at 1:00, yellow at 0:30, red for the last ten
    // seconds, and red through an overrun. In the order they were added, which is not
    // what decides.
    void countingDown()
    {
        const QList<double> times {30, 60, 10};
        QCOMPARE(inForce(times, 300, true), -1);  // above them all: the words' own colour
        QCOMPARE(inForce(times, 60.5, true), -1);
        QCOMPARE(inForce(times, 60, true), 1);    // reached at the time itself
        QCOMPARE(inForce(times, 59.9999999, true), 1);
        QCOMPARE(inForce(times, 45, true), 1);
        QCOMPARE(inForce(times, 30, true), 0);
        QCOMPARE(inForce(times, 12, true), 0);
        QCOMPARE(inForce(times, 10, true), 2);
        QCOMPARE(inForce(times, 0, true), 2);
        QCOMPARE(inForce(times, -25, true), 2);   // an overrun keeps the last
        // ProPresenter's own stage layouts: one trigger, at zero
        QCOMPARE(inForce({0}, 1, true), -1);
        QCOMPARE(inForce({0}, 0, true), 0);
        QCOMPARE(inForce({0}, -3, true), 0);
        QCOMPARE(inForce({}, 0, true), -1);
    }

    // An elapsed time goes up, past each in turn (a guess: see the header)
    void countingUp()
    {
        const QList<double> times {600, 300, 900};
        QCOMPARE(inForce(times, 0, false), -1);
        QCOMPARE(inForce(times, 299, false), -1);
        QCOMPARE(inForce(times, 300, false), 1);
        QCOMPARE(inForce(times, 650, false), 0);
        QCOMPARE(inForce(times, 900, false), 2);
        QCOMPARE(inForce(times, 4000, false), 2);
    }

    // Two triggers at one time: the first of them in the list
    void twoAtOneTime()
    {
        QCOMPARE(inForce({30, 30}, 20, true), 0);
        QCOMPARE(inForce({30, 30}, 40, false), 0);
    }

    void writtenAsTheInspectorShowsThem()
    {
        QCOMPARE(written(0), QStringLiteral("0:00.00"));
        QCOMPARE(written(10), QStringLiteral("0:10.00"));
        QCOMPARE(written(60), QStringLiteral("1:00.00"));
        QCOMPARE(written(90.5), QStringLiteral("1:30.50"));
        QCOMPARE(written(59.999), QStringLiteral("1:00.00"));
        QCOMPARE(written(3660), QStringLiteral("1:01:00.00"));
        QCOMPARE(written(36000 + 59 * 60 + 59.5), QStringLiteral("10:59:59.50"));
        QCOMPARE(written(-5), QStringLiteral("0:00.00"));
    }

    void readAsTyped()
    {
        QCOMPARE(read("1:00.00"), 60.0);
        QCOMPARE(read("1:00"), 60.0);
        QCOMPARE(read(" 0:30.5 "), 30.5);
        QCOMPARE(read(":30"), 30.0);
        QCOMPARE(read("90"), 90.0);
        QCOMPARE(read(".5"), 0.5);
        QCOMPARE(read("1:01:00.00"), 3660.0);
        QCOMPARE(read("0"), 0.0);
        // What it writes, it reads
        for (double seconds : {0.0, 9.25, 60.0, 754.5, 3725.75})
            QCOMPARE(read(written(seconds)), seconds);
        // And what is not a time is not
        for (const char *wrong : {"", "1:", "a", "1:2:3:4", "1:-5", "1.5:00", "1,5", "--"})
            QCOMPARE(read(QString::fromLatin1(wrong)), -1.0);
    }
};

QTEST_APPLESS_MAIN(TestColorTriggers)
#include "tst_colortriggers.moc"
