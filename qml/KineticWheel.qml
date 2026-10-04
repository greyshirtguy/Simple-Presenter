import QtQuick

// Gives trackpad scrolling a little momentum. Declare it inside a ListView, GridView or
// other Flickable.
//
// A Flickable follows the fingers exactly and stops dead when they lift, because on
// Linux nothing sends it the coasting that macOS provides. This watches the same scroll
// events without consuming them, measures how fast the content was moving just before
// the fingers lift, and if it was still moving hands the Flickable a flick at a share of
// that speed, which then decelerates the usual way. Mouse wheels are not affected.
//
// To go back to the old behaviour, remove the `KineticWheel {}` lines (and this file).
WheelHandler {
    id: handler

    // Recent movement, as [time in ms, content pixels moved], newest last
    property var samples: []
    // Speed is averaged over this long before the lift, and never over less than the
    // shorter span: events that arrive in a burst must not look like a fast swipe.
    readonly property int window: 100
    readonly property int shortestSpan: 50
    // Lifts slower than this do not coast; faster than the cap coast at the cap.
    readonly property real minimumVelocity: 150
    readonly property real maximumVelocity: 3000
    // How much of the measured speed to coast with. Coasting distance goes with the
    // square of this: 0.55 gives roughly a third of a full-speed flick.
    readonly property real strength: 0.55
    // A handler declared inside a Flickable ends up on its content item.
    readonly property Flickable view: parent instanceof Flickable ? parent : parent.parent as Flickable

    target: null
    blocking: false
    // The default is mouse only, which leaves trackpads out.
    acceptedDevices: PointerDevice.AllDevices

    onWheel: (event) => {
        const now = Date.now()
        if (event.phase === Qt.ScrollBegin) {
            samples = []
        } else if (event.phase === Qt.ScrollUpdate) {
            samples.push([now, -event.pixelDelta.y])
            while (samples.length > 0 && now - samples[0][0] > window)
                samples.shift()
        } else if (event.phase === Qt.ScrollEnd) {
            // Only what moved shortly before the lift counts, so fingers that came to
            // rest first do not send the list off.
            const recent = samples.filter(sample => now - sample[0] <= window)
            samples = []
            if (recent.length === 0)
                return
            const moved = recent.reduce((sum, sample) => sum + sample[1], 0)
            const span = Math.max(shortestSpan, now - recent[0][0])
            const speed = Math.max(-maximumVelocity, Math.min(maximumVelocity, moved / span * 1000))
            // After the Flickable has finished with this event, so it does not undo the
            // flick. Flickable counts velocity the other way round.
            if (Math.abs(speed) >= minimumVelocity)
                Qt.callLater(() => handler.view.flick(0, -speed * handler.strength))
        }
    }
}
