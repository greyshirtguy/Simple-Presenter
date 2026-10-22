#include "audiolayer.h"

#include "audiolevel.h"
#include "audiometer.h"
#include "sessionlog.h"

#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QUrl>

namespace {

// A stop that is not to click: the sound is taken down over this long, not cut off in
// the middle of a wave.
const qint64 kNoClickMs = 40;

QString quoted(const QString &name)
{
    return u'"' + name + u'"';
}

} // namespace

AudioLayer::AudioLayer(QObject *parent)
    : QObject(parent)
{
    // Fifty steps a second while a fade is under way, which the ear takes for smooth.
    m_fader.setInterval(20);
    connect(&m_fader, &QTimer::timeout, this, &AudioLayer::step);
    m_progress.setInterval(250);
    connect(&m_progress, &QTimer::timeout, this, &AudioLayer::musicPositionChanged);
}

AudioLayer::~AudioLayer()
{
    for (Voice *voice : std::as_const(m_voices)) {
        delete voice->player;
        delete voice->output;
        delete voice->tap;
        delete voice;
    }
}

void AudioLayer::setFade(double seconds)
{
    seconds = qBound(0.0, seconds, 30.0);
    if (seconds == m_fade)
        return;
    m_fade = seconds;
    emit fadeChanged();
}

void AudioLayer::setCrossfade(bool on)
{
    if (on == m_crossfade)
        return;
    m_crossfade = on;
    emit crossfadeChanged();
}

// ---- The device

QMediaDevices *AudioLayer::system()
{
    if (!m_system) {
        m_system = new QMediaDevices(this);
        connect(m_system, &QMediaDevices::audioOutputsChanged, this, &AudioLayer::devicesMayHaveChanged);
    }
    return m_system;
}

void AudioLayer::setDevice(const QString &id)
{
    if (id == m_device)
        return;
    m_device = id;
    // (The system is not asked after its devices for this alone: only if something is
    // playing that is to go over to the one chosen. Whatever is started next asks.)
    for (Voice *voice : std::as_const(m_voices))
        voice->output->setDevice(outputDevice());
    emit deviceChanged();
}

QAudioDevice AudioLayer::outputDevice()
{
    if (m_device.isEmpty())
        return QMediaDevices::defaultAudioOutput();
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    system();
    for (const QAudioDevice &output : outputs) {
        if (QString::fromLatin1(output.id().toBase64()) == m_device)
            return output;
    }
    // (One that was chosen and has gone: until it comes back, where the system sends
    // sound.)
    return QMediaDevices::defaultAudioOutput();
}

bool AudioLayer::deviceFound()
{
    if (m_device.isEmpty())
        return true;
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice &output : outputs) {
        if (QString::fromLatin1(output.id().toBase64()) == m_device)
            return true;
    }
    return false;
}

QVariantList AudioLayer::devices()
{
    system();
    QVariantList list;
    const QByteArray usual = QMediaDevices::defaultAudioOutput().id();
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice &output : outputs) {
        // (A device's id is whatever bytes the system has for it, which are not
        // always text: it is kept in a way that any bytes can be.)
        list.append(QVariantMap {
            {"id", QString::fromLatin1(output.id().toBase64())},
            {"name", output.description()},
            {"usual", output.id() == usual},
        });
    }
    return list;
}

void AudioLayer::devicesMayHaveChanged()
{
    emit devicesChanged();
    // A chosen device that has come back is gone back to, and one that has gone is
    // left for where the system sends sound.
    if (!m_device.isEmpty()) {
        for (Voice *voice : std::as_const(m_voices))
            voice->output->setDevice(outputDevice());
        emit deviceChanged();
    }
}

// ---- The pieces

AudioLayer::Voice *AudioLayer::music()
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (!voice->effect && !voice->leaving && !voice->told)
            return voice;
    }
    return nullptr;
}

void AudioLayer::followMusic()
{
    Voice *now = music();
    QObject *player = now ? now->player : nullptr;
    if (player == m_musicPlayer.data())
        return;
    m_musicPlayer = player;
    emit musicPlayerChanged();
}

double AudioLayer::musicPosition() const
{
    for (const Voice *voice : m_voices) {
        if (!voice->effect && !voice->leaving && !voice->told)
            return voice->player->position() / 1000.0;
    }
    return 0;
}

double AudioLayer::musicDuration() const
{
    for (const Voice *voice : m_voices) {
        if (!voice->effect && !voice->leaving && !voice->told)
            return voice->player->duration() / 1000.0;
    }
    return 0;
}

void AudioLayer::apply(Voice *voice)
{
    const double loud = qBound(0.0, voice->volume * voice->level, 1.0);
    voice->output->setVolume(float(loud));
    voice->tap->setGain(loud);
}

void AudioLayer::fadeTo(Voice *voice, double level, qint64 ms)
{
    voice->from = voice->level;
    voice->to = level;
    voice->fadeMs = qMax<qint64>(0, ms);
    if (voice->fadeMs == 0) {
        voice->level = level;
        apply(voice);
        return;
    }
    voice->fading.start();
    if (!m_fader.isActive())
        m_fader.start();
}

// Takes a piece out over so long, and has done with it when it has gone.
void AudioLayer::leave(Voice *voice, qint64 ms)
{
    voice->leaving = true;
    fadeTo(voice, 0, qMax(ms, kNoClickMs));
    followMusic();
}

void AudioLayer::step()
{
    bool any = false;
    QList<Voice *> gone;
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->fadeMs <= 0)
            continue;
        const double along = qMin(1.0, voice->fading.elapsed() / double(voice->fadeMs));
        // Coming in, and going out, by the two curves that cross evenly; from wherever
        // it was, should it be turned round part of the way.
        const bool in = voice->to > voice->from;
        const double shape = in ? audiolevel::fadeIn(along) : 1 - audiolevel::fadeOut(along);
        voice->level = voice->from + (voice->to - voice->from) * shape;
        apply(voice);
        if (along >= 1) {
            voice->fadeMs = 0;
            voice->level = voice->to;
            if (voice->leaving)
                gone << voice;
        } else {
            any = true;
        }
    }
    for (Voice *voice : std::as_const(gone))
        drop(voice);
    if (!any)
        m_fader.stop();
}

void AudioLayer::drop(Voice *voice)
{
    if (!m_voices.removeOne(voice))
        return;
    voice->tap->setPlaying(false);
    voice->player->stop();
    // (Not deleted from inside one of its own signals.)
    voice->player->deleteLater();
    voice->output->deleteLater();
    voice->tap->deleteLater();
    if (voice->limit)
        voice->limit->deleteLater();
    delete voice;
    if (!music()) {
        m_progress.stop();
        emit musicPositionChanged();
    }
    followMusic();
    emit voicesChanged();
}

void AudioLayer::start(const QVariantMap &audio)
{
    auto *voice = new Voice;
    voice->play = audio.value("play").toInt();
    voice->effect = audio.value("effect").toBool();
    voice->name = audio.value("name").toString();
    voice->playback = audio.value("playback").toString();
    voice->volume = qBound(0.0, audio.value("volume", 1.0).toDouble(), 1.0);

    // Music takes the place of music: the piece that is playing crosses with this one,
    // or stops for it. (One that has been said to be at its end, with its last moments
    // still sounding, is left to play them out: this one is what follows it.)
    Voice *before = voice->effect ? nullptr : music();
    bool crossing = false;
    for (Voice *other : std::as_const(m_voices)) {
        if (!other->effect && !other->leaving && other->told)
            crossing = true;
    }
    if (before) {
        crossing = m_crossfade && m_fade > 0;
        leave(before, crossing ? qint64(m_fade * 1000) : 0);
    }

    voice->tap = new AudioTap(this);
    voice->output = new QAudioOutput(this);
    if (!m_device.isEmpty())
        voice->output->setDevice(outputDevice());
    voice->player = new QMediaPlayer(this);
    voice->player->setAudioOutput(voice->output);
    voice->player->setAudioBufferOutput(voice->tap->output());
    const int loopCount = audio.value("loopCount").toInt();
    const double loopSeconds = audio.value("loopSeconds").toDouble();
    if (voice->playback == QLatin1String("loop"))
        voice->player->setLoops(loopCount > 0 ? loopCount : QMediaPlayer::Infinite);
    const int play = voice->play;
    connect(voice->player, &QMediaPlayer::mediaStatusChanged, this, [this, play](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            ended(play);
    });
    connect(voice->player, &QMediaPlayer::errorOccurred, this, [this, play, name = voice->name](QMediaPlayer::Error, const QString &words) {
        SessionLog::write("PROBLEM", QStringLiteral("The audio %1 will not play: %2").arg(quoted(name), words));
        ended(play);
    });
    if (!voice->effect && voice->playback == QLatin1String("next"))
        connect(voice->player, &QMediaPlayer::positionChanged, this, [this, play] { nearing(play); });
    // Its playback markers, for a piece that has any: looked at as the player says where
    // it is, and for no other
    voice->markers = audio.value("markers").toList();
    if (!voice->markers.isEmpty()) {
        voice->watchingMarkers = true;
        connect(voice->player, &QMediaPlayer::positionChanged, this, [this, play] { passMarkers(play); });
    }
    if (voice->playback == QLatin1String("loop") && loopSeconds > 0) {
        // One that goes round for a length of time has come to its end when the time is up.
        voice->limit = new QTimer(this);
        voice->limit->setSingleShot(true);
        connect(voice->limit, &QTimer::timeout, this, [this, play] { ended(play); });
        voice->limit->start(int(loopSeconds * 1000));
    }

    m_voices.append(voice);
    voice->level = crossing && !voice->effect ? 0 : 1;
    apply(voice);
    if (voice->level < 1)
        fadeTo(voice, 1, qint64(m_fade * 1000));
    voice->player->setSource(audio.value("source").toUrl().isValid() ? audio.value("source").toUrl()
                                                                     : QUrl::fromLocalFile(audio.value("path").toString()));
    voice->tap->setPlaying(true);
    voice->player->play();
    SessionLog::write("audio", QStringLiteral("%1 started%2%3").arg(quoted(voice->name), crossing && !voice->effect ? QStringLiteral(", crossing with what was playing") : QString(),
                                                                   m_device.isEmpty() ? QString()
                                                                   : deviceFound() ? QStringLiteral(", through ") + quoted(voice->output->device().description())
                                                                   : QStringLiteral(", wherever the system sends sound: the device chosen for it is not there now")));
    if (!voice->effect && !m_progress.isActive())
        m_progress.start();
    followMusic();
    emit voicesChanged();
    emit musicPositionChanged();
}

// A piece has played to its end (or will not play, or has gone round for its time).
void AudioLayer::ended(int play)
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->play != play)
            continue;
        // One that is on its way out, or that the show has already been told of, is
        // simply done with.
        const bool tell = !voice->leaving && !voice->told;
        if (voice->limit && voice->player->playbackState() == QMediaPlayer::PlayingState && !voice->leaving) {
            // (Its time is up while it is still sounding: it is taken out, not cut.)
            voice->told = true;
            leave(voice, kNoClickMs);
        } else {
            drop(voice);
        }
        if (tell)
            emit finished(play);
        return;
    }
}

void AudioLayer::passMarkers(int play)
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->play != play || voice->leaving)
            continue;
        const double now = voice->player->position() / 1000.0;
        const double was = voice->lastAt;
        voice->lastAt = now;
        // (A jump forward of more than a second, or back but to the first second as going
        // round is, says nothing of the markers between: see MediaContent.)
        const double from = now < was ? (now < 1 ? -1 : now) : now - was > 1 ? now : was;
        const QVariantList markers = voice->markers;
        for (const QVariant &entry : markers) {
            const double time = entry.toMap().value("time").toDouble();
            if (time > from && time <= now)
                emit markerReached(play, entry.toMap());
        }
        return;
    }
}

// Music the next follows is one fade's length from its end: with `crossfade` on, that
// is when the next is to start, so that the two cross.
void AudioLayer::nearing(int play)
{
    if (!m_crossfade || m_fade <= 0)
        return;
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->play != play || voice->told || voice->leaving)
            continue;
        const qint64 length = voice->player->duration();
        const qint64 fade = qint64(m_fade * 1000);
        // (A piece shorter than two fades is not crossed out of: it would be going
        // out before it had come in.)
        if (length < 2 * fade || voice->player->position() < length - fade)
            return;
        voice->told = true;
        // Its last moments go out as the next comes in. Should nothing follow it, it
        // has faded out over its own ending, which is no loss.
        fadeTo(voice, 0, length - voice->player->position());
        followMusic();
        emit finished(play);
        return;
    }
}

void AudioLayer::setMarkers(int play, const QVariantList &markers)
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->play != play)
            continue;
        voice->markers = markers;
        if (!markers.isEmpty() && !voice->watchingMarkers) {
            voice->watchingMarkers = true;
            connect(voice->player, &QMediaPlayer::positionChanged, this, [this, play] { passMarkers(play); });
        }
    }
}

void AudioLayer::stopMusic()
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (!voice->effect && !voice->leaving)
            leave(voice, qint64(m_fade * 1000));
    }
    m_progress.stop();
    emit musicPositionChanged();
}

void AudioLayer::stopSounds()
{
    for (Voice *voice : std::as_const(m_voices)) {
        if (voice->effect && !voice->leaving)
            leave(voice, qint64(m_fade * 1000));
    }
}
