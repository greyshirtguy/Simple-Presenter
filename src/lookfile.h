#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

// The looks of a workspace, as ProPresenter's file of how the workspace is set up has
// them (Configuration/Workspace, with the screens: see screenfile.h).
//
// A look says, for each audience screen, which layers of the show that screen gets.
// One screen can have the slides over the media, as a room does, while another has the
// slides alone, to lie over a camera's picture. A look can also give a screen a theme:
// a slide of a theme (see themes.h) that every slide shown on that screen is dressed
// in as it is shown, so that the same words are large and central in the room and a
// line across the bottom of the stream, with nothing about the presentation changed.
// A workspace has as many looks as are wanted, by name.
//
// The live look is a look of its own. This is how ProPresenter has it, and it is the
// part that is easy to get wrong. The file has the saved looks in a list
// ("audience_looks"), which ProPresenter calls presets, and beside the list one more
// look ("live_audience_look"), which is what the screens are showing. Making a saved
// look live copies it into the live look, which keeps an id of its own and notes the
// id of the look it was copied from ("original_look_uuid"). After that the two go their
// own ways: the live look can be changed by itself (a layer switched off for this
// service only) without the saved look knowing, and the saved look can be changed
// without the screens changing. So "which look is live" has two answers, and both are
// kept here: what the screens get is Looks::live, and the saved look it started as is
// its `origin`. (Seen in a workspace of ProPresenter's own: a live look named for a
// saved look, with that look's id as its origin and a theme on one screen that the
// saved look has not. ProPresenter's network API likewise answers with a look whose
// id is none of the saved ones'.)
//
// ProPresenter has more layers than this app has. Each look's line for a screen is a
// set of switches, of which three are gone by here: the slides ("presentation
// foreground" in its file), the media ("presentation background") and the props. The
// others (announcements, messages, video inputs, a mask) are read, so that a look can
// be shown whole, and are never changed here: they are left in the file as they were
// found, and a look made here from nothing has them on, as ProPresenter's own new look
// does.
//
// The live look is the workspace's, in ProPresenter's file with the rest, so that the
// workspace opened in ProPresenter again shows what it was showing here. (Up to and
// including version 0.81 this app kept which look was live in its own settings, and
// wrote nothing of it to the file.)
//
// These are plain functions on a workspace folder, with nothing of the app's windows
// about them, so that they can be tested by themselves (tests/unit/tst_lookfile.cpp).
namespace lookfile {

// What a look gives one screen
struct ScreenLook
{
    // The layers this app has, which are what it goes by
    bool slide = true;
    bool media = true;
    bool props = true;
    // The theme its slides are dressed in, as the theme's place under the workspace's
    // Themes folder ("Samples/Black Box"), and which of the theme's slides, by id; ""
    // for slides as they are
    QString theme;
    QString themeSlide;
    // ProPresenter's other layers, which this app has not yet: only read, to be shown
    // (greyed) where a look is shown whole. `mask` is the id of the mask the screen is
    // given, "" for none.
    bool messages = true;
    bool announcements = true;
    bool videoInput = true;
    QString mask;

    bool operator==(const ScreenLook &other) const = default;
};

struct Look
{
    QString id;
    QString name;
    // How long going over to it takes, in seconds
    double transition = 0;
    // By the id of the screen. A screen the look says nothing of gets everything.
    QMap<QString, ScreenLook> screens;
    // Of the live look only: the saved look it was made from, by id; "" if it names none
    QString origin;
};

struct Looks
{
    // The saved looks
    QList<Look> looks;
    // The live look: what the screens get. With no id, the file has none, and every
    // screen gets everything.
    Look live;
    QString error;
};

Looks read(const QString &workspace);

// Each of these changes the file and answers with what went wrong, or nothing.
//
// A new saved look, at the end of the list. It is a copy of the saved look `copyOf`, or
// of the live look if that is liveLook(); with "" it gives each of `screenIds`
// everything.
QString add(const QString &workspace, const QString &name, const QStringList &screenIds, const QString &copyOf, QString *id);
// What stands for the live look where a look is named by id
QString liveLook();
QString rename(const QString &workspace, const QString &id, const QString &name);
QString remove(const QString &workspace, const QString &id);
// What a saved look gives a screen: its slide, media, props and theme, the rest of the
// line being left as it is. (A screen the look had nothing for is given a line.)
QString setScreen(const QString &workspace, const QString &id, const QString &screenId, const ScreenLook &wanted);
// The same for the live look, which is made if the file has none.
QString setLiveScreen(const QString &workspace, const QString &screenId, const ScreenLook &wanted);
// Makes a saved look the live one: the live look becomes a copy of it, under its own
// id, and notes where it came from. Whatever the live look had been changed to is gone.
QString makeLive(const QString &workspace, const QString &id);
// The other way: the saved look the live look was made from is given what the live look
// gives each screen, the whole of each line (what this app does not know of a line
// included). The saved look keeps its own id, name and transition. This is what Save
// does in the Looks window, once the live look has been changed.
QString saveLive(const QString &workspace);

// Where a theme is, under a workspace's Themes folder, from how a file names it: a file
// names a theme by the whole path it had on the computer that wrote it, which is some
// other computer's; what comes after ".../Themes/" is what matters here. "" if it is
// not under a Themes folder at all.
QString themePlace(const QString &named);

}
