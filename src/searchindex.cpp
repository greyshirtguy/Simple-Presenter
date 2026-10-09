#include "searchindex.h"

#include <algorithm>

namespace searchindex {

QString folded(const QString &text)
{
    // Accents come apart from their letters, and go with the rest of what is not a
    // letter or a digit; an apostrophe goes without leaving a gap, so that "we're" is
    // "were" and is found either way.
    const QString apart = text.normalized(QString::NormalizationForm_KD).toLower();
    QString out;
    out.reserve(apart.size());
    bool gap = false;
    for (const QChar c : apart) {
        if (c.isLetterOrNumber()) {
            if (gap && !out.isEmpty())
                out += u' ';
            gap = false;
            out += c;
        } else if (c != u'\'' && c != u'’' && c.category() != QChar::Mark_NonSpacing) {
            gap = true;
        }
    }
    return out;
}

void prepare(Entry *entry)
{
    entry->nameFolded = folded(entry->name);
    QStringList lines;
    for (const QString &slide : std::as_const(entry->slides))
        lines << folded(slide);
    entry->wordsFolded = lines.join(u' ');
}

namespace {

bool hasAll(const QString &text, const QStringList &words)
{
    for (const QString &word : words) {
        if (!text.contains(word))
            return false;
    }
    return true;
}

// The first line of an entry that has what was typed: all of it together if any line
// does, else the first line with the first word.
QString lineWith(const Entry &entry, const QString &phrase, const QStringList &words)
{
    QString first;
    for (const QString &slide : entry.slides) {
        const QStringList lines = slide.split(u'\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString plain = folded(line);
            if (plain.contains(phrase))
                return line.trimmed();
            if (first.isEmpty() && plain.contains(words.first()))
                first = line.trimmed();
        }
    }
    return first;
}

}

QList<Hit> find(const QList<Entry> &entries, const QString &typed, int limit)
{
    const QString phrase = folded(typed);
    const QStringList words = phrase.split(u' ', Qt::SkipEmptyParts);
    QList<Hit> byName;
    QList<Hit> byWords;
    if (words.isEmpty())
        return byName;
    for (int i = 0; i < entries.size(); ++i) {
        const Entry &entry = entries.at(i);
        Hit hit;
        hit.entry = i;
        if (hasAll(entry.nameFolded, words)) {
            hit.byName = true;
            byName << hit;
        } else if (hasAll(entry.wordsFolded, words)) {
            hit.line = lineWith(entry, phrase, words);
            byWords << hit;
        }
    }
    const auto alphabetical = [&entries](const Hit &a, const Hit &b) {
        const int order = entries.at(a.entry).nameFolded.compare(entries.at(b.entry).nameFolded);
        return order != 0 ? order < 0 : entries.at(a.entry).path < entries.at(b.entry).path;
    };
    std::sort(byName.begin(), byName.end(), alphabetical);
    std::sort(byWords.begin(), byWords.end(), alphabetical);
    QList<Hit> all = byName + byWords;
    if (all.size() > limit)
        all.resize(limit);
    return all;
}

}
