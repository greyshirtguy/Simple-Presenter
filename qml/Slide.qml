import QtQuick
import SimplePresenterApp

// One slide, transparent wherever it has no content. Slides are authored against their
// own size (typically 1920x1080) and drawn in units of the output's height over that,
// so everything is rendered at the output's real resolution.
//
// This one component draws a slide everywhere a slide appears: on the output, in the
// thumbnails of the grid, in the preview, and in the editor's list. There is no second,
// simplified renderer for the small ones, so a thumbnail cannot show something
// different from what will be shown; it is the same drawing, smaller.
Item {
    id: root

    // A slide map from ProDocument, or null for a clear slide
    property var slide: null
    // The name TransitionLayer sets its delegates' content by
    property alias content: root.slide
    // Whether to draw shadows; thumbnails go without (see SlideElement)
    property bool effects: true
    readonly property real slideWidth: slide?.width ?? 1920
    readonly property real slideHeight: slide?.height ?? 1080
    readonly property real u: Math.min(width / slideWidth, height / slideHeight)

    // Whether an element shows now, for one with a rule about a timer: such a rule is
    // met or not as the timer runs, so it is asked while the slide is on show, where
    // every other rule was settled when the slide's map was made (`met`).
    function showsNow(element) {
        if (element.hidden)
            return false
        const conditions = element.visibilityConditions
        let met = 0
        for (const condition of conditions) {
            if (condition.timed ? Timers.meets(condition.timerId, condition.timerName, condition.timerCriterion)
                                : condition.met)
                ++met
        }
        // All of them, any of them, or none of them, as the element has it
        switch (element.visibilityCriterion) {
        case 1:
            return conditions.length === 0 || met > 0
        case 2:
            return met === 0
        default:
            return met === conditions.length
        }
    }

    Item {
        anchors.centerIn: parent
        width: root.slideWidth * root.u
        height: root.slideHeight * root.u
        clip: true

        Rectangle {
            anchors.fill: parent
            visible: root.slide?.drawsBackground ?? false
            color: root.slide?.backgroundColor ?? "transparent"
        }

        // Elements that are hidden, or that their visibility rules rule out, are not drawn.
        Repeater {
            model: root.slide?.elements ?? []

            delegate: SlideElement {
                required property var modelData

                source: modelData
                unit: root.u
                effects: root.effects
                // Reading the tick is what has a rule about a timer asked again.
                visible: modelData.visibilityTimed ? Timers.tick >= 0 && root.showsNow(modelData) : modelData.visible
            }
        }
    }
}
