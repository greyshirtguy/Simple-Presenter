// The guard on hardware video decoding (src/decodingguard.h): which graphics chip video
// would be decoded on, what is looked for of nouveau's firmware and where, and on which
// computers hardware decoding is then left untried. Tried on folders made up to look
// like a system's /dev, /sys and /lib/firmware.

#include "decodingguard.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace decodingguard;

namespace {

const QString kRelease = QStringLiteral("7.0.0-38-generic");

// A system made up in a folder of its own
struct System
{
    QTemporaryDir folder;

    QString root() const { return folder.path(); }

    static void touch(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    // A render node, the n-th, run by a kernel driver: the node itself, and what /sys
    // says of it, which is a link to the driver's folder as the real one is.
    void node(int n, const QString &driver)
    {
        const QString name = QStringLiteral("renderD%1").arg(128 + n);
        touch(root() + "/dev/dri/" + name);
        QDir().mkpath(root() + "/sys/bus/pci/drivers/" + driver);
        QDir().mkpath(root() + "/sys/class/drm/" + name + "/device");
        QVERIFY(QFile::link(root() + "/sys/bus/pci/drivers/" + driver, root() + "/sys/class/drm/" + name + "/device/driver"));
    }

    // A graphics card with no render node of its own
    void card(int n, const QString &driver)
    {
        const QString name = QStringLiteral("card%1").arg(n);
        QDir().mkpath(root() + "/sys/bus/pci/drivers/" + driver);
        QDir().mkpath(root() + "/sys/class/drm/" + name + "/device");
        QVERIFY(QFile::link(root() + "/sys/bus/pci/drivers/" + driver, root() + "/sys/class/drm/" + name + "/device/driver"));
        // (And one of its outputs, which is listed beside it and is not a card.)
        QDir().mkpath(root() + "/sys/class/drm/" + name + "-HDMI-A-1");
    }

    void firmware(const QString &name, const QString &folder = QStringLiteral("/lib/firmware"))
    {
        touch(root() + folder + "/nouveau/" + name);
    }

    Found look(int chipset) const
    {
        return decodingguard::look(root(), kRelease, [chipset](const QString &) { return chipset; });
    }
};

}

class TestDecodingGuard : public QObject
{
    Q_OBJECT

private slots:
    // ---- which graphics chip video would be decoded on

    void aSystemWithNoGraphicsChipListedIsNotGuarded()
    {
        System system;
        const Found found = system.look(0);
        QVERIFY(found.node.isEmpty() && found.driver.isEmpty());
        QVERIFY(!judge(found).guarded);
        QCOMPARE(described(found), "the system lists no graphics chip for video to be decoded on (no render node)");
    }

    void aChipOfAnotherMakersIsNotGuardedAndIsNotAskedWhatNouveauCallsIt()
    {
        System system;
        system.node(0, "i915");
        bool asked = false;
        const Found found = decodingguard::look(system.root(), kRelease, [&asked](const QString &) { asked = true; return 0xe4; });
        QCOMPARE(found.node, "/dev/dri/renderD128");
        QCOMPARE(found.driver, "i915");
        QVERIFY(!asked && found.chipset == 0);
        QVERIFY(!judge(found).guarded && judge(found).reason.isEmpty());
        QCOMPARE(described(found), "a hardware decoder would be looked for on /dev/dri/renderD128, which the driver \"i915\" runs");
    }

    void aNodeWhoseDriverTheSystemDoesNotNameIsNotGuarded()
    {
        System system;
        System::touch(system.root() + "/dev/dri/renderD128");
        const Found found = system.look(0xe4);
        QCOMPARE(found.node, "/dev/dri/renderD128");
        QVERIFY(found.driver.isEmpty() && !judge(found).guarded);
        QCOMPARE(described(found), "a hardware decoder would be looked for on /dev/dri/renderD128, whose driver the system does not name");
    }

    void theFirstRenderNodeIsTheOneThatCounts()
    {
        // A laptop with Intel graphics beside an NVIDIA chip on nouveau: video goes to
        // Intel's, which is the first.
        System laptop;
        laptop.node(0, "i915");
        laptop.node(1, "nouveau");
        QCOMPARE(laptop.look(0xe4).driver, "i915");
        QVERIFY(!judge(laptop.look(0xe4)).guarded);
        // The other way about, it is nouveau's.
        System desktop;
        desktop.node(0, "nouveau");
        desktop.node(1, "i915");
        QCOMPARE(desktop.look(0xe4).driver, "nouveau");
        QVERIFY(judge(desktop.look(0xe4)).guarded);
    }

    void aStandInIsPassedOver()
    {
        System system;
        system.node(0, "vgem");
        system.node(1, "nouveau");
        const Found found = system.look(0xe4);
        QCOMPARE(found.node, "/dev/dri/renderD129");
        QCOMPARE(found.driver, "nouveau");
        QVERIFY(judge(found).guarded);
    }

    void withNoRenderNodeAtAllACardRunByNouveauIsStillNouveaus()
    {
        // (Then which chip it is cannot be asked, and it is taken to be one with a decoder.)
        System system;
        system.card(0, "nouveau");
        const Found found = system.look(0xe4);
        QVERIFY(found.node.isEmpty());
        QCOMPARE(found.driver, "nouveau");
        QCOMPARE(found.chipset, 0);
        QVERIFY(judge(found).guarded);
        System other;
        other.card(1, "amdgpu");
        QVERIFY(!judge(other.look(0)).guarded);
    }

    // ---- nouveau, and the firmware its decoder needs

    void nouveauWithoutItsFirmwareIsGuarded()
    {
        // The computer this was made for: a GTX 670, which nouveau calls NVE4.
        System system;
        system.node(0, "nouveau");
        const Found found = system.look(0xe4);
        QCOMPARE(found.chipset, 0xe4);
        QCOMPARE(found.wanted, (QStringList {"nve4_fuc084", "nve4_fuc085"}));
        QVERIFY(!found.firmware);
        const Verdict verdict = judge(found);
        QVERIFY(verdict.guarded);
        QCOMPARE(verdict.reason, "the graphics card is run by the nouveau driver (its chip is NVE4), and the firmware that driver's video decoder needs is not installed");
        QCOMPARE(described(found), "a hardware decoder would be looked for on /dev/dri/renderD128, which the driver \"nouveau\" runs; its chip is NVE4; "
                                   "the firmware its video decoder needs is not installed (nve4_fuc084, nve4_fuc085, in /lib/firmware/nouveau)");
    }

    void withItsFirmwareInPlaceItIsNot()
    {
        System system;
        system.node(0, "nouveau");
        system.firmware("nve4_fuc084");
        system.firmware("nve4_fuc085");
        system.firmware("nve4_fuc086");
        const Found found = system.look(0xe4);
        QVERIFY(found.firmware);
        QVERIFY(!judge(found).guarded);
        QVERIFY(described(found).endsWith("its chip is NVE4; the firmware its video decoder needs is installed"));
    }

    void firmwareMayBeInTwoHalvesOrPacked()
    {
        System halves;
        halves.node(0, "nouveau");
        for (const char *name : {"nve4_fuc084c", "nve4_fuc084d", "nve4_fuc085c", "nve4_fuc085d"})
            halves.firmware(name);
        QVERIFY(halves.look(0xe4).firmware);

        System packed;
        packed.node(0, "nouveau");
        packed.firmware("nve4_fuc084.zst");
        packed.firmware("nve4_fuc085.xz");
        QVERIFY(packed.look(0xe4).firmware);

        // One half is not the whole.
        System half;
        half.node(0, "nouveau");
        half.firmware("nve4_fuc084c");
        half.firmware("nve4_fuc085c");
        half.firmware("nve4_fuc085d");
        QVERIFY(!half.look(0xe4).firmware);
        QVERIFY(judge(half.look(0xe4)).guarded);
    }

    void oneOfTheTwoPiecesOrAnotherChipsFirmwareIsNotEnough()
    {
        System one;
        one.node(0, "nouveau");
        one.firmware("nve4_fuc084");
        QVERIFY(judge(one.look(0xe4)).guarded);

        System other;
        other.node(0, "nouveau");
        other.firmware("nvc0_fuc084");
        other.firmware("nvc0_fuc085");
        QVERIFY(judge(other.look(0xe4)).guarded);
        // (For the chip it is of, it is enough.)
        QVERIFY(!judge(other.look(0xc0)).guarded);
    }

    void theKernelsOtherFirmwareFoldersCount()
    {
        for (const QString &folder : {QStringLiteral("/lib/firmware/updates"), QStringLiteral("/lib/firmware/updates/") + kRelease,
                                      QStringLiteral("/lib/firmware/") + kRelease}) {
            System system;
            system.node(0, "nouveau");
            system.firmware("nve4_fuc084", folder);
            system.firmware("nve4_fuc085", folder);
            QVERIFY2(system.look(0xe4).firmware, qPrintable(folder));
        }
        // A folder named for another kernel's release does not.
        System stale;
        stale.node(0, "nouveau");
        stale.firmware("nve4_fuc084", "/lib/firmware/6.8.0-1-generic");
        stale.firmware("nve4_fuc085", "/lib/firmware/6.8.0-1-generic");
        QVERIFY(!stale.look(0xe4).firmware);
        // And one the system has been told to look in first does.
        System told;
        told.node(0, "nouveau");
        System::touch(told.root() + "/sys/module/firmware_class/parameters/path");
        QFile path(told.root() + "/sys/module/firmware_class/parameters/path");
        QVERIFY(path.open(QIODevice::WriteOnly));
        path.write("/opt/firmware\n");
        path.close();
        told.firmware("nve4_fuc084", "/opt/firmware");
        told.firmware("nve4_fuc085", "/opt/firmware");
        QVERIFY(told.look(0xe4).firmware);
    }

    void aChipNumberedOverAHundredIsNamedInThreeFigures()
    {
        System system;
        system.node(0, "nouveau");
        QCOMPARE(system.look(0x106).wanted, (QStringList {"nv106_fuc084", "nv106_fuc085"}));
        QCOMPARE(chipName(0x106), "NV106");
        QCOMPARE(chipName(0xe4), "NVE4");
        QCOMPARE(chipName(0), "");
    }

    void theChipsWithTheOlderEngineWantOtherFiles()
    {
        System system;
        system.node(0, "nouveau");
        QCOMPARE(system.look(0x86).wanted, (QStringList {"nv84_xuc00f", "nv84_xuc103"}));
        QVERIFY(judge(system.look(0x86)).guarded);
        QVERIFY(judge(system.look(0xa0)).guarded);
        system.firmware("nv84_xuc00f");
        system.firmware("nv84_xuc103");
        QVERIFY(!judge(system.look(0x86)).guarded);
        QVERIFY(!judge(system.look(0xa0)).guarded);
        // (Which are not what a chip with the newer engine wants.)
        QVERIFY(judge(system.look(0x98)).guarded);
    }

    void aChipNouveauHasNoDecoderForIsLeftAlone()
    {
        // Older than the first with one, and newer than the last: there is no decoder
        // to go wrong, and nothing is held back.
        System system;
        system.node(0, "nouveau");
        for (const int chipset : {0x50, 0x67, 0x117, 0x124, 0x136, 0x164}) {
            const Found found = system.look(chipset);
            QVERIFY2(!judge(found).guarded, qPrintable(chipName(chipset)));
            QVERIFY(described(found).endsWith(", for which nouveau has no video decoder"));
        }
        QVERIFY(!hasDecoder(0x83) && hasDecoder(0x84) && hasDecoder(0xe4) && hasDecoder(0x108) && !hasDecoder(0x109) && !hasDecoder(0x117));
    }

    void aChipThatCouldNotBeAskedItsNumberIsTakenForOneWithADecoder()
    {
        // The careful way round: held back unless firmware of the kind is there at all.
        System system;
        system.node(0, "nouveau");
        Found found = system.look(0);
        QVERIFY(found.wanted.isEmpty() && !found.firmware);
        Verdict verdict = judge(found);
        QVERIFY(verdict.guarded);
        QCOMPARE(verdict.reason, "the graphics card is run by the nouveau driver, and the firmware that driver's video decoder needs is not installed");
        QVERIFY(described(found).contains("which chip it is could not be asked"));
        system.firmware("nvd9_fuc084");
        found = system.look(0);
        QVERIFY(found.firmware && !judge(found).guarded);
    }

    void otherFilesInThatFolderAreNotThisFirmware()
    {
        // What a system does have there: the firmware of nouveau's newer cards, in
        // folders, and nothing of the video decoder's.
        System system;
        system.node(0, "nouveau");
        System::touch(system.root() + "/lib/firmware/nouveau/README");
        QDir().mkpath(system.root() + "/lib/firmware/nouveau/nv84_xuc00f.d");
        QVERIFY(judge(system.look(0)).guarded);
        QVERIFY(judge(system.look(0xe4)).guarded);
    }

    // ---- asking the driver

    void whatIsNotAGraphicsChipHasNoNumberToGive()
    {
        QTemporaryDir folder;
        QCOMPARE(nouveauChipset(folder.path() + "/no such node"), 0);
        System::touch(folder.path() + "/a file");
        QCOMPARE(nouveauChipset(folder.path() + "/a file"), 0);
    }
};

QTEST_APPLESS_MAIN(TestDecodingGuard)
#include "tst_decodingguard.moc"
