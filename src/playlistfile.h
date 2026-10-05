#pragma once

#include <QHash>
#include <QString>
#include <QVariantList>

// The playlists of a ProPresenter folder, read from <root>/Playlists/Library: a tree of
// folders holding playlists, each playlist an ordered list of headers and presentations.
struct PlaylistFile
{
    // The tree flattened in display order: { name, path, parent, depth, folder, icon }.
    // `path` is the node's id and `parent` its folder's, "" at the top level; `folder`
    // rows hold other rows and have no items of their own.
    QVariantList nodes;
    // Each playlist's rows by its id: { path, name, kind, icon, file, missing,
    // playlistItem, arrangement, arrangements, detail, color }. `path` is the row's own id, since one
    // presentation can appear several times; `file` is where the presentation is on this
    // machine, with `missing` set if it is not there. `kind` is "header",
    // "presentation" or "other". A playlist row carries its own choice of arrangement,
    // "" being every group in stored order.
    QHash<QString, QVariantList> items;

    // An absent file is an empty set of playlists, not an error.
    static PlaylistFile load(const QString &root, QString *error);
    // The functions below each change the file and write it back, returning an error
    // message that is empty on success. Everything else in the file is kept as it was.

    // Records which arrangement a playlist row uses, by id and name ("" and "" for none).
    static QString setItemArrangement(const QString &root, const QString &itemId,
                                      const QString &arrangementId, const QString &arrangementName);
    // Adds an empty playlist, or with `folder` an empty folder, inside the folder
    // `parentId` or at the top level for "", creating the file if there is none. Sets
    // *id to the new node's id.
    static QString createNode(const QString &root, const QString &name, const QString &parentId, bool folder,
                              QString *id);
    // Renames a playlist or folder.
    static QString renameNode(const QString &root, const QString &id, const QString &name);
    // Removes a playlist, or a folder and everything in it. Only the lists go: the
    // presentations they referred to are untouched.
    static QString removeNode(const QString &root, const QString &id);
    // Appends a presentation file to a playlist, using the arrangement the presentation
    // has selected.
    static QString addPresentation(const QString &root, const QString &playlistId, const QString &file);
    // Removes a row from its playlist.
    static QString removeItem(const QString &root, const QString &itemId);
    // Moves a row to just before, or with `after` just after, another row of the same
    // playlist.
    static QString moveItem(const QString &root, const QString &itemId, const QString &targetId, bool after);
    // Moves a playlist or folder next to another one ("before" or "after" it), or into a
    // folder ("onto" it). A folder cannot go inside itself.
    static QString moveNode(const QString &root, const QString &id, const QString &targetId, const QString &where);
    // Adds the playlists of an exported playlist document (the `data` of a .proplaylist
    // archive) inside the folder `parentId`, or at the top level for "". Their
    // presentations are taken to be in the folder `library`. Sets *id to the first
    // playlist added.
    static QString importPlaylists(const QString &root, const QByteArray &data, const QString &library,
                                   const QString &parentId, QString *id);
};
