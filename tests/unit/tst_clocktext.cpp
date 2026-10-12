// The time of day as a text box linked to the clock shows it (src/clocktext.h): each of
// the five ways a date and a time can be written, on a moment that is given, in the
// words of a place that is given.

#include "clocktext.h"

#include <QTest>
#include <QTimeZone>

using namespace clocktext;

class TestClockText : public QObject
{
    Q_OBJECT

    const QLocale american {QLocale::English, QLocale::UnitedStates};
    // A Tuesday afternoon, and a morning with one figure in its hour, minute and day
    const QDateTime afternoon {QDate(1937, 11, 23), QTime(15, 30, 32), QTimeZone::UTC};
    const QDateTime morning {QDate(2026, 3, 5), QTime(9, 4, 7), QTimeZone::UTC};

private slots:
    void aTimeIsWrittenTheFiveWays()
    {
        QCOMPARE(written(afternoon, None, None, false, american), "");
        QCOMPARE(written(afternoon, None, Short, false, american), "3:30 PM");
        QCOMPARE(written(afternoon, None, Medium, false, american), "3:30:32 PM");
        // The long way has the zone after it, in short; the full way, in words.
        const QString longWay = written(afternoon, None, Long, false, american);
        QVERIFY2(longWay.startsWith("3:30:32 PM ") && longWay.size() > 11, qPrintable(longWay));
        const QString fullWay = written(afternoon, None, Full, false, american);
        QVERIFY2(fullWay.startsWith("3:30:32 PM ") && fullWay.size() >= longWay.size(), qPrintable(fullWay));
    }

    void onTheTwentyFourHourClockThereIsNoHalfOfTheDay()
    {
        QCOMPARE(written(afternoon, None, Short, true, american), "15:30");
        QCOMPARE(written(afternoon, None, Medium, true, american), "15:30:32");
        // The hour has two figures there, and one on the twelve hour clock.
        QCOMPARE(written(morning, None, Short, true, american), "09:04");
        QCOMPARE(written(morning, None, Short, false, american), "9:04 AM");
        QCOMPARE(written(morning, None, Medium, false, american), "9:04:07 AM");
    }

    void aDateIsWrittenTheFiveWays()
    {
        QCOMPARE(written(afternoon, None, None, false, american), "");
        QCOMPARE(written(afternoon, Short, None, false, american), "11/23/37");
        QCOMPARE(written(afternoon, Medium, None, false, american), "Nov 23, 1937");
        QCOMPARE(written(afternoon, Long, None, false, american), "November 23, 1937");
        QCOMPARE(written(afternoon, Full, None, false, american), "Tuesday, November 23, 1937");
        QCOMPARE(written(morning, Medium, None, false, american), "Mar 5, 2026");
    }

    void theOrderAndTheWordsAreThePlacesOwn()
    {
        // Britain puts the day before the month, and so does this.
        const QLocale british(QLocale::English, QLocale::UnitedKingdom);
        QCOMPARE(written(afternoon, Long, None, false, british), "23 November 1937");
        QCOMPARE(written(afternoon, Medium, None, false, british), "23 Nov 1937");
        QVERIFY(written(afternoon, Full, None, false, british).startsWith("Tuesday"));
        QVERIFY(written(afternoon, Full, None, false, british).endsWith("23 November 1937"));
        // And Germany has its own words.
        const QLocale german(QLocale::German, QLocale::Germany);
        QVERIFY2(written(afternoon, Full, None, false, german).contains("Dienstag"), qPrintable(written(afternoon, Full, None, false, german)));
        QVERIFY(written(afternoon, Long, None, false, german).contains("November"));
        QVERIFY(!written(afternoon, Long, None, false, german).contains("Dienstag"));
    }

    void aDateAndATimeHaveSomethingBetweenThem()
    {
        QCOMPARE(written(afternoon, Short, Short, false, american), "11/23/37, 3:30 PM");
        QCOMPARE(written(afternoon, Medium, Short, false, american), "Nov 23, 1937 at 3:30 PM");
        QCOMPARE(written(afternoon, Full, Medium, true, american), "Tuesday, November 23, 1937 at 15:30:32");
    }

    void aPatternHasTheClockWhereItSaysAndElseIsTheClockAlone()
    {
        QCOMPARE(linked(afternoon, None, Short, true, "${clock}", american), "15:30");
        QCOMPARE(linked(afternoon, None, Short, true, "It is ${clock} now", american), "It is 15:30 now");
        QCOMPARE(linked(afternoon, None, Short, true, "", american), "15:30");
        QCOMPARE(linked(afternoon, None, Short, true, "no place for it", american), "15:30");
    }

    void aWayThatIsNoneOfTheFiveIsNothing()
    {
        QCOMPARE(written(afternoon, 9, -3, false, american), "");
        QCOMPARE(written(afternoon, None, 7, false, american), "");
    }
};

QTEST_APPLESS_MAIN(TestClockText)
#include "tst_clocktext.moc"
