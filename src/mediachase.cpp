#include "mediachase.h"

#include "sessionlog.h"

#include <QMediaMetaData>
#include <QVideoFrame>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace {

qint64 steady()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// How far ahead of where the timeline is a video is sent, in milliseconds: what it was
// behind by, once settled, at the jumps before (see the header). Nothing, with a player
// that makes its wait up, which is what there is. Kept between videos.
double s_ahead = 0;
// The most a video is ever sent ahead, and the most times it is sent again for one asking
constexpr double kMostAhead = 1500;
constexpr int kMostAgain = 2;
// How long a picture is waited for before it is given up; and how long after its picture
// a video is left to settle before it is looked at (the frames it was late with are
// hurried through in a hundredth of a second or two, on the laptop this is made on)
constexpr qint64 kGivenUp = 4'000'000'000;
constexpr qint64 kSettle = 250'000'000;

// SP_CHASE_TRACE=1 in the environment has every frame of the half second after each
// asking written to the terminal (when it came, and which it was): for looking at what
// a player does at a jump, frame by frame, on a computer where it does it badly.
bool traced()
{
    static const bool on = qEnvironmentVariableIntValue("SP_CHASE_TRACE") != 0;
    return on;
}

}

MediaChase::MediaChase(QObject *parent) : QObject(parent), m_seen(std::make_shared<Seen>())
{
    m_look.setInterval(2);
    m_look.setTimerType(Qt::PreciseTimer);
    connect(&m_look, &QTimer::timeout, this, &MediaChase::look);
    m_release.setSingleShot(true);
    m_release.setTimerType(Qt::PreciseTimer);
    connect(&m_release, &QTimer::timeout, this, [this] {
        if (m_player)
            m_player->play();
    });
}

MediaChase::~MediaChase()
{
    disconnect(m_listening);
}

void MediaChase::setPlayer(QObject *player)
{
    auto *mediaPlayer = qobject_cast<QMediaPlayer *>(player);
    if (mediaPlayer == m_player)
        return;
    m_player = mediaPlayer;
    emit playerChanged();
}

void MediaChase::setSink(QObject *sink)
{
    auto *videoSink = qobject_cast<QVideoSink *>(sink);
    if (videoSink == m_sink)
        return;
    const bool listening = bool(m_listening);
    disconnect(m_listening);
    m_listening = {};
    m_sink = videoSink;
    if (listening)
        listen();
    emit sinkChanged();
}

double MediaChase::frameLength() const
{
    const double rate = m_player ? m_player->metaData().value(QMediaMetaData::VideoFrameRate).toDouble() : 0;
    return rate > 1 ? 1000.0 / rate : 1000.0 / 30;
}

double MediaChase::place(qint64 now, bool playing) const
{
    const qint64 pts = m_seen->pts.load();
    const qint64 at = m_seen->at.load();
    if (pts < 0 || now - at > 150'000'000)
        return -1;
    return pts / 1000.0 + (playing ? (now - at) / 1e6 : 0);
}

void MediaChase::listen()
{
    if (m_listening || !m_sink)
        return;
    // On the thread the frames are handed over on, and touching nothing but what is
    // noted of them (which outlives this, if this goes while a frame is being noted)
    m_listening = connect(m_sink, &QVideoSink::videoFrameChanged, m_sink, [seen = m_seen](const QVideoFrame &frame) {
        if (!frame.isValid())
            return;
        const qint64 pts = frame.startTime();
        const qint64 now = steady();
        const qint64 before = seen->pts.exchange(pts);
        seen->at.store(now);
        const qint64 from = seen->from.load();
        if (from < 0)
            return;
        // The first picture from where it was sent: at or past there, and not the one
        // that was showing given again
        const bool first = seen->landedPts.load() < 0 && pts != before && pts >= from && pts < from + 3'000'000;
        if (traced() && now - seen->askedAt.load() < 500'000'000)
            fprintf(stderr, "chase: +%6.1f ms  frame at %9.1f ms%s\n", (now - seen->askedAt.load()) / 1e6, pts / 1e3, first ? "  <- the picture" : "");
        if (first) {
            seen->landedAt.store(now);
            seen->landedPts.store(pts);
        }
    }, Qt::DirectConnection);
}

void MediaChase::send(double milliseconds)
{
    if (!m_player)
        return;
    listen();
    m_release.stop();
    const qint64 now = steady();
    const bool playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    if (!m_waiting && !m_settling) {
        // Where the video is: by its last frame and how long ago that came, where one
        // came lately; else by what the player says
        double where = place(now, playing);
        if (where < 0)
            where = double(m_player->position());
        if (std::abs(where - milliseconds) <= frameLength() * 1.5) {
            if (!playing)
                m_player->play();
            return;
        }
        // Held a little past where the timeline now is (the timecode stopped, which is
        // known a few frames after it happens, and has started again from where it
        // stopped): it stays held until the timeline has come to it, and plays on in step
        // with nothing to decode.
        if (!playing && where > milliseconds && where - milliseconds <= 400) {
            m_release.start(int(std::lround(where - milliseconds)));
            return;
        }
    }
    m_again = 0;
    m_firstAskedAt = now;
    seek(milliseconds, now);
}

void MediaChase::seek(double target, qint64 now)
{
    m_target = target;
    m_askedAt = now;
    const double to = std::max(0.0, target + std::min(s_ahead, kMostAhead));
    m_seen->landedPts.store(-1);
    m_seen->askedAt.store(now);
    // (A frame a little before where it was sent counts: a player lands on the frame
    // that holds the place, which starts before it.)
    m_seen->from.store(qint64((to - frameLength() * 1.5) * 1000));
    m_waiting = true;
    m_settling = false;
    m_player->setPosition(qint64(std::llround(to)));
    if (m_player->playbackState() != QMediaPlayer::PlayingState)
        m_player->play();
    m_look.start();
}

void MediaChase::look()
{
    const qint64 now = steady();
    if (m_waiting) {
        const qint64 pts = m_seen->landedPts.load();
        if (pts < 0) {
            if (now - m_askedAt > kGivenUp) {
                m_look.stop();
                m_waiting = false;
                m_seen->from.store(-1);
                SessionLog::write("media", QStringLiteral("video \"%1\" sent to %2 s by its timeline: no picture came from there").arg(m_name).arg(m_target / 1000, 0, 'f', 2));
            }
            return;
        }
        // Its picture has come: how long that took; and now it is left to settle
        m_waiting = false;
        m_settling = true;
        m_cameAt = m_seen->landedAt.load();
        m_wait = (m_cameAt - m_firstAskedAt) / 1e6;
        m_seen->from.store(-1);
        m_look.setInterval(10);
        return;
    }
    if (!m_settling) {
        m_look.stop();
        return;
    }
    if (now - m_cameAt < kSettle)
        return;
    m_look.stop();
    m_look.setInterval(2);
    m_settling = false;
    // How far the video is from where the timeline now is in it (which has gone on
    // since the asking by as long as that is ago)
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
    const double where = place(now, playing);
    settled(now, where < 0 ? 0 : where - (m_target + (now - m_askedAt) / 1e6));
}

void MediaChase::settled(qint64 now, double out)
{
    const double frame = frameLength();
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
    // What it was behind by is allowed for at the next jump, and what it was ahead by
    // taken off what is allowed (most of it, each time)
    s_ahead = std::clamp(s_ahead - out * 0.7, 0.0, kMostAhead);
    if (out < -frame * 1.5 && m_again < kMostAgain && playing) {
        // Behind: sent again, to where the timeline now is and as much further on
        ++m_again;
        s_ahead = std::clamp(std::max(s_ahead, -out), 0.0, kMostAhead);
        seek(m_target + (now - m_askedAt) / 1e6, now);
        return;
    }
    bool held = false;
    if (out > frame * 1.5 && playing) {
        // Ahead: held on its picture until the timeline has come to it
        m_player->pause();
        m_release.start(int(std::lround(out)));
        held = true;
    }
    ++m_landings;
    m_last = {{"wait", m_wait}, {"out", out}, {"held", held}, {"again", m_again}};
    SessionLog::write("media", QStringLiteral("video \"%1\" sent to %2 s by its timeline: its picture came after %3 ms%4, and it was then %5")
                                   .arg(m_name)
                                   .arg(m_target / 1000, 0, 'f', 2)
                                   .arg(std::lround(m_wait))
                                   .arg(m_again > 0 ? QStringLiteral(" (sent %1, having been behind)").arg(m_again == 1 ? QStringLiteral("twice") : QStringLiteral("three times")) : QString())
                                   .arg(held ? QStringLiteral("%1 ms ahead, and was held that long").arg(std::lround(out))
                                             : std::abs(out) <= frame ? QStringLiteral("in step (within a frame)")
                                             : QStringLiteral("%1 ms %2").arg(std::lround(std::abs(out))).arg(out > 0 ? QStringLiteral("ahead") : QStringLiteral("behind"))));
    emit landed();
}

void MediaChase::keep(double milliseconds)
{
    if (!m_player || !m_listening || m_waiting || m_settling || m_release.isActive() || m_player->playbackState() != QMediaPlayer::PlayingState)
        return;
    const qint64 now = steady();
    const double where = place(now, true);
    if (where < 0 || now - m_keptAt < 5'000'000'000)
        return;
    const double out = where - milliseconds;
    // (A little out is nothing; and a great deal out is not the two clocks coming apart
    // but something else: a video that goes round, at the moment it does, or one moved
    // by its own transport, which is left as it was put until the timecode next jumps.)
    if (std::abs(out) <= frameLength() * 2 || std::abs(out) > 1000)
        return;
    m_keptAt = now;
    SessionLog::write("media", QStringLiteral("video \"%1\" had come %2 ms %3 its timeline, and is sent to where that is")
                                   .arg(m_name).arg(std::lround(std::abs(out))).arg(out > 0 ? QStringLiteral("ahead of") : QStringLiteral("behind")));
    m_again = 0;
    m_firstAskedAt = now;
    seek(milliseconds, now);
}

void MediaChase::hold()
{
    m_release.stop();
    m_look.stop();
    m_look.setInterval(2);
    m_waiting = false;
    m_settling = false;
    m_seen->from.store(-1);
    if (m_player)
        m_player->pause();
}
