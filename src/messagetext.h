#pragma once

#include <QObject>
#include <QQuickImageProvider>
#include <QQuickTextDocument>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// A message's words in a box they can be typed in, with its tokens among them.
//
// A token is not words: it is a place in the words where something is put when the
// message is shown (a name, the time of day). ProPresenter shows one in the box as a
// small rounded label with the token's name on it, which is moved over and deleted as
// one thing; and so does this. In the file the same token is "${" + its id + "}" in
// the message's text (see messagefile.h), which is nothing anyone should have to read
// or type.
//
// How. The box is a TextEdit, and a TextEdit's document can hold a picture in a line of
// text as if it were one character. So each token is a picture of its label, made on
// the spot by TokenPictures below, and the picture carries its token's id with it
// (as something of its own that the document keeps with every character). MessageText
// puts a message's words into a box's document that way, puts one more token in where
// the caret is, and reads the document back as the file's text, with the ids of the
// tokens that are still in it: one that has been deleted from the box is no longer
// there to be read, which is how a token comes out of a message.
//
// Text tokens are blue, and the ones that need nothing typed (the clock, a timer) are
// grey-blue, so that the two can be told apart in the box.
class MessageText : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    using QObject::QObject;

    // Replaces the document's contents with a message's words: `pieces` and `tokens`
    // as Messages has them (see messages.h). `scale` is how many of the screen's own
    // points a point of the window is, for the labels to be drawn sharp.
    Q_INVOKABLE void load(QQuickTextDocument *document, const QVariantList &pieces, const QVariantList &tokens, qreal scale) const;
    // The document's contents as a message's text, { text, tokens }: the text as the
    // file has it, and the ids of the tokens standing in it, in order.
    Q_INVOKABLE QVariantMap read(QQuickTextDocument *document) const;
    // Puts a token in place of what is selected, or where the caret is. Returns where
    // the caret is then to be: after it.
    Q_INVOKABLE int insert(QQuickTextDocument *document, int selectionStart, int selectionEnd, const QVariantMap &token, qreal scale) const;
    // What a token's label says: a text token's name, a timer's, or "Clock".
    Q_INVOKABLE QString labelOf(const QVariantMap &token) const;
};

// The pictures of the labels. QML's own store of pictures asks for one by
//
//   image://token/<scale>/<kind>/<label>
//
// (`label` with whatever a web address cannot have in it written the way one writes
// it), and is given the label drawn at that many times its size. It is asked once for
// each label there is, and keeps it.
class TokenPictures : public QQuickImageProvider
{
public:
    TokenPictures();

    // How large a label is drawn, in the window's points
    static QSizeF sizeOf(const QString &label);
    static QString address(const QString &kind, const QString &label, qreal scale);

    // (This can be called from more than one thread, so it keeps nothing between calls.)
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
