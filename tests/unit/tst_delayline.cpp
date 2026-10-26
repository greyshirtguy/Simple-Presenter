// The rules of a screen's video delay (src/delayline.h): which picture held is shown when,
// on a screen of sixty frames a second, a picture drawn at each frame or now and then, a
// little before or after its moment; when the next is due; how many to hold.

#include "delayline.h"

#include <QTest>

using namespace delayline;

class TestDelayLine : public QObject
{
    Q_OBJECT

private slots:
    void noneHeld()
    {
        QCOMPARE(shown({}, 100, 50, 16.7), -1);
        QCOMPARE(nextDue({}, -1, 50, 16.7), -1.0);
    }

    void exactlySoManyFramesLate()
    {
        // A picture at every frame of a 60 Hz screen, each up to 2 ms off its moment,
        // three frames late: at each frame, the one drawn three frames before
        const double frame = 1000.0 / 60;
        QList<double> drawn;
        for (int index = 0; index < 20; ++index)
            drawn << index * frame + (index % 3 == 0 ? 2 : index % 3 == 1 ? -2 : 0);
        for (int index = 3; index < 20; ++index) {
            const double now = index * frame + (index % 2 ? 1.5 : -1.5);
            QCOMPARE(shown(drawn, now, 3 * frame, frame), index - 3);
        }
    }

    void nowAndThen()
    {
        // A slide changed at 0 and at 500, a delay of 100: the first until 600, then the second
        const QList<double> drawn {0, 500};
        QCOMPARE(shown(drawn, 300, 100, 16.7), 0);
        QCOMPARE(shown(drawn, 580, 100, 16.7), 0);
        QCOMPARE(shown(drawn, 600, 100, 16.7), 1);
        QCOMPARE(nextDue(drawn, 0, 100, 16.7), 500 + 100 - 16.7 / 2);
        QCOMPARE(nextDue(drawn, 1, 100, 16.7), -1.0);
        // Until any is old enough: the oldest (the delay just set)
        QCOMPARE(shown({990, 1000}, 1000, 100, 16.7), 0);
    }

    void howManyToHold()
    {
        QCOMPARE(most(100, 1000.0 / 60), 6 + 2);
        QCOMPARE(most(1000.0 / 30 * 4, 1000.0 / 30), 4 + 2);
        QCOMPARE(most(0, 16.7), 2);
        QCOMPARE(most(50, 0), 2);
    }
};

QTEST_GUILESS_MAIN(TestDelayLine)
#include "tst_delayline.moc"
