#include "fontnames.h"

#include <QRegularExpression>

namespace fontnames {

QString plain(const QString &qtFamily)
{
    // Qt's own form: the family, a space, and the foundry in brackets at the end
    static const QRegularExpression foundry(QStringLiteral("^(.*\\S) \\[[^\\[\\]]*\\]$"));
    const QRegularExpressionMatch match = foundry.match(qtFamily);
    return match.hasMatch() ? match.captured(1) : qtFamily;
}

QStringList plainFamilies(const QStringList &qtFamilies)
{
    QStringList out;
    QSet<QString> seen;
    for (const QString &family : qtFamilies) {
        const QString name = plain(family);
        if (!seen.contains(name.toLower())) {
            seen.insert(name.toLower());
            out.append(name);
        }
    }
    return out;
}

QString qtFamily(const QString &family, const QString &foundry, const QSet<QString> &qtFamilies)
{
    const QString underFoundry = family + QStringLiteral(" [") + foundry + u']';
    return !foundry.isEmpty() && qtFamilies.contains(underFoundry) ? underFoundry : family;
}

QString styleKey(const QString &style)
{
    QString key = style.toLower();
    key.remove(u' ');
    key.remove(u'-');
    return key;
}

bool saidByBoldAndItalic(const QString &style)
{
    static const QSet<QString> plainStyles {QStringLiteral("regular"), QStringLiteral("book"), QStringLiteral("roman"),
                                            QStringLiteral("normal"), QStringLiteral("bold"), QStringLiteral("italic"),
                                            QStringLiteral("oblique"), QStringLiteral("bolditalic"), QStringLiteral("boldoblique")};
    return style.isEmpty() || plainStyles.contains(styleKey(style));
}

QString postScriptStyle(const QString &postScriptName)
{
    const qsizetype dash = postScriptName.lastIndexOf(u'-');
    return dash > 0 ? postScriptName.mid(dash + 1) : QString();
}

} // namespace fontnames
