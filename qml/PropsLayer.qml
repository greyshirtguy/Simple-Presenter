import QtQuick

// The props that are on: slides laid over everything else on the output, one over
// another in the order they were turned on, the latest in front. Each comes and goes by
// itself, with a dissolve, and stays until it is turned off.
//
// It is given the props that are on and keeps a list of its own in step with them by
// id, so that turning one prop on or off leaves the others exactly as they are, and so
// that one that has been turned off is still here to be drawn while it fades.
Item {
    id: stack

    // The props that are on, the first at the bottom: [{ id, slide }]
    property var props: []
    // How long one takes to come or go, in milliseconds
    property int duration: 0
    // As for Slide: shadows, and whether a timer's hundredths are worth following
    property bool effects: true

    // Takes away the ones that are off and have faded from sight. (All of them at one
    // go: several asking for this at once, as when the props are cleared, are answered
    // with one call.)
    function sweep() {
        for (let i = shown.count - 1; i >= 0; --i) {
            const item = items.itemAt(i)
            if (!shown.get(i).on && item !== null && item.opacity === 0)
                shown.remove(i)
        }
    }

    onPropsChanged: {
        for (let i = 0; i < shown.count; ++i) {
            if (!props.some(prop => prop.id === shown.get(i).propId))
                shown.setProperty(i, "on", false)
        }
        props.forEach((prop, place) => {
            let row = -1
            for (let i = 0; i < shown.count; ++i) {
                if (shown.get(i).propId === prop.id)
                    row = i
            }
            if (row < 0) {
                shown.append({ propId: prop.id, on: true, place: place })
            } else {
                shown.setProperty(row, "on", true)
                shown.setProperty(row, "place", place)
            }
        })
    }

    ListModel {
        id: shown
    }

    Repeater {
        id: items

        model: shown

        delegate: Slide {
            id: prop

            required property string propId
            required property bool on
            required property int place
            // Not until it has been made, so that it fades in from nothing
            property bool made: false
            // Its slide, kept from when it was last among the props that are on: it
            // is needed for as long as it takes to fade once it is not.
            property var kept: null
            readonly property var now: stack.props.find(candidate => candidate.id === propId)

            anchors.fill: parent
            z: place
            slide: kept
            effects: stack.effects
            opacity: on && made ? 1 : 0
            onNowChanged: {
                if (now)
                    kept = now.slide
            }
            Component.onCompleted: {
                if (now)
                    kept = now.slide
                made = true
            }
            // Gone from sight, and off: there is nothing left of it to draw.
            onOpacityChanged: {
                if (opacity === 0 && !on && made)
                    Qt.callLater(stack.sweep)
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: stack.duration
                }
            }
        }
    }
}
