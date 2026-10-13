#include "messagetext.h"

#include "messagefile.h"

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>

#include <cmath>

namespace {

// Where a token's picture keeps the token's id, among what the document keeps with
// each character
const int kTokenId = QTextFormat::UserProperty + 41;

// The label's letters: the app's own, small and bold
QFont labelFont()
{
    QFont font = QGuiApplication::font();
    font.setPixelSize(11);
    font.setBold(true);
    return font;
}

const qreal kLabelHeight = 16;
const qreal kLabelPad = 6;

QTextImageFormat picture(const QVariantMap &token, const QString &label, qreal scale)
{
    const QString kind = token.value("kind", QStringLiteral("text")).toString();
    const QSizeF size = TokenPictures::sizeOf(label);
    QTextImageFormat format;
    format.setName(TokenPictures::address(kind, label, scale));
    format.setWidth(size.width());
    format.setHeight(size.height());
    // Its middle on the middle of the letters beside it
    format.setVerticalAlignment(QTextCharFormat::AlignMiddle);
    format.setProperty(kTokenId, token.value("id").toString());
    return format;
}

}

QString MessageText::labelOf(const QVariantMap &token) const
{
    const QString kind = token.value("kind").toString();
    if (kind == QLatin1String("clock"))
        return QStringLiteral("Clock");
    const QString name = token.value("name").toString().simplified();
    return name.isEmpty() ? (kind == QLatin1String("timer") ? QStringLiteral("Timer") : QStringLiteral("Token")) : name;
}

void MessageText::load(QQuickTextDocument *document, const QVariantList &pieces, const QVariantList &tokens, qreal scale) const
{
    if (!document || !document->textDocument())
        return;
    QTextDocument *text = document->textDocument();
    text->clear();
    QTextCursor cursor(text);
    cursor.beginEditBlock();
    const QTextCharFormat words;
    for (const QVariant &entry : pieces) {
        const QVariantMap piece = entry.toMap();
        if (!piece.value("token").toBool()) {
            cursor.insertText(piece.value("text").toString(), words);
            continue;
        }
        const QString id = piece.value("text").toString();
        for (const QVariant &candidate : tokens) {
            const QVariantMap token = candidate.toMap();
            if (token.value("id").toString() == id) {
                cursor.insertImage(picture(token, labelOf(token), scale));
                break;
            }
        }
    }
    cursor.endEditBlock();
    text->clearUndoRedoStacks();
    text->setModified(false);
}

QVariantMap MessageText::read(QQuickTextDocument *document) const
{
    QString text;
    QStringList ids;
    if (document && document->textDocument()) {
        const QTextDocument *contents = document->textDocument();
        for (QTextBlock block = contents->begin(); block.isValid(); block = block.next()) {
            if (block != contents->begin())
                text += u'\n';
            for (QTextBlock::iterator each = block.begin(); !each.atEnd(); ++each) {
                const QTextFragment fragment = each.fragment();
                const QTextCharFormat format = fragment.charFormat();
                QString words = fragment.text();
                if (format.isImageFormat()) {
                    // One picture for each character of it: a token each
                    const QString id = format.property(kTokenId).toString();
                    for (const QChar character : std::as_const(words)) {
                        if (character == QChar::ObjectReplacementCharacter && !id.isEmpty()) {
                            text += messagefile::placeOf(id);
                            ids << id;
                        }
                    }
                    continue;
                }
                // (A line broken within a paragraph is a line all the same; and a
                // picture's character that has lost its picture is nothing.)
                words.replace(QChar::LineSeparator, u'\n');
                words.remove(QChar::ObjectReplacementCharacter);
                text += words;
            }
        }
    }
    return {{"text", text}, {"tokens", ids}};
}

int MessageText::insert(QQuickTextDocument *document, int selectionStart, int selectionEnd, const QVariantMap &token, qreal scale) const
{
    if (!document || !document->textDocument())
        return selectionEnd;
    QTextCursor cursor(document->textDocument());
    const int last = qMax(0, document->textDocument()->characterCount() - 1);
    cursor.setPosition(qBound(0, selectionStart, last));
    cursor.setPosition(qBound(0, selectionEnd, last), QTextCursor::KeepAnchor);
    cursor.insertImage(picture(token, labelOf(token), scale));
    return cursor.position();
}

TokenPictures::TokenPictures()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QSizeF TokenPictures::sizeOf(const QString &label)
{
    return QSizeF(std::ceil(QFontMetricsF(labelFont()).horizontalAdvance(label)) + 2 * kLabelPad, kLabelHeight);
}

QString TokenPictures::address(const QString &kind, const QString &label, qreal scale)
{
    return QStringLiteral("image://token/%1/%2/%3").arg(qBound(1.0, scale, 4.0)).arg(kind, QString::fromLatin1(QUrl::toPercentEncoding(label)));
}

QImage TokenPictures::requestImage(const QString &id, QSize *size, const QSize &)
{
    const QString scaleText = id.section(u'/', 0, 0);
    const QString kind = id.section(u'/', 1, 1);
    const QString label = QUrl::fromPercentEncoding(id.section(u'/', 2).toLatin1());
    const qreal scale = qBound(1.0, scaleText.toDouble(), 4.0);
    const QSizeF points = sizeOf(label);
    QImage image(QSize(int(std::ceil(points.width() * scale)), int(std::ceil(points.height() * scale))), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(scale, scale);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(0.5, 0.5, points.width() - 1, points.height() - 1), 5, 5);
    // A text token blue; the clock and a timer, which need nothing typed, grey-blue
    painter.fillPath(shape, kind == QLatin1String("text") ? QColor("#1e6fc4") : QColor("#56677f"));
    painter.setPen(Qt::white);
    painter.setFont(labelFont());
    painter.drawText(QRectF(0, 0, points.width(), points.height() - 1), Qt::AlignCenter, label);
    painter.end();
    if (size)
        *size = image.size();
    return image;
}
