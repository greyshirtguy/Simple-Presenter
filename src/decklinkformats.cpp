#include "decklinkformats.h"

namespace decklinkformats {

namespace {

// A code made of four letters, as the SDK's are: 'Hp29' is 0x48703239.
constexpr quint32 code(const char (&letters)[5])
{
    return (quint32(uchar(letters[0])) << 24) | (quint32(uchar(letters[1])) << 16) | (quint32(uchar(letters[2])) << 8)
         | quint32(uchar(letters[3]));
}

}

const QList<Format> &formats()
{
    static const QList<Format> all {
        {QStringLiteral("1080p24"), 1920, 1080, 24, 1, code("24ps")},
        {QStringLiteral("1080p25"), 1920, 1080, 25, 1, code("Hp25")},
        {QStringLiteral("1080p29.97"), 1920, 1080, 30000, 1001, code("Hp29")},
        {QStringLiteral("1080p30"), 1920, 1080, 30, 1, code("Hp30")},
        {QStringLiteral("1080p50"), 1920, 1080, 50, 1, code("Hp50")},
        {QStringLiteral("1080p59.94"), 1920, 1080, 60000, 1001, code("Hp59")},
        {QStringLiteral("1080p60"), 1920, 1080, 60, 1, code("Hp60")},
        {QStringLiteral("720p50"), 1280, 720, 50, 1, code("hp50")},
        {QStringLiteral("720p59.94"), 1280, 720, 60000, 1001, code("hp59")},
        {QStringLiteral("720p60"), 1280, 720, 60, 1, code("hp60")},
    };
    return all;
}

const Format *formatNamed(const QString &name)
{
    for (const Format &format : formats()) {
        if (format.name == name)
            return &format;
    }
    return nullptr;
}

void unpremultiplied(uchar *pixels, qsizetype count)
{
    for (qsizetype i = 0; i < count; ++i, pixels += 4) {
        const uint alpha = pixels[3];
        if (alpha == 255 || alpha == 0)
            continue;
        // (Rounded to the nearest, and never past white: a colour a little over its
        // opacity, which drawing can leave, is as bright as it can be.)
        for (int c = 0; c < 3; ++c)
            pixels[c] = uchar(qMin<uint>(255, (pixels[c] * 255u + alpha / 2) / alpha));
    }
}

qint64 period(const Format &format)
{
    return qint64(1'000'000'000) * format.rateDenominator / format.rateNumerator;
}

qint64 passDue(const QList<Waiting> &waiting)
{
    // (One with no frame free makes no pass due by itself: it would be due over and over,
    // and drawn in none. Its card giving a frame back asks for a pass again.)
    qint64 soonest = -1;
    for (const Waiting &screen : waiting) {
        if (screen.ready && (soonest < 0 || screen.next < soonest))
            soonest = screen.next;
    }
    if (soonest < 0)
        return -1;
    qint64 due = soonest;
    // (Not for one kept to a card's frames: waited for, the pass would be off the card's
    // beat for all the others kept to it. It is drawn at its next moment instead.)
    for (const Waiting &screen : waiting) {
        if (!screen.ready && !screen.onCard && screen.next - soonest <= screen.period / 2)
            due = qMax(due, soonest + screen.period / 2);
    }
    return due;
}

bool drawnInPass(const Waiting &screen, qint64 now)
{
    // (A millisecond, for one kept to a card's frames: a pass is woken by a timer of
    // whole milliseconds, and is not run sooner than that before it is due)
    return screen.ready && screen.next - now <= (screen.onCard ? 1'000'000 : screen.period / 2);
}

qint64 nextAfter(const Waiting &screen, qint64 now)
{
    if (screen.onCard)
        return now + screen.period / 2;
    return now - screen.next < screen.period ? screen.next + screen.period : now + screen.period;
}

namespace {

// a / b, rounded up, for any a and a positive b
qint64 roundedUp(qint64 a, qint64 b)
{
    return a >= 0 ? (a + b - 1) / b : -(-a / b);
}

}

Waiting onCard(Waiting screen, const CardClock &card, qint64 lead, qint64 now)
{
    if (card.period <= 0)
        return screen;
    // The card's frames begin at at - inFrame and every period on, so the moments are
    // lead before each
    const qint64 first = card.at - card.inFrame - lead;
    const qint64 from = qMax(screen.next, now - kLate);
    screen.next = first + roundedUp(from - first, card.period) * card.period;
    screen.onCard = true;
    return screen;
}

qint64 leadFor(qint64 longestPass, qint64 period)
{
    return qBound<qint64>(4'000'000, longestPass + 8'000'000, qMax<qint64>(4'000'000, period * 3 / 4));
}

}
