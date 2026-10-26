// The app's own mixer (src/audiomix.h), tried without a sound device or a player: sound
// pushed into feeds as a player's thread would, and mixed a stretch at a time as a
// device's thread would ask for it. What is heard where, by the routing, Mute, Solo and
// the test tone; channels' gains; a file at another rate brought to the device's;
// nothing heard until a feed has its fill (what a player hands over at once and the
// device takes at once, the device's first read, which fills Qt's own buffer, not
// counted), and a short sound heard all the same once it has ended; a feed that leaves faded out and let go of; and a device whose clock runs
// fast kept up with, with no gap, for long enough to have run a ring dry without.

#include "audiomix.h"

#include <QTest>

#include <cmath>

using namespace audiomix;

namespace {

// A stretch of `frames` frames of `channels` channels, each channel at its own steady
// level (the first at `level`, the next at half that, and so on)
std::vector<float> steady(int frames, int channels, float level)
{
    std::vector<float> samples(size_t(frames) * channels);
    for (int frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel)
            samples[size_t(frame) * channels + channel] = level / float(1 << channel);
    }
    return samples;
}

// The device's channel `channel` of `out`, in the middle of it (away from where the
// rate converter starts)
float middle(const std::vector<float> &out, int deviceChannels, int channel)
{
    const size_t frames = out.size() / deviceChannels;
    return out[(frames / 2) * deviceChannels + channel];
}

// `frames` frames sent as a player sends them, in stretches of 960 (a fiftieth of a
// second)
void sent(Feed &feed, const std::vector<float> &samples, int frames, int channels, int rate = 48000)
{
    for (int at = 0; at < frames; at += 960)
        feed.push(samples.data() + size_t(at) * channels, qMin(960, frames - at), channels, rate);
}

std::shared_ptr<Feed> fed(Mixer &mixer, int channels, float level, float gain = 1, int frames = 4800, int rate = 48000)
{
    auto feed = std::make_shared<Feed>();
    feed->setGain(gain);
    mixer.add(feed);
    sent(*feed, steady(frames, channels, level), frames, channels, rate);
    return feed;
}

std::vector<float> mixed(Mixer &mixer, int frames, int deviceChannels)
{
    std::vector<float> out(size_t(frames) * deviceChannels, -9);
    mixer.mix(out.data(), frames);
    return out;
}

Config config(int buses, int deviceChannels, audiorouting::Routes deviceRoutes)
{
    Config config;
    config.buses = buses;
    config.deviceChannels = deviceChannels;
    config.deviceRoutes = deviceRoutes;
    return config;
}

} // namespace

class TestAudioMix : public QObject
{
    Q_OBJECT

private slots:
    void stereoAsItIs()
    {
        Mixer mixer;
        fed(mixer, 2, 0.5f);
        const std::vector<float> out = mixed(mixer, 1024, 2);
        QVERIFY(qAbs(middle(out, 2, 0) - 0.5f) < 0.01f);
        QVERIFY(qAbs(middle(out, 2, 1) - 0.25f) < 0.01f);
        float left = 0;
        float right = 0;
        mixer.takePeaks(&left, &right);
        QVERIFY(qAbs(left - 0.5f) < 0.02f);
        QVERIFY(qAbs(right - 0.25f) < 0.02f);
    }

    void routed()
    {
        // Four channels of the app's, a device of eight: the app's third channel to the
        // device's sixth and seventh
        Mixer mixer;
        mixer.setConfig(config(4, 8, {{0}, {1}, {5, 6}, {}}));
        auto feed = std::make_shared<Feed>();
        Settings settings;
        settings.custom = true;
        settings.outputs = {{2}, {}};
        feed->setSettings(settings);
        feed->setGain(1);
        mixer.add(feed);
        sent(*feed, steady(4800, 2, 0.5f), 4800, 2);
        const std::vector<float> out = mixed(mixer, 1024, 8);
        for (int channel = 0; channel < 8; ++channel) {
            const float wanted = channel == 5 || channel == 6 ? 0.5f : 0.0f;
            QVERIFY2(qAbs(middle(out, 8, channel) - wanted) < 0.01f, qPrintable(QString::number(channel)));
        }
    }

    void autoMapsRoundTheAppsChannels()
    {
        // Six channels of a file, two of the app's: 1, 3, 5 to the first and 2, 4, 6 to
        // the second, as ProPresenter's Auto
        Mixer mixer;
        fed(mixer, 6, 0.32f);
        const std::vector<float> out = mixed(mixer, 1024, 2);
        QVERIFY(qAbs(middle(out, 2, 0) - (0.32f + 0.08f + 0.02f)) < 0.01f);
        QVERIFY(qAbs(middle(out, 2, 1) - (0.16f + 0.04f + 0.01f)) < 0.01f);
    }

    void monoToBoth()
    {
        Mixer mixer;
        fed(mixer, 1, 0.5f);
        const std::vector<float> out = mixed(mixer, 1024, 2);
        QVERIFY(qAbs(middle(out, 2, 0) - 0.5f) < 0.01f);
        QVERIFY(qAbs(middle(out, 2, 1) - 0.5f) < 0.01f);
    }

    void muteSoloTone()
    {
        Mixer mixer;
        Config set = config(2, 2, {{0}, {1}});
        set.muted = {true, false};
        mixer.setConfig(set);
        fed(mixer, 2, 0.5f, 1, 48000);
        std::vector<float> out = mixed(mixer, 1024, 2);
        QCOMPARE(middle(out, 2, 0), 0.0f);
        QVERIFY(qAbs(middle(out, 2, 1) - 0.25f) < 0.01f);
        // Solo the first, muted: nothing; the second soloed: only it
        set.muted = {false, false};
        set.soloed = {false, true};
        mixer.setConfig(set);
        out = mixed(mixer, 1024, 2);
        QCOMPARE(middle(out, 2, 0), 0.0f);
        QVERIFY(qAbs(middle(out, 2, 1) - 0.25f) < 0.01f);
        // A tone on the first, with nothing else heard there: a sine of 1 kHz at 0.1
        set.soloed = {false, false};
        set.muted = {true, false};
        set.tone = {true, false};
        mixer.setConfig(set);
        out = mixed(mixer, 960, 2);
        float most = 0;
        int crossings = 0;
        for (int frame = 1; frame < 960; ++frame) {
            most = std::max(most, std::abs(out[size_t(frame) * 2]));
            if ((out[size_t(frame - 1) * 2] < 0) != (out[size_t(frame) * 2] < 0))
                ++crossings;
        }
        QVERIFY(qAbs(most - kToneLevel) < 0.002f);
        // (A fiftieth of a second of 1 kHz crosses nought forty times)
        QVERIFY(qAbs(crossings - 40) <= 1);
    }

    void channelGainsAndLoudness()
    {
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        Settings settings;
        settings.gains = {0.5f, 0.0f};
        feed->setSettings(settings);
        feed->setGain(0.5f);
        mixer.add(feed);
        // (A stretch sent before each asked for, as a player and a device go)
        const std::vector<float> samples = steady(1024, 2, 0.8f);
        const auto next = [&] {
            feed->push(samples.data(), 1024, 2, 48000);
            return mixed(mixer, 1024, 2);
        };
        // (Until it holds a stretch of each, and a little more: two of them, and the
        // one sent before the first that is asked for)
        feed->push(samples.data(), 1024, 2, 48000);
        feed->push(samples.data(), 1024, 2, 48000);
        std::vector<float> out = next();
        QVERIFY(qAbs(middle(out, 2, 0) - 0.2f) < 0.01f);
        QCOMPARE(middle(out, 2, 1), 0.0f);
        // A change of loudness is made over the next stretch, evenly: half way at its middle
        feed->setGain(1);
        out = next();
        QVERIFY(qAbs(middle(out, 2, 0) - 0.3f) < 0.01f);
        out = next();
        QVERIFY(qAbs(middle(out, 2, 0) - 0.4f) < 0.01f);
    }

    void broughtToRate()
    {
        // 440 Hz at 44.1 kHz, heard at 48 kHz: still 440 Hz, as loud
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        mixer.add(feed);
        std::vector<float> samples(44100 * 2);
        for (int frame = 0; frame < 44100; ++frame)
            samples[size_t(frame) * 2] = samples[size_t(frame) * 2 + 1] = 0.5f * float(std::sin(2 * M_PI * 440 * frame / 44100.0));
        // Stretches of 1024 sent, and as many as they come to at 48 kHz asked for
        std::vector<float> heard;
        for (int at = 0; at + 1024 <= 44100; at += 1024) {
            feed->push(samples.data() + size_t(at) * 2, 1024, 2, 44100);
            const std::vector<float> out = mixed(mixer, 1114, 2);
            heard.insert(heard.end(), out.begin(), out.end());
        }
        int crossings = 0;
        float most = 0;
        // A tenth of a second from the middle
        const size_t from = heard.size() / 4;
        for (size_t frame = from + 1; frame < from + 4800; ++frame) {
            most = std::max(most, std::abs(heard[frame * 2]));
            if ((heard[(frame - 1) * 2] < 0) != (heard[frame * 2] < 0))
                ++crossings;
        }
        // (A tenth of a second of 440 Hz crosses nought 88 times)
        QVERIFY2(qAbs(crossings - 88) <= 1, qPrintable(QString::number(crossings)));
        QVERIFY(qAbs(most - 0.5f) < 0.01f);
        QCOMPARE(feed->takeUnderruns(), 0);
    }

    void heardOnceItHasItsFill()
    {
        Mixer mixer;
        auto feed = fed(mixer, 2, 0.5f, 1, 480);
        // A hundredth of a second: less than is kept, so not heard yet
        std::vector<float> out = mixed(mixer, 256, 2);
        QCOMPARE(middle(out, 2, 0), 0.0f);
        QCOMPARE(feed->heldWhenHeard(), 0);
        // Its sound has ended: what there is is heard
        feed->pushEnd();
        out = mixed(mixer, 256, 2);
        QVERIFY(qAbs(middle(out, 2, 0) - 0.5f) < 0.02f);
        QCOMPARE(feed->takeUnderruns(), 0);
    }

    void holdsWhatItIsSentInAndAskedIn()
    {
        // A WAV file's stretches, a tenth of a second each (4096 frames at 44.1 kHz), and
        // a device that asks for 2048 frames at once: not heard until it holds one of
        // each and a little more, and then never found empty between two stretches
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        mixer.add(feed);
        const std::vector<float> samples = steady(4096, 2, 0.5f);
        std::vector<float> out(2048 * 2);
        int silent = 0;
        bool heard = false;
        // Ten simulated seconds, in steps of a millisecond: the player sends when a
        // stretch is due, the device asks when it needs to
        double sentUntil = 0;
        double askedUntil = 0;
        for (int ms = 0; ms < 10000; ++ms) {
            const double now = ms / 1000.0;
            while (sentUntil <= now) {
                feed->push(samples.data(), 4096, 2, 44100);
                sentUntil += 4096 / 44100.0;
            }
            while (askedUntil <= now) {
                mixer.mix(out.data(), 2048);
                askedUntil += 2048 / 48000.0;
                if (out[1000 * 2] > 0.4f)
                    heard = true;
                else if (heard)
                    ++silent;
            }
        }
        QVERIFY(heard);
        QCOMPARE(silent, 0);
        QCOMPARE(feed->takeUnderruns(), 0);
        // (What it held then: a stretch of each, about 4459 and 2048 frames, and a
        // two-hundredth of a second, and no more)
        QVERIFY2(qAbs(feed->heldWhenHeard() - (4459 + 2048 + 240)) <= 2, qPrintable(QString::number(feed->heldWhenHeard())));
    }

    void theDevicesFirstFillIsNotCounted()
    {
        // Qt fills its own buffer whole as the device starts (1440 frames), and then asks
        // for what the system takes at a time (1024): a feed is kept for the second
        Mixer mixer;
        std::vector<float> out(1440 * 2);
        mixer.restart();
        mixer.mix(out.data(), 1440);
        for (int read = 0; read < 3; ++read)
            mixer.mix(out.data(), 1024);
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        mixer.add(feed);
        const std::vector<float> samples = steady(1024, 2, 0.5f);
        for (int stretch = 0; stretch < 6; ++stretch) {
            feed->push(samples.data(), 1024, 2, 48000);
            mixer.mix(out.data(), 1024);
        }
        // A stretch of the player's and of the device's, and a two-hundredth of a second
        QCOMPARE(feed->heldWhenHeard(), 1024 + 1024 + 240);
        QCOMPARE(feed->takeUnderruns(), 0);
    }

    void meterIsWhatTheDeviceIsSent()
    {
        // The app's third and fourth channels, on a device of two: by Auto they go
        // nowhere, and nothing is metered; sent to the device's two, they are
        Mixer mixer;
        mixer.setConfig(config(4, 2, {{0}, {1}, {}, {}}));
        auto feed = std::make_shared<Feed>();
        Settings settings;
        settings.custom = true;
        settings.outputs = {{2}, {3}};
        feed->setSettings(settings);
        feed->setGain(1);
        mixer.add(feed);
        sent(*feed, steady(9600, 2, 0.5f), 9600, 2);
        std::vector<float> out = mixed(mixer, 1024, 2);
        float left = 1;
        float right = 1;
        mixer.takePeaks(&left, &right);
        QCOMPARE(middle(out, 2, 0), 0.0f);
        QCOMPARE(left, 0.0f);
        QCOMPARE(right, 0.0f);
        mixer.setConfig(config(4, 2, {{0}, {1}, {0}, {1}}));
        out = mixed(mixer, 1024, 2);
        mixer.takePeaks(&left, &right);
        QVERIFY(qAbs(middle(out, 2, 0) - 0.5f) < 0.01f);
        QVERIFY(qAbs(left - 0.5f) < 0.02f);
        QVERIFY(qAbs(right - 0.25f) < 0.02f);
    }

    void gapsAreCountedStopsAreNot()
    {
        // Heard, and then its player stops sending (stopped, or paused): what is left is
        // heard and then it is silent, and that is no gap
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        mixer.add(feed);
        const std::vector<float> samples = steady(960, 2, 0.5f);
        for (int stretch = 0; stretch < 10; ++stretch) {
            feed->push(samples.data(), 960, 2, 48000);
            mixed(mixer, 960, 2);
        }
        for (int stretch = 0; stretch < 50; ++stretch)
            mixed(mixer, 960, 2);
        QCOMPARE(feed->takeUnderruns(), 0);
        // Sent again much later (played again): still no gap
        for (int stretch = 0; stretch < 5; ++stretch) {
            feed->push(samples.data(), 960, 2, 48000);
            mixed(mixer, 960, 2);
        }
        QCOMPARE(feed->takeUnderruns(), 0);
        // Late: nothing sent for a tenth of a second while it was heard, then on again
        for (int stretch = 0; stretch < 6; ++stretch)
            mixed(mixer, 960, 2);
        for (int stretch = 0; stretch < 5; ++stretch) {
            feed->push(samples.data(), 960, 2, 48000);
            mixed(mixer, 960, 2);
        }
        QCOMPARE(feed->takeUnderruns(), 1);
    }

    void leavesFadingOut()
    {
        Mixer mixer;
        auto feed = fed(mixer, 2, 0.5f, 1, 48000);
        mixed(mixer, 1024, 2);
        QCOMPARE(mixer.feeds(), 1);
        feed->leave();
        // Faded out over the next stretch, and then let go of
        const std::vector<float> out = mixed(mixer, 1024, 2);
        QVERIFY(qAbs(middle(out, 2, 0) - 0.25f) < 0.01f);
        QVERIFY(std::abs(out[1023 * 2]) < 0.01f);
        mixed(mixer, 1024, 2);
        QCOMPARE(mixer.feeds(), 0);
    }

    void neverPastFull()
    {
        Mixer mixer;
        fed(mixer, 2, 0.8f);
        fed(mixer, 2, 0.8f);
        const std::vector<float> out = mixed(mixer, 1024, 2);
        QCOMPARE(middle(out, 2, 0), 1.0f);
        QVERIFY(qAbs(middle(out, 2, 1) - 0.8f) < 0.01f);
    }

    void rampsAreSmooth()
    {
        // Coming in over a second, given again every twentieth of a second with where its
        // player is, a little off as a player's word is: no step anywhere, from nothing
        // to all of it
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        feed->setRamps(1, 0, 0, -1);
        mixer.add(feed);
        const std::vector<float> samples = steady(960, 2, 0.5f);
        std::vector<float> heard;
        for (int stretch = 0; stretch < 75; ++stretch) {
            feed->push(samples.data(), 960, 2, 48000);
            // (What it has sent, give or take a hundredth of a second)
            if (stretch % 3 == 0)
                feed->setRamps(1, (stretch + 1) * 0.02 + (stretch % 2 ? 0.01 : -0.01), 0, -1);
            const std::vector<float> out = mixed(mixer, 960, 2);
            heard.insert(heard.end(), out.begin(), out.end());
        }
        float biggestStep = 0;
        // (From the second stretch, when it is first heard)
        for (size_t frame = 960 * 2 + 1; frame < heard.size() / 2; ++frame)
            biggestStep = std::max(biggestStep, std::abs(heard[frame * 2] - heard[(frame - 1) * 2]));
        QVERIFY(std::abs(heard[960 * 2 * 2]) < 0.05f);
        QVERIFY(qAbs(heard.back() - 0.25f) < 0.01f);
        QVERIFY(qAbs(heard[heard.size() - 2] - 0.5f) < 0.01f);
        // A smooth second-long rise of 0.5 moves a few hundred-thousandths a frame; a
        // step of a hundredth of a second would be a jump of a hundred times that
        QVERIFY2(biggestStep < 0.0005f, qPrintable(QString::number(biggestStep)));

        // Going out over its last half second, its end said to be half a second away:
        // silent at the end, and still not past it
        feed->setRamps(0, 10, 0.5, 0.5);
        heard.clear();
        for (int stretch = 0; stretch < 40; ++stretch) {
            feed->push(samples.data(), 960, 2, 48000);
            const std::vector<float> out = mixed(mixer, 960, 2);
            heard.insert(heard.end(), out.begin(), out.end());
        }
        QVERIFY(std::abs(heard[0] - 0.5f) < 0.05f);
        QCOMPARE(heard.back(), 0.0f);
    }

    void keptInStep()
    {
        // A player's clock and a device's that runs a tenth of a percent fast: for every
        // thousand frames sent, a thousand and one asked for. Over forty simulated
        // seconds a ring kept at 40 ms would run dry twice over; kept in step, it never
        // does, and stays about where it settled.
        Mixer mixer;
        auto feed = std::make_shared<Feed>();
        feed->setGain(1);
        mixer.add(feed);
        const std::vector<float> samples = steady(1000, 2, 0.5f);
        std::vector<float> out(1001 * 2);
        int silent = 0;
        for (int stretch = 0; stretch < 2000; ++stretch) {
            feed->push(samples.data(), 1000, 2, 48000);
            mixer.mix(out.data(), 1001);
            // (Heard from the second stretch, when it has its fill)
            if (stretch >= 2 && out[500 * 2] < 0.4f)
                ++silent;
        }
        QCOMPARE(silent, 0);
        QCOMPARE(feed->takeUnderruns(), 0);
        QCOMPARE(feed->takeDropped(), 0);
    }
};

QTEST_GUILESS_MAIN(TestAudioMix)
#include "tst_audiomix.moc"
