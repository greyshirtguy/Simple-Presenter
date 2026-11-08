#include "backgroundsound.h"

#include "sessionlog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QSaveFile>
#include <QSet>

namespace {

// What backgroundsound::played answers from: the slides of the workspace open whose
// background is played with its sound, as presentation + "\n" + slide. A plain set, so
// that the question costs a reading of a presentation next to nothing.
QMutex playedLock;
QSet<QString> playedSlides;

QString keyOf(const QString &presentation, const QString &slide)
{
    return presentation + u'\n' + slide;
}

void share(const QList<backgroundsoundfile::Entry> &entries)
{
    QSet<QString> keys;
    for (const backgroundsoundfile::Entry &entry : entries)
        keys.insert(keyOf(entry.presentation, entry.slide));
    const QMutexLocker lock(&playedLock);
    playedSlides = std::move(keys);
}

} // namespace

namespace backgroundsound {

bool played(const QString &presentation, const QString &slide)
{
    if (presentation.isEmpty() || slide.isEmpty())
        return false;
    const QMutexLocker lock(&playedLock);
    return !playedSlides.isEmpty() && playedSlides.contains(keyOf(presentation, slide));
}

} // namespace backgroundsound

BackgroundSound::BackgroundSound(QObject *parent)
    : QObject(parent)
{
}

QString BackgroundSound::file() const
{
    return m_workspace.isEmpty() ? QString() : backgroundsoundfile::pathIn(m_workspace);
}

QString BackgroundSound::open(const QString &workspace)
{
    m_workspace = workspace;
    const QString error = reload();
    if (error.isEmpty() && !m_entries.isEmpty())
        SessionLog::write("media", QStringLiteral("%1 slide%2 whose background is played with its sound").arg(m_entries.size())
                                       .arg(m_entries.size() == 1 ? QString() : QStringLiteral("s")));
    return error;
}

QString BackgroundSound::reload()
{
    QString error;
    QByteArray bytes;
    QFile source(file());
    if (!file().isEmpty() && source.exists()) {
        if (source.open(QIODevice::ReadOnly))
            bytes = source.readAll();
        else
            error = QStringLiteral("Cannot read %1").arg(file());
    }
    m_entries = backgroundsoundfile::read(bytes, &error);
    share(m_entries);
    ++m_revision;
    emit changed();
    return error;
}

QString BackgroundSound::save()
{
    // The file as it is now, for what else it has
    QByteArray was;
    QFile source(file());
    if (source.exists() && source.open(QIODevice::ReadOnly))
        was = source.readAll();
    source.close();
    if (!QDir().mkpath(QFileInfo(file()).absolutePath()))
        return QStringLiteral("Cannot make the folder %1").arg(QFileInfo(file()).absolutePath());
    QSaveFile out(file());
    if (!out.open(QIODevice::WriteOnly))
        return QStringLiteral("Cannot write %1").arg(file());
    out.write(backgroundsoundfile::write(was, m_entries));
    if (!out.commit())
        return QStringLiteral("Cannot write %1").arg(file());
    share(m_entries);
    ++m_revision;
    emit changed();
    return {};
}

bool BackgroundSound::played(const QString &presentation, const QString &slide) const
{
    return backgroundsoundfile::has(m_entries, presentation, slide);
}

QString BackgroundSound::set(const QString &presentation, const QString &path, const QString &name, const QString &slide, bool on)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("There is no workspace open");
    const QString relative = QDir(m_workspace).relativeFilePath(path);
    if (relative.startsWith(QLatin1String("..")) || QFileInfo(relative).isAbsolute())
        return QStringLiteral("Only a presentation of this workspace can have a background played with its sound");
    if (presentation.isEmpty() || slide.isEmpty())
        return QStringLiteral("That slide's background cannot be played with its sound");
    const QString error = reload();
    if (!error.isEmpty())
        return error;
    // Nothing to write when it is as asked already: the file is not made, or touched,
    // for a background muted that was never played (leave nothing unneeded)
    if (backgroundsoundfile::has(m_entries, presentation, slide) == on)
        return {};
    m_entries = on ? backgroundsoundfile::with(m_entries, {presentation, relative, name, slide})
                   : backgroundsoundfile::without(m_entries, presentation, slide);
    // (The log is told by whoever asked, which knows the slide by its number)
    return save();
}
