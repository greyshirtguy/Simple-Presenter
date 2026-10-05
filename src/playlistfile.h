#pragma once

#include <QHash>
#include <QString>
#include <QVariantList>

// The playlists of a ProPresenter folder, read from <root>/Playlists/Library: a tree of
// folders holding playlists, each playlist an ordered list of headers and presentations.
struct PlaylistFile
{
    // The tree flattened in display order: { name, path, depth, folder }. `path` is the
    // node's id; `folder` rows hold other rows and have no items of their own.
    QVariantList nodes;
    // Each playlist's rows by its id: { path, name, kind, file, missing, playlistItem,
    // arrangement, arrangements, detail, color }. `path` is the row's own id, since one
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
    // Adds an empty playlist at the top level, creating the file if there is none, and
    // sets *id to the new playlist's id.
    static QString createPlaylist(const QString &root, const QString &name, QString *id);
    // Renames a playlist or folder.
    static QString renameNode(const QString &root, const QString &id, const QString &name);
    // Deletes a playlist. Folders are refused: deleting one would take its playlists too.
    static QString deletePlaylist(const QString &root, const QString &id);
    // Appends a presentation file to a playlist, using the arrangement the presentation
    // has selected.
    static QString addPresentation(const QString &root, const QString &playlistId, const QString &file);
    // Removes a row from its playlist.
    static QString removeItem(const QString &root, const QString &itemId);
};
