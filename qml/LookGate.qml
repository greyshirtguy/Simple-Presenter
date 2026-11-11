import QtQuick

// How much of one layer of an audience screen is on show, as the look that is live has
// it: all of it or none, going from the one to the other over the look's own time, by
// the screen's frames (a FrameFade: see there). OutputScene has one for each layer the
// looks can keep from a screen.
//
// It is also what keeps a screen from showing what it should not when the look and what
// is on the layer change together (the maintainer, 2026-11-11: with a look live that
// kept the slides from the stream, a slide whose action went over to a look that gave
// them back showed the slide before it there, dissolving away, and for a moment even
// with a cut). A slide and its look action reach a screen one after the other in one
// turn, the layer's change taking a frame or more to be made (its words painted first,
// a frame of the video waited for) and the look's none. Two rules:
//
// A layer the look gives back comes back with what is being put on it, and not with
// what it held while it was away. A change of what is on the layer that arrives while
// the layer is out of sight here is made from nothing: `arrives()` says so, and whoever
// has the layer puts what it held out of sight for good (TransitionLayer.veil). And
// the layer is not let be seen before that change is made (`landed()`), nor before the
// end of the turn in which the look gave it back, so that it makes no difference which
// of the two the screen is told first.
//
// A layer the look takes away goes with what it had. While it fades (`going`), whoever
// has the layer keeps a change of what is on it back (`keep()`), and it is made when the
// layer has gone, out of sight, or as the look gives the layer back. (For the slides and
// the announcements. The media is played by one screen for them all, so its change is
// not kept back; and the props and messages are told of theirs before the look is.)
//
// ProPresenter's own rule for a slide that changes the look is another, by its support
// article "Using Look Transitions": the slide keeps its transition and takes the look's
// length, and the lengths should be matched "to avoid unexpected layers presenting
// during the transition". That softens what this prevents; it is not done here, and
// whether ProPresenter does anything more has not been seen. The rule here is the
// maintainer's: a layer given back by what also changes it shows only what it is
// changed to.
Item {
    id: gate

    // Whether the look gives the screen the layer, and how long the layer takes to come
    // or go when that changes, in milliseconds
    property bool on: true
    property int duration: 0
    // How much of the layer is on show, from 0 to 1
    readonly property real value: fade.value
    // Out of sight here: none of it is on show
    readonly property bool unseen: fade.value <= 0
    // Being taken away: the look no longer gives the layer, and it has not gone yet
    readonly property bool going: !on && fade.value > 0
    // Whether the layer is let be seen, which is what the fade goes to: as `on` says, but
    // for a layer given back, which waits (see the top)
    property bool open: true
    property bool made: false
    // A change of what is on the layer arrived while it was out of sight, and has not
    // been made yet
    property bool arriving: false
    // What was kept back while the layer was going: a function that makes the change
    property var kept: null

    width: 0
    height: 0

    // A change of what is on the layer has been asked for. True if it is to be made
    // from nothing, the layer being out of sight; the layer then stays out of sight
    // until landed() says the change is made.
    function arrives() {
        if (!unseen)
            return false
        arriving = true
        limit.restart()
        return true
    }

    // The change that arrived has been made: the cut, or its transition begun
    function landed() {
        if (!arriving)
            return
        arriving = false
        limit.stop()
        reveal()
    }

    function reveal() {
        if (on && !arriving)
            open = true
    }

    // Keeps a change back while the layer is going; the last one asked for is the one made
    function keep(change) {
        kept = change
    }

    onOnChanged: {
        if (!made)
            return
        if (on)
            Qt.callLater(gate.reveal)
        else
            open = false
    }
    // Gone, or given back before it had: what was kept back is made now
    onGoingChanged: {
        if (going || kept === null)
            return
        const change = kept
        kept = null
        change()
    }
    Component.onCompleted: {
        open = on
        made = true
    }

    FrameFade {
        id: fade

        // (As the look has it until the gate is made, so that a layer the look keeps from
        // the screen is never shown as the screen is made)
        to: (gate.made ? gate.open : gate.on) ? 1 : 0
        duration: gate.duration
    }

    // A change that is never made (its screen drawing no frames) does not keep the layer
    // out of sight: it is let be seen a second and a half after it was asked for.
    Timer {
        id: limit

        interval: 1500
        onTriggered: gate.landed()
    }
}
