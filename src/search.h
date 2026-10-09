#pragma once

#include "searchindex.h"

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Search: finding a presentation in the workspace's libraries by its name or its words.
//
// For the words to be searched they have to have been read, and a library can be a
// thousand songs. So the reading is done once, on another thread, when a workspace is
// opened, into a list kept here (the name and the words of every presentation), and
// done again after anything in the libraries changes, when only the files that have
// changed are read again. Searching the list (see searchindex.h) is then quick enough
// to do on every letter typed.
//
// There is one of these, which QML reaches by its name.
class Search : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether the libraries are being read; and how many presentations have been
    Q_PROPERTY(bool reading READ reading NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    using QObject::QObject;

    bool reading() const { return m_reading; }
    int count() const { return int(m_entries.size()); }

    // Reads the presentations of a workspace's Libraries folder, or reads again those
    // that have changed since they were last read.
    Q_INVOKABLE void read(const QString &librariesDirectory);

    // What is found for what was typed: { path, name, library, byName, line } each,
    // the best first.
    Q_INVOKABLE QVariantList find(const QString &typed, int limit = 80) const;

    // The words of each slide of a presentation that has been read.
    Q_INVOKABLE QStringList wordsOf(const QString &path) const;

signals:
    void changed();

private:
    QList<searchindex::Entry> m_entries;
    QString m_directory;
    bool m_reading = false;
    // Asked for again while a reading was going on: done when that one is over.
    bool m_again = false;
    int m_generation = 0;
};
