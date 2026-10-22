#pragma once

#include <QAudioDevice>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class AudioTap;
class QAudioOutput;
class QMediaDevices;
class QMediaPlayer;

// What plays the audio layer: the music and the sound effects of the audio bin.
//
// What is to be playing is not decided here. That is the show's (showstate.h: music is
// one piece at a time, sound effects are any number, this clears that), and the Show
// says what is to happen as it changes: start this piece, stop the music, stop the
// sound effects. This does those three things with the system's players of media, and
// tells the show when a piece has come to its end, by the number the show gave that
// playing of it, so that the show can say what follows.
//
// How a change sounds is here, as how a slide changes is the window's and not the
// show's:
//
//   - Music that takes the place of music. With `crossfade` on, the piece that is
//     playing goes out over the fade's length while the new one comes in over the same
//     (audiolevel.h has the shape of the two). With it off, the one stops and the
//     other starts, at once.
//   - Music that the next of its playlist follows. With `crossfade` on, it is said to
//     be at its end one fade's length before its end, so that the next starts under
//     its last moments and the two cross. With it off, the next starts when it has
//     ended. Music that nothing follows plays to its end either way.
//   - A sound effect starts at once and as loud as it is.
//   - Clearing: the music, or the sound effects, go out over the fade's length and are
//     then stopped. A fade of no length is a stop.
//
// A piece is as loud as its row says (`volume`), times the fade it is at.
//
// THESE ARE THIS APP'S OWN CHOICES. ProPresenter's audio bin has a fade and a
// crossfade too; how long its crossfade between two pieces of music is, and what
// exactly its fade applies to, have not been looked at. The fade's length here is the
// one the audio bin's slider sets, a second unless changed, and it is the length of
// the crossfade too.
//
// The device the sound goes to is here as well, for everything the app plays: the
// pieces played here, and through `outputDevice` a video's sound (MediaContent.qml).
// With none chosen, sound goes wherever the system sends it, and follows the system
// if that changes. The system's devices are not asked after until something needs to
// know (a device has been chosen, or the settings list them): asking means talking to
// the system's sound service, which nothing should wait for as the app starts.
//
// There is one of these, which QML reaches by its name.
class AudioLayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // How long a fade is, in seconds, and whether music crosses with the music it
    // takes the place of. Kept by the operator window, which sets them here.
    Q_PROPERTY(double fade READ fade WRITE setFade NOTIFY fadeChanged)
    Q_PROPERTY(bool crossfade READ crossfade WRITE setCrossfade NOTIFY crossfadeChanged)
    // The device sound is played through: its id as the system has it, "" for wherever
    // the system sends sound; that device, for a player made in QML; whether a device
    // that was chosen is one the system has now (it may be unplugged); and the devices
    // there are, { id, name, usual } each, `usual` marking the one the system would use
    // itself.
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(QAudioDevice outputDevice READ outputDevice NOTIFY deviceChanged)
    Q_PROPERTY(bool deviceFound READ deviceFound NOTIFY deviceChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // How far through the music that is playing is, and how long it is, in seconds
    // (0 and 0 with none): told about four times a second, for a row to show
    Q_PROPERTY(double musicPosition READ musicPosition NOTIFY musicPositionChanged)
    Q_PROPERTY(double musicDuration READ musicDuration NOTIFY musicPositionChanged)
    // The player of the music that is playing (a QMediaPlayer), or null with none: what
    // the transport for the audio works, as the video's works the video's player. One
    // on its way out, or past its end with the next crossing in, is not it. A pause, or
    // a move, made there is the player's alone: the show still has the piece as the
    // music that is playing, and its end, when it comes, is told as ever.
    Q_PROPERTY(QObject *musicPlayer READ musicPlayer NOTIFY musicPlayerChanged)
    // How many pieces are sounding now, those on their way out among them
    Q_PROPERTY(int voices READ voices NOTIFY voicesChanged)

public:
    explicit AudioLayer(QObject *parent = nullptr);
    ~AudioLayer() override;

    double fade() const { return m_fade; }
    void setFade(double seconds);
    bool crossfade() const { return m_crossfade; }
    void setCrossfade(bool on);
    QString device() const { return m_device; }
    void setDevice(const QString &id);
    QAudioDevice outputDevice();
    bool deviceFound();
    QVariantList devices();
    double musicPosition() const;
    double musicDuration() const;
    QObject *musicPlayer() const { return m_musicPlayer; }
    int voices() const { return int(m_voices.size()); }

    // Starts a piece of audio, as the show hands it over (a row of the audio bin with
    // `play`, the number of this playing of it): music in place of the music that is
    // playing, a sound effect over whatever is.
    Q_INVOKABLE void start(const QVariantMap &audio);
    // The playback markers of the piece playing as `play` have been changed (from the
    // transport, or in the Media Inspector): it goes by `markers` from now.
    Q_INVOKABLE void setMarkers(int play, const QVariantList &markers);
    // Stops the music; and every sound effect.
    Q_INVOKABLE void stopMusic();
    Q_INVOKABLE void stopSounds();

signals:
    void fadeChanged();
    void crossfadeChanged();
    void deviceChanged();
    void devicesChanged();
    void musicPositionChanged();
    void musicPlayerChanged();
    void voicesChanged();
    // A piece has come to its end, or, for music the next follows with `crossfade` on,
    // is one fade's length from it.
    void finished(int play);
    // Playback has reached one of a piece's playback markers (`marker` as
    // playbackmarkers::describe has it), for its actions to be run (Show.runMarker)
    void markerReached(int play, const QVariantMap &marker);

private:
    struct Voice
    {
        int play = 0;
        bool effect = false;
        QString name;
        QString playback;
        QMediaPlayer *player = nullptr;
        QAudioOutput *output = nullptr;
        AudioTap *tap = nullptr;
        // How loud its row says it is, and the fade it is at: from `from` to `to`
        // over `fadeMs` from when `fading` was started
        double volume = 1;
        double level = 1;
        double from = 1;
        double to = 1;
        qint64 fadeMs = 0;
        QElapsedTimer fading;
        // It is on its way out, and is stopped when it has gone
        bool leaving = false;
        // The show has been told it is at its end
        bool told = false;
        // Its playback markers, and where it was last looked at, in seconds (see
        // passMarkers)
        QVariantList markers;
        double lastAt = -1;
        // Its player is looked at for them (once there have been any)
        bool watchingMarkers = false;
        // It goes round for so long, and has so long left of that
        QTimer *limit = nullptr;
    };

    Voice *music();
    // Says, if it is so, that the music's player is another now
    void followMusic();
    // Says each of a piece's markers that playback has reached since it was last looked
    // at (the rule is MediaContent's for a video's: see there)
    void passMarkers(int play);
    void fadeTo(Voice *voice, double level, qint64 ms);
    void leave(Voice *voice, qint64 ms);
    void apply(Voice *voice);
    void drop(Voice *voice);
    void step();
    void ended(int play);
    void nearing(int play);
    QMediaDevices *system();
    void devicesMayHaveChanged();

    QList<Voice *> m_voices;
    QPointer<QObject> m_musicPlayer;
    double m_fade = 1;
    bool m_crossfade = false;
    QString m_device;
    QMediaDevices *m_system = nullptr;
    // Steps every fade that is under way; stopped when none is
    QTimer m_fader;
    QTimer m_progress;
};
