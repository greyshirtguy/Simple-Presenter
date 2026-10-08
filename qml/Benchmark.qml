import QtQuick

// The benchmark's run (--benchmark <file>; see src/benchmark.h): works the app as an
// operator would, and times what the operator would be waiting for.
//
// It is made inside the operator window's own scope (see main.cpp), so that it can work
// the app by the same functions a click or a key does. The names used here that are
// not its own (win, output, editScreen, catalog, openEntry, goLive and the rest) are
// Main.qml's.
//
// A time here is from asking for something to the frame that shows it: the windows say
// when each frame of theirs has reached the screen, and "standing still" is when a
// window has gone a while without a new one. Each is given in milliseconds since the
// asking; where a thing is done many times over, the middle one of them (the median)
// and the worst are given.
QtObject {
    id: run

    // The clock, the report and the rest: a Benchmark (src/benchmark.h)
    required property var bench

    // When each window last had a frame reach the screen, and how many have
    property double operatorFrame: 0
    property double outputFrame: 0
    property int operatorFrames: 0
    property int outputFrames: 0
    // What is being waited for: see timed()
    property var waiting: null
    // Called as each frame of the operator window reaches the screen, if set
    property var eachFrame: null
    // What is still to do, in order: each is a function, which calls next() when done
    property var jobs: []

    property Connections operatorWatch: Connections {
        target: win

        function onFrameSwapped() {
            const now = run.bench.now()
            if (++run.operatorFrames === 1)
                run.bench.record("startup: to the operator window's first frame", now, "ms")
            run.framed(false, now, run.operatorFrame)
            run.operatorFrame = now
            if (run.eachFrame)
                run.eachFrame()
        }
    }

    property Connections outputWatch: Connections {
        target: output

        function onFrameSwapped() {
            const now = run.bench.now()
            if (++run.outputFrames === 1)
                run.bench.record("startup: to the output window's first frame", now, "ms")
            run.framed(true, now, run.outputFrame)
            run.outputFrame = now
        }
    }

    // Looks, many times a second, for whether what is being waited for has happened.
    property Timer watch: Timer {
        interval: 10
        repeat: true
        running: true
        onTriggered: run.look()
    }

    property Timer pause: Timer {
        onTriggered: run.next()
    }

    function framed(ofOutput, now, before) {
        const w = waiting
        if (!w || w.ofOutput !== ofOutput)
            return
        if (w.frames++ === 0)
            w.first = now - w.from
        else
            w.worstGap = Math.max(w.worstGap, now - before)
        w.last = now - w.from
        w.lastAt = now
    }

    function look() {
        const w = waiting
        if (!w)
            return
        const now = bench.now()
        if ((w.frames > 0 && now - w.lastAt >= w.quiet) || now - w.from >= w.limit) {
            waiting = null
            w.then(w)
        }
    }

    // Does something, and waits for a window (the output, or else the operator's) to
    // have shown it and be standing still: `quiet` milliseconds without a new frame.
    // `then` is given what happened: `call`, how long the doing itself took; `first`
    // and `last`, when the first frame after it and the last reached the screen;
    // `frames`, how many there were; and `worstGap`, the longest wait between two.
    function timed(ofOutput, quiet, action, then) {
        waiting = { ofOutput: ofOutput, quiet: quiet, limit: 5000, from: bench.now(), frames: 0, first: 0, last: 0,
                    lastAt: 0, worstGap: 0, call: 0, then: then }
        action()
        waiting.call = bench.now() - waiting.from
    }

    function next() {
        if (jobs.length === 0) {
            bench.finish()
            return
        }
        const job = jobs.shift()
        job()
    }

    function after(ms) {
        pause.interval = ms
        pause.restart()
    }

    function median(values) {
        const sorted = values.slice().sort((a, b) => a - b)
        return sorted.length === 0 ? 0 : sorted[Math.floor(sorted.length / 2)]
    }

    function worst(values) {
        return values.reduce((most, value) => Math.max(most, value), 0)
    }

    function presentation(name) {
        return documents.find(d => d.name === name)
    }

    // Opens each presentation, and times it to the slides all being drawn.
    function opening(name, label) {
        jobs.push(() => timed(false, 200, () => openEntry(presentation(name)), (w) => {
            bench.record(label + " " + name + ": to its slides all being drawn", w.last, "ms")
            next()
        }))
    }

    // Puts each slide of a presentation on the output in turn, as fast as the output
    // shows them, and times each to the frame it is on the output in.
    function showing(name, label, count) {
        const shown = []
        const stood = []
        const calls = []
        let index = 0
        const one = () => timed(true, 60, () => goLive(index), (w) => {
            shown.push(w.first)
            stood.push(w.last)
            calls.push(w.call)
            if (++index < count) {
                one()
                return
            }
            bench.record(label + " " + name + ": to the output showing a slide, median", median(shown), "ms")
            bench.record(label + " " + name + ": to the output showing a slide, worst", worst(shown), "ms")
            bench.record(label + " " + name + ": to the output standing still, median", median(stood), "ms")
            bench.record(label + " " + name + ": work in the click itself, median", median(calls), "ms")
            next()
        })
        jobs.push(() => {
            openEntry(presentation(name))
            after(500)
        })
        jobs.push(() => one())
    }

    // The same with a transition between the slides, which is a run of frames: how
    // many a second there were of them, and the longest wait for one.
    function crossing(name, count) {
        const rates = []
        const gaps = []
        const starts = []
        let index = 0
        const one = () => timed(true, 250, () => goLive(index), (w) => {
            if (w.frames > 1)
                rates.push((w.frames - 1) * 1000 / (w.last - w.first))
            gaps.push(w.worstGap)
            starts.push(w.first)
            if (++index < count) {
                one()
                return
            }
            const label = "transition (" + transition.name + ") through " + name
            bench.record(label + ": to its first frame, median", median(starts), "ms")
            bench.record(label + ": frames a second, median", median(rates), "fps")
            bench.record(label + ": longest wait between frames, worst", worst(gaps), "ms")
            next()
        })
        jobs.push(() => {
            openEntry(presentation(name))
            after(500)
        })
        jobs.push(() => one())
    }

    // A property of the picked element changed forty times over, once a frame, as a
    // slider being dragged changes it: how long each change takes to be on the screen.
    function dragging(label, change) {
        jobs.push(() => {
            let count = 0
            const from = bench.now()
            eachFrame = () => {
                if (count === 40) {
                    eachFrame = null
                    bench.record("editor: " + label + ", a change to the screen", (bench.now() - from) / count, "ms")
                    editScreen.canvas.settle()
                    after(300)
                    return
                }
                change(count++)
                win.requestUpdate()
            }
            win.requestUpdate()
        })
    }

    Component.onCompleted: {
        const names = ["Words", "Shapes", "Pictures"]

        // ---- Starting up: the app is ready when its window has stopped changing
        jobs.push(() => timed(false, 300, () => {}, () => {
            bench.record("startup: to the operator window standing still", operatorFrame, "ms")
            openLibrary(catalog.libraries[0].path)
            after(500)
        }))

        // ---- Opening a presentation, the first time and again
        for (const name of names)
            opening(name, "open")
        for (const name of names)
            opening(name, "open again")

        // ---- Showing slides, with the output on and filling its screen
        jobs.push(() => {
            output.setFullScreen(true)
            selectTransition("Cut")
            after(1200)
        })
        for (const name of names)
            showing(name, "show", 18)
        // A second time round, when what they need has been loaded once
        showing("Shapes", "show again", 18)
        showing("Pictures", "show again", 18)

        jobs.push(() => {
            transitionIndex = 1
            transitionDuration = 0.5
            next()
        })
        for (const name of names)
            crossing(name, 8)

        // ---- The editor
        jobs.push(() => {
            selectTransition("Cut")
            openEntry(presentation("Shapes"))
            after(500)
        })
        jobs.push(() => timed(false, 250, () => startEditing(currentEntry(), document.slides[0].id), (w) => {
            bench.record("editor: opening, to standing still", w.last, "ms")
            editScreen.canvas.pick(editScreen.canvas.elements.find(e => e.shape === "roundedRectangle").id)
            after(400)
        }))
        dragging("corners made rounder", i => editScreen.canvas.setProperties({ roundness: 0.1 + i * 0.008 }, true))
        dragging("opacity turned down", i => editScreen.canvas.setProperties({ opacity: 1 - i * 0.015 }, true))
        dragging("made wider", i => editScreen.canvas.setProperties({ width: 520 + i * 4 }, true))
        jobs.push(() => timed(false, 250, () => stopEditing(), (w) => {
            bench.record("editor: closing, to standing still", w.last, "ms")
            next()
        }))

        // ---- Standing by: a slide on the output and nothing happening
        let cpu = 0
        let since = 0
        let frames = 0
        jobs.push(() => {
            openEntry(presentation("Words"))
            goLive(0)
            after(2000)
        })
        jobs.push(() => {
            cpu = bench.cpu()
            since = bench.now()
            frames = operatorFrames + outputFrames
            after(4000)
        })
        jobs.push(() => {
            const seconds = (bench.now() - since) / 1000
            bench.record("standing by: processor used, of one core", (bench.cpu() - cpu) / 10 / seconds, "%")
            bench.record("standing by: frames drawn a second, both windows", (operatorFrames + outputFrames - frames) / seconds, "fps")
            bench.record("memory: held at the end", bench.memory(), "MB")
            bench.record("memory: most held", bench.peakMemory(), "MB")
            next()
        })

        next()
    }
}
