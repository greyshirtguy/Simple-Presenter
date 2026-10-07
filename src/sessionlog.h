#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The session's log: a text file that says what the app was running on and what it did,
// so that when something has gone wrong there is something to go on: for a person to
// read, or to hand to an AI model with the question.
//
// Where it is. Each run of the app writes a file of its own in
// Documents/SimplePresenter/Logs, named for when it started; the twenty most recent
// are kept. (The self-test writes its log beside its pictures instead.)
//
// What is in it. First what the app is running on: its version and Qt's, the system,
// the processor and memory, the screens, and what draws the windows (the graphics chip
// and its driver). Then, a line each, what was done and what happened: a workspace
// opened, a presentation opened, a slide gone live, media started, a transition run
// with how many frames it managed, a layer cleared, a prop turned on, a timer started,
// a file saved, a window moved to another screen, an error shown to the user. Whatever
// Qt itself has to say, its warnings above all, goes in as well. A line whose second
// column is in capitals is something that went wrong. It holds the names of files,
// presentations and playlists, and nothing of what is in them.
//
// What it is for above all is the case that leaves nothing else behind: the app
// stopping dead, which a transition's shader can do on graphics hardware it does not
// agree with. So a line is in the file before the thing it speaks of is tried (a
// transition says which shader it is about to run), and a crash writes its own last
// lines: what stopped the app, and where it was. If the app is not answering, because
// it is busy or stuck, a line says so after two seconds, and another when it is back.
//
// What it must never do is cost anything that could be noticed.
//   - It is written when something happens, never for each frame: a few lines for a
//     click, none while a slide sits on the output or a video plays.
//   - A line is put together and handed to the system with one call. That costs a few
//     millionths of a second, and the system writes it to the disk in its own time.
//     Nothing waits for the disk. Because the line is handed over at once and not held
//     back to be written later, it is still there if the app dies in the next instant.
//   - What Qt says is limited: a message that repeats is counted and not written again,
//     and messages that come in a flood are let through at one a second.
//   - Frames are counted so that a transition can say how many it drew, by adding one
//     to a number as each is shown.
//   - Two things run by the clock, on a thread of their own: once a second the app is
//     asked whether it is still answering, which costs it the handling of one event,
//     and every five minutes a line says how much of a processor and how much memory
//     it has been using.
//
// There is one log for the whole app. C++ writes to it with write(); QML reaches the
// same thing by the name Log.
class SessionLog : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Log)
    QML_SINGLETON
    // The file being written, or "" if there is no log; and the folder it is in
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QString folder READ folder CONSTANT)

public:
    using QObject::QObject;

    // Opens the session's log and writes what the app is running on. To be called
    // first of all, before the application object exists, so that whatever goes wrong
    // in starting up is caught too. A run that only answers a question (--help,
    // --list-screens) keeps no log.
    static void start(int argc, char *argv[], const QString &version);
    // Once the application object exists: what Qt is drawing through and the screens
    // there are, and from then on a line when a screen comes or goes.
    static void describeDisplay();
    // Once the windows are up: starts asking whether the app is still answering, and
    // saying now and then what it is using.
    static void watchForStalls();
    // At a normal end.
    static void finish();

    // One line: what kind of thing it is, in a word of at most ten letters, and what
    // there is to say. From any thread. Does nothing if there is no log.
    static void write(const char *kind, const QString &text);
    static QString filePath();

    QString path() const { return filePath(); }
    QString folder() const;
    // Shows the folder the logs are in, in the desktop's file manager: for whoever is
    // looking for one.
    Q_INVOKABLE void showFolder() const;
    // A line, as write().
    Q_INVOKABLE void note(const QString &kind, const QString &text) const;
    // Something that went wrong: a line marked as that, and the same on the terminal.
    Q_INVOKABLE void problem(const QString &text) const;
    // Has a window spoken for in the log under `name`: what draws it, and when it is
    // shown, hidden or moved to another screen. Its frames are counted from then on.
    Q_INVOKABLE void watch(QQuickWindow *window, const QString &name) const;
    // How many frames a watched window has shown so far. The difference between two
    // of these is how many it showed in between.
    Q_INVOKABLE int frames(QQuickWindow *window) const;
};
