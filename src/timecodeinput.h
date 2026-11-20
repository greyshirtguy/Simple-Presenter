#pragma once

#include "ltc.h"
#include "timecode.h"

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class QAudioSink;
class QAudioSource;
class QIODevice;
class QMediaDevices;

// The timecode coming in (ProPresenter's Timecode: see timecode.h for what it says of it
// and what is guessed): listens to one channel of a sound device, reads the timecode out
// of it (ltc.h), follows it (timecode::Chase), and says when it starts, jumps and stops,
// for the operator window to have the timelines follow (Main's "The timecode"). And its
// settings, which are ProPresenter's four (the device, the channel of it, the format, an
// offset), whether it is listening at all (ProPresenter's "Not Engaged" is that it is
// not), and the playlist whose presentations it triggers.
//
// Where the settings are kept. ProPresenter's are in a file of its own, which has not
// been seen (its format is there: TimecodeSettings), and name a sound device of the Mac
// or the PC it runs on, which is nothing here. So they are kept in this app's settings,
// as this computer's: the device by its name here, the channel, the format, the offset,
// whether it listens; and the playlist, which is the workspace's, by the workspace.
//
// What it costs. Not listening, which is how it starts, nothing at all: no device is
// opened, and the computer is not so much as asked what sound devices it has until the
// Timecode window wants to list them. Listening, the sound of the device is read as it
// comes, in pieces of a fiftieth or a twenty-fifth of a second (the sound system's
// own), on the thread everything else is done on: a few comparisons a sample (ltc.h),
// which is some tens of millionths of a second a piece. Nothing
// is said for a frame that follows the one before; only a start, a jump or a stop says
// anything, and twice a second while it runs, for a timeline that follows to be kept to
// it. The timecode as text, and how loud the signal is, are worked out only while
// something asks for them (the Timecode window, a text box of a stage layout), as the
// clocks are (clocks.h).
//
// When a frame was heard is taken from where its samples are in the sound and when the
// sound was handed over (timecode::SampleClock), not from when it came to be read here:
// the app being busy for a moment makes the reading late, and must not make the
// timecode late.
//
// A source of its own, to try it with ("Made here"): beside the sound devices, the
// device list has a timecode made in the app, that runs by the computer's clock when it
// is played from the Timecode window, can be stopped, and can be sent anywhere while it
// runs. It is not ProPresenter's: it is there so that a timeline, and a video on one,
// can be seen following timecode on a computer with no timecode coming in. Its frames go
// the way a sound device's do from where they have been read, so what follows them is
// what follows the real thing.
//
// There is one of these, which QML reaches by its name.
class Timecode : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Whether the settings are kept from one run to the next (the tests' are not), and
    // the workspace that is open, by its folder: the playlist chosen is kept by it
    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY rememberChanged)
    Q_PROPERTY(QString workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)
    // The sound devices that can be listened to: each { id, name, channels }, the one
    // made here first (id "made-here")
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY settingsChanged)
    // Which channel of it, from 0, and how many it has
    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY settingsChanged)
    Q_PROPERTY(int channels READ channels NOTIFY settingsChanged)
    // ProPresenter's number for the format (timecode::Format), and the four there are
    Q_PROPERTY(int format READ format WRITE setFormat NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList formats READ formats CONSTANT)
    // Seconds added to the timecode that comes in
    Q_PROPERTY(double offset READ offset WRITE setOffset NOTIFY settingsChanged)
    // Whether it listens at all (not ProPresenter's "Not Engaged")
    Q_PROPERTY(bool engaged READ engaged WRITE setEngaged NOTIFY settingsChanged)
    // The playlist whose presentations it triggers, by its id; "" for none
    Q_PROPERTY(QString playlist READ playlist WRITE setPlaylist NOTIFY settingsChanged)
    // Whether the device chosen is the one made here
    Q_PROPERTY(bool madeHere READ madeHere NOTIFY settingsChanged)

    // Whether timecode is running; and ProPresenter's word for how it is: "Not
    // Engaged", "Stopped" or "Playing"
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    // What is wrong with the device, in words, or ""
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)
    // Goes up whenever the timecode as text may have changed, while anything asks for
    // it: read it in the binding that asks (`text`), to be asked again
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    // What the signal looks like, for a line under the settings: how loud it is (from
    // 0 to 1; 0 for none), and the frames a second it is going at ("25 fps", or "" until
    // a second of it has been heard). Kept up only while `watched`.
    Q_PROPERTY(bool watched READ watched WRITE setWatched NOTIFY watchedChanged)
    Q_PROPERTY(double level READ level NOTIFY signalChanged)
    Q_PROPERTY(QString measured READ measured NOTIFY signalChanged)
    // Whether the signal is being played to the computer's own sound device, to be
    // heard (ProPresenter's speaker button, held): quietly
    Q_PROPERTY(bool audible READ audible WRITE setAudible NOTIFY audibleChanged)

public:
    explicit Timecode(QObject *parent = nullptr);
    ~Timecode() override;

    bool remember() const { return m_remember; }
    void setRemember(bool remember);
    QString workspace() const { return m_workspace; }
    void setWorkspace(const QString &workspace);
    QVariantList devices();
    QString device() const { return m_device; }
    void setDevice(const QString &device);
    int channel() const { return m_channel; }
    void setChannel(int channel);
    int channels();
    int format() const { return int(m_chase.format()); }
    void setFormat(int format);
    QVariantList formats() const { return timecode::formats(); }
    double offset() const { return m_offset; }
    void setOffset(double offset);
    bool engaged() const { return m_engaged; }
    void setEngaged(bool engaged);
    QString playlist() const { return m_playlist; }
    void setPlaylist(const QString &playlist);
    bool madeHere() const;
    bool running() const { return m_chase.running(); }
    QString status() const;
    QString problem() const { return m_problem; }
    int revision() const { return m_revision; }
    bool watched() const { return m_watched; }
    void setWatched(bool watched);
    double level() const { return m_level; }
    QString measured() const { return m_measured; }
    bool audible() const { return m_audible; }
    void setAudible(bool audible);

    // The timecode now, as it is said ("01:00:00;12"): as it comes in, the offset not
    // added. Asking is what has it kept up (see `revision`).
    Q_INVOKABLE QString text();
    // Which of `rows` the timecode is in now, the offset added, and how far into its
    // timeline (timecode::under); nothing while it is not listening
    Q_INVOKABLE QVariantMap under(const QVariantList &rows) const;
    // A timecode read as a clock, as it is written, and what was typed for one (-1 if
    // it is none), at the format set: for a timeline's start
    Q_INVOKABLE QString faceText(double seconds) const;
    Q_INVOKABLE double typed(const QString &text) const;

    // The timecode made here: played from where it is, stopped, and sent to a timecode
    // (seconds of it read as a clock), running or not. Nothing unless it is the device.
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void jump(double faceSeconds);

signals:
    void rememberChanged();
    void workspaceChanged();
    void devicesChanged();
    void settingsChanged();
    void runningChanged();
    void statusChanged();
    void revisionChanged();
    void watchedChanged();
    void signalChanged();
    void audibleChanged();
    // The timecode has "started", "jumped" or "stopped"; or is "kept" (twice a second
    // while it runs, for what follows it to be kept to it); or is no longer listened to
    // ("left": the settings changed, or Not Engaged)
    void moved(const QString &how);

private:
    void load();
    void save() const;
    // The sound devices found, and watched for coming and going: the first time they
    // are wanted, and not before
    void watchDevices();
    void listDevices(bool say = true);
    void open();
    void close();
    void read();
    void changed(timecode::Chase::Change change);
    void make();
    void tick();
    double now() const { return m_clock.nsecsElapsed() / 1e9; }
    double frames() const;

    bool m_remember = true;
    QString m_workspace;
    QVariantList m_devices;
    QString m_device;
    int m_channel = 0;
    double m_offset = 0;
    bool m_engaged = false;
    QString m_playlist;
    QString m_problem;

    QElapsedTimer m_clock;
    timecode::Chase m_chase;
    QMediaDevices *m_watching = nullptr;
    // Listening to a sound device: what reads it, what it is read through, how its
    // sound comes, and what reads the timecode out of it and says when it was heard
    QAudioSource *m_source = nullptr;
    QPointer<QIODevice> m_sound;
    QAudioFormat m_soundFormat;
    std::unique_ptr<ltc::Decoder> m_decoder;
    timecode::SampleClock m_samples;
    std::vector<float> m_one;
    std::vector<ltc::Frame> m_frames;
    double m_arrivedAt = 0;
    // The signal's frames a second, as its frames' lengths have it
    double m_rate = 0;
    // The one made here: whether it is playing; the number of the frame it will next
    // say, and when that frame ends
    bool m_playing = false;
    qint64 m_nextFrame = 0;
    double m_nextEnds = 0;
    // Makes the frames of the one made here, and looks for silence on a device that has
    // stopped handing sound over; says "kept" while it runs; and keeps the text up
    QTimer m_make;
    QTimer m_keep;
    QTimer m_tick;
    // (m_tick's own time is set where it is started: fast while the timecode runs)
    bool m_asked = false;
    int m_revision = 0;
    bool m_watched = false;
    double m_level = 0;
    QString m_measured;
    // The signal played to be heard
    bool m_audible = false;
    QAudioSink *m_monitor = nullptr;
    QPointer<QIODevice> m_monitorSound;
};
