#pragma once

#include <array>
#include <cmath>
#include <vector>

// Sound that is linear timecode, for the tests that read it (tst_ltc, tst_timecode):
// each frame as its eighty bits, each bit a change of side at its start and, for a one,
// another half way (see src/ltc.h). The bits are as long as the frame rate makes them,
// which is seldom a whole number of samples, so where each change falls is kept to a
// part of a sample and the sample it falls in is given what the sound was for each part
// of it, as a sound card would hear it.
namespace ltcmaker {

inline std::array<bool, 80> bits(int hours, int minutes, int seconds, int frames, bool drop)
{
    std::array<bool, 80> made {};
    const auto put = [&](int from, int count, int value) {
        for (int i = 0; i < count; ++i)
            made[from + i] = (value >> i) & 1;
    };
    put(0, 4, frames % 10);
    put(8, 2, frames / 10);
    made[10] = drop;
    put(16, 4, seconds % 10);
    put(24, 3, seconds / 10);
    put(32, 4, minutes % 10);
    put(40, 3, minutes / 10);
    put(48, 4, hours % 10);
    put(56, 2, hours / 10);
    // The sixteen that end it: 0011 1111 1111 1101
    static const bool sync[16] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1};
    for (int i = 0; i < 16; ++i)
        made[64 + i] = sync[i];
    return made;
}

struct Maker
{
    int rate = 48000;
    // Frames in a real second
    double fps = 30;
    float level = 0.5f;
    // Added to every sample: a signal that sits off centre
    float offset = 0;
    std::vector<float> sound;

    // Where the sound has got to, in samples (and a part of one), and which side it is on
    double at = 0;
    bool up = false;

    // Sound on one side from where it is to `to`, the sample a change fell in mixed
    void hold(double to)
    {
        while (double(sound.size()) < to) {
            const double sample = double(sound.size());
            float value = up ? level : -level;
            // (The change into this side came `at - (sample - 1)` of the way through the
            // sample before this one's instant.)
            if (m_changedAt > sample - 1 && m_changedAt <= sample) {
                const double before = m_changedAt - (sample - 1);
                value = float((up ? -level : level) * before + (up ? level : -level) * (1 - before));
            }
            sound.push_back(value + offset);
        }
        at = to;
    }

    void change()
    {
        up = !up;
        m_changedAt = at;
    }

    void bit(bool one)
    {
        const double length = rate / (fps * 80);
        change();
        if (one) {
            hold(at + length / 2);
            change();
            hold(at + length / 2);
        } else {
            hold(at + length);
        }
    }

    void frame(int hours, int minutes, int seconds, int frames, bool drop = false)
    {
        for (bool one : bits(hours, minutes, seconds, frames, drop))
            bit(one);
    }

    // `count` frames from one whose seconds and frames are given, counted on as a
    // count of `each` frames a second does (no frames dropped)
    void run(int minutes, int seconds, int frames, int count, int each, bool drop = false)
    {
        int total = (minutes * 60 + seconds) * each + frames;
        for (int i = 0; i < count; ++i, ++total)
            frame(total / each / 3600, total / each / 60 % 60, total / each % 60, total % each, drop);
    }

    void silence(double seconds)
    {
        const double to = at + seconds * rate;
        while (double(sound.size()) < to)
            sound.push_back(offset);
        at = to;
        m_changedAt = -10;
    }

private:
    double m_changedAt = -10;
};

} // namespace ltcmaker
