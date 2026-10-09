import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// New slides, copying and pasting slides, turning elements, and the hotkeys a new installation starts with and their badges.
// Run with settings of its own that nothing has been saved in.
QtObject {
    id: t

//COMMON
    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor

    function picked() {
        return canvas.selected
    }

    function handle(hx, hy) {
        return Lib.findAll(canvas, i => i.hx !== undefined && i.hy !== undefined).find(i => i.hx === hx && i.hy === hy)
    }

    function words(slide) {
        return slide.elements.map(e => e.words).join(" / ")
    }

    function ids(slide) {
        return slide.elements.map(e => e.id).join(",")
    }

    // A drag with a key held, by way of a number of points
    function dragThrough(points, modifiers) {
        testInput.mouse(0, points[0].x, points[0].y, modifiers)
        for (let i = 1; i < points.length; ++i)
            testInput.mouse(1, points[i].x, points[i].y, modifiers)
        testInput.mouse(2, points[points.length - 1].x, points[points.length - 1].y, modifiers)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                const keys = groups.filter(g => g.key).map(g => g.name + "=" + g.key).join(" ")
                check("a new installation has hotkeys for the verse, the chorus and the bridge", keys === "Verse=V Chorus=C Bridge=B", keys)
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                return 600
            },
            () => {
                const chorus = document.slides.findIndex(s => s.groupStart && /^chorus/i.test(s.group))
                const verse = document.slides.findIndex(s => s.groupStart && /^verse/i.test(s.group))
                kept.chorus = chorus
                check("the presentation has a verse and a chorus", chorus > 0 && verse >= 0, verse + " and " + chorus)
                check("which slides the keys go to", groupKeyAt[chorus] === "C" && groupKeyAt[verse] === "V" && Object.keys(groupKeyAt).length <= 3, JSON.stringify(groupKeyAt))
                const badge = Lib.find(slideCell(chorus), i => i.objectName === "hotkeyBadge")
                const letter = badge ? Lib.find(badge, i => i.text !== undefined) : null
                check("the chorus's first slide has an orange badge with its key, a little see-through", badge !== null && letter !== null && letter.text === "C"
                      && Math.abs(badge.opacity - 0.8) < 0.01 && String(badge.color) === "#ff8a1f", badge ? badge.opacity + " " + badge.color : "none")
                check("the slide after it has none", Lib.find(slideCell(chorus + 1), i => i.objectName === "hotkeyBadge") === null)
                slideCell(chorus)
                testInput.grab("1-badges")
                goLive(0)
                testInput.key(Qt.Key_C, 0, "c")
                check("and the key goes there", liveIndex === chorus, liveIndex)
                // ---- copy and paste in the grid
                kept.count = document.slides.length
                kept.source = document.slides[2]
                showSlideMenu(2, slideCell(2), 20, 20)
                return 300
            },
            () => {
                check("a slide's menu has Copy and Paste, and Paste waits for a copy", labels().endsWith("[Slide], Copy, Paste(off), Delete Slide…"), labels())
                click(menuRow("Copy"))
                return 300
            },
            () => {
                check("copied, nothing has changed", document.slides.length === kept.count && catalog.hasCopiedSlide)
                kept.after = document.slides[kept.chorus].id
                showSlideMenu(kept.chorus, slideCell(kept.chorus), 20, 20)
                return 300
            },
            () => {
                check("now Paste can be chosen", labels().endsWith("[Slide], Copy, Paste, Delete Slide…"), labels())
                click(menuRow("Paste"))
                return 500
            },
            () => {
                const pasted = document.slides[kept.chorus + 1]
                check("pasted, the copy is the slide after the one whose menu it was", document.slides.length === kept.count + 1 && document.slides[kept.chorus].id === kept.after
                      && words(pasted) === words(kept.source) && words(pasted) !== "", words(pasted))
                check("it is a slide of its own, and so is everything on it", pasted.id !== kept.source.id && pasted.elements.length === kept.source.elements.length
                      && pasted.elements.every((e, i) => e.id !== kept.source.elements[i].id), pasted.id)
                check("in the group of the slide it went after", pasted.group === document.slides[kept.chorus].group && !pasted.groupStart, pasted.group)
                check("the slide that was live still is", liveIndex === kept.chorus && liveDocument.slides[liveIndex].id === kept.after, liveIndex)
                const again = catalog.open(document.path).slides
                check("and it is in the file", again.length === kept.count + 1 && again[kept.chorus + 1].id === pasted.id && ids(again[kept.chorus + 1]) === ids(pasted))
                check("the one it was copied from is as it was", ids(document.slides[2]) === ids(kept.source))
                // The second slide of the example is the one with shapes, whose ellipse shows only... nothing; a copy of it keeps its pictures
                kept.pastedId = pasted.id
                // ---- the editor: a new slide
                startEditing(currentEntry(), document.slides[1].id)
                return 600
            },
            () => {
                kept.rows = editor.count
                kept.row = canvas.row
                kept.shapes = canvas.slide.id
                editScreen.showRowMenu(canvas.slide, 30, 30)
                return 300
            },
            () => {
                check("in the editor a slide's menu has New Slide, Copy and Paste", labels() === "[Slide 2], New Slide, Copy, Paste, Delete Slide…", labels())
                click(menuRow("New Slide"))
                return 600
            },
            () => {
                check("a new slide comes after it, with nothing on it, and is the one being worked on", editor.count === kept.rows + 1 && canvas.row === kept.row + 1
                      && canvas.slide !== null && canvas.slide.elements.length === 0 && canvas.slide.id !== kept.shapes, canvas.row + " of " + editor.count)
                check("it is the size of the others", canvas.slide.width === 1920 && canvas.slide.height === 1080, canvas.slide.width + "x" + canvas.slide.height)
                canvas.addShape("rectangle")
                kept.rect = picked().id
                check("and can be drawn on", canvas.slide.elements.length === 1 && picked().shape === "rectangle")
                // ---- turning
                canvas.setProperties({ x: 700, y: 400, width: 400, height: 200 }, false)
                canvas.setProperties({ rotation: 30 }, false)
                check("an element can be turned", picked().rotation === 30)
                const frame = handle(1, 1).parent
                check("its frame and handles turn with it", frame.rotation === 30)
                const field = named("rotation")
                check("the inspector says how far", field !== null && field.value === 30, field ? field.value : "none")
                // A point inside the turned box and outside the upright one, and the other way about
                const at = (sx, sy) => canvas.elementAt(canvas.originX + sx * canvas.u, canvas.originY + sy * canvas.u)
                check("a click finds it where it is drawn, turned", at(1050, 590) !== null && at(1090, 410) === null, (at(1050, 590) !== null) + " " + (at(1090, 410) === null))
                // Ctrl and a corner handle: a quarter of the way round the middle
                const middle = frame.mapToItem(null, frame.width / 2, frame.height / 2)
                const corner = handle(1, 1).mapToItem(null, 9, 9)
                const round = (degrees) => {
                    const a = degrees * Math.PI / 180
                    const dx = corner.x - middle.x
                    const dy = corner.y - middle.y
                    return Qt.point(middle.x + dx * Math.cos(a) - dy * Math.sin(a), middle.y + dx * Math.sin(a) + dy * Math.cos(a))
                }
                dragThrough([corner, round(20), round(40), round(60)], Qt.ControlModifier)
                return 300
            },
            () => {
                check("dragging a corner handle with Ctrl held turns it", Math.abs(picked().rotation - 90) < 0.5 && picked().width === 400 && picked().height === 200, picked().rotation)
                // it settled on the quarter turn, being within two degrees of it
                canvas.setProperties({ rotation: 30 }, false)
                // the right-hand side dragged outwards along the element's own length
                const side = handle(1, 0.5)
                const from = side.mapToItem(null, 9, 9)
                const opposite = handle(0, 0.5).mapToItem(null, 9, 9)
                kept.opposite = opposite
                const a = 30 * Math.PI / 180
                const far = 100 * canvas.u
                drag(from, Qt.point(from.x + far * Math.cos(a), from.y + far * Math.sin(a)))
                return 300
            },
            () => {
                const now = handle(0, 0.5).mapToItem(null, 9, 9)
                check("a turned element is resized along its own sides", picked().width === 500 && picked().height === 200 && picked().rotation === 30, picked().width + "x" + picked().height)
                check("and the side opposite the handle stays where it was", Math.abs(now.x - kept.opposite.x) < 1.5 && Math.abs(now.y - kept.opposite.y) < 1.5, now.x + "," + now.y + " was " + kept.opposite.x + "," + kept.opposite.y)
                testInput.grab("2-turned")
                canvas.undo()
                check("which can be undone", picked().width === 400)
                canvas.redo()
                canvas.setProperties({ rotation: 400 }, false)
                check("a turn of more than a whole one is kept as what is left over", picked().rotation === 40, picked().rotation)
                canvas.editText(kept.rect)
                testInput.type("Turned")
                canvas.finishText()
                check("words can be typed into it", picked().words === "Turned", picked().words)
                // ---- copy and paste in the editor
                kept.made = canvas.slide.id
                editScreen.showRowMenu(canvas.slide, 30, 30)
                return 300
            },
            () => {
                click(menuRow("Copy"))
                return 300
            },
            () => {
                editScreen.showRow(0)
                editScreen.showRowMenu(canvas.slide, 30, 30)
                return 300
            },
            () => {
                click(menuRow("Paste"))
                return 600
            },
            () => {
                check("pasted in the editor, the copy comes after the slide whose menu it was and is the one being worked on", editor.count === kept.rows + 2 && canvas.row === 1
                      && canvas.slide.id !== kept.made && canvas.slide.elements.length === 1 && canvas.slide.elements[0].words === "Turned"
                      && canvas.slide.elements[0].rotation === 40 && canvas.slide.elements[0].id !== kept.rect, canvas.row + " of " + editor.count)
                stopEditing()
                return 500
            },
            () => {
                check("back at the show, the slides added in the editor are there", document.slides.length === kept.count + 3 && document.slides[1].elements.length === 1
                      && document.slides[1].elements[0].rotation === 40, document.slides.length)
                const again = catalog.open(document.path).slides
                check("and in the file", again.length === kept.count + 3 && again.filter(s => words(s) === "Turned").length === 2)
                output.setFullScreen(true)
                goLive(1)
                return 800
            },
            () => {
                testInput.grabOutput("3-output-turned")
                output.setFullScreen(false)
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says what was done to the slides", lines.some(l => l.includes("copied")) && lines.some(l => l.includes("pasted after slide")) && lines.some(l => l.includes("a slide added in the editor")))
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("was not found in the workspace")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme")).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
