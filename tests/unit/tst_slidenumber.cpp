// A slide asked for by its number (src/slidenumber.h): what the digits typed so far
// make, what Backspace leaves of them, and which slide of a presentation a number is.

#include "slidenumber.h"

#include <QTest>

using namespace slidenumber;

class TestSlideNumber : public QObject
{
    Q_OBJECT

private slots:
    void digitsAddUp()
    {
        QCOMPARE(typed("", "1"), "1");
        QCOMPARE(typed("1", "2"), "12");
        QCOMPARE(typed("12", "0"), "120");
        QCOMPARE(typed(typed(typed("", "3"), "0"), "7"), "307");
    }

    void aNumberHasFourDigitsAtMost()
    {
        QCOMPARE(kMostDigits, 4);
        QCOMPARE(typed("123", "4"), "1234");
        QCOMPARE(typed("1234", "5"), "1234");
    }

    void aNoughtThatLeadsIsDroppedForTheDigitAfterIt()
    {
        // It is shown, being what was typed; and is no slide's number.
        QCOMPARE(typed("", "0"), "0");
        QCOMPARE(slideFor("0", 10), -1);
        QCOMPARE(typed("0", "5"), "5");
        QCOMPARE(typed("0", "0"), "0");
        // One that does not lead is a digit like another.
        QCOMPARE(typed("5", "0"), "50");
    }

    void whatIsNoDigitChangesNothing()
    {
        for (const char *key : {"a", "A", " ", "", "12", "-", "+", ".", "\r", "!"}) {
            QCOMPARE(typed("", key), "");
            QCOMPARE(typed("4", key), "4");
        }
        // The digits of another script are not these.
        QCOMPARE(typed("4", QString(QChar(0x0663))), "4");
    }

    void backspaceTakesTheLastDigitOff()
    {
        QCOMPARE(erased("123"), "12");
        QCOMPARE(erased("12"), "1");
        QCOMPARE(erased("1"), "");
        QCOMPARE(erased(""), "");
    }

    void aNumberIsTheSlideThatWearsIt()
    {
        // The slides are numbered from 1 in the grid; their places are from 0.
        QCOMPARE(slideFor("1", 12), 0);
        QCOMPARE(slideFor("9", 12), 8);
        QCOMPARE(slideFor("12", 12), 11);
        QCOMPARE(slideFor("1000", 1000), 999);
    }

    void aNumberNoSlideHasIsNoSlide()
    {
        QCOMPARE(slideFor("13", 12), -1);
        QCOMPARE(slideFor("0", 12), -1);
        QCOMPARE(slideFor("9999", 12), -1);
        QCOMPARE(slideFor("1", 0), -1);
        QCOMPARE(slideFor("", 12), -1);
        // And what is no number at all, however it came to be asked.
        QCOMPARE(slideFor("x", 12), -1);
        QCOMPARE(slideFor("1x", 12), -1);
        QCOMPARE(slideFor("-1", 12), -1);
        QCOMPARE(slideFor("12345", 99999), -1);
    }
};

QTEST_APPLESS_MAIN(TestSlideNumber)
#include "tst_slidenumber.moc"
