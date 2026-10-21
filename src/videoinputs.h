#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlparserstatus.h>
#include <QtQml/qqmlregistration.h>

class QAudioInput;
class QAudioOutput;
class QCamera;
class QMediaCaptureSession;
class QTimer;
class QVideoSink;

// The workspace's video inputs, for QML: the list ProPresenter keeps in the workspace
// (videoinputfile.h), each given one of this computer's capture devices, and the
// capturing of them, which is done only while something shows them.
//
// What is whose (see videoinputfile.h): the list, by name and order, is the workspace's
// and is changed in ProPresenter's file; which of this computer's devices an input is,
// the mode it is captured in, its sound and how loud, and its thumbnail, are this
// computer's, and are kept in a file of this app's own (VideoInputs.json, in its
// settings folder, a set for each workspace by the inputs' ids, as Devices.json keeps
// the communication devices), and its thumbnail as a picture in the app's data folder.
// A device is found again by its id, which on Linux is where it is (/dev/video0), or
// failing that by its name, which a device moved to another socket keeps. Nothing is
// read or kept where `remember` is off (the self-test, the benchmark, the scripted
// tests unless they ask): the choices there last as long as the run.
//
// Capturing, and what it costs. An input is captured while anything holds it: the
// video input layer of the screens showing it, a shape filled with it on a slide that
// is drawn, the preview of it in the settings (see VideoInputFeed, which holds one
// while it is there). It is captured once, however many hold it, into one sink, from
// which each screen is handed the frames (with FrameRelay, as a video's are). The
// last to let go of it stops it, a second and a half later, so that a cut from one
// slide with it to another does not stop and start the camera. So an input nothing
// shows costs nothing, and the list of devices is not asked for until something wants
// it (asking starts Qt's multimedia, which takes a moment the app's start would
// otherwise have to wait for).
//
// Its sound (the maintainer's choice, 2026-10-21, "with its sound"). An input's sound
// is played while anything that holds it wants it: what shows it on an audience
// screen, as ProPresenter's help has it ("the audio plays whenever the video input is
// active on an Audience screen"). Its source is the device's own (Automatic, which is
// the audio input whose name is like the device's: see videoinputfile::ownAudio), one of
// the computer's audio inputs, or none; it is played on the output the app's audio
// plays on, as loud as the input's own volume says. NOT DONE YET: ProPresenter's Delay,
// for sound that comes before its picture, its channel levels and its routing.
//
// A pretend device. With SP_TEST_VIDEOINPUT set (the scripted tests, which have no
// camera), the devices have one more, "Test Pattern", whose picture is one colour,
// which the test can change (setTestColor): so what reaches the screens can be looked
// for pixel by pixel.
class VideoInputs : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The workspace's inputs, in their order, each
    //   { id, name, index (its place, which is what files name it by), color, device
    //     (this computer's device it is, by its id; "" for none), deviceName, missing
    //     (it has a device that is not here now), mode (its key; "" for Automatic),
    //     modeLabel (what Automatic is, or what was asked for), audio ("auto", "none" or
    //     an audio input's id), audioName, volume (0 to 1), thumbnail (a url, or ""),
    //     proDevice (what ProPresenter's file names its device), capturing, problem }
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY changed)
    // This computer's capture devices, { id, name, modes: [{ key, label }] }, and its
    // audio inputs, { id, name }: asked for when first read (see above)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList audioSources READ audioSources NOTIFY devicesChanged)
    // The audio output an input's sound is played on, by its id ("" for the system's
    // own), which the operator window says (its setting for the app's audio)
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioDeviceChanged)
    Q_PROPERTY(bool remember MEMBER m_remember NOTIFY rememberChanged)

public:
    explicit VideoInputs(QObject *parent = nullptr);
    ~VideoInputs() override;

    // The one there is, for the feeds
    static VideoInputs *instance();

    QVariantList inputs() const { return m_inputs; }
    QVariantList devices();
    QVariantList audioSources();
    QString audioDevice() const { return m_audioDevice; }
    void setAudioDevice(const QString &device);

    // Reads the inputs of a workspace folder, and what this computer has chosen for
    // them. Returns an error message, empty on success.
    Q_INVOKABLE QString open(const QString &workspace);

    // Each returns an error message, empty on success. A new input is { id, error }.
    Q_INVOKABLE QVariantMap add(const QString &name);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);
    Q_INVOKABLE QString setDevice(const QString &id, const QString &device);
    Q_INVOKABLE QString setMode(const QString &id, const QString &mode);
    Q_INVOKABLE QString setAudio(const QString &id, const QString &source);
    Q_INVOKABLE QString setVolume(const QString &id, double volume);
    // Its thumbnail: a picture taken of it now (it is captured for a moment, if it is
    // not already), a picture file of the operator's, or none again
    Q_INVOKABLE QString takeThumbnail(const QString &id);
    Q_INVOKABLE QString setThumbnail(const QString &id, const QUrl &file);
    Q_INVOKABLE QString clearThumbnail(const QString &id);

    // An input by its place, or by its id, as `inputs` has it; an empty map for none
    Q_INVOKABLE QVariantMap at(int index) const;
    Q_INVOKABLE QVariantMap find(const QString &id) const;

    // The pretend device's colour (see above)
    Q_INVOKABLE void setTestColor(const QString &color);

    // For the feeds: holding an input, and letting it go; and where its frames arrive
    void hold(const QString &id, bool sound);
    void letGo(const QString &id, bool sound);
    QVideoSink *sinkOf(const QString &id) const;
    bool sendingOf(const QString &id) const;

signals:
    void changed();
    void devicesChanged();
    void audioDeviceChanged();
    void rememberChanged();
    // An input began, or stopped, sending frames
    void capturingChanged(const QString &id);

private:
    struct Choice
    {
        QString device;
        QString deviceName;
        QString mode;
        QString audio = QStringLiteral("auto");
        double volume = 1;
    };
    struct Capture
    {
        QCamera *camera = nullptr;
        QMediaCaptureSession *session = nullptr;
        QVideoSink *sink = nullptr;
        QAudioInput *audioIn = nullptr;
        QAudioOutput *audioOut = nullptr;
        QTimer *pattern = nullptr;
        QTimer *stopping = nullptr;
        int holds = 0;
        int sounds = 0;
        bool sending = false;
        QString problem;
    };

    void reload();
    // This computer's choices for an input, made if it has none yet, starting where
    // ProPresenter's file has it (its sound: see reload)
    Choice &choiceFor(const QString &id);
    // (Without saying so where it is a read of the list that asks for them: the reader
    // has them, and saying so then is a loop of the binding that read them)
    void loadDevices(bool announce = true);
    void readChoices();
    void writeChoices() const;
    QString choiceFile() const;
    QString thumbnailOf(const QString &id) const;
    // (Re)starts the capturing of an input as its choices now say, if anything holds it
    void start(const QString &id);
    void stop(const QString &id);
    void setSound(Capture &capture, const QString &id);
    void pushPattern(Capture &capture);
    int cameraIndex(const Choice &choice) const;

    QString m_workspace;
    QVariantList m_inputs;
    QHash<QString, Choice> m_choices;
    QHash<QString, Capture> m_captures;
    // The devices, once asked for: (Qt's own descriptions, kept as plain data)
    bool m_devicesKnown = false;
    QVariantList m_devices;
    QVariantList m_audioSources;
    QString m_audioDevice;
    QString m_testColor = QStringLiteral("#ff0000");
    bool m_remember = true;
    bool m_test = false;
};

// What holds an input while it is there: the video input layer of a screen, a shape
// filled with one, the preview in the settings. It names the input by its place
// (`index`, as files do) or by its id; `sound` says whether what it shows it on wants
// its sound (an audience screen does; a preview does not). `sink` is where the
// input's frames arrive, for a FrameRelay to take them from; `sending` says whether any
// have. It holds nothing until QML has set all it is given, so that one made not
// `active`, or with its sound, does not hold the input for a moment first (which would
// start a camera).
class VideoInputFeed : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY inputChanged)
    Q_PROPERTY(QString inputId READ inputId WRITE setInputId NOTIFY inputChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool sound READ sound WRITE setSound NOTIFY soundChanged)
    Q_PROPERTY(QObject *sink READ sink NOTIFY sinkChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sinkChanged)

public:
    explicit VideoInputFeed(QObject *parent = nullptr);
    ~VideoInputFeed() override;

    int index() const { return m_index; }
    void setIndex(int index);
    QString inputId() const { return m_inputId; }
    void setInputId(const QString &id);
    bool active() const { return m_active; }
    void setActive(bool active);
    bool sound() const { return m_sound; }
    void setSound(bool sound);
    QObject *sink() const;
    bool sending() const;

    void classBegin() override {}
    void componentComplete() override;

signals:
    void inputChanged();
    void activeChanged();
    void soundChanged();
    void sinkChanged();

private:
    // The input this holds now, by id: by its id if one is given, else by its place
    QString wanted() const;
    void update();

    int m_index = -1;
    QString m_inputId;
    bool m_active = true;
    bool m_sound = false;
    // What it holds now, and whether with its sound; and whether it is following the
    // list
    QString m_held;
    bool m_heldSound = false;
    bool m_following = false;
    // (Made by QML, and not yet given all it was given there)
    bool m_complete = false;
};
