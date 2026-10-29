#pragma once

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QString>

#include <functional>

// A zip archive written one entry at a time, straight to a file: what a PowerPoint file
// is (Export to PowerPoint, pptxwriter.h). A port of ProToPPTX's ZipWriter, with zlib
// (which the app links already, for reading ProPresenter's bundles: zipreader.h) for
// the packing and the checksum.
//
// The XML parts are deflated; media that is already compressed (pictures, video, sound)
// is stored as it is, which is what PowerPoint itself does, and saves time for nothing
// lost. Each entry is written whole, its sizes and checksum in its local header (no data
// descriptors after), then the central directory and the end record at finish(). No
// ZIP64: an entry or an archive of 4 GB or more is refused with a plain error rather
// than written wrong; a slide show that large is not something PowerPoint opens well
// either.
//
// Entries' dates are left at nought (1980-01-01): the same deck written twice is then
// the same bytes, which is worth more here than a date nobody reads.
class PptxZipWriter
{
public:
    enum Packing { Store, Deflate };

    // Creates (or empties) the file and writes the archive into it. False, with *error
    // set, if it cannot be opened.
    bool open(const QString &path, QString *error);
    // Adds one file. `path` uses "/" between its parts and does not start with one.
    bool add(const QString &path, const QByteArray &data, Packing packing, QString *error);
    // The same, for a file on this machine, copied in without being read whole first
    // (a video can be large). Stored, as media is.
    bool addFile(const QString &path, const QString &source, QString *error);
    // Writes the central directory and the end record, and closes the file.
    bool finish(QString *error);

private:
    struct Written
    {
        QByteArray path;
        quint16 method = 0;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 uncompressedSize = 0;
        quint32 offset = 0;
    };

    bool writeEntry(const QByteArray &name, quint16 method, quint32 crc, quint64 compressed, quint64 uncompressed,
                    const std::function<bool(QString *)> &body, QString *error);

    QFile m_file;
    quint64 m_offset = 0;
    QList<Written> m_written;
};
