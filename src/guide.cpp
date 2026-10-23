#include "guide.h"

#include <QColor>
#include <QFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

QString Guide::text() const
{
    QFile file(QStringLiteral(":/docs/guide.md"));
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("# The guide\n\nThe guide is not in this build of the app.");
    const QString all = QString::fromUtf8(file.readAll());
    const qsizetype heading = all.indexOf(QLatin1String("\n# "));
    QString shown = heading >= 0 ? all.mid(heading + 1) : all;
    // (The line saying where the page is seen from is for the repository's readers)
    shown.remove(QLatin1String("*This page is also what the ? button at the right of the toolbar shows, inside the app.*\n\n"));
    QTextDocument document;
    document.setMarkdown(shown);
    // Each link in the app's orange, underlined
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isAnchor())
                continue;
            QTextCursor cursor(&document);
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            QTextCharFormat format;
            format.setForeground(QColor(0xff, 0x8a, 0x1f));
            format.setFontUnderline(true);
            cursor.mergeCharFormat(format);
        }
    }
    return document.toHtml();
}
