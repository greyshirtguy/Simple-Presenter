#include "playlistimport.h"

#include "zipreader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

// Where a media file from the archive goes. It is stored under the full path it had on
// the exporting machine; what is kept is the part below that machine's Media folder, so
// the folders inside Media come across, or just the name if there was no such folder.
// Never the archive's path as given: that could point anywhere.
QString mediaDestination(const QString &archivePath, const QString &root)
{
    const QStringList parts = QDir::cleanPath(archivePath).split(u'/', Qt::SkipEmptyParts);
    const qsizetype media = parts.lastIndexOf(QStringLiteral("Media"));
    QStringList kept = media >= 0 && media + 1 < parts.size() ? parts.mid(media + 1)
                                                              : QStringList {QStringLiteral("Imported"), parts.last()};
    kept.removeAll(QStringLiteral(".."));
    kept.removeAll(QStringLiteral("."));
    return QDir(root).absoluteFilePath(QStringLiteral("Media/") + kept.join(u'/'));
}

} // namespace

PlaylistImport PlaylistImport::unpack(const QString &archive, const QString &root, const QString &library,
                                      QString *error)
{
    PlaylistImport result;
    ZipReader zip;
    if (!zip.open(archive, error))
        return result;

    bool foundPlaylist = false;
    for (const ZipReader::Entry &entry : zip.entries()) {
        if (entry.name.endsWith(u'/'))
            continue;
        const QString name = QFileInfo(entry.name).fileName();

        if (entry.name == QLatin1String("data")) {
            if (!zip.read(entry, &result.playlistData, error))
                return result;
            foundPlaylist = true;
            continue;
        }

        const bool presentation = name.endsWith(QLatin1String(".pro"), Qt::CaseInsensitive);
        const QString destination = presentation ? QDir(library).absoluteFilePath(name)
                                                 : mediaDestination(entry.name, root);
        if (QFile::exists(destination)) {
            ++(presentation ? result.presentationsKept : result.mediaKept);
            continue;
        }
        if (!zip.extract(entry, destination, error))
            return result;
        ++(presentation ? result.presentations : result.media);
    }

    if (!foundPlaylist)
        *error = QStringLiteral("%1 has no playlist in it").arg(QFileInfo(archive).fileName());
    return result;
}

QString PlaylistImport::summary() const
{
    const auto counted = [](int copied, int kept, const QString &one, const QString &many) {
        QString text = QStringLiteral("%1 %2").arg(copied).arg(copied == 1 ? one : many);
        if (kept > 0)
            text += QStringLiteral(" (%1 already here, kept)").arg(kept);
        return text;
    };
    QString text = counted(presentations, presentationsKept, QStringLiteral("presentation"), QStringLiteral("presentations"));
    if (media + mediaKept > 0)
        text += QStringLiteral(" and ") + counted(media, mediaKept, QStringLiteral("media file"), QStringLiteral("media files"));
    else
        text += QStringLiteral(", no media in the archive");
    return text;
}
