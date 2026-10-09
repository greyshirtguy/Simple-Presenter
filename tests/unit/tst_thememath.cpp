// Which text box of a slide goes into which text box of a theme (src/thememath.h).

#include "thememath.h"

#include <QTest>

using namespace thememath;

namespace {

Box box(const QString &name, double width, double height)
{
    return Box {name, width, height};
}

}

class TestThemeMath : public QObject
{
    Q_OBJECT

private slots:
    void oneBoxGoesIntoTheOther()
    {
        QCOMPARE(match({box("", 1600, 900)}, {box("Lyrics", 1800, 300)}), QList<int> {0});
        QCOMPARE(match({}, {box("Lyrics", 1800, 300)}), QList<int> {-1});
        QVERIFY(match({box("Text", 100, 100)}, {}).isEmpty());
    }

    void boxesAreMatchedByNameWhateverTheirOrderOrCapitals()
    {
        const QList<Box> slide {box("Verse", 1600, 700), box("reference", 1600, 100)};
        const QList<Box> theme {box("Reference", 600, 80), box("VERSE", 1700, 500)};
        QCOMPARE(match(slide, theme), (QList<int> {1, 0}));
    }

    void boxesWithoutNamesInCommonAreMatchedBySize()
    {
        // The big box of words and the small one, the other way round in the theme
        const QList<Box> slide {box("Text", 1600, 700), box("Caption", 1600, 100)};
        const QList<Box> theme {box("Small Print", 600, 80), box("Main", 1700, 500)};
        QCOMPARE(match(slide, theme), (QList<int> {1, 0}));
    }

    void boxesMuchOfASizeAreMatchedInOrder()
    {
        const QList<Box> slide {box("", 900, 400), box("", 905, 400)};
        const QList<Box> theme {box("Left", 800, 300), box("Right", 810, 300)};
        QCOMPARE(match(slide, theme), (QList<int> {0, 1}));
    }

    void aNameIsUsedOnceAndTheRestGoBySize()
    {
        const QList<Box> slide {box("Verse", 1600, 300), box("Verse", 1600, 600), box("Note", 400, 100)};
        const QList<Box> theme {box("Footer", 500, 90), box("Verse", 1500, 500), box("Verse 2", 1500, 480)};
        // The first "Verse" by name; of the others the bigger into the bigger
        QCOMPARE(match(slide, theme), (QList<int> {2, 0, 1}));
    }

    void whatIsLeftOverIsMatchedWithNothing()
    {
        // More boxes in the theme than the slide has
        QCOMPARE(match({box("Text", 1600, 700)}, {box("Verse", 1700, 500), box("Reference", 600, 80)}), (QList<int> {0, -1}));
        // And fewer: the slide's second box goes nowhere
        QCOMPARE(match({box("A", 1600, 700), box("B", 600, 100)}, {box("Lyrics", 1700, 500)}), QList<int> {0});
        // A name that is there takes its box, and leaves the bigger one out
        QCOMPARE(match({box("Big", 1600, 700), box("Reference", 600, 100)}, {box("Reference", 500, 80)}), QList<int> {1});
    }
};

QTEST_APPLESS_MAIN(TestThemeMath)
#include "tst_thememath.moc"
