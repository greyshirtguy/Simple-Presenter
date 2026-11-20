#include "timecodeinput.h"

#include "sessionlog.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace {

const QString kMadeHere = QStringLiteral("made-here");
// How loud the signal is played when it is asked to be heard: a third. (Timecode is a
// harsh sound, and the computer's own sound device may be what the room hears.)
constexpr float kHeardAt = 0.3f;

QString said(double rate)
{
    if (rate <= 0)
        return {};
    if (std::abs(rate - 24) < 0.1)
        return QStringLiteral("24 fps");
    if (std::abs(rate - 25) < 0.1)
        return QStringLiteral("25 fps");
    if (std::abs(rate - 30000.0 / 1001) < 0.012)
        return QStringLiteral("29.97 fps");
    if (std::abs(rate - 30) < 0.012)
        return QStringLiteral("30 fps");
    return QStringLiteral("%1 fps").arg(rate, 0, 'f', 2);
}

}

Timecode::Timecode(QObject *parent) : QObject(parent)
{
    m_clock.start();
    m_make.setTimerType(Qt::PreciseTimer);
    connect(&m_make, &QTimer::timeout, this, &Timecode::make);
    m_keep.setInterval(500);
    connect(&m_keep, &QTimer::timeout, this, [this] {
        if (m_chase.running())
            emit moved(QStringLiteral("kept"));
    });
    connect(&m_tick, &QTimer::timeout, this, &Timecode::tick);
    // (The settings are read when the workspace is said, which the operator window does
    // as it starts, after it has said whether they are kept: setWorkspace.)
}

void Timecode::watchDevices()
{
    if (m_watching)
        return;
    m_watching = new QMediaDevices(this);
    connect(m_watching, &QMediaDevices::audioInputsChanged, this, [this] {
        listDevices();
        // (A device that has come, or gone, while it is the one listened to)
        if (m_engaged && !madeHere() && (m_source == nullptr || !m_problem.isEmpty()))
            open();
    });
    // (Said to nobody this first time: whoever wanted them is being answered now, and
    // is not to be told that what it asked after has changed while it asks.)
    listDevices(false);
}

QVariantList Timecode::devices()
{
    watchDevices();
    return m_devices;
}

Timecode::~Timecode()
{
    delete m_monitor;
    delete m_source;
}

void Timecode::load()
{
    if (!m_remember)
        return;
    const QSettings settings;
    m_device = settings.value("timecode/device").toString();
    m_channel = std::max(0, settings.value("timecode/channel", 0).toInt());
    m_chase.setFormat(timecode::formatOf(settings.value("timecode/format", int(timecode::Fps30)).toInt()));
    m_offset = settings.value("timecode/offset", 0.0).toDouble();
    m_engaged = settings.value("timecode/engaged", false).toBool();
    m_playlist = settings.value("timecode/playlists").toMap().value(m_workspace).toString();
}

void Timecode::save() const
{
    if (!m_remember)
        return;
    QSettings settings;
    settings.setValue("timecode/device", m_device);
    settings.setValue("timecode/channel", m_channel);
    settings.setValue("timecode/format", int(m_chase.format()));
    settings.setValue("timecode/offset", m_offset);
    settings.setValue("timecode/engaged", m_engaged);
    if (!m_workspace.isEmpty()) {
        QVariantMap playlists = settings.value("timecode/playlists").toMap();
        if (m_playlist.isEmpty())
            playlists.remove(m_workspace);
        else
            playlists.insert(m_workspace, m_playlist);
        settings.setValue("timecode/playlists", playlists);
    }
}

void Timecode::setRemember(bool remember)
{
    if (m_remember == remember)
        return;
    m_remember = remember;
    emit rememberChanged();
    // (Told not to, as a test is: as new, whatever had been read)
    close();
    m_device.clear();
    m_channel = 0;
    m_offset = 0;
    m_engaged = false;
    m_playlist.clear();
    m_chase.setFormat(timecode::Fps30);
    load();
    emit settingsChanged();
    emit statusChanged();
    if (m_engaged)
        open();
}

void Timecode::setWorkspace(const QString &workspace)
{
    if (m_workspace == workspace)
        return;
    const bool first = m_workspace.isEmpty();
    m_workspace = workspace;
    if (first) {
        load();
        if (m_engaged)
            open();
        emit statusChanged();
    } else {
        m_playlist = m_remember ? QSettings().value("timecode/playlists").toMap().value(m_workspace).toString() : QString();
    }
    emit workspaceChanged();
    emit settingsChanged();
}

void Timecode::listDevices(bool say)
{
    QVariantList devices {QVariantMap {{"id", kMadeHere}, {"name", QStringLiteral("Made here (to try it)")}, {"channels", 1}}};
    for (const QAudioDevice &device : QMediaDevices::audioInputs())
        devices.append(QVariantMap {{"id", QString::fromUtf8(device.id())}, {"name", device.description()},
                                    {"channels", std::max(1, device.maximumChannelCount())}});
    if (devices == m_devices)
        return;
    m_devices = devices;
    if (say) {
        emit devicesChanged();
        emit settingsChanged();
    }
}

bool Timecode::madeHere() const
{
    return m_device == kMadeHere;
}

int Timecode::channels()
{
    watchDevices();
    for (const QVariant &entry : std::as_const(m_devices)) {
        const QVariantMap device = entry.toMap();
        if (device.value("id").toString() == m_device)
            return device.value("channels").toInt();
    }
    return 1;
}

void Timecode::setDevice(const QString &device)
{
    if (m_device == device)
        return;
    close();
    m_device = device;
    m_channel = std::min(m_channel, channels() - 1);
    save();
    emit settingsChanged();
    if (m_engaged)
        open();
    emit statusChanged();
}

void Timecode::setChannel(int channel)
{
    channel = std::clamp(channel, 0, std::max(0, channels() - 1));
    if (m_channel == channel)
        return;
    close();
    m_channel = channel;
    save();
    emit settingsChanged();
    if (m_engaged)
        open();
    emit statusChanged();
}

void Timecode::setFormat(int format)
{
    const timecode::Format asked = timecode::formatOf(format);
    if (asked == m_chase.format())
        return;
    close();
    m_chase.setFormat(asked);
    m_nextFrame = 0;
    save();
    emit settingsChanged();
    if (m_engaged)
        open();
    emit statusChanged();
}

void Timecode::setOffset(double offset)
{
    offset = std::clamp(offset, -86400.0, 86400.0);
    if (m_offset == offset)
        return;
    m_offset = offset;
    save();
    emit settingsChanged();
    // (What follows it is somewhere else by that much.)
    if (m_chase.running())
        emit moved(QStringLiteral("jumped"));
}

void Timecode::setEngaged(bool engaged)
{
    if (m_engaged == engaged)
        return;
    m_engaged = engaged;
    save();
    SessionLog::write("timecode", engaged ? QStringLiteral("listening for timecode") : QStringLiteral("no longer listening for timecode (Not Engaged)"));
    if (engaged)
        open();
    else
        close();
    emit settingsChanged();
    emit statusChanged();
}

void Timecode::setPlaylist(const QString &playlist)
{
    if (m_playlist == playlist)
        return;
    m_playlist = playlist;
    save();
    emit settingsChanged();
}

QString Timecode::status() const
{
    return !m_engaged ? QStringLiteral("Not Engaged") : m_chase.running() ? QStringLiteral("Playing") : QStringLiteral("Stopped");
}

void Timecode::close()
{
    m_make.stop();
    m_playing = false;
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_sound = nullptr;
    m_decoder.reset();
    m_rate = 0;
    m_problem.clear();
    const bool was = m_chase.running();
    m_chase.reset();
    if (was) {
        m_keep.stop();
        emit runningChanged();
        emit statusChanged();
    }
    // (Whatever followed it is let go, running or not.)
    emit moved(QStringLiteral("left"));
}

void Timecode::open()
{
    if (m_source || m_make.isActive())
        close();
    if (!m_engaged)
        return;
    if (madeHere()) {
        m_problem.clear();
        m_make.start(100);
        SessionLog::write("timecode", QStringLiteral("the timecode is the one made here"));
        return;
    }
    watchDevices();
    QAudioDevice found;
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        if (QString::fromUtf8(device.id()) == m_device)
            found = device;
    }
    if (found.isNull()) {
        m_problem = m_device.isEmpty() ? QStringLiteral("No input device is chosen") : QStringLiteral("The input device chosen is not connected");
        SessionLog::write("timecode", m_problem);
        emit statusChanged();
        return;
    }
    // As the device likes its sound, but with enough channels to have the one chosen
    QAudioFormat format = found.preferredFormat();
    format.setChannelCount(std::clamp(std::max(format.channelCount(), m_channel + 1), 1, std::max(1, found.maximumChannelCount())));
    format.setSampleFormat(QAudioFormat::Float);
    if (!found.isFormatSupported(format))
        format = found.preferredFormat();
    m_soundFormat = format;
    m_decoder = std::make_unique<ltc::Decoder>(format.sampleRate());
    m_samples = timecode::SampleClock(format.sampleRate());
    m_source = new QAudioSource(found, format, this);
    // (With the room Qt gives it by itself, a quarter of a second, though it is read as
    // soon as any of it comes. Asked for less room than a piece of sound takes, to have
    // the pieces small, PipeWire's stand-in for PulseAudio handed over only half the
    // sound, in pieces as large as ever (a twenty-fifth of a second): found 2026-11-19,
    // when timecode was heard and not one frame of it could be read.)
    m_sound = m_source->start();
    if (!m_sound || m_source->error() != QAudio::NoError) {
        m_problem = QStringLiteral("The input device chosen could not be listened to");
        SessionLog::write("timecode", m_problem + QStringLiteral(": ") + found.description());
        m_source->deleteLater();
        m_source = nullptr;
        m_sound = nullptr;
        m_decoder.reset();
        emit statusChanged();
        return;
    }
    connect(m_sound, &QIODevice::readyRead, this, &Timecode::read);
    m_arrivedAt = now();
    m_problem.clear();
    // (Looks for a device that has stopped handing sound over at all.)
    m_make.start(100);
    SessionLog::write("timecode", QStringLiteral("listening to channel %1 of %2: %3 Hz, %4 channels")
                                      .arg(std::min(m_channel, format.channelCount() - 1) + 1).arg(found.description())
                                      .arg(format.sampleRate()).arg(format.channelCount()));
    emit statusChanged();
}

void Timecode::read()
{
    if (!m_sound || !m_decoder)
        return;
    const QByteArray data = m_sound->readAll();
    const int channels = std::max(1, m_soundFormat.channelCount());
    const int width = std::max(1, m_soundFormat.bytesPerSample());
    const int count = int(data.size() / (channels * width));
    if (count <= 0)
        return;
    const int channel = std::min(m_channel, channels - 1);
    m_one.resize(size_t(count));
    const char *raw = data.constData();
    switch (m_soundFormat.sampleFormat()) {
    case QAudioFormat::Float: {
        const float *in = reinterpret_cast<const float *>(raw);
        for (int i = 0; i < count; ++i)
            m_one[size_t(i)] = in[i * channels + channel];
        break;
    }
    case QAudioFormat::Int16: {
        const qint16 *in = reinterpret_cast<const qint16 *>(raw);
        for (int i = 0; i < count; ++i)
            m_one[size_t(i)] = in[i * channels + channel] / 32768.0f;
        break;
    }
    case QAudioFormat::Int32: {
        const qint32 *in = reinterpret_cast<const qint32 *>(raw);
        for (int i = 0; i < count; ++i)
            m_one[size_t(i)] = float(in[i * channels + channel] / 2147483648.0);
        break;
    }
    default: {
        const quint8 *in = reinterpret_cast<const quint8 *>(raw);
        for (int i = 0; i < count; ++i)
            m_one[size_t(i)] = (in[i * channels + channel] - 128) / 128.0f;
        break;
    }
    }
    m_frames.clear();
    m_decoder->feed(m_one.data(), count, &m_frames);
    m_arrivedAt = now();
    m_samples.arrived(m_decoder->fed(), m_arrivedAt);
    for (const ltc::Frame &frame : m_frames) {
        if (frame.samples > 0) {
            const double rate = m_soundFormat.sampleRate() / frame.samples;
            m_rate = m_rate <= 0 ? rate : m_rate + (rate - m_rate) * 0.05;
        }
        changed(m_chase.frame({frame.hours, frame.minutes, frame.seconds, frame.frames}, frame.dropFrame, m_samples.at(frame.end)));
    }
    changed(m_chase.heard(m_samples.at(double(m_decoder->fed()))));
    if (m_audible && m_monitorSound) {
        // To both sides of the computer's own sound device, quietly
        std::vector<float> heard(size_t(count) * 2);
        for (int i = 0; i < count; ++i)
            heard[size_t(i) * 2] = heard[size_t(i) * 2 + 1] = m_one[size_t(i)] * kHeardAt;
        m_monitorSound->write(reinterpret_cast<const char *>(heard.data()), qint64(heard.size() * sizeof(float)));
    }
}

void Timecode::make()
{
    const double at = now();
    if (!madeHere()) {
        // A device that has handed nothing over for a while: as silence
        if (m_chase.running() && at - m_arrivedAt > 0.4)
            changed(m_chase.heard(at));
        return;
    }
    const double period = 1 / timecode::rate(m_chase.format());
    if (m_playing) {
        // Every frame that has ended by now, each said with when it ended (several at
        // once, if the app was busy: as a sound device's would come)
        int said = 0;
        while (m_nextEnds <= at && said++ < 120) {
            changed(m_chase.frame(timecode::labelOf(m_nextFrame, m_chase.format(), false), false, m_nextEnds));
            ++m_nextFrame;
            m_nextEnds += period;
        }
        // (Further behind than that, the computer was asleep: on from now.)
        if (m_nextEnds <= at)
            m_nextEnds = at + period;
    }
    changed(m_chase.heard(at));
    // (Not played, and known to have stopped: there is nothing to make, and little to
    // look for, until it is played again.)
    if (!m_playing && !m_chase.running() && m_make.interval() != 100)
        m_make.start(100);
}

void Timecode::changed(timecode::Chase::Change change)
{
    switch (change) {
    case timecode::Chase::Nothing:
        return;
    case timecode::Chase::Started:
        m_keep.start();
        SessionLog::write("timecode", QStringLiteral("timecode started, at %1").arg(timecode::text(m_chase.label(now()))));
        emit runningChanged();
        emit statusChanged();
        emit moved(QStringLiteral("started"));
        break;
    case timecode::Chase::Jumped:
        SessionLog::write("timecode", QStringLiteral("timecode jumped, to %1").arg(timecode::text(m_chase.label(now()))));
        emit moved(QStringLiteral("jumped"));
        break;
    case timecode::Chase::Stopped:
        m_keep.stop();
        SessionLog::write("timecode", QStringLiteral("timecode stopped, at %1").arg(timecode::text(m_chase.label(now()))));
        emit runningChanged();
        emit statusChanged();
        emit moved(QStringLiteral("stopped"));
        break;
    }
    // (Whoever shows the timecode is told at once, and as often as it now changes.)
    if (m_tick.isActive()) {
        m_tick.start(m_chase.running() ? 33 : 250);
        ++m_revision;
        emit revisionChanged();
    }
}

double Timecode::frames() const
{
    return m_chase.frames(now()) + m_offset * timecode::rate(m_chase.format());
}

QString Timecode::text()
{
    // That it was asked is what keeps `revision` going up: thirty times a second while
    // the timecode runs, four while it does not (when all that can change it is its
    // starting, which says so itself, or the one made here being sent somewhere)
    m_asked = true;
    if (!m_tick.isActive())
        m_tick.start(m_chase.running() ? 33 : 250);
    // (The one made here, stopped, is where it has been sent, which may be somewhere
    // it has never run.)
    if (madeHere() && !m_chase.running())
        return timecode::text(timecode::labelOf(m_nextFrame, m_chase.format(), false));
    return timecode::text(m_chase.label(now()));
}

void Timecode::tick()
{
    if (!m_asked && !m_watched) {
        m_tick.stop();
        return;
    }
    if (m_asked) {
        // Whoever asked asks again, which is what keeps this going; a tick that
        // nobody has asked since is the last
        m_asked = false;
        ++m_revision;
        emit revisionChanged();
    }
    if (m_watched) {
        const double level = m_decoder ? std::clamp(double(m_decoder->swing()) / 2, 0.0, 1.0) : madeHere() && m_playing ? 1.0 : 0.0;
        const QString measured = madeHere() ? QString() : said(m_chase.running() ? m_rate : 0);
        if (std::abs(level - m_level) > 0.02 || measured != m_measured) {
            m_level = level;
            m_measured = measured;
            emit signalChanged();
        }
    }
}

void Timecode::setWatched(bool watched)
{
    if (m_watched == watched)
        return;
    m_watched = watched;
    if (watched && !m_tick.isActive())
        m_tick.start(m_chase.running() ? 33 : 250);
    emit watchedChanged();
}

void Timecode::setAudible(bool audible)
{
    if (m_audible == audible)
        return;
    m_audible = audible;
    if (m_monitor) {
        m_monitor->stop();
        m_monitor->deleteLater();
        m_monitor = nullptr;
        m_monitorSound = nullptr;
    }
    if (audible && m_source) {
        QAudioFormat format;
        format.setSampleRate(m_soundFormat.sampleRate());
        format.setChannelCount(2);
        format.setSampleFormat(QAudioFormat::Float);
        const QAudioDevice out = QMediaDevices::defaultAudioOutput();
        if (!out.isNull() && out.isFormatSupported(format)) {
            m_monitor = new QAudioSink(out, format, this);
            m_monitorSound = m_monitor->start();
        }
    }
    emit audibleChanged();
}

QVariantMap Timecode::under(const QVariantList &rows) const
{
    if (!m_engaged)
        return {};
    return timecode::under(rows, frames(), m_chase.format(), m_chase.drop());
}

QString Timecode::faceText(double seconds) const
{
    return timecode::faceText(seconds, m_chase.format());
}

double Timecode::typed(const QString &text) const
{
    return timecode::typed(text, m_chase.format());
}

void Timecode::play()
{
    if (!madeHere() || !m_engaged || m_playing)
        return;
    m_playing = true;
    m_nextEnds = now() + 1 / timecode::rate(m_chase.format());
    m_make.start(5);
}

void Timecode::stop()
{
    m_playing = false;
}

void Timecode::jump(double faceSeconds)
{
    if (!madeHere())
        return;
    m_nextFrame = timecode::frameNumber(timecode::labelAt(faceSeconds, m_chase.format()), m_chase.format(), false);
    if (m_playing)
        m_nextEnds = now() + 1 / timecode::rate(m_chase.format());
    if (m_tick.isActive()) {
        ++m_revision;
        emit revisionChanged();
    }
}
