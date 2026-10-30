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
    // The operator window, where the slide is on show or stands for a screen that it is
    // on (the operator's preview), for the Screen Previews in it to ask what the
    // screens show; null for a slide that is only being looked at (a thumbnail, the
    // editor), which draws none (see SlideElement and ScreenPicture)
    property var pictures: null
    // What the slide is drawn over here, for its elements' fills that are a blur or an
    // invert of what is behind them (TextFills): the screen's media layer, the picture of
    // a thumbnail's media, and so on; null where it is drawn over nothing
    property Item backdrop: null
    // When the slide's builds are run (see BuildRun): the moment each of its steps
    // began, as Date.now() has it, which the screens and the operator's preview are
    // handed. Null to draw every element as it is, which is what a slide is everywhere
    // else: a thumbnail, the editor. A slide without builds is drawn as it is either way.
    property var buildTimes: null
    // What the log calls where the builds run; "" for not to put them in it
    property string logName: ""
    // What runs them, while there are any to run; null otherwise
    readonly property var run: runner.item
    readonly property real slideWidth: slide?.width ?? 1920
    readonly property real slideHeight: slide?.height ?? 1080
    readonly property real u: Math.min(width / slideWidth, height / slideHeight)
    readonly property var elements: slide?.elements ?? []
    // The last element that was drawn: see elementAt(). (Kept in a plain object, so
    // that noting it is not a change to anything that is watched.)
    readonly property var spare: ({ element: null })

    // The element that the item at this place in the slide draws. An item that is about
    // to go, the slide having changed for one with fewer elements, may ask once more
    // on its way out, and is given a real element rather than nothing.
    function elementAt(index) {
        const element = elements[Math.min(index, elements.length - 1)]
        if (element)
            spare.element = element
        return element ?? spare.element
    }

    // Whether an element shows now, for one with a rule about a timer, or about an
    // element that shows what the stage is being told (`staged`): such a rule is met or
    // not as the timer runs, or as the stage is told something, so it is asked while
    // the slide is on show, where every other rule was settled when the slide's map was
    // made (`met`). (An element with a rule of the second kind is not hidden by being
    // hidden in the file: see toSlideMap, src/proconvert.cpp.)
    function showsNow(element) {
        if (element.hidden && element.visibilityStaged !== true)
            return false
        const conditions = element.visibilityConditions
        let met = 0
        for (const condition of conditions) {
            if (condition.timed ? Timers.meets(condition.timerId, condition.timerName, condition.timerCriterion)
                : condition.staged ? (Show.stageMessage !== "") === condition.hasText
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
        //
        // There is an item for each element, and the items are made again only when
        // the number of elements changes. A slide that changes, as it does many times a
        // second while something on it is dragged in the editor, or that gives way to
        // another of as many elements, has the items it has shown the new elements:
        // only what differs is worked out again, where making the items again would lay
        // out every text and load every picture afresh.
        Repeater {
            id: elementItems

            model: root.elements.length

            delegate: SlideElement {
                required property int index
                readonly property var modelData: root.elementAt(index)

                source: modelData
                unit: root.u
                effects: root.effects
                pictures: root.pictures
                // Reading the tick is what has a rule about a timer asked again. (And
                // one about the stage message reads that, where it is asked.) While
                // builds run, an element is not there before its Build In or after its
                // Build Out.
                visible: (modelData.visibilityTimed ? Timers.tick >= 0 && root.showsNow(modelData) : modelData.visible)
                         && (root.run === null || root.run.shows(modelData.id))
                // In order, while builds run, so that an element that is building is drawn
                // just over it (see BuildRun)
                z: root.run !== null ? index : 0
            }
        }

        // The builds, run: only for a slide that has them, where they are to be run
        Loader {
            id: runner

            anchors.fill: parent
            active: root.buildTimes !== null && root.slide?.builds !== undefined
            sourceComponent: BuildRun {
                plan: root.slide?.builds ?? ({})
                times: root.buildTimes ?? []
                items: elementItems
                elements: root.elements
                logName: root.logName
            }
        }
    }
}
