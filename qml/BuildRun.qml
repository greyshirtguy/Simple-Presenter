import QtQuick
import QtQuick.Window
import SimplePresenterApp

// A slide's builds, run: ProPresenter's Build In and Build Out, a transition of an
// element's own as it comes and as it goes (src/builds.h has what they are and when each
// runs). Slide makes one for a slide that has builds where they are to be run, which is
// on the screens and in the operator's preview, which hand it the times of the slide's
// steps; everywhere else a slide is drawn whole, and nothing of this is made.
//
// How it works. Everything is a matter of the clock. When each build starts and ends is
// known from the plan (the slide's map has it) and the times of the steps, so what an
// element is to show at any moment is worked out from the time alone, and nothing is
// kept from one moment to the next. Every screen, and the preview, works it out from
// the same times, so they agree, whenever each of their windows draws.
//
// While nothing is building, nothing is drawn: a timer waits for the next moment
// anything starts or ends, and in between, an element before its Build In, or after its
// Build Out, is simply not drawn. While a build runs, each frame of the window is
// counted (FrameClock, which keeps to the screen's refreshes: see src/frameclock.h) and
// the element is drawn by its transition's shader, the same shader
// a slide's transition would be drawn by (see TransitionCatalogue), between nothing and
// the element: from nothing to it as it builds in, from it to nothing as it builds out.
// For that the element is drawn into a texture of its own, the size of its box (and as
// far round it as its shadows reach), and that is what a build costs: a pass over the
// element's box for each frame the build runs, and nothing before or after.
//
// GUESSES at ProPresenter: that a build's transition goes over the element's own box, as
// a slide's goes over the screen (so a wipe starts at the element's edge and not the
// screen's); that its progress eases in and out, as this app's slide transitions do; and
// that an element whose builds overlap shows the one that began last.
Item {
    id: run

    // What the slide's map says of its builds: when each element comes and goes, by its
    // id (see builds::describePlan)
    required property var plan
    // When each of the slide's steps began, as Date.now() has it (see Slide.buildTimes)
    required property var times
    // What draws the slide's elements, and the elements' maps, by which the item that
    // draws an element is found from its id
    required property Repeater items
    required property var elements
    // What the log calls where the builds run, "" for not to put them in it
    property string logName: ""

    // What each element with builds is to show now, by its id: { shown, animating, out,
    // progress }. An element that is not in it is drawn as it is. (Not `states`, which
    // every Item has for something else.)
    property var phases: ({})
    // The ids of the elements that are building now
    property var building: []

    // Whether an element is to be drawn now (or, while it builds, drawn into its texture)
    function shows(id) {
        const phase = phases[id]
        return phase === undefined || phase.shown
    }

    // Where each element is in the slide, by its id, and its transitions as they are
    // drawn ({ in, out }, null where it has none): worked out once for the plan.
    readonly property var places: {
        const all = {}
        for (let i = 0; i < elements.length; ++i)
            all[elements[i].id] = i
        return all
    }
    readonly property var drawn: {
        const all = {}
        for (const id in plan) {
            const entry = plan[id]
            all[id] = { in: entry.inTransition !== undefined ? catalogue.drawn(entry.inTransition) : null,
                        out: entry.outTransition !== undefined ? catalogue.drawn(entry.outTransition) : null }
        }
        return all
    }

    // When a step began; a step that has not begun is at no time yet
    function stepTime(step) {
        const time = times[step]
        return time === undefined ? Infinity : time
    }

    // What every element with builds is to show at this moment, and when the next
    // moment is that anything changes
    function update() {
        const now = Date.now()
        const next = {}
        const moving = []
        let soonest = Infinity
        for (const id in plan) {
            const entry = plan[id]
            const events = []
            if (entry.inStep !== undefined) {
                const start = stepTime(entry.inStep) + entry.inAt * 1000
                events.push({ out: false, start: start, end: start + entry.inDuration * 1000 })
            }
            if (entry.outStep !== undefined) {
                const start = stepTime(entry.outStep) + entry.outAt * 1000
                events.push({ out: true, start: start, end: start + entry.outDuration * 1000 })
            }
            // The build that began last decides; before either, an element with a Build
            // In is not there yet, and one with only a Build Out is
            let current = null
            for (const event of events) {
                if (event.start <= now && (current === null || event.start >= current.start))
                    current = event
                if (event.start > now && event.start < soonest)
                    soonest = event.start
                if (event.end > now && event.end < soonest)
                    soonest = event.end
            }
            let phase
            if (current === null)
                phase = { shown: entry.inStep === undefined, animating: false }
            else if (now < current.end)
                phase = { shown: true, animating: true, out: current.out, progress: (now - current.start) / (current.end - current.start) }
            else
                phase = { shown: !current.out, animating: false }
            next[id] = phase
            if (phase.animating)
                moving.push(id)
        }
        phases = next
        if (moving.join("\n") !== building.join("\n")) {
            building = moving
            follow(moving)
        }
        frames.running = moving.length > 0
        if (!frames.running && soonest < Infinity) {
            wake.interval = Math.max(1, Math.ceil(soonest - now))
            wake.restart()
        } else {
            wake.stop()
        }
    }

    onPlanChanged: update()
    onTimesChanged: update()
    Component.onCompleted: update()

    TransitionCatalogue {
        id: catalogue
    }

    // Each frame while something builds
    FrameClock {
        id: frames

        onTicked: run.update()
    }

    // The next moment anything starts or ends, while nothing builds
    Timer {
        id: wake

        onTriggered: run.update()
    }

    // Nothing, as a texture: what an element builds in from and out to
    Rectangle {
        id: blank

        width: 2
        height: 2
        color: "transparent"
    }

    ShaderEffectSource {
        id: nothing

        sourceItem: blank
        hideSource: true
        visible: false
    }

    // The things drawing the elements that are building, by their elements' ids: made
    // when an element starts building and taken away when it is done (see `effect`).
    // Made by hand, and not by a Repeater: they stand among the slide's elements, so as
    // to be drawn just over the one each is for, where a Repeater's items stand in the
    // Repeater's parent in its order; and a Repeater makes all of its items again
    // whenever its list changes, which these, being part way through, must not be.
    readonly property var effects: ({})

    // Makes what is wanted for the elements building now, and takes away the rest
    function follow(moving) {
        for (const id of Object.keys(effects)) {
            if (!moving.includes(id)) {
                effects[id].done()
                delete effects[id]
            }
        }
        for (const id of moving) {
            const place = places[id] ?? -1
            const target = place >= 0 ? items.itemAt(place) : null
            if (target && effects[id] === undefined)
                effects[id] = effect.createObject(target.parent, { owner: run, elementId: id, target: target, place: place })
        }
    }

    Component.onDestruction: {
        for (const id of Object.keys(effects))
            effects[id].done()
    }

    // An element that is building, drawn by its transition, where the element is and
    // just over it in the slide's order (Slide puts the elements in order by `z` while
    // their builds run)
    Component {
        id: effect

        Item {
            id: drawer

            // What made it, which it reads what to draw from; the element, by its id,
            // its item and its place in the slide
            property var owner: null
            property string elementId: ""
            property Item target: null
            property int place: -1
            readonly property var phase: (owner ? owner.phases[elementId] : undefined) ?? ({ progress: 0, out: false })
            readonly property var drawing: (owner ? (phase.out ? owner.drawn[elementId]?.out : owner.drawn[elementId]?.in) : undefined)
                                           ?? { shader: "", options: Qt.vector4d(0, 0, 0, 0), tint: Qt.vector4d(0, 0, 0, 0),
                                                direction: Qt.vector2d(0, 0), name: "" }
            // The element's box, and as far round it as its shadows reach, in its own terms.
            // (By the shadows the element's map has, and not by what its item holds: an
            // item's childrenRect is worked out when it is read, and says so, which a
            // binding takes for a change to itself.)
            readonly property rect area: {
                if (!target || !owner)
                    return Qt.rect(0, 0, 0, 0)
                const element = owner.elements[place]
                let reach = 0
                for (const prefix of ["shadow", "textShadow"]) {
                    if (element && element[prefix + "Enabled"])
                        reach = Math.max(reach, (element[prefix + "Radius"] ?? 0) + Math.abs(element[prefix + "Offset"] ?? 0))
                }
                const margin = Math.ceil(reach * target.unit)
                return Qt.rect(-margin, -margin, target.width + 2 * margin, target.height + 2 * margin)
            }
            property double beganAt: 0
            property int framesBefore: 0
            property string name: ""
            property string logName: ""

            // It is done: the log is told how it went, and it goes. (Its bindings are let
            // go of first: it is taken away a moment later, and what made it may be gone.)
            function done() {
                if (logName !== "" && beganAt > 0) {
                    const frames = Log.frames(Window.window) - framesBefore
                    const seconds = (Date.now() - beganAt) / 1000
                    Log.note("transition", logName + ": the build of " + JSON.stringify(name) + " done: " + frames + " frames in "
                             + seconds.toFixed(2) + " s" + (seconds >= 0.1 ? ", " + Math.round(frames / seconds) + " a second" : ""))
                }
                visible = false
                owner = null
                target = null
                destroy()
            }

            x: target ? target.x : 0
            y: target ? target.y : 0
            z: (target ? target.z : 0) + 0.5
            width: target ? target.width : 0
            height: target ? target.height : 0
            rotation: target ? target.rotation : 0
            // (An item's texture is of what is under its own opacity, which is laid
            // over it here, as it is over the item.)
            opacity: target ? target.opacity : 1
            visible: target !== null && drawing.shader !== ""

            Component.onCompleted: {
                if (!owner || !target)
                    return
                name = owner.elements[place]?.name ?? ""
                logName = owner.logName
                if (logName === "")
                    return
                beganAt = Date.now()
                framesBefore = Log.frames(Window.window)
                const entry = owner.plan[elementId]
                Log.note("transition", logName + ": " + (phase.out ? "Build Out" : "Build In") + " of " + JSON.stringify(name) + ", "
                         + drawing.name + " over " + (phase.out ? entry.outDuration : entry.inDuration).toFixed(2) + " s, "
                         + drawing.shader.replace("qrc:/", ""))
            }

            ShaderEffectSource {
                id: picture

                sourceItem: drawer.target
                sourceRect: drawer.area
                hideSource: drawer.visible
                visible: false
            }

            ShaderEffect {
                // As TransitionLayer hands them to a slide's transition, the element in
                // place of the slide
                property var fromTex: drawer.phase.out ? picture : nothing
                property var toTex: drawer.phase.out ? nothing : picture
                property real progress: {
                    // Eased in and out, as TransitionLayer's are (InOutQuad)
                    const t = Math.max(0, Math.min(1, drawer.phase.progress))
                    return t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2
                }
                property real ratio: height > 0 ? width / height : 1
                property size resolution: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                property vector4d options: drawer.drawing.options
                property vector4d tint: drawer.drawing.tint
                property vector2d direction: drawer.drawing.direction

                x: drawer.area.x
                y: drawer.area.y
                width: drawer.area.width
                height: drawer.area.height
                fragmentShader: drawer.drawing.shader !== "" ? drawer.drawing.shader : "qrc:/shaders/dissolve.frag.qsb"
                onStatusChanged: {
                    if (status === ShaderEffect.Error)
                        Log.problem("a build: the graphics driver would not take the shader " + fragmentShader + ". It says: " + log)
                }
            }
        }
    }
}
