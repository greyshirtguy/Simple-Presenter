#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// Lets a TextEdit edit a RichText value in place. The text goes into the TextEdit's
// document with every stretch of it carrying its whole format, including the parts a
// TextEdit cannot show (the document's own font name, the stroke), so what is typed
// next to a stretch takes that format and nothing is lost on the way back out.
// Formatting is not done on the document: the text is taken out, formatted as a
// RichText, and put back, so there is one implementation of what a format change means.
class RichTextBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    using QObject::QObject;

    // Replaces the document's contents with the text.
    Q_INVOKABLE void load(QQuickTextDocument *document, const QVariant &richText) const;
    // The document's contents as a RichText value.
    Q_INVOKABLE QVariant save(QQuickTextDocument *document) const;
    // Replaces what is selected in the document, if anything, with plain text from the
    // clipboard, in the format of the text before it. Returns the position after it.
    Q_INVOKABLE int paste(QQuickTextDocument *document, int selectionStart, int selectionEnd) const;

    // RichText::formatted and RichText::formatAt, for QML.
    Q_INVOKABLE QVariant formatted(const QVariant &richText, int start, int end, const QVariantMap &format) const;
    Q_INVOKABLE QVariantMap formatAt(const QVariant &richText, int start, int end) const;
    Q_INVOKABLE bool isEmpty(const QVariant &richText) const;

    // The font families installed here, for a font menu.
    Q_INVOKABLE QStringList fontFamilies() const;
};
