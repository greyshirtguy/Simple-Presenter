// A song read from the lyrics file SongSelect gives out (src/songselect.h): the title,
// the parts under their labels, a part with no label, the details at the foot (authors,
// song number, year and publishers), what is not a part (a label with nothing under it,
// the footer's lines), line endings of any kind, and which lines are labels. The words
// here are made up: a song's own are its owner's.

#include "songselect.h"

#include <QtTest>

using namespace songselect;

namespace {

const QString kFile = QStringLiteral(
    "Morning Song\n"
    "\n"
    "Verse 1\n"
    "The first line of the first verse\n"
    "The second line of it\n"
    "The third line of it\n"
    "The fourth line of it\n"
    "\n"
    "Chorus\n"
    "The chorus starts here\n"
    "The chorus goes on\n"
    "The chorus ends\n"
    "\n"
    "Interlude 1\n"
    "Singing on\n"
    "\n"
    "Bridge\n"
    "A bridge line\n"
    "Another bridge line\n"
    "\n"
    "Nobody Inparticular, Another Nobody\n"
    "CCLI Song #1234567\n"
    "\u00a9 2001 Example Music; Sample Songs Publishing\n"
    "For use solely with the SongSelect\u00ae Terms of Use.  All rights reserved. www.ccli.com\n"
    "CCLI License #99999");

QString names(const Song &song)
{
    QStringList parts;
    for (const chords::Section &section : song.sections)
        parts.append((section.name.isEmpty() ? QStringLiteral("(none)") : section.name) + u':' + QString::number(section.lines.size()));
    return parts.join(u' ');
}

}

class TestSongSelect : public QObject
{
    Q_OBJECT

private slots:
    void theFileAsSongSelectWritesIt()
    {
        const Song song = parse(kFile);
        QCOMPARE(song.title, QStringLiteral("Morning Song"));
        QCOMPARE(names(song), QStringLiteral("Verse 1:4 Chorus:3 Interlude 1:1 Bridge:2"));
        QCOMPARE(song.sections[0].lines[0].text, QStringLiteral("The first line of the first verse"));
        QCOMPARE(song.sections[0].lines[3].text, QStringLiteral("The fourth line of it"));
        QVERIFY(song.sections[0].lines[0].chords.isEmpty());
        QCOMPARE(song.sections[2].lines[0].text, QStringLiteral("Singing on"));
        // The details, as ProPresenter keeps a song's from SongSelect: the authors on one
        // line, the publishers one a line, the year by itself, the number; the church's
        // own licence number not among them
        QCOMPARE(song.author, QStringLiteral("Nobody Inparticular, Another Nobody"));
        QCOMPARE(song.songNumber, QStringLiteral("1234567"));
        QCOMPARE(song.year, 2001);
        QCOMPARE(song.publisher, QStringLiteral("Example Music\nSample Songs Publishing"));
    }

    void windowsLineEndingsAndAMarkAtTheStart()
    {
        QString text = kFile;
        text.replace(u'\n', QStringLiteral("\r\n"));
        const Song song = parse(QChar(0xFEFF) + text);
        QCOMPARE(song.title, QStringLiteral("Morning Song"));
        QCOMPARE(names(song), QStringLiteral("Verse 1:4 Chorus:3 Interlude 1:1 Bridge:2"));
        QCOMPARE(song.songNumber, QStringLiteral("1234567"));
        // Old Mac endings too, and spaces round the lines, and a line of spaces as a blank one
        const Song spaced = parse(QStringLiteral("  A Title  \r\r  Verse 2 \r Only line \r   \r Chorus\r Line one\rLine two"));
        QCOMPARE(spaced.title, QStringLiteral("A Title"));
        QCOMPARE(names(spaced), QStringLiteral("Verse 2:1 Chorus:2"));
        QCOMPARE(spaced.sections[0].lines[0].text, QStringLiteral("Only line"));
    }

    void wordsCopiedWithNoLabelsAndNoFooter()
    {
        // Pasted words may have none of the labels and nothing at the foot: every block is
        // a part with no name, and there are no details
        const Song song = parse(QStringLiteral("Evening Song\n\nLine one\nLine two\n\nLine three\nLine four\nLine five\n"));
        QCOMPARE(song.title, QStringLiteral("Evening Song"));
        QCOMPARE(names(song), QStringLiteral("(none):2 (none):3"));
        QVERIFY(song.author.isEmpty());
        QVERIFY(song.songNumber.isEmpty());
        QVERIFY(song.publisher.isEmpty());
        QCOMPARE(song.year, 0);
        // A block with a label and one without, mixed
        QCOMPARE(names(parse(QStringLiteral("T\n\nLine\n\nChorus\nLine\nLine\n\nLine\n"))), QStringLiteral("(none):1 Chorus:2 (none):1"));
    }

    void theTitleIsTheFirstLineWhateverFollowsIt()
    {
        // No blank line after the title: the rest of its block goes on as a block
        const Song song = parse(QStringLiteral("Title\nVerse 1\nLine\n\nChorus\nLine\n"));
        QCOMPARE(song.title, QStringLiteral("Title"));
        QCOMPARE(names(song), QStringLiteral("Verse 1:1 Chorus:1"));
        // Blank lines before the title do not count
        QCOMPARE(parse(QStringLiteral("\n\n  \nTitle\n\nLine\n")).title, QStringLiteral("Title"));
    }

    void nothingToMakeASongOf()
    {
        QVERIFY(parse(QString()).sections.isEmpty());
        QVERIFY(parse(QString()).title.isEmpty());
        QVERIFY(parse(QStringLiteral("\n  \n\n")).sections.isEmpty());
        // A title alone, or a title and a footer, has no words
        const Song bare = parse(QStringLiteral("Title\n\nCCLI Song #7\n"));
        QCOMPARE(bare.title, QStringLiteral("Title"));
        QVERIFY(bare.sections.isEmpty());
        QCOMPARE(bare.songNumber, QStringLiteral("7"));
        // A label with nothing under it is no part
        QCOMPARE(names(parse(QStringLiteral("Title\n\nInstrumental\n\nVerse 1\nLine\n"))), QStringLiteral("Verse 1:1"));
    }

    void theFooterWithoutAnAuthorLineOrAYear()
    {
        const Song song = parse(QStringLiteral("Title\n\nVerse\nLine\n\nCCLI Song # 42\n\u00a9 Example Music\nCCLI Licence #1\n"));
        QCOMPARE(names(song), QStringLiteral("Verse:1"));
        QVERIFY(song.author.isEmpty());
        QCOMPARE(song.songNumber, QStringLiteral("42"));
        QCOMPARE(song.year, 0);
        QCOMPARE(song.publisher, QStringLiteral("Example Music"));
        // "(c)" and "Copyright" do as well as the sign, and the footer's lines may be
        // blocks of their own; what is after the number that is not known is left alone
        const Song spelt = parse(QStringLiteral("Title\n\nLine\n\nSomeone\nCCLI Song #5\nSomething else\n\nCopyright 1999 One; Two; ; Three\n"));
        QCOMPARE(names(spelt), QStringLiteral("(none):1"));
        QCOMPARE(spelt.author, QStringLiteral("Someone"));
        QCOMPARE(spelt.year, 1999);
        QCOMPARE(spelt.publisher, QStringLiteral("One\nTwo\nThree"));
        // A last block that starts as a footer does, with no song number, is a footer too
        const Song unnumbered = parse(QStringLiteral("Title\n\nLine\n\n(c) 2020 Someone's Music\n"));
        QCOMPARE(names(unnumbered), QStringLiteral("(none):1"));
        QCOMPARE(unnumbered.year, 2020);
        QCOMPARE(unnumbered.publisher, QStringLiteral("Someone's Music"));
    }

    void whichLinesAreLabels()
    {
        for (const QString &label : {QStringLiteral("Verse"), QStringLiteral("Verse 1"), QStringLiteral("verse 12"), QStringLiteral("Chorus 1a"),
                                     QStringLiteral("CHORUS"), QStringLiteral("Pre-Chorus"), QStringLiteral("Pre-Chorus 2"), QStringLiteral("Interlude 1"),
                                     QStringLiteral("Bridge"), QStringLiteral("Tag"), QStringLiteral("Ending"), QStringLiteral("Intro"),
                                     QStringLiteral("Outro"), QStringLiteral("Vamp"), QStringLiteral("Misc"), QStringLiteral("Verse A"),
                                     QStringLiteral("Verse1"), QStringLiteral(" Refrain ")})
            QVERIFY2(isLabel(label), qPrintable(label));
        for (const QString &words : {QStringLiteral("Singing on"), QStringLiteral("Verse 1 of the song"), QStringLiteral("The bridge is long"),
                                     QStringLiteral("Chorus:"), QStringLiteral("Verse 1 (x2)"), QStringLiteral(""), QStringLiteral("Tag along")})
            QVERIFY2(!isLabel(words), qPrintable(words));
    }
};

QTEST_APPLESS_MAIN(TestSongSelect)
#include "tst_songselect.moc"
