#include "audiomixer.h"

#include "audiometer.h"
#include "mediasettings.h"
#include "sessionlog.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QAudioSink>
#include <QCoreApplication>
#include <QIODevice>
#include <QJSEngine>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QSettings>

#include <limits>

namespace {

AudioMixer *s_mixer = nullptr;

// How much sound the device is given to hold, in milliseconds: enough that the mixer's
// thread, which does nothing else, is never late with the next; little, because it is
// that much more that sound is heard after its picture. (Qt's own players give theirs
// 110 ms, and play their sound out early to make up for it: see audiomix.h.)
constexpr int kDeviceMs = 30;
// How long the device is kept open after the last player has gone, and no tone is on
constexpr qint64 kLingerMs = 30000;

QString quoted(const QString &name)
{
    return u'"' + name + u'"';
}

// Where each of the app's channels goes on the device, as the settings keep it: the
// channels in order, each the device's channels it goes to, "0;1;5,6;" for four
QString encoded(const audiorouting::Routes &routes)
{
    QStringList channels;
    for (const QList<int> &to : routes) {
        QStringList each;
        for (const int channel : to)
            each << QString::number(channel);
        channels << each.join(u',');
    }
    return channels.join(u';');
}

audiorouting::Routes decoded(const QString &text)
{
    audiorouting::Routes routes;
    for (const QString &channel : text.split(u';')) {
        QList<int> to;
        for (const QString &each : channel.split(u',', Qt::SkipEmptyParts)) {
            bool ok = false;
            const int number = each.toInt(&ok);
            if (ok)
                to << number;
        }
        routes << to;
    }
    return routes;
}

} // namespace

// ---- The device's side, on the mixer's own thread

// What the device reads its sound from: the mixer, asked for as much as the device
// wants each time, in the device's own sample format
class MixDevice : public QIODevice
{
public:
    MixDevice(audiomix::Mixer *mixer, QObject *parent)
        : QIODevice(parent)
        , m_mixer(mixer)
    {
    }

    void setFormat(int channels, bool floats)
    {
        m_channels = channels;
        m_floats = floats;
    }
    bool isSequential() const override { return true; }
    // (Always as much as is asked for: the mixer makes silence where there is nothing)
    qint64 bytesAvailable() const override { return std::numeric_limits<int>::max() / 2 + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 most) override
    {
        const int frameBytes = m_channels * (m_floats ? 4 : 2);
        const int frames = int(qMin<qint64>(most / frameBytes, 1 << 16));
        if (frames <= 0)
            return 0;
        if (m_floats) {
            m_mixer->mix(reinterpret_cast<float *>(data), frames);
        } else {
            // A device that will not take floating point: whole numbers of 16 bits
            m_scratch.resize(size_t(frames) * m_channels);
            m_mixer->mix(m_scratch.data(), frames);
            auto *out = reinterpret_cast<qint16 *>(data);
            for (size_t sample = 0; sample < m_scratch.size(); ++sample)
                out[sample] = qint16(std::lround(m_scratch[sample] * 32767.0f));
        }
        return qint64(frames) * frameBytes;
    }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    audiomix::Mixer *m_mixer;
    int m_channels = 2;
    bool m_floats = true;
    std::vector<float> m_scratch;
};

// The device, opened and closed on the mixer's thread (made there, and only ever called
// there)
class MixOutput : public QObject
{
public:
    // (`owner` outlives it: the mixer stops this one's thread before it goes)
    MixOutput(audiomix::Mixer *mixer, AudioMixer *owner)
        : m_source(new MixDevice(mixer, this))
        , m_mixer(mixer)
        , m_owner(owner)
    {
    }

    // Opens it, and answers on the operator window's thread with what it was opened as
    void open(const QAudioDevice &device, int rate, int channels)
    {
        close();
        QAudioFormat format;
        format.setSampleRate(rate);
        format.setChannelCount(channels);
        // (By its number of channels alone, with no layout: see AudioMixer)
        format.setSampleFormat(QAudioFormat::Float);
        if (!device.isFormatSupported(format))
            format.setSampleFormat(QAudioFormat::Int16);
        m_source->setFormat(channels, format.sampleFormat() == QAudioFormat::Float);
        // Unbuffered: so that Qt reads from the mixer only what its own buffer has room
        // for, as the system takes from it, and not 16 KB at a time to keep in a buffer
        // of the QIODevice's own besides (which was 43 ms of two channels' sound, read in
        // one go every 43 ms: found 2026-10-26, from the user's sync tests)
        m_source->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        m_mixer->restart();
        m_sink = new QAudioSink(device, format, this);
        m_sink->setBufferSize(format.bytesForDuration(kDeviceMs * 1000));
        m_sink->start(m_source);
        const QString error = m_sink->error() == QAudio::NoError ? QString() : QStringLiteral("the system would not open it (error %1)").arg(int(m_sink->error()));
        const int heldMs = int(format.durationForBytes(m_sink->bufferSize()) / 1000);
        const QString said = QStringLiteral("%1 channels at %2 Hz, %3").arg(channels).arg(rate).arg(format.sampleFormat() == QAudioFormat::Float ? QStringLiteral("floating point") : QStringLiteral("16 bits"));
        AudioMixer *owner = m_owner;
        QMetaObject::invokeMethod(owner, [owner, said, heldMs, error] { owner->opened(said, heldMs, error); }, Qt::QueuedConnection);
    }

    void close()
    {
        if (!m_sink)
            return;
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
        m_source->close();
    }

private:
    MixDevice *m_source;
    audiomix::Mixer *m_mixer;
    AudioMixer *m_owner;
    QAudioSink *m_sink = nullptr;
};

// ---- The mixer

AudioMixer::AudioMixer(QObject *parent)
    : QObject(parent)
{
    s_mixer = this;
    m_channels = audiosetupfile::defaults();
    m_tone = QList<bool>(m_channels.size(), false);
    m_thread.setObjectName(QStringLiteral("Sound"));
    m_watch.setInterval(500);
    connect(&m_watch, &QTimer::timeout, this, &AudioMixer::watch);
    configure();
}

AudioMixer::~AudioMixer()
{
    if (m_output) {
        MixOutput *output = m_output;
        QMetaObject::invokeMethod(output, [output] {
            output->close();
            delete output;
        }, Qt::BlockingQueuedConnection);
        m_output = nullptr;
    }
    m_thread.quit();
    m_thread.wait();
    if (s_mixer == this)
        s_mixer = nullptr;
}

AudioMixer *AudioMixer::instance()
{
    if (!s_mixer)
        new AudioMixer(QCoreApplication::instance());
    return s_mixer;
}

AudioMixer *AudioMixer::existing()
{
    return s_mixer;
}

AudioMixer *AudioMixer::create(QQmlEngine *, QJSEngine *)
{
    AudioMixer *mixer = instance();
    // (It is the app's, not QML's: the players made in C++ use it too.)
    QJSEngine::setObjectOwnership(mixer, QJSEngine::CppOwnership);
    return mixer;
}

QVariantList AudioMixer::channels() const
{
    QVariantList list;
    for (qsizetype index = 0; index < m_channels.size(); ++index) {
        const audiosetupfile::Channel &channel = m_channels[index];
        list.append(QVariantMap {{"name", channel.name}, {"muted", channel.muted}, {"solo", channel.solo}, {"tone", m_tone.value(index)}});
    }
    return list;
}

QVariantList AudioMixer::deviceRoutes() const
{
    const int buses = int(m_channels.size());
    const audiorouting::Routes routes = m_routesOwn ? audiorouting::fitted(m_routes, buses, m_deviceChannels)
                                                    : audiorouting::autoDevice(buses, m_deviceChannels);
    QVariantList list;
    for (const QList<int> &to : routes) {
        QVariantList each;
        for (const int channel : to)
            each.append(channel);
        list.append(QVariant(each));
    }
    return list;
}

// ---- The channels, the workspace's

QString AudioMixer::open(const QString &workspace)
{
    m_workspace = workspace;
    const audiosetupfile::Setup setup = audiosetupfile::read(workspace);
    reread();
    QStringList names;
    for (const audiosetupfile::Channel &channel : std::as_const(m_channels))
        names << quoted(channel.name) + (channel.muted ? QStringLiteral(" (muted)") : QString()) + (channel.solo ? QStringLiteral(" (soloed)") : QString());
    SessionLog::write("audio", QStringLiteral("%1 channels%2: %3").arg(m_channels.size()).arg(setup.fromFile ? QStringLiteral(" in the workspace's set-up file") : QStringLiteral(", as a workspace has with none set up"), names.join(QStringLiteral(", "))));
    return setup.error;
}

void AudioMixer::reread()
{
    const audiosetupfile::Setup setup = audiosetupfile::read(m_workspace);
    m_channels = setup.channels;
    m_tone.resize(m_channels.size(), false);
    m_tones = int(m_tone.count(true));
    configure();
    emit channelsChanged();
    emit routesChanged();
}

QString AudioMixer::setChannelCount(int count)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::setCount(m_workspace, count);
    if (!error.isEmpty())
        return error;
    reread();
    SessionLog::write("audio", QStringLiteral("The app has %1 channels now").arg(m_channels.size()));
    return {};
}

QString AudioMixer::rename(int channel, const QString &name)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::rename(m_workspace, channel, name);
    if (error.isEmpty())
        reread();
    return error;
}

QString AudioMixer::setMuted(int channel, bool muted)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::setMuted(m_workspace, channel, muted);
    if (error.isEmpty())
        reread();
    return error;
}

QString AudioMixer::setSolo(int channel, bool solo)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::setSolo(m_workspace, channel, solo);
    if (error.isEmpty())
        reread();
    return error;
}

QString AudioMixer::setAllMuted(bool muted)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::setAllMuted(m_workspace, muted);
    if (error.isEmpty())
        reread();
    return error;
}

QString AudioMixer::clearSolos()
{
    if (m_workspace.isEmpty())
        return QStringLiteral("No workspace is open");
    const QString error = audiosetupfile::clearSolos(m_workspace);
    if (error.isEmpty())
        reread();
    return error;
}

void AudioMixer::setTone(int channel, bool on)
{
    if (channel < 0 || channel >= m_tone.size() || m_tone[channel] == on)
        return;
    m_tone[channel] = on;
    m_tones = int(m_tone.count(true));
    if (on) {
        ensureOpen();
        AudioMeter::wake();
    }
    configure();
    emit channelsChanged();
}

// What the mixer is to do now: the channels and where they go on the device
void AudioMixer::configure()
{
    audiomix::Config config;
    config.buses = int(m_channels.size());
    for (qsizetype index = 0; index < m_channels.size(); ++index) {
        config.muted.push_back(m_channels[index].muted);
        config.soloed.push_back(m_channels[index].solo);
        config.tone.push_back(m_tone.value(index));
    }
    config.deviceChannels = m_deviceChannels;
    config.deviceRoutes = m_routesOwn ? audiorouting::fitted(m_routes, config.buses, m_deviceChannels)
                                      : audiorouting::autoDevice(config.buses, m_deviceChannels);
    m_mixer.setConfig(config);
}

// ---- Where they go on the device, this computer's

QString AudioMixer::routesKey() const
{
    if (m_device.isNull())
        return {};
    // (A device's id is whatever bytes the system has for it; written so that it makes
    // one key of the settings, with no slashes)
    return QStringLiteral("audio/routes/") + QString::fromLatin1(m_device.id().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void AudioMixer::loadRoutes()
{
    // (Only when the device is another: what was changed for this one is kept, whether
    // or not the settings are)
    if (m_device.id() == m_routesDevice)
        return;
    m_routesDevice = m_device.id();
    m_routesOwn = false;
    m_routes.clear();
    const QString key = routesKey();
    if (!m_remember || key.isEmpty())
        return;
    QSettings settings;
    if (!settings.contains(key))
        return;
    m_routes = decoded(settings.value(key).toString());
    m_routesOwn = true;
}

void AudioMixer::saveRoutes()
{
    const QString key = routesKey();
    if (!m_remember || key.isEmpty())
        return;
    QSettings settings;
    if (m_routesOwn)
        settings.setValue(key, encoded(m_routes));
    else
        settings.remove(key);
}

void AudioMixer::toggleRoute(int channel, int deviceChannel)
{
    // (Which device it is for, and how many channels it has, found first)
    if (m_device.isNull())
        look();
    const int buses = int(m_channels.size());
    const audiorouting::Routes now = m_routesOwn ? m_routes : audiorouting::autoDevice(buses, m_deviceChannels);
    m_routes = audiorouting::toggled(now, channel, deviceChannel, buses, m_deviceChannels);
    m_routesOwn = true;
    saveRoutes();
    configure();
    emit routesChanged();
}

void AudioMixer::autoRoutes()
{
    // (Which device it is for, and how many channels it has, found first)
    if (m_device.isNull())
        look();
    m_routesOwn = false;
    m_routes.clear();
    saveRoutes();
    configure();
    emit routesChanged();
}

void AudioMixer::clearRoutes()
{
    // (Which device it is for, and how many channels it has, found first)
    if (m_device.isNull())
        look();
    m_routes = audiorouting::Routes(m_channels.size(), QList<int>());
    m_routesOwn = true;
    saveRoutes();
    configure();
    emit routesChanged();
}

// ---- A media's own

QVariantList AudioMixer::mediaRoutes(const QVariantMap &sound, int channels) const
{
    const int buses = int(m_channels.size());
    audiorouting::Routes routes;
    if (sound.value("custom").toBool()) {
        for (const QVariant &each : sound.value("channels").toList()) {
            const QVariantMap channel = each.toMap();
            const int index = channel.value("index").toInt();
            if (index < 0 || index >= channels)
                continue;
            while (routes.size() <= index)
                routes << QList<int>();
            for (const QVariant &output : channel.value("outputs").toList())
                routes[index] << output.toInt();
        }
        routes = audiorouting::fitted(routes, channels, buses);
    } else {
        routes = audiorouting::autoMedia(channels, buses);
    }
    QVariantList list;
    for (const QList<int> &to : std::as_const(routes)) {
        QVariantList each;
        for (const int output : to)
            each.append(output);
        list.append(QVariant(each));
    }
    return list;
}

QVariantMap AudioMixer::soundChange(const QVariantMap &opened, const QVariantMap &wanted) const
{
    return mediasettings::soundChange(opened, wanted);
}

// ---- The device

QAudioDevice AudioMixer::resolve()
{
    if (!m_system) {
        m_system = new QMediaDevices(this);
        connect(m_system, &QMediaDevices::audioOutputsChanged, this, &AudioMixer::devicesMayHaveChanged);
    }
    if (!m_chosen.isEmpty()) {
        for (const QAudioDevice &output : QMediaDevices::audioOutputs()) {
            if (QString::fromLatin1(output.id().toBase64()) == m_chosen)
                return output;
        }
    }
    // None chosen, or the one chosen not there now: wherever the system sends sound
    return QMediaDevices::defaultAudioOutput();
}

void AudioMixer::look()
{
    const QAudioDevice device = resolve();
    if (m_open && device.id() == m_device.id())
        return;
    if (m_open) {
        close();
        ensureOpen();
        return;
    }
    m_device = device;
    m_deviceName = device.description();
    m_deviceChannels = device.isNull() ? 2 : qBound(1, device.maximumChannelCount(), audiomix::kMostDeviceChannels);
    loadRoutes();
    configure();
    emit deviceChanged();
    emit routesChanged();
}

void AudioMixer::setChosen(const QString &id)
{
    if (id == m_chosen)
        return;
    m_chosen = id;
    // (Only if the system has been asked after already: otherwise the device is found
    // when it is first wanted.)
    if (m_open) {
        close();
        ensureOpen();
    } else if (m_system) {
        look();
    }
}

void AudioMixer::devicesMayHaveChanged()
{
    const QAudioDevice device = resolve();
    if (device.id() == m_device.id())
        return;
    if (m_open) {
        SessionLog::write("audio", QStringLiteral("The sound device is another now"));
        close();
        ensureOpen();
    } else {
        look();
    }
}

void AudioMixer::ensureOpen()
{
    if (m_open)
        return;
    m_device = resolve();
    m_deviceName = m_device.description();
    if (m_device.isNull()) {
        SessionLog::write("PROBLEM", QStringLiteral("There is no sound device: nothing can be heard"));
        m_deviceChannels = 2;
        emit deviceChanged();
        return;
    }
    // As many channels as the device has, up to the most that are filled; at the rate it
    // likes best, which every player's sound is brought to from now (before it is
    // opened, so that the first sound is at the rate it will be heard at)
    m_deviceChannels = qBound(1, m_device.maximumChannelCount(), audiomix::kMostDeviceChannels);
    const int preferred = m_device.preferredFormat().sampleRate();
    m_rate = preferred > 0 ? preferred : 48000;
    m_mixer.setRate(m_rate);
    loadRoutes();
    configure();
    emit deviceChanged();
    emit routesChanged();

    if (!m_output) {
        m_output = new MixOutput(&m_mixer, this);
        m_output->moveToThread(&m_thread);
        m_thread.start();
    }
    MixOutput *output = m_output;
    const QAudioDevice device = m_device;
    const int rate = m_rate;
    const int channels = m_deviceChannels;
    QMetaObject::invokeMethod(output, [output, device, rate, channels] { output->open(device, rate, channels); }, Qt::QueuedConnection);
    m_open = true;
    m_idle.invalidate();
    m_watch.start();
}

void AudioMixer::opened(const QString &format, int heldMs, const QString &error)
{
    if (!error.isEmpty()) {
        SessionLog::write("PROBLEM", QStringLiteral("The sound device %1 will not play: %2").arg(quoted(m_deviceName), error));
        return;
    }
    QStringList routes;
    const QVariantList to = deviceRoutes();
    for (qsizetype channel = 0; channel < to.size(); ++channel) {
        QStringList each;
        for (const QVariant &number : to[channel].toList())
            each << QString::number(number.toInt() + 1);
        routes << QStringLiteral("%1 to %2").arg(channel + 1).arg(each.isEmpty() ? QStringLiteral("none") : each.join(u'+'));
    }
    m_heldMs = heldMs;
    SessionLog::write("audio", QStringLiteral("Sound to %1: %2, Qt holding up to %3 ms of it (the system's sound service and the card hold more, which the app cannot see); the app's channels %4 (%5)")
                                   .arg(quoted(m_deviceName), format)
                                   .arg(heldMs)
                                   .arg(routes.join(QStringLiteral(", ")), m_routesOwn ? QStringLiteral("this computer's routing") : QStringLiteral("Auto")));
}

void AudioMixer::close()
{
    if (!m_open)
        return;
    MixOutput *output = m_output;
    QMetaObject::invokeMethod(output, [output] { output->close(); }, Qt::QueuedConnection);
    m_open = false;
    m_watch.stop();
}

void AudioMixer::watch()
{
    for (auto it = m_watched.begin(); it != m_watched.end();) {
        const std::shared_ptr<audiomix::Feed> feed = it->feed.lock();
        if (!feed) {
            it = m_watched.erase(it);
            continue;
        }
        const int underruns = feed->takeUnderruns();
        const int dropped = feed->takeDropped();
        // (Its name kept, for what is said after its player has gone)
        if (it->owner)
            it->name = it->owner->property("name").toString();
        const QString name = it->name;
        // How far behind its player it is heard, once: what is not lip sync is here
        if (!it->told && feed->heldWhenHeard() > 0) {
            it->told = true;
            const int kept = feed->heldWhenHeard() * 1000 / qMax(1, m_rate);
            SessionLog::write("audio", QStringLiteral("The sound of %1 is heard at least %2 ms after it is played: %3 ms kept for it by the mixer, up to %4 by Qt, and what the system's sound service and the card hold")
                                           .arg(quoted(name))
                                           .arg(kept + m_heldMs)
                                           .arg(kept)
                                           .arg(m_heldMs));
            m_heardAfter = {{"total", kept + m_heldMs}, {"mixer", kept}, {"qt", m_heldMs}};
            emit heardAfterChanged();
        }
        if (underruns > 0)
            SessionLog::write("audio", QStringLiteral("The sound of %1 broke off and came back %2 times: its player was late with it, or was sent elsewhere in its file")
                                           .arg(quoted(name))
                                           .arg(underruns));
        if (dropped > 0)
            SessionLog::write("audio", QStringLiteral("%1 ms of the sound of %2 was let go of: the sound device was held up").arg(dropped * 1000 / qMax(1, m_rate)).arg(quoted(name)));
        ++it;
    }
    // Nothing to play, and no tone, for half a minute: the device is closed
    if (m_mixer.feeds() > 0 || m_tones > 0) {
        m_idle.invalidate();
    } else if (!m_idle.isValid()) {
        m_idle.start();
    } else if (m_idle.elapsed() >= kLingerMs) {
        close();
        SessionLog::write("audio", QStringLiteral("Nothing has played for a while: the sound device is let go of until something does"));
    }
}

std::shared_ptr<audiomix::Feed> AudioMixer::add(QObject *owner)
{
    ensureOpen();
    auto feed = std::make_shared<audiomix::Feed>();
    m_mixer.add(feed);
    m_watched.append({feed, owner, owner ? owner->property("name").toString() : QString()});
    return feed;
}

void AudioMixer::playing(bool on)
{
    m_playing = qMax(0, m_playing + (on ? 1 : -1));
    if (on)
        AudioMeter::wake();
}

// ---- A player's sound

AudioFeed::AudioFeed(QObject *parent)
    : QObject(parent)
    , m_output(new QAudioBufferOutput)
{
    m_feed = AudioMixer::instance()->add(this);
    m_feed->setGain(float(m_gain));
    // Taken on the player's own thread as it is sent, straight into the mixer's ring:
    // the operator window's thread, however busy, is never in its way. (What is used is
    // held here, not this, which may go while the player is in the middle of sending.)
    const std::shared_ptr<audiomix::Feed> feed = m_feed;
    connect(m_output, &QAudioBufferOutput::audioBufferReceived, m_output, [feed](const QAudioBuffer &buffer) {
        if (!buffer.isValid()) {
            // (What the player sends when its sound has come to its end)
            feed->pushEnd();
            return;
        }
        const QAudioFormat format = buffer.format();
        const int channels = format.channelCount();
        const int frames = int(buffer.frameCount());
        if (format.sampleFormat() == QAudioFormat::Float) {
            feed->push(buffer.constData<float>(), frames, channels, format.sampleRate());
            return;
        }
        // Qt's players send floating point; anything else is made so here
        thread_local std::vector<float> converted;
        converted.resize(size_t(frames) * channels);
        const size_t samples = converted.size();
        switch (format.sampleFormat()) {
        case QAudioFormat::Int16:
            for (size_t sample = 0; sample < samples; ++sample)
                converted[sample] = buffer.constData<qint16>()[sample] / 32768.0f;
            break;
        case QAudioFormat::Int32:
            for (size_t sample = 0; sample < samples; ++sample)
                converted[sample] = float(buffer.constData<qint32>()[sample] / 2147483648.0);
            break;
        case QAudioFormat::UInt8:
            for (size_t sample = 0; sample < samples; ++sample)
                converted[sample] = (buffer.constData<quint8>()[sample] - 128) / 128.0f;
            break;
        default:
            return;
        }
        feed->push(converted.data(), frames, channels, format.sampleRate());
    }, Qt::DirectConnection);
}

AudioFeed::~AudioFeed()
{
    if (m_playing) {
        if (AudioMixer *mixer = AudioMixer::existing())
            mixer->playing(false);
    }
    // The player first (which waits until it has stopped sending to the buffer output),
    // and only then the buffer output. A player that has gone has stopped sending.
    if (m_player && m_player->audioBufferOutput() == m_output)
        m_player->setAudioBufferOutput(nullptr);
    delete m_output;
    m_feed->leave();
}

QObject *AudioFeed::player() const
{
    return m_player;
}

void AudioFeed::setPlayer(QObject *object)
{
    QMediaPlayer *player = qobject_cast<QMediaPlayer *>(object);
    if (player == m_player)
        return;
    if (m_player) {
        disconnect(m_position);
        if (m_player->audioBufferOutput() == m_output)
            m_player->setAudioBufferOutput(nullptr);
    }
    m_player = player;
    if (m_player)
        m_player->setAudioBufferOutput(m_output);
    follow();
    emit playerChanged();
}

void AudioFeed::setGain(double gain)
{
    gain = qMax(0.0, gain);
    if (gain == m_gain)
        return;
    m_gain = gain;
    m_feed->setGain(float(gain));
    emit gainChanged();
}

void AudioFeed::setPlaying(bool playing)
{
    if (playing == m_playing)
        return;
    m_playing = playing;
    AudioMixer::instance()->playing(playing);
    // Where its ramp in starts: where it first began to play
    if (playing && m_startAt < 0 && m_player)
        m_startAt = m_player->position() / 1000.0;
    update();
    emit playingChanged();
}

void AudioFeed::setSound(const QVariantMap &sound)
{
    if (sound == m_sound)
        return;
    m_sound = sound;
    audiomix::Settings settings;
    settings.custom = sound.contains("outputs");
    for (const QVariant &channel : sound.value("outputs").toList()) {
        QList<int> to;
        for (const QVariant &output : channel.toList())
            to << output.toInt();
        settings.outputs << to;
    }
    for (const QVariant &gain : sound.value("gains").toList())
        settings.gains.push_back(float(gain.toDouble()));
    m_feed->setSettings(settings);
    m_rampIn = qMax(0.0, sound.value("rampIn").toDouble());
    m_rampOut = qMax(0.0, sound.value("rampOut").toDouble());
    follow();
    emit soundChanged();
}

void AudioFeed::setEndAt(double seconds)
{
    if (seconds == m_endAt)
        return;
    m_endAt = seconds;
    update();
    emit endAtChanged();
}

// Its player's position is followed only for a media with ramps: nothing for the rest
void AudioFeed::follow()
{
    disconnect(m_position);
    if (m_player && (m_rampIn > 0 || m_rampOut > 0))
        m_position = connect(m_player, &QMediaPlayer::positionChanged, this, &AudioFeed::update);
    update();
}

// Where its ramps are, by where its player is now (see audiomix::Feed::setRamps)
void AudioFeed::update()
{
    if (m_rampIn <= 0 && m_rampOut <= 0) {
        m_feed->setRamps(0, 0, 0, -1);
        return;
    }
    const double at = m_player ? m_player->position() / 1000.0 : 0;
    // Gone back to before where it began (round again, or sent there): its ramp in is
    // done with
    if (m_startAt >= 0 && at < m_startAt - 0.05)
        m_rampedIn = true;
    const double sinceStart = m_rampedIn ? 1e9 : m_startAt < 0 ? 0 : at - m_startAt;
    const double end = m_endAt < 0 ? (m_player ? m_player->duration() / 1000.0 : 0) : m_endAt;
    const double untilEnd = end > 0 ? qMax(0.0, end - at) : -1;
    m_feed->setRamps(m_rampIn, sinceStart, m_rampOut, untilEnd);
}
