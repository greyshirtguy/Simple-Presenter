#include "audiomix.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <cmath>
#include <cstring>

namespace audiomix {

namespace {

// How many frames are mixed at a time: what the device asks for at once is mixed in
// parts of this many, so that what is kept for mixing is of a size known beforehand
constexpr int kPart = 1024;

} // namespace

// ---- Ring

void Ring::allocate(int channels, int frames)
{
    m_channels = qMax(1, channels);
    m_capacity = qMax(1, frames);
    m_samples.assign(size_t(m_channels) * size_t(m_capacity), 0.0f);
    m_ready.store(true, std::memory_order_release);
}

int Ring::fill() const
{
    return int(m_written.load(std::memory_order_acquire) - m_read.load(std::memory_order_acquire));
}

int Ring::write(const float *frames, int count)
{
    const qint64 written = m_written.load(std::memory_order_relaxed);
    const qint64 read = m_read.load(std::memory_order_acquire);
    const int n = qBound(0, count, m_capacity - int(written - read));
    // In at most two pieces: to the end of the ring, and on from its start
    const int at = int(written % m_capacity);
    const int first = qMin(n, m_capacity - at);
    std::memcpy(&m_samples[size_t(at) * m_channels], frames, sizeof(float) * size_t(first) * m_channels);
    if (n > first)
        std::memcpy(m_samples.data(), frames + size_t(first) * m_channels, sizeof(float) * size_t(n - first) * m_channels);
    m_written.store(written + n, std::memory_order_release);
    return n;
}

int Ring::read(float *into, int count)
{
    const qint64 read = m_read.load(std::memory_order_relaxed);
    const qint64 written = m_written.load(std::memory_order_acquire);
    const int n = qBound(0, count, int(written - read));
    const int at = int(read % m_capacity);
    const int first = qMin(n, m_capacity - at);
    std::memcpy(into, &m_samples[size_t(at) * m_channels], sizeof(float) * size_t(first) * m_channels);
    if (n > first)
        std::memcpy(into + size_t(first) * m_channels, m_samples.data(), sizeof(float) * size_t(n - first) * m_channels);
    m_read.store(read + n, std::memory_order_release);
    return n;
}

void Ring::skip(int count)
{
    const qint64 read = m_read.load(std::memory_order_relaxed);
    const qint64 written = m_written.load(std::memory_order_acquire);
    m_read.store(read + qBound(0, count, int(written - read)), std::memory_order_release);
}

// ---- Feed

Feed::Feed() = default;

Feed::~Feed()
{
    swr_free(&m_swr);
}

void Feed::setSettings(const Settings &settings)
{
    std::lock_guard lock(m_settingsLock);
    m_settings = settings;
    m_settingsVersion.fetch_add(1, std::memory_order_relaxed);
}

void Feed::setRamps(double rampIn, double sinceStart, double rampOut, double untilEnd)
{
    std::lock_guard lock(m_settingsLock);
    m_ramps = {qMax(0.0, rampIn), sinceStart, qMax(0.0, rampOut), untilEnd};
    m_rampsVersion.fetch_add(1, std::memory_order_relaxed);
}

void Feed::push(const float *samples, int frames, int channels, int rate)
{
    if (frames <= 0 || channels <= 0 || rate <= 0)
        return;
    m_ended.store(false, std::memory_order_relaxed);
    const int outRate = m_outRate.load(std::memory_order_relaxed);
    // Half a second of room: four times what is kept in it, and some, so that a device
    // held up for a moment loses nothing
    if (!m_ring.allocated())
        m_ring.allocate(qMin(channels, kMostFeedChannels), outRate / 2);
    const int ringChannels = m_ring.channels();
    // A file with more channels than are heard (or whose channels change on the way,
    // which is rare): the first are kept, and any it has not are silent
    const float *in = samples;
    if (channels != ringChannels) {
        m_narrowed.assign(size_t(frames) * ringChannels, 0.0f);
        const int kept = qMin(channels, ringChannels);
        for (int frame = 0; frame < frames; ++frame) {
            for (int channel = 0; channel < kept; ++channel)
                m_narrowed[size_t(frame) * ringChannels + channel] = samples[size_t(frame) * channels + channel];
        }
        in = m_narrowed.data();
    }

    if (!m_swr || m_swrInRate != rate || m_swrOutRate != outRate || m_swrChannels != ringChannels) {
        swr_free(&m_swr);
        // The same channels in as out, in an order that is no one's in particular, so
        // that none is mixed into another: where each goes is the mixer's to say.
        AVChannelLayout layout {};
        layout.order = AV_CHANNEL_ORDER_UNSPEC;
        layout.nb_channels = ringChannels;
        if (swr_alloc_set_opts2(&m_swr, &layout, AV_SAMPLE_FMT_FLT, outRate, &layout, AV_SAMPLE_FMT_FLT, rate, 0, nullptr) < 0) {
            swr_free(&m_swr);
            return;
        }
        // Brought to rate even where the rates are the same, so that it can be kept in
        // step from its first stretch (otherwise the first change would start it again)
        av_opt_set_int(m_swr, "flags", SWR_FLAG_RESAMPLE, 0);
        if (swr_init(m_swr) < 0) {
            swr_free(&m_swr);
            return;
        }
        m_swrInRate = rate;
        m_swrOutRate = outRate;
        m_swrChannels = ringChannels;
        m_compensation = 0;
    }

    // Kept in step with the device (see the header): from a moment after the mixer has
    // begun to take from it, by where its ring settled then.
    if (m_primed.load(std::memory_order_relaxed)) {
        const int fill = m_ring.fill();
        m_averageFill = m_averageFill < 0 ? fill : m_averageFill + (fill - m_averageFill) * 0.02;
        // (A hundred stretches: two seconds or so, long enough for the average to have
        // settled from where it started)
        if (++m_sincePrimed == 100)
            m_settledFill = m_averageFill;
        if (m_settledFill >= 0) {
            const double over = m_averageFill - m_settledFill;
            // What is over or under is made up over four seconds, by at most half a
            // percent faster or slower; a difference of under two thousandths of a
            // second is let be, so that it does not hunt.
            int delta = std::abs(over) > outRate * 0.002 ? int(std::lround(-over / 4)) : 0;
            delta = qBound(-outRate / 200, delta, outRate / 200);
            // (Given again each stretch: what it is given runs out after a second.)
            if (delta != 0 || m_compensation != 0)
                swr_set_compensation(m_swr, delta, outRate);
            m_compensation = delta;
        }
    } else if (m_averageFill >= 0) {
        m_averageFill = -1;
        m_settledFill = -1;
        m_sincePrimed = 0;
        if (m_compensation != 0)
            swr_set_compensation(m_swr, 0, outRate);
        m_compensation = 0;
    }

    const int most = swr_get_out_samples(m_swr, frames);
    if (most <= 0)
        return;
    if (m_converted.size() < size_t(most) * ringChannels)
        m_converted.resize(size_t(most) * ringChannels);
    auto *out = reinterpret_cast<uint8_t *>(m_converted.data());
    const auto *from = reinterpret_cast<const uint8_t *>(in);
    const int made = swr_convert(m_swr, &out, most, &from, frames);
    if (made > 0) {
        if (made > m_largestPush.load(std::memory_order_relaxed))
            m_largestPush.store(made, std::memory_order_relaxed);
        const int went = m_ring.write(m_converted.data(), made);
        if (went < made)
            m_dropped.fetch_add(made - went, std::memory_order_relaxed);
    }
    m_pushes.fetch_add(1, std::memory_order_relaxed);
}

// ---- Mixer

Mixer::Mixer(int rate)
    : m_rate(qMax(1, rate))
{
    m_buses.assign(size_t(kMostBuses) * kPart, 0.0f);
    m_scratch.assign(size_t(kMostFeedChannels) * kPart, 0.0f);
    m_feeds.reserve(64);
    m_mixing.reserve(64);
    m_heard = {true, true};
    m_active.muted = m_active.soloed = m_active.tone = {false, false};
}

void Mixer::setRate(int rate)
{
    rate = qMax(1, rate);
    m_rate.store(rate, std::memory_order_relaxed);
    std::lock_guard lock(m_lock);
    for (const auto &feed : m_feeds)
        feed->m_outRate.store(rate, std::memory_order_relaxed);
}

void Mixer::setConfig(const Config &config)
{
    Config fit = config;
    fit.buses = qBound(1, fit.buses, kMostBuses);
    fit.deviceChannels = qBound(1, fit.deviceChannels, kMostDeviceChannels);
    fit.muted.resize(size_t(fit.buses), false);
    fit.soloed.resize(size_t(fit.buses), false);
    fit.tone.resize(size_t(fit.buses), false);
    fit.deviceRoutes = audiorouting::fitted(fit.deviceRoutes, fit.buses, fit.deviceChannels);
    std::lock_guard lock(m_lock);
    m_config = fit;
    m_version.fetch_add(1, std::memory_order_relaxed);
}

void Mixer::add(const std::shared_ptr<Feed> &feed)
{
    feed->m_outRate.store(rate(), std::memory_order_relaxed);
    std::lock_guard lock(m_lock);
    m_feeds.push_back(feed);
    m_feedCount.store(int(m_feeds.size()), std::memory_order_relaxed);
    m_version.fetch_add(1, std::memory_order_relaxed);
}

int Mixer::feeds()
{
    return m_feedCount.load(std::memory_order_relaxed);
}

void Mixer::takePeaks(float *left, float *right)
{
    *left = m_peakLeft.exchange(0, std::memory_order_relaxed);
    *right = m_peakRight.exchange(0, std::memory_order_relaxed);
}

// What the operator window has changed, if it is not changing it at this moment (in
// which case it is taken a part later); and the feeds that have faded out let go of.
void Mixer::refresh()
{
    std::unique_lock lock(m_lock, std::try_to_lock);
    if (!lock.owns_lock())
        return;
    const auto done = [](const std::shared_ptr<Feed> &feed) { return feed->m_done; };
    if (std::any_of(m_mixing.begin(), m_mixing.end(), done)) {
        m_feeds.erase(std::remove_if(m_feeds.begin(), m_feeds.end(), done), m_feeds.end());
        m_mixing.erase(std::remove_if(m_mixing.begin(), m_mixing.end(), done), m_mixing.end());
        m_feedCount.store(int(m_feeds.size()), std::memory_order_relaxed);
    }
    const int version = m_version.load(std::memory_order_relaxed);
    if (version == m_seenVersion)
        return;
    m_seenVersion = version;
    m_active = m_config;
    m_mixing = m_feeds;
    const QList<bool> heard = audiorouting::heard(QList<bool>(m_active.muted.begin(), m_active.muted.end()),
                                                  QList<bool>(m_active.soloed.begin(), m_active.soloed.end()));
    m_heard.assign(heard.begin(), heard.end());
}

void Mixer::restart()
{
    m_readsSinceStart.store(0, std::memory_order_relaxed);
    m_largestRead.store(0, std::memory_order_relaxed);
}

void Mixer::mix(float *out, int frames)
{
    // (Not the device's first: Qt fills its own buffer whole as the device starts, before
    // any sound has come, and asks for no more than the system takes at a time after.)
    if (m_readsSinceStart.fetch_add(1, std::memory_order_relaxed) > 0 && frames > m_largestRead.load(std::memory_order_relaxed))
        m_largestRead.store(frames, std::memory_order_relaxed);
    refresh();
    const int deviceChannels = m_active.deviceChannels;
    std::fill(out, out + size_t(frames) * deviceChannels, 0.0f);
    for (int done = 0; done < frames; done += kPart)
        mixPart(out + size_t(done) * deviceChannels, qMin(kPart, frames - done));
}

// Where each of a feed's channels goes among the app's, and how loud: from its settings,
// or by ProPresenter's Auto while it has none of its own
void Mixer::rebuildPairs(Feed *feed)
{
    std::unique_lock lock(feed->m_settingsLock, std::try_to_lock);
    // (Its settings being changed at this moment: what it had until a part later, or
    // Auto if it has had nothing yet)
    if (!lock.owns_lock() && feed->m_matrixVersion >= 0)
        return;
    const int channels = feed->m_ring.channels();
    const int buses = m_active.buses;
    const bool custom = lock.owns_lock() && feed->m_settings.custom;
    const audiorouting::Routes routes = custom ? audiorouting::fitted(feed->m_settings.outputs, channels, buses)
                                               : audiorouting::autoMedia(channels, buses);
    feed->m_pairs.clear();
    for (int channel = 0; channel < channels && channel < routes.size(); ++channel) {
        const float gain = lock.owns_lock() && size_t(channel) < feed->m_settings.gains.size() ? feed->m_settings.gains[size_t(channel)] : 1.0f;
        if (gain <= 0)
            continue;
        for (const int bus : routes[channel])
            feed->m_pairs.push_back({channel, bus, gain});
    }
    feed->m_matrixVersion = lock.owns_lock() ? feed->m_settingsVersion.load(std::memory_order_relaxed) : -1;
    feed->m_matrixBuses = buses;
    feed->m_matrixChannels = channels;
}

void Mixer::mixPart(float *out, int frames)
{
    const int rate = this->rate();
    const int buses = m_active.buses;
    const int deviceChannels = m_active.deviceChannels;
    const int least = qMax(1, int(rate * kLeast));
    const int largestRead = m_largestRead.load(std::memory_order_relaxed);
    std::fill(m_buses.begin(), m_buses.begin() + size_t(buses) * kPart, 0.0f);

    for (const auto &shared : m_mixing) {
        Feed *feed = shared.get();
        if (feed->m_done)
            continue;
        const bool leaving = feed->m_leaving.load(std::memory_order_relaxed);
        if (!feed->m_ring.allocated()) {
            if (leaving)
                feed->m_done = true;
            continue;
        }
        // Whether its player is still sending: one that has stopped sending (paused, or
        // at its end without saying so) has what is left of it heard without waiting.
        // And whether it ran dry while it was heard and then came back soon: a gap in its
        // sound, which only its coming back tells apart from a player that has stopped
        // (at the moment it runs dry the two look the same).
        const qint64 pushes = feed->m_pushes.load(std::memory_order_relaxed);
        if (pushes != feed->m_pushesSeen) {
            feed->m_pushesSeen = pushes;
            feed->m_quietFrames = 0;
            if (feed->m_dryFrames >= 0) {
                if (feed->m_dryFrames < rate / 4)
                    feed->m_underruns.fetch_add(1, std::memory_order_relaxed);
                feed->m_dryFrames = -1;
            }
        } else {
            feed->m_quietFrames += frames;
            if (feed->m_dryFrames >= 0) {
                feed->m_dryFrames += frames;
                if (feed->m_dryFrames >= rate / 4)
                    feed->m_dryFrames = -1;
            }
        }
        // What it is to hold (see the header): a stretch of its player's and of the
        // device's, and a two-hundredth of a second, or the least it holds
        const int target = qMax(least, feed->m_largestPush.load(std::memory_order_relaxed) + largestRead + rate / 200);
        const bool ended = feed->m_ended.load(std::memory_order_relaxed) || feed->m_quietFrames >= 2 * target;
        int fill = feed->m_ring.fill();
        if (!feed->m_heard) {
            if (fill >= target || (ended && fill > 0)) {
                // Heard from exactly what it is to hold, the oldest of anything more let
                // go of (the device was opening, or a stretch of its player's had just
                // come), so that it is no later than it must be. (Exactly that is enough
                // whenever it starts: the most the device can take before the player's
                // next stretch comes is its stretch and one of the device's.)
                if (fill > target) {
                    feed->m_ring.skip(fill - target);
                    fill = target;
                }
                feed->m_heard = true;
                feed->m_primed.store(true, std::memory_order_relaxed);
                feed->m_heldWhenHeard.store(fill, std::memory_order_relaxed);
                // (Heard from its first frame as loud as it is: a sound that starts
                // sharply is not softened)
                feed->m_lastGain = leaving ? 0.0f : feed->m_gain.load(std::memory_order_relaxed);
            } else {
                if (leaving)
                    feed->m_done = true;
                continue;
            }
        } else if (fill > target + int(rate * 0.15)) {
            const int over = fill - target;
            feed->m_ring.skip(over);
            feed->m_dropped.fetch_add(over, std::memory_order_relaxed);
        }

        const int channels = feed->m_ring.channels();
        if (feed->m_matrixVersion != feed->m_settingsVersion.load(std::memory_order_relaxed) || feed->m_matrixBuses != buses
            || feed->m_matrixChannels != channels)
            rebuildPairs(feed);
        // Its ramps, as last given, if they have been given again since (and are not being
        // given at this moment): where it was then, less what it had waiting, which is
        // that much before it. The player says where it is only to within a stretch of
        // sound, so where the mixer has got to is not moved to what it says, which would
        // be a small step in loudness, and heard: its ramp is run a little faster or
        // slower until it agrees (at most a tenth: below). Unless it is far out, as when
        // the player has been sent elsewhere in its file, or the ramps have changed.
        const int rampsVersion = feed->m_rampsVersion.load(std::memory_order_relaxed);
        if (rampsVersion != feed->m_rampsSeen) {
            std::unique_lock lock(feed->m_settingsLock, std::try_to_lock);
            if (lock.owns_lock()) {
                Feed::Ramps given = feed->m_ramps;
                feed->m_rampsSeen = rampsVersion;
                const double waiting = double(fill) / rate;
                given.sinceStart -= waiting;
                if (given.untilEnd >= 0)
                    given.untilEnd += waiting;
                Feed::Ramps &had = feed->m_mixRamps;
                const bool near = feed->m_rampsKnown && had.in == given.in && had.out == given.out && std::abs(given.sinceStart - had.sinceStart) < 0.25
                                  && (given.untilEnd < 0) == (had.untilEnd < 0) && std::abs(given.untilEnd - had.untilEnd) < 0.25;
                if (near) {
                    feed->m_rampBehind = given.sinceStart - had.sinceStart;
                } else {
                    had = given;
                    feed->m_rampBehind = 0;
                    feed->m_rampsKnown = true;
                }
            }
        }
        Feed::Ramps &ramps = feed->m_mixRamps;
        const bool ramped = ramps.in > 0 || ramps.out > 0;
        const float rampFrom = ramped ? float(audiorouting::rampGain(ramps.sinceStart, ramps.in, ramps.untilEnd, ramps.out)) : 1.0f;

        const int got = feed->m_ring.read(m_scratch.data(), frames);
        float rampTo = 1.0f;
        if (ramped) {
            const double played = double(got) / rate;
            const double caught = qBound(-played / 10, feed->m_rampBehind, played / 10);
            feed->m_rampBehind -= caught;
            const double seconds = played + caught;
            ramps.sinceStart += seconds;
            // (Not past its end, which is where a ramp out has left it silent)
            if (ramps.untilEnd >= 0)
                ramps.untilEnd = qMax(0.0, ramps.untilEnd - seconds);
            rampTo = float(audiorouting::rampGain(ramps.sinceStart, ramps.in, ramps.untilEnd, ramps.out));
        }
        const float gainTo = leaving ? 0.0f : feed->m_gain.load(std::memory_order_relaxed);
        const float to = gainTo * rampTo;
        const float from = feed->m_lastGain * rampFrom;
        // A change of loudness made evenly over the part, so that it does not click
        const float step = (to - from) / float(frames);
        for (const Feed::Pair &pair : feed->m_pairs) {
            float *bus = &m_buses[size_t(pair.bus) * kPart];
            const float *in = m_scratch.data() + pair.channel;
            if (from == to) {
                const float gain = to * pair.gain;
                if (gain == 0)
                    continue;
                for (int frame = 0; frame < got; ++frame)
                    bus[frame] += in[size_t(frame) * channels] * gain;
            } else {
                for (int frame = 0; frame < got; ++frame)
                    bus[frame] += in[size_t(frame) * channels] * (from + step * float(frame)) * pair.gain;
            }
        }
        feed->m_lastGain = gainTo;
        if (got < frames) {
            // Run dry while it was heard (a gap, if its player sends again soon: above).
            // Either way it is not heard again until it has its fill again.
            if (!ended)
                feed->m_dryFrames = 0;
            feed->m_heard = false;
            feed->m_primed.store(false, std::memory_order_relaxed);
        }
        // One that has left has faded out over this part, and is done with
        if (leaving)
            feed->m_done = true;
    }

    // The app's channels: muted, soloed, with their tones
    bool anyTone = false;
    const double step = 2 * M_PI * kToneHz / rate;
    for (int bus = 0; bus < buses; ++bus) {
        float *samples = &m_buses[size_t(bus) * kPart];
        if (!m_heard[size_t(bus)])
            std::fill(samples, samples + frames, 0.0f);
        if (m_active.tone[size_t(bus)]) {
            anyTone = true;
            for (int frame = 0; frame < frames; ++frame)
                samples[frame] += kToneLevel * float(std::sin(m_tonePhase + step * frame));
        }
    }
    if (anyTone)
        m_tonePhase = std::fmod(m_tonePhase + step * frames, 2 * M_PI);

    // To the device's channels, by its matrix
    for (int bus = 0; bus < buses && bus < m_active.deviceRoutes.size(); ++bus) {
        const float *samples = &m_buses[size_t(bus) * kPart];
        for (const int channel : m_active.deviceRoutes[bus]) {
            if (channel < 0 || channel >= deviceChannels)
                continue;
            for (int frame = 0; frame < frames; ++frame)
                out[size_t(frame) * deviceChannels + channel] += samples[frame];
        }
    }
    // For the meter, what the device is sent: the loudest of its odd-numbered channels
    // and of its even-numbered (a stereo pair's left and right, wherever on it the pair
    // is), before it is held to full, so that sound that is clipped shows
    float left = 0;
    float right = 0;
    for (size_t sample = 0; sample < size_t(frames) * deviceChannels; ++sample) {
        float &most = sample % deviceChannels % 2 == 0 ? left : right;
        most = std::max(most, std::abs(out[sample]));
        out[sample] = std::clamp(out[sample], -1.0f, 1.0f);
    }
    if (deviceChannels == 1)
        right = left;
    const auto raise = [](std::atomic<float> &kept, float value) {
        float had = kept.load(std::memory_order_relaxed);
        while (value > had && !kept.compare_exchange_weak(had, value, std::memory_order_relaxed)) {
        }
    };
    raise(m_peakLeft, left);
    raise(m_peakRight, right);
}

} // namespace audiomix
