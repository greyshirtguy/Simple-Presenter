#pragma once

#include "timers.pb.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The timers of a workspace: what there are, how each is running, and what each shows.
//
// What a timer is. ProPresenter has three kinds, and keeps a workspace's timers in its
// Configuration/Timers file, which is what is read and written here:
//   - a countdown runs from a length of time down to nothing;
//   - a countdown to a time runs down to a time of day;
//   - an elapsed time runs up from a start, to an end if it has one.
// Each stops when it gets there unless it is allowed to overrun, in which case it runs
// on: a countdown below nothing, as a negative time. The first and the last wait to be
// started; a countdown to a time of day runs of itself, there being only one thing it
// can show, until it is stopped.
//
// What a timer is for. A text box on a slide can be linked to a timer, and then shows
// that timer's time in place of its own text (see linkedText() and SlideElement.qml).
// The link names the timer twice, by id and by name, and is followed by id first and
// then by name: the name is what still means something in another workspace. A link
// that finds no timer either way fails quietly, as does a slide's action for a timer
// that is not there: nothing is said, and the box shows a time of nothing.
//
// How a time is written. A link says how each of four parts is written: the hours, the
// minutes, the seconds and the hundredths of a second (Style, below, which is what
// ProPresenter offers for each). A part that is hidden is carried in the next one
// shown, so seconds alone count past sixty. A time is taken to the smallest part that
// shows: a countdown is rounded up to it, so that it starts on its full length and
// reaches nothing as it ends, and an elapsed time down.
//
// What is kept where. Which timers there are and how each is set up is in the file,
// and every change to that reads the file, makes the one change and writes it back, as
// with the playlists. Whether a timer is running, and where it has got to, is not in
// any file: it is here, for as long as the app runs.
//
// What a slide does to a timer. A slide's cue can carry an action for a timer: start
// it, stop it, put it back at its start, and with that, optionally, how the timer is to
// be set up (which is how one timer serves as a minute's countdown on one slide and
// three on another). act() does what such an action asks when its slide is triggered.
// Triggering a slide writes nothing: what an action sets a timer up as is kept here,
// shown like any other setting, and goes into the file with the next change made by
// hand.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace. While any timer runs it looks ten times a second for a change in
// the whole seconds any of them shows, and says so with `tick`, which is once a second;
// while none runs, nothing runs here. Something that shows a running timer's hundredths
// has to be drawn far more often than that. It follows `beat` (or `slowBeat`, for a
// small picture in which hundredths cannot be read anyway), which only go while some
// such thing is asking for its text: see linkedText().
class Timers : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // { id, name, kind, duration, timeOfDay, startTime, endTime, hasEndTime, overrun }
    // for each timer, in the file's order. `kind` is "countdown", "countdownTo" or
    // "elapsed"; times are in seconds, and `timeOfDay` is from midnight.
    Q_PROPERTY(QVariantList timers READ timers NOTIFY changed)
    // Goes up whenever what any timer shows in whole seconds may have changed: read it
    // in a binding to have the binding follow the timers.
    Q_PROPERTY(int tick READ tick NOTIFY ticked)
    // Go up thirty times and five times a second while a text that shows the hundredths
    // of a running timer is being asked for, and not otherwise.
    Q_PROPERTY(int beat READ beat NOTIFY beaten)
    Q_PROPERTY(int slowBeat READ slowBeat NOTIFY slowBeaten)

public:
    // How one part of a time is written, as ProPresenter's files have it and as it
    // offers them: hidden; one digit; two digits; and one or two digits that are
    // hidden while the part is nothing and nothing larger is showing. For the
    // hundredths, one digit is tenths, and the last two hide it whenever it is nothing.
    enum Style { None = 0, Short = 1, Long = 2, RemoveShort = 3, RemoveLong = 4 };
    Q_ENUM(Style)
    // How the four parts of a time are written, and whether the hundredths are only
    // shown in the last minute
    struct Format
    {
        int hours = None;
        int minutes = None;
        int seconds = None;
        int hundredths = None;
        bool hundredthsUnderMinuteOnly = false;
    };
    // What a slide's action does to a timer, as ProPresenter's files have it
    enum Action { Start = 0, Stop = 1, Reset = 2, ResetAndStart = 3, StopAndReset = 4, Increment = 5 };

    explicit Timers(QObject *parent = nullptr);

    QVariantList timers() const { return m_timers; }
    int tick() const { return m_tick; }
    int beat() const { return m_beat; }
    int slowBeat() const { return m_slowBeat; }

    // Reads the timers of a workspace folder. A workspace with no timers file is shown
    // as having one countdown, which is written with the first change made. With `held`
    // nothing runs, started or not, and a countdown to a time of day, which has no start
    // of its own to stand at, stands at nothing: what the timers show is then the same
    // at any time, which is what the self-test's pictures need. Returns an error
    // message, empty on success; a file that cannot be read is left alone, and there
    // are then no timers.
    Q_INVOKABLE QString open(const QString &workspace, bool held = false);

    // How a timer stands: { running, seconds, text }, the text as hours, minutes and
    // seconds.
    Q_INVOKABLE QVariantMap state(const QString &id) const;
    // What a text box linked to a timer shows: the timer's time written with the given
    // styles for its hours, minutes, seconds and hundredths, put into `pattern` where
    // that has "${timer}". The timer is found by id, or failing that by name; if there
    // is none, the time is nothing. Asking for the hundredths of a timer that is running
    // is what keeps `beat` and `slowBeat` going, so whatever shows them should read one
    // of those in the binding that asks.
    Q_INVOKABLE QString linkedText(const QString &id, const QString &name, int hours, int minutes, int seconds,
                                   int hundredths, bool hundredthsUnderMinuteOnly, const QString &pattern);
    // The timer a link means, by id or failing that by name: its id, or "" if there is
    // none.
    Q_INVOKABLE QString linkedTimer(const QString &id, const QString &name) const;
    // Whether a timer is as a rule for when an element shows asks, the timer being
    // found as a link's is. `criterion` is as ProPresenter's files have it: 0 it has
    // time left, 1 it has run out, 2 it is running, 3 it is not. A timer that is not
    // here is not running, and has neither time left nor run out. Whatever asks should
    // read `tick` in the same binding, to be asked again when the answer may differ.
    Q_INVOKABLE bool meets(const QString &id, const QString &name, int criterion) const;

    Q_INVOKABLE void start(const QString &id);
    Q_INVOKABLE void stop(const QString &id);
    Q_INVOKABLE void reset(const QString &id);
    // Does what a slide's timer action asks. The action is a map as a slide carries it
    // (see ProDocument): `action` (an Action), `timerId` and `timerName` (the timer is
    // found by id, or failing that by name), `amount` (the seconds an Increment adds)
    // and, if the action says how the timer is to be set up, `configuration` (that
    // part of the file, in base 64).
    Q_INVOKABLE void act(const QVariantMap &action);

    // Changes to the file. Each returns an error message, empty on success, or with it
    // as `error` beside what else it returns.
    // Adds a five-minute countdown. Returns { id, error }.
    Q_INVOKABLE QVariantMap add();
    Q_INVOKABLE QString remove(const QString &id);
    // Changes whichever of name, kind, duration, timeOfDay, startTime, endTime,
    // hasEndTime and overrun are given, and puts the timer back at its start.
    Q_INVOKABLE QString configure(const QString &id, const QVariantMap &changes);

    // A time, in seconds, written as the format says (see "How a time is written",
    // above). `countsDown` says which way it is rounded to the smallest part shown.
    static QString written(double seconds, bool countsDown, const Format &format);
    // That, put into `pattern` where the pattern has "${timer}".
    static QString linked(double seconds, bool countsDown, const Format &format, const QString &pattern);

signals:
    void changed();
    void ticked();
    void beaten();
    void slowBeaten();

private:
    // How a timer is running
    struct Run
    {
        bool running = false;
        // Not started since it was last put back at its start
        bool fresh = true;
        // Where it had got to when it was last started or stopped, in seconds
        double base = 0;
        QElapsedTimer since;
        // The whole second it last showed
        qint64 shown = 0;
    };

    QString path() const;
    QString read(rv::data::TimersDocument *document) const;
    QString readAsSetUp(rv::data::TimersDocument *document) const;
    QString write(const rv::data::TimersDocument &document);
    void show(const rv::data::TimersDocument &document);
    void startClockTimers();
    const rv::data::Timer *find(const QString &id, const QString &name = {}) const;
    double seconds(const rv::data::Timer &timer, const Run &run) const;
    static qint64 shownSeconds(const rv::data::Timer &timer, double seconds);
    void look();
    void bump();
    void pulse();

    QString m_workspace;
    bool m_held = false;
    rv::data::TimersDocument m_document;
    QVariantList m_timers;
    QHash<QString, Run> m_runs;
    // How slides have set timers up since the file was last written, by timer id: each
    // a Timer.Configuration as it goes into the file
    QHash<QString, std::string> m_setBySlides;
    QTimer m_clock;
    int m_tick = 0;
    // For what shows hundredths: goes while it is being asked for, which `m_asked`
    // keeps the time of
    QTimer m_fastClock;
    QElapsedTimer m_asked;
    int m_beat = 0;
    int m_slowBeat = 0;
};
