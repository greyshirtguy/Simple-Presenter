#pragma once

#include "stage.pb.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The stage layouts of a workspace.
//
// What a stage layout is. The stage display is for the people on stage, and what it
// shows is laid out as a slide is: a stage layout is a slide whose text boxes are linked
// to what is going on, the words of the slide that is live, those of the one after it,
// a timer (see proconvert and SlideElement for the links). A workspace has as many
// layouts as are wanted, each with a name, and a stage screen is given one of them.
//
// How ProPresenter keeps them. All of a workspace's layouts are in one file,
// Configuration/Stage, which is what is read and written here. Which layout a screen
// has is not in that file, and here is the operator window's to remember.
//
// Every change reads the file, makes the one change and writes it back. What a layout
// looks like is changed in the editor, which opens the same file (see
// PresentationEditor); reload() reads it again afterwards.
//
// There is one of these, which QML reaches by its name; the operator window points it
// at the workspace.
class StageLayouts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // { id, name, slide } for each layout, in the file's order: the slide as a map (see
    // proconvert).
    Q_PROPERTY(QVariantList layouts READ layouts NOTIFY changed)
    // The file, which is what the editor opens. It may not exist yet.
    Q_PROPERTY(QString path READ path NOTIFY changed)
    // The stage screens ProPresenter has set up for the workspace, { id, name } each,
    // from its Configuration/Workspace file; none if there is no such file. Only what
    // they are called is taken from there: an action that gives the stage a layout
    // says which screens it is for, and one made here is to say so in ProPresenter's
    // terms. (This app has the one stage screen, which is taken to be the first.)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)

public:
    using QObject::QObject;

    QVariantList layouts() const { return m_layouts; }
    QVariantList screens() const { return m_screens; }
    QString path() const;

    // Reads the layouts of a workspace folder. A workspace with no file of them has
    // none. Returns an error message, empty on success; a file that cannot be read is
    // left alone, and there are then no layouts.
    Q_INVOKABLE QString open(const QString &workspace);
    // Reads the file again, after something else has changed it.
    Q_INVOKABLE QString reload();

    // The layout with this id, as in `layouts`; empty if there is none.
    Q_INVOKABLE QVariantMap find(const QString &id) const;

    // Changes to the file. Each returns an error message, empty on success, or with it
    // as `error` beside the id of what was made.
    // Adds a layout to start from: the words of the live slide over those of the next.
    Q_INVOKABLE QVariantMap add();
    Q_INVOKABLE QVariantMap duplicate(const QString &id);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);
    Q_INVOKABLE QString remove(const QString &id);

signals:
    void changed();

private:
    QString read(rv::data::Stage::Document *document) const;
    QString write(const rv::data::Stage::Document &document);
    void show(const rv::data::Stage::Document &document);

    QString m_workspace;
    QVariantList m_layouts;
    QVariantList m_screens;
};
