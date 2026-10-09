// Reading NDI's installer (src/ndisetup.h): finding the licence it shows and the archive
// it carries, in scripts made up here to be laid out as NDI's is, and not.

#include "ndisetup.h"

#include <QTest>

using namespace ndisetup;

namespace {

QByteArray script(const QByteArray &licence, const QByteArray &archive)
{
    return "#!/bin/sh\n\n[ -n \"$PAGER\" ] || PAGER=more\n\n$view_eula << NDI_EULA_END\n" + licence + "\nNDI_EULA_END\n\n"
           "read -p \"Type y or Y to agree: \" REPLY\n"
           "ARCHIVE=`awk '/^__NDI_ARCHIVE_BEGIN__/ { print NR+1; exit 0; }' \"$0\"`\ntail -n+$ARCHIVE \"$0\" | tar xvz\n\nexit 0\n\n"
           "__NDI_ARCHIVE_BEGIN__\n" + archive;
}

}

class TestNdiSetup : public QObject
{
    Q_OBJECT

private slots:
    void theLicenceAndTheArchiveAreFound()
    {
        const QByteArray archive("\x1f\x8b\x08\x00 not really an archive \n__NDI_ARCHIVE_BEGIN__\n with the mark in it again", 71);
        const QByteArray whole = script("NDI SDK License Agreement\nPlease read this.\n\n1. DEFINITIONS\nand so on", archive);
        const Installer found = read(whole);
        QVERIFY(found.understood());
        QCOMPARE(found.licence, "NDI SDK License Agreement\nPlease read this.\n\n1. DEFINITIONS\nand so on");
        // From the first byte after the line of the mark, whatever the archive holds
        QCOMPARE(whole.mid(found.archiveAt), archive);
    }

    void theLicenceIsReadAsItIsWritten()
    {
        const Installer found = read(script("NDI\xc2\xae is a trademark", "x"));
        QCOMPARE(found.licence, QString::fromUtf8("NDI\xc2\xae is a trademark"));
    }

    void aScriptLaidOutSomeOtherWayIsNotUnderstood()
    {
        QVERIFY(!read("").understood());
        QVERIFY(!read("#!/bin/sh\necho hello\n").understood());
        // The licence and no archive; an archive and no licence; the mark with nothing after it
        QVERIFY(!read("$view_eula << NDI_EULA_END\nwords\nNDI_EULA_END\n").understood());
        QVERIFY(!read("#!/bin/sh\n\n__NDI_ARCHIVE_BEGIN__\nbytes").understood());
        QVERIFY(!read("$view_eula << NDI_EULA_END\nwords\nNDI_EULA_END\n\n__NDI_ARCHIVE_BEGIN__\n").understood());
        // A page that is not the script at all, as a download that went somewhere else gives
        QVERIFY(!read("<html><body>Please sign in</body></html>").understood());
    }

    void theLibraryIsLookedForByProcessor()
    {
        QCOMPARE(libraryFolder("x86_64"), "x86_64-linux-gnu");
        QCOMPARE(libraryFolder("arm64"), "aarch64-rpi4-linux-gnueabi");
        QCOMPARE(libraryFolder("riscv64"), "");
    }
};

QTEST_APPLESS_MAIN(TestNdiSetup)
#include "tst_ndisetup.moc"
