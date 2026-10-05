#pragma once

#include <QByteArray>
#include <QString>

// Unpacks a ProPresenter exported playlist (.proplaylist): a zip archive holding the
// playlist itself, the presentations it uses and, if they were included in the export,
// their media.
struct PlaylistImport
{
    // The archive's playlist document, to be added to the playlists by
    // PlaylistFile::importPlaylists once the files are in place.
    QByteArray playlistData;
    int presentations = 0;       // copied into the library
    int presentationsKept = 0;   // already there, left as they were
    int media = 0;               // copied into the media folder
    int mediaKept = 0;           // already there, left as they were

    // Copies the archive's presentations into `library` and its media under
    // <root>/Media. Nothing that already exists is overwritten. Safe to run off the
    // main thread; it changes no playlists. Sets *error and stops at the first failure.
    static PlaylistImport unpack(const QString &archive, const QString &root, const QString &library, QString *error);

    // One sentence saying what was done.
    QString summary() const;
};
