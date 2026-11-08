#include "playlistimport.h"

#include "zipreader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

// A presentation about to be replaced by an archive's, copied first where the editor keeps
// its copies of presentations as they were (PresentationEditor::backupFolder): the app's
// data folder, Edit Backups, the workspace's name and the library's folder in it, the
// file's name with the time. An error message, empty on success.
QString backUp(const QString &file, const QString &root)
{
    const QFileInfo info(file);
    QString place = info.dir().dirName();
    const QString relative = QDir(root).relativeFilePath(info.absolutePath());
    if (!root.isEmpty() && !relative.startsWith(QLatin1String("..")))
        place = QFileInfo(root).fileName() + u'/' + relative;
    const QDir folder(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/Edit Backups/") + place);
    if (!folder.mkpath(QStringLiteral(".")))
        return QStringLiteral("Cannot make %1 to keep %2 in before it is replaced").arg(folder.path(), info.fileName());
    const QString target = folder.filePath(info.completeBaseName() + QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HH.mm.ss"))
                                           + u'.' + info.suffix());
    if (!QFile::exists(target) && !QFile::copy(file, target))
        return QStringLiteral("Cannot keep a copy of %1 before it is replaced").arg(info.fileName());
    return {};
}

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
                                      bool replace, QString *error)
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
        // (Nothing to copy out of an entry that has no file name: a folder's, or a damaged one's)
        if (name.isEmpty())
            continue;

        if (entry.name == QLatin1String("data")) {
            if (!zip.read(entry, &result.playlistData, error))
                return result;
            foundPlaylist = true;
            continue;
        }

        const bool presentation = name.endsWith(QLatin1String(".pro"), Qt::CaseInsensitive);
        const QString destination = presentation ? QDir(library).absoluteFilePath(name)
                                                 : mediaDestination(entry.name, root);
        const bool here = QFile::exists(destination);
        if (here && !(presentation && replace)) {
            ++(presentation ? result.presentationsKept : result.mediaKept);
            continue;
        }
        if (here) {
            *error = backUp(destination, root);
            if (!error->isEmpty())
                return result;
        }
        if (!zip.extract(entry, destination, error))
            return result;
        ++(here ? result.presentationsReplaced : presentation ? result.presentations : result.media);
    }

    if (!foundPlaylist)
        *error = QStringLiteral("%1 has no playlist in it").arg(QFileInfo(archive).fileName());
    return result;
}

QStringList PlaylistImport::presentationsHere(const QString &archive, const QString &library, QByteArray *playlistData, QString *error)
{
    QStringList here;
    ZipReader zip;
    if (!zip.open(archive, error))
        return here;
    for (const ZipReader::Entry &entry : zip.entries()) {
        const QString name = QFileInfo(entry.name).fileName();
        if (entry.name == QLatin1String("data")) {
            if (!zip.read(entry, playlistData, error))
                return here;
        } else if (name.endsWith(QLatin1String(".pro"), Qt::CaseInsensitive) && QFile::exists(QDir(library).absoluteFilePath(name))) {
            here << QFileInfo(name).completeBaseName();
        }
    }
    return here;
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
    if (presentationsReplaced > 0)
        text += QStringLiteral(", %1 replaced (the old kept among the editor's backups)").arg(presentationsReplaced);
    if (media + mediaKept > 0)
        text += QStringLiteral(" and ") + counted(media, mediaKept, QStringLiteral("media file"), QStringLiteral("media files"));
    else
        text += QStringLiteral(", no media in the archive");
    return text;
}
