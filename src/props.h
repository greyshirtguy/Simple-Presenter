#pragma once

#include "propDocument.pb.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The props of a workspace.
//
// What a prop is. A prop is a slide that is put over everything else on the output and
// stays there until it is taken off: a logo in a corner, a clock, a countdown. Several
// can be on at once, the one turned on last in front. Which are on is not kept here or
// in any file: it is the operator window's, like what is live.
//
// How ProPresenter keeps them. All of a workspace's props are in one file,
// Configuration/Props, which is what is read and written here. Each prop is a cue with
// a name and a slide. They used to be one flat list; now they are grouped into named
// collections, one level deep, each a list of the cues that are in it. A collection may
// be set to show one prop at a time, in which case turning one of its props on takes
// its others off. Props that are in no collection (a file from before there were any)
// are shown as a collection of their own, and put into one in the file when the file
// is next written.
//
// Every change reads the file, makes the one change and writes it back, as with the
// timers and the playlists. What a prop looks like is changed in the editor, which
// opens the same file (see PresentationEditor); reload() reads it again afterwards.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class Props : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The collections, in the file's order, each { id, name, single, props }, where
    // `single` is whether it shows one prop at a time and `props` is a list of
    // { id, name, slide }: the prop's cue id, its name, and its slide as a map (see
    // proconvert).
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    // How long a prop takes to come and to go, in seconds: the length of the
    // transition the file gives its props, or half a second if it gives none
    Q_PROPERTY(double transitionDuration READ transitionDuration NOTIFY changed)
    // The file, which is what the editor opens. It may not exist yet.
    Q_PROPERTY(QString path READ path NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList collections() const { return m_collections; }
    double transitionDuration() const { return m_transitionDuration; }
    QString path() const;

    // Reads the props of a workspace folder. A workspace with no props file has none.
    // Returns an error message, empty on success; a file that cannot be read is left
    // alone, and there are then no props.
    Q_INVOKABLE QString open(const QString &workspace);
    // Reads the file again, after something else has changed it.
    Q_INVOKABLE QString reload();

    // The prop with this id, as in `collections`, with `collection` (its collection's
    // id) and `single` (that collection's) beside; empty if there is none.
    Q_INVOKABLE QVariantMap find(const QString &id) const;

    // Changes to the file. Each returns an error message, empty on success, or with it
    // as `error` beside the id of what was made.
    // Adds a prop with nothing on it to a collection, or to the first if none is named.
    Q_INVOKABLE QVariantMap add(const QString &collection);
    Q_INVOKABLE QVariantMap duplicate(const QString &id);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);
    // Moves a prop to the end of another collection.
    Q_INVOKABLE QString move(const QString &id, const QString &collection);
    // Moves a prop within its collection: to just before another prop of it, or, with
    // none named, to the end.
    Q_INVOKABLE QString place(const QString &id, const QString &before);
    Q_INVOKABLE QVariantMap addCollection();
    Q_INVOKABLE QString renameCollection(const QString &id, const QString &name);
    // Removes a collection and the props that are in it.
    Q_INVOKABLE QString removeCollection(const QString &id);
    Q_INVOKABLE QString setSingle(const QString &id, bool single);

signals:
    void changed();

private:
    QString read(rv::data::PropDocument *document) const;
    QString write(rv::data::PropDocument *document);
    void show(const rv::data::PropDocument &document);

    QString m_workspace;
    QVariantList m_collections;
    double m_transitionDuration = 0;
};
