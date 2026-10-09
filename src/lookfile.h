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
// A workspace has as many looks as are wanted, by name, and one of them is live.
//
// ProPresenter has more layers than this app has. Each look's line for a screen is a
// set of switches, of which three are gone by here: the slides ("presentation
// foreground" in its file), the media ("presentation background") and the props. The
// others (announcements, messages, video inputs, a mask) are left in the file as they
// were found, and a look made here has them on, as ProPresenter's own new look does.
//
// Which look is live is not written to the file: that is the show's business, and
// changes as a show runs (see Show). ProPresenter's note of which was live when the
// file was written is read, as where to start.
//
// These are plain functions on a workspace folder, with nothing of the app's windows
// about them, so that they can be tested by themselves (tests/unit/tst_lookfile.cpp).
namespace lookfile {

// What a look gives one screen
struct ScreenLook
{
    bool slide = true;
    bool media = true;
    bool props = true;
    // The theme its slides are dressed in, as the theme's place under the workspace's
    // Themes folder ("Samples/Black Box"), and which of the theme's slides, by id; ""
    // for slides as they are
    QString theme;
    QString themeSlide;

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
};

struct Looks
{
    QList<Look> looks;
    // The look that was live when ProPresenter wrote the file, by id; "" if it says none
    QString live;
    QString error;
};

Looks read(const QString &workspace);

// Each of these changes the file and answers with what went wrong, or nothing.
//
// A new look, at the end of the list, that gives each of these screens everything.
QString add(const QString &workspace, const QString &name, const QStringList &screenIds, QString *id);
QString rename(const QString &workspace, const QString &id, const QString &name);
QString remove(const QString &workspace, const QString &id);
// What a look gives a screen. (A screen the look had nothing for is given a line.)
QString setScreen(const QString &workspace, const QString &id, const QString &screenId, const ScreenLook &wanted);

// Where a theme is, under a workspace's Themes folder, from how a file names it: a file
// names a theme by the whole path it had on the computer that wrote it, which is some
// other computer's; what comes after ".../Themes/" is what matters here. "" if it is
// not under a Themes folder at all.
QString themePlace(const QString &named);

}
