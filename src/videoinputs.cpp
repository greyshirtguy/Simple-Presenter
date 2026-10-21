#include "videoinputs.h"

#include "sessionlog.h"
#include "videoinputfile.h"

#include <algorithm>
#include <QAudioDevice>
#include <QAudioInput>
#include <QAudioOutput>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtQml/qqmlengine.h>

#include <memory>

namespace {

VideoInputs *s_instance = nullptr;

// The pretend device of the scripted tests (see videoinputs.h)
const QString testDevice = QStringLiteral("test:pattern");

QString quoted(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("(unnamed)") : QStringLiteral("“%1”").arg(name);
}

// Qt's name for a form of frame, as a mode's key has it ("Jpeg", "YUYV")
QString formatName(QVideoFrameFormat::PixelFormat format)
{
    QString name = QVideoFrameFormat::pixelFormatToString(format);
    return name.startsWith(QLatin1String("Format_")) ? name.mid(7) : name;
}

QList<videoinputfile::Mode> modesOf(const QCameraDevice &device)
{
    QList<videoinputfile::Mode> modes;
    for (const QCameraFormat &format : device.videoFormats())
        modes.append({format.resolution().width(), format.resolution().height(), double(format.maxFrameRate()), formatName(format.pixelFormat())});
    return modes;
}

} // namespace

VideoInputs::VideoInputs(QObject *parent)
    : QObject(parent)
    , m_test(!qEnvironmentVariableIsEmpty("SP_TEST_VIDEOINPUT"))
{
    s_instance = this;
}

VideoInputs::~VideoInputs()
{
    const QStringList ids = m_captures.keys();
    for (const QString &id : ids)
        stop(id);
    if (s_instance == this)
        s_instance = nullptr;
}

VideoInputs *VideoInputs::instance()
{
    return s_instance;
}

QString VideoInputs::choiceFile() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/SimplePresenter/VideoInputs.json");
}

QString VideoInputs::thumbnailOf(const QString &id) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Video Inputs/") + id + QStringLiteral(".png");
}

void VideoInputs::readChoices()
{
    m_choices.clear();
    if (!m_remember || m_workspace.isEmpty())
        return;
    QFile file(choiceFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject mine = QJsonDocument::fromJson(file.readAll()).object().value("workspaces").toObject().value(m_workspace).toObject();
    for (auto it = mine.constBegin(); it != mine.constEnd(); ++it) {
        const QJsonObject kept = it.value().toObject();
        Choice choice;
        choice.device = kept.value("device").toString();
        choice.deviceName = kept.value("deviceName").toString();
        choice.mode = kept.value("mode").toString();
        choice.audio = kept.value("audio").toString(QStringLiteral("auto"));
        choice.volume = qBound(0.0, kept.value("volume").toDouble(1), 1.0);
        m_choices.insert(it.key(), choice);
    }
}

// Every workspace's choices are kept, each set written over as it is for the workspace
// that is open.
void VideoInputs::writeChoices() const
{
    if (!m_remember || m_workspace.isEmpty())
        return;
    QJsonObject all;
    {
        QFile file(choiceFile());
        if (file.open(QIODevice::ReadOnly))
            all = QJsonDocument::fromJson(file.readAll()).object();
    }
    QJsonObject workspaces = all.value("workspaces").toObject();
    QJsonObject mine;
    for (auto it = m_choices.constBegin(); it != m_choices.constEnd(); ++it) {
        mine.insert(it.key(), QJsonObject {{"device", it->device}, {"deviceName", it->deviceName}, {"mode", it->mode},
                                           {"audio", it->audio}, {"volume", it->volume}});
    }
    workspaces.insert(m_workspace, mine);
    all.insert("workspaces", workspaces);
    QDir().mkpath(QFileInfo(choiceFile()).absolutePath());
    QSaveFile file(choiceFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(all).toJson());
        file.commit();
    }
}

void VideoInputs::loadDevices(bool announce)
{
    m_devicesKnown = true;
    static QMediaDevices *watching = nullptr;
    if (!watching) {
        watching = new QMediaDevices(this);
        connect(watching, &QMediaDevices::videoInputsChanged, this, [this] { loadDevices(); reload(); });
        connect(watching, &QMediaDevices::audioInputsChanged, this, [this] { loadDevices(); reload(); });
    }
    m_devices.clear();
    for (const QCameraDevice &device : QMediaDevices::videoInputs()) {
        QVariantList modes;
        for (const videoinputfile::Mode &mode : modesOf(device))
            modes.append(QVariantMap {{"key", mode.key()}, {"label", mode.label()}});
        m_devices.append(QVariantMap {{"id", QString::fromUtf8(device.id())}, {"name", device.description()}, {"modes", modes}});
    }
    if (m_test)
        m_devices.append(QVariantMap {{"id", testDevice}, {"name", QStringLiteral("Test Pattern")},
                                      {"modes", QVariantList {QVariantMap {{"key", "320x180@30 RGBA8888"}, {"label", "320 × 180, 30 a second"}}}}});
    m_audioSources.clear();
    for (const QAudioDevice &device : QMediaDevices::audioInputs())
        m_audioSources.append(QVariantMap {{"id", QString::fromUtf8(device.id())}, {"name", device.description()}});
    if (announce)
        emit devicesChanged();
}

QVariantList VideoInputs::devices()
{
    if (!m_devicesKnown)
        loadDevices(false);
    return m_devices;
}

QVariantList VideoInputs::audioSources()
{
    if (!m_devicesKnown)
        loadDevices(false);
    return m_audioSources;
}

void VideoInputs::setAudioDevice(const QString &device)
{
    if (m_audioDevice == device)
        return;
    m_audioDevice = device;
    emit audioDeviceChanged();
    // Sound that is playing goes over to it.
    for (auto it = m_captures.begin(); it != m_captures.end(); ++it) {
        if (it->audioOut) {
            const QString id = it.key();
            QAudioOutput *was = it->audioOut;
            it->audioOut = nullptr;
            if (it->session)
                it->session->setAudioOutput(nullptr);
            was->deleteLater();
            setSound(*it, id);
        }
    }
}

int VideoInputs::cameraIndex(const Choice &choice) const
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (int i = 0; i < cameras.size(); ++i) {
        if (QString::fromUtf8(cameras.at(i).id()) == choice.device)
            return i;
    }
    for (int i = 0; i < cameras.size(); ++i) {
        if (!choice.deviceName.isEmpty() && cameras.at(i).description() == choice.deviceName)
            return i;
    }
    return -1;
}

void VideoInputs::reload()
{
    QString error;
    const QList<videoinputfile::Input> list = m_workspace.isEmpty() ? QList<videoinputfile::Input>() : videoinputfile::inputs(m_workspace, &error);
    if (!error.isEmpty())
        SessionLog::write("input", error);
    m_inputs.clear();
    QStringList ids;
    for (int i = 0; i < list.size(); ++i) {
        const videoinputfile::Input &input = list.at(i);
        ids << input.id;
        // What ProPresenter's file says of its sound is where a computer that has not
        // chosen starts from.
        Choice choice = m_choices.value(input.id);
        if (!m_choices.contains(input.id))
            choice.audio = input.audioType == 1 ? QStringLiteral("none") : QStringLiteral("auto");
        QString deviceName = choice.deviceName;
        bool missing = false;
        QString modeLabel;
        if (!choice.device.isEmpty() && m_devicesKnown) {
            const auto found = std::find_if(m_devices.cbegin(), m_devices.cend(), [&](const QVariant &device) {
                return device.toMap().value("id").toString() == choice.device; });
            if (found != m_devices.cend()) {
                deviceName = found->toMap().value("name").toString();
            } else if (choice.device != testDevice && cameraIndex(choice) >= 0) {
                deviceName = QMediaDevices::videoInputs().at(cameraIndex(choice)).description();
            } else {
                missing = true;
            }
            if (!missing && choice.device != testDevice && cameraIndex(choice) >= 0) {
                const QList<videoinputfile::Mode> modes = modesOf(QMediaDevices::videoInputs().at(cameraIndex(choice)));
                const int chosen = videoinputfile::chooseMode(modes, choice.mode);
                if (chosen >= 0)
                    modeLabel = modes.at(chosen).label();
            }
        }
        QString audioName = choice.audio == QLatin1String("none") ? QStringLiteral("No Audio Source") : QString();
        if (choice.audio != QLatin1String("none") && choice.audio != QLatin1String("auto")) {
            for (const QVariant &source : std::as_const(m_audioSources)) {
                if (source.toMap().value("id").toString() == choice.audio)
                    audioName = source.toMap().value("name").toString();
            }
        }
        const Capture capture = m_captures.value(input.id);
        // (With the file's time in it, so that a thumbnail taken again, at the same path,
        // is read again)
        QString thumbnail;
        if (const QFileInfo file(thumbnailOf(input.id)); file.exists()) {
            QUrl url = QUrl::fromLocalFile(file.absoluteFilePath());
            url.setQuery(QStringLiteral("t=%1").arg(file.lastModified().toMSecsSinceEpoch()));
            thumbnail = url.toString();
        }
        m_inputs.append(QVariantMap {
            {"id", input.id}, {"name", input.name}, {"index", i}, {"color", input.color},
            {"device", choice.device}, {"deviceName", deviceName}, {"missing", missing},
            {"mode", choice.mode}, {"modeLabel", modeLabel}, {"audio", choice.audio}, {"audioName", audioName},
            {"volume", choice.volume}, {"thumbnail", thumbnail}, {"proDevice", input.proDevice},
            {"capturing", capture.sending}, {"problem", capture.problem},
        });
    }
    // An input that is no longer in the workspace is captured no longer.
    const QStringList captured = m_captures.keys();
    for (const QString &id : captured) {
        if (!ids.contains(id)) {
            stop(id);
            Capture gone = m_captures.take(id);
            if (gone.sink)
                gone.sink->deleteLater();
            if (gone.stopping)
                gone.stopping->deleteLater();
        }
    }
    emit changed();
}

QString VideoInputs::open(const QString &workspace)
{
    if (m_workspace != workspace) {
        const QStringList captured = m_captures.keys();
        for (const QString &id : captured)
            stop(id);
    }
    m_workspace = workspace;
    readChoices();
    QString error;
    videoinputfile::inputs(workspace, &error);
    reload();
    return error;
}

QVariantMap VideoInputs::add(const QString &name)
{
    if (m_workspace.isEmpty())
        return {{"id", QString()}, {"error", QStringLiteral("No workspace is open")}};
    // Without a name, "Input" and the first number no input has, as ProPresenter's own
    // first is "Input 1"
    QString made = name;
    for (int number = 1; made.isEmpty(); ++number) {
        const QString candidate = QStringLiteral("Input %1").arg(number);
        if (std::none_of(m_inputs.cbegin(), m_inputs.cend(), [&](const QVariant &input) { return input.toMap().value("name") == candidate; }))
            made = candidate;
    }
    QString id;
    const QString error = videoinputfile::add(m_workspace, made, &id);
    if (error.isEmpty())
        SessionLog::write("input", QStringLiteral("a video input added, %1").arg(quoted(made)));
    reload();
    return {{"id", id}, {"error", error}};
}

QString VideoInputs::rename(const QString &id, const QString &name)
{
    const QString error = videoinputfile::rename(m_workspace, id, name.trimmed());
    reload();
    return error;
}

QString VideoInputs::remove(const QString &id)
{
    const QString error = videoinputfile::remove(m_workspace, id);
    if (error.isEmpty()) {
        SessionLog::write("input", QStringLiteral("a video input removed"));
        m_choices.remove(id);
        writeChoices();
        QFile::remove(thumbnailOf(id));
    }
    reload();
    return error;
}

VideoInputs::Choice &VideoInputs::choiceFor(const QString &id)
{
    if (!m_choices.contains(id)) {
        Choice made;
        made.audio = find(id).value("audio", made.audio).toString();
        m_choices.insert(id, made);
    }
    return m_choices[id];
}

QString VideoInputs::setDevice(const QString &id, const QString &device)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    devices();
    Choice &choice = choiceFor(id);
    choice.device = device;
    choice.deviceName.clear();
    for (const QVariant &entry : std::as_const(m_devices)) {
        if (entry.toMap().value("id").toString() == device)
            choice.deviceName = entry.toMap().value("name").toString();
    }
    // (A mode is a device's own.)
    choice.mode.clear();
    writeChoices();
    SessionLog::write("input", QStringLiteral("%1 is the device %2").arg(quoted(find(id).value("name").toString()),
                                                                          device.isEmpty() ? QStringLiteral("none") : quoted(choice.deviceName)));
    reload();
    start(id);
    return {};
}

QString VideoInputs::setMode(const QString &id, const QString &mode)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    choiceFor(id).mode = mode;
    writeChoices();
    reload();
    start(id);
    return {};
}

QString VideoInputs::setAudio(const QString &id, const QString &source)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    choiceFor(id).audio = source.isEmpty() ? QStringLiteral("auto") : source;
    writeChoices();
    reload();
    auto it = m_captures.find(id);
    if (it != m_captures.end() && it->audioIn) {
        // (A source is set by having it asked for again.)
        if (it->session) {
            it->session->setAudioInput(nullptr);
            it->session->setAudioOutput(nullptr);
        }
        it->audioIn->deleteLater();
        it->audioIn = nullptr;
        if (it->audioOut) {
            it->audioOut->deleteLater();
            it->audioOut = nullptr;
        }
    }
    if (it != m_captures.end())
        setSound(*it, id);
    return {};
}

QString VideoInputs::setVolume(const QString &id, double volume)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    choiceFor(id).volume = qBound(0.0, volume, 1.0);
    writeChoices();
    auto it = m_captures.find(id);
    if (it != m_captures.end() && it->audioOut)
        it->audioOut->setVolume(float(choiceFor(id).volume));
    reload();
    return {};
}

QString VideoInputs::takeThumbnail(const QString &id)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    const auto save = [this, id](const QVideoFrame &frame) {
        QImage picture = QVideoFrame(frame).toImage();
        if (picture.isNull())
            return false;
        QDir().mkpath(QFileInfo(thumbnailOf(id)).absolutePath());
        picture.scaledToWidth(qMin(picture.width(), 480), Qt::SmoothTransformation).save(thumbnailOf(id));
        SessionLog::write("input", QStringLiteral("a thumbnail taken of %1").arg(quoted(find(id).value("name").toString())));
        reload();
        return true;
    };
    const Capture now = m_captures.value(id);
    if (now.sending && now.sink && save(now.sink->videoFrame()))
        return {};
    // Captured for a moment, for its first frame
    hold(id, false);
    QVideoSink *sink = sinkOf(id);
    auto *waiting = new QTimer(this);
    waiting->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    const auto finish = [this, id, waiting, done] {
        if (*done)
            return;
        *done = true;
        waiting->deleteLater();
        letGo(id, false);
    };
    connect(sink, &QVideoSink::videoFrameChanged, waiting, [save, finish, done](const QVideoFrame &frame) {
        if (!*done && frame.isValid() && save(frame))
            finish();
    });
    connect(waiting, &QTimer::timeout, this, [this, id, finish] {
        SessionLog::write("input", QStringLiteral("no thumbnail could be taken of %1: it sent no picture").arg(quoted(find(id).value("name").toString())));
        finish();
    });
    waiting->start(5000);
    return {};
}

QString VideoInputs::setThumbnail(const QString &id, const QUrl &file)
{
    if (find(id).isEmpty())
        return QStringLiteral("There is no such video input");
    const QImage picture(file.toLocalFile());
    if (picture.isNull())
        return QStringLiteral("That is not a picture this app can read");
    QDir().mkpath(QFileInfo(thumbnailOf(id)).absolutePath());
    if (!picture.scaledToWidth(qMin(picture.width(), 480), Qt::SmoothTransformation).save(thumbnailOf(id)))
        return QStringLiteral("The thumbnail could not be kept");
    reload();
    return {};
}

QString VideoInputs::clearThumbnail(const QString &id)
{
    QFile::remove(thumbnailOf(id));
    reload();
    return {};
}

QVariantMap VideoInputs::at(int index) const
{
    return index >= 0 && index < m_inputs.size() ? m_inputs.at(index).toMap() : QVariantMap();
}

QVariantMap VideoInputs::find(const QString &id) const
{
    for (const QVariant &input : m_inputs) {
        if (input.toMap().value("id").toString() == id)
            return input.toMap();
    }
    return {};
}

void VideoInputs::setTestColor(const QString &color)
{
    m_testColor = color;
}

void VideoInputs::hold(const QString &id, bool sound)
{
    if (id.isEmpty())
        return;
    Capture &capture = m_captures[id];
    if (!capture.sink) {
        capture.sink = new QVideoSink(this);
        connect(capture.sink, &QVideoSink::videoFrameChanged, this, [this, id] {
            auto it = m_captures.find(id);
            if (it == m_captures.end() || it->sending)
                return;
            it->sending = true;
            SessionLog::write("input", QStringLiteral("%1 is sending pictures").arg(quoted(find(id).value("name").toString())));
            reload();
            emit capturingChanged(id);
        });
    }
    if (capture.stopping)
        capture.stopping->stop();
    const bool wasHeld = capture.holds > 0 || capture.camera || capture.pattern;
    ++capture.holds;
    if (sound)
        ++capture.sounds;
    if (!wasHeld)
        start(id);
    else if (sound)
        setSound(m_captures[id], id);
}

void VideoInputs::letGo(const QString &id, bool sound)
{
    auto it = m_captures.find(id);
    if (it == m_captures.end() || it->holds <= 0)
        return;
    --it->holds;
    if (sound && it->sounds > 0) {
        --it->sounds;
        setSound(*it, id);
    }
    if (it->holds > 0)
        return;
    if (!it->stopping) {
        it->stopping = new QTimer(this);
        it->stopping->setSingleShot(true);
        connect(it->stopping, &QTimer::timeout, this, [this, id] {
            auto now = m_captures.find(id);
            if (now != m_captures.end() && now->holds == 0)
                stop(id);
        });
    }
    it->stopping->start(1500);
}

QVideoSink *VideoInputs::sinkOf(const QString &id) const
{
    return m_captures.value(id).sink;
}

void VideoInputs::start(const QString &id)
{
    auto it = m_captures.find(id);
    if (it == m_captures.end() || it->holds <= 0)
        return;
    // What was capturing it stops first: a device, a mode or a source has changed.
    const int holds = it->holds;
    const int sounds = it->sounds;
    stop(id);
    it = m_captures.find(id);
    it->holds = holds;
    it->sounds = sounds;
    const QVariantMap input = find(id);
    const Choice choice = m_choices.value(id);
    const QString name = quoted(input.value("name").toString());
    if (choice.device.isEmpty()) {
        it->problem = QStringLiteral("It has no device on this computer");
        SessionLog::write("input", name + QStringLiteral(" has no device on this computer, and shows nothing"));
        return;
    }
    it->problem.clear();
    if (choice.device == testDevice && m_test) {
        it->pattern = new QTimer(this);
        connect(it->pattern, &QTimer::timeout, this, [this, id] {
            auto now = m_captures.find(id);
            if (now != m_captures.end())
                pushPattern(*now);
        });
        it->pattern->start(33);
        SessionLog::write("input", name + QStringLiteral(" capturing the test pattern"));
        return;
    }
    const int index = cameraIndex(choice);
    if (index < 0) {
        it->problem = QStringLiteral("Its device, %1, is not on this computer now").arg(quoted(choice.deviceName));
        SessionLog::write("input", name + QStringLiteral(": its device, %1, is not on this computer now").arg(quoted(choice.deviceName)));
        return;
    }
    const QCameraDevice device = QMediaDevices::videoInputs().at(index);
    it->camera = new QCamera(device, this);
    const QList<videoinputfile::Mode> modes = modesOf(device);
    const int mode = videoinputfile::chooseMode(modes, choice.mode);
    if (mode >= 0)
        it->camera->setCameraFormat(device.videoFormats().at(mode));
    it->session = new QMediaCaptureSession(this);
    it->session->setCamera(it->camera);
    it->session->setVideoSink(it->sink);
    connect(it->camera, &QCamera::errorOccurred, this, [this, id, name](QCamera::Error error, const QString &said) {
        if (error == QCamera::NoError)
            return;
        auto now = m_captures.find(id);
        if (now != m_captures.end())
            now->problem = said;
        SessionLog::write("input", name + QStringLiteral(": the device said: %1").arg(said));
        reload();
    });
    it->camera->start();
    SessionLog::write("input", name + QStringLiteral(" capturing %1, %2").arg(quoted(device.description()),
                                                                                mode >= 0 ? modes.at(mode).label() : QStringLiteral("as it sends")));
    setSound(*it, id);
}

void VideoInputs::stop(const QString &id)
{
    auto it = m_captures.find(id);
    if (it == m_captures.end())
        return;
    const bool was = it->camera || it->pattern;
    if (it->pattern) {
        it->pattern->stop();
        it->pattern->deleteLater();
        it->pattern = nullptr;
    }
    if (it->session) {
        it->session->setAudioInput(nullptr);
        it->session->setAudioOutput(nullptr);
        it->session->setVideoSink(nullptr);
        it->session->deleteLater();
        it->session = nullptr;
    }
    if (it->camera) {
        it->camera->stop();
        it->camera->deleteLater();
        it->camera = nullptr;
    }
    if (it->audioIn) {
        it->audioIn->deleteLater();
        it->audioIn = nullptr;
    }
    if (it->audioOut) {
        it->audioOut->deleteLater();
        it->audioOut = nullptr;
    }
    const bool sent = it->sending;
    it->sending = false;
    if (was)
        SessionLog::write("input", quoted(find(id).value("name").toString()) + QStringLiteral(" captured no longer"));
    if (sent) {
        reload();
        emit capturingChanged(id);
    }
}

// Its sound plays while something that holds it wants it, from the source its choices
// say (see videoinputs.h).
void VideoInputs::setSound(Capture &capture, const QString &id)
{
    if (!capture.session)
        return;
    const Choice choice = m_choices.contains(id) ? m_choices.value(id) : Choice();
    const QString audio = m_choices.contains(id) ? choice.audio
                                                 : find(id).value("audio").toString();
    QAudioDevice source;
    if (capture.sounds > 0 && audio != QLatin1String("none")) {
        const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
        if (audio == QLatin1String("auto") || audio.isEmpty()) {
            QStringList names;
            for (const QAudioDevice &input : inputs)
                names << input.description();
            const int own = capture.camera ? videoinputfile::ownAudio(capture.camera->cameraDevice().description(), names) : -1;
            if (own >= 0)
                source = inputs.at(own);
        } else {
            for (const QAudioDevice &input : inputs) {
                if (QString::fromUtf8(input.id()) == audio)
                    source = input;
            }
        }
    }
    if (source.isNull()) {
        if (capture.audioIn) {
            capture.session->setAudioInput(nullptr);
            capture.session->setAudioOutput(nullptr);
            capture.audioIn->deleteLater();
            capture.audioIn = nullptr;
            if (capture.audioOut) {
                capture.audioOut->deleteLater();
                capture.audioOut = nullptr;
            }
            SessionLog::write("input", quoted(find(id).value("name").toString()) + QStringLiteral(": its sound stopped"));
        }
        return;
    }
    if (capture.audioIn && capture.audioIn->device() == source)
        return;
    if (capture.audioIn)
        capture.audioIn->deleteLater();
    capture.audioIn = new QAudioInput(source, this);
    if (!capture.audioOut) {
        QAudioDevice output = QMediaDevices::defaultAudioOutput();
        for (const QAudioDevice &candidate : QMediaDevices::audioOutputs()) {
            if (QString::fromUtf8(candidate.id()) == m_audioDevice)
                output = candidate;
        }
        capture.audioOut = new QAudioOutput(output, this);
    }
    capture.audioOut->setVolume(float(choice.volume));
    capture.session->setAudioInput(capture.audioIn);
    capture.session->setAudioOutput(capture.audioOut);
    SessionLog::write("input", quoted(find(id).value("name").toString()) + QStringLiteral(": its sound from %1").arg(quoted(source.description())));
}

void VideoInputs::pushPattern(Capture &capture)
{
    if (!capture.sink)
        return;
    QImage picture(320, 180, QImage::Format_RGBA8888);
    picture.fill(QColor(m_testColor));
    capture.sink->setVideoFrame(QVideoFrame(picture));
}

// ---- VideoInputFeed

VideoInputFeed::VideoInputFeed(QObject *parent)
    : QObject(parent)
{
}

void VideoInputFeed::componentComplete()
{
    m_complete = true;
    update();
}

VideoInputFeed::~VideoInputFeed()
{
    if (!m_held.isEmpty() && VideoInputs::instance())
        VideoInputs::instance()->letGo(m_held, m_heldSound);
}

void VideoInputFeed::setIndex(int index)
{
    if (m_index == index)
        return;
    m_index = index;
    emit inputChanged();
    update();
}

void VideoInputFeed::setInputId(const QString &id)
{
    if (m_inputId == id)
        return;
    m_inputId = id;
    emit inputChanged();
    update();
}

void VideoInputFeed::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
    update();
}

void VideoInputFeed::setSound(bool sound)
{
    if (m_sound == sound)
        return;
    m_sound = sound;
    emit soundChanged();
    update();
}

QString VideoInputFeed::wanted() const
{
    VideoInputs *inputs = VideoInputs::instance();
    if (!inputs || !m_active)
        return {};
    if (!m_inputId.isEmpty())
        return inputs->find(m_inputId).isEmpty() ? QString() : m_inputId;
    return inputs->at(m_index).value("id").toString();
}

void VideoInputFeed::update()
{
    if (!m_complete)
        return;
    VideoInputs *inputs = VideoInputs::instance();
    // (A feed can be made before anything has asked for the inputs by name.)
    if (!inputs) {
        if (QQmlEngine *engine = qmlEngine(this))
            inputs = engine->singletonInstance<VideoInputs *>("SimplePresenterApp", "VideoInputs");
    }
    if (!inputs)
        return;
    // The list changing (an input added, or one removed, which moves those after it up
    // a place) can change which input a place is; and the input beginning to send
    // pictures is said.
    if (!m_following) {
        m_following = true;
        connect(inputs, &VideoInputs::changed, this, &VideoInputFeed::update);
        connect(inputs, &VideoInputs::capturingChanged, this, [this](const QString &id) {
            if (id == m_held)
                emit sinkChanged();
        });
    }
    const QString now = wanted();
    if (now == m_held && m_sound == m_heldSound)
        return;
    if (!m_held.isEmpty())
        inputs->letGo(m_held, m_heldSound);
    m_held = now;
    m_heldSound = m_sound;
    if (!m_held.isEmpty())
        inputs->hold(m_held, m_heldSound);
    emit sinkChanged();
}

bool VideoInputs::sendingOf(const QString &id) const
{
    const auto it = m_captures.constFind(id);
    return it != m_captures.cend() && it->sending;
}

QObject *VideoInputFeed::sink() const
{
    VideoInputs *inputs = VideoInputs::instance();
    return inputs && !m_held.isEmpty() ? inputs->sinkOf(m_held) : nullptr;
}

bool VideoInputFeed::sending() const
{
    VideoInputs *inputs = VideoInputs::instance();
    return inputs && !m_held.isEmpty() && inputs->sendingOf(m_held);
}
