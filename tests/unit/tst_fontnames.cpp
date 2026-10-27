// Font names (src/fontnames.h): Qt's name for a family it has from two foundries made
// the family's own again, and a list of them each once; what to ask Qt for to have a
// face of one foundry; style names compared whatever their spacing and case, and which
// bold and italic say by themselves; and the style part of a PostScript name. The names
// are those of the maintainer's CMG Sans, which Qt lists as "CMG Sans [ULA ]" (the
// foundry with its space) and "CMG Sans [PYRS]".

#include "fontnames.h"

#include <QTest>

using namespace fontnames;

class TestFontNames : public QObject
{
    Q_OBJECT

private slots:
    void aFamilyFromTwoFoundriesIsItsOwnName()
    {
        QCOMPARE(plain(QStringLiteral("CMG Sans [ULA ]")), QStringLiteral("CMG Sans"));
        QCOMPARE(plain(QStringLiteral("CMG Sans [PYRS]")), QStringLiteral("CMG Sans"));
        // Any other name as it is: one listed once, one with brackets of its own
        QCOMPARE(plain(QStringLiteral("CMG Sans Light")), QStringLiteral("CMG Sans Light"));
        QCOMPARE(plain(QStringLiteral("[Brackets] Sans")), QStringLiteral("[Brackets] Sans"));
        QCOMPARE(plain(QString()), QString());
    }

    void aMenuHasEachFamilyOnce()
    {
        QCOMPARE(plainFamilies({QStringLiteral("Arial"), QStringLiteral("CMG Sans [ULA ]"), QStringLiteral("CMG Sans [PYRS]"),
                                QStringLiteral("CMG Sans Light"), QStringLiteral("cmg sans [X]")}),
                 (QStringList {QStringLiteral("Arial"), QStringLiteral("CMG Sans"), QStringLiteral("CMG Sans Light")}));
    }

    void aFaceOfOneFoundryIsAskedForUnderItsName()
    {
        const QSet<QString> listed {QStringLiteral("CMG Sans [ULA ]"), QStringLiteral("CMG Sans [PYRS]"), QStringLiteral("Lato")};
        QCOMPARE(qtFamily(QStringLiteral("CMG Sans"), QStringLiteral("PYRS"), listed), QStringLiteral("CMG Sans [PYRS]"));
        QCOMPARE(qtFamily(QStringLiteral("CMG Sans"), QStringLiteral("ULA "), listed), QStringLiteral("CMG Sans [ULA ]"));
        // A family Qt lists by its name alone, or a face with no foundry, is asked for by it
        QCOMPARE(qtFamily(QStringLiteral("Lato"), QStringLiteral("tyPL"), listed), QStringLiteral("Lato"));
        QCOMPARE(qtFamily(QStringLiteral("CMG Sans"), QString(), listed), QStringLiteral("CMG Sans"));
    }

    void stylesAndPostScriptNames()
    {
        QCOMPARE(styleKey(QStringLiteral("Bold Italic")), styleKey(QStringLiteral("BoldItalic")));
        QCOMPARE(styleKey(QStringLiteral("Light")), QStringLiteral("light"));
        QVERIFY(styleKey(QStringLiteral("Light")) != styleKey(QStringLiteral("Light Italic")));
        QCOMPARE(postScriptStyle(QStringLiteral("CMGSans-LightItalic")), QStringLiteral("LightItalic"));
        // Which styles bold and italic say by themselves, and which need their names
        QVERIFY(saidByBoldAndItalic(QStringLiteral("Regular")) && saidByBoldAndItalic(QStringLiteral("Bold Italic"))
                && saidByBoldAndItalic(QStringLiteral("Book")) && saidByBoldAndItalic(QString()));
        QVERIFY(!saidByBoldAndItalic(QStringLiteral("Light")) && !saidByBoldAndItalic(QStringLiteral("Bold Condensed"))
                && !saidByBoldAndItalic(QStringLiteral("Medium CAPS")));
        QCOMPARE(postScriptStyle(QStringLiteral("Helvetica")), QString());
    }
};

QTEST_GUILESS_MAIN(TestFontNames)
#include "tst_fontnames.moc"
