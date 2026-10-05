#pragma once

#include "playlistfile.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>

// What is on disk under the app's root folder:
//   <root>/Libraries/<library>/*.pro   presentations, one flat folder per library
//   <root>/Media/...                   images and videos, in any depth of folders
//   <root>/Playlists/Library           the playlists, in ProPresenter's own format
// which is how ProPresenter lays out its own folder, so that can be the root.
// Kept up to date as files and folders come and go.
class Catalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")
    Q_PROPERTY(QString librariesDirectory READ librariesDirectory CONSTANT)
    Q_PROPERTY(QString mediaDirectory READ mediaDirectory CONSTANT)
    // { name, path } for each library folder, sorted by name.
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY changed)
    // { name, path, depth } for the media folder and every folder under it, in tree order.
    Q_PROPERTY(QVariantList mediaFolders READ mediaFolders NOTIFY changed)
    // { name, path, depth, folder } for each playlist and playlist folder, in tree order.
    // `path` is the node's id; folder rows only hold other rows.
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    // { name, path, depth, folder, icon } for each media playlist and media playlist
    // folder, in tree order, from ProPresenter's media playlists file. Their paths start
    // with "playlist:", which mediaIn understands. Read only.
    Q_PROPERTY(QVariantList mediaPlaylists READ mediaPlaylists NOTIFY changed)
    // Goes up whenever anything on disk changed; bind to it to re-query the folder contents.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // Whether an import is still copying files.
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)

public:
    explicit Catalog(const QString &root, QObject *parent = nullptr);

    QString librariesDirectory() const { return m_librariesDirectory; }
    QString mediaDirectory() const { return m_mediaDirectory; }
    QVariantList libraries() const { return m_libraries; }
    QVariantList mediaFolders() const { return m_mediaFolders; }
    QVariantList playlists() const { return m_playlists.nodes; }
    QVariantList mediaPlaylists() const { return m_mediaPlaylists.nodes; }
    int revision() const { return m_revision; }
    bool importing() const { return m_importing; }

    // A row for each presentation directly in a library folder, sorted by name, in the
    // shape PlaylistFile describes for playlist rows: here `path` and `file` are both the
    // file's path, and the arrangement is the one selected in the file.
    Q_INVOKABLE QVariantList documentsIn(const QString &library) const;
    // { name, path, source, video } for each image and video directly in a folder, sorted
    // by name; or, given a media playlist's path, its media in playlist order, where a
    // file that cannot be found has `missing` set and no path.
    Q_INVOKABLE QVariantList mediaIn(const QString &folder) const;
    // The rows of the playlist with this id, as PlaylistFile describes them.
    Q_INVOKABLE QVariantList playlistItems(const QString &playlist) const;
    // Returns { name, path, slides, arrangements, arrangement, error }; error is empty on
    // success. Follows the arrangement the presentation has selected.
    Q_INVOKABLE QVariantMap open(const QString &path) const;
    // The same, following the named arrangement; "" is every group in stored order.
    Q_INVOKABLE QVariantMap openArranged(const QString &path, const QString &arrangement) const;
    // Records which arrangement of the presentation `file` a playlist row uses, "" for
    // none, and saves the playlists. Returns an error message, empty on success.
    Q_INVOKABLE QString setPlaylistItemArrangement(const QString &item, const QString &file, const QString &arrangement);
    // Changes to the playlists, each saved at once. They return an error message, empty
    // on success, except the two that create, which return { id, error }. A new playlist
    // or folder goes inside the folder `parent`, or at the top level for "". Removing
    // only removes lists and rows: presentations are never touched.
    Q_INVOKABLE QVariantMap createPlaylist(const QString &name, const QString &parent);
    Q_INVOKABLE QVariantMap createPlaylistFolder(const QString &name, const QString &parent);
    Q_INVOKABLE QString renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString removePlaylist(const QString &id);
    Q_INVOKABLE QString addToPlaylist(const QString &playlist, const QString &file);
    Q_INVOKABLE QString removePlaylistItem(const QString &item);
    Q_INVOKABLE QString movePlaylistItem(const QString &item, const QString &target, bool after);
    // Moves a playlist or folder "before" or "after" another, or "onto" (into) a folder.
    Q_INVOKABLE QString movePlaylistNode(const QString &id, const QString &target, const QString &where);
    // Imports an exported playlist (.proplaylist): its presentations go into the folder
    // `library`, its media under Media, and its playlists inside the folder `parent` or
    // at the top level for "". Nothing already on disk is overwritten. The copying runs
    // in the background; importFinished reports the outcome.
    Q_INVOKABLE void importPlaylist(const QUrl &archive, const QString &library, const QString &parent);

    // Selects the named arrangement in the presentation file, or none for "", and saves
    // the file. Returns an error message, empty on success.
    Q_INVOKABLE QString setArrangement(const QString &path, const QString &arrangement);
    // Makes a slide (by its `id`) trigger the given media file, replacing any media it
    // triggered before, and saves the presentation file. Returns an error message, empty
    // on success.
    Q_INVOKABLE QString setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath);
    // Stops a slide triggering media, and saves the presentation file.
    Q_INVOKABLE QString removeSlideMedia(const QString &path, const QString &slideId);

signals:
    void changed();
    void importingChanged();
    // `error` is empty on success, when `summary` says what was brought in and
    // `playlist` is the id of the first playlist added.
    void importFinished(const QString &error, const QString &summary, const QString &playlist);

private:
    void rescan();
    QString afterChange(const QString &error);
    QVariantMap open(const QString &path, const std::optional<QString> &arrangement) const;

    QString m_root;
    QString m_librariesDirectory;
    QString m_mediaDirectory;
    QVariantList m_libraries;
    QVariantList m_mediaFolders;
    PlaylistFile m_playlists;
    PlaylistFile m_mediaPlaylists;
    int m_revision = 0;
    bool m_importing = false;
    QFileSystemWatcher m_watcher;
};
