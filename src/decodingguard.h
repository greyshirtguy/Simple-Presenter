#pragma once

#include <QString>
#include <QStringList>

#include <functional>

// The guard on hardware video decoding: whether this computer is one on which the
// graphics driver's video decoder is known to do harm when it is tried, so that it is
// left untried without anyone having to know to ask. Plain rules, with nothing of the
// windows in them, so that they can be tried by themselves
// (tests/unit/tst_decodingguard), on folders made up to look like a system's.
//
// The one case there is. An NVIDIA card run by the open-source nouveau driver decodes
// video on an engine of its own, which needs firmware that only NVIDIA's own driver
// comes with; nobody else may pass it on, so a system does not have it unless whoever
// set it up took it out of NVIDIA's driver themselves (nouveau's own pages say how).
// Without it the decoder cannot work. But Mesa's driver offers it all the same, Qt
// takes the offer, and what was seen on such a computer (a GTX 670, whose chip nouveau
// calls NVE4, on Ubuntu 26.04) was this: the first video played, decoded by the
// processor after all; the next showed nothing; and then the whole graphics chip
// stopped answering, with the kernel's log naming the firmware it had not found
// (nve4_fuc084, nve4_fuc085, nve4_fuc086) and the channel FFmpeg's decoder had opened.
// With hardware decoding left untried, nothing of that happened.
//
// So the rule: if the graphics chip video would be decoded on is run by nouveau, is of
// a kind nouveau has a decoder for, and the firmware that decoder needs is not in the
// system's firmware folders, hardware decoding is not tried. Whoever knows better can
// say so (decodingchoice.h: the setting, and --hardware-decoding).
//
// Which graphics chip that is. Qt asks FFmpeg for a hardware decoder without naming a
// device, and FFmpeg's VA-API then takes the first of the system's render nodes
// (/dev/dri/renderD128 and on), passing over one that is only a stand-in ("vgem"). So
// that is the one looked at here, by the name of the kernel driver that runs it, which
// the system says in /sys. A computer with two graphics chips is judged by the one
// video would go to, which on a laptop with Intel graphics beside an NVIDIA chip is
// Intel's. (A system that lists no render node at all is judged by its first graphics
// card: FFmpeg then asks the desktop, which that card is taken to draw.)
//
// Which chips. nouveau numbers the chips it runs, and has a decoder for those from
// 0x84 to 0x108: the cards of 2007 to 2014, up to the GTX 780. For the older ones and
// the newer (the GTX 750 and all since) it offers no decoder at all, so there is none
// to go wrong, and the guard has nothing to do. The number is asked of the driver
// itself.
// If it cannot be had, the chip is taken to be one of those with a decoder: a guard
// that holds when it need not costs the processor some work, and one that fails to
// hold costs the computer.
//
// Which firmware. The kernel looks in /lib/firmware (and its "updates" folder, and
// the folders of those named for the kernel's release) for nouveau/nvXX_fuc084 and
// nvXX_fuc085, XX being the chip's number, each as one file or as a pair ending in c
// and d; the chips up to 0x96 and 0xa0, which have an older engine, want
// nouveau/nv84_xuc00f and nv84_xuc103 instead. A file may be packed (.zst, .xz).
// Where the chip's number is not known, any such file at all is taken for the firmware
// being there.
//
// GUESSES, this having been worked out from one computer's logs and from reading, on a
// computer that has no such card, and not tried on a card with its firmware in place:
// that the firmware being there is enough for the decoder to be safe to try (nouveau's
// pages speak of pictures that come out wrong on some videos, and old reports of
// hangs); the exact names for the older engine; and that the driver says the chip's
// number when asked as it is asked here (if it does not, the guard still holds: see
// above).
namespace decodingguard {

// What was found of the computer
struct Found
{
    // The render node FFmpeg's VA-API would take ("/dev/dri/renderD128"), and the
    // kernel driver that runs it ("nouveau", "i915", "amdgpu"); both empty where there
    // is none
    QString node;
    QString driver;
    // nouveau's number for the chip, where the driver is nouveau and it said; 0 if not
    int chipset = 0;
    // Whether the firmware nouveau's decoder needs was found, and the files looked for
    // where the chip's number says which they are
    bool firmware = false;
    QStringList wanted;
};

struct Verdict
{
    // Whether hardware decoding is to be left untried on this computer
    bool guarded = false;
    // Why, in words that finish "video is decoded by the processor only, because ..."
    QString reason;
};

// nouveau's name for a chip, from its number: "NVE4"
QString chipName(int chipset);

// Whether nouveau has a video decoder for a chip, by its number
bool hasDecoder(int chipset);

// Looks at the computer. `root` is put before every path that is read ("" for the
// system itself; a folder made up for a test). `chipsetOf` is asked for nouveau's
// number for the chip behind a render node, and answers 0 if it cannot say; `release`
// is the kernel's release, which two of the firmware folders are named for.
Found look(const QString &root, const QString &release, const std::function<int(const QString &node)> &chipsetOf);

// The rule.
Verdict judge(const Found &found);

// What was found, in a line for the log
QString described(const Found &found);

// nouveau's number for the chip behind a render node, asked of the driver; 0 if the
// node cannot be opened or the driver does not answer (it is not nouveau, say).
int nouveauChipset(const QString &node);

} // namespace decodingguard
