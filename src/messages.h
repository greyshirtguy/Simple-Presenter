#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's messages: the list in ProPresenter's Configuration/Messages, for the
// tab of the show controls they are made and shown from, and for the show (which is
// told of them, so that an action can show one and so that one can go by itself after
// its time). What a message is, how the file is read and how it is changed are
// messagefile's (see messagefile.h); this is what QML talks to, and two things besides
// that belong to showing one:
//
//   - A message as a slide. A message is drawn in a slide of a theme: the theme slide
//     as it is, with the message's words in its first text box, in that box's own
//     format. slideOf() makes that slide. A theme's text boxes are the things on it
//     with words in them, which stand for the words to come (as dressing a slide in a
//     theme takes them: see themefile.h); on a theme slide where nothing has words the
//     first thing on it is taken, an empty text box being still a text box. A message
//     with no theme, or with one this workspace has not, has a slide of this app's own:
//     white words on a dark band across the foot of the screen.
//
//     A GUESS at ProPresenter, which says nothing of how it uses a theme for a message:
//     that it is the first text box, and that the rest of the theme slide (its shapes
//     and pictures, its other text boxes with their own words) is shown with it.
//
//   - The words a message is showing. A message's words can have the time of day in
//     them, or a timer's time, and so change while it is shown. They are worked out
//     where the clock and the timers can be asked (the operator window does it, once
//     for all the screens, and only for messages that are shown), handed here with
//     setWords(), and read from here by the text box that draws them (SlideElement),
//     which reads `revision` to be told when to read again. So a message's slide is
//     made once, when the message is shown, and only its words are drawn again as they
//     change.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class Messages : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The messages, in the file's order, each
    //   { id, name, text, pieces, tokens, theme, themeSlide, themeSlideName,
    //     themeSlideIndex, clear, seconds }
    // text: its words as the file has them, each token standing in them as "${<its
    // id>}"; pieces: the same as a list in order, { token: false, text: words } or
    // { token: true, text: a token's id }; tokens: { id, kind ("text", "timer" or
    // "clock"), name, timerId, value, hours, minutes, seconds, hundredths,
    // hundredthsUnderMinute, date, time, twentyFour } (see messagefile::Token); theme:
    // the name of the theme it is drawn in, "" for none, and which of its slides, by
    // id, by name and by its place; clear: how it goes away, 0 by hand, 1 after
    // `seconds`, 2 ProPresenter's "when its timers have run out", which is not done
    // here and is by hand.
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    // The names a text token is offered under before any is typed
    Q_PROPERTY(QStringList usualTokens READ usualTokens CONSTANT)
    // Goes up whenever the words of a message that is shown have changed: read it in
    // the binding that asks for them.
    Q_PROPERTY(int revision READ revision NOTIFY wordsChanged)

public:
    using QObject::QObject;

    QVariantList messages() const { return m_messages; }
    QStringList usualTokens() const;
    int revision() const { return m_revision; }

    // Reads the messages of a workspace folder. Returns an error message, empty on
    // success; a workspace with no file has none.
    Q_INVOKABLE QString open(const QString &workspace);

    // A new message, last in the list. Returns { id, error }.
    Q_INVOKABLE QVariantMap add(const QString &name);
    // Each changes the workspace's file and reads it again, and returns an error
    // message, empty on success (see messagefile.h). `tokens` are maps as above, of
    // which the id, the kind, the name and the timer's id are what a new one is made
    // from.
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);
    Q_INVOKABLE QString move(const QString &id, int by);
    Q_INVOKABLE QString setText(const QString &id, const QString &text, const QVariantList &tokens);
    Q_INVOKABLE QString setValue(const QString &id, const QString &tokenId, const QString &value);
    Q_INVOKABLE QString setTheme(const QString &id, const QString &theme, const QString &slideId, const QString &slideName, int slideIndex);
    Q_INVOKABLE QString setClear(const QString &id, int clear, double seconds);
    // An id for a new token, in the form ProPresenter uses.
    Q_INVOKABLE QString newId() const;

    // A message as a slide (see above): `themeSlide` is the slide of its theme as a
    // slide map (see proconvert.h), or null for this app's own.
    Q_INVOKABLE QVariantMap slideOf(const QString &id, const QVariant &themeSlide) const;

    // The words a message is showing, as whoever works them out last gave them; and
    // giving them. Words given for a message that is not being shown are forgotten
    // with forget().
    Q_INVOKABLE QString words(const QString &id) const;
    Q_INVOKABLE void setWords(const QString &id, const QString &words);
    Q_INVOKABLE void forget(const QString &id);

signals:
    void changed();
    void wordsChanged();

private:
    QString reload();

    QString m_workspace;
    QVariantList m_messages;
    QHash<QString, QString> m_words;
    int m_revision = 0;
};
