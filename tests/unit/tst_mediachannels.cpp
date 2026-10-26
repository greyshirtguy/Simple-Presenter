// How many channels of sound a file has (src/mediachannels.h), read from files made for
// the purpose: WAV files of two and of six channels (the second with its layout said, as
// a surround file says it), a file that is not media, and one that is not there.

#include "mediachannels.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A WAV file of a tenth of a second of silence, 16 bits, 48 kHz; with `mask` written as
// a WAVE_FORMAT_EXTENSIBLE file's channel mask (its layout), or a plain WAV without
void writeWav(const QString &path, int channels, quint32 mask = 0)
{
    const quint32 frames = 4800;
    const quint32 data = frames * channels * 2;
    QByteArray bytes;
    const auto u16 = [&](quint16 v) { bytes.append(char(v & 0xff)).append(char(v >> 8)); };
    const auto u32 = [&](quint32 v) { u16(quint16(v & 0xffff)); u16(quint16(v >> 16)); };
    const bool extensible = mask != 0;
    bytes.append("RIFF");
    u32(4 + 8 + (extensible ? 40 : 16) + 8 + data);
    bytes.append("WAVEfmt ");
    u32(extensible ? 40 : 16);
    u16(extensible ? 0xFFFE : 1);
    u16(quint16(channels));
    u32(48000);
    u32(48000 * channels * 2);
    u16(quint16(channels * 2));
    u16(16);
    if (extensible) {
        u16(22);
        u16(16);
        u32(mask);
        // KSDATAFORMAT_SUBTYPE_PCM
        const unsigned char pcm[16] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
        bytes.append(reinterpret_cast<const char *>(pcm), 16);
    }
    bytes.append("data");
    u32(data);
    bytes.append(QByteArray(int(data), '\0'));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

} // namespace

class TestMediaChannels : public QObject
{
    Q_OBJECT

private slots:
    void read()
    {
        QTemporaryDir folder;
        writeWav(folder.filePath("stereo.wav"), 2);
        // Front left and right, centre, low frequency, back left and right: 5.1
        writeWav(folder.filePath("surround.wav"), 6, 0x3F);
        QFile text(folder.filePath("words.txt"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("Not media at all, only words.\n");
        text.close();

        QCOMPARE(mediachannels::read(folder.filePath("stereo.wav")).channels, 2);
        const mediachannels::Found surround = mediachannels::read(folder.filePath("surround.wav"));
        QCOMPARE(surround.channels, 6);
        QCOMPARE(surround.names, (QStringList {"FL", "FR", "FC", "LFE", "BL", "BR"}));
        QCOMPARE(mediachannels::read(folder.filePath("words.txt")).channels, 0);
        QCOMPARE(mediachannels::read(folder.filePath("not-there.wav")).channels, 0);
        QCOMPARE(mediachannels::read(QString()).channels, 0);
    }

    void readOffTheThread()
    {
        QTemporaryDir folder;
        writeWav(folder.filePath("surround.wav"), 6, 0x3F);
        MediaChannels probe;
        QCOMPARE(probe.channels(), -1);
        QSignalSpy found(&probe, &MediaChannels::found);
        probe.setPath(folder.filePath("surround.wav"));
        QCOMPARE(probe.channels(), -1);
        QTRY_COMPARE(probe.channels(), 6);
        QCOMPARE(probe.names().size(), 6);
        // Another file asked for before the first is read: the first's answer is not taken
        writeWav(folder.filePath("stereo.wav"), 2);
        probe.setPath(folder.filePath("surround.wav") + "x");
        probe.setPath(folder.filePath("stereo.wav"));
        QTRY_COMPARE(probe.channels(), 2);
        QTest::qWait(100);
        QCOMPARE(probe.channels(), 2);
    }
};

QTEST_GUILESS_MAIN(TestMediaChannels)
#include "tst_mediachannels.moc"
