#pragma once

#include <QByteArray>
#include <QString>

// Who decodes video, the graphics chip or the processor alone: the one choice there is
// to make about how video is decoded, and the rule by which it is made when the app
// starts. Plain rules, with nothing of the windows in them, so that they can be tried
// by themselves (tests/unit/tst_decodingchoice).
//
// Who decodes a video. This app does not: Qt Multimedia does, through FFmpeg. For each
// video it looks for a hardware decoder first (on Linux through VA-API, or another of
// the ways FFmpeg has, whichever the graphics driver offers) and falls back to
// FFmpeg's own decoder, on the processor, when there is none. That choice is Qt's and
// is made anew for every video that is opened. Decoding on the graphics chip is what
// keeps a 4K video cheap, and it is the default here.
//
// Why it can be switched off. A graphics driver can offer a decoder that does not
// work. The case this was made for: an NVIDIA card of 2012 on the open-source nouveau
// driver, whose video decoder needs firmware the system does not have. The first video
// played; the next showed nothing; and the whole graphics chip then stopped answering,
// the kernel's log naming the channel FFmpeg's H.264 decoder had opened on it. With
// the hardware decoders untried, that part of the driver is never touched.
//
// How it is switched off. Qt's FFmpeg backend reads which hardware decoders it may try
// from a variable of the environment, QT_FFMPEG_DECODING_HW_DEVICE_TYPES: a list of
// FFmpeg's names for them, separated by commas. A list that names none leaves it no
// hardware decoder to try, and so no automatic choosing of one; every video is then
// decoded by FFmpeg's own decoders. The variable is read once, when the backend first
// decodes, so it has to be right before anything is played: the choice is made as the
// app starts, and a change to the setting takes effect at the next start.
//
// What it does not change. Only where the pictures of a video are worked out. They are
// handed over in memory and not as textures of the graphics chip, and from there on
// everything is as it was: the output is drawn by the graphics chip, through OpenGL,
// with the same textures and the same transitions.
//
// (The one other place a video is decoded is the app's own: a single picture taken
// from a file for its thumbnail, with FFmpeg's libraries directly. That is done on the
// processor by its making, with no hardware decoder asked for: see videoframe.h. So it
// has no part in this, either way.)
//
// The setting has three values:
//
//   automatic        the graphics chip where its driver offers a decoder, as Qt chooses;
//                    but the processor alone on a computer where that decoder is known
//                    to do harm when it is tried, which is the guard's to say
//                    (decodingguard.h). This is what an app that has been told nothing
//                    does.
//   processor only   no hardware decoder is tried, on any computer.
//   graphics chip    hardware decoders are tried whatever the guard says: for a
//                    computer the guard is wrong about, or whose owner wants to see
//                    for themselves.
//
// Who has the say, in this order: the command line, for trying something once
// (--software-decoding, or --hardware-decoding, the first of them if both are given);
// the setting, where it is one of the two that are not automatic; the environment as
// it was found, which whoever starts the app may have set for themselves, to nothing
// or to the decoders they want tried; and then the guard. So the guard holds only
// where nobody has said anything.
namespace decodingchoice {

// The variable Qt's FFmpeg backend reads
extern const char kVariable[];

// The setting
enum class Mode { Automatic, ProcessorOnly, GraphicsChip };
// As it is kept in the app's settings: "auto", "processor", "hardware". Anything else
// is automatic.
Mode modeOf(const QString &saved);
QString nameOf(Mode mode);

enum class By {
    Nobody,      // nothing was asked: Qt tries whatever the machine has
    CommandLine, // --software-decoding, or --hardware-decoding
    Settings,    // the setting, where it is not automatic
    Environment, // the variable was set already, by whoever started the app
    Guard,       // the guard, nobody having said otherwise
};

// What there is to go by
struct Asked
{
    // The variable as it was found
    bool environmentSet = false;
    QByteArray environment;
    // --software-decoding and --hardware-decoding
    bool softwareFlag = false;
    bool hardwareFlag = false;
    Mode mode = Mode::Automatic;
    // The guard's verdict on this computer, and its reason in words (decodingguard.h)
    bool guarded = false;
    QString guardReason;
};

struct Choice
{
    // Whether the hardware decoders are left untried
    bool softwareOnly = false;
    By by = By::Nobody;
    // What the variable is to be set to, where it is this app that sets it; null where
    // it is left as it was found
    QByteArray set;
    // The hardware decoders the environment keeps Qt to, by FFmpeg's names for them,
    // where it was the environment that named some
    QString kept;
    // Whether this computer is one the guard holds hardware decoding back on, with its
    // reason; and whether hardware decoding is tried on it all the same, because it
    // was asked for
    bool guarded = false;
    QString guardReason;
    bool overridden = false;
};

// Whether a value of the variable names no hardware decoder at all.
bool namesNone(const QByteArray &value);

// The choice at start.
Choice choose(const Asked &asked);

// The choice in words: a line for the log, and a few words for the settings screen
// ("by the processor only, as the settings ask").
QString logLine(const Choice &choice);
QString inWords(const Choice &choice);

// The choice this run of the app made, for whatever needs to know it (what says how a
// video's pictures arrived, the settings screen, and the notice the guard puts up). It
// is made once, before anything is played, and does not change.
void setSession(const Choice &choice);
const Choice &session();

} // namespace decodingchoice
