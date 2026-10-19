#pragma once

#include "presentationSlide.pb.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

// ProPresenter's builds: a transition for one element of a slide as it comes in (its Build
// In) or goes (its Build Out), and when each runs. Read from the file, planned, and changed
// in it; plain rules, with nothing of the windows in them (tests/unit/tst_builds.cpp).
//
// What the file holds (slide.proto), as ProPresenter 21.4 writes it in the maintainer's
// files:
//  - each element's `build_in` and `build_out`: an id of the build's own, the element's id,
//    when it starts (`start`: On Click, With Previous, After Previous or With Slide; On
//    Click is the field's nought, and so is not written), a delay in seconds
//    (`delayTime`), and a transition: its length and an Effect naming it (`render_id`, as
//    transitionkinds.h has them) with its variables (a Direction, a Burn Color, a Scale).
//  - the slide's `element_build_order`: the builds' own ids, ins and outs together, in the
//    order they run, which ProPresenter's Build tab shows as a list to drag about.
//  - the slide's own transition, a Transition beside the slide in the PresentationSlide:
//    what ProPresenter's Build tab calls the slide's Build In when no element is picked.
//    ProPresenter has no Build Out for a whole slide.
//  - the text reveals (`childBuilds`, `reveal_type`: a text built a part at a time, which
//    the Build tab's Delivery sets): kept as they are, not read here.
//
// When builds run, as the maintainer describes ProPresenter, and as the file's names say:
//  - The slide comes in with its transition first (the slide's own, or the one in use),
//    which brings in every element that has no Build In; an element with one is not there
//    until it builds in, and one with only a Build Out is there from the start.
//  - Then the builds go in their order, each set going by its start and then waiting its
//    delay. One With Slide is set going as the slide's transition ends; After Previous,
//    when the build before it ends; On Click, at the next Next (or a click on the
//    slide), and the builds after it are timed from that click. With Previous goes with
//    the build before it: it is set going when that one was, its own delay counted from
//    there, not from the end of that one's delay. So in ProPresenter's lower third (a
//    name wiped out After Previous with a delay of 5 s, its underline With Previous with
//    5.2 s), the underline goes 0.2 s after the name, and not 5.2 s after it (the
//    maintainer's correction, 2026-10-19). After one that is itself With Previous, it
//    goes from when that one began: the role, With Previous after the underline with
//    no delay of its own, goes with the underline, and not as the name's delay began.
//    The slide's first step is the builds before any On Click one; each On Click starts
//    a step of its own.
//  - Next on a slide with an On Click build still to run runs it (and those timed from
//    it) instead of going to the next slide. When the next slide is shown, builds that
//    have not run are not run: the slide's transition takes everything away.
// GUESSES: that a build With Slide starts when the slide's transition ends rather than
// when it starts; that a With Previous build after another With Previous is timed from
// when that one began (the only reading under which ProPresenter's own lower third
// does not take its role away as it comes on); that With Previous and After Previous
// with no build before them are
// timed from the start of their step; that a build the order does not name runs after
// those it names, element by element, its Build In before its Build Out; and that a build
// whose transition this app does not have is a Dissolve of its length.
namespace builds {

// A transition as QML has it: { renderId, name, category, duration, values }, `values`
// being its variables by name: a number, a colour, or a direction's number (0 the top
// left to 8 the bottom right, as qml/TransitionCatalogue.qml numbers them). Empty for a
// message with no effect named in it.
QVariantMap describeTransition(const rv::data::Transition &transition);

// Writes a transition described so into the file's: its length, and its effect, kept with
// what the file has of it if it is the same transition (with the values that are named
// changed), or made afresh as ProPresenter makes one (transitionkinds.h) if it is
// another, or if `reset` is set (ProPresenter's Reset to Default Parameters: its values
// and its length as ProPresenter has them to begin with). Returns an error message,
// empty on success, in which case nothing has been changed.
QString writeTransition(rv::data::Transition *into, const QVariantMap &transition);

// The builds of a slide in the order they run: [{ id, element, out, start ("click",
// "withPrevious", "afterPrevious", "withSlide"), delay, transition }].
QVariantList describe(const rv::data::Slide &slide);

// When each build runs: the step it is in (0 the slide's own, then one for each On Click
// build), and when in the step it starts, in seconds from the step's start.
struct Planned
{
    QString id;
    QString element;
    bool out = false;
    int step = 0;
    double at = 0;
    double duration = 0;
};

struct Plan
{
    QList<Planned> builds;
    // How many On Click steps there are after the slide's own
    int steps = 0;
};

Plan plan(const rv::data::Slide &slide);

// The plan as the slide's map has it for drawing, by element: { <element id>: { inStep,
// inAt, inDuration, inTransition, outStep, outAt, outDuration, outTransition } }, the in
// or out half only for an element that has that build. Empty for a slide with none.
QVariantMap describePlan(const rv::data::Slide &slide, int *steps = nullptr);

// Changes to a slide's builds as the file has them. Each returns an error message, empty
// on success, in which case nothing has been changed.
//  - add: a Build In (or Out) for an element that has none, starting On Click with no
//    delay and the transition described (as describeTransition gives one), put at the
//    end of the order; `made` is set to the build's id.
//  - remove: the build, and its place in the order.
//  - change: { start, delay, transition } as describe() has them, any of them.
//  - move: the build to that place in the order (the builds the order did not name are
//    named in it first, where they ran).
QString add(rv::data::Slide *slide, const QString &element, bool out, const QVariantMap &transition, QString *made = nullptr);
QString remove(rv::data::Slide *slide, const QString &buildId);
QString change(rv::data::Slide *slide, const QString &buildId, const QVariantMap &changes);
QString move(rv::data::Slide *slide, const QString &buildId, int index);

// The slide's own transition (the Build tab's Build In for the whole slide): set as
// described, or taken off with an empty map.
QString setSlideTransition(rv::data::PresentationSlide *slide, const QVariantMap &transition);

// A slide that has just been dressed in a theme slide (themefile::dress) given the
// theme's builds, as it is to be shown on a screen the live look gives that theme: a
// theme slide's elements have builds as a slide's do, and a look's theme is a way to
// have a slide come on one way on one screen (a name wiped in on the stream) and plainly
// on the others. `tookLook` says, by the id of each of the slide's elements that took a
// theme text box's look, which theme element that was. Such an element takes that
// element's builds in place of any it had; one the theme brought whole has the theme's
// already; the slide's own others keep theirs. And the theme's order of builds comes
// first, then the slide's, for the builds that are still there.
// GUESSES at ProPresenter: that its looks' themes bring their builds, for the user
// uses them so, and that they then replace the slide's own builds of the text boxes
// they dress.
void takeThemeBuilds(rv::data::Slide *slide, const rv::data::Slide &theme, const QHash<QString, QString> &tookLook);

} // namespace builds
