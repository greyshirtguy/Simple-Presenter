// Where an element dragged up or down the editor's list of a slide's elements lands
// (src/elementorder.h): the list has the front one first and the file the back one
// first, and a landing is a gap between rows, counted with the element still in place.

#include "elementorder.h"

#include <QList>
#include <QTest>

using namespace elementorder;

namespace {

// The order drawn, as letters, after the one at `from` has been let go at `landing`:
// done the long way, by taking it out of the list as the editor shows it and putting
// it in again, to hold placed() to.
QString after(const QString &drawn, int from, int landing)
{
    QList<QChar> list;
    for (int i = drawn.size() - 1; i >= 0; --i)
        list.append(drawn.at(i));
    const int row = drawn.size() - 1 - from;
    const QChar moved = list.takeAt(row);
    list.insert(landing > row ? landing - 1 : landing, moved);
    QString result;
    for (int i = list.size() - 1; i >= 0; --i)
        result.append(list.at(i));
    return result;
}

// The same, by what placed() answers: the element moved to that place in the order
// drawn, or the order as it was where it answers that nothing changes.
QString moved(const QString &drawn, int from, int landing)
{
    const int to = placed(int(drawn.size()), from, landing);
    if (to < 0)
        return drawn;
    QString result = drawn;
    const QChar element = result.at(from);
    result.remove(from, 1);
    result.insert(to, element);
    return result;
}

}

class TestElementOrder : public QObject
{
    Q_OBJECT

private slots:
    void theTopOfTheListIsTheFront()
    {
        // Three elements, drawn back, middle, front: the list reads front, middle, back.
        // The back one (row 2) let go over the first row is the front one after.
        QCOMPARE(placed(3, 0, 0), 2);
        QCOMPARE(moved("bmf", 0, 0), "mfb");
    }

    void underTheLastRowIsTheBack()
    {
        // The front one (row 0) let go under the last row
        QCOMPARE(placed(3, 2, 3), 0);
        QCOMPARE(moved("bmf", 2, 3), "fbm");
    }

    void betweenTwoRows()
    {
        // The front one let go between the middle one and the back one
        QCOMPARE(placed(3, 2, 2), 1);
        QCOMPARE(moved("bmf", 2, 2), "bfm");
        // The back one let go between the front one and the middle one
        QCOMPARE(placed(3, 0, 1), 1);
        QCOMPARE(moved("bmf", 0, 1), "mbf");
    }

    void justOverItselfOrJustUnderChangesNothing()
    {
        // The middle one is row 1: the landings on either side of it are 1 and 2.
        QCOMPARE(placed(3, 1, 1), -1);
        QCOMPARE(placed(3, 1, 2), -1);
        // And the ends: the front one over the first row, the back one under the last
        QCOMPARE(placed(3, 2, 0), -1);
        QCOMPARE(placed(3, 2, 1), -1);
        QCOMPARE(placed(3, 0, 2), -1);
        QCOMPARE(placed(3, 0, 3), -1);
        // One element alone has nowhere to go.
        QCOMPARE(placed(1, 0, 0), -1);
        QCOMPARE(placed(1, 0, 1), -1);
    }

    void whatIsNotThereGoesNowhere()
    {
        QCOMPARE(placed(0, 0, 0), -1);
        QCOMPARE(placed(3, -1, 0), -1);
        QCOMPARE(placed(3, 3, 0), -1);
        QCOMPARE(placed(3, 1, -1), -1);
        QCOMPARE(placed(3, 1, 4), -1);
    }

    void everyLandingOfEveryElementIsWhereTheListWouldHaveIt()
    {
        // Every element of a pile of up to six, let go at every landing there is: the
        // order drawn after is what taking the row out of the list and putting it in
        // again gives, and nothing is answered but for a change.
        const QString all = "abcdef";
        for (int count = 1; count <= all.size(); ++count) {
            const QString drawn = all.left(count);
            for (int from = 0; from < count; ++from) {
                for (int landing = 0; landing <= count; ++landing) {
                    const QString wanted = after(drawn, from, landing);
                    QCOMPARE(moved(drawn, from, landing), wanted);
                    QCOMPARE(placed(count, from, landing) < 0, wanted == drawn);
                }
            }
        }
    }
};

QTEST_APPLESS_MAIN(TestElementOrder)
#include "tst_elementorder.moc"
