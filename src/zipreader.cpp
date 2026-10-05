#include "zipreader.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QtEndian>

#include <zlib.h>

namespace {

const quint32 endOfDirectory = 0x06054b50;
const quint32 zip64EndOfDirectory = 0x06064b50;
const quint32 directoryEntry = 0x02014b50;
const quint32 localHeader = 0x04034b50;

quint16 u16(const QByteArray &bytes, qsizetype at) { return qFromLittleEndian<quint16>(bytes.constData() + at); }
quint32 u32(const QByteArray &bytes, qsizetype at) { return qFromLittleEndian<quint32>(bytes.constData() + at); }
quint64 u64(const QByteArray &bytes, qsizetype at) { return qFromLittleEndian<quint64>(bytes.constData() + at); }

// Position of the last occurrence of a record signature, or -1.
qsizetype lastSignature(const QByteArray &bytes, quint32 signature)
{
    char pattern[4];
    qToLittleEndian(signature, pattern);
    return bytes.lastIndexOf(QByteArray(pattern, 4));
}

} // namespace

bool ZipReader::open(const QString &path, QString *error)
{
    m_entries.clear();
    m_file.close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot open %1: %2").arg(QFileInfo(path).fileName(), m_file.errorString());
        return false;
    }
    const QString notZip = QStringLiteral("%1 is not a playlist archive").arg(QFileInfo(path).fileName());

    // The end-of-directory record is at the very end, after at most a 64 KB comment.
    const qint64 tailSize = qMin<qint64>(m_file.size(), 66000);
    m_file.seek(m_file.size() - tailSize);
    const QByteArray tail = m_file.read(tailSize);
    const qsizetype end = lastSignature(tail, endOfDirectory);
    if (end < 0 || end + 22 > tail.size()) {
        *error = notZip;
        return false;
    }
    quint64 count = u16(tail, end + 10);
    quint64 directoryOffset = u32(tail, end + 16);

    // Values too big for those fields are in the zip64 record just before. It is found
    // by its signature, not through its locator, whose offset can be wrong.
    if (count == 0xFFFF || directoryOffset == 0xFFFFFFFF) {
        const qsizetype end64 = lastSignature(tail.left(end), zip64EndOfDirectory);
        if (end64 < 0 || end64 + 56 > tail.size()) {
            *error = notZip;
            return false;
        }
        count = u64(tail, end64 + 32);
        directoryOffset = u64(tail, end64 + 48);
    }
    if (directoryOffset >= quint64(m_file.size())) {
        *error = notZip;
        return false;
    }

    m_file.seek(qint64(directoryOffset));
    for (quint64 i = 0; i < count; ++i) {
        const QByteArray fixed = m_file.read(46);
        if (fixed.size() < 46 || u32(fixed, 0) != directoryEntry)
            break;
        const int nameLength = u16(fixed, 28);
        const int extraLength = u16(fixed, 30);
        const int commentLength = u16(fixed, 32);
        const QByteArray name = m_file.read(nameLength);
        const QByteArray extra = m_file.read(extraLength);
        m_file.skip(commentLength);

        Entry entry;
        entry.name = QString::fromUtf8(name);
        entry.method = u16(fixed, 10);
        entry.crc = u32(fixed, 16);
        entry.compressedSize = u32(fixed, 20);
        entry.size = u32(fixed, 24);
        entry.headerOffset = u32(fixed, 42);

        // The zip64 extra field carries, in this order, whichever of the three did not fit.
        for (qsizetype at = 0; at + 4 <= extra.size();) {
            const int id = u16(extra, at);
            const int length = u16(extra, at + 2);
            qsizetype value = at + 4;
            if (id == 0x0001) {
                const qsizetype last = value + length;
                if (entry.size == 0xFFFFFFFF && value + 8 <= last) {
                    entry.size = u64(extra, value);
                    value += 8;
                }
                if (entry.compressedSize == 0xFFFFFFFF && value + 8 <= last) {
                    entry.compressedSize = u64(extra, value);
                    value += 8;
                }
                if (entry.headerOffset == 0xFFFFFFFF && value + 8 <= last)
                    entry.headerOffset = u64(extra, value);
            }
            at += 4 + length;
        }
        m_entries.append(entry);
    }

    if (m_entries.isEmpty()) {
        *error = notZip;
        return false;
    }
    return true;
}

bool ZipReader::read(const Entry &entry, QByteArray *data, QString *error)
{
    QBuffer buffer(data);
    buffer.open(QIODevice::WriteOnly);
    return copyOut(entry, &buffer, error);
}

bool ZipReader::extract(const Entry &entry, const QString &destination, QString *error)
{
    QDir().mkpath(QFileInfo(destination).absolutePath());
    // Written under a temporary name and renamed, so a failed copy leaves nothing behind.
    QSaveFile out(destination);
    if (!out.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("Cannot write %1: %2").arg(destination, out.errorString());
        return false;
    }
    if (!copyOut(entry, &out, error))
        return false;
    if (!out.commit()) {
        *error = QStringLiteral("Cannot write %1: %2").arg(destination, out.errorString());
        return false;
    }
    return true;
}

bool ZipReader::copyOut(const Entry &entry, QIODevice *out, QString *error)
{
    const QString damaged = QStringLiteral("%1 is damaged in the archive").arg(entry.name);
    if (entry.method != 0 && entry.method != 8) {
        *error = QStringLiteral("%1 is compressed in a way this cannot read").arg(entry.name);
        return false;
    }

    // The data follows a local header whose name and extra lengths can differ from the
    // directory's, so they are read from the header itself.
    m_file.seek(qint64(entry.headerOffset));
    const QByteArray header = m_file.read(30);
    if (header.size() < 30 || u32(header, 0) != localHeader) {
        *error = damaged;
        return false;
    }
    m_file.skip(u16(header, 26) + u16(header, 28));

    z_stream stream {};
    if (entry.method == 8 && inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        *error = damaged;
        return false;
    }

    const qint64 chunk = 1 << 20;
    QByteArray inflated(entry.method == 8 ? chunk : 0, Qt::Uninitialized);
    quint32 crc = crc32(0, nullptr, 0);
    quint64 remaining = entry.compressedSize;
    quint64 written = 0;
    bool ok = true;

    const auto emitBytes = [&](const char *bytes, qint64 length) {
        crc = crc32(crc, reinterpret_cast<const Bytef *>(bytes), uInt(length));
        written += quint64(length);
        return out->write(bytes, length) == length;
    };

    while (ok && remaining > 0) {
        const QByteArray input = m_file.read(qint64(qMin<quint64>(remaining, quint64(chunk))));
        if (input.isEmpty()) {
            ok = false;
            break;
        }
        remaining -= quint64(input.size());
        if (entry.method == 0) {
            ok = emitBytes(input.constData(), input.size());
            continue;
        }
        stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
        stream.avail_in = uInt(input.size());
        while (ok && stream.avail_in > 0) {
            stream.next_out = reinterpret_cast<Bytef *>(inflated.data());
            stream.avail_out = uInt(inflated.size());
            const int result = inflate(&stream, Z_NO_FLUSH);
            if (result != Z_OK && result != Z_STREAM_END) {
                ok = false;
                break;
            }
            ok = emitBytes(inflated.constData(), inflated.size() - qint64(stream.avail_out));
            if (result == Z_STREAM_END)
                break;
        }
    }
    if (entry.method == 8)
        inflateEnd(&stream);

    if (!ok || written != entry.size || crc != entry.crc) {
        *error = damaged;
        return false;
    }
    return true;
}
