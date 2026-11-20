#pragma once

#include <QtGlobal>

#include <vector>

// Linear timecode (LTC, of SMPTE 12M) read out of sound: the timecode a playback rig or
// a desk sends down an audio cable, which is what ProPresenter's Timecode takes (its
// settings ask for an audio device and a channel of it). Plain arithmetic on samples,
// with nothing of the sound device or the windows in it, so that it can be tried by
// itself on sound that is made for it (tests/unit/tst_ltc).
//
// What the sound is. Each frame of the timecode is eighty bits, sent one after another
// at eighty times the frame rate (2,400 a second at thirty frames), as a square wave
// that changes side at the start of every bit, and once more in the middle of a bit
// that is a one (biphase mark). So a zero is one long stretch on one side, and a one is
// two short ones; which side is up means nothing, and a cable that turns the signal over
// changes nothing. The first sixty-four bits are the time, its digits in fours with the
// least first (frames, seconds, minutes, hours), with a flag among them that says the
// count drops frames (29.97's), and bits a rig may use for anything; the last sixteen
// are always 0011 1111 1111 1101, which is in no other place and is how the end of a
// frame is found. A frame is therefore known only when it has all been heard: it names
// the frame that began eighty bits before, and the frame after it begins where its last
// bit ends.
//
// How it is read. Where the sound crosses from one side to the other is found (about a
// middle that follows the sound, so that a signal sitting off centre is still read, and
// by more than a little, so that hiss is not taken for crossings), to a part of a
// sample; the stretches between are told apart as short or long against the length of a
// bit, which is learnt from the stretches themselves, so that any frame rate, and a
// source running a little fast or slow, is read without being told which; and the bits
// that makes are kept, the last eighty, and looked at for the sixteen that end a frame.
// A frame whose digits are no time (a nine in a place of tens, say) is let go. Where the
// sound comes up out of silence is taken as the start of a bit, and where it stops, as
// the end of the one it was in the middle of: so the first frame and the last of a run
// are read as the rest are, though nothing marks the one's start or the other's end.
//
// What it costs: a few comparisons a sample, on forty-eight thousand of them a second,
// and nothing is kept but the last eighty bits.
namespace ltc {

struct Frame
{
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    // The flag that says the count drops frames (two at the start of every minute but
    // each tenth): 29.97 frames a second kept to the time of day
    bool dropFrame = false;
    // Where it ends, in samples from the first ever given (and a part of one): the
    // frame it names began `samples` before this, and the next begins here
    double end = 0;
    // How many samples long it was: from the end of the frame before to its own; 0
    // where the frame before it was not heard, so that its length is not known
    double samples = 0;
};

class Decoder
{
public:
    explicit Decoder(int sampleRate);

    // More of the sound, of one channel, each sample from -1 to 1: the frames that
    // are completed in it are added to `frames`, in order.
    void feed(const float *samples, int count, std::vector<Frame> *frames);
    // How far apart the two sides of the sound are, from 0 (silence) to 2 (as loud as
    // can be): for something to say that a signal is there at all
    float swing() const { return m_high - m_low; }
    // Samples given so far
    qint64 fed() const { return m_fed; }
    // As new: what was half heard is let go (the count of samples goes on)
    void reset();

private:
    void crossing(double at, std::vector<Frame> *frames);
    void bit(bool one, double end, std::vector<Frame> *frames);
    int digits(int from, int count) const;

    int m_rate;
    qint64 m_fed = 0;
    // The sides of the sound, each following it down or up slowly; which side it is
    // on; and the sample before
    float m_high = 0;
    float m_low = 0;
    bool m_up = false;
    float m_last = 0;
    // Whether there is sound at all (its sides far enough apart to be read)
    bool m_loud = false;
    // Where it last crossed, and whether it has yet
    double m_crossed = 0;
    bool m_crossedYet = false;
    // The length of a bit, in samples, as it has been learnt
    double m_bit;
    // A short stretch has been heard, and the other half of its one is awaited
    bool m_half = false;
    // The last eighty bits, round and round: `m_next` is where the next goes, which is
    // also where the oldest is; how many of them are good, up to eighty; and the last
    // sixteen as a number, the newest lowest
    unsigned char m_bits[80];
    int m_next = 0;
    int m_good = 0;
    unsigned m_tail = 0;
    // Where the frame before ended, and how many bits ago
    double m_frameEnd = 0;
    int m_sinceFrame = -1;
};

} // namespace ltc
