import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The slides' grid following the slide that goes live, as Settings → Slides says (the maintainer, 2026-11-08: "make the
// auto scroll nice and smooth ... an option ... early, just in time"). Early, the default: a slide that goes live in the
// bottom row in view brings the row under it into view, the grid moving there smoothly, frame by frame, a moment after
// the change; one in a row above moves nothing. Just in Time, chosen on the settings screen and kept in the settings: the
// grid stays put while the live slide is in view, and moves, smoothly, to bring one in that is not.
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
