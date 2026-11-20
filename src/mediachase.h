#pragma once

#include <QMediaPlayer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Sends a video that is playing somewhere else in itself, for a timeline that has been
// put somewhere in it: its playhead moved while it plays, or the timecode it follows
// gone somewhere else (Main's "The timecode"; show::Effect::MediaSeek). ProPresenter
// keeps a video on a timeline to its timecode through such jumps without a frame seen
// to go missing (the maintainer, 2026-11-19), on computers a good deal faster than the
// one this is made on; this is the going at the same with the player there is, with
// nothing of the player rebuilt for it.
//
// What a jump costs. A video's frames are mostly kept as what has changed since the
// frame before, with a whole picture only every so often (every eight seconds or so, as
// most files are made): to show the frame at some place, the decoder has to go back to
// the whole picture before it and decode every frame from there. So between being asked
// and having the picture there is a wait, which no way of asking shortens: a hundredth
// of a second where the place is just after a whole picture, and three tenths of one
// where it is just before the next (a 1080p video made so, on the laptop this is made
// on, its graphics chip decoding; a 4K one made so, up to nine tenths). A video with a
// whole picture more often waits less: a 4K background as they are sold, with one every
// twenty-five frames, two to five hundredths. All that time the picture that was showing
// stays (never black); and all that time the timeline goes on.
//
// What the player does about that, and what is done here. Qt's player, sent somewhere
// while it plays, counts its time from the asking and not from the picture's coming: when
// the picture comes it hurries through the frames it is late with, and is in step, to a
// few thousandths of a second, a quarter of a second after (measured, with this, on Qt
// 6.10: every one of forty jumps in four videos). So the
// video is simply sent to where the timeline is, and left to it. But that is how one
// version of one player behaves, and is not relied on: a moment after each jump, how far
// the video then is from where the timeline is in it is looked at. Ahead by more than a
// frame and a half, it is held on its picture for the difference; behind by more, it is
// sent again, that much further on; and what it was behind by is allowed for at the next
// jump, by sending it that much ahead (which for a player that counted from the
// picture's coming would be its wait, and for this one is nothing). What each jump came
// to is written in the log: how long the picture took, and how far out the video was
// once it had settled.
//
// A video that is where it is wanted already (within a frame and a half) is left alone;
// one that is held there (the timecode stopped, and started again) is only played; and
// one held a little past there is played when the timeline has come to it.
//
// What it costs while nothing is sent anywhere: nothing; it listens for frames only from
// the first time it is asked, and then does no more for each than note which it was and
// when (on the thread that hands the frames over, so that the app being busy does not
// colour what is measured).
//
// GUESSES: that this is near what ProPresenter does at a jump at all, of which nothing
// is known but how it looks; and the frame and a half.
class MediaChase : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // The QMediaPlayer playing the video, and the QVideoSink its frames go to
    Q_PROPERTY(QObject *player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(QObject *sink READ sink WRITE setSink NOTIFY sinkChanged)
    // What the video is called, for the log
    Q_PROPERTY(QString name MEMBER m_name NOTIFY nameChanged)
    // How many times it has been sent somewhere and has settled there; and what the last
    // came to: { wait (ms from the asking to the first picture from there), out (ms the
    // video was ahead of the timeline once settled, behind if under 0), held (it was
    // ahead, and held for that long), again (times it was behind, and sent again) }
    Q_PROPERTY(int landings READ landings NOTIFY landed)
    Q_PROPERTY(QVariantMap last READ last NOTIFY landed)

public:
    explicit MediaChase(QObject *parent = nullptr);
    ~MediaChase() override;

    QObject *player() const { return m_player; }
    void setPlayer(QObject *player);
    QObject *sink() const { return m_sink; }
    void setSink(QObject *sink);
    int landings() const { return m_landings; }
    QVariantMap last() const { return m_last; }

    // The video belongs `milliseconds` into its file now, playing on from there
    Q_INVOKABLE void send(double milliseconds);
    // Paused where it is (the timecode has stopped); `send` starts it again
    Q_INVOKABLE void hold();
    // Looked at as the timecode goes on: the video belongs `milliseconds` in now, and
    // if it has come two frames or more away from there (its clock and the timecode's
    // not being one), it is sent there. Not more often than every five seconds, so that
    // a video that will not keep step is not for ever being sent.
    Q_INVOKABLE void keep(double milliseconds);

signals:
    void playerChanged();
    void sinkChanged();
    void nameChanged();
    void landed();

private:
    // What is noted of the frames, on the thread that hands them over: the last (which
    // it was, in microseconds of the video, and when, in nanoseconds of the steady
    // clock), and the first at or after `from` since it was last asked for (-1 for not
    // asked, or not come)
    struct Seen
    {
        std::atomic<qint64> pts {-1};
        std::atomic<qint64> at {0};
        std::atomic<qint64> from {-1};
        std::atomic<qint64> askedAt {0};
        std::atomic<qint64> landedPts {-1};
        std::atomic<qint64> landedAt {0};
    };

    void listen();
    void seek(double target, qint64 now);
    void look();
    void settled(qint64 now, double out);
    double frameLength() const;
    // Where the video is at `now`, in milliseconds, by its last frame; -1 if none came lately
    double place(qint64 now, bool playing) const;

    QPointer<QMediaPlayer> m_player;
    QPointer<QVideoSink> m_sink;
    QString m_name;
    std::shared_ptr<Seen> m_seen;
    QMetaObject::Connection m_listening;
    // Looks for the picture from the new place, and then for the video having settled,
    // while either is awaited; and ends a hold
    QTimer m_look;
    QTimer m_release;
    // What was asked: where the video belonged then (ms), and when (ns); when it was
    // first asked, where it has been sent again since; how many times that is; whether
    // its picture is awaited, or (the picture come) its settling; when the picture came
    // and how long it took
    double m_target = 0;
    qint64 m_askedAt = 0;
    qint64 m_firstAskedAt = 0;
    int m_again = 0;
    bool m_waiting = false;
    bool m_settling = false;
    qint64 m_cameAt = 0;
    double m_wait = 0;
    qint64 m_keptAt = 0;
    int m_landings = 0;
    QVariantMap m_last;
};
