// Finding presentations by their names and their words (src/searchindex.h), in lists
// made up here.

#include "searchindex.h"

#include <QTest>

using namespace searchindex;

namespace {

Entry song(const QString &name, const QStringList &slides)
{
    Entry entry;
    entry.path = "/library/" + name + ".pro";
    entry.name = name;
    entry.library = "Songs";
    entry.slides = slides;
    prepare(&entry);
    return entry;
}

QList<Entry> library()
{
    return {
        song("Purple Teapots", {"Purple teapots marching slowly\nDown the hill we're going", "Sing it once again"}),
        song("Marching Song", {"Left and right and left again", "Señor, the band is playing"}),
        song("A Quiet One", {"Nothing much to say"}),
        song("Teapot Blues", {"I've got the blues"}),
    };
}

QStringList names(const QList<Entry> &entries, const QList<Hit> &hits)
{
    QStringList found;
    for (const Hit &hit : hits)
        found << entries.at(hit.entry).name;
    return found;
}

}

class TestSearchIndex : public QObject
{
    Q_OBJECT

private slots:
    void textIsMatchedWithoutCapitalsPunctuationOrAccents()
    {
        QCOMPARE(folded("We're  GOING, down!"), "were going down");
        QCOMPARE(folded(QString::fromUtf8("Señor — the band")), "senor the band");
        QCOMPARE(folded("  ...  "), "");
        QCOMPARE(folded(QString::fromUtf8("It’s 4 o'clock")), "its 4 oclock");
    }

    void nothingTypedFindsNothing()
    {
        const QList<Entry> entries = library();
        QVERIFY(find(entries, "", 10).isEmpty());
        QVERIFY(find(entries, "  ,. ", 10).isEmpty());
    }

    void namesComeBeforeWordsAndEachInTheOrderOfTheAlphabet()
    {
        const QList<Entry> entries = library();
        // "marching" is in one name and in the words of another
        const QList<Hit> hits = find(entries, "marching", 10);
        QCOMPARE(names(entries, hits), (QStringList {"Marching Song", "Purple Teapots"}));
        QVERIFY(hits.at(0).byName);
        QVERIFY(!hits.at(1).byName);
        // Part of a word will do, in a name as in the words
        QCOMPARE(names(entries, find(entries, "teapot", 10)), (QStringList {"Purple Teapots", "Teapot Blues"}));
    }

    void everyWordHasToBeThereInAnyOrder()
    {
        const QList<Entry> entries = library();
        QCOMPARE(names(entries, find(entries, "slowly purple", 10)), QStringList {"Purple Teapots"});
        QCOMPARE(names(entries, find(entries, "again sing", 10)), QStringList {"Purple Teapots"});
        QVERIFY(find(entries, "purple elephants", 10).isEmpty());
        // Across slides too: a song is found by its words, wherever in it they are
        QCOMPARE(names(entries, find(entries, "left band", 10)), QStringList {"Marching Song"});
    }

    void aSongFoundByItsWordsComesWithTheLineThatHasThem()
    {
        const QList<Entry> entries = library();
        QCOMPARE(find(entries, "were going", 10).at(0).line, "Down the hill we're going");
        QCOMPARE(find(entries, "senor", 10).at(0).line, QString::fromUtf8("Señor, the band is playing"));
        // Not all on one line: the first line with the first word
        QCOMPARE(find(entries, "left band", 10).at(0).line, "Left and right and left again");
        // One found by its name has no need of a line
        QVERIFY(find(entries, "quiet", 10).at(0).line.isEmpty());
    }

    void noMoreAreGivenThanAskedFor()
    {
        QList<Entry> entries;
        for (int i = 0; i < 50; ++i)
            entries << song(QStringLiteral("Song %1").arg(i, 2, 10, QChar(u'0')), {"la la la"});
        const QList<Hit> hits = find(entries, "song", 7);
        QCOMPARE(hits.size(), 7);
        QCOMPARE(entries.at(hits.at(0).entry).name, "Song 00");
        QCOMPARE(find(entries, "la", 100).size(), 50);
    }
};

QTEST_APPLESS_MAIN(TestSearchIndex)
#include "tst_searchindex.moc"
