#include "search.h"

#include "richtext.h"
#include "rtf.h"
#include "sessionlog.h"

#include "presentation.pb.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QThreadPool>

namespace {

// A presentation's name and the words of its slides, read from its file.
bool readEntry(const QString &path, searchindex::Entry *entry)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.readAll();
    rv::data::Presentation presentation;
    if (!presentation.ParseFromArray(bytes.constData(), int(bytes.size())))
        return false;
    entry->slides.clear();
    for (const rv::data::Cue &cue : presentation.cues()) {
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.has_slide() || !action.slide().has_presentation())
                continue;
            QStringList words;
            for (const rv::data::Slide::Element &element : action.slide().presentation().base_slide().elements()) {
                const std::string &rtf = element.element().text().rtf_data();
                if (rtf.empty())
                    continue;
                const QString text = parseRtf(QByteArray::fromStdString(rtf)).plainText().trimmed();
                if (!text.isEmpty())
                    words << text;
            }
            if (!words.isEmpty())
                entry->slides << words.join(u'\n');
        }
    }
    searchindex::prepare(entry);
    return true;
}

}

void Search::read(const QString &librariesDirectory)
{
    if (m_reading) {
        m_directory = librariesDirectory;
        m_again = true;
        return;
    }
    if (librariesDirectory != m_directory) {
        m_entries.clear();
        m_directory = librariesDirectory;
    }
    m_reading = true;
    m_again = false;
    emit changed();
    const int generation = ++m_generation;
    const QList<searchindex::Entry> before = m_entries;
    QPointer<Search> self(this);
    QThreadPool::globalInstance()->start([self, generation, librariesDirectory, before] {
        QElapsedTimer taken;
        taken.start();
        QHash<QString, int> had;
        for (int i = 0; i < before.size(); ++i)
            had.insert(before.at(i).path, i);
        QList<searchindex::Entry> entries;
        int readAfresh = 0;
        QDirIterator files(librariesDirectory, {QStringLiteral("*.pro")}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QFileInfo info(files.next());
            const qint64 modified = info.lastModified().toMSecsSinceEpoch();
            const int known = had.value(info.filePath(), -1);
            if (known >= 0 && before.at(known).modified == modified) {
                entries << before.at(known);
                continue;
            }
            searchindex::Entry entry;
            entry.path = info.filePath();
            entry.name = info.completeBaseName();
            entry.library = info.dir().dirName();
            entry.modified = modified;
            if (!readEntry(entry.path, &entry))
                searchindex::prepare(&entry);
            entries << entry;
            ++readAfresh;
        }
        const qint64 ms = taken.elapsed();
        QMetaObject::invokeMethod(qApp, [self, generation, entries, readAfresh, ms] {
            if (!self || generation != self->m_generation)
                return;
            self->m_entries = entries;
            self->m_reading = false;
            if (readAfresh > 0)
                SessionLog::write("search", QStringLiteral("%1 presentations can be searched; %2 read for it, in %3 ms")
                                                .arg(entries.size()).arg(readAfresh).arg(ms));
            emit self->changed();
            if (self->m_again)
                self->read(self->m_directory);
        }, Qt::QueuedConnection);
    });
}

QVariantList Search::find(const QString &typed, int limit) const
{
    QVariantList found;
    const QList<searchindex::Hit> hits = searchindex::find(m_entries, typed, limit);
    for (const searchindex::Hit &hit : hits) {
        const searchindex::Entry &entry = m_entries.at(hit.entry);
        found << QVariantMap {{"path", entry.path}, {"name", entry.name}, {"library", entry.library},
                              {"byName", hit.byName}, {"line", hit.line}};
    }
    return found;
}

QStringList Search::wordsOf(const QString &path) const
{
    for (const searchindex::Entry &entry : m_entries) {
        if (entry.path == path)
            return entry.slides;
    }
    return {};
}
