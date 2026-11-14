#include <QtTest>

#include "dragstart.h"

// When a press becomes a drag (dragstart.h): twelve pixels away, straight, with the
// button held for 150 ms; both, not either.
class TestDragStart : public QObject
{
    Q_OBJECT

private slots:
    // A press held long enough, and moved far enough, is a drag
    void farEnoughAndLongEnoughIsADrag()
    {
        QVERIFY(dragstart::begins({100, 100}, {100, 112}, 150));
        QVERIFY(dragstart::begins({100, 100}, {80, 100}, 400));
    }

    // Moved far, but within the first 150 ms: a click made with the pointer on its way
    void farButQuickIsNot()
    {
        QVERIFY(!dragstart::begins({100, 100}, {400, 100}, 0));
        QVERIFY(!dragstart::begins({100, 100}, {100, 300}, 149));
    }

    // Held long, but shifted a few pixels: a press on a trackpad
    void longButNearIsNot()
    {
        QVERIFY(!dragstart::begins({100, 100}, {105, 103}, 2000));
        QVERIFY(!dragstart::begins({100, 100}, {111.9, 100}, 2000));
    }

    // The distance is straight, not across and down added up: 9 and 9 is 12.7 away, and
    // 8 and 8 is 11.3
    void theDistanceIsStraight()
    {
        QVERIFY(dragstart::begins({0, 0}, {9, 9}, 200));
        QVERIFY(!dragstart::begins({0, 0}, {8, 8}, 200));
        QVERIFY(dragstart::begins({0, 0}, {-9, -9}, 200));
    }

    // The steady clock goes on, and what QML is given measures from a press's now()
    void theClockAndWhatQmlAsks()
    {
        const double before = dragstart::now();
        QTest::qWait(20);
        QVERIFY(dragstart::now() >= before + 15);

        DragStart rule;
        QVERIFY(!rule.begins({0, 0}, {50, 0}, rule.now()));
        QVERIFY(rule.begins({0, 0}, {50, 0}, rule.now() - 200));
        QVERIFY(!rule.begins({0, 0}, {5, 0}, rule.now() - 200));
    }
};

QTEST_GUILESS_MAIN(TestDragStart)
#include "tst_dragstart.moc"
