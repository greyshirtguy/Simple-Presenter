// How a passage is flowed across slides (src/bibleflow.h), on made-up verses: no
// Bible's words are in this repository. A slide here holds so many characters (each
// word and a space after it), which stands in for laying the words out in a theme's
// text box.

#include "bibleflow.h"

#include <QTest>

using namespace bibleflow;

namespace {

// A passage of made-up verses
QList<bible::PassageVerse> verses(const QStringList &texts)
{
    QList<bible::PassageVerse> all;
    for (int i = 0; i < texts.size(); ++i)
        all.append({1, 1, {i + 1, texts.at(i), {}}});
    return all;
}

// So many characters to a slide
Fits room(const QList<Word> &words, int characters)
{
    return [words, characters](int from, int to) {
        int length = 0;
        for (int i = from; i < to; ++i)
            length += int(words.at(i).text.size()) + 1;
        return length <= characters;
    };
}

// The slides as their words, a line each
QStringList slidesOf(const QList<Word> &words, const QList<Slide> &slides)
{
    QStringList all;
    for (const Slide &slide : slides) {
        QStringList line;
        for (int i = slide.from; i < slide.to; ++i)
            line << words.at(i).text;
        all << line.join(u' ');
    }
    return all;
}

QStringList flowed(const QStringList &texts, int characters, Options options = {})
{
    const QList<Word> words = wordsOf(verses(texts));
    return slidesOf(words, flow(words, options, room(words, characters)));
}

}

class TestBibleFlow : public QObject
{
    Q_OBJECT

private slots:
    void theWordsOfAPassage()
    {
        bible::PassageVerse red {1, 1, {1, QStringLiteral("He said, come here now."), {{9, 14}}}};
        const QList<Word> words = wordsOf({red, {1, 1, {2, QStringLiteral("  And  so. "), {}}}});
        QCOMPARE(words.size(), 7);
        QVERIFY(words[0].first && !words[1].first && words[5].first);
        QVERIFY(!words[1].red && words[2].red && words[3].red && words[4].red && !words[5].red);
        QCOMPARE(words[5].verse, 1);
        QCOMPARE(words[6].text, "so.");
    }

    void whatABreakCosts()
    {
        const QList<Word> words = wordsOf(verses({QStringLiteral("One two, three; four. Five"), QStringLiteral("six. Seven “eight.” Nine")}));
        // (after "One", "two,", "three;", "four.", "Five" (verse end), "six.", "Seven", "eight.”", "Nine")
        QCOMPARE(breakCost(words, 0), 20);
        QCOMPARE(breakCost(words, 1), 6);
        QCOMPARE(breakCost(words, 2), 4);
        QCOMPARE(breakCost(words, 3), 1);
        QCOMPARE(breakCost(words, 4), 3);
        QCOMPARE(breakCost(words, 5), 1);
        QCOMPARE(breakCost(words, 7), 1);
        QCOMPARE(breakCost(words, 8), 0);
        const QList<Word> ends = wordsOf(verses({QStringLiteral("A sentence ends."), QStringLiteral("And another;"), QStringLiteral("and more,"), QStringLiteral("then more.")}));
        QCOMPARE(breakCost(ends, 2), 0); // a sentence and a verse
        QCOMPARE(breakCost(ends, 4), 2); // a verse, after a semicolon
        QCOMPARE(breakCost(ends, 6), 3); // a verse, after a comma
    }

    void allThatFitsIsOneSlide()
    {
        QCOMPARE(flowed({QStringLiteral("Short and done.")}, 100), QStringList({"Short and done."}));
    }

    void noOrphanAtTheEnd()
    {
        // Filled, the last slide has a word or two
        const QStringList texts {QStringLiteral("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm nnnn oooo pppp qqqq rrrr ssss tttt uuuu vvvv")};
        const QStringList filled = flowed(texts, 50, {Mode::Fill, false});
        QCOMPARE(filled.size(), 3);
        QCOMPARE(filled.last(), "uuuu vvvv");
        // Even: as many slides, as long as each other
        const QStringList even = flowed(texts, 50);
        QCOMPARE(even.size(), 3);
        for (const QString &slide : even)
            QVERIFY2(slide.split(u' ').size() >= 7 && slide.split(u' ').size() <= 8, qPrintable(slide));
    }

    void aSentenceEndNearTheMiddleIsWhereItBreaks()
    {
        // Two slides' worth: the full stop is a word off the middle, and wins over it
        const QStringList even = flowed({QStringLiteral("aaaa bbbb cccc dddd eeee ffff gggg. hhhh iiii jjjj kkkk llll mmmm nnnn oooo")}, 60);
        QCOMPARE(even, QStringList({"aaaa bbbb cccc dddd eeee ffff gggg.", "hhhh iiii jjjj kkkk llll mmmm nnnn oooo"}));
    }

    void aCommaNearTheMiddleWinsOverAFullStopFarFromIt()
    {
        const QStringList even = flowed({QStringLiteral("aaaa bbbb cccc dddd eeee ffff, gggg hhhh iiii jjjj kkkk llll. mmmm")}, 50);
        QCOMPARE(even, QStringList({"aaaa bbbb cccc dddd eeee ffff,", "gggg hhhh iiii jjjj kkkk llll. mmmm"}));
    }

    void aPhraseLongerThanASlideBreaksBetweenWordsAndLeavesNoScrap()
    {
        // No mark at all: it breaks near the middle, and never leaves a word or two
        // before or after the full stop at its end
        const QStringList even = flowed({QStringLiteral("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii jjjj kkkk llll mmmm. Nnnn oooo pppp")}, 50);
        QCOMPARE(even.size(), 2);
        QVERIFY2(!even.last().startsWith(u"mmmm."), qPrintable(even.join(" | ")));
        QVERIFY2(even.first().split(u' ').size() >= 6, qPrintable(even.join(" | ")));
    }

    void neverMoreSlidesThanFillMakes()
    {
        QStringList texts;
        for (int v = 0; v < 12; ++v)
            texts << QStringLiteral("This is a made up verse number %1, with a pause; and an end.").arg(v + 1);
        for (int room : {40, 70, 120, 200}) {
            const QStringList filled = flowed(texts, room, {Mode::Fill, false});
            const QStringList even = flowed(texts, room);
            QCOMPARE(even.size(), filled.size());
            // Every word, once, in order
            QCOMPARE(even.join(u' '), filled.join(u' '));
        }
    }

    void aWordTooLongForTheBoxHasASlideToItself()
    {
        const QStringList even = flowed({QStringLiteral("aaaa bbbb averyveryverylongword cccc dddd")}, 12);
        QVERIFY(even.contains("averyveryverylongword"));
        QCOMPARE(even.join(u' '), "aaaa bbbb averyveryverylongword cccc dddd");
    }

    void breakOnNewVerse()
    {
        const QStringList texts {QStringLiteral("One verse."), QStringLiteral("aaaa bbbb cccc dddd, eeee ffff gggg hhhh iiii jjjj kkkk llll."), QStringLiteral("Last.")};
        const QStringList each = flowed(texts, 45, {Mode::Even, true});
        // Every verse starts a slide; the long one is two, broken at its comma
        QCOMPARE(each, QStringList({"One verse.", "aaaa bbbb cccc dddd,", "eeee ffff gggg hhhh iiii jjjj kkkk llll.", "Last."}));
        // Filled, the same verses are broken where the box is full
        const QStringList filled = flowed(texts, 45, {Mode::Fill, true});
        QCOMPARE(filled, QStringList({"One verse.", "aaaa bbbb cccc dddd, eeee ffff gggg hhhh", "iiii jjjj kkkk llll.", "Last."}));
        // And where what follows the comma would not fit, it breaks between two words
        // instead, near even, leaving none of them by itself
        const QStringList tight = flowed(texts, 40, {Mode::Even, true});
        QCOMPARE(tight[1], "aaaa bbbb cccc dddd, eeee ffff gggg");
    }

    // A passage grown round slides that are to stay: they stay as they were, and only the
    // words before and after them are flowed, each by itself
    void slidesKeptAsTheyWere()
    {
        const QStringList texts {QStringLiteral("aaaa bbbb cccc dddd eeee ffff."), QStringLiteral("gggg hhhh iiii jjjj, kkkk llll mmmm nnnn."),
                                 QStringLiteral("oooo pppp qqqq."), QStringLiteral("rrrr ssss, tttt uuuu vvvv wwww xxxx.")};
        const QList<Word> words = wordsOf(verses(texts));
        const Fits fits = room(words, 45);
        // As the first two verses were flowed, the second of their slides kept: the last
        // two verses, which "Next Verse" added, flowed after it by themselves
        const QList<Word> firstTwo = wordsOf(verses(texts.mid(0, 2)));
        const QList<Slide> before = flow(firstTwo, {Mode::Even, false}, room(firstTwo, 45));
        QVERIFY(before.size() >= 2);
        const QList<Slide> grown = flow(words, {Mode::Even, false}, fits, {before.mid(0, 2)});
        QCOMPARE(grown.mid(0, 2), before.mid(0, 2));
        QCOMPARE(grown.last().to, int(words.size()));
        for (qsizetype i = 1; i < grown.size(); ++i)
            QCOMPARE(grown.at(i).from, grown.at(i - 1).to);
        // What came after the kept slides is what the same words make flowed by themselves
        const QList<Word> rest = words.mid(before.at(1).to);
        QCOMPARE(grown.size() - 2, flow(rest, {Mode::Even, false}, room(rest, 45)).size());

        // Kept in the middle: the words either side flowed each by itself
        const QList<Slide> middle {{6, 10}, {10, 14}};
        const QList<Slide> round = flow(words, {Mode::Even, false}, fits, middle);
        QVERIFY(round.contains(middle.at(0)) && round.contains(middle.at(1)));
        QCOMPARE(round.first().from, 0);
        QCOMPARE(round.last().to, int(words.size()));
        for (qsizetype i = 1; i < round.size(); ++i)
            QCOMPARE(round.at(i).from, round.at(i - 1).to);

        // Slides that are not of these words, or leave a gap, keep nothing
        QCOMPARE(flow(words, {Mode::Even, false}, fits, {{0, 3}, {4, 6}}), flow(words, {Mode::Even, false}, fits));
        QCOMPARE(flow(words, {Mode::Even, false}, fits, {{0, int(words.size()) + 1}}), flow(words, {Mode::Even, false}, fits));
    }
};

QTEST_APPLESS_MAIN(TestBibleFlow)
#include "tst_bibleflow.moc"
