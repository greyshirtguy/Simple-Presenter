#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// What plays a presentation's timeline (timeline.h): keeps its time, and says when each
// of its cues is due, for the operator window to do (Main's timeline part): a slide to
// go live, an action to run. What a cue does is not decided here, any more than what
// a slide's Go To Next Timer leads to is decided by the timer that counts it: the show
// is changed through Show, as by any other hand.
//
// There are two, as ProPresenter has two timelines that can play at once: the one of
// the presentation on the slide and media layers, and the one of the presentation the
// announcements are showing (its API and its clear groups name the two). Each has the
// timeline of one presentation at a time, by the row that presentation was opened
// from, and whichever is played or recorded is given the presentation first.
//
// The time. While it plays, the time is where it was started from and what a steady
// clock has counted since; a cue is due when the time passes its own, and a timer is
// set for the next of them and for the timeline's end, and for nothing in between. So
// nothing at all is done between cues; and a cue is done within a millisecond or two
// of its time, however long the gap before it. (The timeline's time is its own: a
// piece of audio it starts is started where the timeline is in it, and plays on by
// the audio's own clock, which a few minutes of a song do not set apart from this one
// by any amount to hear. Following timecode, which ProPresenter can, is to come: the
// maintainer, 2026-11-14. Then this is set to where the timecode says, by seek().)
//
// Where it is, for the playhead the operator window draws, is told ten times a second
// while it plays and something shows it (`watched`), and at once when it moves by
// hand; as the meters of the audio are told (audiometer.h), and for the same reason: a
// line that moves a few pixels a second needs no more, and the operator window is not
// drawn sixty times a second for it.
//
// Recording (ProPresenter's Record): what is triggered by hand while it is on is put on
// the timeline where the playhead is (`record`), playing or not. What is recorded is
// kept here, and shown with the rest, until recording stops, when the operator window
// writes it to the presentation's file all at once: the file is not read and written
// again at every slide of a song being recorded over its track. A cue recorded while
// the timeline plays is not done by it until it has been written.
class TimelinePlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // The presentation whose timeline this has: its file, and the row it was opened
    // from (`key`, `playlistId`), whose slides the timeline's take live; all "" for none
    Q_PROPERTY(QString path READ path NOTIFY loadedChanged)
    Q_PROPERTY(QString key READ key NOTIFY loadedChanged)
    Q_PROPERTY(QString playlistId READ playlistId NOTIFY loadedChanged)
    // The timeline, as timeline::describe has it, with what has been recorded and not
    // yet written among its cues (each marked `recorded`, with no place)
    Q_PROPERTY(QVariantMap timeline READ timeline NOTIFY timelineChanged)
    // How many cues have been recorded and not yet written
    Q_PROPERTY(int recordedCount READ recordedCount NOTIFY timelineChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool recording READ recording WRITE setRecording NOTIFY recordingChanged)
    // Where the playhead is, in seconds from the start (see above for how often it is told)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    // Whether anything shows where the playhead is, for it to be told as it plays
    Q_PROPERTY(bool watched READ watched WRITE setWatched NOTIFY watchedChanged)

public:
    explicit TimelinePlayer(QObject *parent = nullptr);

    QString path() const { return m_path; }
    QString key() const { return m_key; }
    QString playlistId() const { return m_playlistId; }
    QVariantMap timeline() const;
    int recordedCount() const { return int(m_recorded.size()); }
    bool playing() const { return m_playing; }
    bool recording() const { return m_recording; }
    void setRecording(bool recording);
    double position() const;
    bool watched() const { return m_watched; }
    void setWatched(bool watched);

    // Gives this the timeline of the presentation in file `path`, opened from the row
    // `key` of `playlistId`. Another presentation's that was playing stops, and the
    // playhead goes to the start; the same presentation's is only given its cues as
    // they are now (it has been read again), and goes on where it is, playing or not.
    // Whatever was recorded and not taken (takeRecorded) is dropped with another's.
    Q_INVOKABLE void load(const QString &path, const QString &key, const QString &playlistId, const QVariantMap &timeline);
    // Whether this has the timeline of the presentation of that row
    Q_INVOKABLE bool holds(const QString &key, const QString &playlistId) const;

    // Play, from where the playhead is; from its end, from the start. What is in force
    // where it starts is done first (see timeline::entering).
    Q_INVOKABLE void play();
    // Pause: the playhead stays where it is, and `paused` is said, for the audio the
    // timeline started to stop with it (so that playing on starts it again where the
    // timeline is, and the two go on together)
    Q_INVOKABLE void pause();
    // Stopped from outside, by a clear group or by another presentation taking the
    // layer: as a pause, but nothing it started is stopped
    Q_INVOKABLE void stop();
    // The playhead put somewhere; while it plays, it plays on from there as if started
    // there (paused first)
    Q_INVOKABLE void seek(double seconds);
    // ProPresenter's Rewind: to the start
    Q_INVOKABLE void rewind() { seek(0); }

    // A slide of the presentation of row `key` of `playlistId` has been taken live on
    // the layer this plays on, by something other than this: if that is another
    // presentation, this stops. (A GUESS at ProPresenter, which has not been seen
    // doing it: a song's timeline left playing would take the next song's slides away.)
    Q_INVOKABLE void wentLive(const QString &key, const QString &playlistId);

    // Puts a cue on the timeline where the playhead is, while recording: `cue` as
    // timeline::describe has a cue, but for its place and time, and for an action, with
    // `wanted` beside it: what actions::build is to make it from. Nothing while not
    // recording.
    Q_INVOKABLE void record(const QVariantMap &cue);
    // What has been recorded, in order, each { time, name, and slideId or wanted }, for
    // the operator window to write; it is no longer kept here.
    Q_INVOKABLE QVariantList takeRecorded();

    // Which of the slides shown (their ids, in the arrangement's order) a slide cue for
    // `slideId` triggers, with the show at place `after` of them (timeline::slidePlace)
    Q_INVOKABLE int slidePlace(const QStringList &ids, const QString &slideId, int after) const;

signals:
    void loadedChanged();
    void timelineChanged();
    void playingChanged();
    void recordingChanged();
    void positionChanged();
    void watchedChanged();
    // A cue is due: `cue` as timeline::describe has it, and `offset` how far into it the
    // timeline is (for audio and video started part way; 0 for the rest)
    void cueDue(const QVariantMap &cue, double offset);
    // Paused, by hand or by the API: what it started is to stop (see pause)
    void paused();

private:
    void schedule();
    void due();
    // Says the cues at `places` are due, in turn; false if one of them stopped this
    bool tell(const QList<int> &places, double now);
    void enter(double at);
    double duration() const;
    void halt();

    QString m_path;
    QString m_key;
    QString m_playlistId;
    QVariantMap m_timeline;
    QVariantList m_cues;
    QVariantList m_recorded;
    bool m_playing = false;
    bool m_recording = false;
    bool m_watched = false;
    // Where it was started from, and the clock since; or, stopped, where it is
    double m_base = 0;
    QElapsedTimer m_clock;
    // The time up to which cues have been done
    double m_doneTo = 0;
    // One more each time it is started, stopped or moved: a cue that was being said
    // when that happened is the last said
    int m_run = 0;
    QTimer m_due;
    QTimer m_tick;
};
