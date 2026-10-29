#include "pptxzip.h"

#include <QFileInfo>

#include <functional>

#include <zlib.h>

namespace {

void le16(QByteArray *out, quint16 v)
{
    out->append(char(v & 0xFF));
    out->append(char(v >> 8));
}

void le32(QByteArray *out, quint32 v)
{
    le16(out, quint16(v & 0xFFFF));
    le16(out, quint16(v >> 16));
}

// Raw deflate (no zlib header or trailer, as zip wants it), or empty if it could not be
// done (the caller then stores the bytes instead).
QByteArray rawDeflate(const QByteArray &data)
{
    z_stream stream {};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray out;
    out.resize(int(deflateBound(&stream, uLong(data.size()))));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = uInt(data.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = uInt(out.size());
    const int result = deflate(&stream, Z_FINISH);
    const qsizetype made = qsizetype(stream.total_out);
    deflateEnd(&stream);
    if (result != Z_STREAM_END)
        return {};
    out.resize(made);
    return out;
}

quint32 checksum(const QByteArray &data)
{
    return quint32(crc32(crc32(0, nullptr, 0), reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size())));
}

constexpr quint64 limit = 0xFFFFFFFFu;

} // namespace

bool PptxZipWriter::open(const QString &path, QString *error)
{
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = QStringLiteral("%1 could not be written: %2").arg(QFileInfo(path).fileName(), m_file.errorString());
        return false;
    }
    m_offset = 0;
    m_written.clear();
    return true;
}

bool PptxZipWriter::writeEntry(const QByteArray &name, quint16 method, quint32 crc, quint64 compressed,
                               quint64 uncompressed, const std::function<bool(QString *)> &body, QString *error)
{
    if (compressed >= limit || uncompressed >= limit || m_offset >= limit) {
        *error = QStringLiteral("%1 is too large for a PowerPoint file").arg(QString::fromUtf8(name));
        return false;
    }
    QByteArray header;
    le32(&header, 0x04034b50);
    le16(&header, 20);     // version needed: 2.0
    le16(&header, 0x0800); // flags: the name is UTF-8
    le16(&header, method);
    le16(&header, 0);      // time
    le16(&header, 0x0021); // date: 1980-01-01
    le32(&header, crc);
    le32(&header, quint32(compressed));
    le32(&header, quint32(uncompressed));
    le16(&header, quint16(name.size()));
    le16(&header, 0);      // no extra field
    header.append(name);
    if (m_file.write(header) != header.size() || !body(error)) {
        if (error->isEmpty())
            *error = QStringLiteral("The PowerPoint file could not be written: %1").arg(m_file.errorString());
        return false;
    }
    m_written.append(Written {name, method, crc, quint32(compressed), quint32(uncompressed), quint32(m_offset)});
    m_offset += quint64(header.size()) + compressed;
    return true;
}

bool PptxZipWriter::add(const QString &path, const QByteArray &data, Packing packing, QString *error)
{
    quint16 method = 0;
    QByteArray body = data;
    if (packing == Deflate && !data.isEmpty()) {
        const QByteArray packed = rawDeflate(data);
        if (!packed.isEmpty() && packed.size() < data.size()) {
            method = 8;
            body = packed;
        }
    }
    return writeEntry(path.toUtf8(), method, checksum(data), quint64(body.size()), quint64(data.size()),
                      [this, &body](QString *) { return m_file.write(body) == body.size(); }, error);
}

bool PptxZipWriter::addFile(const QString &path, const QString &source, QString *error)
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("%1 could not be read: %2").arg(QFileInfo(source).fileName(), in.errorString());
        return false;
    }
    // Once through for the checksum, which goes in the header before the bytes, and
    // once to copy, in pieces, so a video of a gigabyte is never in memory whole.
    quint32 crc = quint32(crc32(0, nullptr, 0));
    const qint64 piece = 1 << 20;
    while (!in.atEnd()) {
        const QByteArray bytes = in.read(piece);
        if (bytes.isEmpty() && in.error() != QFile::NoError)
            break;
        crc = quint32(crc32(crc, reinterpret_cast<const Bytef *>(bytes.constData()), uInt(bytes.size())));
    }
    const quint64 size = quint64(in.size());
    in.seek(0);
    return writeEntry(path.toUtf8(), 0, crc, size, size, [this, &in, piece, &source](QString *problem) {
        while (!in.atEnd()) {
            const QByteArray bytes = in.read(piece);
            if (bytes.isEmpty() && in.error() != QFile::NoError) {
                *problem = QStringLiteral("%1 could not be read: %2").arg(QFileInfo(source).fileName(), in.errorString());
                return false;
            }
            if (m_file.write(bytes) != bytes.size())
                return false;
        }
        return true;
    }, error);
}

bool PptxZipWriter::finish(QString *error)
{
    const quint64 directoryStart = m_offset;
    QByteArray directory;
    for (const Written &entry : std::as_const(m_written)) {
        le32(&directory, 0x02014b50);
        le16(&directory, 0x0314); // made by: Unix, 2.0 (so the permissions below count)
        le16(&directory, 20);
        le16(&directory, 0x0800);
        le16(&directory, entry.method);
        le16(&directory, 0);
        le16(&directory, 0x0021);
        le32(&directory, entry.crc);
        le32(&directory, entry.compressedSize);
        le32(&directory, entry.uncompressedSize);
        le16(&directory, quint16(entry.path.size()));
        le16(&directory, 0); // extra
        le16(&directory, 0); // comment
        le16(&directory, 0); // disk
        le16(&directory, 0); // internal attributes
        le32(&directory, quint32(0100644) << 16); // a plain file, readable by all
        le32(&directory, entry.offset);
        directory.append(entry.path);
    }
    if (m_written.size() >= 0xFFFF || directoryStart + quint64(directory.size()) >= limit) {
        *error = QStringLiteral("The PowerPoint file would be too large");
        m_file.close();
        return false;
    }
    QByteArray end;
    le32(&end, 0x06054b50);
    le16(&end, 0);
    le16(&end, 0);
    le16(&end, quint16(m_written.size()));
    le16(&end, quint16(m_written.size()));
    le32(&end, quint32(directory.size()));
    le32(&end, quint32(directoryStart));
    le16(&end, 0);
    const bool written = m_file.write(directory) == directory.size() && m_file.write(end) == end.size() && m_file.flush();
    if (!written)
        *error = QStringLiteral("The PowerPoint file could not be written: %1").arg(m_file.errorString());
    m_file.close();
    return written;
}
