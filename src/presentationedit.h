#pragma once

#include "presentation.pb.h"

#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// A presentation's slides, groups and arrangements changed as the extended API asks
// (extendedapi.h): the changes the operator window has no one function for, or makes a
// file at a time. Plain functions on a presentation as the file has it, with nothing of
// the files or the network in them (tests/unit/tst_presentationedit); what carries them
// out reads the file, calls these and writes it back (extendedapirun.h), so that
// everything this app does not understand of the file goes back as it came.
//
// How a presentation keeps its slides (prodocument.h has the whole of it): a list of
// cues, one a slide; a list of groups, each a name, a colour and the ids of its cues,
// in the order the slides run in, which ProPresenter calls Master; and arrangements,
// each a list of the ids of groups. "The presentation's own order" here is Master: the
// groups as they are stored, each one's cues in turn. It is the order the API numbers
// slides in, whatever arrangement is selected, since an arrangement can leave a slide
// out or have it twice.
//
// What a slide is described by, where one is to be made, is the API's own form
// (extendedapi.h says it for whoever sends one): `text`, or `elements`; `label`,
// `label_color`, `notes`, `enabled`, `background`. What its cue does besides (its
// media, its Go To Next Timer, its actions) is given to the cue afterwards by the
// functions the window uses for those (ProDocument's), which whoever carries this out
// calls: so each of those is still done in one place.
//
// GUESSES, ProPresenter not having been looked at for them:
//   - A slide moved goes into the group of the slide it is put beside, and is taken out
//     of the one it was in. (ProPresenter's own dragging of a slide is taken to do so.)
//   - A group renamed keeps its id and its place in every arrangement, as one chosen
//     again from a slide's menu does (groupedit.h). Given a name the workspace's own
//     list of groups has, and no colour, it takes that group's colour and its id in the
//     list, as ProPresenter writes a group chosen from its list.
//   - A group removed is taken out of every arrangement too. (An arrangement naming a
//     group that is no more shows nothing for it here, but there is no knowing what
//     ProPresenter makes of one.)
//   - An arrangement is given its groups as a whole list: there is no other way to say
//     an order than to say it.
namespace presentationedit {

struct Context
{
    // The workspace's own groups, { id, name, color } each (GroupKeys::groups): what a
    // group named as one of them takes its colour and its id in the list from
    QVariantList groups;
    // The size of a slide made where the presentation has none to go by
    QSizeF size = QSizeF(1920, 1080);
};

// ---- finding

// The ids of the presentation's cues in its own order, and the same group by group (in
// step with its groups). A presentation with no groups has its cues in the order it
// lists them, and no groups to tell.
QStringList cueOrder(const rv::data::Presentation &presentation);
QList<QStringList> groupCues(const rv::data::Presentation &presentation);
// Which cue is meant: by its id, or by its number in that order from 0. "" for none.
QString cueId(const rv::data::Presentation &presentation, const QString &which);
rv::data::Cue *cue(rv::data::Presentation *presentation, const QString &id);
// The slide a cue shows (the first it has, as everything here reads one), or null
rv::data::PresentationSlide *slideOf(rv::data::Cue *cue);
// Which group is meant, by its id, its name or its number from 0; and which arrangement,
// by its id or its name: the place of each in the presentation's list, or -1
int groupIndex(const rv::data::Presentation &presentation, const QString &which);
int arrangementIndex(const rv::data::Presentation &presentation, const QString &which);

// ---- slides

// Where slides are put: just after a cue, or just before one, or at the end of a group
// (each by id), or with none of them at the end of the presentation
struct Where
{
    QString after;
    QString before;
    QString group;
};

// A slide described as text alone, or as a map, as a map
QVariantMap described(const QVariant &slide);

// Adds a slide for each description, in order, at that place. `*made` is given the ids
// of their cues, in step with the descriptions. Returns an error message, empty on
// success; on an error the presentation is as it was.
QString addSlides(rv::data::Presentation *presentation, const QVariantList &slides, const Where &where, const Context &context,
                  QStringList *made);
// Changes what `changes` has of a slide's label, label_color, notes, enabled,
// background, text and group (see extendedapi.h for each). Whatever else `changes` has
// is not this function's and is passed over. On an error the presentation is as it was.
QString changeSlide(rv::data::Presentation *presentation, const QString &cueId, const QVariantMap &changes, const Context &context);
// Moves a cue to a place (see the GUESSES)
QString moveCue(rv::data::Presentation *presentation, const QString &cueId, const Where &where);
// Takes a cue out of the presentation and out of every group that lists it
QString removeCue(rv::data::Presentation *presentation, const QString &cueId);

// ---- groups

// Makes a group, the last, of that name (and of the colour and the id the workspace's
// list has for the name, or of `color`), with no slides yet: its place in the list
int addGroup(rv::data::Presentation *presentation, const QString &name, const QString &color, const Context &context);
// Renames or recolours the group at `index`: `changes` may have `name` and `color`
QString changeGroup(rv::data::Presentation *presentation, int index, const QVariantMap &changes, const Context &context);
// Moves it to just before, or with `after` just after, the group at `target`
QString moveGroup(rv::data::Presentation *presentation, int index, int target, bool after);
// Takes it out, with its cues, and out of every arrangement
void removeGroup(rv::data::Presentation *presentation, int index);

// ---- arrangements

// Makes the arrangement `which` names (by id or name), or changes it: `groups`, where
// given, is the groups it goes through, each by id, name or number; `name` another name
// for it. A new one without `groups` has every group once. `*id` is the arrangement's.
QString setArrangement(rv::data::Presentation *presentation, const QString &which, const QVariantMap &changes, QString *id);
// Selects the arrangement `which` names, or none for ""
QString selectArrangement(rv::data::Presentation *presentation, const QString &which);

// ---- a presentation made

// Fills a presentation that has nothing in it yet (proconvert::newPresentation) from
// what create_presentation is given: its `groups`, each with its slides, or its
// `slides` in a group with no name, or one slide with nothing on it; its
// `arrangements` and which is selected; its `destination`, `copyright` and `key`.
// `*made` is the ids of its cues in order, and `*descriptions` what each was described
// by, for what is to be done to each cue after.
QString build(rv::data::Presentation *presentation, const QVariantMap &wanted, const Context &context, QStringList *made,
              QVariantList *descriptions);

}
