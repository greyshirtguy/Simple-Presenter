import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The slides' grid following the slide that goes live, as Settings → Slides says (the maintainer, 2026-11-08: "make the
// auto scroll nice and smooth ... an option ... early, just in time"). Early, the default: a slide that goes live in the
// bottom row in view brings the row under it into view, the grid moving there smoothly, frame by frame, starting in the
// frames straight after the change (the maintainer, 2026-11-09: "it should start instantly"); one in a row above moves
// nothing. Just in Time, chosen on the settings screen and kept in the settings: the grid stays put while the live slide
// is in view, and moves, smoothly, to bring one in that is not. How long a step takes and whether it eases in and out of
// the move (on, unless switched off) are settings there too, kept in the settings; a jump to a slide out of view takes a
// tenth of a second whatever is set, by a click's way and by the API's; and Next held down has the grid keep up with the
// slides while it is held. And Ctrl+- and Ctrl+= make the thumbnails the pointer is over smaller and larger, the slides'
// or the media bin's.
QtObject {
    id: t

//COMMON
    // Where the grid has been, while it is watched, and when, with the slide live then
    property var samples: []
    property var stamps: []
    property Timer sampler: Timer {
        interval: 15
        repeat: true
        onTriggered: {
            t.samples.push(t.view().contentY)
            t.stamps.push({ at: Date.now(), live: liveIndex })
        }
    }

    function watch() {
        samples = []
        stamps = []
        kept.from = Date.now()
        sampler.start()
    }

    // When the grid first moved from `top` after the watching began, and when it last moved, in milliseconds from then;
    // -1 if it did not
    function firstMove(top) {
        const i = samples.findIndex(y => Math.abs(y - top) > 0.01)
        return i < 0 ? -1 : stamps[i].at - kept.from
    }

    function lastMove() {
        let last = -1
        for (let i = 1; i < samples.length; ++i) {
            if (Math.abs(samples[i] - samples[i - 1]) > 0.01)
                last = i
        }
        return last < 0 ? -1 : stamps[last].at - kept.from
    }

    // Next held down: the key going down, then again every 30 ms as a desktop repeats a held key, `left` times in all,
    // and let go
    property Timer holder: Timer {
        property int left: 0

        interval: 30
        repeat: true
        onTriggered: {
            if (--left > 0) {
                testInput.keyUp(Qt.Key_Right, true)
                testInput.keyDown(Qt.Key_Right, true)
            } else {
                stop()
                testInput.keyUp(Qt.Key_Right)
                t.kept.letGo = Date.now()
            }
        }
    }

    // How much of the cell at `index` is in view with the grid at `y`, from 0 to 1, by the grid's rows (a cell out of
    // view need not have been made)
    function shownOf(index, y) {
        const grid = view()
        const top = grid.originY + Math.floor(index / grid.columns) * grid.cellHeight
        return Math.max(0, Math.min(top + grid.cellHeight, y + grid.height) - Math.max(top, y)) / grid.cellHeight
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
                // ---- Ctrl+- and Ctrl+=, for the thumbnails the pointer is over
                const grid = view().mapToItem(null, view().width / 2, view().height / 2)
                testInput.mouse(1, grid.x, grid.y)
                kept.slideWidth = thumbnailWidth
                kept.mediaWidth = mediaThumbnailWidth
                return 200
            },
            () => {
                check("(the pointer over the slides)", thumbnailsUnderPointer === "slides", thumbnailsUnderPointer)
                testInput.key(Qt.Key_Minus, Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl+- over the slides makes their thumbnails smaller, and the media's as they were", thumbnailWidth < kept.slideWidth
                      && mediaThumbnailWidth === kept.mediaWidth, kept.slideWidth + " -> " + thumbnailWidth)
                kept.smaller = thumbnailWidth
                testInput.key(Qt.Key_Equal, Qt.ControlModifier)
                return 300
            },
            () => {
                // (By the − and + buttons' steps, which need not come back to the size it had: see zoomThumbnails)
                check("and Ctrl+= larger again", thumbnailWidth > kept.smaller, kept.smaller + " -> " + thumbnailWidth)
                kept.slideWidth = thumbnailWidth
                const bin = named("mediaGrid")
                const over = bin.mapToItem(null, bin.width / 2, bin.height / 2)
                testInput.mouse(1, over.x, over.y)
                return 200
            },
            () => {
                check("(the pointer over the media bin)", thumbnailsUnderPointer === "media", thumbnailsUnderPointer)
                testInput.key(Qt.Key_Minus, Qt.ControlModifier)
                return 300
            },
            () => {
                check("over the media bin, Ctrl+- makes the media's smaller, and the slides' as they were", mediaThumbnailWidth < kept.mediaWidth
                      && thumbnailWidth === kept.slideWidth, kept.mediaWidth + " -> " + mediaThumbnailWidth)
                kept.smaller = mediaThumbnailWidth
                testInput.key(Qt.Key_Equal, Qt.ControlModifier)
                return 300
            },
            () => {
                check("and Ctrl+= larger again", mediaThumbnailWidth > kept.smaller, kept.smaller + " -> " + mediaThumbnailWidth)
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
                watch()
                goLive(kept.bottom)
                kept.inClick = view().contentY
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
                // (In 0.129 it waited for the change's transition and 150 ms more)
                const started = firstMove(kept.top)
                check("at once: not in the click itself, but in the frames straight after it", kept.inClick === kept.top && started >= 0 && started < 100,
                      "moving " + started + " ms after the click")
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
                // The first slide of the row under the bottom one in view; the slide before it is the last of the bottom row
                kept.under = (Math.floor(kept.bottom / kept.columns) + 1) * kept.columns
                goLive(kept.under - 1)
                return 700
            },
            () => {
                check("a slide in the bottom row moves nothing now", view().contentY === kept.top && inView(kept.under - 1), view().contentY)
                watch()
                // (Next, a step: to the slide beside the live one, out of view)
                step(1)
                return 900
            },
            () => {
                sampler.stop()
                const end = view().contentY
                check("Next to one out of view brings it in, and the row under it is not", liveIndex === kept.under && inView(kept.under) && end > kept.top
                      && (kept.under + kept.columns >= document.slides.length || !inView(kept.under + kept.columns)), liveIndex + ", " + end + " from " + kept.top)
                check("smoothly too", samples.filter(y => y > kept.top + 0.5 && y < end - 0.5).length >= 3, samples.map(y => Math.round(y)).join(" "))
                testInput.grab("2-just-in-time")
                // ---- How long, and the curve: eased in and out unless switched off
                check("by default, eased in and out over 0.4 s", slideScrollEased === true && Math.abs(slideScrollDuration - 0.4) < 1e-6)
                kept.eased = moveSteps(samples)
                check("eased: the first and the last steps of the move are smaller than the middle ones", kept.eased.ok, kept.eased.detail)
                const took = lastMove()
                check("a step taking the time set", took > 300 && took < 650, took + " ms")
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                screen.slideScrollDurationEdited(0.8)
                screen.slideScrollEasedEdited(false)
                check("both settings, kept in the settings", Math.abs(slideScrollDuration - 0.8) < 1e-6 && !slideScrollEased
                      && Math.abs(Number(settings.value("slideScrollDuration")) - 0.8) < 1e-6 && String(settings.value("slideScrollEased")) === "false",
                      settings.value("slideScrollDuration") + " " + settings.value("slideScrollEased"))
                check("on the settings screen", named("slideScrollDuration") !== null && named("slideScrollEased") !== null)
                // (The last slide of the row now at the bottom, which moves nothing, and Next from it)
                goLive(kept.under + kept.columns - 1)
                return 400
            },
            () => {
                kept.top = view().contentY
                watch()
                step(1)
                return 1400
            },
            () => {
                sampler.stop()
                const moving = samples.filter((y, i) => i > 0 && Math.abs(y - samples[i - 1]) > 0.01)
                const straight = moveSteps(samples)
                check("at one speed throughout, switched off: no step much smaller than the rest", moving.length >= 6 && !straight.ok && straight.even,
                      straight.detail)
                check("and slower: more frames in the move than in the quarter-of-a-second ones", moving.length > kept.eased.count, moving.length + " against " + kept.eased.count)
                // ---- A jump: to a slide out of view that is not beside the live one, as a hotkey or a typed number goes
                kept.far = liveIndex + 2 * kept.columns + 1
                check("(a slide out of view to jump to)", kept.far < document.slides.length && shownOf(kept.far, view().contentY) === 0,
                      kept.far + " of " + document.slides.length)
                kept.top = view().contentY
                watch()
                goLive(kept.far)
                return 600
            },
            () => {
                sampler.stop()
                const took = lastMove()
                check("a jump to one out of view brings it in, in about a tenth of a second, whatever is set", inView(kept.far) && took >= 0 && took < 200,
                      took + " ms: " + samples.map(y => Math.round(y)).join(" "))
                check("still through places between", samples.filter(y => y > kept.top + 0.5 && y < view().contentY - 0.5).length >= 2,
                      samples.map(y => Math.round(y)).join(" "))
                // ---- The same by the API, which triggers a slide as HttpApiControl's triggerActive does: by goLiveIn, and
                // not by the window's goLive
                kept.top = view().contentY
                watch()
                goLiveIn(document, Show.liveKey, Show.livePlaylistId, 0)
                return 600
            },
            () => {
                sampler.stop()
                const took = lastMove()
                check("a slide out of view taken live through the API brings the grid to it as quickly (it once left the grid where it was)",
                      liveIndex === 0 && inView(0) && view().contentY < kept.top && took >= 0 && took < 200, liveIndex + ", " + took + " ms")
                // ---- Next held down, early and eased over 0.4 s again: the grid keeps up with the slides while the key is held
                slideScroll = "early"
                slideScrollDuration = 0.4
                slideScrollEased = true
                view().positionViewAtBeginning()
                return 400
            },
            () => {
                kept.top = view().contentY
                kept.presses = Math.min(document.slides.length - 1, 5 * kept.columns)
                watch()
                testInput.keyDown(Qt.Key_Right)
                holder.left = kept.presses
                holder.start()
                return kept.presses * 30 + 900
            },
            () => {
                sampler.stop()
                const held = stamps.filter(s => s.at < kept.letGo).length
                const first = firstMove(kept.top)
                check("Next held down goes through the slides, a slide a press", liveIndex === kept.presses, liveIndex + " of " + kept.presses)
                check("and the grid moves while it is held, not only once it is let go", first >= 0 && first < kept.letGo - kept.from,
                      "first moved " + first + " ms in, let go at " + (kept.letGo - kept.from))
                // How much of the live slide was in view at each moment, from the grid's first move to its being let go
                const from = samples.findIndex(y => Math.abs(y - kept.top) > 0.01)
                const shown = []
                for (let i = Math.max(0, from); i < held; ++i)
                    shown.push(shownOf(stamps[i].live, samples[i]))
                // (In 0.129 the grid did not move until the key was let go; and with the first move eased in over the time
                // set, the live slide was all but out of view for a few frames at the start, 0.05 of it in view)
                check("keeping up: a quarter of the live slide in view at the least, all the while", shown.length > 5 && shown.every(f => f >= 0.25),
                      "least " + Math.min(...shown).toFixed(2) + " of it, in " + shown.length + " samples: " + shown.map(f => f.toFixed(2)).join(" "))
                // (Never stopping between rows: no four samples, 60 ms, at one place while the key was held after it set off)
                let still = 0, longest = 0
                for (let i = Math.max(1, from); i < held; ++i) {
                    still = Math.abs(samples[i] - samples[i - 1]) < 0.01 ? still + 1 : 0
                    longest = Math.max(longest, still)
                }
                check("and smoothly: never back, and not stopping and starting again at each row", longest < 4
                      && samples.every((y, i) => i === 0 || y >= samples[i - 1] - 0.01), "standing for " + longest + " samples at most: "
                      + samples.map(y => Math.round(y)).join(" "))
                check("let go, the live slide and the row under it in view", inView(liveIndex) && inView(Math.min(liveIndex + kept.columns, document.slides.length - 1)))
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
