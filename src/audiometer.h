#pragma once

#include <QAudioBufferOutput>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>

class AudioTap;

// The meter of what is being played: how loud the sound the app is making is, on the
// left and on the right, for the two bars in the preview of the output.
//
// Whatever plays sound (a piece of audio in the audio layer, a video with its sound)
// has an AudioTap, which is handed the sound as it is decoded and keeps the loudest it
// has got on each side. Ten times a second, while anything is playing, the meter
// asks every tap what it has had since it was last asked, adds them up (two things
// playing at once are louder than one), and shows that: up to a new peak at once and
// down from an old one over a moment (see audiolevel.h).
//
// What it costs. Nothing while nothing plays: the clock that asks runs only while a tap
// says it is playing, and for the moment after in which the bars fall. While something
// plays, a look at each stretch of sound as it is decoded (a thousand or two numbers
// gone through once, some forty times a second: millionths of a second each time),
// and ten times a second two numbers for two small bars to follow, which is the
// operator window drawn again that often while sound plays and the bars move: what
// the preview of a playing video costs it already, and no more.
//
// What it shows is not quite what is heard. It is the sound as the file has it, made
// quieter by as much as its player is told to play it quieter (its own volume, and the
// fade it is at). The system's own volume, and whatever comes after the app, is not in
// it. And a stretch of sound is seen when it is decoded, a moment before it is heard:
// some tenths of a second at most, which a meter is none the worse for.
//
// There is one of these, which QML reaches by its name.
class AudioMeter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // How far up each bar is, from 0 to 1 (by decibels: see audiolevel::metered)
    Q_PROPERTY(double left READ left NOTIFY levelsChanged)
    Q_PROPERTY(double right READ right NOTIFY levelsChanged)
    // Whether anything is playing, or the bars are still falling from it: whether there
    // is a meter to show
    Q_PROPERTY(bool sounding READ sounding NOTIFY soundingChanged)

public:
    explicit AudioMeter(QObject *parent = nullptr);
    ~AudioMeter() override;

    double left() const { return m_left; }
    double right() const { return m_right; }
    bool sounding() const { return m_clock.isActive(); }

    // The one there is, or null before QML has made it. A tap made in C++ finds it by
    // this once it is there.
    static AudioMeter *instance();
    // A tap comes, goes, or starts or stops playing.
    static void add(AudioTap *tap);
    static void remove(AudioTap *tap);
    static void wake();

signals:
    void levelsChanged();
    void soundingChanged();

private:
    void tick();
    void start();

    double m_left = 0;
    double m_right = 0;
    QTimer m_clock;
    QElapsedTimer m_since;
};

// What a player of sound is given to hand its sound to, so that the meter has it: see
// AudioMeter. In QML, a MediaPlayer's `audioBufferOutput` is this one's `output`.
class AudioTap : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // What the player is to be given
    Q_PROPERTY(QAudioBufferOutput *output READ output CONSTANT)
    // How much quieter than its file the player is playing it, from 0 to 1: its volume
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY gainChanged)
    // Whether it is playing: the meter runs while any tap is
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)

public:
    explicit AudioTap(QObject *parent = nullptr);
    ~AudioTap() override;

    QAudioBufferOutput *output() { return &m_output; }
    double gain() const { return m_gain; }
    void setGain(double gain);
    bool playing() const { return m_playing; }
    void setPlaying(bool playing);

    // The loudest it has been on each side since this was last asked, with its gain.
    void take(double *left, double *right);

signals:
    void gainChanged();
    void playingChanged();

private:
    QAudioBufferOutput m_output;
    // (Kept so that it would not matter which thread wrote them and which read.)
    std::atomic<float> m_peakLeft {0};
    std::atomic<float> m_peakRight {0};
    // What the last stretch of sound it was handed came to, and when that was: sound
    // comes in stretches of a tenth of a second and more, further apart than the meter
    // asks, and between two of them the meter is told the last again and not silence.
    float m_heldLeft = 0;
    float m_heldRight = 0;
    QElapsedTimer m_heldSince;
    double m_gain = 1;
    bool m_playing = false;
};
