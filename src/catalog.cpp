#include "catalog.h"

#include "prodocument.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
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

void addFolderTree(const QString &directory, const QString &name, int depth, QVariantList *folders)
{
    folders->append(QVariantMap {{"name", name}, {"path", directory}, {"depth", depth}});
    for (const QFileInfo &child : entries(directory, {}, QDir::Dirs))
        addFolderTree(child.absoluteFilePath(), child.fileName(), depth + 1, folders);
}

} // namespace

Catalog::Catalog(const QString &root, QObject *parent)
    : QObject(parent)
    , m_root(QDir(root).absolutePath())
    , m_librariesDirectory(QDir(root).absoluteFilePath("Libraries"))
    , m_mediaDirectory(QDir(root).absoluteFilePath("Media"))
{
    QDir().mkpath(m_librariesDirectory);
    QDir().mkpath(m_mediaDirectory);
    QDir().mkpath(QDir(m_root).absoluteFilePath("Playlists"));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Catalog::rescan);
    rescan();
}

void Catalog::rescan()
{
    m_libraries.clear();
    for (const QFileInfo &library : entries(m_librariesDirectory, {}, QDir::Dirs))
        m_libraries.append(QVariantMap {{"name", library.fileName()}, {"path", library.absoluteFilePath()}});

    m_mediaFolders.clear();
    addFolderTree(m_mediaDirectory, QStringLiteral("Media"), 0, &m_mediaFolders);

    QString error;
    m_playlists = PlaylistFile::load(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));

    // Watch every folder whose contents are shown, so any change triggers a rescan.
    QStringList watched {m_librariesDirectory, QDir(m_root).absoluteFilePath("Playlists")};
    for (const QVariant &library : std::as_const(m_libraries))
        watched << library.toMap().value("path").toString();
    for (const QVariant &folder : std::as_const(m_mediaFolders))
        watched << folder.toMap().value("path").toString();
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

QVariantList Catalog::mediaIn(const QString &folder) const
{
    QVariantList media;
    if (folder.isEmpty())
        return media;
    for (const QFileInfo &file : entries(folder, videoPatterns + imagePatterns, QDir::Files)) {
        media.append(QVariantMap {
            {"name", file.fileName()},
            {"path", file.absoluteFilePath()},
            {"source", QUrl::fromLocalFile(file.absoluteFilePath())},
            {"video", QDir::match(videoPatterns, file.fileName())},
        });
    }
    return media;
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
    const ProDocument document = ProDocument::load(path, m_mediaDirectory, arrangement, &error);
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

QVariantMap Catalog::createPlaylist(const QString &name)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createPlaylist(m_root, name, &id));
    return {{"id", id}, {"error", error}};
}

QString Catalog::renamePlaylist(const QString &id, const QString &name)
{
    return afterChange(PlaylistFile::renameNode(m_root, id, name));
}

QString Catalog::deletePlaylist(const QString &id)
{
    return afterChange(PlaylistFile::deletePlaylist(m_root, id));
}

QString Catalog::addToPlaylist(const QString &playlist, const QString &file)
{
    return afterChange(PlaylistFile::addPresentation(m_root, playlist, file));
}

QString Catalog::removePlaylistItem(const QString &item)
{
    return afterChange(PlaylistFile::removeItem(m_root, item));
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
    return ProDocument::setCueMedia(path, slideId, mediaPath, QDir::match(videoPatterns, QFileInfo(mediaPath).fileName()));
}

QString Catalog::removeSlideMedia(const QString &path, const QString &slideId)
{
    return ProDocument::removeCueMedia(path, slideId);
}
