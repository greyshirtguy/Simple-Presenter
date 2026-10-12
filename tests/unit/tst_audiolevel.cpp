// The arithmetic of sound (src/audiolevel.h): how loud a stretch of it is, what a
// meter shows of that, and how a fade goes.

#include "audiolevel.h"

#include <QList>
#include <QTest>

#include <cmath>

using namespace audiolevel;

class TestAudioLevel : public QObject
{
    Q_OBJECT

private slots:
    // ---- How loud
    void thePeakOfEachChannelIsItsLoudestSample()
    {
        // Left, right, left, right: the left gets to half, one way or the other, and
        // the right to a quarter.
        const qint16 samples[] = {1000, -200, -16384, 8192, 16000, 0, 3, -8000};
        const Peaks found = peaks(samples, sizeof(samples), Format::Int16, 2);
        QCOMPARE(found.left, 0.5f);
        QCOMPARE(found.right, 0.25f);
    }

    void soundOfOneChannelIsAsLoudOnBothSides()
    {
        const qint16 samples[] = {100, -8192, 4000};
        const Peaks found = peaks(samples, sizeof(samples), Format::Int16, 1);
        QCOMPARE(found.left, 0.25f);
        QCOMPARE(found.right, 0.25f);
    }

    void ofMoreThanTwoChannelsTheFirstTwoAreTaken()
    {
        // Six channels, one moment of them: the third is the loudest and is not looked at.
        const float samples[] = {0.1f, -0.2f, 0.9f, 0.3f, 0.3f, 0.3f};
        const Peaks found = peaks(samples, sizeof(samples), Format::Float, 6);
        QCOMPARE(found.left, 0.1f);
        QCOMPARE(found.right, 0.2f);
    }

    void everyWayOfWritingSamplesIsRead()
    {
        const quint8 bytes[] = {128, 128, 192, 64, 255, 0};
        Peaks found = peaks(bytes, sizeof(bytes), Format::UInt8, 2);
        QVERIFY(found.left > 0.99f && found.left <= 1.0f);
        QCOMPARE(found.right, 1.0f);
        const qint32 wide[] = {1073741824, -536870912};
        found = peaks(wide, sizeof(wide), Format::Int32, 2);
        QCOMPARE(found.left, 0.5f);
        QCOMPARE(found.right, 0.25f);
        const float floating[] = {-0.75f, 0.5f, 0.25f, -0.125f};
        found = peaks(floating, sizeof(floating), Format::Float, 2);
        QCOMPARE(found.left, 0.75f);
        QCOMPARE(found.right, 0.5f);
    }

    void asLoudAsCanBeIsOneAndNoMore()
    {
        // The most a 16-bit sample can say downwards, and floating samples over the top
        const qint16 samples[] = {-32768, 32767};
        Peaks found = peaks(samples, sizeof(samples), Format::Int16, 2);
        QCOMPARE(found.left, 1.0f);
        QVERIFY(found.right > 0.9999f && found.right <= 1.0f);
        const float hot[] = {2.5f, -7.0f};
        found = peaks(hot, sizeof(hot), Format::Float, 2);
        QCOMPARE(found.left, 1.0f);
        QCOMPARE(found.right, 1.0f);
    }

    void whatIsNoSoundIsSilence()
    {
        const qint16 samples[] = {0, 0, 0, 0};
        Peaks found = peaks(samples, sizeof(samples), Format::Int16, 2);
        QCOMPARE(found.left, 0.0f);
        QCOMPARE(found.right, 0.0f);
        found = peaks(nullptr, 64, Format::Int16, 2);
        QCOMPARE(found.left, 0.0f);
        found = peaks(samples, 0, Format::Int16, 2);
        QCOMPARE(found.left, 0.0f);
        found = peaks(samples, sizeof(samples), Format::Int16, 0);
        QCOMPARE(found.left, 0.0f);
        // A number that is none, and half a sample left over at the end
        const float odd[] = {std::nanf(""), 0.5f};
        found = peaks(odd, sizeof(odd), Format::Float, 2);
        QCOMPARE(found.left, 0.0f);
        QCOMPARE(found.right, 0.5f);
        found = peaks(samples, 3, Format::Int16, 1);
        QCOMPARE(found.left, 0.0f);
    }

    // ---- The meter
    void theMeterGoesByDecibels()
    {
        QCOMPARE(metered(1.0), 1.0);
        // Half as loud is 6 dB down, a tenth of the meter (a hair over 6 dB, in truth)
        QVERIFY(qAbs(metered(0.5) - 0.9) < 0.001);
        QVERIFY(qAbs(metered(0.1) - 2.0 / 3) < 0.0001);     // 20 dB down
        QVERIFY(qAbs(metered(0.01) - 1.0 / 3) < 0.0001);    // 40 dB down
        QCOMPARE(metered(0.001), 0.0);                       // 60 dB down: its foot
        QCOMPARE(metered(0.0001), 0.0);
        QCOMPARE(metered(0.0), 0.0);
        QCOMPARE(metered(-1.0), 0.0);
        QCOMPARE(metered(3.0), 1.0);
    }

    void theMeterGoesUpAtOnceAndComesDownOverAMoment()
    {
        // Up to a new peak at once
        QCOMPARE(settled(0.2, 0.9, 0.05), 0.9);
        // Down from an old one by as much of its height as of its fall has gone by
        QVERIFY(qAbs(settled(1.0, 0.0, kFall / 2) - 0.5) < 1e-9);
        QCOMPARE(settled(1.0, 0.0, kFall), 0.0);
        QCOMPARE(settled(1.0, 0.0, 10.0), 0.0);
        // Not below what the sound is at now
        QCOMPARE(settled(1.0, 0.7, kFall), 0.7);
        // And never out of the meter
        QCOMPARE(settled(0.5, 1.4, 0.0), 1.0);
        QCOMPARE(settled(0.5, -1.0, 100.0), 0.0);
        QCOMPARE(settled(0.5, 0.0, -1.0), 0.5);
    }

    // ---- Fades
    void aFadeGoesFromNothingToAllAndBack()
    {
        QCOMPARE(fadeIn(0.0), 0.0);
        QVERIFY(qAbs(fadeIn(1.0) - 1.0) < 1e-12);
        QCOMPARE(fadeOut(0.0), 1.0);
        QVERIFY(qAbs(fadeOut(1.0)) < 1e-12);
        // Before its start and after its end it is where it starts and ends.
        QCOMPARE(fadeIn(-3.0), 0.0);
        QVERIFY(qAbs(fadeIn(7.0) - 1.0) < 1e-12);
        QCOMPARE(fadeOut(-3.0), 1.0);
        // Always the one way
        double last = -1;
        for (int step = 0; step <= 20; ++step) {
            QVERIFY(fadeIn(step / 20.0) > last);
            last = fadeIn(step / 20.0);
        }
    }

    void twoPiecesCrossingAreTogetherAsLoudAllTheWay()
    {
        // The power of the two together, which is what loudness goes by, is the same
        // at every point of the crossing. (With straight lines it is half in the
        // middle.)
        for (int step = 0; step <= 20; ++step) {
            const double along = step / 20.0;
            const double power = fadeIn(along) * fadeIn(along) + fadeOut(along) * fadeOut(along);
            QVERIFY(qAbs(power - 1.0) < 1e-12);
        }
    }
};

QTEST_APPLESS_MAIN(TestAudioLevel)
#include "tst_audiolevel.moc"
