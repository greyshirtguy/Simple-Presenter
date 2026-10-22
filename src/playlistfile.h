#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <functional>

namespace rv::data {
class Action;
}

// The playlists of a ProPresenter folder, read from <root>/Playlists/Library: a tree of
// folders holding playlists, each playlist an ordered list of headers and presentations.
// The media playlists, in <root>/Playlists/Media, are the same kind of file with media
// files for rows, and are handled by the same code.
//
// And the audio bin, in <root>/Playlists/Audio, is the same again with pieces of audio
// for rows (audiocue.h has what a row of it says about how it plays).
//
// All three files are one `PlaylistDocument` message: a root node whose children are folders
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
    // Which of the three files: the playlists of presentations, the media playlists, or
    // the audio bin's.
    enum Kind { Presentations, Media, Audio };

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
    // retriggers, volume, cueId, next, nextCue }, where `id` is the row's own id and
    // `path` the media file on this machine, empty with `missing` set if it cannot be
    // found. Four are how the row's media behaves, as workspace::MediaBehaviour
    // describes; the last three are its cue's id and what follows it, ProPresenter's
    // Next Cue, as mediacue::describe has them (the last two only for a row followed by
    // anything).
    //
    // For a playlist of the audio bin: { id, name, path, source, missing, playback,
    // effect, volume, duration, loopCount, loopSeconds }, the first five as a media
    // playlist's are and the rest how the row plays, as audiocue::Behaviour describes.
    QHash<QString, QVariantList> items;

    // An absent file is an empty set of playlists, not an error.
    static PlaylistFile load(const QString &root, QString *error);
    // The media playlists, from <root>/Playlists/Media; playlists carry the icon
    // "mediaPlaylist".
    static PlaylistFile loadMedia(const QString &root, QString *error);
    // The audio bin's playlists, from <root>/Playlists/Audio; playlists carry the icon
    // "audioPlaylist".
    static PlaylistFile loadAudio(const QString &root, QString *error);

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
    // Records on every row, of any playlist, that is this presentation file where the
    // presentation is sent: "announcements", or "" for the slide and media layers. A
    // row that says so already is left, and with none to change nothing is written.
    //
    // ProPresenter keeps a presentation's destination in the presentation's own file
    // and on each of its rows as well (in the workspaces looked at, a presentation
    // sent to the announcements had it in both, on every row), so a change of it here
    // is made in both (see Catalog::setDestination). This app reads the file's.
    static QString setRowsDestination(const QString &root, const QString &file, const QString &destination);
    // The rows, of any playlist, that are this presentation file: their ids. Asked
    // before the file is renamed, while it is still there to be found.
    static QStringList rowsOf(const QString &root, const QString &file);
    // Brings those rows along when their presentation has been renamed: each now
    // refers to a file called `fileName` in the folder it referred to before, and a
    // row that was called what the presentation was called (`oldName`, as a row is when
    // it is made) is called what the presentation is called now. Nothing else of a
    // row is changed, its arrangement least of all. With no rows there is nothing to
    // do, and nothing is written.
    static QString renameRows(const QString &root, const QStringList &ids, const QString &oldName, const QString &fileName);
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
    // Changes the media action of a row of the media bin (`kind` Media) or of the audio
    // bin (Audio) by `change`, which returns an error message, and writes the file if
    // there is none: what a row's playback markers are changed through
    // (playbackmarkers.h).
    // The media action of such a row as the file has it, handed to `look`; false where
    // the file, the row or its media is not there. (What its markers are read from.)
    static bool readMediaAction(const QString &root, Kind kind, const QString &itemId,
                                const std::function<void(const rv::data::Action &)> &look);
    static QString changeMediaAction(const QString &root, Kind kind, const QString &itemId,
                                     const std::function<QString(rv::data::Action *)> &change);
    // What follows a row when its video has played to its end (see mediacue.h): `next`
    // "" for nothing, "next", "random", "first", or "cue" with `targetId` the row to go
    // to. Anything to follow makes the row's video stop at its end.
    static QString setMediaNext(const QString &root, const QString &itemId, const QString &next, const QString &targetId = {});
    // What the Media Inspector sets of a row's media, as mediasettings::apply takes
    // changes; and the row's media replaced by another file, keeping what
    // mediasettings::carryOver keeps (and the row's name, if it was given one).
    static QString setMediaSettings(const QString &root, const QString &itemId, const QVariantMap &changes);
    // All of those settings, as mediasettings::describe gives them (empty if the row is
    // not there): what the row has only for media set to anything.
    static QVariantMap mediaSettings(const QString &root, const QString &itemId);
    static QString replaceMediaFile(const QString &root, const QString &itemId, const QString &file);
    // Audio only:

    // Adds sound files to a playlist of the audio bin, in the order given, where
    // addMedia would put them. `seconds` is how long each is, in the same order, with 0
    // (or nothing) for one whose length is not known.
    static QString addAudio(const QString &root, const QString &playlistId, const QStringList &files,
                            const QList<double> &seconds, const QString &targetId = {}, bool after = false);
    // How a row plays on from its end: "stop", "loop" or "next" (see audiocue.h).
    static QString setAudioPlayback(const QString &root, const QString &itemId, const QString &playback);
    // Makes a row a sound effect, or music.
    static QString setAudioEffect(const QString &root, const QString &itemId, bool effect);
    // A copy of the action of a row, as the file has it, for a slide or a macro that is
    // to play the same thing: given an id of its own, and nothing else changed.
    static QString audioAction(const QString &root, const QString &itemId, rv::data::Action *action);

    // If there is no media playlists file yet, writes one that mirrors the folders under
    // <root>/Media: a playlist for each folder of media, inside playlist folders that
    // follow the folders on disk. Does nothing if the file exists.
    static QString seedMediaFromFolders(const QString &root);
};
