#include "frameclock.h"

#include "framepacing.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QVideoSink>

FrameClock::FrameClock(QQuickItem *parent) : QQuickItem(parent)
{
}

FrameClock::~FrameClock()
{
    if (m_frames)
        m_frames->remove(this);
}

double FrameClock::milliseconds()
{
    static const QElapsedTimer started = [] {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();
    return double(started.nsecsElapsed()) / 1e6;
}

double FrameClock::frameTime()
{
    // (Only ever asked on the app's own thread.) Kept until the thread has finished what it
    // is doing and turns to the next thing it was asked: a call posted to it now is run
    // then, after whatever was asked before it. (A posted call, the cheapest of the
    // thread's ways of being told something: a timer for it would be made and thrown away
    // a hundred times a second. And not when the thread next waits, which a busy thread
    // may not do for a long while, the time standing still meanwhile.)
    static double taken = -1e9;
    static bool thisTurn = false;
    const double now = milliseconds();
    if (thisTurn || now - taken < 2.0)
        return taken;
    taken = now;
    thisTurn = true;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [] { thisTurn = false; }, Qt::QueuedConnection);
    return taken;
}

double FrameClock::now() const
{
    return frameTime();
}

void FrameClock::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    if (running && m_frames)
        m_frames->clockStarted();
    emit runningChanged();
}

void FrameClock::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange)
        attach(value.window);
}

void FrameClock::attach(QQuickWindow *window)
{
    if (m_frames)
        m_frames->remove(this);
    m_frames = window ? WindowFrames::of(window) : nullptr;
    if (m_frames) {
        m_frames->add(this);
        if (m_running)
            m_frames->clockStarted();
    }
}

FramePace::FramePace(QQuickItem *parent) : QQuickItem(parent)
{
}

FramePace::~FramePace()
{
    if (m_frames)
        m_frames->setPace(nullptr);
}

QObject *FramePace::video() const
{
    return m_video;
}

void FramePace::setVideo(QObject *video)
{
    auto *sink = qobject_cast<QVideoSink *>(video);
    if (sink == m_video)
        return;
    m_video = sink;
    if (m_frames)
        m_frames->setPace(sink);
    emit videoChanged();
}

void FramePace::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange)
        attach(value.window);
}

void FramePace::attach(QQuickWindow *window)
{
    if (m_frames)
        m_frames->setPace(nullptr);
    m_frames = window ? WindowFrames::of(window) : nullptr;
    if (m_frames)
        m_frames->setPace(m_video);
}

WindowFrames *WindowFrames::of(QQuickWindow *window)
{
    if (auto *frames = window->findChild<WindowFrames *>(QString(), Qt::FindDirectChildrenOnly))
        return frames;
    return new WindowFrames(window);
}

WindowFrames::WindowFrames(QQuickWindow *window) : QObject(window), m_window(window)
{
    // A frame handed to the screen: said on the window's drawing thread (or on this one,
    // for a screen the app draws itself, for NDI or a DeckLink card), and taken here on
    // this one, after the frame is done with.
    connect(window, &QQuickWindow::afterFrameEnd, this, &WindowFrames::frameShown, Qt::QueuedConnection);
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &WindowFrames::drawAtNext);
    m_stopped.setSingleShot(true);
    connect(&m_stopped, &QTimer::timeout, this, [this] {
        if (anyRunning() && !videoPlaying())
            askForFrame();
    });
}

void WindowFrames::add(FrameClock *clock)
{
    m_clocks.append(clock);
}

void WindowFrames::remove(FrameClock *clock)
{
    m_clocks.removeAll(clock);
    m_clocks.removeAll(nullptr);
}

bool WindowFrames::anyRunning() const
{
    for (const QPointer<FrameClock> &clock : m_clocks) {
        if (clock && clock->running())
            return true;
    }
    return false;
}

// Milliseconds between the refreshes of the screen the window is on
double WindowFrames::refresh() const
{
    const QScreen *screen = m_window->screen();
    const double rate = screen ? screen->refreshRate() : 0;
    return rate > 1 ? 1000.0 / rate : 1000.0 / 60;
}

void WindowFrames::clockStarted()
{
    // The first frame of whatever has begun to move, which shows it where it begins: the
    // rest follow from it. (Not moved on now: this is said as it is started, while what
    // started it is still being set.)
    m_window->update();
}

// What moves is moved on to now, and the window asked for its next frame, which it will
// make when it can: at the screen's next refresh, or straight away if the screen has
// taken the last already
void WindowFrames::askForFrame()
{
    tick();
    m_window->update();
}

void WindowFrames::setPace(QVideoSink *video)
{
    if (video == m_pace)
        return;
    disconnect(m_paceConnection);
    m_pace = video;
    m_arrivals.clear();
    m_between.clear();
    m_timer.stop();
    m_stopped.stop();
    // (Queued, on this thread: the player hands each frame over on a thread of its own,
    // and the VideoOutput showing it has its own call to this thread queued just before
    // this one, to mark itself changed, so this is run as the frame is about to be drawn:
    // when it is drawn is what the moments between are counted from.)
    if (video)
        m_paceConnection = connect(video, &QVideoSink::videoFrameChanged, this, &WindowFrames::videoFrame, Qt::QueuedConnection);
    // (Something moving keeps going by the window's own refreshes, until the new video's
    // frames come, if there is one.)
    if (anyRunning())
        askForFrame();
}

void WindowFrames::tick()
{
    if (m_clocks.isEmpty())
        return;
    const double now = FrameClock::frameTime();
    // (A copy: a clock may stop, or go, as it is told.)
    const QList<QPointer<FrameClock>> clocks = m_clocks;
    for (const QPointer<FrameClock> &clock : clocks) {
        if (clock && clock->running())
            emit clock->ticked(now);
    }
}

// Whether the video is giving frames now, on a screen that is a window (a screen drawn by
// the app for NDI or a card draws at its own rate, and is not paced)
bool WindowFrames::videoPlaying() const
{
    return m_pace && m_window->isVisible() && !m_arrivals.isEmpty()
           && !framepacing::videoStopped(FrameClock::milliseconds() - m_arrivals.last(), framepacing::videoInterval(m_arrivals));
}

void WindowFrames::frameShown()
{
    if (!anyRunning())
        return;
    // A window on a screen that is playing a video draws when the video says (videoFrame),
    // unless it has stopped.
    if (!videoPlaying())
        askForFrame();
}

void WindowFrames::videoFrame()
{
    const double now = FrameClock::milliseconds();
    m_arrivals.append(now);
    while (m_arrivals.size() > 9)
        m_arrivals.removeFirst();
    m_between.clear();
    m_timer.stop();
    if (!anyRunning() || !m_window->isVisible())
        return;
    // Moved on in the frame the video's own frame is drawn in, which is already asked for
    tick();
    const double interval = framepacing::videoInterval(m_arrivals);
    for (const double after : framepacing::framesBetween(interval, refresh()))
        m_between.append(now + after);
    if (!m_between.isEmpty())
        m_timer.start(qMax(0, qRound(m_between.first() - FrameClock::milliseconds())));
    if (interval > 0)
        m_stopped.start(qRound(interval * 1.5) + 1);
}

void WindowFrames::drawAtNext()
{
    if (m_between.isEmpty())
        return;
    m_between.removeFirst();
    if (anyRunning())
        askForFrame();
    if (!m_between.isEmpty())
        m_timer.start(qMax(0, qRound(m_between.first() - FrameClock::milliseconds())));
}
