#include "fontresolver.h"

#include <QHash>
#include <QRegularExpression>

#include <fontconfig/fontconfig.h>

namespace {

bool lookUpInstalled(const QString &postScriptName, ResolvedFont *font)
{
    const QByteArray name = postScriptName.toUtf8();
    FcPattern *pattern = FcPatternBuild(nullptr, FC_POSTSCRIPT_NAME, FcTypeString, name.constData(), nullptr);
    FcObjectSet *properties = FcObjectSetBuild(FC_FAMILY, FC_WEIGHT, FC_SLANT, nullptr);
    FcFontSet *matches = FcFontList(nullptr, pattern, properties);

    bool found = false;
    if (matches && matches->nfont > 0) {
        FcChar8 *family = nullptr;
        if (FcPatternGetString(matches->fonts[0], FC_FAMILY, 0, &family) == FcResultMatch) {
            int weight = FC_WEIGHT_REGULAR;
            int slant = FC_SLANT_ROMAN;
            FcPatternGetInteger(matches->fonts[0], FC_WEIGHT, 0, &weight);
            FcPatternGetInteger(matches->fonts[0], FC_SLANT, 0, &slant);
            font->family = QString::fromUtf8(reinterpret_cast<const char *>(family));
            font->bold = weight >= FC_WEIGHT_DEMIBOLD;
            font->italic = slant != FC_SLANT_ROMAN;
            found = true;
        }
    }

    if (matches)
        FcFontSetDestroy(matches);
    FcObjectSetDestroy(properties);
    FcPatternDestroy(pattern);
    return found;
}

// "HelveticaNeue-BoldItalic" -> family "Helvetica Neue", bold, italic.
ResolvedFont guessFromName(const QString &postScriptName, const QString &familyHint)
{
    ResolvedFont font;
    const qsizetype dash = postScriptName.lastIndexOf(u'-');
    const QString base = dash > 0 ? postScriptName.left(dash) : postScriptName;
    const QString style = dash > 0 ? postScriptName.mid(dash + 1) : QString();
    font.bold = style.contains(u"Bold", Qt::CaseInsensitive) || style.contains(u"Black", Qt::CaseInsensitive)
        || style.contains(u"Heavy", Qt::CaseInsensitive);
    font.italic = style.contains(u"Italic", Qt::CaseInsensitive) || style.contains(u"Oblique", Qt::CaseInsensitive);

    if (!familyHint.isEmpty()) {
        font.family = familyHint;
    } else {
        static const QRegularExpression camelBoundary(QStringLiteral("(?<=[a-z])(?=[A-Z])"));
        font.family = base;
        font.family.replace(camelBoundary, QStringLiteral(" "));
    }
    return font;
}

} // namespace

ResolvedFont resolvePostScriptName(const QString &postScriptName, const QString &familyHint)
{
    static QHash<QString, ResolvedFont> cache;
    const QString key = postScriptName + u'\n' + familyHint;
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return *cached;

    ResolvedFont font;
    if (postScriptName.isEmpty() || !lookUpInstalled(postScriptName, &font))
        font = guessFromName(postScriptName, familyHint);
    cache.insert(key, font);
    return font;
}
