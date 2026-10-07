#pragma once

#include "playlistfile.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>

// What is on disk in a workspace: one folder holding everything for a setup,
//   <workspace>/Libraries/<library>/*.pro   presentations, one flat folder per library
//   <workspace>/Media/...                   images and videos, in any depth of folders
//   <workspace>/Playlists/Library           the playlists, in ProPresenter's own format
//   <workspace>/Playlists/Media             the media playlists, likewise
// which is how ProPresenter lays out its own folder, so a copy of one is a workspace.
// Workspaces sit side by side in one folder, and the catalog can be switched from one to
// another.
//
// This is the one object the QML side talks to about what is on disk. It holds what the
// lists show (libraries, playlists, media playlists and their rows) as plain lists of
// maps, read whenever the folders change; `changed` says they have. Every change the app
// makes to a workspace goes through here too, as a function that writes the file and
// returns an error message, empty on success. The reading and writing themselves are in
// PlaylistFile (the two playlists files), ProDocument (presentations) and
// PlaylistImport (exported playlists); the editor has its own, PresentationEditor.
class Catalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created in main.cpp")
    // The open workspace's folder and its name, and { name, path } for it and every
    // folder beside it, sorted by name.
    Q_PROPERTY(QString workspacePath READ workspacePath NOTIFY workspaceChanged)
    Q_PROPERTY(QString workspaceName READ workspaceName NOTIFY workspaceChanged)
    Q_PROPERTY(QVariantList workspaces READ workspaces NOTIFY changed)
    Q_PROPERTY(QString librariesDirectory READ librariesDirectory NOTIFY workspaceChanged)
    Q_PROPERTY(QString mediaDirectory READ mediaDirectory NOTIFY workspaceChanged)
    // { name, path } for each library folder, sorted by name.
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY changed)
    // { name, path, depth, folder } for each playlist and playlist folder, in tree order.
    // `path` is the node's id; folder rows only hold other rows.
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
    // The same for the media playlists and their folders, which are what the media bin
    // shows.
    Q_PROPERTY(QVariantList mediaPlaylists READ mediaPlaylists NOTIFY changed)
    // Goes up whenever anything on disk changed; bind to it to re-query the folder contents.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    // Goes up when cached thumbnails have been discarded. Thumbnail urls carry it, so the
    // views ask for theirs again.
    Q_PROPERTY(int thumbnailRevision READ thumbnailRevision NOTIFY thumbnailsDiscarded)
    // Whether an import is still copying files.
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    // A filter for a file dialog, showing the media files the app can use.
    Q_PROPERTY(QString mediaDialogFilter READ mediaDialogFilter CONSTANT)

public:
    explicit Catalog(const QString &workspace, QObject *parent = nullptr);

    // Switches to another workspace folder, creating what it lacks. Everything the
    // catalog reports changes with it.
    Q_INVOKABLE void openWorkspace(const QString &path);

    QString workspacePath() const { return m_root; }
    QString workspaceName() const;
    QVariantList workspaces() const { return m_workspaces; }
    QString librariesDirectory() const { return m_librariesDirectory; }
    QString mediaDirectory() const { return m_mediaDirectory; }
    QVariantList libraries() const { return m_libraries; }
    QVariantList playlists() const { return m_playlists.nodes; }
    QVariantList mediaPlaylists() const { return m_mediaPlaylists.nodes; }
    int revision() const { return m_revision; }
    bool importing() const { return m_importing; }
    int thumbnailRevision() const { return m_thumbnailRevision; }
    QString mediaDialogFilter() const;

    // Discards the cached thumbnails of these media files; they are made again as the
    // views next show them.
    Q_INVOKABLE void rebuildThumbnails(const QStringList &paths);

    // A row for each presentation directly in a library folder, sorted by name, in the
    // shape PlaylistFile describes for playlist rows: here `path` and `file` are both the
    // file's path, and the arrangement is the one selected in the file.
    Q_INVOKABLE QVariantList documentsIn(const QString &library) const;
    // The rows of the media playlist with this id, as PlaylistFile describes them.
    Q_INVOKABLE QVariantList mediaIn(const QString &playlist) const;
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

    // The same changes for the media playlists, and adding media files to one. Files are
    // referred to where they are, not copied.
    Q_INVOKABLE QVariantMap createMediaPlaylist(const QString &name, const QString &parent);
    Q_INVOKABLE QVariantMap createMediaFolder(const QString &name, const QString &parent);
    Q_INVOKABLE QString renameMediaPlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString removeMediaPlaylist(const QString &id);
    Q_INVOKABLE QString moveMediaPlaylist(const QString &id, const QString &target, const QString &where);
    // Files go at the end of the playlist, or, given one of its rows as `target`, just
    // before that row or with `after` just after it.
    Q_INVOKABLE QString addMedia(const QString &playlist, const QList<QUrl> &files, const QString &target = {},
                                 bool after = false);
    // Which of these files are images and videos the app can use, as paths: what is
    // dragged in from a file manager can be anything.
    Q_INVOKABLE QStringList mediaAmong(const QList<QUrl> &files) const;
    Q_INVOKABLE QString removeMediaItem(const QString &item);
    // Makes a media playlist's row a background or a foreground. It is the row's own:
    // the same file on a slide, or in another row, keeps the behaviour it has there.
    Q_INVOKABLE QString setMediaItemForeground(const QString &item, bool foreground);
    Q_INVOKABLE QString moveMediaItem(const QString &item, const QString &target, bool after);

    // Selects the named arrangement in the presentation file, or none for "", and saves
    // the file. Returns an error message, empty on success.
    Q_INVOKABLE QString setArrangement(const QString &path, const QString &arrangement);
    // Makes a slide (by its `id`) trigger the given media file, as a background or a
    // foreground, replacing any media it triggered before, and saves the presentation
    // file. Returns an error message, empty on success.
    Q_INVOKABLE QString setSlideMedia(const QString &path, const QString &slideId, const QString &mediaPath,
                                      bool foreground);
    // Makes the media a slide triggers a background or a foreground (see
    // workspace::MediaBehaviour), and saves the presentation file.
    Q_INVOKABLE QString setSlideMediaForeground(const QString &path, const QString &slideId, bool foreground);
    // Stops a slide triggering media, and saves the presentation file.
    Q_INVOKABLE QString removeSlideMedia(const QString &path, const QString &slideId);
    // Adds a slide to the presentation for each of these media files, each with nothing
    // on it but triggering its file as a foreground (see ProDocument::insertMediaCues):
    // just before the slide with this id or with `after` just after it, or at the end
    // for "". Saves the presentation file.
    Q_INVOKABLE QString insertMediaSlides(const QString &path, const QString &slideId, bool after,
                                          const QStringList &mediaPaths);

signals:
    void changed();
    void workspaceChanged();
    void importingChanged();
    void thumbnailsDiscarded();
    // `error` is empty on success, when `summary` says what was brought in and
    // `playlist` is the id of the first playlist added.
    void importFinished(const QString &error, const QString &summary, const QString &playlist);

private:
    void rescan();
    QVariantList listDocuments(const QString &library) const;
    QString afterChange(const QString &error);
    QVariantMap open(const QString &path, const std::optional<QString> &arrangement) const;

    QString m_root;
    QString m_librariesDirectory;
    QString m_mediaDirectory;
    QVariantList m_workspaces;
    QVariantList m_libraries;
    PlaylistFile m_playlists;
    PlaylistFile m_mediaPlaylists;
    int m_revision = 0;
    bool m_importing = false;
    int m_thumbnailRevision = 0;
    // The presentations of each library that has been asked for, by its path
    mutable QHash<QString, QVariantList> m_documents;
    // Whether the workspace has been read at all yet
    bool m_scanned = false;
    QFileSystemWatcher m_watcher;
    // Runs out a little after the last change on disk, and then the workspace is read
    QTimer m_settle;
};
