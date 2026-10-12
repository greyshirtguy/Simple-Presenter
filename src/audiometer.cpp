#include "audiometer.h"

#include "audiolevel.h"

#include <QAudioBuffer>
#include <QAudioFormat>

#include <cmath>

namespace {

AudioMeter *s_meter = nullptr;
// (A tap may be made before the meter is: QML makes the meter when something first
// asks for it.)
QList<AudioTap *> &taps()
{
    static QList<AudioTap *> list;
    return list;
}

void raise(std::atomic<float> &kept, float value)
{
    float had = kept.load(std::memory_order_relaxed);
    while (value > had && !kept.compare_exchange_weak(had, value, std::memory_order_relaxed)) {
    }
}

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
    for (AudioTap *tap : std::as_const(taps())) {
        if (tap->playing())
            start();
    }
}

AudioMeter::~AudioMeter()
{
    if (s_meter == this)
        s_meter = nullptr;
}

AudioMeter *AudioMeter::instance()
{
    return s_meter;
}

void AudioMeter::add(AudioTap *tap)
{
    taps().append(tap);
}

void AudioMeter::remove(AudioTap *tap)
{
    taps().removeAll(tap);
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
    // What every tap has had, together: two things playing at once are louder than one.
    double left = 0;
    double right = 0;
    bool playing = false;
    for (AudioTap *tap : std::as_const(taps())) {
        double tapLeft = 0;
        double tapRight = 0;
        tap->take(&tapLeft, &tapRight);
        left += tapLeft;
        right += tapRight;
        playing = playing || tap->playing();
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

AudioTap::AudioTap(QObject *parent)
    : QObject(parent)
{
    AudioMeter::add(this);
    // Each stretch of sound is looked at here, on this object's own thread, whichever
    // thread it was decoded on (it is handed over, not copied, where that is another):
    // so that a tap that is done with is never in the middle of a stretch as it goes.
    // Two numbers are kept of it.
    connect(&m_output, &QAudioBufferOutput::audioBufferReceived, this, [this](const QAudioBuffer &buffer) {
        const QAudioFormat format = buffer.format();
        audiolevel::Format samples;
        switch (format.sampleFormat()) {
        case QAudioFormat::UInt8: samples = audiolevel::Format::UInt8; break;
        case QAudioFormat::Int16: samples = audiolevel::Format::Int16; break;
        case QAudioFormat::Int32: samples = audiolevel::Format::Int32; break;
        case QAudioFormat::Float: samples = audiolevel::Format::Float; break;
        default: return;
        }
        const audiolevel::Peaks found = audiolevel::peaks(buffer.constData<void>(), buffer.byteCount(), samples, format.channelCount());
        raise(m_peakLeft, found.left);
        raise(m_peakRight, found.right);
        m_heldLeft = found.left;
        m_heldRight = found.right;
        m_heldSince.start();
    });
}

AudioTap::~AudioTap()
{
    AudioMeter::remove(this);
}

void AudioTap::setGain(double gain)
{
    gain = qBound(0.0, gain, 1.0);
    if (gain == m_gain)
        return;
    m_gain = gain;
    emit gainChanged();
}

void AudioTap::setPlaying(bool playing)
{
    if (playing == m_playing)
        return;
    m_playing = playing;
    // (What it had from before it stopped is not carried into its next playing.)
    if (playing) {
        m_peakLeft.store(0, std::memory_order_relaxed);
        m_peakRight.store(0, std::memory_order_relaxed);
        m_heldSince.invalidate();
        AudioMeter::wake();
    }
    emit playingChanged();
}

void AudioTap::take(double *left, double *right)
{
    float foundLeft = m_peakLeft.exchange(0, std::memory_order_relaxed);
    float foundRight = m_peakRight.exchange(0, std::memory_order_relaxed);
    // Nothing handed over since it was last asked: the stretch before stands for a
    // third of a second, which is longer than any gap between two of them while it
    // plays. (Without this a steady note has the bars dipping between stretches.)
    if (foundLeft == 0 && foundRight == 0 && m_heldSince.isValid() && m_heldSince.elapsed() < 330) {
        foundLeft = m_heldLeft;
        foundRight = m_heldRight;
    }
    *left = foundLeft * m_gain;
    *right = foundRight * m_gain;
    // (Sound still coming from a player that has been told it is not playing, the
    // tail of what was decoded ahead, is not shown.)
    if (!m_playing)
        *left = *right = 0;
}
