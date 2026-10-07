#include "fontresolver.h"

#include "sessionlog.h"

#include <QFont>
#include <QFontInfo>
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
    if (postScriptName.isEmpty() || !lookUpInstalled(postScriptName, &font)) {
        font = guessFromName(postScriptName, familyHint);
        // Said once for each font, which is how often it gets here: text set in a font
        // that is missing is drawn in another, and does not look or fit as it was made to.
        if (!postScriptName.isEmpty()) {
            SessionLog::write("font", QStringLiteral("\"%1\" is not installed; text set in it is drawn in \"%2\"")
                                          .arg(postScriptName, QFontInfo(QFont(font.family)).family()));
        }
    }
    cache.insert(key, font);
    return font;
}

namespace {

QString styleSuffix(bool bold, bool italic)
{
    return bold && italic ? QStringLiteral("-BoldItalic") : bold ? QStringLiteral("-Bold")
         : italic ? QStringLiteral("-Italic") : QString();
}

// The PostScript name of the installed face of this family and style, or empty if the
// family is not installed.
QString installedPostScriptName(const QString &family, bool bold, bool italic)
{
    FcPattern *pattern = FcPatternBuild(nullptr, FC_FAMILY, FcTypeString, family.toUtf8().constData(),
                                        FC_WEIGHT, FcTypeInteger, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR,
                                        FC_SLANT, FcTypeInteger, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN, nullptr);
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result = FcResultNoMatch;
    FcPattern *match = FcFontMatch(nullptr, pattern, &result);

    QString name;
    if (match) {
        FcChar8 *matchedFamily = nullptr;
        FcChar8 *postScriptName = nullptr;
        // Fontconfig always matches something; it only counts if it is the family asked for.
        if (FcPatternGetString(match, FC_FAMILY, 0, &matchedFamily) == FcResultMatch
            && family.compare(QString::fromUtf8(reinterpret_cast<const char *>(matchedFamily)), Qt::CaseInsensitive) == 0
            && FcPatternGetString(match, FC_POSTSCRIPT_NAME, 0, &postScriptName) == FcResultMatch)
            name = QString::fromUtf8(reinterpret_cast<const char *>(postScriptName));
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pattern);
    return name;
}

} // namespace

QString postScriptNameFor(const QString &family, bool bold, bool italic)
{
    const QString installed = installedPostScriptName(family, bold, italic);
    if (!installed.isEmpty())
        return installed;
    QString base = family;
    base.remove(u' ');
    return base + styleSuffix(bold, italic);
}

QString restyledPostScriptName(const QString &postScriptName, const QString &family, bool bold, bool italic)
{
    const QString installed = installedPostScriptName(family, bold, italic);
    if (!installed.isEmpty())
        return installed;
    if (postScriptName.isEmpty())
        return postScriptNameFor(family, bold, italic);
    // Only the bold and italic parts of the name change: "CMGSans-Light" made bold is
    // still a guess, but "CMGSans-LightItalic" made upright should stay Light.
    const qsizetype dash = postScriptName.lastIndexOf(u'-');
    const QString base = dash > 0 ? postScriptName.left(dash) : postScriptName;
    QString style = dash > 0 ? postScriptName.mid(dash + 1) : QString();
    for (const QString &part : {QStringLiteral("BoldItalic"), QStringLiteral("BoldOblique"), QStringLiteral("Bold"),
                                QStringLiteral("Italic"), QStringLiteral("Oblique"), QStringLiteral("Regular")})
        style.remove(part);
    if (bold)
        style += QStringLiteral("Bold");
    if (italic)
        style += QStringLiteral("Italic");
    return style.isEmpty() ? base : base + u'-' + style;
}
