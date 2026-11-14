#pragma once

#include <QObject>
#include <QPointF>
#include <QtQml/qqmlregistration.h>

// When a press becomes a drag, for every drag and drop of the operator window: a row of
// a list or of a bin, a button of the show controls, a group of the arrangement editor,
// a slide carried onto the timeline, a cue moved along it. (Not the editors' own
// handles: an element moved or sized on the slide editor's canvas, a row of its list of
// elements, a chord carried in the chord editor. Those are worked with care, away from a
// show, and keep the few pixels they had.)
//
// The maintainer asked, 2026-11-14, for "a light protection against accidental drag
// and drop", and not one that makes a drag "too hard" on a laptop's trackpad. So a press
// becomes a drag only when both of these are so, and a drag made on purpose has done
// both long before it gets anywhere:
//   - the pointer is twelve pixels from where it was pressed, straight. Moving some way
//     first is how desktops tell a drag from a click (from four pixels to ten, by the
//     desktop; this app's drags took ten, counted across and down), and this is a
//     little more, for what is clicked all through a service, where a press on a
//     trackpad can shift the pointer a few pixels;
//   - the button has been down for 150 ms. A distance does not catch a click made while
//     the hand is still moving the pointer on its way, which can cover any distance in
//     the tenth of a second a click holds the button down. That stays a click (on what
//     was pressed, if it is let go on it), and nothing is dragged.
// Nothing has to be held still, and nothing waits on a long press: a drag starts at the
// first move after the 150 ms, wherever the pointer has got to by then; and one let go
// after them, the twelve pixels away, without a move since, is dragged there and dropped
// at once (each place that starts a drag does that on its release).
//
// The time is of a steady clock (now()), not the wall clock, which a correction of the
// computer's time could move during a press.
namespace dragstart {

// How far the pointer is from where it was pressed, straight, in the window's pixels
constexpr double kDistance = 12;
// How long the button has been down, in milliseconds
constexpr double kHold = 150;

// Whether a press at `pressedAt`, with the button down `heldFor` milliseconds and the
// pointer now at `at` (both points in the same coordinates), has become a drag
bool begins(QPointF pressedAt, QPointF at, double heldFor);

// Milliseconds of a steady clock, from an arbitrary start
double now();

} // namespace dragstart

// What QML reaches the rule by. There is one of these, which QML reaches by its name: a
// press keeps `DragStart.now()`, and each move after it asks `DragStart.begins()`.
class DragStart : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE double now() const { return dragstart::now(); }
    // Whether a press at `pressedAt` at `pressedTime` (a now() of the press) has become a
    // drag, with the pointer at `at` now
    Q_INVOKABLE bool begins(QPointF pressedAt, QPointF at, double pressedTime) const
    {
        return dragstart::begins(pressedAt, at, dragstart::now() - pressedTime);
    }
};
