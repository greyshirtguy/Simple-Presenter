#pragma once

#include "audiomix.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <array>

// The meters of what is being played: how loud the sound the app is making is, on each
// of the app's channels, for the bars beside the preview of the output (PreviewPanel).
//
// All the app's sound but a video input's goes through its own mixer (audiomixer.h),
// which keeps the loudest each of its channels has been, as it mixes them: after their
// Mute and Solo, with their test tones, with every player in them at the loudness it is
// played at (its volume, its fade, its ramp), and whether or not the channel goes
// anywhere on the device. Ten times a second, while anything is playing or a tone is
// on, the meter takes that and shows it: up to a new peak at once and down from an old
// one over a moment (see audiolevel.h). There is a level for every channel the app can
// have (audiomix::kMostBuses), one the workspace has not got staying at 0; how many of
// them have a bar is a setting of the preview's.
//
// What it costs. Nothing while nothing plays: the clock that asks runs only while the
// mixer has something sounding, and for the moment after in which the bars fall. While
// something plays, a number a channel kept by the mixer as it goes, and ten times a
// second as many numbers for as many small bars to follow, which is the operator
// window drawn again that often while sound plays and the bars move: what the preview
// of a playing video costs it already, and no more.
//
// What it shows is what the app sends on, a moment before it is heard (the device holds
// a little: see audiomix.h): the app's channels, before the device's matrix. The
// system's own volume, and whatever comes after the app, is not in it. (Until 0.125 it
// was what the device was sent, its odd-numbered channels on one bar and its
// even-numbered on the other: the maintainer asked, 2026-11-07, for a bar a channel, as
// ProPresenter has beside its preview.)
//
// There is one of these, which QML reaches by its name.
class AudioMeter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // How far up each channel's bar is, from 0 to 1 (by decibels: see
    // audiolevel::metered): `most` of them, the first for the app's first channel
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(int most READ most CONSTANT)
    // The first two, for short: the left and the right of a workspace of two channels
    Q_PROPERTY(double left READ left NOTIFY levelsChanged)
    Q_PROPERTY(double right READ right NOTIFY levelsChanged)
    // Whether anything is playing, or the bars are still falling from it: whether there
    // is a meter to show
    Q_PROPERTY(bool sounding READ sounding NOTIFY soundingChanged)

public:
    explicit AudioMeter(QObject *parent = nullptr);
    ~AudioMeter() override;

    QVariantList levels() const;
    int most() const { return audiomix::kMostBuses; }
    double left() const { return m_levels[0]; }
    double right() const { return m_levels[1]; }
    bool sounding() const { return m_clock.isActive(); }

    // Something has started to sound: the meter follows it (if QML has made the meter)
    static void wake();

signals:
    void levelsChanged();
    void soundingChanged();

private:
    void tick();
    void start();

    std::array<double, audiomix::kMostBuses> m_levels {};
    QTimer m_clock;
    QElapsedTimer m_since;
};
