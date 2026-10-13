#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Clear groups: named sets of layers that are cleared together, at one click. Plain
// rules on ProPresenter's file of them, with nothing of the windows in them, so that
// they can be tried by themselves (tests/unit/tst_clearfile).
//
// How ProPresenter keeps them. A workspace's Configuration/ClearGroups is a list of
// groups. Each has an id, a name, the layers it clears, a picture for its button (one
// of thirty-two ProPresenter has, by number, or a picture of the user's own, kept in
// the file), whether that picture is tinted and with what colour, whether its button
// is hidden, and two things to do with timelines and with the slide a presentation
// would go to next, which this app has nothing for and leaves as they are.
//
// One file of ProPresenter's own was looked at for this (version 20 on a Mac), and it
// held what ProPresenter starts every workspace with: one group, called "Clear All",
// with all eight layers, the picture that is called All, a white tint that is not
// switched on, both timelines, and the next slide cleared too. That is what standard()
// is, and what a workspace that has no file is taken to have: it is not written until
// something is changed, and then it is written with the change.
//
// The layers, by the names used here and as the file has them (a layer is a "target
// layer" and a "content destination"; all but the announcements are of the first alone):
//
//   audio          the audio layer
//   audioEffects   audio effects
//   messages       messages
//   props          props
//   announcements  the announcements, which are a destination of their own with no
//                  layer named, and so everything of it
//   slide          the slide
//   media          the media behind the slides, which the file calls the background
//   videoInput     video inputs, which the file calls live video
//
// That is the order ProPresenter's own group has them in, and the order they are
// offered in here. This app has six of them (the audio and the audio effects, which
// are the two halves of its audio layer, the messages, the props, the slide and the
// media); the others are ProPresenter's, are kept exactly as a group has them, and are not
// changed here. A target that is none of the eight (clearing to the logo, say) is kept
// too and is not listed.
//
// GUESSES, where a thing is made here that ProPresenter has not been seen to make:
//   - a new group. It is given a name, no layers, a picture, the white tint switched
//     off that ProPresenter's own has, and nothing else. What ProPresenter gives a new
//     group has not been looked at.
//   - the id of the group a workspace with no file is taken to have. ProPresenter's own
//     has an id, and whether that is the same in every workspace is not known from one
//     file; so the one here is this app's own, the same wherever it makes the file.
//
// Every change reads the file, changes the one thing and writes the whole of it back,
// so that what else is in it, and anything this does not know of, is kept. A change
// that changes nothing (a layer given that the group has) writes nothing.
namespace clearfile {

struct Group
{
    QString id;
    QString name;
    // The layers it clears, by the names above, in the file's order
    QStringList layers;
    // Its picture, as the file numbers them: 0 is a picture of the user's own (`image`),
    // 1 to 10 the digits 1 to 9 and 0, 11 the one called All, and so on to 32
    int icon = 0;
    QByteArray image;
    // Whether the picture is tinted, and the colour ("#rrggbb", or empty where the file
    // has none)
    bool tinted = false;
    QString tint;
    // Whether its button is kept out of sight
    bool hidden = false;
};

// The layers there are, in ProPresenter's order; and those of them this app has
QStringList layers();
QStringList ourLayers();

// The pictures ProPresenter has for a group, by number: how many (the numbers run from
// 1), and what each is of, in a word ("1", "All", "Bell")
int pictures();
QString pictureName(int icon);

// The group every workspace starts with (see above)
Group standard();

// The workspace's groups, in their order. A workspace with no file has the standard
// one, which is not an error.
QList<Group> groups(const QString &workspace, QString *error);

// Sees that the workspace has a file: one that has none is given one, holding the
// group every workspace starts with. For when something is about to name that group
// by its id (an action on a slide does), which wants it to be there under that id
// for ProPresenter to find too. Returns an error message, empty on success.
QString keep(const QString &workspace);

// Each of these changes the file and writes it back, and returns an error message,
// empty on success. A group is named by its id.
//
// A new group, last in the list, with the id given.
QString add(const QString &workspace, const QString &id, const QString &name);
QString rename(const QString &workspace, const QString &id, const QString &name);
// Whether the group clears a layer. Only the layers this app has can be changed.
QString setLayer(const QString &workspace, const QString &id, const QString &layer, bool on);
// Its picture, by number (1 to pictures()).
QString setIcon(const QString &workspace, const QString &id, int icon);
// Whether its picture is tinted. A colour ("#rrggbb") is kept as the tint; an empty one
// leaves the colour the group has.
QString setTint(const QString &workspace, const QString &id, bool tinted, const QString &color);
QString setHidden(const QString &workspace, const QString &id, bool hidden);
// Moves it up the list (by -1) or down (by 1).
QString move(const QString &workspace, const QString &id, int by);
QString remove(const QString &workspace, const QString &id);

} // namespace clearfile
