// The zip reader (src/zipreader.h), which the playlist import reads ProPresenter's exported
// playlists with: an archive laid out as a zip writer lays one out, read and copied out; and
// an entry that inflates to more than the directory says it is, which is found out as it
// goes past that and not after it has filled the memory or the disk: a damaged archive, or
// one made to do just that.

#include "zipreader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <zlib.h>

namespace {

void put16(QByteArray &out, quint16 value)
{
    char bytes[2];
    qToLittleEndian(value, bytes);
    out.append(bytes, 2);
}

void put32(QByteArray &out, quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    out.append(bytes, 4);
}

// A raw deflate stream of `data`, as a zip holds one
QByteArray deflated(const QByteArray &data)
{
    z_stream stream {};
    deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    QByteArray out(qsizetype(deflateBound(&stream, uLong(data.size()))), Qt::Uninitialized);
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = uInt(data.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = uInt(out.size());
    deflate(&stream, Z_FINISH);
    out.resize(qsizetype(stream.total_out));
    deflateEnd(&stream);
    return out;
}

quint32 crcOf(const QByteArray &data)
{
    return quint32(crc32(crc32(0, nullptr, 0), reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size())));
}

struct Member
{
    QByteArray name;
    // The bytes as the archive holds them
    QByteArray stored;
    quint32 crc;
    // What the directory says the file is
    quint32 size;
    // 0 stored, 8 deflated
    int method;
};

// An archive of these members as a zip writer lays one out: each member's local header and
// bytes, then the directory, then the record that ends it
QByteArray archive(const QList<Member> &members)
{
    QByteArray out;
    QList<quint32> offsets;
    for (const Member &member : members) {
        offsets << quint32(out.size());
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, quint16(member.method));
        put16(out, 0);
        put16(out, 0);
        put32(out, member.crc);
        put32(out, quint32(member.stored.size()));
        put32(out, member.size);
        put16(out, quint16(member.name.size()));
        put16(out, 0);
        out += member.name;
        out += member.stored;
    }
    const quint32 directory = quint32(out.size());
    for (qsizetype i = 0; i < members.size(); ++i) {
        const Member &member = members.at(i);
        put32(out, 0x02014b50);
        put16(out, 20);
        put16(out, 20);
        put16(out, 0);
        put16(out, quint16(member.method));
        put16(out, 0);
        put16(out, 0);
        put32(out, member.crc);
        put32(out, quint32(member.stored.size()));
        put32(out, member.size);
        put16(out, quint16(member.name.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, offsets.at(i));
        out += member.name;
    }
    const quint32 directorySize = quint32(out.size()) - directory;
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, quint16(members.size()));
    put16(out, quint16(members.size()));
    put32(out, directorySize);
    put32(out, directory);
    put16(out, 0);
    return out;
}

QString written(const QTemporaryDir &folder, const QString &name, const QByteArray &bytes)
{
    const QString path = folder.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(bytes);
    return path;
}

} // namespace

class TestZipReader : public QObject
{
    Q_OBJECT

private slots:
    void readsAndCopiesOutWhatTheDirectorySays()
    {
        const QByteArray words = "a playlist's data, said for a test";
        const QByteArray picture(3000, 'p');
        QTemporaryDir folder;
        const QString path = written(folder, QStringLiteral("playlist.zip"),
                                     archive({{"data", words, crcOf(words), quint32(words.size()), 0},
                                              {"Media/Pictures/one.png", deflated(picture), crcOf(picture), quint32(picture.size()), 8}}));

        ZipReader zip;
        QString error;
        QVERIFY2(zip.open(path, &error), qPrintable(error));
        QCOMPARE(zip.entries().size(), 2);
        QCOMPARE(zip.entries().at(1).name, QStringLiteral("Media/Pictures/one.png"));
        QByteArray data;
        QVERIFY2(zip.read(zip.entries().at(0), &data, &error), qPrintable(error));
        QCOMPARE(data, words);
        const QString out = folder.filePath(QStringLiteral("one.png"));
        QVERIFY2(zip.extract(zip.entries().at(1), out, &error), qPrintable(error));
        QFile made(out);
        QVERIFY(made.open(QIODevice::ReadOnly));
        QCOMPARE(made.readAll(), picture);
    }

    void anEntryThatInflatesPastWhatItSaysIsDamaged()
    {
        // Forty megabytes of one byte deflate to some forty kilobytes; the directory says
        // the file is a hundred bytes
        const QByteArray much(40 * 1024 * 1024, 'x');
        QTemporaryDir folder;
        const QString path = written(folder, QStringLiteral("bomb.zip"), archive({{"data", deflated(much), crcOf(much), 100, 8}}));

        ZipReader zip;
        QString error;
        QVERIFY2(zip.open(path, &error), qPrintable(error));
        QByteArray data;
        QVERIFY(!zip.read(zip.entries().at(0), &data, &error));
        QVERIFY2(error.contains(QStringLiteral("damaged")), qPrintable(error));
        // Given up as it went past the hundred, not at its end
        QVERIFY2(data.size() <= 2 * 1024 * 1024, qPrintable(QString::number(data.size())));
        const QString out = folder.filePath(QStringLiteral("out.bin"));
        QVERIFY(!zip.extract(zip.entries().at(0), out, &error));
        QVERIFY(!QFile::exists(out));
    }

    void anEntryWhoseBytesAreNotWhatTheySayIsDamaged()
    {
        const QByteArray words = "words";
        QTemporaryDir folder;
        // The right size, the wrong checksum
        const QString path = written(folder, QStringLiteral("wrong.zip"), archive({{"data", words, crcOf(words) ^ 1, quint32(words.size()), 0}}));
        ZipReader zip;
        QString error;
        QVERIFY2(zip.open(path, &error), qPrintable(error));
        QByteArray data;
        QVERIFY(!zip.read(zip.entries().at(0), &data, &error));
        QVERIFY2(error.contains(QStringLiteral("damaged")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestZipReader)
#include "tst_zipreader.moc"
