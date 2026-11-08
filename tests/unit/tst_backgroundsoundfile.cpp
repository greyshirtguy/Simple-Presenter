// Backgrounds played with their sound (src/backgroundsoundfile.h): a file with two slides and keys this app does not
// know, read; a slide put in, put in again and taken out, with the rest of the file kept, the keys it does not know
// included; one that says too little, or the same slide twice, left out; a file that is not one said to be so; and
// where the file is.

#include "backgroundsoundfile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace backgroundsoundfile;

namespace {

// (Written out as plain strings, as tst_disabledatefile's is: written as a raw string literal, moc made nothing of a test.)
const QByteArray file = "{"
    "\"about\": \"kept as it is\","
    "\"kept\": { \"by\": \"someone else\" },"
    "\"played\": ["
    "  { \"presentation\": \"P1\", \"path\": \"Libraries/Demo/News.pro\", \"name\": \"News\", \"slide\": \"S1\","
    "    \"note\": \"not this app's\" },"
    "  { \"presentation\": \"P2\", \"path\": \"Libraries/Demo/Notices.pro\", \"name\": \"Notices\", \"slide\": \"S9\" },"
    "  { \"presentation\": \"P2\", \"slide\": \"S9\" },"
    "  { \"presentation\": \"P3\", \"slide\": \"\" }"
    "]}";

QJsonObject parsed(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).object();
}

} // namespace

class TestBackgroundSoundFile : public QObject
{
    Q_OBJECT

private slots:
    void readsTheSlidesItCanKnow()
    {
        QString error;
        const QList<Entry> entries = read(file, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).presentation, QStringLiteral("P1"));
        QCOMPARE(entries.at(0).path, QStringLiteral("Libraries/Demo/News.pro"));
        QCOMPARE(entries.at(0).name, QStringLiteral("News"));
        QCOMPARE(entries.at(0).slide, QStringLiteral("S1"));
        QCOMPARE(entries.at(1).slide, QStringLiteral("S9"));
        QVERIFY(has(entries, "P1", "S1"));
        QVERIFY(!has(entries, "P1", "S9"));
        QVERIFY(read(QByteArray(), &error).isEmpty() && error.isEmpty());
        QVERIFY(read("not a file of slides", &error).isEmpty() && !error.isEmpty());
    }

    void writesThemWithTheRestOfTheFileKept()
    {
        QString error;
        QList<Entry> entries = read(file, &error);
        // A slide put in, the same one put in again (one entry, its path as now), and one taken out
        entries = with(entries, {"P4", "Libraries/Demo/Week.pro", "Week", "S2"});
        entries = with(entries, {"P4", "Libraries/Other/Week.pro", "Week", "S2"});
        entries = without(entries, "P2", "S9");
        entries = without(entries, "P5", "S5");
        QCOMPARE(entries.size(), 2);
        const QJsonObject written = parsed(write(file, entries));
        QCOMPARE(written.value("kept").toObject().value("by").toString(), QStringLiteral("someone else"));
        QVERIFY(written.value("about").toString().contains("ProPresenter does not use this file"));
        const QJsonArray played = written.value("played").toArray();
        QCOMPARE(played.size(), 2);
        QCOMPARE(played.at(0).toObject().value("note").toString(), QStringLiteral("not this app's"));
        QCOMPARE(played.at(1).toObject().value("path").toString(), QStringLiteral("Libraries/Other/Week.pro"));
        QCOMPARE(read(write(file, entries), &error).size(), 2);
        // Into no file at all
        const QJsonObject made = parsed(write(QByteArray(), {{"P1", "x.pro", "x", "S1"}}));
        QCOMPARE(made.value("played").toArray().size(), 1);
        QVERIFY(!made.value("about").toString().isEmpty());
        // Every slide taken out leaves a file that says none
        QVERIFY(read(write(file, {}), &error).isEmpty() && error.isEmpty());
    }

    void isKeptInTheWorkspacesOwnFolder()
    {
        QCOMPARE(pathIn("/w/Church"), QStringLiteral("/w/Church/SimplePresenter/Background Sound.json"));
    }
};

QTEST_GUILESS_MAIN(TestBackgroundSoundFile)
#include "tst_backgroundsoundfile.moc"
