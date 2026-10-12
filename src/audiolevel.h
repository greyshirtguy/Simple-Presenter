#pragma once

#include <QtGlobal>

// The arithmetic of sound for the audio layer: how loud a stretch of it is, what a
// meter shows of that, and how a fade goes. Plain functions on numbers, with nothing of
// what plays the sound in them, so that they can be tried by themselves
// (tests/unit/tst_audiolevel).
namespace audiolevel {

// How a stretch of sound has its samples written
enum class Format { UInt8, Int16, Int32, Float };

// The loudest that each of two channels gets in a stretch of sound, each from 0, which
// is silence, to 1, which is as loud as a sample can say.
struct Peaks
{
    float left = 0;
    float right = 0;
};

// The peaks of a stretch of sound: `bytes` of samples at `data`, written as `format`
// says, the channels one after another for each moment (left, right, left, right).
// Sound of one channel is as loud on the right as on the left; of more than two, the
// first two are taken, which are the left and the right in every usual layout. What
// is no sound at all (no channels, no samples) is silence.
Peaks peaks(const void *data, qsizetype bytes, Format format, int channels);

// How far up a meter a peak comes, from 0 to 1. A meter goes by decibels, as the ear
// does and as every meter on a sound desk does: its top is a peak of 1 (0 dB, as loud
// as can be), each tenth of it is 6 dB, and its foot is 60 dB down, below which
// nothing is shown. So sound half as loud as can be, 6 dB down, is at nine tenths.
double metered(double peak);

// What a meter shows a little later: it goes up to a new peak at once, so that none is
// missed, and comes down from an old one over a moment (the whole of its height in
// `kFall` seconds), so that the eye can follow it. `shown` and the answer are how far
// up the meter, `level` how far up the sound has come since, and `seconds` how long
// it has been.
double settled(double shown, double level, double seconds);
inline constexpr double kFall = 0.45;

// How loud sound is at a point of a fade, `along` running from 0 at the fade's start
// to 1 at its end: from nothing to all of it, and from all of it to nothing. The two
// are a quarter of a circle each and not straight lines, so that two pieces crossing,
// one going out as the other comes in, are together as loud all the way through as
// either is alone: with straight lines the middle of a crossfade dips.
double fadeIn(double along);
double fadeOut(double along);

} // namespace audiolevel
