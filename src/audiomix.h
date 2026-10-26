#pragma once

#include "audiorouting.h"

#include <QtGlobal>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

struct SwrContext;

// The app's own mixer, as plain C++: the sound of every player brought to the device's
// rate, sent to the app's channels by where its own channels go, the app's channels
// muted, soloed or given a test tone, and sent to the device's channels by the device's
// matrix (audiorouting.h has the rules of all that). Nothing here knows of a sound
// device or of a player: AudioMixer (audiomixer.h) gives this a device to fill and the
// players' sound to fill it with. So it is tried by itself, a stretch at a time, in
// tests/unit/tst_audiomix.cpp.
//
// Why the app mixes its own sound. The system's players each send their sound to a
// device of the system's as it is, the first two of a file's channels to the device's
// first two; they cannot send a file's third channel to a device's fifth, nor two of the
// app's channels to one of the device's. ProPresenter can (support.renewedvision.com,
// "Audio Routing in ProPresenter"), and so this does it itself. The maintainer chose it
// over keeping the system's players for what needs no routing: all the app's sound goes
// one way, at the cost of a little more delay (below).
//
// Three threads meet here, and none ever waits for another:
//
//   - A player's own thread hands over its sound as it decodes it, at the moment it is
//     due (Qt plays it out to a buffer output in time with its picture: tried
//     2026-10-25), stretches of about a fiftieth of a second, in the file's own rate
//     and channels. Feed::push brings each to the device's rate (FFmpeg's
//     libswresample) and puts it in the feed's ring.
//   - The device's thread asks for sound when it needs it: Mixer::mix takes what each
//     feed's ring has and mixes it. A ring has one writer and one reader, and is kept
//     by two counters, so that neither waits.
//   - The operator window's thread changes what is heard: a feed's loudness (a number
//     set at once), and the routing and the channels (changed under a lock that the
//     mixer only tries: if it is held at that moment, the mixer goes on with what it
//     had, and takes the change a stretch later).
//
// The delay, and keeping in step. A player sends its sound a stretch at a time, as each
// is due, and the device takes it a stretch at a time, as it needs it: a twentieth of a
// second or so each for a video's sound (1024 frames), but a tenth for a WAV file's
// (4096), and the system's sound service asks for 2048 frames at once from some devices
// (PipeWire's sink with no hardware behind it, which the tests use: seen 2026-10-25).
// So a feed is not mixed until its ring holds the largest stretch its player has sent
// and the largest the device has asked for, and a little more, and at least kLeast
// (primed): then the device never finds it empty between two of the player's. Its
// sound is heard that much after its picture, plus what the device holds; that is the
// cost of mixing here (Qt's own players play their sound out ahead of time to make up
// for the device's part; a buffer output gives sound only when it is due). The log says
// how much it is for each player. And the players go by
// the computer's clock while the device goes by its own, which runs a little fast or
// slow (some hundredths of a percent): left alone, a ring would fill or empty over an
// hour. So each feed watches how full its ring keeps, on average, from a moment after it
// was primed, and brings its sound to the device's rate that much faster or slower
// (swr_set_compensation: a few hundredths of a percent, which is no change of pitch
// anyone can hear) to keep it there. A ring that empties all the same (the player held
// up) is primed again before it is heard again; one that fills far past where it should
// (the device held up) has what is too old let go of.
//
// What it costs: for each player, its sound gone through twice (brought to rate, then
// mixed into the channels it goes to), and the channels once to the device, about two
// hundred times a second in stretches of a few hundred: some millionths of a second each
// time, on threads that are not the one that draws. Nothing at all while nothing plays:
// AudioMixer closes the device then.
namespace audiomix {

constexpr int kMostBuses = audiorouting::kMostChannels;
// The most channels of a device that are filled, and of a file that are heard (more
// than any device or file the maintainer has seen: the rest are silent)
constexpr int kMostDeviceChannels = 32;
constexpr int kMostFeedChannels = 32;
// The least sound a feed's ring holds before it is heard, in seconds, however small the
// stretches it is sent in and taken in (see above); the log says if a ring ever runs dry
constexpr double kLeast = 0.040;
// The test tone: ProPresenter's T. A GUESS at its pitch and its loudness (a sine of
// 1 kHz at 20 dB below full, a mixing desk's usual tone).
constexpr double kToneHz = 1000;
constexpr float kToneLevel = 0.1f;

// The sound of a feed waiting to be mixed: frames of its channels side by side, written
// by one thread and read by another (see above).
class Ring
{
public:
    // Made by the writer before it first writes, and never again
    void allocate(int channels, int frames);
    bool allocated() const { return m_ready.load(std::memory_order_acquire); }
    int channels() const { return m_channels; }
    // How many frames are waiting
    int fill() const;
    // As many of `frames` as there is room for; the rest are let go of. Answers how
    // many went in.
    int write(const float *frames, int count);
    // Up to `count` frames into `into`; answers how many there were
    int read(float *into, int count);
    void skip(int count);

private:
    std::vector<float> m_samples;
    int m_channels = 0;
    int m_capacity = 0;
    std::atomic<qint64> m_written {0};
    std::atomic<qint64> m_read {0};
    std::atomic<bool> m_ready {false};
};

// Where a feed's channels go, and how loud each is (audiorouting::soundOf)
struct Settings
{
    // Whether its channels go where `outputs` says, or by ProPresenter's Auto
    bool custom = false;
    audiorouting::Routes outputs;
    // Each of its channels' gain (audiorouting::gainOf); one it does not have is 1
    std::vector<float> gains;
};

// One player's sound on its way to the mixer
class Feed
{
public:
    Feed();
    ~Feed();
    Feed(const Feed &) = delete;
    Feed &operator=(const Feed &) = delete;

    // From the operator window's thread
    //
    // How loud, from 0: its volume and its fade, and its ramp, together. A change is
    // made over the next stretch, not at once, so that it does not click.
    void setGain(float gain) { m_gain.store(gain, std::memory_order_relaxed); }
    void setSettings(const Settings &settings);
    // Its ramps (see AudioFeed): coming in over `rampIn` seconds, `sinceStart` seconds
    // into its playing now; going out over the last `rampOut`, `untilEnd` seconds before
    // its end now (less than 0 for none). Given again each time its player says where it
    // is; between, the mixer goes on by what it has mixed, so that a ramp is smooth and
    // not in steps of a twentieth of a second (how often Qt's player says where it is).
    void setRamps(double rampIn, double sinceStart, double rampOut, double untilEnd);
    // It is done with: what is left of it fades out over a stretch, and the mixer lets
    // go of it
    void leave() { m_leaving.store(true, std::memory_order_relaxed); }
    // How many times its sound broke off while it was heard and came back within a
    // quarter of a second (its player late with it, or sent elsewhere in its file: a
    // player that stopped is not counted), and how many frames it has had to let go of,
    // since this was last asked
    int takeUnderruns() { return m_underruns.exchange(0, std::memory_order_relaxed); }
    int takeDropped() { return m_dropped.exchange(0, std::memory_order_relaxed); }
    // How much it held, in frames, when it was last first heard: how far behind its
    // player it is heard (with what the device holds); 0 before
    int heldWhenHeard() const { return m_heldWhenHeard.load(std::memory_order_relaxed); }

    // From the player's thread
    //
    // A stretch of its sound: `frames` frames of `channels` channels side by side, at
    // `rate`
    void push(const float *samples, int frames, int channels, int rate);
    // Its sound has come to its end (what is left in the ring is heard without waiting
    // for more)
    void pushEnd() { m_ended.store(true, std::memory_order_relaxed); }

private:
    friend class Mixer;

    // Both
    Ring m_ring;
    std::atomic<float> m_gain {0};
    std::atomic<bool> m_leaving {false};
    std::atomic<bool> m_ended {false};
    // The device's rate, which the ring is at: set by the mixer
    std::atomic<int> m_outRate {48000};
    // Whether the mixer is taking from it, which is when the writer keeps it in step
    std::atomic<bool> m_primed {false};
    std::atomic<qint64> m_pushes {0};
    std::atomic<int> m_underruns {0};
    std::atomic<int> m_dropped {0};
    // The most frames it has been sent at once (at the device's rate)
    std::atomic<int> m_largestPush {0};
    std::atomic<int> m_heldWhenHeard {0};
    std::mutex m_settingsLock;
    Settings m_settings;
    std::atomic<int> m_settingsVersion {0};
    struct Ramps
    {
        double in = 0;
        double sinceStart = 0;
        double out = 0;
        double untilEnd = -1;
    };
    Ramps m_ramps;
    std::atomic<int> m_rampsVersion {0};

    // The writer's own
    SwrContext *m_swr = nullptr;
    int m_swrInRate = 0;
    int m_swrChannels = 0;
    int m_swrOutRate = 0;
    std::vector<float> m_converted;
    std::vector<float> m_narrowed;
    // How full the ring keeps on average while it is mixed, and where it settled a
    // moment after it was primed (-1 before)
    double m_averageFill = -1;
    double m_settledFill = -1;
    int m_sincePrimed = 0;
    int m_compensation = 0;

    // The mixer's own
    bool m_heard = false;
    float m_lastGain = 0;
    int m_matrixVersion = -1;
    int m_matrixBuses = -1;
    int m_matrixChannels = -1;
    struct Pair
    {
        int channel;
        int bus;
        float gain;
    };
    std::vector<Pair> m_pairs;
    qint64 m_pushesSeen = -1;
    int m_quietFrames = 0;
    // How long since it ran dry while it was heard, in frames; -1 if it has not, or that
    // was too long ago to have been a gap
    int m_dryFrames = -1;
    bool m_done = false;
    // Its ramps as the mixer has them, gone on by what it has mixed since they were
    // last given
    Ramps m_mixRamps;
    int m_rampsSeen = 0;
    bool m_rampsKnown = false;
    // How far behind what its player last said the mixer's ramp is, in seconds (less
    // than 0 for ahead), still to be caught up
    double m_rampBehind = 0;
};

// The app's channels and where they go: the workspace's and this computer's
struct Config
{
    int buses = 2;
    std::vector<bool> muted;
    std::vector<bool> soloed;
    std::vector<bool> tone;
    int deviceChannels = 2;
    // Where each of the app's channels goes, by the device's channels
    audiorouting::Routes deviceRoutes {{0}, {1}};
};

class Mixer
{
public:
    explicit Mixer(int rate = 48000);

    // From the operator window's thread
    //
    // The device's rate, which every feed is brought to (set before the device is
    // opened, so that a feed's first sound is at the rate it will be heard at)
    void setRate(int rate);
    // From the device's thread, as the device is started: what it takes at a time is to
    // be found out again
    void restart();
    int rate() const { return m_rate.load(std::memory_order_relaxed); }
    void setConfig(const Config &config);
    // A feed to be mixed until it has left and faded
    void add(const std::shared_ptr<Feed> &feed);
    // How many feeds it has, those fading out among them
    int feeds();
    // The loudest the device's channels have been since this was last asked, from 0 to 1
    // (and over, for sound that is clipped), its odd-numbered channels on the left and
    // its even-numbered on the right (its only channel on both, for a device with one):
    // for the meter
    void takePeaks(float *left, float *right);

    // From the device's thread
    //
    // `frames` frames of the device's channels, side by side, into `out`
    void mix(float *out, int frames);

private:
    void refresh();
    void mixPart(float *out, int frames);
    void rebuildPairs(Feed *feed);

    std::atomic<int> m_rate;
    std::mutex m_lock;
    Config m_config;
    std::vector<std::shared_ptr<Feed>> m_feeds;
    std::atomic<int> m_version {0};
    std::atomic<int> m_feedCount {0};
    // The most frames the device has asked for at once, and how many times it has asked
    // since it was started
    std::atomic<int> m_largestRead {0};
    std::atomic<int> m_readsSinceStart {0};

    // The device's thread's own
    int m_seenVersion = -1;
    Config m_active;
    std::vector<bool> m_heard;
    std::vector<std::shared_ptr<Feed>> m_mixing;
    std::vector<float> m_buses;
    std::vector<float> m_scratch;
    double m_tonePhase = 0;
    std::atomic<float> m_peakLeft {0};
    std::atomic<float> m_peakRight {0};
};

} // namespace audiomix
