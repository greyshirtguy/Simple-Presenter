#include "disabledatefile.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace disabledatefile {

namespace {

const QString about = QStringLiteral("Simple Presenter's: slides of this workspace's presentations that it is to disable at a date "
                                     "and time, by the uuid of the presentation and of the slide's cue. ProPresenter does not use "
                                     "this file.");

bool sameSlide(const Entry &entry, const QString &presentation, const QString &slide)
{
    return entry.presentation == presentation && entry.slide == slide;
}

} // namespace

QString pathIn(const QString &workspace)
{
    return QDir(workspace).filePath(QStringLiteral("SimplePresenter/Disable Dates.json"));
}

QList<Entry> read(const QByteArray &bytes, QString *error)
{
    QList<Entry> entries;
    if (bytes.trimmed().isEmpty())
        return entries;
    QJsonParseError parsed;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parsed);
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("The file of disable dates cannot be read: %1").arg(parsed.errorString());
        return entries;
    }
    for (const QJsonValue &value : document.object().value(QLatin1String("dates")).toArray()) {
        const QJsonObject date = value.toObject();
        Entry entry;
        entry.presentation = date.value(QLatin1String("presentation")).toString();
        entry.path = date.value(QLatin1String("path")).toString();
        entry.name = date.value(QLatin1String("name")).toString();
        entry.slide = date.value(QLatin1String("slide")).toString();
        const QDateTime at = QDateTime::fromString(date.value(QLatin1String("at")).toString(), Qt::ISODate);
        // (One that says too little to be carried out is left out)
        if (entry.presentation.isEmpty() || entry.slide.isEmpty() || !at.isValid())
            continue;
        entry.at = at.toSecsSinceEpoch();
        entries.append(entry);
    }
    return entries;
}

QByteArray write(const QByteArray &bytes, const QList<Entry> &entries, const QTimeZone &zone)
{
    QJsonObject file = QJsonDocument::fromJson(bytes).object();
    const QJsonArray was = file.value(QLatin1String("dates")).toArray();
    QJsonArray dates;
    for (const Entry &entry : entries) {
        // The date as it was, if the file had it, for whatever else it has
        QJsonObject date;
        for (const QJsonValue &value : was) {
            const QJsonObject old = value.toObject();
            if (old.value(QLatin1String("presentation")).toString() == entry.presentation
                && old.value(QLatin1String("slide")).toString() == entry.slide) {
                date = old;
                break;
            }
        }
        date.insert(QLatin1String("presentation"), entry.presentation);
        date.insert(QLatin1String("path"), entry.path);
        date.insert(QLatin1String("name"), entry.name);
        date.insert(QLatin1String("slide"), entry.slide);
        date.insert(QLatin1String("at"), QDateTime::fromSecsSinceEpoch(entry.at, zone).toString(Qt::ISODate));
        dates.append(date);
    }
    if (!file.contains(QLatin1String("about")))
        file.insert(QLatin1String("about"), about);
    file.insert(QLatin1String("dates"), dates);
    return QJsonDocument(file).toJson(QJsonDocument::Indented);
}

QList<Entry> with(QList<Entry> entries, const Entry &entry)
{
    for (Entry &kept : entries) {
        if (sameSlide(kept, entry.presentation, entry.slide)) {
            kept = entry;
            return entries;
        }
    }
    entries.append(entry);
    return entries;
}

QList<Entry> without(QList<Entry> entries, const QString &presentation, const QString &slide)
{
    entries.removeIf([&](const Entry &entry) { return sameSlide(entry, presentation, slide); });
    return entries;
}

long long dateOf(const QList<Entry> &entries, const QString &presentation, const QString &slide)
{
    for (const Entry &entry : entries) {
        if (sameSlide(entry, presentation, slide))
            return entry.at;
    }
    return 0;
}

QList<Entry> due(const QList<Entry> &entries, long long now)
{
    QList<Entry> found;
    for (const Entry &entry : entries) {
        if (entry.at <= now)
            found.append(entry);
    }
    std::stable_sort(found.begin(), found.end(), [](const Entry &a, const Entry &b) { return a.at < b.at; });
    return found;
}

long long next(const QList<Entry> &entries, long long now)
{
    long long first = -1;
    for (const Entry &entry : entries) {
        if (entry.at > now && (first < 0 || entry.at < first))
            first = entry.at;
    }
    return first;
}

} // namespace disabledatefile
