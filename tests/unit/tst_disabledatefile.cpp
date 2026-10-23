// Disable dates (src/disabledatefile.h): a file with two dates and keys this app does not know, read; a date set,
// changed and taken out, with the rest of the file kept, the keys it does not know included; one that says too little
// left out; a file that is not one said to be so; where the file is; and which dates have come and which is next.

#include "disabledatefile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace disabledatefile;

namespace {

// (Written out as plain strings: written as a raw string literal, it had moc make nothing of the test, which did not link.)
const QByteArray file = "{"
    "\"about\": \"kept as it is\","
    "\"kept\": { \"by\": \"someone else\" },"
    "\"dates\": ["
    "  { \"presentation\": \"P1\", \"path\": \"Libraries/Demo/Welcome.pro\", \"name\": \"Welcome\", \"slide\": \"S1\","
    "    \"at\": \"2026-10-30T09:00:00+11:00\", \"note\": \"not this app's\" },"
    "  { \"presentation\": \"P2\", \"path\": \"Libraries/Demo/Notices.pro\", \"name\": \"Notices\", \"slide\": \"S9\","
    "    \"at\": \"2026-11-02T00:00:00Z\" },"
    "  { \"presentation\": \"P3\", \"slide\": \"\", \"at\": \"2026-11-02T00:00:00Z\" }"
    "]}";

const QTimeZone sydney("Australia/Sydney");

long long at(const char *iso)
{
    return QDateTime::fromString(QString::fromLatin1(iso), Qt::ISODate).toSecsSinceEpoch();
}

QJsonObject parsed(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).object();
}

} // namespace

class TestDisableDateFile : public QObject
{
    Q_OBJECT

private slots:
    void readsTheDatesItCanCarryOut()
    {
        QString error;
        const QList<Entry> entries = read(file, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).presentation, QStringLiteral("P1"));
        QCOMPARE(entries.at(0).path, QStringLiteral("Libraries/Demo/Welcome.pro"));
        QCOMPARE(entries.at(0).slide, QStringLiteral("S1"));
        QCOMPARE(entries.at(0).at, at("2026-10-29T22:00:00Z"));
        QCOMPARE(entries.at(1).at, at("2026-11-02T00:00:00Z"));
        QVERIFY(read(QByteArray(), &error).isEmpty() && error.isEmpty());
        QVERIFY(read("not a file of dates", &error).isEmpty() && !error.isEmpty());
    }

    void writesTheDatesAndKeepsTheRest()
    {
        QString error;
        QList<Entry> entries = read(file, &error);
        // Changed, added, and one taken out
        Entry changed = entries.at(0);
        changed.at = at("2026-10-31T09:30:00+11:00");
        entries = with(entries, changed);
        Entry added { QStringLiteral("P4"), QStringLiteral("Libraries/Demo/New.pro"), QStringLiteral("New"), QStringLiteral("S4"),
                      at("2026-12-25T00:00:00+11:00") };
        entries = with(entries, added);
        entries = without(entries, QStringLiteral("P2"), QStringLiteral("S9"));
        QCOMPARE(entries.size(), 2);
        const QJsonObject written = parsed(write(file, entries, sydney));
        QCOMPARE(written.value("about").toString(), QStringLiteral("kept as it is"));
        QCOMPARE(written.value("kept").toObject().value("by").toString(), QStringLiteral("someone else"));
        const QJsonArray dates = written.value("dates").toArray();
        QCOMPARE(dates.size(), 2);
        QCOMPARE(dates.at(0).toObject().value("note").toString(), QStringLiteral("not this app's"));
        QCOMPARE(dates.at(0).toObject().value("at").toString(), QStringLiteral("2026-10-31T09:30:00+11:00"));
        QCOMPARE(dates.at(1).toObject().value("presentation").toString(), QStringLiteral("P4"));
        QCOMPARE(dates.at(1).toObject().value("at").toString(), QStringLiteral("2026-12-25T00:00:00+11:00"));
        // Read again, it is what was written
        const QList<Entry> again = read(write(file, entries, sydney), &error);
        QCOMPARE(again.size(), 2);
        QCOMPARE(again.at(0).at, changed.at);
        QCOMPARE(again.at(1).name, QStringLiteral("New"));
        // A new file says what it is
        const QJsonObject fresh = parsed(write(QByteArray(), {added}, sydney));
        QVERIFY(fresh.value("about").toString().contains("Simple Presenter"));
        QCOMPARE(fresh.value("dates").toArray().size(), 1);
    }

    void whereTheFileIs()
    {
        QCOMPARE(pathIn(QStringLiteral("/home/x/WorkSpaces/Church")), QStringLiteral("/home/x/WorkSpaces/Church/SimplePresenter/Disable Dates.json"));
    }

    void whichHaveComeAndWhichIsNext()
    {
        QString error;
        const QList<Entry> entries = read(file, &error);
        QCOMPARE(dateOf(entries, QStringLiteral("P1"), QStringLiteral("S1")), at("2026-10-29T22:00:00Z"));
        QCOMPARE(dateOf(entries, QStringLiteral("P1"), QStringLiteral("S9")), 0LL);
        QVERIFY(due(entries, at("2026-10-29T21:59:59Z")).isEmpty());
        QCOMPARE(due(entries, at("2026-10-29T22:00:00Z")).size(), 1);
        const QList<Entry> both = due(entries, at("2026-12-01T00:00:00Z"));
        QCOMPARE(both.size(), 2);
        QVERIFY(both.at(0).at < both.at(1).at);
        QCOMPARE(next(entries, at("2026-10-01T00:00:00Z")), at("2026-10-29T22:00:00Z"));
        QCOMPARE(next(entries, at("2026-10-29T22:00:00Z")), at("2026-11-02T00:00:00Z"));
        QCOMPARE(next(entries, at("2026-11-02T00:00:00Z")), -1LL);
    }
};

QTEST_GUILESS_MAIN(TestDisableDateFile)
#include "tst_disabledatefile.moc"
