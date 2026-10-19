#pragma once

#include <QList>
#include <QString>

#include <array>

// ProPresenter's transitions as its files know them: the id a file names each by (an
// Effect's `render_id`), its name, what it says it does, its category, its usual length,
// and what can be adjusted about it, with the least, the most and the value it starts
// at. Taken from the descriptions that come with ProPresenter (an .rvtx file for each),
// as mediaeffects.h has its effects.
//
// What this app draws for each, and the options it shows, are qml/TransitionCatalogue.qml,
// which knows a transition by the same id and an option by the same label; this is what a
// transition chosen there is written into a file as (builds::writeTransition), so that a
// build or a slide's transition made here is laid out as ProPresenter lays out its own.
// Five of ProPresenter's are for text alone (category "TextAnimation": Blur Reveal and
// the rest, which build a text a part at a time); the app draws none of those.
namespace transitionkinds {

struct Variable
{
    // What the file and the inspector call it, and what it says it is for
    QString name;
    QString description;
    // A number, a colour, a direction (one of nine, numbered across and down from the
    // top left), or one of a text animation's groupings or motions (by number)
    enum Kind { Float, Color, Direction, Grouping, Motion } kind = Float;
    double least = 0;
    double most = 1;
    // What it is to begin with: the number, or for a direction, grouping or motion its
    // number; a colour starts at its own (red, green, blue and opacity, from 0 to 1)
    double initial = 0;
    std::array<float, 4> initialColor {};
    // For a direction, grouping or motion: a bit for each one that can be chosen (for a
    // direction, as ProPresenter's files have it: 1 the top left, 2 the top, 16 the
    // centre, 256 the bottom right)
    int allowed = 0;
};

struct Transition
{
    QString renderId;
    QString name;
    QString description;
    QString category;
    double duration = 0.6;
    QList<Variable> variables;
};

// Every one, in the order of their names
const QList<Transition> &all();
// The one with that id, or null for one ProPresenter does not have
const Transition *find(const QString &renderId);

} // namespace transitionkinds
