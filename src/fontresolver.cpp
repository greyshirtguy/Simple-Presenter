#include "fontresolver.h"

#include "fontnames.h"
#include "sessionlog.h"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

#include <fontconfig/fontconfig.h>

namespace {

// The families as Qt lists them, read once, for what a face is asked for by
// (fontnames::qtFamily). Fonts installed while the app runs are found by fontconfig but
// drawn in their family's first foundry until it starts again.
const QSet<QString> &qtFamilies()
{
    static const QSet<QString> families = [] {
        const QStringList all = QFontDatabase::families();
        return QSet<QString>(all.begin(), all.end());
    }();
    return families;
}

QString stringOf(FcPattern *pattern, const char *object, int index = 0)
{
    FcChar8 *value = nullptr;
    return FcPatternGetString(pattern, object, index, &value) == FcResultMatch
               ? QString::fromUtf8(reinterpret_cast<const char *>(value)) : QString();
}

// A face fontconfig has listed, as the app draws it. Its first style goes with its first
// family: "Light" with "CMG Sans", where the same face's "Regular" goes with its other
// family, "CMG Sans Light".
ResolvedFont faceOf(FcPattern *face)
{
    ResolvedFont font;
    font.family = stringOf(face, FC_FAMILY);
    int weight = FC_WEIGHT_REGULAR;
    int slant = FC_SLANT_ROMAN;
    FcPatternGetInteger(face, FC_WEIGHT, 0, &weight);
    FcPatternGetInteger(face, FC_SLANT, 0, &slant);
    font.bold = weight >= FC_WEIGHT_DEMIBOLD;
    font.italic = slant != FC_SLANT_ROMAN;
    font.installed = !font.family.isEmpty();
    font.style = stringOf(face, FC_STYLE);
    font.qtFamily = fontnames::qtFamily(font.family, stringOf(face, FC_FOUNDRY), qtFamilies());
    return font;
}

// The faces fontconfig lists for `pattern`, with what faceOf needs of each
FcFontSet *listed(FcPattern *pattern)
{
    FcObjectSet *properties = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FOUNDRY, FC_WEIGHT, FC_SLANT, nullptr);
    FcFontSet *faces = FcFontList(nullptr, pattern, properties);
    FcObjectSetDestroy(properties);
    return faces;
}

bool lookUpInstalled(const QString &postScriptName, ResolvedFont *font)
{
    const QByteArray name = postScriptName.toUtf8();
    FcPattern *pattern = FcPatternBuild(nullptr, FC_POSTSCRIPT_NAME, FcTypeString, name.constData(), nullptr);
    FcFontSet *faces = listed(pattern);
    const bool found = faces && faces->nfont > 0;
    if (found)
        *font = faceOf(faces->fonts[0]);
    if (faces)
        FcFontSetDestroy(faces);
    FcPatternDestroy(pattern);
    return found && font->installed;
}

// The installed face of `family` whose style is `style`, compared as fontnames::styleKey
// does, the style taken that goes with the family's name in the face (a face of two
// families has a style for each); not installed if there is none
ResolvedFont faceOfStyle(const QString &family, const QString &style)
{
    ResolvedFont found;
    if (family.isEmpty() || style.isEmpty())
        return found;
    const QByteArray name = family.toUtf8();
    FcPattern *pattern = FcPatternBuild(nullptr, FC_FAMILY, FcTypeString, name.constData(), nullptr);
    FcFontSet *faces = listed(pattern);
    const QString wanted = fontnames::styleKey(style);
    for (int f = 0; faces && f < faces->nfont && !found.installed; ++f) {
        for (int i = 0; !stringOf(faces->fonts[f], FC_FAMILY, i).isEmpty(); ++i) {
            if (stringOf(faces->fonts[f], FC_FAMILY, i).compare(family, Qt::CaseInsensitive) != 0)
                continue;
            const QString itsStyle = stringOf(faces->fonts[f], FC_STYLE, i);
            if (fontnames::styleKey(itsStyle.isEmpty() ? stringOf(faces->fonts[f], FC_STYLE) : itsStyle) == wanted)
                found = faceOf(faces->fonts[f]);
            break;
        }
    }
    if (faces)
        FcFontSetDestroy(faces);
    FcPatternDestroy(pattern);
    return found;
}

// "HelveticaNeue-BoldItalic" -> family "Helvetica Neue", bold, italic.
ResolvedFont guessFromName(const QString &postScriptName, const QString &familyHint)
{
    ResolvedFont font;
    const QString style = fontnames::postScriptStyle(postScriptName);
    const qsizetype dash = postScriptName.lastIndexOf(u'-');
    const QString base = dash > 0 ? postScriptName.left(dash) : postScriptName;
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
    static QMutex guard;
    static QHash<QString, ResolvedFont> cache;
    const QString hint = fontnames::plain(familyHint);
    const QString key = postScriptName + u'\n' + hint;
    {
        const QMutexLocker locked(&guard);
        const auto cached = cache.constFind(key);
        if (cached != cache.constEnd())
            return *cached;
    }

    ResolvedFont font;
    if (postScriptName.isEmpty() || !lookUpInstalled(postScriptName, &font)) {
        font = guessFromName(postScriptName, hint);
        // Said once for each font, which is how often it gets here: text set in a font
        // that is missing is drawn in another, and does not look or fit as it was made to.
        // (A face of its family of the same style looks as it was made to, if not to the
        // letter, and is said as well.)
        const ResolvedFont styled = faceOfStyle(font.family, fontnames::postScriptStyle(postScriptName));
        if (styled.installed) {
            font = styled;
            SessionLog::write("font", QStringLiteral("\"%1\" is not installed; text set in it is drawn in \"%2\" %3, the face of that style")
                                          .arg(postScriptName, font.family, font.style));
        } else if (!postScriptName.isEmpty()) {
            SessionLog::write("font", QStringLiteral("\"%1\" is not installed; text set in it is drawn in \"%2\"")
                                          .arg(postScriptName, fontnames::plain(QFontInfo(QFont(font.family)).family())));
        }
    }
    const QMutexLocker locked(&guard);
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
// family is not installed. The family may be any of a face's: "CMG Sans Light" is one of
// CMGSans-Light's.
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
        // Fontconfig always matches something; it only counts if it is the family asked for.
        bool same = false;
        for (int i = 0; !same && !stringOf(match, FC_FAMILY, i).isEmpty(); ++i)
            same = family.compare(stringOf(match, FC_FAMILY, i), Qt::CaseInsensitive) == 0;
        if (same)
            name = stringOf(match, FC_POSTSCRIPT_NAME);
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pattern);
    return name;
}

} // namespace

QString postScriptNameFor(const QString &qtFamily, bool bold, bool italic)
{
    const QString family = fontnames::plain(qtFamily);
    const QString installed = installedPostScriptName(family, bold, italic);
    if (!installed.isEmpty())
        return installed;
    QString base = family;
    base.remove(u' ');
    return base + styleSuffix(bold, italic);
}

QString restyledPostScriptName(const QString &postScriptName, const QString &qtFamily, bool bold, bool italic)
{
    const QString family = fontnames::plain(qtFamily);
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
