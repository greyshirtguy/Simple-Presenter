#include "catalog.h"

#include "audioprobe.h"
#include "mediaeffects.h"
#include "mediasettings.h"
#include "songimport.h"

#include "playlistimport.h"
#include "prodocument.h"
#include "sessionlog.h"
#include "thumbnailprovider.h"
#include "workspacefiles.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QUrl>

using workspace::sortedEntries;

Catalog::Catalog(const QString &workspace, QObject *parent)
    : QObject(parent)
{
    // Changes on disk come in bursts: one file saved is several events, and an import
    // or a copy in a file manager is hundreds. They are waited out, and the folders
    // read once when they stop.
    m_settle.setSingleShot(true);
    m_settle.setInterval(150);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_settle, qOverload<>(&QTimer::start));
    connect(&m_settle, &QTimer::timeout, this, &Catalog::rescan);
    openWorkspace(workspace);
}

QString Catalog::mediaDialogFilter() const
{
    return workspace::mediaDialogFilter();
}

QString Catalog::audioDialogFilter() const
{
    return workspace::audioDialogFilter();
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
    m_documents.clear();
    // Whatever the next scan finds counts as a change, even an identical workspace.
    m_scanned = false;
    emit workspaceChanged();
    rescan();
}

// Reads the workspace from disk again. The views are only told if what they show has
// come out different: most of what happens on disk (a presentation saved with the same
// arrangements, a thumbnail written beside the files) changes nothing they show, and
// telling them anyway would have every list rebuilt for nothing.
void Catalog::rescan()
{
    // Files may have come or gone: what is looked for by name is looked for afresh.
    workspace::setOpenWorkspace(m_root);
    m_settle.stop();

    // The workspaces are the folders beside the open one.
    const QString workspacesDirectory = QFileInfo(m_root).absolutePath();
    QVariantList workspaces;
    for (const QFileInfo &workspace : sortedEntries(workspacesDirectory, {}, QDir::Dirs))
        workspaces.append(QVariantMap {{"name", workspace.fileName()}, {"path", workspace.absoluteFilePath()}});

    QVariantList libraries;
    QStringList libraryPaths;
    for (const QFileInfo &library : sortedEntries(m_librariesDirectory, {}, QDir::Dirs)) {
        libraries.append(QVariantMap {
            {"name", library.fileName()},
            {"path", library.absoluteFilePath()},
            {"icon", QStringLiteral("library")},
        });
        libraryPaths << library.absoluteFilePath();
    }

    QString error;
    const PlaylistFile playlists = PlaylistFile::load(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));
    error.clear();
    const PlaylistFile mediaPlaylists = PlaylistFile::loadMedia(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));
    error.clear();
    const PlaylistFile audioPlaylists = PlaylistFile::loadAudio(m_root, &error);
    if (!error.isEmpty())
        qWarning("%s", qPrintable(error));

    // The presentations of the libraries that have been looked into, read again.
    QHash<QString, QVariantList> documents;
    for (auto it = m_documents.constBegin(); it != m_documents.constEnd(); ++it) {
        if (libraryPaths.contains(it.key()))
            documents.insert(it.key(), listDocuments(it.key()));
    }

    // Watch every folder whose contents are shown, so any change triggers a rescan.
    const QStringList watched = QStringList {workspacesDirectory, m_librariesDirectory,
                                             QDir(m_root).absoluteFilePath("Playlists")} + libraryPaths;
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_watcher.addPaths(watched);

    const bool same = m_scanned && workspaces == m_workspaces && libraries == m_libraries
                      && playlists.nodes == m_playlists.nodes && playlists.items == m_playlists.items
                      && mediaPlaylists.nodes == m_mediaPlaylists.nodes && mediaPlaylists.items == m_mediaPlaylists.items
                      && audioPlaylists.nodes == m_audioPlaylists.nodes && audioPlaylists.items == m_audioPlaylists.items
                      && documents == m_documents;
    if (same)
        return;
    m_scanned = true;
    m_workspaces = workspaces;
    m_libraries = libraries;
    m_playlists = playlists;
    m_mediaPlaylists = mediaPlaylists;
    m_audioPlaylists = audioPlaylists;
    m_documents = documents;
    ++m_revision;
    emit changed();
}

// Lists a library the first time it is asked for; from then on rescan() keeps the list
// up to date. A workspace can have many libraries, and only the ones looked into cost
// anything.
QVariantList Catalog::documentsIn(const QString &library) const
{
    if (library.isEmpty())
        return {};
    auto listed = m_documents.constFind(library);
    if (listed == m_documents.constEnd())
        listed = m_documents.insert(library, listDocuments(library));
    return *listed;
}

QVariantList Catalog::listDocuments(const QString &library) const
{
    QVariantList documents;
    for (const QFileInfo &file : sortedEntries(library, {QStringLiteral("*.pro")}, QDir::Files)) {
        QStringList arrangements;
        QString arrangement;
        QString destination;
        ProDocument::arrangementsOf(file.absoluteFilePath(), &arrangements, &arrangement, nullptr, &destination);
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
            {"destination", destination},
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

QVariantList Catalog::audioIn(const QString &playlist) const
{
    return m_audioPlaylists.items.value(playlist);
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

QVariantMap Catalog::createLibrary(const QString &name)
{
    const QString wanted = name.simplified();
    if (wanted.isEmpty() || wanted.contains(u'/') || wanted.startsWith(u'.'))
        return {{"path", QString()}, {"error", QStringLiteral("A library needs a name, without a slash in it")}};
    if (m_librariesDirectory.isEmpty())
        return {{"path", QString()}, {"error", QStringLiteral("There is no workspace open")}};
    QDir libraries(m_librariesDirectory);
    if (libraries.exists(wanted))
        return {{"path", QString()}, {"error", QStringLiteral("There is a library named %1 already").arg(wanted)}};
    if (!libraries.mkpath(wanted))
        return {{"path", QString()}, {"error", QStringLiteral("Cannot make the folder %1").arg(libraries.filePath(wanted))}};
    rescan();
    return {{"path", libraries.filePath(wanted)}, {"error", QString()}};
}

QVariantMap Catalog::renameLibrary(const QString &path, const QString &name)
{
    const QString wanted = name.simplified();
    const QFileInfo folder(path);
    if (wanted == folder.fileName())
        return {{"path", path}, {"error", QString()}};
    if (wanted.isEmpty() || wanted.contains(u'/') || wanted.startsWith(u'.'))
        return {{"path", path}, {"error", QStringLiteral("A library needs a name, without a slash in it")}};
    if (!folder.isDir() || folder.absolutePath() != QDir(m_librariesDirectory).absolutePath())
        return {{"path", path}, {"error", QStringLiteral("That is not one of the workspace's libraries")}};
    if (!QDir(path).isEmpty())
        return {{"path", path}, {"error", QStringLiteral("Only an empty library can be renamed here: playlists find their presentations by the library's name")}};
    QDir libraries(m_librariesDirectory);
    if (libraries.exists(wanted))
        return {{"path", path}, {"error", QStringLiteral("There is a library named %1 already").arg(wanted)}};
    if (!libraries.rename(folder.fileName(), wanted))
        return {{"path", path}, {"error", QStringLiteral("Cannot rename the folder %1").arg(path)}};
    rescan();
    return {{"path", libraries.filePath(wanted)}, {"error", QString()}};
}

QVariantMap Catalog::createPresentation(const QString &library, const QString &name)
{
    QString made;
    const QString error = ProDocument::create(library, name, &made);
    // As when a song is imported: the lists are told at once.
    if (error.isEmpty())
        rescan();
    return {{"path", made}, {"error", error}};
}

QVariantMap Catalog::renamePresentation(const QString &file, const QString &name)
{
    // The rows that are this presentation are looked for while its file is still
    // there under its old name, which is how a row is tied to a file.
    const QStringList rows = PlaylistFile::rowsOf(m_root, file);
    QString renamed;
    QString error = ProDocument::rename(file, name, &renamed);
    if (error.isEmpty() && renamed != file) {
        error = PlaylistFile::renameRows(m_root, rows, QFileInfo(file).completeBaseName(), QFileInfo(renamed).fileName());
        if (!error.isEmpty())
            error = QStringLiteral("The presentation was renamed, but the playlists that have it could not be brought up to date: %1").arg(error);
    }
    if (renamed != file)
        rescan();
    return {{"path", renamed}, {"error", error}};
}

QVariantMap Catalog::deletePresentation(const QString &file)
{
    if (!QFileInfo(file).isFile())
        return {{"trash", QString()}, {"error", QStringLiteral("%1 is no longer there").arg(QFileInfo(file).fileName())}};
    QFile presentation(file);
    QString trash;
    if (!presentation.moveToTrash()) {
        return {{"trash", QString()},
                {"error", QStringLiteral("%1 could not be moved to the Trash, and has been left where it is: %2")
                              .arg(QFileInfo(file).fileName(), presentation.errorString())}};
    }
    // The file knows where it has gone.
    trash = presentation.fileName();
    SessionLog::write("trash", QStringLiteral("%1 moved to the Trash, as %2").arg(file, trash));
    rescan();
    return {{"trash", trash}, {"error", QString()}};
}

QVariantMap Catalog::importSong(const QUrl &file, const QString &library, int linesPerSlide)
{
    QString made;
    const QString error = songimport::importFile(file.isLocalFile() ? file.toLocalFile() : file.toString(), library, linesPerSlide, &made);
    // The library has one more in it than was listed: the lists are told at once, not
    // when the folder's watcher gets round to it.
    if (error.isEmpty())
        rescan();
    return {{"path", made}, {"error", error}};
}

QVariantMap Catalog::open(const QString &path, const std::optional<QString> &arrangement) const
{
    QString error;
    const ProDocument document = ProDocument::load(path, m_root, arrangement, &error);
    return {
        {"name", QFileInfo(path).completeBaseName()},
        {"path", path},
        {"uuid", document.uuid},
        {"slides", document.slides},
        {"arrangements", document.arrangements},
        {"arrangement", document.arrangement},
        {"groups", document.groups},
        {"arrangementList", document.arrangementList},
        {"hasChords", document.hasChords},
        {"originalKey", document.originalKey},
        {"userKey", document.userKey},
        {"destination", document.destination},
        {"slideShow", document.slideShow},
        {"transition", document.transition},
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

// In the presentation's own file, and on every row of a playlist that is that
// presentation, as ProPresenter has it (see PlaylistFile::setRowsDestination).
QString Catalog::setDestination(const QString &file, const QString &destination)
{
    QString error = ProDocument::setDestination(file, destination);
    if (error.isEmpty())
        error = PlaylistFile::setRowsDestination(m_root, file, destination);
    // (Read again whichever way it went: the file may have been changed and the rows not.)
    rescan();
    return error;
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

// (The lists are read again after each of the four that change which arrangements a
// presentation has or what they are called: every row of a list knows its
// presentation's arrangements, for its menu. A change to what an arrangement goes
// through is no list's concern.)
QVariantMap Catalog::addArrangement(const QString &path)
{
    QString name;
    const QString error = afterChange(ProDocument::addArrangement(path, &name));
    return {{"name", error.isEmpty() ? name : QString()}, {"error", error}};
}

QVariantMap Catalog::duplicateArrangement(const QString &path, const QString &id)
{
    QString name;
    const QString error = afterChange(ProDocument::duplicateArrangement(path, id, &name));
    return {{"name", error.isEmpty() ? name : QString()}, {"error", error}};
}

QString Catalog::renameArrangement(const QString &path, const QString &id, const QString &name)
{
    return afterChange(ProDocument::renameArrangement(path, id, name));
}

QString Catalog::removeArrangement(const QString &path, const QString &id)
{
    return afterChange(ProDocument::removeArrangement(path, id));
}

QString Catalog::insertArrangementGroup(const QString &path, const QString &id, const QString &groupId, int landing)
{
    return ProDocument::insertArrangementGroup(path, id, groupId, landing);
}

QString Catalog::moveArrangementGroup(const QString &path, const QString &id, int from, int landing)
{
    return ProDocument::moveArrangementGroup(path, id, from, landing);
}

QString Catalog::removeArrangementGroup(const QString &path, const QString &id, int place)
{
    return ProDocument::removeArrangementGroup(path, id, place);
}

QString Catalog::setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath, bool foreground)
{
    return ProDocument::setCueMedia(path, slideId, mediaPath, foreground, m_root);
}

QString Catalog::setSlideShow(const QString &path, double seconds)
{
    return ProDocument::setSlideShow(path, seconds);
}

QString Catalog::setSlideGoToNext(const QString &path, const QString &slideId, bool on, double seconds, bool loop)
{
    return ProDocument::setCueGoToNext(path, slideId, on, seconds, loop);
}

QString Catalog::setSlideMediaForeground(const QString &path, const QString &slideId, bool foreground)
{
    return ProDocument::setCueMediaForeground(path, slideId, foreground);
}

QString Catalog::removeSlide(const QString &path, const QString &slideId)
{
    return ProDocument::removeCue(path, slideId);
}

QString Catalog::setSlideMediaPlayback(const QString &path, const QString &slideId, int playback, int loopCount, double loopSeconds)
{
    return ProDocument::setCueMediaPlayback(path, slideId, playback, loopCount, loopSeconds);
}

QString Catalog::setSlideMediaSettings(const QString &path, const QString &slideId, const QVariantMap &changes)
{
    return ProDocument::setCueMediaSettings(path, slideId, changes);
}

QVariantMap Catalog::slideMediaSettings(const QString &path, const QString &slideId) const
{
    return ProDocument::cueMediaSettings(path, slideId);
}

QString Catalog::replaceSlideMediaFile(const QString &path, const QString &slideId, const QString &file)
{
    return ProDocument::replaceCueMediaFile(path, slideId, file, m_root);
}

QVariantList Catalog::mediaEffects() const
{
    QVariantList list;
    for (const mediaeffects::Effect &effect : mediaeffects::all()) {
        list.append(QVariantMap {
            {"renderId", effect.renderId},
            {"name", effect.name},
            {"description", effect.description},
            {"drawn", !effect.shader.isEmpty()},
        });
    }
    return list;
}

QVariantMap Catalog::newMediaEffect(const QString &renderId) const
{
    return mediasettings::newEffect(renderId);
}

void Catalog::showInFolder(const QString &path) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                       QStringLiteral("/org/freedesktop/FileManager1"),
                                                       QStringLiteral("org.freedesktop.FileManager1"),
                                                       QStringLiteral("ShowItems"));
    call << QStringList {QUrl::fromLocalFile(path).toString()} << QString();
    // Not waited for: a file manager that is not running is started by the bus first,
    // which can take a second, and the window goes on meanwhile. The folder alone is
    // the answer if there is none.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 5000));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [path](QDBusPendingCallWatcher *answer) {
        if (answer->isError()) {
            SessionLog::write("media", QStringLiteral("no file manager would pick out %1 (%2); its folder is opened")
                                           .arg(QFileInfo(path).fileName(), answer->error().name()));
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        }
        answer->deleteLater();
    });
}

QString Catalog::addSlideAction(const QString &path, const QString &slideId, const QVariantMap &action)
{
    return ProDocument::addCueAction(path, slideId, action);
}

QString Catalog::changeSlideAction(const QString &path, const QString &slideId, const QString &actionId, const QVariantMap &action)
{
    return ProDocument::changeCueAction(path, slideId, actionId, action);
}

QString Catalog::removeSlideAction(const QString &path, const QString &slideId, const QString &actionId)
{
    return ProDocument::removeCueAction(path, slideId, actionId);
}

QString Catalog::setSlideLabel(const QString &path, const QString &slideId, const QString &text, const QString &color)
{
    return ProDocument::setCueLabel(path, slideId, text.trimmed(), color);
}

QVariantMap Catalog::setSlideGroup(const QString &path, const QString &slideId, const QVariantMap &group)
{
    bool changed = false;
    const QString error = ProDocument::setGroupFrom(path, slideId, group.value("name").toString(), group.value("color").toString(),
                                                    group.value("id").toString(), &changed);
    return {{"changed", changed}, {"error", error}};
}

QVariantMap Catalog::insertSlide(const QString &path, const QString &slideId)
{
    QString id;
    const QString error = ProDocument::insertBlankCue(path, slideId, true, &id);
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QString Catalog::copySlide(const QString &path, const QString &slideId)
{
    QString error;
    const QByteArray copied = ProDocument::copyCue(path, slideId, &error);
    if (!error.isEmpty())
        return error;
    m_copiedSlide = copied;
    emit copiedSlideChanged();
    return {};
}

QVariantMap Catalog::pasteSlide(const QString &path, const QString &slideId)
{
    QString id;
    const QString error = ProDocument::pasteCue(path, slideId, true, m_copiedSlide, &id);
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QString Catalog::insertMediaSlides(const QString &path, const QString &slideId, bool after, const QStringList &mediaPaths)
{
    return ProDocument::insertMediaCues(path, slideId, after, mediaPaths, m_root);
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
    SessionLog::write("import", QStringLiteral("started: ") + archive.toLocalFile());

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

QStringList Catalog::mediaAmong(const QList<QUrl> &files) const
{
    QStringList paths;
    for (const QUrl &file : files) {
        const QString path = file.toLocalFile();
        if (workspace::isMedia(path) && QFileInfo(path).isFile())
            paths << path;
    }
    return paths;
}

QString Catalog::addMedia(const QString &playlist, const QList<QUrl> &files, const QString &target, bool after)
{
    const QStringList paths = mediaAmong(files);
    if (paths.isEmpty())
        return QStringLiteral("None of those files are images or videos this can show");
    return afterChange(PlaylistFile::addMedia(m_root, playlist, paths, target, after));
}

QString Catalog::removeMediaItem(const QString &item)
{
    return afterChange(PlaylistFile::removeItem(m_root, PlaylistFile::Media, item));
}

QString Catalog::setMediaItemForeground(const QString &item, bool foreground)
{
    return afterChange(PlaylistFile::setMediaForeground(m_root, item, foreground));
}

QString Catalog::setMediaItemSettings(const QString &item, const QVariantMap &changes)
{
    return afterChange(PlaylistFile::setMediaSettings(m_root, item, changes));
}

QVariantMap Catalog::mediaItemSettings(const QString &item) const
{
    return PlaylistFile::mediaSettings(m_root, item);
}

QString Catalog::replaceMediaItemFile(const QString &item, const QString &file)
{
    return afterChange(PlaylistFile::replaceMediaFile(m_root, item, file));
}

QString Catalog::moveMediaItem(const QString &item, const QString &target, bool after)
{
    return afterChange(PlaylistFile::moveItem(m_root, PlaylistFile::Media, item, target, after));
}

QVariantMap Catalog::createAudioPlaylist(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Audio, name, parent, false, &id));
    return {{"id", id}, {"error", error}};
}

QVariantMap Catalog::createAudioFolder(const QString &name, const QString &parent)
{
    QString id;
    const QString error = afterChange(PlaylistFile::createNode(m_root, PlaylistFile::Audio, name, parent, true, &id));
    return {{"id", id}, {"error", error}};
}

QString Catalog::renameAudioPlaylist(const QString &id, const QString &name)
{
    return afterChange(PlaylistFile::renameNode(m_root, PlaylistFile::Audio, id, name));
}

QString Catalog::removeAudioPlaylist(const QString &id)
{
    return afterChange(PlaylistFile::removeNode(m_root, PlaylistFile::Audio, id));
}

QString Catalog::moveAudioPlaylist(const QString &id, const QString &target, const QString &where)
{
    return afterChange(PlaylistFile::moveNode(m_root, PlaylistFile::Audio, id, target, where));
}

QStringList Catalog::audioAmong(const QList<QUrl> &files) const
{
    QStringList paths;
    for (const QUrl &file : files) {
        const QString path = file.toLocalFile();
        if (workspace::isAudio(path) && QFileInfo(path).isFile())
            paths << path;
    }
    return paths;
}

QString Catalog::addAudio(const QString &playlist, const QList<QUrl> &files, const QString &target, bool after)
{
    const QStringList paths = audioAmong(files);
    if (paths.isEmpty())
        return QStringLiteral("None of those files are sound files this can play");
    QList<double> seconds;
    for (const QString &path : paths)
        seconds << audioprobe::seconds(path);
    return afterChange(PlaylistFile::addAudio(m_root, playlist, paths, seconds, target, after));
}

QString Catalog::removeAudioItem(const QString &item)
{
    return afterChange(PlaylistFile::removeItem(m_root, PlaylistFile::Audio, item));
}

QString Catalog::moveAudioItem(const QString &item, const QString &target, bool after)
{
    return afterChange(PlaylistFile::moveItem(m_root, PlaylistFile::Audio, item, target, after));
}

QString Catalog::setAudioPlayback(const QString &item, const QString &playback)
{
    return afterChange(PlaylistFile::setAudioPlayback(m_root, item, playback));
}

QString Catalog::setAudioEffect(const QString &item, bool effect)
{
    return afterChange(PlaylistFile::setAudioEffect(m_root, item, effect));
}

void Catalog::rebuildThumbnails(const QStringList &paths)
{
    ThumbnailProvider::discard(paths);
    ++m_thumbnailRevision;
    emit thumbnailsDiscarded();
}
