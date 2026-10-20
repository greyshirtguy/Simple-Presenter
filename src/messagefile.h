#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace rv::data {
class Action;
}

// Messages: named things to say on the screens over everything else (a parent wanted at
// the creche, a car with its lights on), each with words, places in the words for what
// is only known when it is shown, a look, and how it goes away. Plain rules on
// ProPresenter's file of them, with nothing of the windows in them, so that they can
// be tried by themselves (tests/unit/tst_messagefile).
//
// How ProPresenter keeps them. A workspace's Configuration/Messages is a list of
// messages. Each has:
//   - an id and a title, which is its name in the list;
//   - its text, with a TOKEN wherever something is to be put in when it is shown. A
//     token stands in the text as "${" + its id + "}", and is one of three kinds: text
//     (it has a name, "Name" say, and is given words by whoever shows the message), a
//     timer (it names one of the workspace's timers, and shows that timer's time), or
//     the clock;
//   - for each token, what it was last given, kept apart from the token itself and
//     found by the token's id: a text token's words, how a timer token's time is
//     written, how the clock's date and time are;
//   - the slide of a theme it is drawn in, named by the theme's name, the slide's id,
//     the slide's name and its place among the theme's slides;
//   - how it goes away: by hand, after so many seconds, or (ProPresenter's, not done
//     here) when its timers have run out;
//   - whether it can be seen on the network, which this app has nothing for and leaves
//     as it is.
// A message that has only just been made is an id and a title and nothing else.
//
// What that is known from: the file format (messages.proto), one file a Mac's
// ProPresenter of version 18 wrote, which someone published, and (since 2026-10-20) a
// workspace of the maintainer's, whose messages file has a message with a text token
// and whose presentations have ProPresenter's own actions that show a message. A text
// token's value is kept as this writes it: its words, with the token's name beside
// them, as a timer's is (that was a guess, until that file). Where this writes a thing
// neither file has an example of, it is A GUESS that ProPresenter writes it so:
//   - a clock token's value: the time alone, the short way;
//   - what a new timer token says of how its time is written: minutes as they come and
//     seconds in two figures ("5:03"), which is what the one example has. Nothing is
//     said of the timer's own setting, which the example has too: this app shows the
//     timer as it stands, and does not set it going.
//
// Every change reads the file, changes the one thing and writes the whole of it back,
// so that what else is in it, and anything this does not know of, is kept. A change
// that changes nothing writes nothing. A workspace with no file has no messages, and is
// given the file when one is made.
namespace messagefile {

struct Token
{
    QString id;
    // "text", "timer" or "clock"
    QString kind;
    // A text token's name; a timer token's is the name of its timer
    QString name;
    // A timer token's timer, by id
    QString timerId;
    // What a text token was last given
    QString value;
    // How a timer token's time is written: each part as Timers::Style (which are the
    // file's own numbers), and whether the hundredths show only in the last minute
    int hours = 0;
    int minutes = 1;
    int seconds = 2;
    int hundredths = 0;
    bool hundredthsUnderMinute = false;
    // How the clock's date and time are written, each as clocktext::Style (the file's
    // own numbers too), and whether the time is on the 24 hour clock
    int date = 0;
    int time = 1;
    bool twentyFour = false;
};

struct Message
{
    QString id;
    QString title;
    // Its text as the file has it, each token standing in it as "${<its id>}"
    QString text;
    // Its tokens, in the file's order
    QList<Token> tokens;
    // The slide of a theme it is drawn in: the theme's name ("" for none), the slide's
    // id and name, and its place among the theme's slides
    QString theme;
    QString themeSlide;
    QString themeSlideName;
    int themeSlideIndex = 0;
    // How it goes away: 0 by hand, 1 after `seconds`, 2 when its timers have run out
    int clear = 0;
    double seconds = 0;
};

// The names a text token is offered under before any is typed: ProPresenter's own.
QStringList usualTokens();
// How a token stands in a message's text.
QString placeOf(const QString &tokenId);
// A message's text as a list of its pieces in order, each either words ({ false, words })
// or a token ({ true, id }); a "${...}" that is the id of none of its tokens is words.
struct Piece
{
    bool token = false;
    QString text;
};
QList<Piece> pieces(const Message &message);

// The workspace's messages, in their order. A workspace with no file has none, which
// is not an error.
QList<Message> messages(const QString &workspace, QString *error);

// Each of these changes the file and writes it back, and returns an error message,
// empty on success. A message is named by its id.
//
// A new message, last in the list, with the id given.
QString add(const QString &workspace, const QString &id, const QString &title);
QString rename(const QString &workspace, const QString &id, const QString &title);
QString remove(const QString &workspace, const QString &id);
// Moves it up the list (by -1) or down (by 1).
QString move(const QString &workspace, const QString &id, int by);
// Its text, and the tokens that stand in it. A token of `tokens` that the message has
// already (by id) is left exactly as it is, with what it was last given; one it has
// not is made, as its kind, name and timer say; and one the message has that is not in
// `tokens`, or stands nowhere in the text, is taken out, with what it was given.
QString setText(const QString &workspace, const QString &id, const QString &text, const QList<Token> &tokens);
// What a text token is given.
QString setValue(const QString &workspace, const QString &id, const QString &tokenId, const QString &value);
// The slide of a theme it is drawn in; an empty `theme` for none.
QString setTheme(const QString &workspace, const QString &id, const QString &theme, const QString &slideId, const QString &slideName, int slideIndex);
// How it goes away: by hand (0), or after so many seconds (1).
QString setClear(const QString &workspace, const QString &id, int clear, double seconds);

// An action that shows a message (see actions.h) can say what the message's tokens are
// to be given, and the message is then shown with that, as ProPresenter shows it. The
// action keeps it as the message keeps its own last values: a value for each token, by
// the token's id, with the token's name beside it. ProPresenter's own (seen in the
// maintainer's presentations, 2026-10-20) has one for the timer token of the message it
// shows, with the timer's setting in it, and none for a message with no tokens.
//
// What an action says its tokens are given, in its order: the token's id and name, its
// kind ("text", "timer" or "clock"), and for a text token its words. (A timer's setting
// and the clock's format are not read: the message shows the timer as it stands, as it
// does when it is shown from its tab, and the clock as the message says.)
struct ActionValue
{
    QString tokenId;
    QString name;
    QString kind;
    QString value;
};
QList<ActionValue> actionValues(const rv::data::Action &action);
// Gives a message action's text tokens the words `values` have for them, by the
// token's id: one that has a value has its words changed (a value of another kind, as
// it was, becomes words), and one that has none is given one. Its other values, and a
// value for a token `values` do not name, are kept as they are. Only text values are
// taken from `values`.
void setActionValues(rv::data::Action *action, const QList<ActionValue> &values);

} // namespace messagefile
