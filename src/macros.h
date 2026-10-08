#pragma once

#include "macros.pb.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

// The macros of a workspace.
//
// What a macro is. A named list of actions, of the kinds a slide's cue can have (see
// src/actions.h): start a timer, clear the props, give the stage a layout. Running the
// macro does them all, in order. A macro is run by hand, from the show controls, or by
// a slide, whose cue can have "run this macro" as one of its own actions; so a thing
// that many slides should do is set up once, in a macro, and changed in one place.
//
// How ProPresenter keeps them. All of a workspace's macros are in one file,
// Configuration/Macros, which is what is read and written here: the macros, and the
// collections they are sorted into, each a list of macros by their ids. Every change
// reads the file, makes the one change and writes it back, so that whatever else the
// file holds (a macro's colour and picture, actions of kinds not understood here) goes
// back as it came.
//
// What running one does is the operator window's business (it has the layers and the
// stage); this is the file.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class Macros : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The collections, in the file's order, each { id, name, macros }, where `macros`
    // is a list of { id, name, color, letter, actions }: its colour as "#rrggbb", or ""
    // if it has not been given one; the letter or digit it has for a picture, or ""
    // for any other picture (ProPresenter has a set of them, and takes one of the
    // user's own); and its actions, as src/actions.h describes them.
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList collections() const { return m_collections; }
    QString path() const;

    // Reads the macros of a workspace folder. A workspace with no macros file has
    // none. Returns an error message, empty on success; a file that cannot be read is
    // left alone, and there are then no macros.
    Q_INVOKABLE QString open(const QString &workspace);
    Q_INVOKABLE QString reload();

    // The macro with this id, or failing that the first of this name, as in
    // `collections`, with `collection` and `collectionName` (its collection's) beside;
    // empty if there is none. An action that runs a macro names it both ways.
    Q_INVOKABLE QVariantMap find(const QString &id, const QString &name = {}) const;

    // Changes to the file. Each returns an error message, empty on success, or with it
    // as `error` beside the id of what was made.
    // Adds a macro with no actions to a collection, or to the first if none is named.
    Q_INVOKABLE QVariantMap add(const QString &collection);
    Q_INVOKABLE QVariantMap duplicate(const QString &id);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    // Gives a macro a colour ("#rrggbb"), or with "" takes its colour away.
    Q_INVOKABLE QString setColor(const QString &id, const QString &color);
    Q_INVOKABLE QString remove(const QString &id);
    // Moves a macro to the end of another collection.
    Q_INVOKABLE QString move(const QString &id, const QString &collection);
    Q_INVOKABLE QVariantMap addCollection();
    Q_INVOKABLE QString renameCollection(const QString &id, const QString &name);
    // Removes a collection and the macros that are in it.
    Q_INVOKABLE QString removeCollection(const QString &id);
    // A macro's actions: gives it another, changes one by its id, takes one away.
    Q_INVOKABLE QString addAction(const QString &id, const QVariantMap &action);
    Q_INVOKABLE QString changeAction(const QString &id, const QString &actionId, const QVariantMap &action);
    Q_INVOKABLE QString removeAction(const QString &id, const QString &actionId);

signals:
    void changed();

private:
    QString read(rv::data::MacrosDocument *document) const;
    QString write(rv::data::MacrosDocument *document);
    void show(const rv::data::MacrosDocument &document);
    // Reads the file, hands the macro with this id to `change`, and writes the file
    // back unless that gave an error.
    QString changeMacro(const QString &id, const std::function<QString(rv::data::MacrosDocument::Macro *)> &change);

    QString m_workspace;
    QVariantList m_collections;
};
