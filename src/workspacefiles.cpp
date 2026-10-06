#include "workspacefiles.h"

#include "action.pb.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace workspace {

namespace {

const QStringList videoSuffixes = {"mp4", "mov", "m4v", "mkv", "webm", "avi"};
const QStringList imageSuffixes = {"jpg", "jpeg", "png", "webp", "bmp", "gif"};

} // namespace

bool isVideo(const QString &file)
{
    return videoSuffixes.contains(QFileInfo(file).suffix().toLower());
}

bool isMedia(const QString &file)
{
    return isVideo(file) || imageSuffixes.contains(QFileInfo(file).suffix().toLower());
}

QStringList mediaPatterns()
{
    QStringList patterns;
    for (const QString &suffix : videoSuffixes + imageSuffixes)
        patterns << QStringLiteral("*.") + suffix;
    return patterns;
}

QString mediaDialogFilter()
{
    return QStringLiteral("Images and videos (%1)").arg(mediaPatterns().join(u' '));
}

QList<QFileInfo> sortedEntries(const QString &directory, const QStringList &patterns, QDir::Filters filters)
{
    QList<QFileInfo> found = QDir(directory).entryInfoList(patterns, filters | QDir::NoDotAndDotDot);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(found.begin(), found.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });
    return found;
}

std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

FileFinder::FileFinder(const QString &workspaceFolder, const QString &searchFolder)
    : m_workspace(workspaceFolder)
    , m_searchDirectory(workspaceFolder.isEmpty() ? QString() : QDir(workspaceFolder).absoluteFilePath(searchFolder))
{
}

QString FileFinder::find(const rv::data::URL &reference)
{
    const QString relativePath = reference.has_local()
                                 && reference.local().root() == rv::data::URL::LocalRelativePath::ROOT_SHOW
                                     ? QString::fromStdString(reference.local().path()) : QString();
    if (!relativePath.isEmpty() && !m_workspace.isEmpty()) {
        const QString relative = QDir(m_workspace).absoluteFilePath(relativePath);
        if (QFile::exists(relative))
            return relative;
    }
    const QString recorded = QUrl(QString::fromStdString(reference.absolute_string())).toLocalFile();
    if (!recorded.isEmpty() && QFile::exists(recorded))
        return recorded;

    // By name: the name it was recorded under, or the one in its relative path.
    QStringList names;
    if (!recorded.isEmpty())
        names << QFileInfo(recorded).fileName();
    if (reference.has_local() && !reference.local().path().empty())
        names << QFileInfo(QString::fromStdString(reference.local().path())).fileName();
    if (m_searchDirectory.isEmpty())
        return {};
    for (const QString &name : std::as_const(names)) {
        if (name.isEmpty())
            continue;
        if (!m_listed) {
            m_listed = true;
            QDirIterator it(m_searchDirectory, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                const QString key = it.fileName().toLower();
                // The first of a name wins, as it would if each were looked for in turn.
                if (!m_byName.contains(key))
                    m_byName.insert(key, path);
            }
        }
        const auto found = m_byName.constFind(name.toLower());
        if (found != m_byName.constEnd())
            return *found;
    }
    return {};
}

void recordFile(rv::data::URL *reference, const QString &file, const QString &workspaceFolder)
{
    reference->Clear();
    reference->set_absolute_string(QUrl::fromLocalFile(file).toString(QUrl::FullyEncoded).toStdString());
    if (workspaceFolder.isEmpty())
        return;
    const QString relative = QDir(workspaceFolder).relativeFilePath(file);
    if (!relative.startsWith(QLatin1String(".."))) {
        reference->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_SHOW);
        reference->mutable_local()->set_path(relative.toStdString());
    }
}

QString fileNameOf(const rv::data::URL &reference)
{
    const QString name = QUrl(QString::fromStdString(reference.absolute_string())).fileName();
    return name.isEmpty() ? QFileInfo(QString::fromStdString(reference.local().path())).fileName() : name;
}

rv::data::Media mediaElement(const QString &file, const QString &workspaceFolder)
{
    rv::data::Media element;
    element.mutable_uuid()->set_string(newUuid());
    recordFile(element.mutable_url(), file, workspaceFolder);
    element.mutable_metadata()->set_format(QFileInfo(file).suffix().toUpper().toStdString());
    if (isVideo(file)) {
        rv::data::Media::VideoTypeProperties *properties = element.mutable_video();
        properties->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        properties->mutable_audio()->set_volume(1);
        properties->mutable_transport()->set_play_rate(1);
        properties->mutable_transport()->set_playback_behavior(rv::data::Media::TransportProperties::PLAYBACK_BEHAVIOR_LOOP);
        recordFile(properties->mutable_file()->mutable_local_url(), file, workspaceFolder);
    } else {
        rv::data::Media::ImageTypeProperties *properties = element.mutable_image();
        const QSize size = QImageReader(file).size();
        if (size.isValid()) {
            properties->mutable_drawing()->mutable_natural_size()->set_width(size.width());
            properties->mutable_drawing()->mutable_natural_size()->set_height(size.height());
        }
        properties->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        recordFile(properties->mutable_file()->mutable_local_url(), file, workspaceFolder);
    }
    return element;
}

QString readMessage(const QString &path, google::protobuf::MessageLite *message, const QString &what)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read %1: %2").arg(what, file.errorString());
    const QByteArray data = file.readAll();
    if (!message->ParseFromArray(data.constData(), int(data.size())))
        return QStringLiteral("The file of %1 is not one this app can read").arg(what);
    return {};
}

QString writeMessage(const QString &path, const google::protobuf::MessageLite &message, const QString &what)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("Cannot make the folder for %1").arg(what);
    std::string bytes;
    if (!message.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode %1").arg(what);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())
        || !file.commit())
        return QStringLiteral("Cannot write %1: %2").arg(what, file.errorString());
    return {};
}

void MediaBehaviour::describe(QVariantMap *media) const
{
    media->insert("foreground", foreground);
    media->insert("loops", loops);
    media->insert("retriggers", retriggers);
}

MediaBehaviour mediaBehaviour(const rv::data::Action &action)
{
    using Transport = rv::data::Media::TransportProperties;
    MediaBehaviour behaviour;
    behaviour.foreground = action.media().layer_type() == rv::data::Action::LAYER_TYPE_FOREGROUND;
    behaviour.retriggers = action.media().always_retrigger();
    if (action.media().element().has_video()) {
        const Transport &transport = action.media().element().video().transport();
        // Looping a number of times, or for a length of time, is looping here.
        behaviour.loops = transport.playback_behavior() != Transport::PLAYBACK_BEHAVIOR_STOP;
        behaviour.retriggers = behaviour.retriggers || transport.retrigger() == Transport::RETRIGGER_SETTING_ALWAYS;
    }
    return behaviour;
}

void setMediaForeground(rv::data::Action *action, bool foreground)
{
    using Transport = rv::data::Media::TransportProperties;
    action->mutable_media()->set_layer_type(foreground ? rv::data::Action::LAYER_TYPE_FOREGROUND
                                                       : rv::data::Action::LAYER_TYPE_BACKGROUND);
    if (action->media().element().has_video()) {
        action->mutable_media()->mutable_element()->mutable_video()->mutable_transport()->set_playback_behavior(
            foreground ? Transport::PLAYBACK_BEHAVIOR_STOP : Transport::PLAYBACK_BEHAVIOR_LOOP);
    }
}

} // namespace workspace
