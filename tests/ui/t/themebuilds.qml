import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A theme's builds, run on the screen the live look dresses in that theme (builds::takeThemeBuilds, Main.qml's
// slideFor and buildStarts): the maintainer's theme slide "L3rd Name - Animated", whose text boxes wipe in with the
// slide and out again some five seconds later: the name After Previous with a delay of 5 s, the underline With
// Previous with 5.2 s, which goes 0.2 s after the name and not 5.2 s (the maintainer's bug report: With Previous had
// been timed from the end of the name's delay), and the role with the underline. Its boxes are empty, and are for the
// words of a slide's boxes of their names: the slides of the Names Library, each a Name and a Role with no builds of
// its own, and each running a macro that makes the look "Stream L3rd Name" live, which gives the stream that theme
// (and here the output too, the stream being drawn nowhere). The first is shown: its macro makes the look live, and
// from then, on the output and in the operator's preview, the name's box is dark while the slide comes in, has the name
// once its wipe is done, and is still there five seconds on; a moment later the whole lower third is gone; the log
// names each build, the underline's going 0.2 s after the name's. Next shows the next, its macro making the same look
// live again: its builds are timed from the slide coming on. Then the theme's slide in the editor: its Build tab has
// the theme's builds in their order, and no Build In for the slide, a theme's slide having no transition of its own;
// a delay changed is in the file, and undone is gone again.
QtObject {
    id: t

//COMMON
    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor

    // The theme's box that takes the slide's words, as shares of the slide (worked out from the theme)
    property rect box: Qt.rect(0, 0, 0, 0)

    function litOut() {
        return testInput.lit(false, box.x, box.y, box.width, box.height)
    }

    // The whole lower third, the name's box to the bottom of the role's
    function litThird() {
        return testInput.lit(false, box.x, box.y, box.width, box.height * 2)
    }

    // When the log says a build began, in seconds of the day
    function loggedAt(pattern) {
        const line = logged(pattern)[0]
        const time = line ? /^(\d+):(\d+):([\d.]+)/.exec(line) : null
        return time ? +time[1] * 3600 + +time[2] * 60 + +time[3] : NaN
    }

    function litPreview() {
        const preview = named("previewSlide")
        const at = preview.mapToItem(null, preview.width * box.x, preview.height * box.y)
        return testInput.windowLighter(at.x, at.y, preview.width * box.width, preview.height * box.height, 40)
    }

    function drawn() {
        return "output " + litOut().toFixed(3) + ", preview " + litPreview().toFixed(3)
    }

    function until(seconds) {
        return Math.max(0, Math.round(kept.t0 + seconds * 1000 - testInput.now()))
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0.6
                kept.screen = Screens.audience[0].id
                const theme = Themes.theme("New Life Chapel")
                const animated = theme ? theme.slides.find(s => s.name === "L3rd Name - Animated") : undefined
                check("(the maintainer's theme has its animated lower third)", animated !== undefined)
                kept.themeSlide = animated.id
                const name = animated.slide.elements.find(e => e.name === "Name")
                box = Qt.rect(name.x / 1920, name.y / 1080, name.width / 1920, name.height / 1080)
                check("the theme slide's map has its builds, by its elements", animated.slide.builds !== undefined
                      && Object.keys(animated.slide.builds).length === 3 && animated.slide.builds[name.id].inStep === 0,
                      JSON.stringify(animated.slide.builds ? Object.keys(animated.slide.builds) : []))
                // The look the name slides' macro goes to gives the stream the theme; here it gives it the output as well,
                // the stream being drawn nowhere in this workspace
                const look = Looks.looks.find(l => l.name === "Stream L3rd Name")
                check("(the look the name slides' macro goes to)", look !== undefined)
                kept.look = look.id
                check("(that look gives the output the animated theme too)", Looks.setScreen(look.id, kept.screen,
                                                                                            { theme: "New Life Chapel", themeSlide: kept.themeSlide }) === "")
                check("(the live look is another, which gives the output another theme)", Looks.live.origin !== kept.look
                      && Looks.liveOf(kept.screen).themeSlide !== kept.themeSlide, Looks.live.name)
                // Two slides of the Names Library, one after the other, each a Name and a Role and no builds of its own
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    const d = entry("Names Library")
                    if (!d)
                        continue
                    const slides = catalog.open(d.file).slides
                    const named = s => s.media === undefined && s.builds === undefined
                                       && s.elements.some(e => e.name === "Name" && testInput.plain(e.displayText).trim() !== "")
                    const at = slides.findIndex((s, i) => named(s) && slides[i + 1] !== undefined && named(slides[i + 1]))
                    if (at >= 0) {
                        kept.entry = d
                        kept.index = at
                        break
                    }
                }
                check("(two name slides of the Names Library, one after the other, without builds)", kept.entry !== undefined,
                      kept.entry ? kept.entry.name + " slide " + (kept.index + 1) : "none")
                openEntry(kept.entry)
                return 800
            },
            () => {
                check("(the presentation has no transition of its own)", document.transition.renderId === undefined, JSON.stringify(document.transition))
                kept.shownAt = testInput.now()
                click(centre(slideCell(kept.index)))
                return 100
            },
            () => {
                const times = buildTimesFor(kept.screen)
                kept.t0 = times ? times[0] : 0
                check("the slide's macro made the look live, which gives the output the theme", Looks.live.origin === kept.look
                      && Looks.liveOf(kept.screen).themeSlide === kept.themeSlide, Looks.live.name)
                check("the slide has no builds, the theme does: they run on the output from when it was given the theme there, as the slide came on",
                      liveSlide.builds === undefined && buildStarts[kept.screen] !== undefined && times !== null
                      && Math.abs(kept.t0 - kept.shownAt - 600) < 250, JSON.stringify(times) + " shown at " + kept.shownAt)
                return until(-0.3)
            },
            () => {
                check("while the slide comes in, the name's box is dark, on the output and in the preview", litOut() < 0.005 && litPreview() < 0.005, drawn())
                return until(1.6)
            },
            () => {
                const dressed = slideFor(kept.screen, liveSlide)
                const name = dressed.elements.find(e => e.name === "Name")
                check("the slide's name is in the theme's box called Name, which has the theme's builds", name !== undefined
                      && testInput.plain(name.displayText).trim() !== "" && Math.abs(name.y / 1080 - box.y) < 0.01 && dressed.builds !== undefined
                      && dressed.builds[name.id] !== undefined && dressed.builds[name.id].inStep === 0,
                      name ? JSON.stringify([name.y, testInput.plain(name.displayText)]) : "none")
                testInput.grabOutput("wiped-in")
                check("once it has wiped in, the name is there", litOut() > 0.02 && litPreview() > 0.02, drawn())
                check("the log names the build", logged(/slide layer: Build In of "Name", Wipe over 0\.60 s/).length === 1,
                      logged(/Build In of/).join(" | "))
                return until(5.4)
            },
            () => {
                check("five seconds on, the name is still there", litOut() > 0.02 && litPreview() > 0.02, drawn())
                return until(7.0)
            },
            () => {
                check("a moment later the whole lower third is gone: name, underline and role", litThird() < 0.005 && litPreview() < 0.005,
                      litThird().toFixed(3) + " " + drawn())
                const name = loggedAt(/slide layer: Build Out of "Name"/)
                const underline = loggedAt(/slide layer: Build Out of "Underline"/)
                const role = loggedAt(/slide layer: Build Out of "Role"/)
                check("the underline, With Previous with 5.2 s after the name After Previous with 5, went 0.2 s after the name, and the role with it",
                      Math.abs(underline - name - 0.2) < 0.1 && Math.abs(role - underline) < 0.1,
                      [name, underline, role].map(t => t.toFixed(3)).join(" "))
                kept.shownAt = testInput.now()
                testInput.key(Qt.Key_Right)
                return 100
            },
            () => {
                const times = buildTimesFor(kept.screen)
                kept.t0 = times ? times[0] : 0
                check("Next: the next name, its macro making the same look live again; the output already has the theme, and the builds are timed from the slide",
                      Show.liveIndex === kept.index + 1 && Looks.live.origin === kept.look && buildStarts[kept.screen] === undefined && times !== null
                      && Math.abs(kept.t0 - kept.shownAt - 600) < 250, JSON.stringify(times) + " shown at " + kept.shownAt)
                return until(-0.3)
            },
            () => {
                check("the next name is not there while its slide comes in", litOut() < 0.005 && litPreview() < 0.005, drawn())
                return until(1.6)
            },
            () => {
                check("and then it wipes in", litOut() > 0.02 && litPreview() > 0.02, drawn())
                Looks.setScreen(Looks.live.id, kept.screen, { theme: "" })
                return 900
            },
            () => {
                check("the theme taken off the output: the slide is shown as it is, without builds", slideFor(kept.screen, liveSlide).builds === undefined
                      && buildStarts[kept.screen] === undefined)
                Show.clearSlide()
                startEditingTheme("New Life Chapel", kept.themeSlide)
                return 1200
            },
            // ---- The theme's slide in the editor
            () => {
                check("(the editor is on the theme slide)", editing && editor.kind === "theme" && canvas.slide.label === "L3rd Name - Animated",
                      editing ? canvas.slide.label : "not editing")
                check("a theme's slides have a Build tab", named("tabBuild") !== null && named("tabBuild").visible)
                click(centre(named("tabBuild")))
                canvas.pick("")
                return 400
            },
            () => {
                const builds = editor.builds(canvas.row)
                check("it has the theme's six builds, in their order", builds.length === 6 && !builds[0].out && builds[0].start === "withSlide"
                      && named("buildRow5") !== null, JSON.stringify(builds.map(b => [b.out, b.start, b.delay])))
                check("and, with nothing picked, no Build In for the slide: a theme's slide has no transition of its own",
                      named("slideBuildName") === null || !named("slideBuildName").visible)
                kept.build = builds[1]
                check("a delay changed", editor.changeBuild(canvas.row, builds[1].id, { delay: 0.5 }) === "" && editor.builds(canvas.row)[1].delay === 0.5)
                return 300
            },
            () => {
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 400
            },
            () => {
                check("and undone, it is as it was", editor.builds(canvas.row)[1].delay === kept.build.delay, JSON.stringify(editor.builds(canvas.row)[1]))
                stopEditing()
                return 900
            },
            () => {
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR|qrc:/.test(l) && !l.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
