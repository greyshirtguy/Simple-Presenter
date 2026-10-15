// A text box's lines made into one with each run's look kept (src/linejoin.h), as the
// Single Line and Replace Line Returns transforms draw a text box's own text. On texts
// made here: a verse's raised number first, red words, two paragraphs and a line break.

#include "linejoin.h"

#include <QTest>

namespace {

TextRun run(const QString &text, int superscript = 0, QColor fill = Qt::white)
{
    TextRun made;
    made.text = text;
    made.family = QStringLiteral("Made Up Sans");
    made.size = 85;
    made.superscript = superscript;
    made.fill = fill;
    return made;
}

TextParagraph paragraph(const QList<TextRun> &runs, Qt::Alignment alignment = Qt::AlignHCenter)
{
    TextParagraph made;
    made.alignment = alignment;
    made.runs = runs;
    return made;
}

QString words(const RichText &text)
{
    QStringList all;
    for (const TextParagraph &p : text.paragraphs) {
        QString line;
        for (const TextRun &r : p.runs)
            line += r.text;
        all << line;
    }
    return all.join(u'|');
}

} // namespace

class TestLineJoin : public QObject
{
    Q_OBJECT

private slots:
    // The case that was drawn wrongly: a raised number first, the reference a paragraph
    // of its own. Each run keeps its look; the number alone is raised.
    void aRaisedNumberFirstStaysTheNumbersOwn()
    {
        RichText text;
        text.paragraphs = {paragraph({run(QStringLiteral("16 "), 1), run(QStringLiteral("Made up words, ")),
                                      run(QStringLiteral("said in red"), 0, QColor(0xff, 0x5a, 0x4f)), run(QStringLiteral(", for a test."))}),
                           paragraph({run(QStringLiteral("John 3:16 MUV"))})};
        const RichText one = joinedLines(text, QStringLiteral(" "));
        QCOMPARE(one.paragraphs.size(), 1);
        QCOMPARE(words(one), QStringLiteral("16 Made up words, said in red, for a test. John 3:16 MUV"));
        const QList<TextRun> &runs = one.paragraphs.first().runs;
        QCOMPARE(runs.first().superscript, 1);
        QCOMPARE(runs.first().text, QStringLiteral("16 "));
        for (qsizetype i = 1; i < runs.size(); ++i)
            QCOMPARE(runs.at(i).superscript, 0);
        QVERIFY(std::any_of(runs.begin(), runs.end(), [](const TextRun &r) { return r.text == QStringLiteral("said in red") && r.fill == QColor(0xff, 0x5a, 0x4f); }));
    }

    // Replace Line Returns: the delimiter between lines, in the look of the run before it;
    // a line break within a paragraph counts as a line's end too
    void theDelimiterBetweenLines()
    {
        RichText text;
        text.paragraphs = {paragraph({run(QStringLiteral("First line") + QChar(QChar::LineSeparator) + QStringLiteral("second line"))}),
                           paragraph({run(QStringLiteral("red third"), 0, Qt::red)})};
        const RichText one = joinedLines(text, QStringLiteral("  •  "));
        QCOMPARE(words(one), QStringLiteral("First line  •  second line  •  red third"));
        const QList<TextRun> &runs = one.paragraphs.first().runs;
        QCOMPARE(runs.last().fill, QColor(Qt::red));
        QCOMPARE(runs.at(runs.size() - 2).text, QStringLiteral("  •  "));
        QCOMPARE(runs.at(runs.size() - 2).fill, QColor(Qt::white));
    }

    // Lines are trimmed, through runs that are only space, and empty ones left out, as the
    // app has always done with the words alone
    void trimmedAndEmptyLinesLeftOut()
    {
        RichText text;
        text.paragraphs = {paragraph({run(QStringLiteral("  ")), run(QStringLiteral(" one "))}),
                           paragraph({run(QStringLiteral("   "))}),
                           paragraph({}),
                           paragraph({run(QStringLiteral("two"), 1), run(QStringLiteral("  "))})};
        const RichText one = joinedLines(text, QStringLiteral(" "));
        QCOMPARE(words(one), QStringLiteral("one two"));
    }

    // The first paragraph's alignment and spacing; and a text with nothing in it is one
    // empty run in its first run's look
    void alignmentAndNothingAtAll()
    {
        RichText text;
        TextParagraph left = paragraph({run(QStringLiteral("a"))}, Qt::AlignLeft);
        left.lineHeight = 300;
        left.lineHeightIsMultiple = true;
        text.paragraphs = {left, paragraph({run(QStringLiteral("b"))}, Qt::AlignRight)};
        const RichText one = joinedLines(text, QStringLiteral(" "));
        QCOMPARE(one.paragraphs.first().alignment, Qt::Alignment(Qt::AlignLeft));
        QCOMPARE(one.paragraphs.first().lineHeight, 300);
        QVERIFY(one.paragraphs.first().lineHeightIsMultiple);

        RichText blank;
        blank.paragraphs = {paragraph({run(QStringLiteral(" "), 1)})};
        const RichText none = joinedLines(blank, QStringLiteral(" "));
        QCOMPARE(none.paragraphs.size(), 1);
        QCOMPARE(none.paragraphs.first().runs.size(), 1);
        QVERIFY(none.paragraphs.first().runs.first().text.isEmpty());
        QCOMPARE(none.paragraphs.first().runs.first().size, 85.0);
    }
};

QTEST_GUILESS_MAIN(TestLineJoin)
#include "tst_linejoin.moc"
