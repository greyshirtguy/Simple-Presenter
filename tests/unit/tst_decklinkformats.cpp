// What a screen sent out through a DeckLink card is sent as (src/decklinkformats.h): the
// formats offered, each with its size, its rate and the SDK's code for it; and its
// pictures made straight from premultiplied.

#include "decklinkformats.h"

#include <QTest>

using namespace decklinkformats;

class TestDeckLinkFormats : public QObject
{
    Q_OBJECT

private slots:
    void theFormatsAsked()
    {
        QStringList names;
        for (const Format &format : formats())
            names << format.name;
        QCOMPARE(names.join(u' '), QStringLiteral("1080p24 1080p25 1080p29.97 1080p30 1080p50 1080p59.94 1080p60 720p50 720p59.94 720p60"));
    }

    void eachHasItsSizeRateAndCode()
    {
        const Format *drop = formatNamed(QStringLiteral("1080p29.97"));
        QVERIFY(drop);
        QCOMPARE(drop->width, 1920);
        QCOMPARE(drop->height, 1080);
        QCOMPARE(drop->rateNumerator, 30000);
        QCOMPARE(drop->rateDenominator, 1001);
        // The SDK's codes, as its header has them (bmdModeHD1080p2997, bmdModeHD720p50, bmdModeHD1080p24)
        QCOMPARE(drop->mode, 0x48703239u);
        QCOMPARE(formatNamed(QStringLiteral("720p50"))->mode, 0x68703530u);
        QCOMPARE(formatNamed(QStringLiteral("720p50"))->height, 720);
        QCOMPARE(formatNamed(QStringLiteral("1080p24"))->mode, 0x32347073u);
        QCOMPARE(formatNamed(QStringLiteral("1080p60"))->rateDenominator, 1);
        QVERIFY(!formatNamed(QStringLiteral("720p25")));
        QVERIFY(!formatNamed(QString()));
    }

    void straightFromPremultiplied()
    {
        // Blue, green, red, opacity: opaque stays, nothing stays nothing, half-seen red
        // at half strength becomes full red, a quarter-seen grey a quarter as bright
        // becomes that grey, and a colour drawing left a little over its opacity is white.
        uchar pixels[] = {10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 128, 128, 16, 16, 16, 64, 70, 70, 70, 64};
        unpremultiplied(pixels, 5);
        const uchar wanted[] = {10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 255, 128, 64, 64, 64, 64, 255, 255, 255, 64};
        for (int i = 0; i < 20; ++i)
            QCOMPARE(int(pixels[i]), int(wanted[i]));
    }
};

QTEST_APPLESS_MAIN(TestDeckLinkFormats)
#include "tst_decklinkformats.moc"
