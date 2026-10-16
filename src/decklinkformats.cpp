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

}
