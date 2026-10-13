#pragma once

// Where an element lands when it is dragged up or down the editor's list of a slide's
// elements and let go.
//
// Two orders are in play, and they run opposite ways. A slide's file has its elements
// in the order they are drawn in, the back one first: what comes later is drawn over
// what came before, so where two of them overlap the order is which one is seen. The
// editor's list (Editor.qml) has the front one first, as ProPresenter's has: the top
// of the list is the top of the pile.
//
// A drag in the list ends between two rows, over the first or under the last. That
// place is a "landing": the row of the list the element is to be put before, counted
// from the top with the element still where it is, or the number of rows there are for
// under the last one (which is the back). There is one more landing than there are
// rows, and two of them, just over the element and just under it, leave it where it
// is.
//
// What is worked out here is that one step, from a landing to a place in the order
// drawn: it is where a place too few or too many is easily counted. Plain numbers in
// and out, so that it can be tried by itself (tests/unit/tst_elementorder.cpp);
// PresentationEditor::place() goes by it, and moves the element in the file.
namespace elementorder {

// Where in the order drawn (0 the back) the element now at `from` in that order is to
// go when it is let go at `landing`, among `count` elements. -1 when that is where it
// already is, or when there is no such element or no such landing.
int placed(int count, int from, int landing);

}
