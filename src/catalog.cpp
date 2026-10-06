#include "catalog.h"

#include "playlistimport.h"
#include "prodocument.h"
#include "thumbnailprovider.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QUrl>

#include <algorithm>

namespace {

const QStringList videoPatterns = {"*.mp4", "*.mov", "*.m4v", "*.mkv", "*.webm", "*.avi"};
const QStringList imagePatterns = {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp", "*.gif"};

// Entries of one directory, sorted the way a file manager would ("Song 2" before "Song 10").
QList<QFileInfo> entries(const QString &directory, const QStringList &patterns, QDir::Filters filters)
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

} // namespace

Catalog::Catalog(const QString &workspace, QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Catalog::rescan);
    openWorkspace(workspace);
}

QString Catalog::workspaceName() const
{
    return QFileInfo(m_root).fileName();
}

void Catalog::openWorkspace(const QString &path)
{
    m_root = QDir(path).absolutePath();
    m_librariesDirectory = QDir(m_root).absoluteFilePath("Libraries");
    m_mediaDirectory = QDir(m_root).absoluteFilePath("Media");
    QDir().mkpath(m_librariesDirectory);
    QDir().mkpath(m_mediaDirectory);
    QDir().mkpath(QDir(m_root).absoluteFilePath("Playlists"));
    // The media bin shows media playlists, as ProPresenter's does. A workspace that has
    // never had any gets a set that mirrors the folders under Media, once.
    const QString seedError = PlaylistFile::seedMediaFromFolders(m_root);
    if (!seedError.isEmpty())
        qWarning("%s", qPrintable(seedError));
    emit workspaceChanged();
    rescan();
}

void Catalog::rescan()
{
    // The workspaces are the folders beside the open one.
    const QString workspacesDirectory = QFileInfo(m_root).absolutePath();
    m_workspaces.clear();
    for (const QFileInfo &workspace : entries(workspacesDirectory, {}, QDir::Dirs))
        m_workspaces.append(QVariantMap {{"name", workspace.fileName()}, {"path", workspace.absoluteFilePath()}});

    m_libraries.clear();
    for (const QFileInfo &library : entries(m_librariesDirectory, {}, QDir::Dirs))
        m_libraries.append(QVariantMap {
            {"name", library.fileName()},
            {"path", library.absoluteFilePath()},
            {"icon", QStringLiteral("library")},
        });

    QString error;
    m_playlists = PlaylistFile::load(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));
    error.clear();
    m_mediaPlaylists = PlaylistFile::loadMedia(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));

    // Watch every folder whose contents are shown, so any change triggers a rescan.
    QStringList watched {workspacesDirectory, m_librariesDirectory, QDir(m_root).absoluteFilePath("Playlists")};
    for (const QVariant &library : std::as_const(m_libraries))
        watched << library.toMap().value("path").toString();
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_watcher.addPaths(watched);

    ++m_revision;
    emit changed();
}

QVariantList Catalog::documentsIn(const QString &library) const
{
    QVariantList documents;
    if (library.isEmpty())
        return documents;
    for (const QFileInfo &file : entries(library, {QStringLiteral("*.pro")}, QDir::Files)) {
        QStringList arrangements;
        QString arrangement;
        ProDocument::arrangementsOf(file.absoluteFilePath(), &arrangements, &arrangement);
        documents.append(QVariantMap {
            {"path", file.absoluteFilePath()},
            {"name", file.completeBaseName()},
            {"kind", QStringLiteral("presentation")},
            {"icon", QStringLiteral("presentation")},
            {"file", file.absoluteFilePath()},
            {"missing", false},
            {"playlistItem", false},
            {"arrangement", arrangement},
            {"arrangements", arrangements},
            {"detail", arrangement.isEmpty() ? QString() : u'[' + arrangement + u']'},
            {"color", QString()},
        });
    }
    return documents;
}

QVariantList Catalog::mediaIn(const QString &playlist) const
{
    return m_mediaPlaylists.items.value(playlist);
}

QVariantList Catalog::playlistItems(const QString &playlist) const
{
    return m_playlists.items.value(playlist);
}

QVariantMap Catalog::open(const QString &path) const
{
    return open(path, std::nullopt);
}

QVariantMap Catalog::openArranged(const QString &path, const QString &arrangement) const
{
    return open(path, arrangement);
}

QVariantMap Catalog::open(const QString &path, const std::optional<QString> &arrangement) const
{
    QString error;
    const ProDocument document = ProDocument::load(path, m_root, arrangement, &error);
    return {
        {"name", QFileInfo(path).completeBaseName()},
        {"path", path},
        {"slides", document.slides},
        {"arrangements", document.arrangements},
        {"arrangement", document.arrangement},
        {"error", error},
    };
}

QString Catalog::setPlaylistItemArrangement(const QString &item, const QString &file, const QString &arrangement)
{
    const QString id = arrangement.isEmpty() ? QString() : ProDocument::arrangementId(file, arrangement);
    if (!arrangement.isEmpty() && id.isEmpty())
        return QStringLiteral("%1 has no arrangement named %2").arg(QFileInfo(file).fileName(), arrangement);
    return afterChange(PlaylistFile::setItemArrangement(m_root, item, id, arrangement));
}

// Shows a successful change at once, without waiting for the folder watcher.
QString Catalog::afterChange(const QString &error)
{
    if (error.isEmpty())
        rescan();
    return error;
}

QVariantMap Catalog::createPlaylist(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Presentations, name, parent, false, &id));
    return {{"id", id}, {"error", error}};
}

QVariantMap Catalog::createPlaylistFolder(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Presentations, name, parent, true, &id));
    return {{"id", id}, {"error", error}};
}

QString Catalog::renamePlaylist(const QString &id, const QString &name)
{
    return afterChange(PlaylistFile::renameNode(m_root, PlaylistFile::Presentations, id, name));
}

QString Catalog::removePlaylist(const QString &id)
{
    return afterChange(PlaylistFile::removeNode(m_root, PlaylistFile::Presentations, id));
}

QString Catalog::addToPlaylist(const QString &playlist, const QString &file)
{
    return afterChange(PlaylistFile::addPresentation(m_root, playlist, file));
}

QString Catalog::removePlaylistItem(const QString &item)
{
    return afterChange(PlaylistFile::removeItem(m_root, PlaylistFile::Presentations, item));
}

QString Catalog::setArrangement(const QString &path, const QString &arrangement)
{
    const QString error = ProDocument::setArrangement(path, arrangement);
    if (error.isEmpty())
        rescan();
    return error;
}

QString Catalog::setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath)
{
    return ProDocument::setCueMedia(path, slideId, mediaPath,
                                    QDir::match(videoPatterns, QFileInfo(mediaPath).fileName()), m_root);
}

QString Catalog::removeSlideMedia(const QString &path, const QString &slideId)
{
    return ProDocument::removeCueMedia(path, slideId);
}

QString Catalog::movePlaylistItem(const QString &item, const QString &target, bool after)
{
    return afterChange(PlaylistFile::moveItem(m_root, PlaylistFile::Presentations, item, target, after));
}

QString Catalog::movePlaylistNode(const QString &id, const QString &target, const QString &where)
{
    return afterChange(PlaylistFile::moveNode(m_root, PlaylistFile::Presentations, id, target, where));
}

void Catalog::importPlaylist(const QUrl &archive, const QString &library, const QString &parent)
{
    if (m_importing)
        return;
    m_importing = true;
    emit importingChanged();

    // With no library to put the presentations in, they get one of their own.
    const QString destination = library.isEmpty() ? QDir(m_librariesDirectory).absoluteFilePath("Imported") : library;
    const QString path = archive.toLocalFile();
    const QString root = m_root;

    // Copying can be hundreds of megabytes of media, so it happens off the main thread
    // and the output keeps running. The playlists file is only touched back here.
    QThread *worker = QThread::create([this, path, root, destination, parent] {
        QString error;
        const PlaylistImport unpacked = PlaylistImport::unpack(path, root, destination, &error);
        QMetaObject::invokeMethod(this, [this, unpacked, error, root, destination, parent] {
            QString failure = error;
            QString playlist;
            if (failure.isEmpty())
                failure = PlaylistFile::importPlaylists(root, unpacked.playlistData, destination, parent, &playlist);
            m_importing = false;
            emit importingChanged();
            rescan();
            emit importFinished(failure, unpacked.summary(), playlist);
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

QVariantMap Catalog::createMediaPlaylist(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Media, name, parent, false, &id));
    return {{"id", id}, {"error", error}};
}

QVariantMap Catalog::createMediaFolder(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Media, name, parent, true, &id));
    return {{"id", id}, {"error", error}};
}

QString Catalog::renameMediaPlaylist(const QString &id, const QString &name)
{
    return afterChange(PlaylistFile::renameNode(m_root, PlaylistFile::Media, id, name));
}

QString Catalog::removeMediaPlaylist(const QString &id)
{
    return afterChange(PlaylistFile::removeNode(m_root, PlaylistFile::Media, id));
}

QString Catalog::moveMediaPlaylist(const QString &id, const QString &target, const QString &where)
{
    return afterChange(PlaylistFile::moveNode(m_root, PlaylistFile::Media, id, target, where));
}

QString Catalog::addMedia(const QString &playlist, const QList<QUrl> &files)
{
    QStringList paths;
    for (const QUrl &file : files) {
        const QString path = file.toLocalFile();
        if (QDir::match(videoPatterns + imagePatterns, QFileInfo(path).fileName()))
            paths << path;
    }
    if (paths.isEmpty())
        return QStringLiteral("None of those files are images or videos this can show");
    return afterChange(PlaylistFile::addMedia(m_root, playlist, paths));
}

QString Catalog::removeMediaItem(const QString &item)
{
    return afterChange(PlaylistFile::removeItem(m_root, PlaylistFile::Media, item));
}

QString Catalog::moveMediaItem(const QString &item, const QString &target, bool after)
{
    return afterChange(PlaylistFile::moveItem(m_root, PlaylistFile::Media, item, target, after));
}

void Catalog::rebuildThumbnails(const QStringList &paths)
{
    ThumbnailProvider::discard(paths);
    ++m_thumbnailRevision;
    emit thumbnailsDiscarded();
}
