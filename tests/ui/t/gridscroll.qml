import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The slides' grid following the slide that goes live, as Settings → Slides says (the maintainer, 2026-11-08: "make the
// auto scroll nice and smooth ... an option ... early, just in time"). Early, the default: a slide that goes live in the
// bottom row in view brings the row under it into view, the grid moving there smoothly, frame by frame, a moment after
// the change; one in a row above moves nothing. Just in Time, chosen on the settings screen and kept in the settings: the
// grid stays put while the live slide is in view, and moves, smoothly, to bring one in that is not. How long it takes and
// whether it eases in and out of the move (on, unless switched off) are settings there too, kept in the settings.
QtObject {
    id: t

//COMMON
    // Where the grid has been, while it is watched
    property var samples: []
    property Timer sampler: Timer {
        interval: 15
        repeat: true
        onTriggered: t.samples.push(t.view().contentY)
    }

    function view() {
        return named("slideGrid")
    }

    // Whether a cell of the grid is wholly in view, by the grid's own place (the test's slideCell would scroll to it)
    function inView(index) {
        const grid = view()
        const cell = grid.itemAtIndex(index)
        return cell !== null && cell.y >= grid.contentY - 0.5 && cell.y + cell.height <= grid.contentY + grid.height + 0.5
    }

    // The last slide wholly in view that has a row under it
    function bottomRow() {
        let found = -1
        for (let i = 0; i < document.slides.length - view().columns; ++i) {
            if (inView(i))
                found = i
        }
        return found
    }

    // The steps a move made from frame to frame (the samples are 15 ms apart): whether its first and last few are smaller
    // than its middle ones (eased), whether they are all about alike (straight), and how many there were
    function moveSteps(taken) {
        const moves = []
        for (let i = 1; i < taken.length; ++i) {
            const step = taken[i] - taken[i - 1]
            if (Math.abs(step) > 0.01)
                moves.push(Math.abs(step))
        }
        if (moves.length < 6)
            return { ok: false, even: false, count: moves.length, detail: moves.length + " steps" }
        const third = Math.floor(moves.length / 3)
        const mean = list => list.reduce((a, b) => a + b, 0) / list.length
        const start = mean(moves.slice(0, 2)), end = mean(moves.slice(-2)), middle = mean(moves.slice(third, moves.length - third))
        const inner = moves.slice(1, -1)
        return { ok: start < middle * 0.6 && end < middle * 0.6, even: Math.max(...inner) < Math.min(...inner) * 2.5 + 2,
                 count: moves.length, detail: moves.map(m => m.toFixed(1)).join(" ") }
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                return 600
            },
            () => {
                view().positionViewAtBeginning()
                kept.columns = view().columns
                return 300
            },
            () => {
                check("the grid follows the live slide early, unless set otherwise", slideScroll === "early")
                kept.bottom = bottomRow()
                check("(a presentation of more rows than the grid shows)", kept.bottom >= kept.columns && !inView(kept.bottom + kept.columns),
                      kept.bottom + " of " + document.slides.length + ", " + kept.columns + " a row")
                kept.top = view().contentY
                goLive(kept.bottom - kept.columns)
                return 600
            },
            () => {
                check("a slide that goes live in a row above the bottom one moves nothing", view().contentY === kept.top, view().contentY)
                samples = []
                sampler.start()
                goLive(kept.bottom)
                return 900
            },
            () => {
                sampler.stop()
                const end = view().contentY
                check("one in the bottom row brings the row under it into view, and stays in view itself", inView(kept.bottom) && inView(kept.bottom + kept.columns)
                      && end > kept.top, end + " from " + kept.top)
                const between = samples.filter(y => y > kept.top + 0.5 && y < end - 0.5)
                check("moving there smoothly: through places between, frame by frame, and never back", between.length >= 3
                      && samples.every((y, i) => i === 0 || y >= samples[i - 1] - 0.01), between.length + " between: " + samples.map(y => Math.round(y)).join(" "))
                check("and only after the change, not in it: the grid had not moved by the time the slide was live", samples.length > 2 && samples[0] === kept.top,
                      samples.slice(0, 3).join(" "))
                // ---- Just in Time, from the settings screen
                settingsOpen = true
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                screen.section = "slides"
                return 400
            },
            () => {
                const way = named("slideScroll:justInTime")
                check("the settings have the two ways, Early chosen", way !== null && named("slideScroll:early") !== null && named("slideScroll:early").chosen
                      && !way.chosen)
                testInput.grab("1-settings")
                click(centre(way))
                return 300
            },
            () => {
                check("Just in Time chosen, and kept in the settings", slideScroll === "justInTime" && String(settings.value("slideScroll")) === "justInTime"
                      && named("slideScroll:justInTime").chosen, String(settings.value("slideScroll")))
                check("the log says so", logged(/settings +the slides' grid follows the live slide just in time/).length === 1)
                settingsOpen = false
                takeFocus()
                return 300
            },
            () => {
                view().positionViewAtBeginning()
                return 300
            },
            () => {
                kept.top = view().contentY
                kept.bottom = bottomRow()
                goLive(kept.bottom)
                return 700
            },
            () => {
                check("a slide in the bottom row moves nothing now", view().contentY === kept.top && inView(kept.bottom), view().contentY)
                samples = []
                sampler.start()
                goLive(kept.bottom + kept.columns)
                return 900
            },
            () => {
                sampler.stop()
                const end = view().contentY
                check("one out of view is brought in, and the row under it is not", inView(kept.bottom + kept.columns) && end > kept.top
                      && (kept.bottom + 2 * kept.columns >= document.slides.length || !inView(kept.bottom + 2 * kept.columns)), end + " from " + kept.top)
                check("smoothly too", samples.filter(y => y > kept.top + 0.5 && y < end - 0.5).length >= 3, samples.map(y => Math.round(y)).join(" "))
                testInput.grab("2-just-in-time")
                // ---- How long, and the curve: eased in and out unless switched off
                check("by default, eased in and out over 0.4 s", slideScrollEased === true && Math.abs(slideScrollDuration - 0.4) < 1e-6)
                kept.eased = moveSteps(samples)
                check("eased: the first and the last steps of the move are smaller than the middle ones", kept.eased.ok, kept.eased.detail)
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                screen.slideScrollDurationEdited(0.8)
                screen.slideScrollEasedEdited(false)
                check("both settings, kept in the settings", Math.abs(slideScrollDuration - 0.8) < 1e-6 && !slideScrollEased
                      && Math.abs(Number(settings.value("slideScrollDuration")) - 0.8) < 1e-6 && String(settings.value("slideScrollEased")) === "false",
                      settings.value("slideScrollDuration") + " " + settings.value("slideScrollEased"))
                check("on the settings screen", named("slideScrollDuration") !== null && named("slideScrollEased") !== null)
                kept.top = view().contentY
                samples = []
                sampler.start()
                goLive(kept.bottom + 2 * kept.columns)
                return 1400
            },
            () => {
                sampler.stop()
                const moving = samples.filter((y, i) => i > 0 && Math.abs(y - samples[i - 1]) > 0.01)
                const straight = moveSteps(samples)
                check("at one speed throughout, switched off: no step much smaller than the rest", moving.length >= 6 && !straight.ok && straight.even,
                      straight.detail)
                check("and slower: more frames in the move than in the quarter-of-a-second ones", moving.length > kept.eased.count, moving.length + " against " + kept.eased.count)
                clearAll()
                return 300
            },
            () => {
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0,
                      logged(/PROBLEM|WARNING|ERROR/).join(" | "))
            }
        ]
        next()
    }
}
