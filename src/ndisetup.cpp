#include "ndisetup.h"

namespace ndisetup {

Installer read(const QByteArray &script)
{
    Installer found;
    // The licence: what the script hands its pager, from the line after "<< NDI_EULA_END"
    // to the line that says NDI_EULA_END by itself.
    const QByteArray opens = QByteArrayLiteral("<< NDI_EULA_END\n");
    const QByteArray closes = QByteArrayLiteral("\nNDI_EULA_END\n");
    const qsizetype from = script.indexOf(opens);
    const qsizetype to = from < 0 ? -1 : script.indexOf(closes, from);
    if (from >= 0 && to > from)
        found.licence = QString::fromUtf8(script.mid(from + opens.size(), to - from - opens.size())).trimmed();
    // The archive: everything after the line that is the mark and nothing else. (The
    // script names the mark once more, where it looks for it, but not on a line of its own.)
    const QByteArray mark = QByteArrayLiteral("\n__NDI_ARCHIVE_BEGIN__\n");
    const qsizetype at = script.indexOf(mark);
    if (at >= 0 && at + mark.size() < script.size())
        found.archiveAt = at + mark.size();
    return found;
}

QString libraryFolder(const QString &processor)
{
    if (processor == QLatin1String("x86_64"))
        return QStringLiteral("x86_64-linux-gnu");
    if (processor == QLatin1String("i386"))
        return QStringLiteral("i686-linux-gnu");
    if (processor == QLatin1String("arm64"))
        return QStringLiteral("aarch64-rpi4-linux-gnueabi");
    return {};
}

}
