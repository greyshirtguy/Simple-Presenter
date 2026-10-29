#include "pptxfonts.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QStringList>

#include <climits>
#include <cstdlib>

#include <fontconfig/fontconfig.h>

namespace pptx {

bool FontFace::isBold() const
{
    return legacySubfamily.contains(QLatin1String("bold"), Qt::CaseInsensitive);
}

bool FontFace::isItalic() const
{
    return legacySubfamily.contains(QLatin1String("italic"), Qt::CaseInsensitive)
           || legacySubfamily.contains(QLatin1String("oblique"), Qt::CaseInsensitive);
}

bool FontFace::hasOfficeStyle() const
{
    static const QStringList office = {QStringLiteral("regular"), QStringLiteral("bold"), QStringLiteral("italic"),
                                       QStringLiteral("bold italic"), QStringLiteral("oblique"),
                                       QStringLiteral("bold oblique"), QStringLiteral("roman"), QStringLiteral("normal")};
    return office.contains(legacySubfamily.toLower());
}

QString FontFace::officeTypeface() const
{
    return hasOfficeStyle() ? legacyFamily : fullName;
}

bool FontFace::officeBold() const
{
    return hasOfficeStyle() && isBold();
}

bool FontFace::officeItalic() const
{
    return hasOfficeStyle() && isItalic();
}

namespace {

quint16 u16(const QByteArray &b, qsizetype i)
{
    return i + 1 < b.size() ? quint16((uchar(b.at(i)) << 8) | uchar(b.at(i + 1))) : 0;
}

quint32 u32(const QByteArray &b, qsizetype i)
{
    return (quint32(u16(b, i)) << 16) | u16(b, i + 2);
}

QString fcString(FcPattern *pattern, const char *object)
{
    FcChar8 *value = nullptr;
    if (FcPatternGetString(pattern, object, 0, &value) == FcResultMatch && value)
        return QString::fromUtf8(reinterpret_cast<const char *>(value));
    return {};
}

int fcInt(FcPattern *pattern, const char *object, int otherwise)
{
    int value = 0;
    return FcPatternGetInteger(pattern, object, 0, &value) == FcResultMatch ? value : otherwise;
}

// Every installed face of exactly this PostScript name, or of this typographic family:
// FcFontList, which lists what is installed, where FcFontMatch would always find
// something, another font in the place of one that is not here.
FcFontSet *listed(const char *object, const QString &value)
{
    FcPattern *pattern = FcPatternCreate();
    const QByteArray bytes = value.toUtf8();
    FcPatternAddString(pattern, object, reinterpret_cast<const FcChar8 *>(bytes.constData()));
    FcObjectSet *wanted = FcObjectSetBuild(FC_FILE, FC_INDEX, FC_FAMILY, FC_FULLNAME, FC_STYLE, FC_POSTSCRIPT_NAME,
                                           FC_WEIGHT, FC_SLANT, FC_WIDTH, nullptr);
    FcFontSet *set = FcFontList(nullptr, pattern, wanted);
    FcObjectSetDestroy(wanted);
    FcPatternDestroy(pattern);
    return set;
}

std::optional<FontFace> faceOf(FcPattern *found, const QString &postScriptName)
{
    FontFace face;
    face.postScriptName = postScriptName;
    face.family = fcString(found, FC_FAMILY);
    face.fullName = fcString(found, FC_FULLNAME);
    if (face.fullName.isEmpty())
        face.fullName = face.family + u' ' + fcString(found, FC_STYLE);
    face.weight = fcInt(found, FC_WEIGHT, FC_WEIGHT_REGULAR);
    const int slant = fcInt(found, FC_SLANT, FC_SLANT_ROMAN);
    const auto names = legacyNames(nameTable(fcString(found, FC_FILE), fcInt(found, FC_INDEX, 0) & 0xFFFF));
    if (names) {
        face.legacyFamily = names->first;
        face.legacySubfamily = names->second;
    } else {
        // A guess from the face's own weight and slant, as ProToPPTX guesses from
        // CoreText's traits
        const bool bold = face.weight >= FC_WEIGHT_BOLD;
        const bool italic = slant != FC_SLANT_ROMAN;
        face.legacyFamily = face.family;
        face.legacySubfamily = bold && italic ? QStringLiteral("Bold Italic") : bold ? QStringLiteral("Bold")
                             : italic          ? QStringLiteral("Italic")
                                               : QStringLiteral("Regular");
    }
    return face;
}

QMutex &knownLock()
{
    static QMutex lock;
    return lock;
}

QHash<QString, std::optional<FontFace>> &known()
{
    static QHash<QString, std::optional<FontFace>> faces;
    return faces;
}

} // namespace

QByteArray nameTable(const QString &fontFile, int index)
{
    QFile file(fontFile);
    if (fontFile.isEmpty() || !file.open(QIODevice::ReadOnly))
        return {};
    // A collection (.ttc) starts with a table of where each of its fonts begins.
    QByteArray head = file.read(12);
    qint64 start = 0;
    if (head.startsWith("ttcf")) {
        const quint32 count = u32(head, 8);
        if (index < 0 || quint32(index) >= count || !file.seek(12 + 4 * qint64(index)))
            return {};
        start = u32(file.read(4), 0);
        if (!file.seek(start))
            return {};
        head = file.read(12);
    }
    const int tables = u16(head, 4);
    const QByteArray records = file.read(16 * qint64(tables));
    for (int k = 0; k < tables; ++k) {
        if (records.mid(16 * k, 4) != "name")
            continue;
        const quint32 offset = u32(records, 16 * k + 8);
        const quint32 length = u32(records, 16 * k + 12);
        // (Offsets in a collection are from the file's start, as in a single font.)
        if (length > 1 << 20 || !file.seek(offset))
            return {};
        return file.read(length);
    }
    return {};
}

std::optional<std::pair<QString, QString>> legacyNames(const QByteArray &bytes)
{
    if (bytes.size() < 6)
        return std::nullopt;
    const int count = u16(bytes, 2);
    const int storage = u16(bytes, 4);
    struct Best
    {
        int rank = -1;
        QString value;
    };
    Best best[3];
    for (int k = 0; k < count; ++k) {
        const int r = 6 + k * 12;
        if (r + 12 > bytes.size())
            break;
        const int platform = u16(bytes, r);
        const int encoding = u16(bytes, r + 2);
        const int language = u16(bytes, r + 4);
        const int id = u16(bytes, r + 6);
        const int length = u16(bytes, r + 8);
        const int offset = u16(bytes, r + 10);
        if (id != 1 && id != 2)
            continue;
        const int from = storage + offset;
        if (from + length > bytes.size())
            continue;
        const QByteArray slice = bytes.mid(from, length);
        int rank = 0;
        QString value;
        if (platform == 3 && (encoding == 1 || encoding == 0)) {
            rank = language == 0x409 ? 3 : 2;
            for (int j = 0; j + 1 < slice.size(); j += 2)
                value.append(QChar(char16_t((uchar(slice.at(j)) << 8) | uchar(slice.at(j + 1)))));
        } else if (platform == 1 && encoding == 0) {
            rank = language == 0 ? 1 : 0;
            // Mac Roman: the same as Latin-1 for the letters a family's name has
            value = QString::fromLatin1(slice);
        }
        if (!value.isEmpty() && rank > best[id].rank)
            best[id] = Best {rank, value};
    }
    if (best[1].value.isEmpty())
        return std::nullopt;
    return std::make_pair(best[1].value, best[2].value.isEmpty() ? QStringLiteral("Regular") : best[2].value);
}

std::optional<FontFace> installedFace(const QString &postScriptName)
{
    {
        const QMutexLocker lock(&knownLock());
        const auto found = known().constFind(postScriptName);
        if (found != known().constEnd())
            return *found;
    }
    std::optional<FontFace> face;
    if (FcFontSet *set = listed(FC_POSTSCRIPT_NAME, postScriptName)) {
        if (set->nfont > 0)
            face = faceOf(set->fonts[0], postScriptName);
        FcFontSetDestroy(set);
    }
    const QMutexLocker lock(&knownLock());
    known().insert(postScriptName, face);
    return face;
}

std::optional<FontFace> installedFace(const QString &postScriptName, bool bold, bool italic)
{
    const std::optional<FontFace> plain = installedFace(postScriptName);
    if (!plain)
        return std::nullopt;
    const bool wantBold = bold && !plain->isBold() && plain->weight < FC_WEIGHT_BOLD;
    const bool wantItalic = italic && !plain->isItalic();
    if (!wantBold && !wantItalic)
        return plain;
    // The family's face with the weight and slant asked for and the same width: as near
    // the weight as can be (bold, for a regular face with \b), the rest as the face has
    // them. As macOS's NSFontManager converts a font to have a trait.
    std::optional<FontFace> best;
    int bestDistance = INT_MAX;
    if (FcFontSet *set = listed(FC_FAMILY, plain->family)) {
        FcFontSet *own = listed(FC_POSTSCRIPT_NAME, postScriptName);
        const int width = own && own->nfont > 0 ? fcInt(own->fonts[0], FC_WIDTH, FC_WIDTH_NORMAL) : FC_WIDTH_NORMAL;
        const int slant = own && own->nfont > 0 ? fcInt(own->fonts[0], FC_SLANT, FC_SLANT_ROMAN) : FC_SLANT_ROMAN;
        if (own)
            FcFontSetDestroy(own);
        const int wantedWeight = wantBold ? FC_WEIGHT_BOLD : plain->weight;
        const bool wantedItalic = wantItalic || slant != FC_SLANT_ROMAN;
        for (int k = 0; k < set->nfont; ++k) {
            FcPattern *candidate = set->fonts[k];
            if (fcInt(candidate, FC_WIDTH, FC_WIDTH_NORMAL) != width)
                continue;
            if ((fcInt(candidate, FC_SLANT, FC_SLANT_ROMAN) != FC_SLANT_ROMAN) != wantedItalic)
                continue;
            const int weight = fcInt(candidate, FC_WEIGHT, FC_WEIGHT_REGULAR);
            if (wantBold && weight < FC_WEIGHT_DEMIBOLD)
                continue;
            const int distance = std::abs(weight - wantedWeight);
            const QString name = fcString(candidate, FC_POSTSCRIPT_NAME);
            if (distance < bestDistance && !name.isEmpty()) {
                bestDistance = distance;
                best = installedFace(name);
            }
        }
        FcFontSetDestroy(set);
    }
    return best ? best : plain;
}

} // namespace pptx
