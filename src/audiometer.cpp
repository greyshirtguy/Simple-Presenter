#include "audiometer.h"

#include "audiolevel.h"
#include "audiomixer.h"

#include <cmath>

namespace {

AudioMeter *s_meter = nullptr;

} // namespace

AudioMeter::AudioMeter(QObject *parent)
    : QObject(parent)
{
    s_meter = this;
    // Ten times a second, which is how often the preview beside it takes a picture of
    // a video that is playing: often enough for a bar to be seen to follow the sound,
    // and the window drawn again no more often for sound than it is for a video.
    m_clock.setInterval(100);
    connect(&m_clock, &QTimer::timeout, this, &AudioMeter::tick);
    // (Something may have started before QML made the meter)
    if (AudioMixer *mixer = AudioMixer::existing(); mixer && mixer->sounding())
        start();
}

AudioMeter::~AudioMeter()
{
    if (s_meter == this)
        s_meter = nullptr;
}

void AudioMeter::wake()
{
    if (s_meter)
        s_meter->start();
}

void AudioMeter::start()
{
    if (m_clock.isActive())
        return;
    m_since.start();
    m_clock.start();
    emit soundingChanged();
}

void AudioMeter::tick()
{
    float left = 0;
    float right = 0;
    bool playing = false;
    if (AudioMixer *mixer = AudioMixer::existing()) {
        mixer->takePeaks(&left, &right);
        playing = mixer->sounding();
    }
    const double seconds = m_since.restart() / 1000.0;
    // To the nearest decibel, which is a sixtieth of the meter's height: a bar that
    // has not moved by that much has not moved to the eye, and is not drawn again.
    const auto whole = [](double level) { return std::round(level * 60) / 60; };
    const double nowLeft = whole(audiolevel::settled(m_left, audiolevel::metered(left), seconds));
    const double nowRight = whole(audiolevel::settled(m_right, audiolevel::metered(right), seconds));
    if (nowLeft != m_left || nowRight != m_right) {
        m_left = nowLeft;
        m_right = nowRight;
        emit levelsChanged();
    }
    // Nothing playing and the bars at the bottom: there is nothing more to follow.
    if (!playing && m_left <= 0 && m_right <= 0) {
        m_clock.stop();
        emit soundingChanged();
    }
}
