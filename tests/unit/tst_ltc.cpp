#include <QtTest>

#include "ltc.h"
#include "ltcmaker.h"

#include <random>

// Linear timecode read out of sound (src/ltc.h), on sound made for it (ltcmaker.h): the
// frames it reads, where each is said to end, and what it does with sound that is not
// timecode, or is timecode heard badly.
class TestLtc : public QObject
{
    Q_OBJECT

    static std::vector<ltc::Frame> read(const std::vector<float> &sound, int rate = 48000, int piece = 0)
    {
        ltc::Decoder decoder(rate);
        std::vector<ltc::Frame> frames;
        if (piece <= 0) {
            decoder.feed(sound.data(), int(sound.size()), &frames);
            return frames;
        }
        for (size_t from = 0; from < sound.size(); from += piece)
            decoder.feed(sound.data() + from, int(std::min<size_t>(piece, sound.size() - from)), &frames);
        return frames;
    }

    static QString said(const ltc::Frame &frame)
    {
        return QStringLiteral("%1:%2:%3;%4").arg(frame.hours).arg(frame.minutes).arg(frame.seconds).arg(frame.frames);
    }

private slots:
    // Thirty frames a second: every frame after the first is read, in order, each
    // ending where its last bit does, 1,600 samples after the one before
    void aRunIsReadFrameByFrame()
    {
        ltcmaker::Maker maker;
        maker.run(59, 58, 20, 40, 30);
        maker.silence(0.01);
        const std::vector<ltc::Frame> frames = read(maker.sound);
        // (The first is read, though nothing came before it; and the last, though
        // nothing comes after to mark its end.)
        QCOMPARE(int(frames.size()), 40);
        QCOMPARE(said(frames.front()), QStringLiteral("0:59:58;20"));
        QCOMPARE(said(frames.at(9)), QStringLiteral("0:59:58;29"));
        QCOMPARE(said(frames.at(10)), QStringLiteral("0:59:59;0"));
        QCOMPARE(said(frames.back()), QStringLiteral("0:59:59;29"));
        for (size_t i = 0; i < frames.size(); ++i) {
            QVERIFY(!frames.at(i).dropFrame);
            // Where it ends: the end of its eightieth bit, to a sample
            QVERIFY2(std::abs(frames.at(i).end - 1600.0 * (i + 1)) < 1.0, qPrintable(QString::number(frames.at(i).end)));
            if (i > 0)
                QVERIFY(std::abs(frames.at(i).samples - 1600.0) < 0.5);
        }
        // The first has nothing before it to be measured from
        QCOMPARE(frames.front().samples, 0.0);
    }

    // Every rate there is, by the sound card's rate: nothing is told which it is
    void everyRateIsRead_data()
    {
        QTest::addColumn<int>("rate");
        QTest::addColumn<double>("fps");
        QTest::addColumn<int>("each");
        QTest::addColumn<bool>("drop");
        QTest::newRow("24 at 48000") << 48000 << 24.0 << 24 << false;
        QTest::newRow("25 at 48000") << 48000 << 25.0 << 25 << false;
        QTest::newRow("29.97 at 48000") << 48000 << 30000.0 / 1001 << 30 << true;
        QTest::newRow("30 at 44100") << 44100 << 30.0 << 30 << false;
        QTest::newRow("24 at 44100") << 44100 << 24.0 << 24 << false;
        QTest::newRow("25 at 96000") << 96000 << 25.0 << 25 << false;
        QTest::newRow("30 at 32000") << 32000 << 30.0 << 30 << false;
        // A source running fast and slow by a twelfth
        QTest::newRow("30 fast") << 48000 << 32.5 << 30 << false;
        QTest::newRow("24 slow") << 48000 << 22.0 << 24 << false;
    }

    void everyRateIsRead()
    {
        QFETCH(int, rate);
        QFETCH(double, fps);
        QFETCH(int, each);
        QFETCH(bool, drop);
        ltcmaker::Maker maker;
        maker.rate = rate;
        maker.fps = fps;
        maker.run(1, 2, 3, 3 * each, each, drop);
        maker.silence(0.01);
        const std::vector<ltc::Frame> frames = read(maker.sound, rate);
        QCOMPARE(int(frames.size()), 3 * each);
        QCOMPARE(said(frames.front()), QStringLiteral("0:1:2;3"));
        QCOMPARE(said(frames.back()), QStringLiteral("0:1:5;2"));
        QCOMPARE(frames.back().dropFrame, drop);
        // How long its frames are says how fast it is going: to a part in a thousand,
        // over a second of them
        double length = 0;
        for (size_t i = 1; i <= size_t(each); ++i)
            length += frames.at(i).samples;
        QVERIFY2(std::abs(rate / (length / each) - fps) < fps * 0.0005, qPrintable(QString::number(rate / (length / each), 'f', 4)));
    }

    // Turned over, quiet, sitting off centre, and with hiss on it: read all the same
    void aPoorSignalIsStillRead()
    {
        std::mt19937 random(7);
        std::uniform_real_distribution<float> hiss(-0.04f, 0.04f);
        for (int which = 0; which < 4; ++which) {
            ltcmaker::Maker maker;
            maker.up = which == 0;
            maker.level = which == 1 ? 0.03f : 0.3f;
            maker.offset = which == 2 ? 0.4f : 0;
            maker.silence(0.05);
            maker.run(0, 10, 0, 30, 30);
            maker.silence(0.01);
            if (which == 3) {
                for (float &sample : maker.sound)
                    sample += hiss(random);
            }
            const std::vector<ltc::Frame> frames = read(maker.sound);
            QVERIFY2(frames.size() == 30, qPrintable(QStringLiteral("%1: %2 frames").arg(which).arg(frames.size())));
            QCOMPARE(said(frames.back()), QStringLiteral("0:0:10;29"));
        }
    }

    // Handed over in pieces of any size, what is read is the same as from the whole
    void piecesReadAsTheWhole()
    {
        ltcmaker::Maker maker;
        maker.fps = 25;
        maker.run(3, 0, 0, 50, 25);
        maker.silence(0.01);
        const std::vector<ltc::Frame> whole = read(maker.sound);
        QCOMPARE(int(whole.size()), 50);
        for (int piece : {1, 7, 480, 1024, 4801}) {
            const std::vector<ltc::Frame> parts = read(maker.sound, 48000, piece);
            QCOMPARE(parts.size(), whole.size());
            for (size_t i = 0; i < whole.size(); ++i) {
                QCOMPARE(said(parts.at(i)), said(whole.at(i)));
                QVERIFY(std::abs(parts.at(i).end - whole.at(i).end) < 1e-6);
            }
        }
    }

    // The timecode jumping (a playback rig sent somewhere else): the frames either side
    // are read, and the first after the jump is as long as any
    void aJumpIsReadThrough()
    {
        ltcmaker::Maker maker;
        maker.run(0, 10, 0, 10, 30);
        maker.run(20, 0, 0, 10, 30);
        maker.silence(0.01);
        const std::vector<ltc::Frame> frames = read(maker.sound);
        QCOMPARE(int(frames.size()), 20);
        QCOMPARE(said(frames.at(9)), QStringLiteral("0:0:10;9"));
        QCOMPARE(said(frames.at(10)), QStringLiteral("0:20:0;0"));
        QVERIFY(std::abs(frames.at(10).samples - 1600.0) < 0.5);
    }

    // Stopped and started again: the frames after the silence are read from the first
    // whole one, which has nothing before it to be measured from
    void silenceBetweenRuns()
    {
        ltcmaker::Maker maker;
        maker.run(0, 1, 0, 5, 30);
        maker.silence(0.5);
        maker.run(0, 2, 0, 5, 30);
        maker.silence(0.01);
        const std::vector<ltc::Frame> frames = read(maker.sound);
        QCOMPARE(int(frames.size()), 10);
        QCOMPARE(said(frames.at(4)), QStringLiteral("0:0:1;4"));
        QCOMPARE(said(frames.at(5)), QStringLiteral("0:0:2;0"));
        QCOMPARE(frames.at(5).samples, 0.0);
        QVERIFY(std::abs(frames.at(6).samples - 1600.0) < 0.5);
        // The sixth ends eighty bits after the silence does
        QVERIFY(std::abs(frames.at(5).end - (5 * 1600.0 + 24000 + 1600)) < 1.0);
    }

    // What is not timecode is not read as any: silence, hiss, a tone, and music of a kind
    void whatIsNotTimecodeIsNot()
    {
        std::mt19937 random(11);
        std::uniform_real_distribution<float> hiss(-0.5f, 0.5f);
        std::vector<float> sound(48000 * 3, 0.0f);
        QVERIFY(read(sound).empty());
        for (float &sample : sound)
            sample = hiss(random);
        QVERIFY(read(sound).empty());
        for (size_t i = 0; i < sound.size(); ++i)
            sound[i] = 0.5f * std::sin(2 * M_PI * 1200 * i / 48000.0);
        QVERIFY(read(sound).empty());
        for (size_t i = 0; i < sound.size(); ++i)
            sound[i] = 0.3f * std::sin(2 * M_PI * 440 * i / 48000.0) + 0.2f * std::sin(2 * M_PI * 2400 * i / 48000.0)
                       + 0.1f * std::sin(2 * M_PI * 97 * i / 48000.0);
        QVERIFY(read(sound).empty());
    }

    // A frame whose digits are no time is let go, and the frames after it are read
    void digitsThatAreNoTimeAreLetGo()
    {
        ltcmaker::Maker maker;
        maker.run(0, 0, 0, 3, 30);
        // (Seventy-nine seconds, and thirty-five frames)
        std::array<bool, 80> bad = ltcmaker::bits(0, 0, 9, 5, false);
        bad[24] = bad[25] = bad[26] = true;
        for (bool one : bad)
            maker.bit(one);
        bad = ltcmaker::bits(0, 0, 1, 5, false);
        bad[8] = bad[9] = true;
        for (bool one : bad)
            maker.bit(one);
        maker.run(0, 1, 2, 3, 30);
        maker.silence(0.01);
        const std::vector<ltc::Frame> frames = read(maker.sound);
        QCOMPARE(int(frames.size()), 6);
        QCOMPARE(said(frames.at(2)), QStringLiteral("0:0:0;2"));
        QCOMPARE(said(frames.at(3)), QStringLiteral("0:0:1;2"));
        // (How long the frame after them was is still known: where they ended was kept.)
        QVERIFY(std::abs(frames.at(3).samples - 1600.0) < 0.5);
    }

    // Played backwards (a tape rewinding), nothing is read: the bits that end a frame
    // come first and the other way round
    void backwardsIsNotRead()
    {
        ltcmaker::Maker maker;
        maker.silence(0.01);
        maker.run(0, 5, 0, 30, 30);
        maker.silence(0.01);
        std::reverse(maker.sound.begin(), maker.sound.end());
        QVERIFY(read(maker.sound).empty());
    }

    // How far apart its sides are is said, for a light that says a signal is there
    void theSwingIsSaid()
    {
        ltc::Decoder decoder(48000);
        std::vector<ltc::Frame> frames;
        QVERIFY(decoder.swing() < 0.01f);
        ltcmaker::Maker maker;
        maker.level = 0.25f;
        maker.run(0, 0, 0, 5, 30);
        decoder.feed(maker.sound.data(), int(maker.sound.size()), &frames);
        QVERIFY(std::abs(decoder.swing() - 0.5f) < 0.05f);
        QCOMPARE(decoder.fed(), qint64(maker.sound.size()));
    }
};

QTEST_APPLESS_MAIN(TestLtc)
#include "tst_ltc.moc"
