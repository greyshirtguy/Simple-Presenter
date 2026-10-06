#include "richtextbridge.h"

#include "richtext.h"
#include "textlayout.h"

#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QTextCursor>
#include <QTextDocument>

void RichTextBridge::load(QQuickTextDocument *document, const QVariant &richText) const
{
    if (document && document->textDocument())
        toDocument(richText.value<RichText>(), document->textDocument());
}

QVariant RichTextBridge::save(QQuickTextDocument *document) const
{
    if (!document || !document->textDocument())
        return QVariant::fromValue(RichText());
    return QVariant::fromValue(fromDocument(document->textDocument()));
}

int RichTextBridge::paste(QQuickTextDocument *document, int selectionStart, int selectionEnd) const
{
    if (!document || !document->textDocument())
        return selectionEnd;
    QString text = QGuiApplication::clipboard()->text();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(u'\r', u'\n');

    QTextCursor cursor(document->textDocument());
    cursor.setPosition(selectionStart);
    cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    cursor.beginEditBlock();
    cursor.removeSelectedText();
    // Line by line, so each new paragraph is a copy of the one the caret was in.
    const QStringList lines = text.split(u'\n');
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (i > 0)
            cursor.insertBlock();
        cursor.insertText(lines.at(i));
    }
    cursor.endEditBlock();
    return cursor.position();
}

QVariant RichTextBridge::formatted(const QVariant &richText, int start, int end, const QVariantMap &format) const
{
    return QVariant::fromValue(richText.value<RichText>().formatted(start, end, format));
}

QVariantMap RichTextBridge::formatAt(const QVariant &richText, int start, int end) const
{
    return richText.value<RichText>().formatAt(start, end);
}

bool RichTextBridge::isEmpty(const QVariant &richText) const
{
    return richText.value<RichText>().isEmpty();
}

QStringList RichTextBridge::fontFamilies() const
{
    return QFontDatabase::families();
}
