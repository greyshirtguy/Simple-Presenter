// Formats put on the text of many slides at once (src/textformat.h): what each run is
// given, the size's arrows stepping each run's own size and never below a point, a face
// by its style's name, an outline turned on at three hundredths of a run's size (and by
// its colour alone); the paragraphs' alignment; the box's part, a shadow's colour
// turning it on; and what many runs and boxes have in common, a key dropped for good at
// the first that differs.

#include "textformat.h"

#include <QColor>
#include <QTest>

using namespace textformat;

namespace {

QVariantMap run(double size, double strokeWidth = 0, const QString &family = QStringLiteral("Helvetica"), bool bold = false)
{
    return {{"family", family}, {"size", size}, {"bold", bold}, {"italic", false}, {"color", QColor(Qt::white)},
            {"capitalization", 0}, {"strokeWidth", strokeWidth}, {"strokeColor", QColor(Qt::black)}, {"alignment", int(Qt::AlignHCenter)}};
}

}

class TestTextFormat : public QObject
{
    Q_OBJECT

private slots:
    void theSizeArrowsStepEachRunsOwnSize()
    {
        QCOMPARE(forRun({{"sizeBy", 1}}, run(72)), (QVariantMap {{"size", 73.0}}));
        QCOMPARE(forRun({{"sizeBy", -1}}, run(36)), (QVariantMap {{"size", 35.0}}));
        // A size typed is the same for every run
        QCOMPARE(forRun({{"size", 48}}, run(72)), (QVariantMap {{"size", 48}}));
    }

    void aFaceByItsStyle()
    {
        QCOMPARE(styles(), (QStringList {"Regular", "Italic", "Bold", "Bold Italic"}));
        QCOMPARE(forRun({{"style", "Bold Italic"}}, run(40)), (QVariantMap {{"bold", true}, {"italic", true}}));
        // (only what differs: the run was not italic)
        QCOMPARE(forRun({{"style", "Regular"}}, run(40, 0, "Helvetica", true)), (QVariantMap {{"bold", false}}));
        QCOMPARE(styleOf(true, false), QStringLiteral("Bold"));
        QCOMPARE(forRun({{"family", "Arial"}}, run(40)), (QVariantMap {{"family", "Arial"}}));
    }

    void whatARunHasItIsNotGivenAgain()
    {
        QCOMPARE(forRun({{"family", "Helvetica"}}, run(40)), QVariantMap {});
        QCOMPARE(forRun({{"size", 40}}, run(40)), QVariantMap {});
        QCOMPARE(forRun({{"color", QColor(Qt::white)}, {"capitalization", 0}, {"style", "Regular"}}, run(40)), QVariantMap {});
        QCOMPARE(forRun({{"strokeOn", false}}, run(40)), QVariantMap {});
        QCOMPARE(forRun({{"sizeBy", -1}}, run(1)), QVariantMap {});
    }

    void anOutlineTurnedOnAndOff()
    {
        // On: three hundredths of the run's size, at least a point, for a run without one
        QCOMPARE(forRun({{"strokeOn", true}}, run(100)), (QVariantMap {{"strokeWidth", 3.0}}));
        QCOMPARE(forRun({{"strokeOn", true}}, run(20)), (QVariantMap {{"strokeWidth", 1.0}}));
        // a run with one keeps it
        QCOMPARE(forRun({{"strokeOn", true}}, run(100, 5)), QVariantMap {});
        // Off, for every run
        QCOMPARE(forRun({{"strokeOn", false}}, run(100, 5)), (QVariantMap {{"strokeWidth", 0.0}}));
        // A colour alone gives an outline to a run without one, as the editor's does
        QCOMPARE(forRun({{"strokeColor", QColor(Qt::red)}}, run(100)), (QVariantMap {{"strokeColor", QColor(Qt::red)}, {"strokeWidth", 3.0}}));
        QCOMPARE(forRun({{"strokeColor", QColor(Qt::red)}}, run(100, 4)), (QVariantMap {{"strokeColor", QColor(Qt::red)}}));
        // and a width with it is the width
        QCOMPARE(forRun({{"strokeOn", true}, {"strokeWidth", 6}}, run(100)), (QVariantMap {{"strokeWidth", 6}}));
        // The size and the outline together: the outline by the new size
        QCOMPARE(forRun({{"sizeBy", 100}, {"strokeOn", true}}, run(100)), (QVariantMap {{"size", 200.0}, {"strokeWidth", 6.0}}));
        // The width's arrows step each outline's own width, never below half a point, and
        // give none to a run without
        QCOMPARE(forRun({{"strokeWidthBy", 1}}, run(100, 4)), (QVariantMap {{"strokeWidth", 5.0}}));
        QCOMPARE(forRun({{"strokeWidthBy", -1}}, run(100, 1)), (QVariantMap {{"strokeWidth", 0.5}}));
        QCOMPARE(forRun({{"strokeWidthBy", 1}}, run(100)), QVariantMap {});
    }

    void paragraphsAndBoxes()
    {
        QCOMPARE(alignmentOf({{"alignment", int(Qt::AlignRight)}}), int(Qt::AlignRight));
        QCOMPARE(alignmentOf({{"size", 3}}), -1);
        QCOMPARE(forRun({{"alignment", int(Qt::AlignRight)}}, run(40)), QVariantMap {});
        QColor drawn(0, 0, 0);
        drawn.setAlphaF(0.5);
        const QVariantMap box {{"verticalAlignment", int(Qt::AlignTop)}, {"textShadowEnabled", false}, {"textShadowColor", drawn},
                               {"textShadowRadius", 4.0}, {"textShadowAngle", 315.0}, {"textShadowOffset", 5.0}};
        QCOMPARE(forBox({{"verticalAlignment", int(Qt::AlignBottom)}, {"size", 40}}, box), (QVariantMap {{"verticalAlignment", int(Qt::AlignBottom)}}));
        // A shadow's colour turns it on; its opacity is its own
        QCOMPARE(forBox({{"textShadowSolidColor", QColor(Qt::blue)}}, box),
                 (QVariantMap {{"textShadowSolidColor", QColor(Qt::blue)}, {"textShadowEnabled", true}}));
        QCOMPARE(forBox({{"textShadowSolidColor", QColor(Qt::blue)}, {"textShadowEnabled", false}}, box),
                 (QVariantMap {{"textShadowSolidColor", QColor(Qt::blue)}}));
        QCOMPARE(forBox({{"textShadowOpacity", 0.75}}, box), (QVariantMap {{"textShadowOpacity", 0.75}}));
        QCOMPARE(forBox({{"sizeBy", 1}}, box), QVariantMap {});
        // What the box has, it is not given again: a shadow turned off where it is off,
        // which would leave an empty one in the file where there was none
        QCOMPARE(forBox({{"textShadowEnabled", false}, {"verticalAlignment", int(Qt::AlignTop)}, {"textShadowOpacity", 0.5},
                         {"textShadowSolidColor", QColor(Qt::black)}, {"textShadowRadius", 4.0}}, box), QVariantMap {});
        QCOMPARE(forBox({{"textShadowEnabled", false}}, QVariantMap {}), QVariantMap {});
    }

    void theShadowsArrowsStepEachBoxsOwn()
    {
        QColor drawn(0, 0, 0);
        drawn.setAlphaF(0.5);
        const QVariantMap box {{"textShadowEnabled", false}, {"textShadowColor", drawn}, {"textShadowRadius", 4.0},
                               {"textShadowAngle", 359.0}, {"textShadowOffset", 0.0}};
        QCOMPARE(forBox({{"textShadowRadiusBy", 1}}, box), (QVariantMap {{"textShadowRadius", 5.0}}));
        QCOMPARE(forBox({{"textShadowOpacityBy", 0.01}}, box), (QVariantMap {{"textShadowOpacity", 0.51}}));
        // The angle turns round; the offset stops at nothing; neither turns the shadow on
        QCOMPARE(forBox({{"textShadowAngleBy", 1}}, box), (QVariantMap {{"textShadowAngle", 0.0}}));
        QCOMPARE(forBox({{"textShadowOffsetBy", -1}}, box), QVariantMap {});
        QCOMPARE(forBox({{"textShadowOffsetBy", 1}}, box), (QVariantMap {{"textShadowOffset", 1.0}}));
        // The opacity is kept from 0 to 1
        QColor full(0, 0, 0);
        QCOMPARE(forBox({{"textShadowOpacityBy", 0.01}}, {{"textShadowColor", full}}), QVariantMap {});
        // A number typed as well wins
        QCOMPARE(forBox({{"textShadowRadiusBy", 1}, {"textShadowRadius", 9}}, box), (QVariantMap {{"textShadowRadius", 9}}));
    }

    void whatManyHaveInCommon()
    {
        Common common;
        common.addRun(run(72));
        common.addRun(run(40));
        QColor shadow(10, 20, 30);
        shadow.setAlphaF(0.75);
        common.addBox({{"verticalAlignment", int(Qt::AlignVCenter)}, {"textShadowEnabled", true}, {"textShadowColor", shadow},
                       {"textShadowRadius", 4.0}, {"textShadowAngle", 315.0}, {"textShadowOffset", 5.0}});
        common.addRun(run(72));
        common.addBox({{"verticalAlignment", int(Qt::AlignVCenter)}, {"textShadowEnabled", true}, {"textShadowColor", shadow},
                       {"textShadowRadius", 8.0}, {"textShadowAngle", 315.0}, {"textShadowOffset", 5.0}});
        const QVariantMap values = common.values();
        // The sizes differ: no size (and it stays gone, though the last run was 72 again)
        QVERIFY(!values.contains("size"));
        QCOMPARE(values.value("family").toString(), QStringLiteral("Helvetica"));
        QCOMPARE(values.value("style").toString(), QStringLiteral("Regular"));
        QCOMPARE(values.value("alignment").toInt(), int(Qt::AlignHCenter));
        QCOMPARE(values.value("strokeOn").toBool(), false);
        // (No run has an outline: no width or colour of one)
        QVERIFY(!values.contains("strokeWidth") && !values.contains("strokeColor"));
        QCOMPARE(values.value("verticalAlignment").toInt(), int(Qt::AlignVCenter));
        // The shadow's colour without its opacity, and the opacity apart
        QCOMPARE(values.value("shadowColor").value<QColor>(), QColor(10, 20, 30));
        QCOMPARE(values.value("shadowOpacity").toDouble(), 0.75);
        QVERIFY(!values.contains("shadowBlur"));
        QCOMPARE(values.value("shadowAngle").toDouble(), 315.0);
        QCOMPARE(values.value("boxes").toInt(), 2);
    }

    void anOutlinesWidthFromTheRunsWithOne()
    {
        Common common;
        common.addRun(run(72, 3));
        common.addRun(run(40, 3));
        QCOMPARE(common.values().value("strokeOn").toBool(), true);
        QCOMPARE(common.values().value("strokeWidth").toDouble(), 3.0);
        // One without: on and off differ, the width stays that of those with one
        common.addRun(run(40, 0));
        QVERIFY(!common.values().contains("strokeOn"));
        QCOMPARE(common.values().value("strokeWidth").toDouble(), 3.0);
        QCOMPARE(Common().values(), (QVariantMap {{"boxes", 0}}));
    }
};

QTEST_GUILESS_MAIN(TestTextFormat)
#include "tst_textformat.moc"
