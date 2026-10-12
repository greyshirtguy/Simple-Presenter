#include "clocks.h"

#include "clocktext.h"
#include "timers.h"

#include <QDateTime>
#include <QLocale>

#include <cmath>

Clocks::Clocks(QObject *parent)
    : QObject(parent)
{
    m_tick.setSingleShot(true);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        // Those who show the clock ask for it again as they are told of the second, at
        // once: if nobody has, nobody is showing it, and it stops.
        m_clockAsked = false;
        m_ticking = true;
        ++m_second;
        emit secondChanged();
        m_ticking = false;
        if (m_clockAsked && !m_held)
            waitForNextSecond();
    });
}

// Until just after the second turns, so that what is shown changes when a clock on the
// wall does and not some part of a second later.
void Clocks::waitForNextSecond()
{
    m_tick.start(1000 - QTime::currentTime().msec() + 5);
}

void Clocks::setHeld(bool held)
{
    if (held == m_held)
        return;
    m_held = held;
    emit heldChanged();
    ++m_second;
    emit secondChanged();
}

QString Clocks::clockText(int dateStyle, int timeStyle, bool twentyFourHour, const QString &pattern)
{
    m_clockAsked = true;
    if (!m_held && !m_ticking && !m_tick.isActive())
        waitForNextSecond();
    const QDateTime moment = m_held ? QDateTime(QDate(2000, 1, 1), QTime(12, 0, 0)) : QDateTime::currentDateTime();
    return clocktext::linked(moment, dateStyle, timeStyle, twentyFourHour, pattern, QLocale());
}

QString Clocks::videoText(int hours, int minutes, int seconds, int hundredths, bool hundredthsUnderMinuteOnly, bool ignoreLooping,
                          const QString &pattern)
{
    m_videoAsked = true;
    if (hundredths != Timers::None)
        m_videoFine = true;
    if (!m_videoWatched)
        settleWatched();
    const Timers::Format format {hours, minutes, seconds, hundredths, hundredthsUnderMinuteOnly};
    const bool counted = m_videoLeft >= 0 && !(ignoreLooping && m_videoLoops) && !m_held;
    return Timers::linked(counted ? m_videoLeft : 0, true, format, pattern);
}

// Whether the video is watched follows whether its time has been asked for, a moment
// after and not on the spot. Where it changes is in the middle of something: a box
// working out its words, or the window telling this of the video. What the window does
// about being told (it starts, or stops, telling this of the video) would come round to
// that same box or that same telling before it was done.
void Clocks::settleWatched()
{
    if (m_settlePending)
        return;
    m_settlePending = true;
    QMetaObject::invokeMethod(this, [this] {
        m_settlePending = false;
        if (m_videoAsked == m_videoWatched)
            return;
        m_videoWatched = m_videoAsked;
        emit videoWatchedChanged();
    }, Qt::QueuedConnection);
}

// Tells those who show the video's time that it has changed. They ask for it again at
// once; if nobody does, nobody is showing it, and the video need not be watched.
void Clocks::told()
{
    m_videoAsked = false;
    m_videoFine = false;
    ++m_videoRevision;
    emit videoChanged();
    if (!m_videoAsked && m_videoWatched)
        settleWatched();
}

void Clocks::setVideo(double secondsLeft, bool loops)
{
    secondsLeft = qMax(0.0, secondsLeft);
    // A player says where it has got to many times a second. What is shown changes
    // with the seconds, so only a new second is told of, unless some box shows
    // hundredths.
    const bool news = loops != m_videoLoops || m_videoLeft < 0
                      || (m_videoFine ? secondsLeft != m_videoLeft : std::floor(secondsLeft) != std::floor(m_videoLeft));
    m_videoLeft = secondsLeft;
    m_videoLoops = loops;
    if (news)
        told();
}

void Clocks::noVideo()
{
    if (m_videoLeft < 0)
        return;
    m_videoLeft = -1;
    m_videoLoops = false;
    told();
}
