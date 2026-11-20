#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Timecode, as ProPresenter's timeline follows it: the rules, with nothing of the sound
// device or the windows in them, so that they can be tried by themselves
// (tests/unit/tst_timecode). Reading it out of sound is ltc.h's; hearing the sound, and
// the settings, timecodeinput.h's; what a timeline does about it, Main's "The timecode".
//
// What ProPresenter says of it (its help, "Timecode in ProPresenter"): the playhead of
// a timeline can be controlled by timecode. A presentation's timeline has a Timecode
// box to tick, with the timecode at which it starts beside it; a window (View,
// Timecode) shows the timecode coming in and whether it is Playing or Stopped (a word
// that can be clicked to "Not Engaged", when nothing is listened for and nothing is
// triggered), has a playlist chosen in it, whose presentations it then triggers at
// their times, and has settings behind a cog: the input device, the channel of it, the
// format, and an offset from the signal. Its files (third_party/ProPresenter7-Proto)
// have four formats, 24, 25, 29.97 and 30 frames a second, which are what is here, with
// ProPresenter's own numbers for them.
//
// What is a GUESS, ProPresenter's help saying nothing of it and no file of its settings
// having been seen:
//   - that a timeline's start (`timecode_offset` in its file) is in seconds, as every
//     other time of a timeline is: the timecode read as a clock, 01:00:00;12 at thirty
//     frames a second being 3,600.4;
//   - that the offset of the settings is seconds added to the timecode that comes in;
//   - what is done at 29.97 frames a second, where a second of the timecode is not quite
//     a second: here a timeline's place is counted in frames from its start, and the
//     frames turned into time at their real rate (so that a video on it keeps with the
//     timecode for as long as it runs), by the count the signal itself says it uses,
//     dropping frames or not;
//   - that a jump in the timecode is believed at its second frame, and a single frame
//     out of place is passed over (a bit read wrongly down a poor cable must not send a
//     show somewhere else); and that the timecode has stopped when three and a half
//     frames' worth of sound has gone by with none.
namespace timecode {

// ProPresenter's formats, by its numbers for them (TimecodeSettings.Format)
enum Format { Fps24 = 0, Fps25 = 1, Fps2997 = 2, Fps30 = 3 };

// How many frames a second of the timecode counts (24, 25, 30, 30), and how many go by
// in a real second (24, 25, 29.97..., 30)
int count(Format format);
double rate(Format format);
// "24 fps", "25 fps", "29.97 fps", "30 fps": each { value, name }, for a menu
QVariantList formats();
Format formatOf(int value);

// A timecode as it is said: 01:02:03;04
struct Label
{
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    bool operator==(const Label &other) const = default;
};

// A timecode read as a clock, in seconds (its frames a part of a second), and back; the
// way a timeline's start is kept. Both keep within a day.
double faceSeconds(const Label &label, Format format);
Label labelAt(double faceSeconds, Format format);
// As ProPresenter writes one beside a timeline's Timecode box: "01:02:03;04"
QString text(const Label &label);
QString faceText(double faceSeconds, Format format);
// What was typed for one, as seconds of the clock; -1 if it is no timecode. Taken as
// hours, minutes, seconds and frames from the right ("3;12" is three seconds and twelve
// frames; "1:00:00" an hour; with no marks between, pairs of digits: "01000000" an hour).
double typed(const QString &text, Format format);

// How many frames a timecode is from 00:00:00;00, by a count that drops frames or not,
// and the timecode a number of frames is; and when it is, in real seconds from then
qint64 frameNumber(const Label &label, Format format, bool drop);
Label labelOf(qint64 frameNumber, Format format, bool drop);
double realSeconds(const Label &label, Format format, bool drop);

// When each sample of sound was heard, on a steady clock, from when the sound was
// handed over: it is handed over in pieces, each a little late and some very (the app
// was busy), but never early, so the earliest any piece says its samples began is the
// nearest to right, and is what is gone by, over the last second or so.
class SampleClock
{
public:
    explicit SampleClock(int sampleRate = 48000);
    // `total` samples have been handed over in all, the last of them at `at` (seconds)
    void arrived(qint64 total, double at);
    // When sample `index` (from the first) was heard
    double at(double index) const;
    bool started() const { return m_any; }

private:
    int m_rate;
    bool m_any = false;
    // The earliest the first sample is said to have been heard, in this stretch of
    // time and in the one before; and when this stretch began
    double m_zero = 0;
    double m_zeroBefore = 0;
    bool m_hasBefore = false;
    double m_since = 0;
};

// Follows the timecode from its frames as they are read: whether it is running, where
// it is at any moment (between frames it goes on by the clock), and when it starts,
// jumps and stops.
class Chase
{
public:
    enum Change { Nothing, Started, Jumped, Stopped };

    void setFormat(Format format);
    Format format() const { return m_format; }
    // A frame has been read, which ended at `at` on the clock (so the one after it
    // began then)
    Change frame(const Label &label, bool drop, double at);
    // Sound has been heard up to `at`, with whatever frames were in it already given
    Change heard(double at);
    bool running() const { return m_running; }
    // Whether the count drops frames, as the last frame read said
    bool drop() const { return m_drop; }
    // Where the timecode is at `now`: in frames from 00:00:00;00 (and a part of one),
    // in real seconds from then, and as it is said. Stopped, where it stopped.
    double frames(double now) const;
    double seconds(double now) const { return frames(now) / rate(m_format); }
    Label label(double now) const;
    // As new: stopped, at nothing
    void reset();

private:
    Format m_format = Fps30;
    bool m_running = false;
    bool m_drop = false;
    // Frame `m_frame` began at `m_began` on the clock: what the timecode is counted on
    // from while it runs; stopped, `m_frame` is where
    double m_frame = 0;
    double m_began = 0;
    // When the last frame that was believed ended
    double m_heard = 0;
    // A frame out of place, kept to see whether the next follows it: its number, and
    // when it ended
    bool m_odd = false;
    qint64 m_oddFrame = 0;
    double m_oddAt = 0;
};

// Which presentation of a playlist the timecode is in: `rows` are the playlist's
// presentations whose timelines follow timecode, each with `start` (seconds of the
// clock: see faceSeconds) and `duration`; answered is the one whose timeline the
// timecode at `frames` (a count from 00:00:00;00) falls in, with `position` added, how
// far into its timeline that is in real seconds; or nothing. Of two that both hold it,
// the one that started later.
QVariantMap under(const QVariantList &rows, double frames, Format format, bool drop);

} // namespace timecode
