// The reflow editor's rules for a slide's words (src/reflow.h): where a slide is split
// for a caret, a stretch of a text taken with its look, two texts put together, words
// typed over, and where the chords go through each. On texts made here: two paragraphs,
// the first in two looks.

#include "reflow.h"

#include <QTest>

using namespace reflow;

namespace {

TextRun run(const QString &text, const QString &font, qreal size)
{
    TextRun made;
    made.text = text;
    made.fontName = font;
    made.family = font;
    made.size = size;
    return made;
}

TextParagraph paragraph(const QList<TextRun> &runs, Qt::Alignment alignment = Qt::AlignHCenter)
{
    TextParagraph made;
    made.alignment = alignment;
    made.runs = runs;
    return made;
}

// "Bright morning" with the second word bigger, then "over the hill" on a line of its
// own, set to the left.
RichText text()
{
    RichText made;
    made.paragraphs = {paragraph({run("Bright ", "Plain", 60), run("morning", "Big", 90)}),
                       paragraph({run("over the hill", "Plain", 60)}, Qt::AlignLeft)};
    return made;
}

// A text as the look of its words, to hold what comes back to:
// "Plain60:Bright |Big90:morning / Plain60:over the hill<". Neighbours that look alike
// are said as one stretch, however many runs they are in: the rules say how each
// character looks, not how the runs are divided (two alike side by side are written to
// the file as one).
QString shape(const RichText &rich)
{
    QStringList lines;
    for (const TextParagraph &line : rich.paragraphs) {
        QStringList stretches;
        QString look;
        for (const TextRun &part : line.runs) {
            const QString its = QStringLiteral("%1%2").arg(part.fontName).arg(part.size);
            if (its == look && !stretches.isEmpty())
                stretches.last() += part.text;
            else
                stretches << its + u':' + part.text;
            look = its;
        }
        lines << stretches.join('|') + (line.alignment == Qt::AlignLeft ? QStringLiteral("<") : QString());
    }
    return lines.join(QStringLiteral(" / "));
}

QString where(const QList<chords::Chord> &chords)
{
    QStringList all;
    for (const chords::Chord &chord : chords)
        all << QStringLiteral("%1@%2").arg(chord.name).arg(chord.at);
    return all.join(' ');
}

}

class TestReflow : public QObject
{
    Q_OBJECT

private slots:
    void aTextIsCountedAsAnEditorCountsIt()
    {
        QCOMPARE(words(text()), "Bright morning\nover the hill");
        QCOMPARE(length(text()), 28);
        QCOMPARE(length(RichText()), 0);
    }

    void aSlideIsSplitWhereTheWordsStopAndStartAgain()
    {
        const QString all = "Bright morning\nover the hill";
        // Between two lines: the break goes to neither slide.
        Split split = splitAt(all, 14);
        QCOMPARE(split.leftEnd, 14);
        QCOMPARE(split.rightStart, 15);
        split = splitAt(all, 15);
        QCOMPARE(split.leftEnd, 14);
        QCOMPARE(split.rightStart, 15);
        // Between two words: the space goes to neither.
        split = splitAt(all, 6);
        QCOMPARE(split.leftEnd, 6);
        QCOMPARE(split.rightStart, 7);
        // In the middle of a word, the word is cut there.
        split = splitAt(all, 3);
        QCOMPARE(split.leftEnd, 3);
        QCOMPARE(split.rightStart, 3);
        // At the very start and at the very end, one of the two has nothing.
        split = splitAt(all, 0);
        QCOMPARE(split.leftEnd, 0);
        QCOMPARE(split.rightStart, 0);
        split = splitAt(all, 28);
        QCOMPARE(split.leftEnd, 28);
        QCOMPARE(split.rightStart, 28);
        // Several spaces and breaks together all go.
        split = splitAt("one  \n\n two", 5);
        QCOMPARE(split.leftEnd, 3);
        QCOMPARE(split.rightStart, 8);
        // A caret out of the text is at its nearer end.
        QCOMPARE(splitAt(all, 99).leftEnd, 28);
        QCOMPARE(splitAt(all, -5).rightStart, 0);
    }

    void aLineOfChordsAloneIsNotSpaceToBeDropped()
    {
        // Such a line is made of wide and zero-width spaces, which are its own.
        const QString all = QStringLiteral("words\n") + QChar(0x2001) + QChar(0x200B);
        const Split split = splitAt(all, 6);
        QCOMPARE(split.leftEnd, 5);
        QCOMPARE(split.rightStart, 6);
    }

    void aStretchIsTakenWithItsLook()
    {
        // Within one run, across two runs, and across the break between the lines
        QCOMPARE(shape(cut(text(), 0, 6)), "Plain60:Bright");
        QCOMPARE(shape(cut(text(), 3, 10)), "Plain60:ght |Big90:mor");
        QCOMPARE(shape(cut(text(), 7, 19)), "Big90:morning / Plain60:over<");
        // The whole of it is the whole of it.
        QCOMPARE(shape(cut(text(), 0, 28)), shape(text()));
        QCOMPARE(shape(cut(text(), -4, 99)), shape(text()));
        // From the second line on: only that line, set as it was.
        QCOMPARE(shape(cut(text(), 15, 28)), "Plain60:over the hill<");
        QCOMPARE(words(cut(text(), 15, 28)), "over the hill");
    }

    void aBreakTakenHasALineOnEitherSideOfIt()
    {
        QCOMPARE(words(cut(text(), 7, 15)), "morning\n");
        QCOMPARE(cut(text(), 7, 15).paragraphs.size(), 2);
        QCOMPARE(words(cut(text(), 14, 19)), "\nover");
        QCOMPARE(cut(text(), 14, 19).paragraphs.size(), 2);
        // The empty line is set as its own line was, and says how words typed on it look.
        QCOMPARE(shape(cut(text(), 7, 15)), "Big90:morning / Plain60:<");
    }

    void nothingTakenStillSaysHowWordsThereWouldLook()
    {
        // At the end of the first line: as the bigger word; in the second line: as that line
        RichText none = cut(text(), 14, 14);
        QCOMPARE(words(none), "");
        QCOMPARE(none.paragraphs.size(), 1);
        QCOMPARE(none.paragraphs.first().runs.size(), 1);
        QCOMPARE(none.paragraphs.first().runs.first().fontName, "Big");
        none = cut(text(), 20, 20);
        QCOMPARE(shape(none), "Plain60:<");
        // And of a text with nothing in it at all, nothing with no look.
        QCOMPARE(length(cut(RichText(), 0, 5)), 0);
    }

    void splitAndJoinedAgainIsWhatItWas()
    {
        // At the break between the two lines, the two parts put together are the text.
        const Split split = splitAt(words(text()), 14);
        const RichText first = cut(text(), 0, split.leftEnd);
        const RichText second = cut(text(), split.rightStart, length(text()));
        QCOMPARE(shape(first), "Plain60:Bright |Big90:morning");
        QCOMPARE(shape(second), "Plain60:over the hill<");
        QCOMPARE(shape(joined(first, second)), shape(text()));
    }

    void joinedBeginsTheSecondOnANewLine()
    {
        const RichText first = cut(text(), 0, 6);
        const RichText second = cut(text(), 7, 14);
        QCOMPARE(words(joined(first, second)), "Bright\nmorning");
        QCOMPARE(shape(joined(first, second)), "Plain60:Bright / Big90:morning");
        // One with no words gives the other alone, with no empty line.
        const RichText none = cut(text(), 14, 14);
        QCOMPARE(shape(joined(none, second)), shape(second));
        QCOMPARE(shape(joined(first, none)), shape(first));
        // Two with none: still one line, set as the first.
        QCOMPARE(shape(joined(none, cut(text(), 20, 20))), "Big90:");
    }

    void wordsTypedTakeTheLookOfWhatIsBeforeThem()
    {
        // A word corrected in the first run; the rest is as it was.
        QCOMPARE(shape(retyped(text(), "Light morning\nover the hill")), "Plain60:Light |Big90:morning / Plain60:over the hill<");
        // Letters added to the bigger word are bigger.
        QCOMPARE(shape(retyped(text(), "Bright mornings\nover the hill")), "Plain60:Bright |Big90:mornings / Plain60:over the hill<");
        // Nothing changed is the same text, run for run.
        QCOMPARE(shape(retyped(text(), words(text()))), shape(text()));
    }

    void aLineBreakTypedMakesALineSetAsItsLineWas()
    {
        // In the second line, which is set to the left
        const RichText broken = retyped(text(), "Bright morning\nover\nthe hill");
        QCOMPARE(words(broken), "Bright morning\nover\nthe hill");
        QCOMPARE(shape(broken), "Plain60:Bright |Big90:morning / Plain60:over< / Plain60:the hill<");
        // And a break taken out again puts the two lines on one, with what was typed.
        QCOMPARE(words(retyped(broken, "Bright morning\nover the hill")), "Bright morning\nover the hill");
        QCOMPARE(retyped(broken, "Bright morning\nover the hill").paragraphs.size(), 2);
    }

    void everythingTypedOverAndEverythingDeleted()
    {
        const RichText all = retyped(text(), "New words");
        QCOMPARE(words(all), "New words");
        QCOMPARE(all.paragraphs.first().runs.first().fontName, "Plain");
        const RichText none = retyped(text(), "");
        QCOMPARE(words(none), "");
        QCOMPARE(none.paragraphs.size(), 1);
        // Typed into again, a text with no words takes the look it kept.
        QCOMPARE(retyped(none, "Back").paragraphs.first().runs.first().fontName, "Plain");
        // And a text that never had any look takes what is typed all the same.
        QCOMPARE(words(retyped(RichText(), "Words")), "Words");
    }

    void theChordsOfASplitSlideStayOverTheirCharacters()
    {
        // One over each word of "Bright morning\nover the hill"
        const QList<chords::Chord> chords = {{0, "G"}, {7, "D"}, {15, "Em"}, {24, "C"}};
        Split split = splitAt("Bright morning\nover the hill", 14);
        QCOMPARE(where(chordsLeft(chords, split)), "G@0 D@7");
        QCOMPARE(where(chordsRight(chords, split)), "Em@0 C@9");
        // Split within the first line: the second slide has the rest of that line and the next.
        split = splitAt("Bright morning\nover the hill", 6);
        QCOMPARE(where(chordsLeft(chords, split)), "G@0");
        QCOMPARE(where(chordsRight(chords, split)), "D@0 Em@8 C@17");
    }

    void aChordOverASpaceThatGoesToNeitherSlideGoesToTheSecond()
    {
        // Over the space between the words; and not if the second begins with a chord already
        Split split = splitAt("Bright morning", 6);
        QCOMPARE(where(chordsRight({{6, "A"}}, split)), "A@0");
        QCOMPARE(where(chordsLeft({{6, "A"}}, split)), "");
        QCOMPARE(where(chordsRight({{6, "A"}, {7, "D"}}, split)), "D@0");
    }

    void theChordsOfTwoSlidesJoinedStandAsFarAlongAsTheirWords()
    {
        const QList<chords::Chord> first = {{0, "G"}, {7, "D"}};
        const QList<chords::Chord> second = {{0, "Em"}, {9, "C"}};
        // "Bright morning" is 14 characters; the second's words begin after the break.
        QCOMPARE(where(chordsJoined(first, 14, second, true)), "G@0 D@7 Em@15 C@24");
        // With no words before them they are where they were.
        QCOMPARE(where(chordsJoined({}, 0, second, false)), "Em@0 C@9");
    }

    void splitThenJoinedTheChordsAreWhereTheyWere()
    {
        const QString all = "Bright morning\nover the hill";
        const QList<chords::Chord> chords = {{0, "G"}, {7, "D"}, {15, "Em"}, {24, "C"}};
        for (int caret : {14, 15}) {
            const Split split = splitAt(all, caret);
            QCOMPARE(where(chordsJoined(chordsLeft(chords, split), split.leftEnd, chordsRight(chords, split), true)), where(chords));
        }
    }
};

QTEST_APPLESS_MAIN(TestReflow)
#include "tst_reflow.moc"
