#include "decodingguard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <drm/nouveau_drm.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace decodingguard {

namespace {

// The chips with the older of nouveau's video engines, which wants other firmware
// (see decodingguard.h)
bool olderEngine(int chipset)
{
    return (chipset >= 0x84 && chipset <= 0x96) || chipset == 0xa0;
}

// The names of the files in the folders the kernel takes nouveau's firmware from.
QSet<QString> firmwareFiles(const QString &root, const QString &release)
{
    QStringList folders;
    // A folder the system has been told to look in first, if it has been told of one
    QFile told(root + QStringLiteral("/sys/module/firmware_class/parameters/path"));
    if (told.open(QIODevice::ReadOnly)) {
        const QString path = QString::fromLocal8Bit(told.readAll()).trimmed();
        if (!path.isEmpty())
            folders << path;
    }
    if (!release.isEmpty())
        folders << QStringLiteral("/lib/firmware/updates/") + release;
    folders << QStringLiteral("/lib/firmware/updates");
    if (!release.isEmpty())
        folders << QStringLiteral("/lib/firmware/") + release;
    folders << QStringLiteral("/lib/firmware");

    QSet<QString> names;
    for (const QString &folder : std::as_const(folders)) {
        // (Files, and links to files that are there: a link to nothing is nothing the
        // kernel can load.)
        const QStringList found = QDir(root + folder + QStringLiteral("/nouveau")).entryList(QDir::Files);
        for (const QString &name : found)
            names.insert(name);
    }
    return names;
}

// Whether a file of firmware is there, as it is or packed
bool has(const QSet<QString> &files, const QString &name)
{
    return files.contains(name) || files.contains(name + QStringLiteral(".zst")) || files.contains(name + QStringLiteral(".xz"));
}

// Whether a piece of firmware is there: as one file, or as its two halves
bool whole(const QSet<QString> &files, const QString &name)
{
    return has(files, name) || (has(files, name + u'c') && has(files, name + u'd'));
}

} // namespace

QString chipName(int chipset)
{
    return chipset > 0 ? QStringLiteral("NV") + QString::number(chipset, 16).toUpper() : QString();
}

bool hasDecoder(int chipset)
{
    return chipset >= 0x84 && chipset <= 0x108;
}

Found look(const QString &root, const QString &release, const std::function<int(const QString &)> &chipsetOf)
{
    Found found;
    // The first render node there is, gone through as FFmpeg goes through them: it
    // stops at the first that is not there, and passes over one that is a stand-in.
    for (int n = 0; n < 8; ++n) {
        const QString name = QStringLiteral("renderD%1").arg(128 + n);
        const QString node = QStringLiteral("/dev/dri/") + name;
        if (!QFileInfo::exists(root + node))
            break;
        const QString driver = QFileInfo(QFileInfo(root + QStringLiteral("/sys/class/drm/") + name + QStringLiteral("/device/driver"))
                                             .symLinkTarget()).fileName();
        if (driver == QLatin1String("vgem"))
            continue;
        found.node = node;
        found.driver = driver;
        break;
    }
    // With no render node to go to, FFmpeg asks the desktop instead. The card that
    // draws the desktop is taken to be the first the system lists (a guess, for a system
    // that is hardly met with: every driver of today's gives its card a render node).
    if (found.node.isEmpty()) {
        static const QRegularExpression card(QStringLiteral("^card[0-9]+$"));
        const QStringList listed = QDir(root + QStringLiteral("/sys/class/drm")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &name : listed) {
            if (!card.match(name).hasMatch())
                continue;
            found.driver = QFileInfo(QFileInfo(root + QStringLiteral("/sys/class/drm/") + name + QStringLiteral("/device/driver"))
                                         .symLinkTarget()).fileName();
            break;
        }
    }
    if (found.driver != QLatin1String("nouveau"))
        return found;

    // (A chip can only be asked its number through a render node.)
    found.chipset = chipsetOf && !found.node.isEmpty() ? chipsetOf(root + found.node) : 0;
    const QSet<QString> files = firmwareFiles(root, release);
    if (found.chipset == 0) {
        // Which files this chip wants is not known: any of the kind is taken for it.
        static const QRegularExpression any(QStringLiteral("^nv[0-9a-f]+_(fuc084|xuc)"));
        for (const QString &name : files)
            found.firmware = found.firmware || any.match(name).hasMatch();
    } else if (olderEngine(found.chipset)) {
        found.wanted = {QStringLiteral("nv84_xuc00f"), QStringLiteral("nv84_xuc103")};
        found.firmware = has(files, found.wanted.at(0)) && has(files, found.wanted.at(1));
    } else {
        const QString chip = QStringLiteral("nv%1").arg(found.chipset, 2, 16, QLatin1Char('0'));
        found.wanted = {chip + QStringLiteral("_fuc084"), chip + QStringLiteral("_fuc085")};
        found.firmware = whole(files, found.wanted.at(0)) && whole(files, found.wanted.at(1));
    }
    return found;
}

Verdict judge(const Found &found)
{
    Verdict verdict;
    if (found.driver != QLatin1String("nouveau"))
        return verdict;
    // (A chip whose number is not known is taken to be one with a decoder: see
    // decodingguard.h for why that way round.)
    if (found.chipset != 0 && !hasDecoder(found.chipset))
        return verdict;
    if (found.firmware)
        return verdict;
    verdict.guarded = true;
    verdict.reason = QStringLiteral("the graphics card is run by the nouveau driver%1, and the firmware that driver's video decoder needs is not installed")
                         .arg(found.chipset != 0 ? QStringLiteral(" (its chip is %1)").arg(chipName(found.chipset)) : QString());
    return verdict;
}

QString described(const Found &found)
{
    if (found.node.isEmpty() && found.driver.isEmpty())
        return QStringLiteral("the system lists no graphics chip for video to be decoded on (no render node)");
    if (found.driver.isEmpty())
        return QStringLiteral("a hardware decoder would be looked for on %1, whose driver the system does not name").arg(found.node);
    QString said = found.node.isEmpty()
        ? QStringLiteral("the system lists no render node; a hardware decoder would be looked for on the graphics card, which the driver \"%1\" runs").arg(found.driver)
        : QStringLiteral("a hardware decoder would be looked for on %1, which the driver \"%2\" runs").arg(found.node, found.driver);
    if (found.driver != QLatin1String("nouveau"))
        return said;
    if (found.chipset != 0)
        said += QStringLiteral("; its chip is %1").arg(chipName(found.chipset));
    else
        said += QStringLiteral("; which chip it is could not be asked");
    if (found.chipset != 0 && !hasDecoder(found.chipset))
        return said + QStringLiteral(", for which nouveau has no video decoder");
    said += found.firmware ? QStringLiteral("; the firmware its video decoder needs is installed")
                           : QStringLiteral("; the firmware its video decoder needs is not installed");
    if (!found.firmware && !found.wanted.isEmpty())
        said += QStringLiteral(" (%1, in /lib/firmware/nouveau)").arg(found.wanted.join(QStringLiteral(", ")));
    return said;
}

int nouveauChipset(const QString &node)
{
    const int fd = ::open(QFile::encodeName(node).constData(), O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return 0;
    // (One of the questions nouveau has answered since before render nodes, and still
    // does, of a render node too: it changes nothing, and needs nothing set up first.
    // That is from the kernel's sources and its header; it could not be tried here,
    // there being no such card. If it goes unanswered the chip's number is not known,
    // which the rule allows for.)
    drm_nouveau_getparam asked {};
    asked.param = NOUVEAU_GETPARAM_CHIPSET_ID;
    const int answered = ::ioctl(fd, DRM_IOCTL_NOUVEAU_GETPARAM, &asked);
    ::close(fd);
    return answered == 0 ? int(asked.value) : 0;
}

} // namespace decodingguard
