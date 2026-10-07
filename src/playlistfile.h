#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>

// The playlists of a ProPresenter folder, read from <root>/Playlists/Library: a tree of
// folders holding playlists, each playlist an ordered list of headers and presentations.
// The media playlists, in <root>/Playlists/Media, are the same kind of file with media
// files for rows, and are handled by the same code.
//
// Both files are one `PlaylistDocument` message: a root node whose children are folders
// (nodes that hold nodes) and playlists (nodes that hold items). A playlist does not
// contain its presentations or media; each row refers to a file (see workspacefiles.h
// for how those references are read and written).
//
// Reading flattens the tree into rows for a list to show. Every change is its own
// function, which reads the file, makes the one change to the message and writes it
// back; nothing is held in memory between changes, so the file on disk is always the
// truth, whichever of this app and ProPresenter wrote it last.
struct PlaylistFile
{
    // Which of the two files: the playlists of presentations, or the media playlists.
    enum Kind { Presentations, Media };

    // The tree flattened in display order: { name, path, parent, depth, folder, icon }.
    // `path` is the node's id and `parent` its folder's, "" at the top level; `folder`
    // rows hold other rows and have no items of their own.
    QVariantList nodes;
    // Each playlist's rows by its id.
    //
    // For a playlist of presentations: { path, name, kind, icon, file, missing,
    // playlistItem, arrangement, arrangements, detail, color }. `path` is the row's own
    // id, since one presentation can appear several times; `file` is where the
    // presentation is on this machine, with `missing` set if it is not there. `kind` is
    // "header", "presentation" or "other". A playlist row carries its own choice of
    // arrangement, "" being every group in stored order.
    //
    // For a media playlist: { id, name, path, source, video, missing, foreground, loops,
    // retriggers, volume }, where `id` is the row's own id and `path` the media file on
    // this machine, empty with `missing` set if it cannot be found. The last four are
    // how the row's media behaves, as workspace::MediaBehaviour describes.
    QHash<QString, QVariantList> items;

    // An absent file is an empty set of playlists, not an error.
    static PlaylistFile load(const QString &root, QString *error);
    // The media playlists, from <root>/Playlists/Media; playlists carry the icon
    // "mediaPlaylist".
    static PlaylistFile loadMedia(const QString &root, QString *error);

    // The functions below each change a file and write it back, returning an error
    // message that is empty on success. Everything else in the file is kept as it was.
    // Removing only ever removes lists and rows: the files they referred to are untouched.

    // Adds an empty playlist, or with `folder` an empty folder, inside the folder
    // `parentId` or at the top level for "", creating the file if there is none. Sets
    // *id to the new node's id.
    static QString createNode(const QString &root, Kind kind, const QString &name, const QString &parentId,
                              bool folder, QString *id);
    // Renames a playlist or folder.
    static QString renameNode(const QString &root, Kind kind, const QString &id, const QString &name);
    // Removes a playlist, or a folder and everything in it.
    static QString removeNode(const QString &root, Kind kind, const QString &id);
    // Moves a playlist or folder next to another one ("before" or "after" it), or into a
    // folder ("onto" it). A folder cannot go inside itself.
    static QString moveNode(const QString &root, Kind kind, const QString &id, const QString &targetId,
                            const QString &where);
    // Removes a row from its playlist.
    static QString removeItem(const QString &root, Kind kind, const QString &itemId);
    // Moves a row to just before, or with `after` just after, another row of the same
    // playlist.
    static QString moveItem(const QString &root, Kind kind, const QString &itemId, const QString &targetId,
                            bool after);

    // Presentations only:

    // Records which arrangement a playlist row uses, by id and name ("" and "" for none).
    static QString setItemArrangement(const QString &root, const QString &itemId,
                                      const QString &arrangementId, const QString &arrangementName);
    // Appends a presentation file to a playlist, using the arrangement the presentation
    // has selected.
    static QString addPresentation(const QString &root, const QString &playlistId, const QString &file);
    // Adds the playlists of an exported playlist document (the `data` of a .proplaylist
    // archive) inside the folder `parentId`, or at the top level for "". Their
    // presentations are taken to be in the folder `library`. Sets *id to the first
    // playlist added.
    static QString importPlaylists(const QString &root, const QByteArray &data, const QString &library,
                                   const QString &parentId, QString *id);

    // Media only:

    // Adds image and video files to a media playlist, in the order given: at its end,
    // or, given one of its rows as `targetId`, just before that row or with `after`
    // just after it.
    static QString addMedia(const QString &root, const QString &playlistId, const QStringList &files,
                            const QString &targetId = {}, bool after = false);
    // Makes a row a background or a foreground (see workspace::MediaBehaviour).
    static QString setMediaForeground(const QString &root, const QString &itemId, bool foreground);
    // If there is no media playlists file yet, writes one that mirrors the folders under
    // <root>/Media: a playlist for each folder of media, inside playlist folders that
    // follow the folders on disk. Does nothing if the file exists.
    static QString seedMediaFromFolders(const QString &root);
};
