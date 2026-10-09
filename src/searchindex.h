#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Finding presentations by their names and their words.
//
// What is searched is a list of entries, one for each presentation of the workspace's
// libraries: its name, and the words of each of its slides. The list is made by Search
// (search.h), which reads the files; this is the finding, kept apart as plain functions
// on plain values so that it can be tested by itself (tests/unit/tst_searchindex.cpp).
//
// What is typed is taken as words. A presentation is found by its name if every word
// is in the name, and by its words if every word is somewhere in them; capitals,
// punctuation and accents make no difference ("were" finds "We're", "senor" finds
// "Señor"). Those found by name come first, in the order of the alphabet, then those
// found by their words, each with the first line that has what was typed in it.
namespace searchindex {

struct Entry
{
    QString path;
    QString name;
    QString library;
    // The words of each slide, as lines
    QStringList slides;
    // When the file was last changed, for knowing whether it has to be read again
    qint64 modified = 0;
    // The name and the words as they are matched (see folded()); made by prepare()
    QString nameFolded;
    QString wordsFolded;
};

struct Hit
{
    // Which entry
    int entry = -1;
    // Found by its name, or only by its words
    bool byName = false;
    // For one found by its words: the first line that has them
    QString line;
};

// Text as it is matched: in small letters, without accents or punctuation, with single
// spaces between words.
QString folded(const QString &text);

// Fills in what an entry is matched by, from its name and its slides.
void prepare(Entry *entry);

// The entries that what was typed finds, the best first, at most `limit` of them.
// Nothing typed finds nothing.
QList<Hit> find(const QList<Entry> &entries, const QString &typed, int limit);

}
