#pragma once

#include "playlistfile.h"

#include <QFileSystemWatcher>
#include <QObject>
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
    // Goes up whenever anything on disk changed; bind to it to re-query the folder contents.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit Catalog(const QString &root, QObject *parent = nullptr);

    QString librariesDirectory() const { return m_librariesDirectory; }
    QString mediaDirectory() const { return m_mediaDirectory; }
    QVariantList libraries() const { return m_libraries; }
    QVariantList mediaFolders() const { return m_mediaFolders; }
    QVariantList playlists() const { return m_playlists.nodes; }
    int revision() const { return m_revision; }

    // A row for each presentation directly in a library folder, sorted by name, in the
    // shape PlaylistFile describes for playlist rows: here `path` and `file` are both the
    // file's path, and the arrangement is the one selected in the file.
    Q_INVOKABLE QVariantList documentsIn(const QString &library) const;
    // { name, path, source, video } for each image and video directly in a folder, sorted by name.
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
    // on success; createPlaylist returns { id, error }.
    Q_INVOKABLE QVariantMap createPlaylist(const QString &name);
    Q_INVOKABLE QString renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE QString deletePlaylist(const QString &id);
    Q_INVOKABLE QString addToPlaylist(const QString &playlist, const QString &file);
    Q_INVOKABLE QString removePlaylistItem(const QString &item);
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
    int m_revision = 0;
    QFileSystemWatcher m_watcher;
};
