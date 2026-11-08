#pragma once

#include <QByteArray>
#include <QString>

// Unpacks a ProPresenter exported playlist (.proplaylist): a zip archive holding the
// playlist itself, the presentations it uses and, if they were included in the export,
// their media.
//
// A presentation the library has already, by its file's name, is left as it is, or with
// `replace` given the archive's in its place: what the person is asked before the import
// when there are any (the maintainer, 2026-11-08: an import had left the library's older
// copies, and they would rather be offered to overwrite them). One that is replaced is
// copied first among the editor's backups (see PresentationEditor::backupFolder), so that
// nothing is lost by it. Media already there is always left as it is: a file of the same
// name under Media is taken to be the same file, and may be in use elsewhere.
struct PlaylistImport
{
    // The archive's playlist document, to be added to the playlists by
    // PlaylistFile::importPlaylists once the files are in place.
    QByteArray playlistData;
    int presentations = 0;          // copied into the library
    int presentationsKept = 0;      // already there, left as they were
    int presentationsReplaced = 0;  // already there, replaced by the archive's
    int media = 0;                  // copied into the media folder
    int mediaKept = 0;              // already there, left as they were

    // Copies the archive's presentations into `library` and its media under
    // <root>/Media, replacing presentations already there only with `replace`. Safe to
    // run off the main thread; it changes no playlists. Sets *error and stops at the first
    // failure.
    static PlaylistImport unpack(const QString &archive, const QString &root, const QString &library, bool replace, QString *error);

    // The names of the archive's presentations that `library` has already, and its
    // playlist document (*playlistData): what is asked about before an import. Reads the
    // archive's list of what is in it and its playlist, and copies nothing.
    static QStringList presentationsHere(const QString &archive, const QString &library, QByteArray *playlistData, QString *error);

    // One sentence saying what was done.
    QString summary() const;
};
