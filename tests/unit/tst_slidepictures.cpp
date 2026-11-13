// The pictures of slides for ProPresenter's API (src/slidepictures.h): the size asked
// for and the picture's size in the slide's proportions, JPEG or PNG, the checkerboard's
// square (ProPresenter's 8 at 400 wide), what goes to and from the process that draws
// them, and the name a picture is kept under, which must change with anything it is
// drawn from.

#include "slidepictures.h"

#include <QtTest>

using namespace slidepictures;

class TestSlidePictures : public QObject
{
    Q_OBJECT

private slots:
    void theQualityIsTheLongerSide()
    {
        QCOMPARE(qualityOf(QString()), 400);
        QCOMPARE(qualityOf(QStringLiteral("200")), 200);
        QCOMPARE(qualityOf(QStringLiteral(" 640 ")), 640);
        QCOMPARE(qualityOf(QStringLiteral("0")), 400);
        QCOMPARE(qualityOf(QStringLiteral("-5")), 400);
        QCOMPARE(qualityOf(QStringLiteral("big")), 400);
        QCOMPARE(qualityOf(QStringLiteral("5000")), 1920);
        QCOMPARE(qualityOf(QStringLiteral("8")), 16);

        QCOMPARE(sizeFor(1920, 1080, 400), QSize(400, 225));
        QCOMPARE(sizeFor(1080, 1920, 400), QSize(225, 400));
        QCOMPARE(sizeFor(1024, 768, 400), QSize(400, 300));
        QCOMPARE(sizeFor(1920, 1080, 200), QSize(200, 113));
        QCOMPARE(sizeFor(0, 0, 400), QSize(400, 225));
    }

    void jpegUnlessPngIsAskedFor()
    {
        QVERIFY(!pngAsked(QString(), QByteArray()));
        QVERIFY(pngAsked(QStringLiteral("png"), QByteArray()));
        QVERIFY(pngAsked(QStringLiteral("PNG"), "image/jpeg"));
        // thumbnail_type before the Accept header
        QVERIFY(!pngAsked(QStringLiteral("jpeg"), "image/png"));
        QVERIFY(pngAsked(QString(), "image/png"));
        QVERIFY(pngAsked(QString(), "image/png, image/jpeg"));
        QVERIFY(!pngAsked(QString(), "image/jpeg, image/png"));
        QVERIFY(!pngAsked(QString(), "*/*"));
    }

    void theCheckerboardsSquareGrowsWithThePicture()
    {
        QCOMPARE(squareFor(400), 8);
        QCOMPARE(squareFor(800), 16);
        QCOMPARE(squareFor(200), 4);
        QCOMPARE(squareFor(16), 2);
    }

    void whatTheWorkerIsAskedGoesAndComesBackWhole()
    {
        Asked asked;
        asked.file = QStringLiteral("/w/Libraries/Demo/Amazing Grace.pro");
        asked.index = 3;
        asked.quality = 640;
        asked.png = true;
        qint64 id = 0;
        Asked read;
        const QByteArray line = askedLine(7, asked);
        QVERIFY(line.endsWith('\n') && line.count('\n') == 1);
        QVERIFY(readAsked(line, &id, &read));
        QCOMPARE(id, 7);
        QCOMPARE(read, asked);
        QVERIFY(!read.arrangement);
        // A playlist's choice of arrangement, even none ("" is not "the presentation's own")
        asked.arrangement = QString();
        QVERIFY(readAsked(askedLine(8, asked), &id, &read));
        QVERIFY(read.arrangement && read.arrangement->isEmpty());
        asked.arrangement = QStringLiteral("Short");
        QVERIFY(readAsked(askedLine(9, asked), &id, &read));
        QCOMPARE(read, asked);
        QVERIFY(!readAsked("{\"id\":1}", &id, &read));
        QVERIFY(!readAsked("nothing", &id, &read));

        Answer answer;
        answer.id = 9;
        answer.status = 200;
        answer.type = "image/png";
        answer.length = 6107;
        Answer back;
        QVERIFY(readAnswer(answerLine(answer), &back));
        QCOMPARE(back.id, 9);
        QCOMPARE(back.status, 200);
        QCOMPARE(back.type, QByteArray("image/png"));
        QCOMPARE(back.length, 6107);
        answer.length = -1;
        QVERIFY(!readAnswer(answerLine(answer), &back));
    }

    void aPictureIsKeptUnderEverythingItIsDrawnFrom()
    {
        Asked asked;
        asked.file = QStringLiteral("/w/Libraries/Demo/Amazing Grace.pro");
        asked.index = 3;
        const QString name = cacheName(asked, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138"));
        QVERIFY(name.endsWith(QStringLiteral(".jpg")));
        QCOMPARE(cacheName(asked, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")), name);
        QVERIFY(cacheName(asked, 1001, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")) != name);
        QVERIFY(cacheName(asked, 1000, QStringLiteral("/w/Media/b.jpg"), 2000, QStringLiteral("0.138")) != name);
        QVERIFY(cacheName(asked, 1000, QStringLiteral("/w/Media/a.jpg"), 2001, QStringLiteral("0.138")) != name);
        QVERIFY(cacheName(asked, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.139")) != name);
        Asked other = asked;
        other.index = 4;
        QVERIFY(cacheName(other, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")) != name);
        other = asked;
        other.quality = 200;
        QVERIFY(cacheName(other, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")) != name);
        other = asked;
        other.arrangement = QString();
        QVERIFY(cacheName(other, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")) != name);
        other = asked;
        other.png = true;
        QVERIFY(cacheName(other, 1000, QStringLiteral("/w/Media/a.jpg"), 2000, QStringLiteral("0.138")).endsWith(QStringLiteral(".png")));
    }
};

QTEST_GUILESS_MAIN(TestSlidePictures)
#include "tst_slidepictures.moc"
