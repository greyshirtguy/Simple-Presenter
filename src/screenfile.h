#pragma once

#include <QList>
#include <QMap>
#include <QString>

// The screens of a workspace, as ProPresenter's file of how the workspace is set up has
// them (Configuration/Workspace).
//
// A screen, to ProPresenter, is somewhere the show is drawn for: an audience screen,
// which gets the slides, the media and the props, or a stage screen, which gets a stage
// layout. It has a name and an id, and things that name a screen (a stage action, a
// look) go by those. What it is sent out through (a display, a video card, NDI) is
// part of the same file there; here that is kept apart, in the app's own settings (see
// screens.h), because it is a fact about one computer: the same workspace opened on
// another has the same screens and other things plugged in. So of this file the app
// reads and writes the list of screens, and leaves what ProPresenter says each is
// connected to exactly as it found it.
//
// The file holds a great deal more (looks, masks, audio, recording). As everywhere, a
// change is made by reading the whole file, altering what the change is about and
// writing the whole back, so the rest goes back as it came.
//
// These are plain functions on a workspace folder, with nothing of the app's windows
// about them, so that they can be tested by themselves (tests/unit/tst_screenfile.cpp).
namespace screenfile {

// How many screens a workspace may have, of both kinds together
constexpr int limit = 16;

struct Screen
{
    QString id;
    QString name;
    bool stage = false;
    // The size ProPresenter draws it at
    int width = 1920;
    int height = 1080;

    bool operator==(const Screen &other) const = default;
};

struct Screens
{
    QList<Screen> screens;
    // The stage layout each stage screen has, by the ids of both, as ProPresenter left it
    QMap<QString, QString> layouts;
    // Whether the file says what screens there are. A workspace ProPresenter has not
    // set up (or has given no screens) has the two the app has always had, which are
    // not in any file until the list is first changed.
    bool fromFile = false;
    // What went wrong reading the file, if anything did
    QString error;
};

// The screens of a workspace that has none in its file: one for the audience and one
// for the stage, with ids of their own that are the same in every such workspace.
QList<Screen> defaults();

Screens read(const QString &workspace);

// Each of these changes the file (making it, with the screens the workspace has had so
// far, if it is not there) and answers with what went wrong, or nothing.
//
// A new screen of that kind, at the end of the list, as ProPresenter writes one that is
// not connected to anything. `id` is given its id.
QString add(const QString &workspace, bool stage, const QString &name, QString *id);
// (The last audience screen cannot be removed: there would be nothing to show on.)
QString remove(const QString &workspace, const QString &id);
QString rename(const QString &workspace, const QString &id, const QString &name);

}
