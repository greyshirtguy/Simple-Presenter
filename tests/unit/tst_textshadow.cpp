// The shadow words cast in their own picture (src/textshadow.h): MultiEffect's measured blur
// read off its table; a dot's shadow where the offset puts it, with all of the dot's cover,
// spread as much as the sigma asked for, in the colour asked for, at each of the fractions
// of the picture's size it is worked out at; under what is there, which is left as it was;
// and none for a colour with no opacity.

#include "textshadow.h"

#include <QTest>

using namespace textshadow;

namespace {

// What a picture's shadow is: of every pixel outside the square (x, y, size) that the
// shadow alone makes, the opacity, summed; where its middle is; how far it spreads, across
// and down (the standard deviation of where it is, by opacity); and whether it is all of
// the colour asked for
struct Measured
{
    double mass = 0;
    QPointF middle;
    QPointF spread;
    bool ofColour = true;
};

Measured measure(const QImage &picture, const QRect &dot, const QColor &colour)
{
    Measured m;
    double sx = 0, sy = 0, sxx = 0, syy = 0;
    for (int y = 0; y < picture.height(); ++y) {
        for (int x = 0; x < picture.width(); ++x) {
            if (dot.contains(x, y))
                continue;
            const QRgb pixel = picture.pixel(x, y);
            const double a = qAlpha(pixel);
            if (a == 0)
                continue;
            // (Premultiplied: each part of the colour in proportion to the opacity, within
            // three: stretching the shadow rounds each part down on its own, twice. Where it
            // is faint, that is most of each part, and so only where it is an eighth opaque
            // or more.)
            const QRgb premultiplied = reinterpret_cast<const QRgb *>(picture.constScanLine(y))[x];
            if (a >= 32
                && (qAbs(qRed(premultiplied) - colour.red() * a / 255) > 3 || qAbs(qGreen(premultiplied) - colour.green() * a / 255) > 3
                    || qAbs(qBlue(premultiplied) - colour.blue() * a / 255) > 3))
                m.ofColour = false;
            m.mass += a;
            sx += a * (x + 0.5);
            sy += a * (y + 0.5);
            sxx += a * (x + 0.5) * (x + 0.5);
            syy += a * (y + 0.5) * (y + 0.5);
        }
    }
    if (m.mass > 0) {
        m.middle = QPointF(sx / m.mass, sy / m.mass);
        m.spread = QPointF(std::sqrt(sxx / m.mass - m.middle.x() * m.middle.x()), std::sqrt(syy / m.mass - m.middle.y() * m.middle.y()));
    }
    return m;
}

QImage clear(int width, int height)
{
    QImage picture(width, height, QImage::Format_ARGB32_Premultiplied);
    picture.fill(Qt::transparent);
    return picture;
}

} // namespace

class TestTextShadow : public QObject
{
    Q_OBJECT

private slots:
    void sigmaFromTheTable()
    {
        QCOMPARE(multiEffectSigma(0), 0.0);
        QCOMPARE(multiEffectSigma(-5), 0.0);
        QCOMPARE(multiEffectSigma(16), 2.75);
        QCOMPARE(multiEffectSigma(64), 18.75);
        // Past the table, its last
        QCOMPARE(multiEffectSigma(100), 18.75);
        // Between two of it, in a straight line: 14 is three eighths of the way from 12.8 to 16
        QVERIFY(qAbs(multiEffectSigma(14) - 2.4375) < 1e-9);
    }

    void aDotsShadow_data()
    {
        QTest::addColumn<double>("sigma");
        // (One of each of the fractions it is worked out at: whole, a half, a quarter, an eighth)
        QTest::newRow("1") << 1.0;
        QTest::newRow("3") << 3.0;
        QTest::newRow("6") << 6.0;
        QTest::newRow("12") << 12.0;
    }

    void aDotsShadow()
    {
        QFETCH(double, sigma);
        // A white square twelve pixels across, about as broad as a letter's stroke on a
        // screen, its shadow moved well clear of it. (Not a dot of a few pixels: a picture
        // has whole numbers of 0 to 255 for how opaque each pixel is, and the shadow of so
        // little, spread wide, is a few at most, whose rounding loses a good part of it,
        // a third at a sigma of 12, as anything drawn so would.)
        QImage picture = clear(260, 260);
        const QRect dot(60, 60, 12, 12);
        for (int y = dot.top(); y <= dot.bottom(); ++y) {
            for (int x = dot.left(); x <= dot.right(); ++x)
                picture.setPixel(x, y, qRgba(255, 255, 255, 255));
        }
        const QColor colour(20, 40, 200, 255);
        cast(picture, colour, QPointF(80, 60), sigma);

        const Measured shadow = measure(picture, dot, colour);
        // All of the square's cover, moved by the offset (within half a cell of where it is
        // worked out, and a pixel). (A wide shadow is a little short of it: a pixel's opacity
        // is a whole number of 0 to 255, and stretching the shadow to the picture rounds the
        // faint ones down, a tenth of it at a sigma of 12. See textshadow.h.)
        const double cover = dot.width() * dot.height() * 255.0;
        const double short_ = sigma >= 8 ? 0.12 : 0.05;
        QVERIFY2(shadow.mass > cover * (1 - short_) && shadow.mass < cover * 1.03, qPrintable(QString::number(shadow.mass / cover)));
        const int step = sigma < 1.5 ? 1 : sigma < 4 ? 2 : sigma < 8 ? 4 : 8;
        const QPointF expected = QPointF(dot.center()) + QPointF(0.5, 0.5) + QPointF(80, 60);
        QVERIFY2(qAbs(shadow.middle.x() - expected.x()) <= step / 2.0 + 1 && qAbs(shadow.middle.y() - expected.y()) <= step / 2.0 + 1,
                 qPrintable(QStringLiteral("%1, %2").arg(shadow.middle.x()).arg(shadow.middle.y())));
        // As spread as the blur, past the square's own breadth (a box n wide spreads by
        // sqrt((n² - 1) / 12)), within a fifth
        const double own = std::sqrt((dot.width() * dot.width() - 1) / 12.0);
        const double across = std::sqrt(std::max(0.0, shadow.spread.x() * shadow.spread.x() - own * own));
        const double down = std::sqrt(std::max(0.0, shadow.spread.y() * shadow.spread.y() - own * own));
        QVERIFY2(qAbs(across - sigma) < sigma * 0.2 + 0.3 && qAbs(down - sigma) < sigma * 0.2 + 0.3,
                 qPrintable(QStringLiteral("%1, %2").arg(across).arg(down)));
        QVERIFY(shadow.ofColour);
        // The square itself as it was
        QCOMPARE(picture.pixel(65, 65), qRgba(255, 255, 255, 255));
    }

    void underWhatIsThere()
    {
        // A square of half see-through red, its shadow moved half onto it: where they meet, the
        // red over the shadow, which is half as strong as the square's cover is (the red's
        // half, and half of the shadow's under it: three quarters, 192); elsewhere on the
        // square, the red as it was
        QImage picture = clear(120, 120);
        const QRgb red = qPremultiply(qRgba(255, 0, 0, 128));
        for (int y = 40; y < 80; ++y) {
            for (int x = 40; x < 80; ++x)
                reinterpret_cast<QRgb *>(picture.scanLine(y))[x] = red;
        }
        cast(picture, QColor(0, 0, 0, 255), QPointF(20, 0), 0.5);
        const QRgb alone = reinterpret_cast<const QRgb *>(picture.constScanLine(60))[45];
        QCOMPARE(alone, red);
        const QRgb met = reinterpret_cast<const QRgb *>(picture.constScanLine(60))[70];
        QVERIFY2(qAbs(qAlpha(met) - 192) <= 2 && qRed(met) >= 126 && qRed(met) <= 129, qPrintable(QString::number(met, 16)));
        // Beyond the square, the shadow alone, black
        const QRgb shade = reinterpret_cast<const QRgb *>(picture.constScanLine(60))[90];
        QVERIFY2(qAlpha(shade) > 120 && qAlpha(shade) < 136 && qRed(shade) == 0, qPrintable(QString::number(shade, 16)));
    }

    void noColourNoShadow()
    {
        QImage picture = clear(40, 40);
        picture.setPixel(20, 20, qRgba(255, 255, 255, 255));
        const QImage before = picture.copy();
        cast(picture, QColor(0, 0, 0, 0), QPointF(5, 5), 3);
        QCOMPARE(picture, before);
    }
};

QTEST_GUILESS_MAIN(TestTextShadow)
#include "tst_textshadow.moc"
