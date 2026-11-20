#include "timelineplayer.h"

#include "timeline.h"

#include <cmath>

namespace {

// How far from where the timecode says it is a timeline that follows it may be, and be
// put right where it stands: a frame and a half at thirty a second. Further than that is
// the timecode having gone somewhere else.
constexpr double kSlip = 0.05;

}

TimelinePlayer::TimelinePlayer(QObject *parent) : QObject(parent)
{
    m_due.setSingleShot(true);
    m_due.setTimerType(Qt::PreciseTimer);
    connect(&m_due, &QTimer::timeout, this, &TimelinePlayer::due);
    m_tick.setInterval(100);
    connect(&m_tick, &QTimer::timeout, this, &TimelinePlayer::positionChanged);
}

QVariantMap TimelinePlayer::timeline() const
{
    if (m_recorded.isEmpty())
        return m_timeline;
    QVariantMap shown = m_timeline;
    QVariantList cues = m_cues;
    for (const QVariant &entry : m_recorded) {
        QVariantMap cue = entry.toMap();
        cue.insert("recorded", true);
        cue.insert("place", -1);
        cues.append(cue);
    }
    shown.insert("cues", cues);
    return shown;
}

double TimelinePlayer::duration() const
{
    const double length = m_timeline.value("duration").toDouble();
    return length > 0 ? length : timeline::kUsualDuration;
}

double TimelinePlayer::position() const
{
    if (!m_playing)
        return m_base;
    return qMin(duration(), m_base + m_clock.nsecsElapsed() / 1e9);
}

void TimelinePlayer::setWatched(bool watched)
{
    if (m_watched == watched)
        return;
    m_watched = watched;
    if (m_watched && m_playing)
        m_tick.start();
    else
        m_tick.stop();
    emit watchedChanged();
}

void TimelinePlayer::setRecording(bool recording)
{
    if (m_recording == recording || (recording && m_path.isEmpty()))
        return;
    m_recording = recording;
    emit recordingChanged();
}

void TimelinePlayer::load(const QString &path, const QString &key, const QString &playlistId, const QVariantMap &timeline)
{
    const bool same = path == m_path && key == m_key && playlistId == m_playlistId;
    if (!same) {
        setFollowing(false);
        halt();
        m_base = 0;
        m_recorded.clear();
        if (m_recording) {
            m_recording = false;
            emit recordingChanged();
        }
    }
    m_path = path;
    m_key = key;
    m_playlistId = playlistId;
    m_timeline = timeline;
    m_cues = timeline.value("cues").toList();
    if (!same)
        emit loadedChanged();
    emit timelineChanged();
    emit positionChanged();
    if (m_playing) {
        // (Read again while it plays: the cues there are now, from where it has got to.
        // One moved to a time it has passed is not done; one moved past where it is, is.)
        m_doneTo = qMin(m_doneTo, position());
        schedule();
    } else if (m_base > duration()) {
        m_base = duration();
    }
}

bool TimelinePlayer::holds(const QString &key, const QString &playlistId) const
{
    return !m_path.isEmpty() && key == m_key && playlistId == m_playlistId;
}

void TimelinePlayer::play()
{
    if (m_playing || m_path.isEmpty())
        return;
    // (From its end, from its start: A GUESS at ProPresenter's Play there.)
    if (m_base >= duration())
        m_base = 0;
    m_playing = true;
    ++m_run;
    m_clock.start();
    emit playingChanged();
    if (m_watched)
        m_tick.start();
    enter(m_base);
}

void TimelinePlayer::enter(double at)
{
    const int run = m_run;
    m_doneTo = at;
    for (const timeline::Entry &entry : timeline::entering(m_cues, at)) {
        emit cueDue(m_cues.at(entry.place).toMap(), entry.offset, true);
        if (run != m_run)
            return;
    }
    schedule();
}

void TimelinePlayer::schedule()
{
    if (!m_playing)
        return;
    const double next = timeline::nextTime(m_cues, m_doneTo);
    const double at = next < 0 || next > duration() ? duration() : next;
    m_due.start(int(std::max(0.0, std::ceil((at - position()) * 1000))));
}

bool TimelinePlayer::tell(const QList<int> &places, double now)
{
    const int run = m_run;
    for (int place : places) {
        const QVariantMap cue = m_cues.at(place).toMap();
        // (Audio or media whose time has just gone by is started where the timeline is
        // in it: a millisecond or two, when the timer is a little late.)
        const QString track = cue.value("track").toString();
        const double offset = track == QLatin1String("audio") || track == QLatin1String("media")
                                  ? std::max(0.0, now - cue.value("time").toDouble()) : 0.0;
        emit cueDue(cue, offset < 0.05 ? 0.0 : offset, false);
        if (run != m_run)
            return false;
    }
    return true;
}

void TimelinePlayer::due()
{
    if (!m_playing)
        return;
    const double now = position();
    const double end = duration();
    const double to = std::min(now, end);
    const double from = m_doneTo;
    m_doneTo = to;
    if (!tell(timeline::passed(m_cues, from, to), to))
        return;
    if (now < end) {
        schedule();
        return;
    }
    if (m_timeline.value("loop").toBool() && !m_following) {
        // Round again: from the start, as at the start
        m_base = 0;
        ++m_run;
        m_clock.start();
        emit positionChanged();
        enter(0);
        return;
    }
    m_base = end;
    halt();
}

void TimelinePlayer::halt()
{
    if (m_playing)
        m_base = position();
    m_due.stop();
    m_tick.stop();
    ++m_run;
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
    emit positionChanged();
}

void TimelinePlayer::pause()
{
    setFollowing(false);
    if (!m_playing)
        return;
    halt();
    emit paused();
}

void TimelinePlayer::stop()
{
    setFollowing(false);
    halt();
}

void TimelinePlayer::setFollowing(bool following)
{
    if (m_following == following)
        return;
    m_following = following;
    emit followingChanged();
}

void TimelinePlayer::follow(double seconds)
{
    if (m_path.isEmpty())
        return;
    const double at = std::clamp(seconds, 0.0, duration());
    setFollowing(true);
    if (!m_playing) {
        // (At its very end there is nothing to play: it stays there.)
        m_base = at;
        if (at >= duration()) {
            emit positionChanged();
            return;
        }
        play();
        return;
    }
    const double out = at - position();
    if (std::abs(out) > kSlip) {
        seek(at);
        return;
    }
    // A little out: put right, and the next cue waited for from where it now is. (A cue
    // in the little it was put on by is done at once by that; one in the little it was
    // put back by has been done, and is not done again.)
    m_base += out;
    schedule();
    if (m_watched)
        emit positionChanged();
}

void TimelinePlayer::unfollow()
{
    if (!m_following)
        return;
    setFollowing(false);
    if (m_playing) {
        halt();
        emit paused();
    }
}

void TimelinePlayer::seek(double seconds)
{
    const double at = std::clamp(seconds, 0.0, duration());
    if (!m_playing) {
        m_base = at;
        emit positionChanged();
        return;
    }
    halt();
    emit paused();
    m_base = at;
    play();
}

void TimelinePlayer::wentLive(const QString &key, const QString &playlistId)
{
    if (m_playing && !holds(key, playlistId))
        stop();
}

void TimelinePlayer::record(const QVariantMap &cue)
{
    if (!m_recording)
        return;
    QVariantMap recorded = cue;
    recorded.insert("time", std::round(position() * 1000) / 1000);
    m_recorded.append(recorded);
    emit timelineChanged();
}

QVariantList TimelinePlayer::takeRecorded()
{
    QVariantList taken;
    for (const QVariant &entry : std::as_const(m_recorded)) {
        const QVariantMap cue = entry.toMap();
        QVariantMap written {{"time", cue.value("time")}, {"name", cue.value("name")}};
        if (cue.value("track").toString() == QLatin1String("slide"))
            written.insert("slideId", cue.value("slideId"));
        else
            written.insert("wanted", cue.value("wanted"));
        taken.append(written);
    }
    m_recorded.clear();
    emit timelineChanged();
    return taken;
}

int TimelinePlayer::slidePlace(const QStringList &ids, const QString &slideId, int after) const
{
    return timeline::slidePlace(ids, slideId, after);
}
