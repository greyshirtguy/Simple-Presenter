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
    std::array<float, audiomix::kMostBuses> peaks {};
    bool playing = false;
    if (AudioMixer *mixer = AudioMixer::existing()) {
        mixer->takePeaks(peaks.data(), int(peaks.size()));
        playing = mixer->sounding();
    }
    const double seconds = m_since.restart() / 1000.0;
    // To the nearest decibel, which is a sixtieth of a bar's height: a bar that has not
    // moved by that much has not moved to the eye, and is not drawn again.
    const auto whole = [](double level) { return std::round(level * 60) / 60; };
    bool moved = false;
    bool up = false;
    for (size_t bus = 0; bus < m_levels.size(); ++bus) {
        const double now = whole(audiolevel::settled(m_levels[bus], audiolevel::metered(peaks[bus]), seconds));
        if (now != m_levels[bus]) {
            m_levels[bus] = now;
            moved = true;
        }
        if (now > 0)
            up = true;
    }
    if (moved)
        emit levelsChanged();
    // Nothing playing and the bars at the bottom: there is nothing more to follow.
    if (!playing && !up) {
        m_clock.stop();
        emit soundingChanged();
    }
}

QVariantList AudioMeter::levels() const
{
    QVariantList list;
    list.reserve(int(m_levels.size()));
    for (const double level : m_levels)
        list.append(level);
    return list;
}
