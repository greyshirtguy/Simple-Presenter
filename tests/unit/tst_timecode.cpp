#include <QtTest>

#include "ltc.h"
#include "ltcmaker.h"
#include "timecode.h"

#include <random>

using timecode::Chase;
using timecode::Label;

// The rules of timecode (src/timecode.h): what a timecode is in frames and in time, at
// each of ProPresenter's four formats; when the sound it came in was heard; when it
// starts, jumps and stops; and which presentation of a playlist it is in.
class TestTimecode : public QObject
{
    Q_OBJECT

    // Gives a chase `count` frames from frame number `from`, the first ending at `at`,
    // each a frame's length after the one before; what the last did, and when it ended
    static Chase::Change give(Chase &chase, qint64 from, int count, double *at, bool drop = false)
    {
        Chase::Change change = Chase::Nothing;
        for (int i = 0; i < count; ++i) {
            change = chase.frame(timecode::labelOf(from + i, chase.format(), drop), drop, *at);
            *at += 1 / timecode::rate(chase.format());
        }
        return change;
    }

private slots:
    void theFormatsAreProPresenters()
    {
        // (Its numbers for them, in its files: TimecodeSettings.Format)
        QCOMPARE(int(timecode::Fps24), 0);
        QCOMPARE(int(timecode::Fps25), 1);
        QCOMPARE(int(timecode::Fps2997), 2);
        QCOMPARE(int(timecode::Fps30), 3);
        QCOMPARE(timecode::count(timecode::Fps24), 24);
        QCOMPARE(timecode::count(timecode::Fps2997), 30);
        QCOMPARE(timecode::rate(timecode::Fps25), 25.0);
        QVERIFY(std::abs(timecode::rate(timecode::Fps2997) - 29.97003) < 1e-5);
        const QVariantList formats = timecode::formats();
        QCOMPARE(formats.size(), 4);
        QCOMPARE(formats.at(2).toMap().value("name").toString(), QStringLiteral("29.97 fps"));
        QCOMPARE(timecode::formatOf(1), timecode::Fps25);
        QCOMPARE(timecode::formatOf(99), timecode::Fps30);
    }

    // A timeline's start is kept as the timecode read as a clock, and written as
    // ProPresenter writes one
    void aTimecodeAsAClock()
    {
        const Label label {1, 0, 0, 12};
        QCOMPARE(timecode::faceSeconds(label, timecode::Fps30), 3600.4);
        QCOMPARE(timecode::faceSeconds(label, timecode::Fps24), 3600.5);
        QCOMPARE(timecode::labelAt(3600.4, timecode::Fps30), label);
        QCOMPARE(timecode::labelAt(3600.5, timecode::Fps24), label);
        QCOMPARE(timecode::text(label), QStringLiteral("01:00:00;12"));
        QCOMPARE(timecode::faceText(0, timecode::Fps30), QStringLiteral("00:00:00;00"));
        QCOMPARE(timecode::faceText(86399.99, timecode::Fps25), QStringLiteral("23:59:59;24"));
        // Past the day, or before it, the last or the first there is
        QCOMPARE(timecode::faceText(1e9, timecode::Fps25), QStringLiteral("23:59:59;24"));
        QCOMPARE(timecode::faceText(-5, timecode::Fps25), QStringLiteral("00:00:00;00"));
        // Every frame of a minute, there and back, at each format
        for (timecode::Format format : {timecode::Fps24, timecode::Fps25, timecode::Fps2997, timecode::Fps30}) {
            for (int frame = 0; frame < 60 * timecode::count(format); ++frame) {
                const Label each {2, 59, frame / timecode::count(format), frame % timecode::count(format)};
                QCOMPARE(timecode::labelAt(timecode::faceSeconds(each, format), format), each);
            }
        }
    }

    void whatIsTyped()
    {
        using timecode::typed;
        QCOMPARE(typed(QStringLiteral("01:00:00;12"), timecode::Fps30), 3600.4);
        QCOMPARE(typed(QStringLiteral(" 01:00:00:12 "), timecode::Fps30), 3600.4);
        QCOMPARE(typed(QStringLiteral("1:00:00"), timecode::Fps30), 3600.0);
        QCOMPARE(typed(QStringLiteral("2:30"), timecode::Fps30), 150.0);
        QCOMPARE(typed(QStringLiteral("3;12"), timecode::Fps24), 3.5);
        QCOMPARE(typed(QStringLiteral("3.12"), timecode::Fps24), 3.5);
        QCOMPARE(typed(QStringLiteral("01000000"), timecode::Fps25), 3600.0);
        QCOMPARE(typed(QStringLiteral("10000"), timecode::Fps25), 60.0);
        QCOMPARE(typed(QStringLiteral("0"), timecode::Fps25), 0.0);
        // No timecode
        QCOMPARE(typed(QString(), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("soon"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("00:00:00;30"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("00:00:00;24"), timecode::Fps24), -1.0);
        QCOMPARE(typed(QStringLiteral("00:61:00;00"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("24:00:00;00"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("1:2:3:4:5"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("123456789"), timecode::Fps30), -1.0);
        QCOMPARE(typed(QStringLiteral("-3"), timecode::Fps30), -1.0);
    }

    // Frames from 00:00:00;00: plain counting, but at 29.97 where the signal says its
    // count drops frames, which passes two numbers over at each minute but every tenth
    void framesAreCounted()
    {
        using timecode::frameNumber;
        QCOMPARE(frameNumber({1, 0, 0, 12}, timecode::Fps30, false), qint64(108012));
        QCOMPARE(frameNumber({1, 0, 0, 12}, timecode::Fps24, false), qint64(86412));
        QCOMPARE(frameNumber({1, 0, 0, 0}, timecode::Fps2997, false), qint64(108000));
        // Dropping: the frame after 00:00:59;29 is 00:01:00;02, and an hour is 107,892
        QCOMPARE(frameNumber({0, 0, 59, 29}, timecode::Fps2997, true), qint64(1799));
        QCOMPARE(frameNumber({0, 1, 0, 2}, timecode::Fps2997, true), qint64(1800));
        QCOMPARE(frameNumber({0, 10, 0, 0}, timecode::Fps2997, true), qint64(17982));
        QCOMPARE(frameNumber({1, 0, 0, 0}, timecode::Fps2997, true), qint64(107892));
        // (The flag means nothing at another format.)
        QCOMPARE(frameNumber({1, 0, 0, 0}, timecode::Fps30, true), qint64(108000));
        QCOMPARE(timecode::labelOf(1800, timecode::Fps2997, true), (Label {0, 1, 0, 2}));
        QCOMPARE(timecode::labelOf(17982, timecode::Fps2997, true), (Label {0, 10, 0, 0}));
        QCOMPARE(timecode::labelOf(108012, timecode::Fps30, false), (Label {1, 0, 0, 12}));
        // There and back, over the first eleven minutes and round an hour
        for (bool drop : {false, true}) {
            for (qint64 number : {0LL, 1LL, 1799LL, 1800LL, 1801LL, 3597LL, 3598LL, 17981LL, 17982LL, 17983LL, 19780LL, 107891LL, 107892LL, 215784LL})
                QCOMPARE(frameNumber(timecode::labelOf(number, timecode::Fps2997, drop), timecode::Fps2997, drop), number);
        }
        // In time: an hour of timecode that drops frames is an hour to within four
        // thousandths of a second; one that does not is 3.6 seconds longer
        QVERIFY(std::abs(timecode::realSeconds({1, 0, 0, 0}, timecode::Fps2997, true) - 3600.0) < 0.004);
        QVERIFY(std::abs(timecode::realSeconds({1, 0, 0, 0}, timecode::Fps2997, false) - 3603.6) < 1e-6);
        QCOMPARE(timecode::realSeconds({1, 0, 0, 0}, timecode::Fps25, false), 3600.0);
    }

    // Sound is handed over in pieces, each late by a little and some by a lot: when a
    // sample was heard is gone by the piece that was least late
    void whenASampleWasHeard()
    {
        timecode::SampleClock clock(48000);
        QVERIFY(!clock.started());
        std::mt19937 random(3);
        std::uniform_real_distribution<double> late(0.002, 0.012);
        qint64 total = 0;
        // Ten seconds of pieces of 480 samples, the sound having begun at 100 s
        for (int piece = 0; piece < 1000; ++piece) {
            total += 480;
            double at = 100 + total / 48000.0 + late(random);
            // (Now and then the app was busy for a tenth of a second.)
            if (piece % 97 == 50)
                at += 0.1;
            clock.arrived(total, at);
            if (piece > 200)
                QVERIFY2(std::abs(clock.at(total) - (100 + total / 48000.0) - 0.002) < 0.0015, qPrintable(QString::number(clock.at(total), 'f', 5)));
        }
        QVERIFY(clock.started());
        // A sample in the middle of a piece, by where it is among them
        QVERIFY(std::abs(clock.at(total - 240) - clock.at(total) + 0.005) < 1e-9);
    }

    // Lost sound (the device fell behind): what comes after is heard later than its
    // count says, and that is believed within a couple of seconds
    void lostSoundIsCaughtUp()
    {
        timecode::SampleClock clock(48000);
        qint64 total = 0;
        double now = 50;
        for (int piece = 0; piece < 300; ++piece) {
            total += 480;
            now += 0.01;
            clock.arrived(total, now);
        }
        QVERIFY(std::abs(clock.at(total) - now) < 1e-6);
        // A quarter of a second of sound never handed over
        now += 0.25;
        for (int piece = 0; piece < 300; ++piece) {
            total += 480;
            now += 0.01;
            clock.arrived(total, now);
        }
        QVERIFY(std::abs(clock.at(total) - now) < 1e-6);
    }

    // The timecode starts at its second frame, which follows the first; and then goes
    // on by the clock between frames
    void itStartsAtItsSecondFrame()
    {
        Chase chase;
        chase.setFormat(timecode::Fps30);
        QVERIFY(!chase.running());
        double at = 10;
        // One frame alone starts nothing
        QCOMPARE(give(chase, 108000, 1, &at), Chase::Nothing);
        QVERIFY(!chase.running());
        QCOMPARE(give(chase, 108001, 1, &at), Chase::Started);
        QVERIFY(chase.running());
        // The frame after the second began as the second ended
        const double began = at - 1 / 30.0;
        QVERIFY(std::abs(chase.frames(began) - 108002) < 1e-6);
        QVERIFY(std::abs(chase.frames(began + 0.5 / 30) - 108002.5) < 1e-6);
        QVERIFY(std::abs(chase.seconds(began) - 108002 / 30.0) < 1e-6);
        QCOMPARE(chase.label(began + 0.01), (Label {1, 0, 0, 2}));
        // Frames that follow change nothing, and it is where the clock had it
        QCOMPARE(give(chase, 108002, 60, &at), Chase::Nothing);
        QVERIFY(std::abs(chase.frames(at - 1 / 30.0) - 108062) < 1e-3);
    }

    // A frame that does not follow is passed over, once: a bit read wrongly
    void oneFrameOutOfPlaceIsPassedOver()
    {
        Chase chase;
        chase.setFormat(timecode::Fps25);
        double at = 0;
        QCOMPARE(give(chase, 500, 10, &at), Chase::Nothing);
        QVERIFY(chase.running());
        QCOMPARE(give(chase, 90000, 1, &at), Chase::Nothing);
        // The run goes on where it was, and where it is has not moved
        QCOMPARE(give(chase, 511, 10, &at), Chase::Nothing);
        QVERIFY(chase.running());
        QVERIFY(std::abs(chase.frames(at - 0.04) - 521) < 1e-3);
        // And a frame missed (a click on the line) is as nothing
        at += 0.04;
        QCOMPARE(give(chase, 522, 10, &at), Chase::Nothing);
        QVERIFY(std::abs(chase.frames(at - 0.04) - 532) < 1e-3);
    }

    // A jump is believed at its second frame, and it is then there
    void aJumpIsBelievedAtItsSecondFrame()
    {
        Chase chase;
        chase.setFormat(timecode::Fps30);
        double at = 5;
        give(chase, 3000, 20, &at);
        QCOMPARE(give(chase, 54000, 1, &at), Chase::Nothing);
        // (Until then it is where it was going)
        QVERIFY(std::abs(chase.frames(at - 1 / 30.0) - 3021) < 1e-3);
        QCOMPARE(give(chase, 54001, 1, &at), Chase::Jumped);
        QVERIFY(std::abs(chase.frames(at - 1 / 30.0) - 54002) < 1e-6);
        // Back again, the same; and to the frame it would have been at anyway is no jump
        give(chase, 54002, 5, &at);
        QCOMPARE(give(chase, 10, 2, &at), Chase::Jumped);
        QCOMPARE(give(chase, 12, 30, &at), Chase::Nothing);
        // Two frames that do not follow each other are two frames out of place
        QCOMPARE(give(chase, 700, 1, &at), Chase::Nothing);
        QCOMPARE(give(chase, 900, 1, &at), Chase::Nothing);
        QCOMPARE(give(chase, 901, 1, &at), Chase::Jumped);
    }

    // Stopped when sound has been heard for three and a half frames with none in it;
    // and it stays where the frame after the last one began
    void itStopsWhenTheFramesDo()
    {
        Chase chase;
        chase.setFormat(timecode::Fps30);
        double at = 0;
        give(chase, 100, 10, &at);
        const double last = at - 1 / 30.0;
        QCOMPARE(chase.heard(last + 3.0 / 30), Chase::Nothing);
        QVERIFY(chase.running());
        QCOMPARE(chase.heard(last + 3.6 / 30), Chase::Stopped);
        QVERIFY(!chase.running());
        QVERIFY(std::abs(chase.frames(last + 10) - 110) < 1e-6);
        QCOMPARE(chase.label(last + 10), (Label {0, 0, 3, 20}));
        QCOMPARE(chase.heard(last + 20), Chase::Nothing);
        // Started again from where it stopped: a start, at its second frame
        at = last + 30;
        QCOMPARE(give(chase, 110, 1, &at), Chase::Nothing);
        QCOMPARE(give(chase, 111, 1, &at), Chase::Started);
        // One frame, then silence, then another that would have followed it: not a start
        chase.reset();
        at = 0;
        give(chase, 100, 1, &at);
        chase.heard(at + 1);
        at += 5;
        QCOMPARE(give(chase, 101, 1, &at), Chase::Nothing);
        QVERIFY(!chase.running());
    }

    // The app busy for a fifth of a second: the frames of that time are given all at
    // once, late, each with when it was heard; nothing stops and nothing jumps
    void framesGivenLateAllAtOnce()
    {
        Chase chase;
        chase.setFormat(timecode::Fps30);
        double at = 0;
        give(chase, 100, 30, &at);
        QCOMPARE(chase.heard(at), Chase::Nothing);
        // (Six frames arrive together, and then what was heard up to the last of them.)
        QCOMPARE(give(chase, 130, 6, &at), Chase::Nothing);
        QCOMPARE(chase.heard(at), Chase::Nothing);
        QVERIFY(chase.running());
        QVERIFY(std::abs(chase.frames(at - 1 / 30.0) - 136) < 1e-3);
    }

    // A source whose clock is not this computer's: the timecode is kept to, with no
    // jump, and between frames it is never a quarter of a frame out
    void aSourceALittleFastIsKeptTo()
    {
        Chase chase;
        chase.setFormat(timecode::Fps30);
        double at = 0;
        // A thousandth fast (29.97 read as 30 is as far out as that)
        const double period = 1 / 30.0 / 1.001;
        for (int i = 0; i < 3000; ++i) {
            QVERIFY(chase.frame(timecode::labelOf(1000 + i, timecode::Fps30, false), false, at) != Chase::Jumped);
            if (i > 2)
                QVERIFY2(std::abs(chase.frames(at) - (1001 + i)) < 0.25, qPrintable(QString::number(chase.frames(at) - (1001 + i))));
            at += period;
        }
    }

    // What the signal says of dropping frames is kept, for what the timecode is said as
    void theCountTheSignalUses()
    {
        Chase chase;
        chase.setFormat(timecode::Fps2997);
        double at = 0;
        // Up to the end of the first minute: 00:00:59;28, ;29, then 00:01:00;02
        QCOMPARE(give(chase, 1798, 2, &at, true), Chase::Started);
        QVERIFY(chase.drop());
        QCOMPARE(chase.label(at - 0.01), (Label {0, 1, 0, 2}));
        QCOMPARE(give(chase, 1800, 30, &at, true), Chase::Nothing);
        QCOMPARE(chase.label(at - 0.01), (Label {0, 1, 1, 2}));
        // A change of format starts it afresh
        chase.setFormat(timecode::Fps24);
        QVERIFY(!chase.running());
        QCOMPARE(chase.frames(at), 0.0);
    }

    // Sound to frames to the chase, as the app does it: thirty frames a second from the
    // hour, a jump to two hours, and a stop
    void fromSoundToWhereItIs()
    {
        ltcmaker::Maker maker;
        maker.silence(0.1);
        maker.run(60, 0, 0, 45, 30);
        maker.frame(2, 0, 0, 0);
        maker.frame(2, 0, 0, 1);
        maker.frame(2, 0, 0, 2);
        maker.silence(0.5);
        ltc::Decoder decoder(48000);
        timecode::SampleClock clock(48000);
        Chase chase;
        chase.setFormat(timecode::Fps30);
        QList<QPair<Chase::Change, double>> changes;
        std::vector<ltc::Frame> frames;
        // In pieces of a hundredth of a second, each handed over three thousandths late
        for (size_t from = 0; from < maker.sound.size(); from += 480) {
            const int count = int(std::min<size_t>(480, maker.sound.size() - from));
            frames.clear();
            decoder.feed(maker.sound.data() + from, count, &frames);
            clock.arrived(decoder.fed(), 1000 + decoder.fed() / 48000.0 + 0.003);
            for (const ltc::Frame &frame : frames) {
                const Chase::Change change = chase.frame({frame.hours, frame.minutes, frame.seconds, frame.frames}, frame.dropFrame, clock.at(frame.end));
                if (change != Chase::Nothing)
                    changes.append({change, chase.frames(clock.at(frame.end))});
            }
            const Chase::Change change = chase.heard(clock.at(double(decoder.fed())));
            if (change != Chase::Nothing)
                changes.append({change, chase.frames(clock.at(double(decoder.fed())))});
        }
        QCOMPARE(changes.size(), 3);
        QCOMPARE(changes.at(0).first, Chase::Started);
        QCOMPARE(changes.at(0).second, 108002.0);
        QCOMPARE(changes.at(1).first, Chase::Jumped);
        QCOMPARE(changes.at(1).second, 216002.0);
        QCOMPARE(changes.at(2).first, Chase::Stopped);
        QCOMPARE(changes.at(2).second, 216003.0);
    }

    // Which presentation of the playlist the timecode is in, and how far into its timeline
    void thePresentationTheTimecodeIsIn()
    {
        const QVariantList rows {
            QVariantMap {{"key", "a"}, {"start", 3600.0}, {"duration", 300.0}},
            QVariantMap {{"key", "b"}, {"start", 3900.0}, {"duration", 120.5}},
            QVariantMap {{"key", "c"}, {"start", 3700.0}, {"duration", 30.0}},
        };
        using timecode::under;
        const auto key = [&](double seconds) { return under(rows, seconds * 30, timecode::Fps30, false).value("key").toString(); };
        QCOMPARE(key(3599.99), QString());
        QCOMPARE(key(3600), QStringLiteral("a"));
        QCOMPARE(under(rows, 3650.5 * 30, timecode::Fps30, false).value("position").toDouble(), 50.5);
        // Two hold it: the one that started later
        QCOMPARE(key(3710), QStringLiteral("c"));
        QCOMPARE(key(3731), QStringLiteral("a"));
        QCOMPARE(key(3899.99), QStringLiteral("a"));
        QCOMPARE(key(3900), QStringLiteral("b"));
        QCOMPARE(key(4020.4), QStringLiteral("b"));
        QCOMPARE(key(4020.6), QString());
        QVERIFY(under({}, 100, timecode::Fps30, false).isEmpty());
        // At 29.97 the place is real time: sixty seconds of timecode that drops no
        // frames, past a start of an hour, is 60.06 seconds; of one that does, 59.99..
        const QVariantList one {QVariantMap {{"key", "a"}, {"start", 3600.0}, {"duration", 300.0}}};
        const double plain = under(one, double(timecode::frameNumber({1, 1, 0, 0}, timecode::Fps2997, false)), timecode::Fps2997, false).value("position").toDouble();
        QVERIFY2(std::abs(plain - 60.06) < 1e-6, qPrintable(QString::number(plain, 'f', 6)));
        const double dropping = under(one, double(timecode::frameNumber({1, 1, 0, 2}, timecode::Fps2997, true)), timecode::Fps2997, true).value("position").toDouble();
        QVERIFY2(std::abs(dropping - 60.06) < 1e-6, qPrintable(QString::number(dropping, 'f', 6)));
    }
};

QTEST_APPLESS_MAIN(TestTimecode)
#include "tst_timecode.moc"
