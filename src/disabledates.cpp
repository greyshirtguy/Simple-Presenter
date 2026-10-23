#include "disabledates.h"

#include "calendarfile.h"
#include "proconvert.h"
#include "prodocument.h"
#include "sessionlog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QTimeZone>

#include <algorithm>

namespace {

long long nowSeconds()
{
    return QDateTime::currentSecsSinceEpoch();
}

QString when(long long at)
{
    return calendarfile::whenText(at, QTimeZone::systemTimeZone(), QLocale());
}

QString quoted(const QString &name)
{
    return QStringLiteral("\"%1\"").arg(name);
}

} // namespace

DisableDates::DisableDates(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &DisableDates::look);
}

QString DisableDates::file() const
{
    return m_workspace.isEmpty() ? QString() : disabledatefile::pathIn(m_workspace);
}

QString DisableDates::open(const QString &workspace)
{
    m_workspace = workspace;
    m_said.clear();
    const QString error = reload();
    if (error.isEmpty() && !m_entries.isEmpty())
        SessionLog::write("dates", QStringLiteral("%1 slide%2 to be disabled at a date").arg(m_entries.size())
                                       .arg(m_entries.size() == 1 ? QString() : QStringLiteral("s")));
    look();
    return error;
}

QString DisableDates::reload()
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
    m_entries = disabledatefile::read(bytes, &error);
    ++m_revision;
    emit changed();
    return error;
}

QString DisableDates::save()
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
    out.write(disabledatefile::write(was, m_entries, QTimeZone::systemTimeZone()));
    if (!out.commit())
        return QStringLiteral("Cannot write %1").arg(file());
    ++m_revision;
    emit changed();
    return {};
}

double DisableDates::dateOf(const QString &presentation, const QString &slide) const
{
    return double(disabledatefile::dateOf(m_entries, presentation, slide));
}

QVariantMap DisableDates::datesOf(const QString &presentation) const
{
    QVariantMap dates;
    for (const disabledatefile::Entry &entry : m_entries) {
        if (entry.presentation == presentation)
            dates.insert(entry.slide, double(entry.at));
    }
    return dates;
}

QString DisableDates::set(const QString &presentation, const QString &path, const QString &name, const QString &slide, double at)
{
    if (m_workspace.isEmpty())
        return QStringLiteral("There is no workspace open");
    const QString relative = QDir(m_workspace).relativeFilePath(path);
    if (relative.startsWith(QLatin1String("..")) || QFileInfo(relative).isAbsolute())
        return QStringLiteral("Only a presentation of this workspace can have a slide disabled at a date");
    if (presentation.isEmpty() || slide.isEmpty())
        return QStringLiteral("That slide cannot be given a date");
    const QString error = reload();
    if (!error.isEmpty())
        return error;
    const disabledatefile::Entry entry {presentation, relative, name, slide, (long long)at};
    m_entries = disabledatefile::with(m_entries, entry);
    m_said.remove(presentation + slide);
    const QString saved = save();
    if (!saved.isEmpty())
        return saved;
    SessionLog::write("dates", QStringLiteral("a slide of %1 to be disabled on %2").arg(quoted(name), when((long long)at)));
    look();
    return {};
}

QString DisableDates::clear(const QString &presentation, const QString &slide)
{
    if (m_workspace.isEmpty())
        return {};
    const QString error = reload();
    if (!error.isEmpty())
        return error;
    if (disabledatefile::dateOf(m_entries, presentation, slide) == 0)
        return {};
    m_entries = disabledatefile::without(m_entries, presentation, slide);
    const QString saved = save();
    if (saved.isEmpty())
        SessionLog::write("dates", QStringLiteral("a slide's disable date taken off"));
    schedule();
    return saved;
}

void DisableDates::schedule()
{
    m_timer.stop();
    const long long now = nowSeconds();
    const long long next = disabledatefile::next(m_entries, now);
    if (next < 0)
        return;
    // To the second it comes (and a little after, so that it has come when looked at), and
    // in any case within the hour
    const long long wait = std::clamp<long long>((next - now) * 1000 + 50, 50, 3600LL * 1000);
    m_timer.start(int(wait));
}

void DisableDates::look()
{
    bool done = false;
    for (const disabledatefile::Entry &entry : disabledatefile::due(m_entries, nowSeconds())) {
        if (carryOut(entry)) {
            m_entries = disabledatefile::without(m_entries, entry.presentation, entry.slide);
            done = true;
        }
    }
    if (done) {
        const QString saved = save();
        if (!saved.isEmpty())
            SessionLog::write("dates", saved);
    }
    schedule();
}

bool DisableDates::carryOut(const disabledatefile::Entry &entry)
{
    const QString path = QDir(m_workspace).filePath(entry.path);
    const QString key = entry.presentation + entry.slide;
    // What is where the presentation was is to be that presentation still
    rv::data::Presentation presentation;
    QString error;
    if (!QFile::exists(path) || !proconvert::readPresentation(path, &presentation, &error)
        || QString::fromStdString(presentation.uuid().string()) != entry.presentation) {
        if (!m_said.contains(key)) {
            m_said.insert(key);
            SessionLog::write("dates", QStringLiteral("PROBLEM: a slide of %1 was to be disabled on %2, but that presentation is not "
                                                      "at %3 any more; the date is kept, and tried again when the workspace is next opened")
                                           .arg(quoted(entry.name), when(entry.at), entry.path));
        }
        return false;
    }
    error = ProDocument::setCueEnabled(path, entry.slide, false);
    if (!error.isEmpty()) {
        if (!m_said.contains(key)) {
            m_said.insert(key);
            SessionLog::write("dates", QStringLiteral("PROBLEM: a slide of %1 was to be disabled on %2, which failed: %3")
                                           .arg(quoted(entry.name), when(entry.at), error));
        }
        return false;
    }
    SessionLog::write("dates", QStringLiteral("a slide of %1 disabled, its date (%2) having come").arg(quoted(entry.name), when(entry.at)));
    emit disabled(path, entry.slide);
    return true;
}
