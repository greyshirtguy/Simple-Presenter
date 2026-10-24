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
//
// The last part is of what keeps drawing: a video playing under the slides, on the
// output and in a Screen Preview of the output on the stage screen. There nothing is
// waited for; what is counted, over some seconds, is how many frames each window drew
// and how many the video's decoder gave, the longest gap between two frames of the
// output (a gap is a frame of the video that was not shown), the processor used, and
// the longest the app's own thread was too busy to look at the clock, which is how
// long a click would have waited for it. The stage screen fills a display of its own
// where there is a second one, as it does in a church; on a computer with one display
// it is a window of 1280 by 720 over the output. (Whether the desktop draws a window
// that is over another that fills the screen is the desktop's affair: the stage's
// frames a second say whether it did.)
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
    // The longest gap between two frames of the output since it was last set to nothing
    property double outputWorstGap: 0
    // The stage window, and how many of its frames have reached the screen
    property var stageWindow: bench.window("stage")
    property int stageFrames: 0
    // How many frames the decoder of the video that is playing has handed the screens
    property int videoFrames: 0
    // The longest the app's thread was too busy to look at the clock (see look()) since
    // it was last set to nothing, in milliseconds, and when it last did; kept only while
    // something that asks for it is measured (`keepingStall`), so that the clock is not
    // asked a hundred times a second, at a cost of its own, while the app stands by
    property double stall: 0
    property double lookedAt: 0
    property bool keepingStall: false
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
            run.outputWorstGap = Math.max(run.outputWorstGap, now - run.outputFrame)
            run.outputFrame = now
        }
    }
    property Connections stageWatch: Connections {
        target: run.stageWindow
        function onFrameSwapped() {
            ++run.stageFrames
        }
    }
    property Connections videoWatch: Connections {
        target: output ? output.liveVideoSink : null
        function onVideoFrameChanged() {
            ++run.videoFrames
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

    // (Asked for every 10 ms on the app's own thread: a longer wait than that is time the
    // thread was doing something else, and is kept as `stall`.)
    function look() {
        if (keepingStall) {
            const at = bench.now()
            if (lookedAt > 0)
                stall = Math.max(stall, at - lookedAt - watch.interval)
            lookedAt = at
        }
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
    function crossing(name, count, prefix = "") {
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
            const label = prefix + "transition (" + transition.name + ") through " + name
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

    // Begins keeping what becomes of each frame of the video on the output, if one is
    // playing (see VideoTiming in src/benchmark.h)
    function watchVideo() {
        if (output && output.liveVideoSink)
            bench.watchVideo(output, output.liveVideoSink)
    }

    // What became of them, as lines of the report: how many frames of the video were on
    // the screen for longer than a frame of it lasts (a stutter), how many were never
    // shown, and the longest wait at each step on the way
    function recordVideo(label, marks) {
        const timing = bench.videoTiming(marks || [])
        if (!timing.frames || timing.frames < 3)
            return
        bench.record(label + ": video frames held on the screen too long", timing.held, "frames")
        bench.record(label + ": video frames not shown", timing.notShown, "frames")
        bench.record(label + ": frames of the output for anything but the video", timing.others, "frames")
        bench.record(label + ": longest a video frame waited for the app's thread", timing.waitForThread, "ms")
        bench.record(label + ": longest sync of the output", timing.sync, "ms")
        bench.record(label + ": longest from a video frame coming to its being on the screen", timing.toScreen, "ms")
    }

    // The first item under `item`, itself included, that `test` is true of, or null
    function findItem(item, test) {
        if (test(item))
            return item
        for (const child of item.children) {
            const found = findItem(child, test)
            if (found)
                return found
        }
        return null
    }

    // Begins keeping how evenly the words of a scrolling text box on the output go, if
    // there is one (see MotionTiming in src/benchmark.h)
    function watchMotion() {
        const scroller = output ? findItem(output.contentItem, item => item.travelled !== undefined && item.settings !== undefined && item.visible) : null
        if (scroller)
            bench.watchMotion(output, scroller, "travelled")
    }

    // How evenly, as lines of the report: how many frames' steps of the words were a
    // quarter more or less than most, and how many did not move them
    function recordMotion(label) {
        const timing = bench.motionTiming()
        if (!timing.frames || timing.frames < 10)
            return
        bench.record(label + ": frames the words moved unevenly in", timing.uneven, "frames")
        bench.record(label + ": frames the words stood still in", timing.stopped, "frames")
    }

    // Some seconds of whatever is playing, with nothing being done: see the top.
    function playing(label, seconds) {
        let from = 0
        let cpu = 0
        let output = 0
        let stage = 0
        let decoded = 0
        jobs.push(() => {
            from = bench.now()
            cpu = bench.cpu()
            output = outputFrames
            stage = stageFrames
            decoded = videoFrames
            outputWorstGap = 0
            stall = 0
            lookedAt = 0
            keepingStall = true
            watchVideo()
            watchMotion()
            after(seconds * 1000)
        })
        jobs.push(() => {
            keepingStall = false
            recordVideo(label)
            recordMotion(label)
            const lasted = (bench.now() - from) / 1000
            bench.record(label + ": output frames a second", (outputFrames - output) / lasted, "fps")
            bench.record(label + ": video frames decoded a second", (videoFrames - decoded) / lasted, "fps")
            bench.record(label + ": stage frames a second", (stageFrames - stage) / lasted, "fps")
            bench.record(label + ": longest wait between frames of the output", outputWorstGap, "ms")
            bench.record(label + ": processor used, of one core", (bench.cpu() - cpu) / 10 / lasted, "%")
            bench.record(label + ": longest the app was too busy to answer", stall, "ms")
            next()
        })
    }

    // Slides of a presentation put on the output one after another, `spacing`
    // milliseconds apart, while something plays: the work in each click, and the longest
    // the app was too busy to answer while they changed; and what became of the video's
    // frames meanwhile (see recordVideo). (Not the time to the output, which is drawing
    // a frame of the video every thirtieth of a second anyway.)
    function changing(name, label, count, spacing = 700) {
        const calls = []
        const clicks = []
        jobs.push(() => {
            stall = 0
            lookedAt = 0
            keepingStall = true
            watchVideo()
            next()
        })
        for (let index = 1; index <= count; ++index) {
            jobs.push(() => {
                const from = bench.now()
                clicks.push(from)
                goLive(index)
                calls.push(bench.now() - from)
                after(spacing)
            })
        }
        jobs.push(() => {
            keepingStall = false
            recordVideo(label, clicks)
            bench.record(label + ": work in the click itself, median", median(calls), "ms")
            bench.record(label + ": longest the app was too busy to answer", stall, "ms")
            next()
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

        // ---- A Screen Preview of the output on the stage screen, which fills a display
        // of its own if there is a second one (one the output is not on)
        const previewLayout = () => StageLayouts.layouts.find(layout => layout.name === "Screen Preview")?.id ?? ""
        jobs.push(() => {
            const stage = Screens.stage[0]
            const other = Screens.displays.find(display => display.name !== output.screen.name)
            if (stage && other)
                Screens.setOutput(stage.id, { output: "display", display: other.name })
            after(1500)
        })
        jobs.push(() => {
            // (Found again: a window moved to another display may be one made anew.) On
            // one display, the window is made the size of a small screen, so that what
            // it draws is not made to look cheaper than it is.
            stageWindow = bench.window("stage")
            if (stageWindow && stageWindow.screen.name === output.screen.name) {
                stageWindow.width = 1280
                stageWindow.height = 720
            }
            Show.stageLayoutId = previewLayout()
            after(1000)
        })
        showing("Words", "stage preview: show", 18)
        // The same by a transition, which the Screen Preview makes again at its size
        jobs.push(() => {
            transitionIndex = 1
            transitionDuration = 0.5
            next()
        })
        crossing("Words", 8, "stage preview: ")
        // And the same with this window's preview showing the stage screen, whose layout's
        // Screen Preview moves here too (as Screen Previews that move has it: see movingPreviews
        // in Main.qml)
        jobs.push(() => {
            pickPreviewScreen(Screens.stage[0] ? Screens.stage[0].id : "")
            after(800)
        })
        crossing("Words", 8, "stage preview, in this window's preview too: ")
        jobs.push(() => {
            pickPreviewScreen("")
            selectTransition("Cut")
            after(500)
        })

        // ---- Scrolling text: a ticker along the foot of the slide on the output, for some
        // seconds with the stage's plain view and some with its Screen Preview, which
        // scrolls it too, nothing being done
        jobs.push(() => {
            Show.stageLayoutId = ""
            openEntry(presentation("Scrolling"))
            goLive(0)
            after(1500)
        })
        playing("scrolling text, the stage's plain view", 6)
        jobs.push(() => {
            Show.stageLayoutId = previewLayout()
            after(1500)
        })
        playing("scrolling text, a Screen Preview on the stage", 6)
        jobs.push(() => {
            Show.clearSlide()
            Show.stageLayoutId = previewLayout()
            after(500)
        })

        // ---- A video playing under the slides, with the stage screen's plain view and
        // then with its Screen Preview of the output: where the workspace has the video,
        // which is known only once its library is open (the jobs for it are put in
        // front of what is left)
        jobs.push(() => {
            if (presentation("Video")) {
                const rest = jobs
                jobs = []
                jobs.push(() => {
                    Show.stageLayoutId = ""
                    openEntry(presentation("Video"))
                    goLive(0)
                    after(3000)
                })
                playing("video, the stage's plain view", 8)
                jobs.push(() => {
                    Show.stageLayoutId = previewLayout()
                    after(1500)
                })
                playing("video, a Screen Preview on the stage", 8)
                changing("Video", "video, a Screen Preview on the stage, slides changed", 10)
                // And by a dissolve, each over before the next
                jobs.push(() => {
                    selectTransition("Dissolve")
                    transitionDuration = 0.5
                    after(500)
                })
                changing("Video", "video, a Screen Preview on the stage, slides dissolved", 8, 1200)
                jobs.push(() => {
                    selectTransition("Cut")
                    clearMedia()
                    after(500)
                })
                jobs = jobs.concat(rest)
            }
            next()
        })
        jobs.push(() => {
            Show.stageLayoutId = ""
            next()
        })

        next()
    }
}
