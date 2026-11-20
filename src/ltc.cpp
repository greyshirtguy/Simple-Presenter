#include "ltc.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ltc {

namespace {

// The quietest signal that is read: the two sides this far apart (a fiftieth of full
// scale each way, which is some 34 dB down). Under it is taken for silence, and hiss.
constexpr float kQuietest = 0.04f;
// How far past the middle the sound has to go to count as having crossed, as a part of
// how far apart its sides are
constexpr float kPast = 0.12f;
// The sixteen bits that end a frame, as they come: 0011 1111 1111 1101
constexpr unsigned kSync = 0x3FFD;

}

Decoder::Decoder(int sampleRate) : m_rate(std::max(8000, sampleRate))
{
    reset();
}

void Decoder::reset()
{
    // (Between the slowest and the fastest there is: twenty-four frames a second is
    // 1,920 bits, thirty is 2,400. Started there, a short stretch of either is under
    // three quarters of it and a long one over, so the first bits are told apart
    // rightly whichever it is, and the length is then learnt.)
    m_bit = m_rate / 2160.0;
    m_half = false;
    m_good = 0;
    m_tail = 0;
    m_next = 0;
    m_sinceFrame = -1;
    m_crossedYet = false;
    m_loud = false;
    std::memset(m_bits, 0, sizeof m_bits);
}

void Decoder::feed(const float *samples, int count, std::vector<Frame> *frames)
{
    // The sides follow the sound out at once and come back in slowly: a fiftieth of a
    // second or so, which is some forty bits, so that they keep to a signal that fades
    // and let go of a click.
    const float back = 1.0f / (m_rate * 0.02f);
    for (int i = 0; i < count; ++i) {
        const float x = samples[i];
        const float apart = m_high - m_low;
        if (x > m_high)
            m_high = x;
        else
            m_high -= apart * back;
        if (x < m_low)
            m_low = x;
        else
            m_low += apart * back;
        const float swing = m_high - m_low;
        const bool loud = swing >= (m_loud ? kQuietest * 0.7f : kQuietest);
        if (loud && !m_loud) {
            // Come up out of silence: where it did is where a bit begins, whichever
            // side it went to, and everything is begun again from there
            m_up = x > m_last;
            m_crossedYet = true;
            m_crossed = double(m_fed + i) - 0.5;
            m_half = false;
            m_good = 0;
            m_sinceFrame = -1;
        } else if (loud) {
            const float middle = (m_high + m_low) / 2;
            const float edge = m_up ? middle - swing * kPast : middle + swing * kPast;
            if (m_up ? x < edge : x > edge) {
                m_up = !m_up;
                // Where between the sample before and this one it passed the edge
                const float step = x - m_last;
                const double part = step != 0 ? std::clamp(double((edge - m_last) / step), 0.0, 1.0) : 0.0;
                crossing(double(m_fed + i - 1) + part, frames);
            } else if (m_half && double(m_fed + i) - m_crossed > m_bit * 0.75) {
                // The second half of a one has gone on past where it ends with no
                // change of side: the timecode has stopped there (its last frame ends
                // with a one, and nothing comes after to mark the end of it). The one is
                // taken as made, ending where its half would have, so that the last
                // frame of a run is read like any other; and what comes next, if
                // anything does, is begun again.
                m_half = false;
                bit(true, m_crossed + m_bit / 2, frames);
                m_good = 0;
                m_sinceFrame = -1;
                m_crossedYet = false;
            }
        }
        m_loud = loud;
        m_last = x;
    }
    m_fed += count;
}

void Decoder::crossing(double at, std::vector<Frame> *frames)
{
    if (!m_crossedYet) {
        m_crossedYet = true;
        m_crossed = at;
        return;
    }
    const double stretch = at - m_crossed;
    m_crossed = at;
    // Far too short or too long for a bit of any timecode: a click, or the signal come
    // back after a gap. What was half made is let go.
    if (stretch < m_bit * 0.25 || stretch > m_bit * 1.6) {
        m_half = false;
        m_good = 0;
        m_sinceFrame = -1;
        if (stretch > m_bit * 4)
            m_bit = m_rate / 2160.0;
        return;
    }
    if (stretch < m_bit * 0.75) {
        // Half of a one
        if (!m_half) {
            m_half = true;
            return;
        }
        m_half = false;
        // (The length of a bit is learnt from every bit, a little at a time.)
        m_bit += (stretch * 2 - m_bit) * 0.06;
        bit(true, at, frames);
        return;
    }
    // A zero. (After half a one, the halves have come apart: begun again from here.)
    if (m_half) {
        m_half = false;
        m_good = 0;
        m_sinceFrame = -1;
    }
    m_bit += (stretch - m_bit) * 0.12;
    bit(false, at, frames);
}

int Decoder::digits(int from, int count) const
{
    // (Bit `from` of the frame is that many after the oldest kept, which is at m_next.)
    int value = 0;
    for (int i = 0; i < count; ++i)
        value |= m_bits[(m_next + from + i) % 80] << i;
    return value;
}

void Decoder::bit(bool one, double end, std::vector<Frame> *frames)
{
    m_bits[m_next] = one ? 1 : 0;
    m_next = (m_next + 1) % 80;
    m_tail = ((m_tail << 1) | (one ? 1u : 0u)) & 0xFFFF;
    if (m_good < 80)
        ++m_good;
    if (m_sinceFrame >= 0)
        ++m_sinceFrame;
    if (m_good < 80 || m_tail != kSync)
        return;
    Frame frame;
    const int frameUnits = digits(0, 4), frameTens = digits(8, 2);
    const int secondUnits = digits(16, 4), secondTens = digits(24, 3);
    const int minuteUnits = digits(32, 4), minuteTens = digits(40, 3);
    const int hourUnits = digits(48, 4), hourTens = digits(56, 2);
    frame.frames = frameTens * 10 + frameUnits;
    frame.seconds = secondTens * 10 + secondUnits;
    frame.minutes = minuteTens * 10 + minuteUnits;
    frame.hours = hourTens * 10 + hourUnits;
    frame.dropFrame = digits(10, 1) != 0;
    frame.end = end;
    frame.samples = m_sinceFrame == 80 ? end - m_frameEnd : 0;
    m_frameEnd = end;
    m_sinceFrame = 0;
    // Digits that are no time: sound that happened to end as a frame does, or a frame
    // with a bit read wrongly. (Where it ended is still kept, for the next one's length.)
    if (frameUnits > 9 || secondUnits > 9 || minuteUnits > 9 || hourUnits > 9
        || frame.frames > 29 || frame.seconds > 59 || frame.minutes > 59 || frame.hours > 23)
        return;
    frames->push_back(frame);
}

} // namespace ltc
