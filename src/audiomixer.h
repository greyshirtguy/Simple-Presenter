#pragma once

#include "audiomix.h"
#include "audiosetupfile.h"

#include <QAudioDevice>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QAudioBufferOutput;
class QJSEngine;
class QMediaDevices;
class QMediaPlayer;
class QQmlEngine;
class MixOutput;

// The app's sound, all of it but a video input's: its channels, the device they go to,
// and the mixer between (audiomix.h has how it mixes, and why the app mixes its own).
//
//   - The app's channels are the workspace's, in ProPresenter's file of how it is set up
//     (audiosetupfile.h): how many, their names, and their Mute and Solo, which are
//     changed here and written there. A channel's test tone is only for now, and is
//     never written.
//   - The device is the one chosen in the settings (AudioLayer has the choice, and tells
//     this), or the system's own where none is, which is followed if the system changes
//     it. It is opened with as many channels as it has, up to 32, at the rate it likes
//     best, and each channel the app sends is the device's channel of that number: on
//     the system's sound service a device opened by its number of channels alone is
//     sent them in order (tried on PipeWire on 2026-10-25, through both of Qt's ways to
//     it, with a surround device and with one whose channels have no names; told that
//     they are a surround layout, it moved them about).
//   - Where each of the app's channels goes on the device is this computer's, for each
//     device it has (another computer has other devices): kept in the app's settings
//     ("audio/routes/<the device>") once it has been changed, and until then
//     ProPresenter's Auto, the app's first channel to the device's first and so on.
//
// The device is opened on a thread of its own, which does nothing else, so that nothing
// the operator window is doing can keep it waiting for sound. It is opened when the first
// player that is to be heard is made (as its file is being opened, so that the two are
// ready together), and closed half a minute after the last has gone and no tone is on:
// nothing of it is kept running for nothing. The system's sound service is not asked
// after its devices until then, either.
//
// There is one of these, which QML reaches by its name.
class AudioMixer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The app's channels, { name, muted, solo, tone } each, and the most there can be
    Q_PROPERTY(QVariantList channels READ channels NOTIFY channelsChanged)
    Q_PROPERTY(int most READ most CONSTANT)
    // The device: its name, and how many channels it has. Known once the device has been
    // opened, or looked at (look()).
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(int deviceChannels READ deviceChannels NOTIFY deviceChanged)
    // Where each of the app's channels goes on the device: for each, the device's
    // channels, from 0; and whether that is this computer's own, or Auto
    Q_PROPERTY(QVariantList deviceRoutes READ deviceRoutes NOTIFY routesChanged)
    Q_PROPERTY(bool deviceRoutesOwn READ deviceRoutesOwn NOTIFY routesChanged)
    // Whether this computer's routing is read from and kept in the settings (not for
    // the self-test)
    Q_PROPERTY(bool remember MEMBER m_remember NOTIFY rememberChanged)
    // How late the sound last begun is heard, as far as the app can see, in milliseconds:
    // { total, mixer, qt }, what the mixer kept of it when it was first heard and what Qt
    // holds (the system's sound service and the card hold more, which Qt does not ask
    // them about); empty before anything has been heard. For the screens' statistics.
    Q_PROPERTY(QVariantMap heardAfter READ heardAfter NOTIFY heardAfterChanged)

public:
    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer() override;

    // The one there is, made when first asked for; and the same for QML
    static AudioMixer *instance();
    static AudioMixer *create(QQmlEngine *, QJSEngine *);

    QVariantList channels() const;
    int most() const { return audiomix::kMostBuses; }
    QString deviceName() const { return m_deviceName; }
    int deviceChannels() const { return m_deviceChannels; }
    QVariantList deviceRoutes() const;
    bool deviceRoutesOwn() const { return m_routesOwn; }
    QVariantMap heardAfter() const { return m_heardAfter; }

    // Reads the channels of a workspace folder. Answers with what went wrong, or nothing.
    Q_INVOKABLE QString open(const QString &workspace);
    // Changes to the channels, each written to the workspace's file; each answers with
    // what went wrong, or nothing (see audiosetupfile.h)
    Q_INVOKABLE QString setChannelCount(int count);
    Q_INVOKABLE QString rename(int channel, const QString &name);
    Q_INVOKABLE QString setMuted(int channel, bool muted);
    Q_INVOKABLE QString setSolo(int channel, bool solo);
    Q_INVOKABLE QString setAllMuted(bool muted);
    Q_INVOKABLE QString clearSolos();
    // A channel's test tone, on or off
    Q_INVOKABLE void setTone(int channel, bool on);
    // Where the app's channels go on the device: one cell of the matrix clicked; Auto;
    // and none anywhere
    Q_INVOKABLE void toggleRoute(int channel, int deviceChannel);
    Q_INVOKABLE void autoRoutes();
    Q_INVOKABLE void clearRoutes();
    // Finds out which the device is and how many channels it has, without opening it:
    // for the settings, as they are shown
    Q_INVOKABLE void look();
    // For a media's Audio tab: where each of its `channels` channels goes among the
    // app's by its `sound` (mediasettings::sound), its own mapping or Auto; and the
    // changes that make its sound what `wanted` says (mediasettings::soundChange)
    Q_INVOKABLE QVariantList mediaRoutes(const QVariantMap &sound, int channels) const;
    Q_INVOKABLE QVariantMap soundChange(const QVariantMap &opened, const QVariantMap &wanted) const;

    // The device chosen in the settings, by its id as AudioLayer has it ("" for the
    // system's own)
    void setChosen(const QString &id);
    // A player's sound to be mixed (AudioFeed `owner`, whose `name` the log calls it
    // by): the mixer keeps it until it has left and faded
    std::shared_ptr<audiomix::Feed> add(QObject *owner);
    // The one there is, or null if there is none (not made by asking)
    static AudioMixer *existing();
    // A feed has started or stopped playing (for the meter)
    void playing(bool on);
    // Whether anything is playing, or a tone is on
    bool sounding() const { return m_playing > 0 || m_tones > 0; }
    // The loudest the app's first two channels have been since this was last asked
    void takePeaks(float *left, float *right) { m_mixer.takePeaks(left, right); }

signals:
    void channelsChanged();
    void deviceChanged();
    void routesChanged();
    void rememberChanged();
    void heardAfterChanged();

private:
    friend class MixOutput;

    void reread();
    void configure();
    void ensureOpen();
    void close();
    void watch();
    QAudioDevice resolve();
    QString routesKey() const;
    void loadRoutes();
    void saveRoutes();
    void devicesMayHaveChanged();
    void opened(const QString &format, int bufferMs, const QString &error);

    audiomix::Mixer m_mixer;
    QString m_workspace;
    QList<audiosetupfile::Channel> m_channels;
    QList<bool> m_tone;
    int m_tones = 0;
    int m_playing = 0;
    bool m_remember = true;

    QString m_chosen;
    QMediaDevices *m_system = nullptr;
    QAudioDevice m_device;
    QString m_deviceName;
    int m_deviceChannels = 2;
    int m_rate = 48000;
    // How much the device holds, in milliseconds, as it said when it was opened
    int m_heldMs = 0;
    QVariantMap m_heardAfter;
    audiorouting::Routes m_routes;
    bool m_routesOwn = false;
    // The device they are for
    QByteArray m_routesDevice;
    bool m_open = false;

    QThread m_thread;
    MixOutput *m_output = nullptr;
    // Every half second while the device is open: closes it a while after the last
    // player has gone, and says in the log if a player's sound ran dry
    QTimer m_watch;
    QElapsedTimer m_idle;
    struct Watched
    {
        std::weak_ptr<audiomix::Feed> feed;
        QPointer<QObject> owner;
        QString name;
        // Whether the log has been told how far behind it is heard
        bool told = false;
    };
    QList<Watched> m_watched;
};

// A player's sound, handed to the app's mixer: what a player that is to be heard is
// given instead of an audio output of its own. In QML, inside what plays a video:
//
//     AudioFeed { player: mediaPlayer; gain: ...; playing: ...; sound: content.sound }
//
// It gives the player a buffer output (Qt plays sound out to one in time with its
// picture), and takes the sound from it on the player's own thread, straight into the
// mixer's ring for it, so that the sound never waits on the operator window. When it is
// done with it lets the player go first, and only then the buffer output: Qt changes a
// player's output on the player's thread, and waits until it has, so after that nothing
// more is sent to the one let go of.
//
// The media's ramps are here: its sound comes in over `rampIn` seconds from where it
// began to play, and goes out over the last `rampOut` seconds before where it stops
// (`endAt`). Where it is in them is told the mixer each time the player says where it
// is (a twentieth of a second apart), and the mixer goes on between by what it has
// mixed, easing toward what the player says rather than jumping to it, so that a ramp
// is smooth (audiomix::Feed::setRamps; audiorouting::rampGain has the curve).
class AudioFeed : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // The QMediaPlayer whose sound this is
    Q_PROPERTY(QObject *player READ player WRITE setPlayer NOTIFY playerChanged)
    // How loud, from 0 to 1: its volume and its fade
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY gainChanged)
    // Whether it is playing (for the meter, and for where a ramp in starts)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // Where its channels go and how loud each is, and its ramps: as
    // audiorouting::soundOf has them; empty for as it is
    Q_PROPERTY(QVariantMap sound READ sound WRITE setSound NOTIFY soundChanged)
    // Where it stops, in seconds into its file, for a ramp out: -1 for the end of its
    // file, 0 for nowhere (one that goes round)
    Q_PROPERTY(double endAt READ endAt WRITE setEndAt NOTIFY endAtChanged)
    // What the log calls it
    Q_PROPERTY(QString name MEMBER m_name NOTIFY nameChanged)

public:
    explicit AudioFeed(QObject *parent = nullptr);
    ~AudioFeed() override;

    QObject *player() const;
    void setPlayer(QObject *player);
    double gain() const { return m_gain; }
    void setGain(double gain);
    bool playing() const { return m_playing; }
    void setPlaying(bool playing);
    QVariantMap sound() const { return m_sound; }
    void setSound(const QVariantMap &sound);
    double endAt() const { return m_endAt; }
    void setEndAt(double seconds);

signals:
    void playerChanged();
    void gainChanged();
    void playingChanged();
    void soundChanged();
    void endAtChanged();
    void nameChanged();

private:
    void update();
    double ramp();
    void follow();

    std::shared_ptr<audiomix::Feed> m_feed;
    QAudioBufferOutput *m_output = nullptr;
    QPointer<QMediaPlayer> m_player;
    QMetaObject::Connection m_position;
    double m_gain = 1;
    bool m_playing = false;
    QVariantMap m_sound;
    double m_endAt = -1;
    double m_rampIn = 0;
    double m_rampOut = 0;
    double m_startAt = -1;
    bool m_rampedIn = false;
    QString m_name;
};
