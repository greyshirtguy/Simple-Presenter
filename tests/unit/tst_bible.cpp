// A Bible, its books and references to passages of it (src/bible.h), and Bibles read
// from the four kinds of XML file (src/biblexml.h). Every verse here is made up for the
// test: no Bible's words are in this repository.

#include "bible.h"
#include "biblexml.h"

#include <QTest>

using namespace bible;

namespace {

Reference ref(int book, QList<Range> ranges)
{
    Reference r;
    r.book = book;
    r.ranges = ranges;
    return r;
}

// A small made-up Bible: Genesis 1 (3 verses) and 2 (2), John 3 (4, from 15), Jude 1 (3)
Bible madeUp()
{
    Bible b;
    b.name = QStringLiteral("Test Bible");
    b.books = {
        {1, QStringLiteral("Genesis"), {{1, {{1, QStringLiteral("First of one."), {}}, {2, QStringLiteral("Second of one."), {}}, {3, QStringLiteral("Third of one."), {}}}},
                                        {2, {{1, QStringLiteral("First of two."), {}}, {2, QStringLiteral("Second of two."), {}}}}}},
        {43, QStringLiteral("John"), {{3, {{15, QStringLiteral("Fifteen."), {}}, {16, QStringLiteral("Sixteen."), {}}, {17, QStringLiteral("Seventeen."), {}}, {18, QStringLiteral("Eighteen."), {}}}}}},
        {65, QStringLiteral("Jude"), {{1, {{1, QStringLiteral("One."), {}}, {2, QStringLiteral("Two."), {}}, {3, QStringLiteral("Three."), {}}}}}},
    };
    return b;
}

QStringList numbers(const QList<PassageVerse> &verses)
{
    QStringList all;
    for (const PassageVerse &v : verses)
        all << QString::number(v.chapter) + u':' + QString::number(v.verse.number);
    return all;
}

}

class TestBible : public QObject
{
    Q_OBJECT

private slots:
    void theBooksByNameAndByShortForm()
    {
        QCOMPARE(bookNumber("Genesis"), 1);
        QCOMPARE(bookNumber("gen"), 1);
        QCOMPARE(bookNumber("Matt"), 40);
        QCOMPARE(bookNumber("Mt"), 40);
        QCOMPARE(bookNumber("1 Cor"), 46);
        QCOMPARE(bookNumber("1Co"), 46);
        QCOMPARE(bookNumber("I Corinthians"), 46);
        QCOMPARE(bookNumber("First John"), 62);
        QCOMPARE(bookNumber("3 jn"), 64);
        QCOMPARE(bookNumber("Ps"), 19);
        QCOMPARE(bookNumber("Psalm"), 19);
        QCOMPARE(bookNumber("Song of Solomon"), 22);
        QCOMPARE(bookNumber("Phil"), 50);
        QCOMPARE(bookNumber("Philem"), 57);
        QCOMPARE(bookNumber("Jud"), 65);
        QCOMPARE(bookNumber("Judg"), 7);
        QCOMPARE(bookNumber("Rev."), 66);
        QCOMPARE(bookNumber("JHN"), 43);
        // The start of exactly one name; and none of two
        QCOMPARE(bookNumber("Deuter"), 5);
        QCOMPARE(bookNumber("Jo"), 0);
        QCOMPARE(bookNumber("Nothing"), 0);
        // A Bible's own names first
        QCOMPARE(bookNumber("Johannes", {{43, QStringLiteral("Johannes"), {}}}), 43);
        QCOMPARE(usfmId(43), "JHN");
        QCOMPARE(osisId(19), "Ps");
        QVERIFY(singleChapter(65) && !singleChapter(43));
        QCOMPARE(chapterCount(19), 150);
        QCOMPARE(chapterCount(43), 21);
        int all = 0;
        for (int book = 1; book <= bookCount; ++book) {
            all += chapterCount(book);
            QCOMPARE(chapterCount(book) == 1, singleChapter(book));
        }
        QCOMPARE(all, 1189); // the chapters of the 66, as is usually counted
    }

    void referencesAsPeopleTypeThem()
    {
        QCOMPARE(parse("John 3:16"), ref(43, {{3, 16, 3, 16}}));
        QCOMPARE(parse("John 3.16"), ref(43, {{3, 16, 3, 16}}));
        QCOMPARE(parse("john 3 16"), ref(43, {{3, 16, 3, 16}}));
        QCOMPARE(parse("Jn 3:16-18"), ref(43, {{3, 16, 3, 18}}));
        QCOMPARE(parse("John 3:16–18"), ref(43, {{3, 16, 3, 18}}));
        QCOMPARE(parse("John 3:16 - 18"), ref(43, {{3, 16, 3, 18}}));
        // ProPresenter's own example: "Matt 1 2 3" is Matthew 1:2-3
        QCOMPARE(parse("Matt 1 2 3"), ref(40, {{1, 2, 1, 3}}));
        QCOMPARE(parse("Psalm 23"), ref(19, {{23, 0, 23, 0}}));
        QCOMPARE(parse("Gen 1:1-2:3"), ref(1, {{1, 1, 2, 3}}));
        QCOMPARE(parse("Ruth 1-2"), ref(8, {{1, 0, 2, 0}}));
        QCOMPARE(parse("1 Cor 13:4-7"), ref(46, {{13, 4, 13, 7}}));
        QCOMPARE(parse("John 3:16, 18, 20-21"), ref(43, {{3, 16, 3, 16}, {3, 18, 3, 18}, {3, 20, 3, 21}}));
        QCOMPARE(parse("John 3:16; 4:2"), ref(43, {{3, 16, 3, 16}, {4, 2, 4, 2}}));
        // A book of one chapter: a number alone is a verse
        QCOMPARE(parse("Jude 3"), ref(65, {{1, 3, 1, 3}}));
        QCOMPARE(parse("Jude 3-5"), ref(65, {{1, 3, 1, 5}}));
        // A book alone is its first chapter
        QCOMPARE(parse("Mark"), ref(41, {{1, 0, 1, 0}}));
        // What is no passage
        QVERIFY(!parse("").valid());
        QVERIFY(!parse("3:16").valid());
        QVERIFY(!parse("Nothing 3:16").valid());
        QVERIFY(!parse("John 3:18-16").valid());
        QVERIFY(!parse("John 0:1").valid());
    }

    void referencesWrittenOut()
    {
        QCOMPARE(written(ref(43, {{3, 16, 3, 18}})), "John 3:16–18");
        QCOMPARE(written(ref(43, {{3, 16, 3, 16}})), "John 3:16");
        QCOMPARE(written(ref(19, {{23, 0, 23, 0}})), "Psalm 23");
        QCOMPARE(written(ref(19, {{23, 0, 24, 0}})), "Psalms 23–24");
        QCOMPARE(written(ref(1, {{1, 31, 2, 1}})), "Genesis 1:31–2:1");
        QCOMPARE(written(ref(43, {{3, 16, 3, 16}, {3, 18, 3, 18}})), "John 3:16, 18");
        QCOMPARE(written(ref(43, {{3, 16, 3, 16}, {4, 2, 4, 2}})), "John 3:16; 4:2");
        QCOMPARE(written(ref(65, {{1, 3, 1, 5}})), "Jude 3–5");
        QCOMPARE(written(ref(43, {{3, 16, 3, 16}}), "Johannes"), "Johannes 3:16");
    }

    void aPassageOfABible()
    {
        const Bible b = madeUp();
        QString error;
        QCOMPARE(numbers(passage(b, parse("Gen 1"), &error)), QStringList({"1:1", "1:2", "1:3"}));
        QCOMPARE(numbers(passage(b, parse("Gen 1:2-2:1"), &error)), QStringList({"1:2", "1:3", "2:1"}));
        // An end past the end of the chapter is its end
        QCOMPARE(numbers(passage(b, parse("John 3:17-40"), &error)), QStringList({"3:17", "3:18"}));
        QCOMPARE(numbers(passage(b, parse("Jude 2"), &error)), QStringList({"1:2"}));
        QVERIFY(passage(b, parse("Exodus 1"), &error).isEmpty() && error.contains("Exodus"));
        QVERIFY(passage(b, parse("John 9"), &error).isEmpty() && !error.isEmpty());
        // Each slide's own reference
        QCOMPARE(written(referenceTo(passage(b, parse("Gen 1:3-2:2"), &error))), "Genesis 1:3–2:2");
        QCOMPARE(written(referenceTo(passage(b, parse("John 3:16, 18"), &error))), "John 3:16, 18");
    }

    void theNextAndThePreviousVerse()
    {
        const Bible b = madeUp();
        QString error;
        QCOMPARE(nextVerse(b, passage(b, parse("Gen 1:2"), &error)), ref(1, {{1, 3, 1, 3}}));
        // Across a chapter, and across a book
        QCOMPARE(nextVerse(b, passage(b, parse("Gen 1:3"), &error)), ref(1, {{2, 1, 2, 1}}));
        QCOMPARE(nextVerse(b, passage(b, parse("Gen 2:2"), &error)), ref(43, {{3, 15, 3, 15}}));
        QCOMPARE(previousVerse(b, passage(b, parse("John 3:15"), &error)), ref(1, {{2, 2, 2, 2}}));
        QCOMPARE(previousVerse(b, passage(b, parse("Gen 2:1"), &error)), ref(1, {{1, 3, 1, 3}}));
        // Nothing before the first, or after the last
        QVERIFY(!previousVerse(b, passage(b, parse("Gen 1:1"), &error)).valid());
        QVERIFY(!nextVerse(b, passage(b, parse("Jude 3"), &error)).valid());
    }

    void aZefaniaFile()
    {
        const QByteArray xml =
            "<?xml version=\"1.0\"?>\n<XMLBIBLE biblename=\"Made Up Version\">\n"
            "<INFORMATION><title>Ignored when the name is given</title><identifier>MUV</identifier><rights>Made up for a test.</rights></INFORMATION>\n"
            "<BIBLEBOOK bnumber=\"43\" bname=\"John\" bsname=\"Jn\"><CHAPTER cnumber=\"3\">\n"
            "<VERS vnumber=\"16\">So   it\n is <STYLE css=\"color:#ff0000\">said in red</STYLE> here.<NOTE>a note</NOTE></VERS>\n"
            "<VERS vnumber=\"17\">And <gr str=\"1\">then</gr> more.</VERS>\n"
            "</CHAPTER></BIBLEBOOK></XMLBIBLE>";
        QCOMPARE(biblexml::detect(xml), biblexml::Format::Zefania);
        Bible b;
        QString error;
        QVERIFY2(biblexml::read(xml, "file", &b, &error), qPrintable(error));
        QCOMPARE(b.name, "Made Up Version");
        QCOMPARE(b.abbreviation, "MUV");
        QCOMPARE(b.copyright, "Made up for a test.");
        const QList<PassageVerse> verses = passage(b, parse("John 3:16-17"), &error);
        QCOMPARE(verses.size(), 2);
        QCOMPARE(verses[0].verse.text, "So it is said in red here.");
        QCOMPARE(verses[0].verse.red, (QList<QPair<int, int>> {{9, 11}}));
        QCOMPARE(verses[0].verse.text.mid(9, 11), "said in red");
        QCOMPARE(verses[1].verse.text, "And then more.");
    }

    void anOsisFileWithVersesHeldAndAsMilestones()
    {
        const QByteArray held =
            "<osis><osisText osisIDWork=\"MUV\"><header><work osisWork=\"MUV\"><title>Made Up</title><rights>None.</rights></work></header>"
            "<div type=\"book\" osisID=\"Gen\"><chapter osisID=\"Gen.1\"><title>A heading</title>"
            "<verse osisID=\"Gen.1.1\">Words <note>left out</note>of one.</verse>"
            "<verse osisID=\"Gen.1.2\">He said, <q who=\"Jesus\">come here.</q></verse></chapter></div></osisText></osis>";
        QCOMPARE(biblexml::detect(held), biblexml::Format::Osis);
        Bible b;
        QString error;
        QVERIFY2(biblexml::read(held, "file", &b, &error), qPrintable(error));
        QCOMPARE(b.name, "Made Up");
        QCOMPARE(b.abbreviation, "MUV");
        QList<PassageVerse> verses = passage(b, parse("Gen 1"), &error);
        QCOMPARE(verses.size(), 2);
        QCOMPARE(verses[0].verse.text, "Words of one.");
        QCOMPARE(verses[1].verse.text, "He said, come here.");
        QCOMPARE(verses[1].verse.text.mid(verses[1].verse.red.value(0).first, verses[1].verse.red.value(0).second), "come here.");

        const QByteArray milestones =
            "<osis><osisText osisIDWork=\"MUV\"><div type=\"book\" osisID=\"John\"><chapter sID=\"John.3\" osisID=\"John.3\"/>"
            "<verse sID=\"John.3.16\" osisID=\"John.3.16\"/>First words<lb/>go on.<verse eID=\"John.3.16\"/>"
            "<verse sID=\"John.3.17\" osisID=\"John.3.17\"/>Second <title>not this</title>words.<verse eID=\"John.3.17\"/>"
            "<chapter eID=\"John.3\"/></div></osisText></osis>";
        QVERIFY2(biblexml::read(milestones, "Fallback", &b, &error), qPrintable(error));
        QCOMPARE(b.name, "Fallback");
        verses = passage(b, parse("John 3"), &error);
        QCOMPARE(numbers(verses), QStringList({"3:16", "3:17"}));
        QCOMPARE(verses[0].verse.text, "First words go on.");
        QCOMPARE(verses[1].verse.text, "Second words.");
    }

    void bebliaAndOpenSongFiles()
    {
        const QByteArray beblia =
            "<bible translation=\"Made Up Beblia\"><testament name=\"New\"><book number=\"43\"><chapter number=\"1\">"
            "<verse number=\"1\">One.</verse><verse number=\"2\">Two.</verse></chapter></book></testament></bible>";
        QCOMPARE(biblexml::detect(beblia), biblexml::Format::Beblia);
        Bible b;
        QString error;
        QVERIFY2(biblexml::read(beblia, "file", &b, &error), qPrintable(error));
        QCOMPARE(b.name, "Made Up Beblia");
        QCOMPARE(b.books.value(0).name, "John");
        QCOMPARE(numbers(passage(b, parse("John 1"), &error)), QStringList({"1:1", "1:2"}));

        const QByteArray openSong =
            "<bible><b n=\"Genesis\"><c n=\"1\"><v n=\"1\">Eins.</v></c></b><b n=\"Exodus\"><c n=\"1\"><v n=\"1\">Zwei.</v></c></b></bible>";
        QCOMPARE(biblexml::detect(openSong), biblexml::Format::OpenSong);
        QVERIFY2(biblexml::read(openSong, "Made Up OpenSong", &b, &error), qPrintable(error));
        QCOMPARE(b.name, "Made Up OpenSong");
        QCOMPARE(passage(b, parse("Exodus 1:1"), &error).value(0).verse.text, "Zwei.");
        // Books named in another language are taken in the order of the 66
        const QByteArray other = "<bible><b n=\"Erstes Buch\"><c n=\"1\"><v n=\"1\">Eins.</v></c></b><b n=\"Zweites Buch\"><c n=\"1\"><v n=\"1\">Zwei.</v></c></b></bible>";
        QVERIFY2(biblexml::read(other, "Other", &b, &error), qPrintable(error));
        QCOMPARE(passage(b, parse("Exodus 1:1"), &error).value(0).verse.text, "Zwei.");
        QCOMPARE(passage(b, parse("Zweites 1:1", b.books), &error).value(0).verse.text, "Zwei.");
    }

    void whatIsNoBible()
    {
        Bible b;
        QString error;
        QVERIFY(!biblexml::read("<songs><song/></songs>", "x", &b, &error) && error.contains("Zefania"));
        QVERIFY(!biblexml::read("<XMLBIBLE><BIBLEBOOK bnumber=\"1\"></BIBLEBOOK></XMLBIBLE>", "x", &b, &error) && error.contains("no verses"));
        QVERIFY(!biblexml::read("<XMLBIBLE><BIBLEBOOK bnumber=\"1\"><CHAPTER cnumber=\"1\"><VERS vnumber=\"1\">Broken", "x", &b, &error));
    }
};

QTEST_APPLESS_MAIN(TestBible)
#include "tst_bible.moc"
