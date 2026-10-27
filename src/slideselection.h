#pragma once

#include <QList>

// Which of the slides of the presentation being shown are selected, for the Text
// button's formats to go to (TextFormatPanel), as the maintainer asked for it
// (2026-10-27; how ProPresenter's own selection goes has not been looked at here). A
// click on a slide takes it live, as it always has, and leaves nothing else selected; a
// click with Ctrl held selects it as well as those that are, or takes it out again, and
// takes nothing live; one with Shift held selects every slide from the last one selected
// to it. The slide that is live counts as selected (the maintainer's words: "Any active
// slide is considered selected"), so that a first click with Ctrl held keeps it and adds
// the other. With nothing selected, a format goes to every slide of the presentation.
//
// Plain rules on the slides' places in the list (from 0), with nothing of the windows
// in them, tried by tests/unit/tst_slideselection; SlideSelection (slideselectionbridge.h)
// hands them to QML. What is selected is given back in the list's order, each once.
namespace slideselection {

// A click with Ctrl held on `clicked`: put in, or taken out if it was in. `active` is the
// live slide's place, -1 when none of these is live; with nothing selected yet, it is
// what the click adds to.
QList<int> toggled(const QList<int> &selected, int active, int clicked);

// A click with Shift held on `clicked`: every slide from the anchor to it put in with
// what is selected. The anchor is the last slide a click selected (`last`, -1 for none),
// or failing that the live one, or failing that the first.
QList<int> extended(const QList<int> &selected, int last, int active, int clicked);

// The slides a change to the selected ones goes to, of `count`: those selected, if any
// are; otherwise all of them
QList<int> targets(const QList<int> &selected, int count);

} // namespace slideselection
