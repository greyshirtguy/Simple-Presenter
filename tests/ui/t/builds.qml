import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// ProPresenter's builds (src/builds.h, qml/BuildRun.qml), on the test's own presentation "Builds" (prepare_builds in
// run.py): blocks of colour in the four quarters of the slide, each of which builds in or out, or not at all. Watched on
// the output and in the operator's preview, by the colour in each block at moments worked out from the times the
// window gave the slide's steps: a block that builds in is not there before its build and is there after it, one that
// builds out the other way round, and a wipe half way is half there. The presentation's own transition is made for a
// slide that has none of its own, and a slide's own in place of it (the log says which); Next on a slide with On
// Click builds runs them, one at a time, before it goes on, and so does a click on the slide; the grid's thumbnail
// shows the slide whole throughout. Then the editor's Build tab, on the slide with none: a Build In added to its
// block, given another start, a delay, another transition and a direction; a Build Out added, the two put in another
// order by dragging, which is undone; the Build Out taken off, the Build In reset; the slide's own transition taken
// off. What the file then holds is looked at by after_builds in run.py.
QtObject {
    id: t

//COMMON
    // The middles of the four blocks, and points near the left and the right of the green one, as shares of the slide
    readonly property point atRed: Qt.point(430 / 1920, 0.25)
    readonly property point atGreen: Qt.point(1490 / 1920, 0.25)
    readonly property point atBlue: Qt.point(430 / 1920, 0.75)
    readonly property point atWhite: Qt.point(1490 / 1920, 0.75)
    readonly property point greenLeft: Qt.point(1200 / 1920, 0.25)
    readonly property point greenRight: Qt.point(1780 / 1920, 0.25)

    readonly property string red: "#ff0000"
    readonly property string green: "#00ff00"
    readonly property string blue: "#0000ff"
    readonly property string white: "#ffffff"
    readonly property string black: "#000000"

    function out(p) {
        return String(testInput.pixel(false, p.x, p.y))
    }

    function seen(p) {
        const preview = named("previewSlide")
        const at = preview.mapToItem(null, preview.width * p.x, preview.height * p.y)
        return String(testInput.windowPixel(at.x, at.y))
    }

    function is(colour, wanted) {
        const part = (c, i) => parseInt(String(c).slice(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(colour, i) - part(wanted, i)) <= 40)
    }

    // Whether the output and the operator's preview both have these colours at these points, and what they have
    function shows(wanted) {
        return wanted.every(([p, colour]) => is(out(p), colour) && is(seen(p), colour))
    }

    function drawn(points) {
        return "output " + points.map(p => out(p)).join(" ") + "; preview " + points.map(p => seen(p)).join(" ")
    }

    // The grid's thumbnail of a slide, at a point of it
    function thumbnailAt(index, p) {
        const cell = slideCell(index)
        const slide = Lib.find(cell, item => item.slideWidth !== undefined && item.buildTimes !== undefined)
        const at = slide.mapToItem(null, slide.width * p.x, slide.height * p.y)
        return String(testInput.windowPixel(at.x, at.y))
    }

    // How long until a moment of the slide's first step, `seconds` after it began, in milliseconds
    function until(seconds) {
        return Math.max(0, Math.round(kept.t0 + seconds * 1000 - testInput.now()))
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor

    // The builds of the slide being edited, as the editor reads them from what it holds
    function edited() {
        return editor.builds(canvas.row)
    }

    function said(builds) {
        return JSON.stringify(builds.map(b => [b.out ? "out" : "in", b.start, b.delay, b.transition.name, b.transition.values]))
    }

    // The Build tab's part of the inspector, and a thing in it by what it shows
    function buildTab() {
        return Lib.find(win.contentItem, item => item.buildList !== undefined && item.pickedBuild !== undefined)
    }

    function shownIn(item, test) {
        return Lib.find(item, test)
    }

    function run() {
        steps = [
            () => {
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    if (entry("Builds"))
                        break
                }
                check("(the test's presentation is there)", entry("Builds") !== undefined)
                openEntry(entry("Builds"))
                // (Long enough for the grid's thumbnails to have been drawn, which they
                // are after the presentation opens: at 800 ms they once were not.)
                return 1500
            },
            () => {
                const slides = document.slides
                check("the slides with builds say when each element comes and goes, and a slide without has nothing of it",
                      slides[0].builds !== undefined && Object.keys(slides[0].builds).length === 3 && slides[0].buildSteps === undefined
                      && slides[1].buildSteps === 2 && slides[2].builds === undefined,
                      JSON.stringify(slides.map(s => [s.builds ? Object.keys(s.builds).length : 0, s.buildSteps ?? 0])))
                check("a slide's own transition is in its map, and the presentation's in the presentation's",
                      slides[1].transition !== undefined && slides[1].transition.name === "Cut" && slides[2].transition.name === "Wipe"
                      && slides[0].transition === undefined && document.transition.name === "Dissolve" && document.transition.duration === 1.5,
                      JSON.stringify([slides[1].transition, document.transition]))
                check("the grid's thumbnail shows the slide whole", is(thumbnailAt(0, atRed), red) && is(thumbnailAt(0, atGreen), green)
                      && is(thumbnailAt(0, atBlue), blue) && is(thumbnailAt(0, atWhite), white),
                      [atRed, atGreen, atBlue, atWhite].map(p => thumbnailAt(0, p)).join(" "))
                kept.shownAt = testInput.now()
                click(centre(slideCell(0)))
                return 100
            },
            () => {
                kept.t0 = win.liveBuildTimes ? win.liveBuildTimes[0] : 0
                check("its builds are timed from the end of the presentation's own transition, a second and a half on",
                      win.liveBuildTimes !== null && win.liveBuildTimes.length === 1 && Math.abs(kept.t0 - kept.shownAt - 1500) < 150,
                      JSON.stringify(win.liveBuildTimes) + " shown at " + kept.shownAt)
                check("the log says the presentation's own transition was made", logged(/slide layer: Dissolve, the presentation's own, over 1\.50 s/).length >= 1)
                return until(-0.4)
            },
            () => {
                check("while the slide comes in, the block that builds in with it is not there yet", shows([[atRed, black]]), drawn([atRed, atWhite]))
                return until(0.8)
            },
            () => {
                check("built in: the red block is there, the green one, which builds in after it a second on, is not, and the blue and the white are",
                      shows([[atRed, red], [atGreen, black], [atBlue, blue], [atWhite, white]]), drawn([atRed, atGreen, atBlue, atWhite]))
                return until(1.8)
            },
            () => {
                check("half way through its wipe from the left, the green block is there at its left and not at its right",
                      shows([[greenLeft, green], [greenRight, black]]), drawn([greenLeft, greenRight]))
                testInput.grabOutput("half-way")
                testInput.grab("half-way-operator")
                return until(2.6)
            },
            () => {
                check("the green block is there once its wipe is done, and the blue one still", shows([[atGreen, green], [atBlue, blue]]),
                      drawn([atGreen, atBlue]))
                return until(4.0)
            },
            () => {
                check("a second after that, the blue block has built out, and the others are there", shows([[atBlue, black], [atRed, red], [atGreen, green],
                                                                                                          [atWhite, white]]),
                      drawn([atRed, atGreen, atBlue, atWhite]))
                check("the thumbnail still shows the slide whole", is(thumbnailAt(0, atBlue), blue) && is(thumbnailAt(0, atRed), red), thumbnailAt(0, atBlue))
                check("each build is in the log, with its transition, and how it went",
                      logged(/slide layer: Build In of "Green", Wipe over 0\.80 s, shaders\/wipe\.frag\.qsb/).length === 1
                      && logged(/slide layer: Build Out of "Blue", Dissolve over 0\.40 s/).length === 1
                      && logged(/slide layer: the build of "Green" done: \d+ frames/).length === 1,
                      logged(/Build In|Build Out|the build of/).join(" | "))
                testInput.key(Qt.Key_Right)
                return 250
            },
            () => {
                check("Next on a slide with no On Click builds goes to the next slide", Show.liveIndex === 1, Show.liveIndex)
                check("which comes in with its own transition, a cut: what is there of it is there at once, and its On Click builds have not run",
                      shows([[atBlue, blue], [atRed, black], [atGreen, black]]), drawn([atRed, atGreen, atBlue]))
                testInput.key(Qt.Key_Right)
                return 800
            },
            () => {
                check("Next runs its first On Click build, and not the next slide", Show.liveIndex === 1, Show.liveIndex)
                check("the red block built in, and the green one with it, the blue one still there", shows([[atRed, red], [atGreen, green], [atBlue, blue]]),
                      drawn([atRed, atGreen, atBlue]))
                check("the log says which build of which slide that was", logged(/build 1 of 2 of slide 2 of "Builds"/).length === 1)
                check("the times of its steps are two", win.liveBuildTimes !== null && win.liveBuildTimes.length === 2, JSON.stringify(win.liveBuildTimes))
                click(centre(slideCell(1)))
                return 700
            },
            () => {
                check("a click on the slide on show runs its next On Click build, and does not start it again",
                      Show.liveIndex === 1 && shows([[atBlue, black], [atRed, red], [atGreen, green]]), drawn([atRed, atGreen, atBlue]))
                testInput.key(Qt.Key_Right)
                return 900
            },
            () => {
                check("with none left, Next goes on to the next slide", Show.liveIndex === 2, Show.liveIndex)
                check("which comes in with its own transition, a wipe", logged(/slide layer: Wipe, the slide's own, over 0\.50 s/).length === 1,
                      logged(/slide layer: Wipe/).join(" | "))
                check("a slide without builds has no times for them", win.liveBuildTimes === null)
                check("and is shown whole", shows([[atWhite, white], [atRed, black]]), drawn([atWhite, atRed]))
                testInput.key(Qt.Key_Left)
                return 250
            },
            () => {
                check("back on the slide of On Click builds, they start again: none has run", Show.liveIndex === 1
                      && shows([[atRed, black], [atBlue, blue]]), drawn([atRed, atBlue]))
                Show.clearSlide()
                startEditing(currentEntry(), document.slides[2].id)
                return 900
            },
            // ---- The editor's Build tab
            () => {
                check("(the editor is on the slide with no builds)", canvas.slide !== null && canvas.slide.label === "Own", canvas.slide ? canvas.slide.label : "none")
                check("a presentation's slides have a Build tab", named("tabBuild") !== null && named("tabBuild").visible)
                click(centre(named("tabBuild")))
                canvas.pick("")
                return 400
            },
            () => {
                check("with nothing picked, it has the slide's own Build In, its transition", buildTab() !== null && named("slideBuildName").visible
                      && named("slideBuildName").text === "Wipe", named("slideBuildName") ? named("slideBuildName").text : "none")
                check("and a Build Order that says there is none", shownIn(buildTab(), i => i.text === "No element of the slide has a build." && i.visible) !== null)
                canvas.pick(canvas.elements[0].id)
                return 300
            },
            () => {
                check("an element with no builds has Add Build for its Build In and for its Build Out", named("addbuildIn").visible && named("addbuildOut").visible)
                click(centre(named("addbuildIn")))
                return 400
            },
            () => {
                const builds = edited()
                check("Add Build gives it a Build In: On Click, a Dissolve, with no delay", builds.length === 1 && !builds[0].out && builds[0].start === "click"
                      && builds[0].transition.name === "Dissolve" && builds[0].delay === 0, said(builds))
                check("which is a step to undo", editor.canUndo)
                check("the Build Order has it, by its element's name", named("buildRow0") !== null && /White.*In: Dissolve/.test(shownIn(named("buildRow0"), i => i.textFormat === Text.StyledText).text))
                check("and Start says On Click", named("buildStart").currentText === "On Click", named("buildStart").currentText)
                named("buildStart").activated(1)
                return 300
            },
            () => {
                check("Start set to With Previous", edited()[0].start === "withPrevious", said(edited()))
                named("buildDelay").edited(1.5)
                return 300
            },
            () => {
                check("a delay typed in", edited()[0].delay === 1.5, said(edited()))
                click(centre(named("buildInChange")))
                return 300
            },
            () => {
                check("Change has the transitions, under their categories, the one it has ticked", /\*Dissolve/.test(labels()) && /\[Wipes\]/.test(labels()), labels())
                click(menuRow("Wipe"))
                return 400
            },
            () => {
                const build = edited()[0]
                check("another transition chosen: a Wipe, with ProPresenter's Direction, its start and delay as they were",
                      build.transition.name === "Wipe" && build.transition.values.Direction !== undefined && build.start === "withPrevious" && build.delay === 1.5,
                      said(edited()))
                const fromLeft = shownIn(buildTab(), i => i.text === "→" && i.available !== undefined)
                check("(the pad of directions is there)", fromLeft !== null)
                click(centre(fromLeft))
                return 300
            },
            () => {
                check("a direction picked: from the left", edited()[0].transition.values.Direction === 3, said(edited()))
                click(centre(named("addbuildOut")))
                return 400
            },
            () => {
                const builds = edited()
                check("a Build Out added: after the Build In in the order", builds.length === 2 && !builds[0].out && builds[1].out, said(builds))
                testInput.grab("build-tab")
                const from = centre(named("buildRow1"))
                const to = named("buildRow0").mapToItem(null, named("buildRow0").width / 2, 2)
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
                kept.landingShown = named("buildLanding").visible
                testInput.mouse(2, to.x, to.y)
                return 400
            },
            () => {
                check("while one is dragged, where it would land is shown", kept.landingShown)
                check("dragged above the other, the Build Out is first", edited().length === 2 && edited()[0].out && !edited()[1].out, said(edited()))
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 400
            },
            () => {
                check("undone, the order is as it was", edited().length === 2 && !edited()[0].out && edited()[1].out, said(edited()))
                click(centre(named("buildOutRemove")))
                return 400
            },
            () => {
                check("the Build Out taken off; the Build In is left", edited().length === 1 && !edited()[0].out, said(edited()))
                check("and the element has Add Build for its Build Out again", named("addbuildOut").visible)
                click(centre(named("buildInReset")))
                return 400
            },
            () => {
                const build = edited()[0]
                check("Reset to Default Parameters puts the Wipe back as ProPresenter has it: from the top, its start and delay as they were",
                      build.transition.name === "Wipe" && build.transition.values.Direction === 1 && build.start === "withPrevious" && build.delay === 1.5,
                      said(edited()))
                canvas.pick("")
                return 300
            },
            () => {
                testInput.grab("build-tab-slide")
                click(centre(named("slideBuildRemove")))
                return 400
            },
            () => {
                check("the slide's own transition taken off: it says what the slide comes in with instead",
                      canvas.slide.transition === undefined && shownIn(buildTab(), i => /its presentation's transition, Dissolve/.test(i.text ?? "") && i.visible) !== null,
                      JSON.stringify(canvas.slide.transition))
                testInput.snapshot(document.path, "builds-edited")
                stopEditing()
                return 900
            },
            () => {
                check("the presentation read again: the block of the third slide builds in", document.slides[2].builds !== undefined
                      && Object.keys(document.slides[2].builds).length === 1 && document.slides[2].transition === undefined,
                      JSON.stringify(document.slides[2].builds))
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR|qrc:/.test(l) && !l.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
