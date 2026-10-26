#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// The meter of what is being played: how loud the sound the app is making is, on the
// left and on the right, for the two bars in the preview of the output.
//
// All the app's sound but a video input's goes through its own mixer (audiomixer.h),
// which keeps the loudest its first two channels have been, as it mixes them: after
// their Mute and Solo, with their test tones, and with every player in them at the
// loudness it is played at (its volume, its fade, its ramp). Ten times a second, while
// anything is playing or a tone is on, the meter takes that and shows it: up to a new
// peak at once and down from an old one over a moment (see audiolevel.h). A workspace
// with one channel has it on both bars.
//
// What it costs. Nothing while nothing plays: the clock that asks runs only while the
// mixer has something sounding, and for the moment after in which the bars fall. While
// something plays, two numbers kept by the mixer as it goes, and ten times a second two
// numbers for two small bars to follow, which is the operator window drawn again that
// often while sound plays and the bars move: what the preview of a playing video costs
// it already, and no more.
//
// What it shows is what the app sends, a moment before it is heard (the device holds a
// little: see audiomix.h): the app's first two channels, wherever they go on the device.
// The system's own volume, and whatever comes after the app, is not in it.
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

    // Something has started to sound: the meter follows it (if QML has made the meter)
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
