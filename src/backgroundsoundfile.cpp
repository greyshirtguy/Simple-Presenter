#include "backgroundsoundfile.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace backgroundsoundfile {

namespace {

const QString about = QStringLiteral("Simple Presenter's: slides of this workspace's presentations whose background video "
                                     "it plays with its sound (a background is silent there otherwise), by the uuid of the "
                                     "presentation and of the slide's cue. ProPresenter does not use this file.");

bool sameSlide(const Entry &entry, const QString &presentation, const QString &slide)
{
    return entry.presentation == presentation && entry.slide == slide;
}

} // namespace

QString pathIn(const QString &workspace)
{
    return QDir(workspace).filePath(QStringLiteral("SimplePresenter/Background Sound.json"));
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
            *error = QStringLiteral("The file of backgrounds played with their sound cannot be read: %1").arg(parsed.errorString());
        return entries;
    }
    for (const QJsonValue &value : document.object().value(QLatin1String("played")).toArray()) {
        const QJsonObject played = value.toObject();
        Entry entry;
        entry.presentation = played.value(QLatin1String("presentation")).toString();
        entry.path = played.value(QLatin1String("path")).toString();
        entry.name = played.value(QLatin1String("name")).toString();
        entry.slide = played.value(QLatin1String("slide")).toString();
        // (One that says too little to know its slide by is left out)
        if (entry.presentation.isEmpty() || entry.slide.isEmpty() || has(entries, entry.presentation, entry.slide))
            continue;
        entries.append(entry);
    }
    return entries;
}

QByteArray write(const QByteArray &bytes, const QList<Entry> &entries)
{
    QJsonObject file = QJsonDocument::fromJson(bytes).object();
    const QJsonArray was = file.value(QLatin1String("played")).toArray();
    QJsonArray played;
    for (const Entry &entry : entries) {
        // The slide as it was, if the file had it, for whatever else it has
        QJsonObject slide;
        for (const QJsonValue &value : was) {
            const QJsonObject old = value.toObject();
            if (old.value(QLatin1String("presentation")).toString() == entry.presentation
                && old.value(QLatin1String("slide")).toString() == entry.slide) {
                slide = old;
                break;
            }
        }
        slide.insert(QLatin1String("presentation"), entry.presentation);
        slide.insert(QLatin1String("path"), entry.path);
        slide.insert(QLatin1String("name"), entry.name);
        slide.insert(QLatin1String("slide"), entry.slide);
        played.append(slide);
    }
    file.insert(QLatin1String("about"), about);
    file.insert(QLatin1String("played"), played);
    return QJsonDocument(file).toJson(QJsonDocument::Indented);
}

QList<Entry> with(QList<Entry> entries, const Entry &entry)
{
    for (Entry &had : entries) {
        if (sameSlide(had, entry.presentation, entry.slide)) {
            had = entry;
            return entries;
        }
    }
    entries.append(entry);
    return entries;
}

QList<Entry> without(QList<Entry> entries, const QString &presentation, const QString &slide)
{
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const Entry &entry) { return sameSlide(entry, presentation, slide); }),
                  entries.end());
    return entries;
}

bool has(const QList<Entry> &entries, const QString &presentation, const QString &slide)
{
    return std::any_of(entries.begin(), entries.end(), [&](const Entry &entry) { return sameSlide(entry, presentation, slide); });
}

} // namespace backgroundsoundfile
