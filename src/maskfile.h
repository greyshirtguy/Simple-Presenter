#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// The masks of a workspace, as ProPresenter keeps them.
//
// A mask is a slide that is laid over everything else an audience screen shows: the
// top layer of all. Whatever is drawn on it covers what is under it, and where it has
// nothing the output shows through, so a mask of black shapes round the edges gives a
// screen another outline, and one black box keeps a projector off a doorway. A look
// says which mask a screen gets, if any (see lookfile.h), and there is no clearing it:
// it is part of how the screen is set up, not of the show.
//
// ProPresenter keeps them in the workspace's set-up file (Configuration/Workspace,
// with the screens and the looks), each a slide and a name. That is all a mask has: it
// is known by the id of its slide, which is what a look's line for a screen names.
//
// Every change here reads the whole file, changes the masks alone, and writes the whole
// file back, so that the screens, the looks and everything else in it are as they were.
//
// GUESSES, no workspace looked at having had a mask in it:
//   - that a look names a mask by the id of the mask's slide (a mask has no other id);
//   - what a new mask is given: a slide of 1920 by 1080 with an id, nothing on it and
//     no background, and a name. ProPresenter lets a mask's size be set, to match the
//     screen it is for; here it has the size every slide made here has.
//   - that what is drawn on a mask covers the output in whatever colour it is drawn
//     in. (ProPresenter's own words: the layer "is used to mask parts of the
//     presentation, creating a custom shape or transparent areas".)
namespace maskfile {

struct Mask
{
    // The id of its slide, by which a look names it
    QString id;
    QString name;
    // Its slide, as the bytes the file has for it (an rv.data.Slide): for whoever
    // draws it. This file knows nothing of what is on a slide.
    QByteArray slide;
};

// The workspace's masks, in the file's order. A workspace with no file, or none in
// it, has none; `*error` is set if the file is there and cannot be read.
QList<Mask> read(const QString &workspace, QString *error = nullptr);

// A name none of them has: `wanted`, or that with a number after it ("Mask 2").
QString freeName(const QList<Mask> &masks, const QString &wanted);

// Each answers with what went wrong, or nothing.
//
// A new mask, last in the list, with nothing on it: `name`, or "Mask" for none, made
// one that no other mask has. `*id` is given its id.
QString add(const QString &workspace, const QString &name, QString *id);
// A name can be had by more than one mask, as far as the file goes; but not none.
QString rename(const QString &workspace, const QString &id, const QString &name);
// The mask goes from the file, and so does its name from every look's line that gave
// it to a screen, saved looks and the live look alike: a screen is not left set up
// with a mask that is not there. (A GUESS, that ProPresenter does the same.)
QString remove(const QString &workspace, const QString &id);

}
