#pragma once

#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class QQuickWindow;
class QVideoSink;

// What became of each frame of a playing video on its way to the output (see
// Benchmark::watchVideo).
//
// A frame of a video reaches the output in three steps, on three threads. Qt
// Multimedia's player hands it to the output's VideoOutput at the moment it is due, on
// a thread of the player's own. The VideoOutput keeps it and asks the app's own thread,
// by a queued call, to mark it changed; only then can the output window's next sync,
// which the app's thread also takes part in, take the frame into what is drawn. Then
// the output's drawing thread draws it and the frame is swapped onto the screen. A
// frame can be late at either step: the app's thread busy with something else (a
// slide's items being made), or the drawing thread busy (a slide's text being drawn,
// which happens in the sync).
//
// So this keeps the time of each of those, for every frame: when it came from the
// player; when the app's thread got to a queued call of this one's, made at the same
// moment as the VideoOutput's own and so run straight after it; and the start and end
// of every sync of the output window and the time of every swap. From those it works
// out which frame of the video each frame of the output showed, and so how long each
// frame of the video was on the screen. (A sync is taken to draw the newest frame that
// had come before it began, if the app's thread had got to one since the sync before;
// the VideoOutput reads its frame during the sync, so that is near enough.)
//
// The times are written from three threads, under a lock: a few doubles a frame.
class VideoTiming : public QObject
{
    Q_OBJECT
public:
    VideoTiming(QQuickWindow *window, QVideoSink *sink, QObject *parent = nullptr);
    // What has happened since this was made, and stops keeping it: see
    // Benchmark::videoTiming
    QVariantMap summary();
    // The times themselves, for a trace
    QVector<double> arrivedTimes() { QMutexLocker locked(&m_lock); return m_arrived; }
    QVector<double> seenTimes() { QMutexLocker locked(&m_lock); return m_seen; }
    QVector<double> syncFromTimes() { QMutexLocker locked(&m_lock); return m_syncFrom; }
    QVector<double> syncToTimes() { QMutexLocker locked(&m_lock); return m_syncTo; }
    QVector<double> swappedTimes() { QMutexLocker locked(&m_lock); return m_swapped; }

private:
    QMutex m_lock;
    // When each frame came from the player, and when the app's thread got to it
    QVector<double> m_arrived;
    QVector<double> m_seen;
    // When each sync of the output began and ended, and each swap
    QVector<double> m_syncFrom;
    QVector<double> m_syncTo;
    QVector<double> m_swapped;
    QPointer<QQuickWindow> m_window;
    QPointer<QVideoSink> m_sink;
};

// The benchmark (--benchmark <file>): times the things an operator waits for, on a
// workspace of its own making, and writes them to a file as a line each.
//
// It is for telling whether a change to the app has made it slower. So what it times
// has to be the same from one run to the next, which a real workspace is not: the
// benchmark makes its own, with three presentations of fixed content (words; shapes,
// gradients and pictures cut to shapes; words over pictures), a stage layout whose one
// shape is a Screen Preview of the output, and, where benchmark/run.py has made a
// video and named it in SP_BENCHMARK_VIDEO, a fourth presentation of words over that
// video; and throws it away when it is done. The user's workspaces and settings are
// not read or touched.
//
// The run itself is qml/Benchmark.qml, which works the app as an operator would. This
// is what that needs and QML cannot do: a fine clock, the processor time and memory the
// app has used, and somewhere to write the answers.
//
// benchmark/run.py runs it several times over and compares the result with
// benchmark/baseline.txt; see benchmark/README.md.
// How evenly something that moves moves on the output: at every sync of the window (on
// its drawing thread, while the app's thread waits for the sync, as Qt's own reading of
// the items for the frame does), the number that says where it is (a property of an
// item: how far the words of a scrolling text box have gone). What a frame draws is
// where it was then, so each frame's step is the difference from the frame before; on a
// screen drawing at every refresh, steps that are even are moving that looks even.
class MotionTiming : public QObject
{
    Q_OBJECT
public:
    MotionTiming(QQuickWindow *window, QObject *item, const QByteArray &property, QObject *parent = nullptr);
    // frames, and uneven: how many frames' steps were a quarter more or less than the
    // middle one, and stopped: how many did not move it at all, while it moved
    QVariantMap summary();

private:
    QMutex m_lock;
    QVector<double> m_where;
    QPointer<QQuickWindow> m_window;
    QPointer<QObject> m_item;
    QByteArray m_property;
};

class Benchmark : public QObject
{
    Q_OBJECT

public:
    Benchmark(const QString &reportPath, QObject *parent = nullptr);

    // Writes the benchmark's workspace into `folder`, which should be empty. Returns
    // an error message, empty on success.
    static QString makeWorkspace(const QString &folder);
    // Milliseconds from when the program was started to now: counted from the start of
    // the process, before any of the program's own code ran.
    static double sinceStart();

    // The same, for the run: every time in the report is on this clock.
    Q_INVOKABLE double now() const { return sinceStart(); }
    // One of the app's windows, by its name ("stage"), for the run to count its frames;
    // null if there is none
    Q_INVOKABLE QObject *window(const QString &name) const;
    // Milliseconds of processor time the app has used, on all its threads
    Q_INVOKABLE double cpu() const;
    // Megabytes of memory the app holds now, and the most it has held
    Q_INVOKABLE double memory() const;
    Q_INVOKABLE double peakMemory() const;
    // Begins keeping what becomes of each frame of the video playing into `sink` (a
    // QVideoSink, the output's VideoOutput's) on its way to `window` (the output's), from
    // now until videoTiming() is asked for: see VideoTiming.
    Q_INVOKABLE void watchVideo(QObject *window, QObject *sink);
    // What became of them, and stops keeping it: frames (how many came from the
    // player), others (how many frames the output drew that took no new frame of the
    // video: for something else that changed; and othersAt, when each one's sync began,
    // on the clock of sinceStart), notShown (how many no frame of the output
    // showed), held (how many were on
    // the screen for longer than a frame of the video lasts, by more than a quarter of
    // that: the stutter), interval (the milliseconds a frame of the video lasts), and the
    // longest, in milliseconds: waitForThread (from a frame coming to the app's thread
    // getting to it), sync (a sync of the output), toScreen (from a frame coming to its
    // being on the screen). Empty if nothing was being watched.
    //
    // Where SP_BENCHMARK_VIDEO_TRACE names a folder, every time kept is written there as
    // well, a file for each time this is asked, with `marks` (the times the run did
    // something, on the run's clock), for reading the frames against them.
    Q_INVOKABLE QVariantMap videoTiming(const QVariantList &marks = {});
    // Begins keeping how evenly `property` of `item` changes at each sync of `window`, and
    // what was kept (see MotionTiming); each stops keeping it
    Q_INVOKABLE void watchMotion(QObject *window, QObject *item, const QString &property);
    Q_INVOKABLE QVariantMap motionTiming();
    // One measurement, for the report, said on the terminal as it is made
    Q_INVOKABLE void record(const QString &name, double value, const QString &unit);
    // Writes the report and ends the app.
    Q_INVOKABLE void finish();

private:
    QString m_reportPath;
    QStringList m_lines;
    VideoTiming *m_video = nullptr;
    MotionTiming *m_motion = nullptr;
};
