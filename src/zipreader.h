#pragma once

#include <QFile>
#include <QList>
#include <QString>

// Reads zip archives, including the large-file (zip64) kind ProPresenter writes for its
// exported playlists. Those record a directory size that is slightly off, which stricter
// readers reject; this one only trusts where the directory starts and how many entries
// it has. Files are copied out in pieces, so a large video never sits in memory.
class ZipReader
{
public:
    struct Entry
    {
        QString name;
        quint64 size = 0;
        quint64 compressedSize = 0;
        quint64 headerOffset = 0;
        quint32 crc = 0;
        int method = 0; // 0 stored, 8 deflated
    };

    // False, with *error set, if the file is not a zip archive this can read.
    bool open(const QString &path, QString *error);
    const QList<Entry> &entries() const { return m_entries; }

    // Both check the entry's checksum and fail if it does not match.
    bool read(const Entry &entry, QByteArray *data, QString *error);
    bool extract(const Entry &entry, const QString &destination, QString *error);

private:
    bool copyOut(const Entry &entry, QIODevice *out, QString *error);

    QFile m_file;
    QList<Entry> m_entries;
};
