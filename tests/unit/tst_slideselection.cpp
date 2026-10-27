// Which slides are selected (src/slideselection.h): a click with Ctrl held puts one in,
// or takes it out, the live one counting as selected; a click with Shift held selects
// from the last selected, or the live one, or the first; and what a change goes to,
// the selected ones or, with none, all of them.

#include "slideselection.h"

#include <QTest>

using namespace slideselection;

class TestSlideSelection : public QObject
{
    Q_OBJECT

private slots:
    void withCtrl()
    {
        // The live slide (4) counts as selected: the first click with Ctrl keeps it
        QCOMPARE(toggled({}, 4, 7), (QList<int> {4, 7}));
        // and one with nothing live starts from nothing
        QCOMPARE(toggled({}, -1, 7), QList<int> {7});
        // Clicked again, it is taken out; the others stay
        QCOMPARE(toggled({4, 7}, 4, 7), QList<int> {4});
        // The live one can be taken out too
        QCOMPARE(toggled({4, 7}, 4, 4), QList<int> {7});
        // In the list's order, each once
        QCOMPARE(toggled({9, 2}, -1, 5), (QList<int> {2, 5, 9}));
    }

    void withShift()
    {
        // From the last selected
        QCOMPARE(extended({2}, 2, -1, 5), (QList<int> {2, 3, 4, 5}));
        // upwards as well
        QCOMPARE(extended({8}, 8, -1, 5), (QList<int> {5, 6, 7, 8}));
        // keeping what else is selected
        QCOMPARE(extended({0, 8}, 8, -1, 6), (QList<int> {0, 6, 7, 8}));
        // With none selected, from the live slide
        QCOMPARE(extended({}, -1, 3, 6), (QList<int> {3, 4, 5, 6}));
        // and with none live either, from the first
        QCOMPARE(extended({}, -1, -1, 2), (QList<int> {0, 1, 2}));
        // The last one selected is the anchor, not the live one
        QCOMPARE(extended({1, 3}, 1, 3, 4), (QList<int> {1, 2, 3, 4}));
    }

    void whatAChangeGoesTo()
    {
        QCOMPARE(targets({}, 3), (QList<int> {0, 1, 2}));
        QCOMPARE(targets({2, 0}, 3), (QList<int> {0, 2}));
        // (A place past the end, the presentation having lost slides since, is no target)
        QCOMPARE(targets({1, 7}, 3), QList<int> {1});
        QCOMPARE(targets({7}, 3), (QList<int> {0, 1, 2}));
        QCOMPARE(targets({}, 0), QList<int> {});
    }
};

QTEST_GUILESS_MAIN(TestSlideSelection)
#include "tst_slideselection.moc"
