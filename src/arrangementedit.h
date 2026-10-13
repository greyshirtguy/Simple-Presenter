#pragma once

#include "presentation.pb.h"

#include <QString>

#include <string>

// Making and changing a presentation's arrangements, as ProPresenter's arrangement
// editor does over the slides. Plain rules on the presentation as the file has it, with
// nothing of the windows in them, so that they can be tried by themselves
// (tests/unit/tst_arrangementedit). ProDocument reads the file, calls these and writes
// it back; qml/ArrangementBar.qml is what the user works them from.
//
// How a presentation keeps them (see also groupedit.h). It has a list of its groups in
// the order its slides run in, which ProPresenter calls Master. An arrangement is a
// name, an id of its own, and a list of the ids of groups: the groups to go through,
// in order, in which a group may come up several times and need not come up at all.
// The presentation also says which arrangement is selected, by id, or none, which is
// Master. Master itself is not an arrangement in the file and cannot be changed here:
// it is the order the groups are in.
//
// What is changed is the arrangement and nothing else: the groups, their slides and
// the other arrangements are as they were.
//
// GUESSES, where ProPresenter was not looked at:
//   - A new arrangement has every group once, in Master's order. (That is as it was
//     asked for. ProPresenter's help says only that one is made and groups are dragged
//     into it.)
//   - It is called "New Arrangement", and a copy of one is called by the name of the
//     one copied, each with the first number after it that makes a name no other
//     arrangement of the presentation has ("Sunday 2"), as copies are named elsewhere
//     in this app.
//   - Two arrangements of one presentation cannot be given the same name here. The
//     file could hold two, but an arrangement is chosen by its name in a playlist's row
//     and in the menus, and two of a name could not be told apart.
//   - Removing the arrangement that is selected leaves none selected: Master.
namespace arrangementedit {

enum class Outcome {
    Done,
    Unchanged,         // it is that way already: nothing is to be written
    NoSuchArrangement,
    NoSuchGroup,       // the presentation has no group of that id
    NoSuchPlace,       // the arrangement has no such place in it
    NoName,            // a name of nothing but spaces
    NameTaken,         // another arrangement of the presentation has that name
};

// `wanted`, or `wanted` with the first number after it that makes a name none of the
// presentation's arrangements has.
QString freeName(const rv::data::Presentation &presentation, const QString &wanted);

// Makes an arrangement, the last of them, of every group once in Master's order. Its
// name is `name` made free (see freeName). Returns the name it was given.
QString add(rv::data::Presentation *presentation, const QString &name, const std::string &newId);

// Makes a copy of an arrangement, straight after it, named after it. `*made` is the
// name the copy was given.
Outcome duplicate(rv::data::Presentation *presentation, const std::string &id, const std::string &newId, QString *made);

Outcome rename(rv::data::Presentation *presentation, const std::string &id, const QString &name);

// Takes an arrangement out. If it was the one selected, none is.
Outcome remove(rv::data::Presentation *presentation, const std::string &id);

// A place in an arrangement is counted from 0. A "landing" is a gap between two of its
// groups, where something dragged is let go: the place it is put before, or the number
// of places there are for after the last.

// Puts a group into an arrangement at a landing. The group may be in it already: it
// then comes up once more.
Outcome insertGroup(rv::data::Presentation *presentation, const std::string &id, const std::string &groupId, int landing);

// Moves the group at a place of an arrangement to a landing, counted with the group
// still where it is. Let go just before itself or just after, nothing changes.
Outcome moveGroup(rv::data::Presentation *presentation, const std::string &id, int from, int landing);

// Takes the group at a place out of an arrangement. (That one coming up of it: the
// group is still the presentation's, and may come up elsewhere in the arrangement.)
Outcome removeGroup(rv::data::Presentation *presentation, const std::string &id, int place);

// Where the thing at `from` among `count` is once it has been let go at `landing`:
// its new place, or -1 where that is the place it has, or there is no such thing or
// landing.
int placed(int count, int from, int landing);

} // namespace arrangementedit
