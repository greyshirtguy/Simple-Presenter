import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Quick Edit (qml/QuickEdit.qml, EditorCanvas's text-only mode), on a slide ProPresenter wrote: chosen from the
// slide's menu, beside Edit, it puts the slide up over the window with its text box being typed in, and the output
// keeps what it had; a click in the box puts the caret there, words typed go in, Ctrl+B does nothing, and a drag
// selects rather than moves; Esc puts it away and the file has the words, in the box's own formatting, with the
// box where it was and everything else on the slide as it was, and the grid shows them; a click outside puts it
// away and writes nothing; between boxes, Delete deletes nothing and Tab goes back to typing.
QtObject {
    id: t

//COMMON
    function worded(slide) {
        return slide.elements.filter(e => testInput.plain(e.text).trim() !== "")
    }

    // The one text box with words of a slide that has no actions and no media, or null
    function oneBox(slide) {
        const boxes = worded(slide)
        return slide.actions.length === 0 && slide.media === undefined && boxes.length === 1 ? boxes[0] : null
    }

    function slidesOf(entry) {
        return catalog.open(entry.file).slides
    }

    // An element as the file has it, but for what changes when its words change
    function restOf(e) {
        const rest = {}
        for (const key of Object.keys(e).sort()) {
            if (key !== "text" && key !== "displayText" && key !== "visible" && key !== "words")
                rest[key] = e[key]
        }
        return JSON.stringify(rest)
    }

    function looks(e) {
        return testInput.runsOf(e.text).map(r => [r.family, r.size, r.bold, r.italic, String(r.color)].join(" "))
    }

    // Which of bold and italic the words' stretches are, each way once
    function flags(text) {
        return testInput.runsOf(text).map(r => r.bold + "," + r.italic).filter((v, i, a) => a.indexOf(v) === i).join(" ")
    }

    function panel() {
        return quickEditLoader.item
    }

    // A point of a slide's element in the Quick Edit canvas, as fractions of its box, in window coordinates
    function inBox(e, fx, fy) {
        const canvas = panel().canvas
        return canvas.mapToItem(null, canvas.originX + (e.x + fx * e.width) * canvas.u, canvas.originY + (e.y + fy * e.height) * canvas.u)
    }

    // How much of an element's box in the Quick Edit canvas is drawn lighter than grey: its words, on a dark slide
    function lightIn(e) {
        const from = inBox(e, 0, 0)
        const to = inBox(e, 1, 1)
        return testInput.windowLighter(from.x, from.y, to.x - from.x, to.y - from.y, 150)
    }

    function run() {
        steps = [
            () => {
                // ---- A slide with one text box of words, and another slide to have live meanwhile
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = slidesOf(d)
                        const index = slides.findIndex(s => oneBox(s) !== null)
                        if (index >= 0 && slides.length > index + 1) {
                            kept.entry = d
                            kept.index = index
                            kept.id = slides[index].id
                            kept.box = oneBox(slides[index])
                            break
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a slide with one text box of words, and another after it)", kept.entry !== undefined,
                      kept.entry ? kept.entry.name + " slide " + (kept.index + 1) : "none")
                kept.before = slidesOf(kept.entry)
                openEntry(kept.entry)
                return 600
            },
            () => {
                goLive(kept.index + 1)
                return 900
            },
            () => {
                kept.liveIndex = Show.liveIndex
                kept.outputLit = testInput.lit(false, 0, 0, output.width, output.height)
                // A point of the window well away from the slide, to see that it is not darkened
                kept.away = Qt.point(60, win.height - 120)
                kept.awayBefore = testInput.windowPixel(kept.away.x, kept.away.y)
                showSlideMenu(kept.index, slideCell(kept.index), 10, 10)
                return 300
            },
            () => {
                check("a slide's menu has Quick Edit, after Edit", labels().startsWith("Edit, Quick Edit,"), labels())
                click(menuRow("Quick Edit"))
                return 700
            },
            () => {
                const canvas = panel() ? panel().canvas : null
                check("Quick Edit puts the slide up over the window, on its own", panel() !== null && panel().open && named("quickEdit") !== null
                      && canvas.slide !== null && canvas.slide.id === kept.id && canvas.textOnly)
                check("with its text box being typed in", canvas !== null && canvas.editingId === kept.box.id, canvas ? canvas.editingId : "none")
                check("its words drawn there", lightIn(kept.box) > 0.002, lightIn(kept.box))
                // Small, over the slide's own thumbnail: about twice its width, and the thumbnail's middle inside it
                const cell = slideCell(kept.index)
                const box = named("quickEdit")
                const middle = cell.mapToItem(box, cell.width / 2, cell.width * 9 / 32)
                check("about twice the size of the slide's thumbnail, over it", Math.abs(box.width - 2 * cell.width) < 0.2 * cell.width
                      && middle.x > 0 && middle.x < box.width && middle.y > 0 && middle.y < box.height,
                      JSON.stringify([Math.round(box.width), Math.round(cell.width), Math.round(middle.x), Math.round(middle.y)]))
                check("and the window behind is not darkened", testInput.windowPixel(kept.away.x, kept.away.y) === kept.awayBefore,
                      kept.awayBefore + " " + testInput.windowPixel(kept.away.x, kept.away.y))
                check("and the output keeps what it had", Show.liveIndex === kept.liveIndex
                      && Math.abs(testInput.lit(false, 0, 0, output.width, output.height) - kept.outputLit) < 0.001)
                check("the log says so", /quick edit opened on slide/.test(testInput.readText(Log.path)))
                testInput.grab("q01-up")
                // A click at the end of the box's words puts the caret there; Ctrl+End to be sure of the end
                click(inBox(kept.box, 0.98, 0.5))
                testInput.key(Qt.Key_End, Qt.ControlModifier)
                testInput.type(" Amen")
                return 400
            },
            () => {
                const canvas = panel().canvas
                check("what is typed goes in, where the caret is", testInput.plain(canvas.liveText) === testInput.plain(kept.box.text) + " Amen",
                      JSON.stringify(testInput.plain(canvas.liveText)))
                // No formatting in Quick Edit
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.key(Qt.Key_B, Qt.ControlModifier)
                testInput.key(Qt.Key_I, Qt.ControlModifier)
                testInput.key(Qt.Key_End, Qt.ControlModifier)
                return 300
            },
            () => {
                const canvas = panel().canvas
                check("Ctrl+B and Ctrl+I make nothing bold or italic", flags(canvas.liveText) === flags(kept.box.text),
                      flags(canvas.liveText) + " / " + flags(kept.box.text))
                // A drag across the box selects; it does not move it.
                drag(inBox(kept.box, 0.3, 0.5), inBox(kept.box, 0.7, 0.6))
                return 400
            },
            () => {
                const canvas = panel().canvas
                const box = canvas.editor.slideAt(canvas.row).elements.find(e => e.id === kept.box.id)
                check("a drag across the box moves nothing", box.x === kept.box.x && box.y === kept.box.y && box.width === kept.box.width,
                      JSON.stringify([box.x, box.y]))
                testInput.key(Qt.Key_End, Qt.ControlModifier)
                testInput.key(Qt.Key_Escape)
                return 900
            },
            () => {
                // ---- Esc: put away, and kept
                check("Esc puts it away", !panel().open)
                const after = slidesOf(kept.entry)
                const slide = after[kept.index]
                const box = slide.elements.find(e => e.id === kept.box.id)
                check("the file has the words", box !== undefined && testInput.plain(box.text) === testInput.plain(kept.box.text) + " Amen",
                      box ? JSON.stringify(testInput.plain(box.text)) : "none")
                check("in the box's own formatting", box !== undefined && JSON.stringify(looks(box)) === JSON.stringify(looks(kept.box)),
                      box ? JSON.stringify([looks(box), looks(kept.box)]) : "none")
                const was = kept.before[kept.index]
                check("the box is where it was, and nothing else of the slide or the presentation changes", box !== undefined
                      && restOf(box) === restOf(kept.box) && slide.elements.length === was.elements.length
                      && slide.elements.filter(e => e.id !== kept.box.id).every((e, i) => restOf(e) + testInput.plain(e.text) === restOf(was.elements.filter(x => x.id !== kept.box.id)[i]) + testInput.plain(was.elements.filter(x => x.id !== kept.box.id)[i].text))
                      && JSON.stringify(slide.actions) === JSON.stringify(was.actions)
                      && after.length === kept.before.length && after.every((s, i) => i === kept.index || s.plainText === kept.before[i].plainText))
                check("the grid shows the slide as it now is", document.slides[kept.index].plainText.includes("Amen"))
                check("and the output still has what it had", Show.liveIndex === kept.liveIndex
                      && Math.abs(testInput.lit(false, 0, 0, output.width, output.height) - kept.outputLit) < 0.001)
                check("the log says what was kept", /quick edit of slide \d+ of .* put away; what was typed was saved/.test(testInput.readText(Log.path)))
                kept.hash = testInput.fileHash(kept.entry.file)
                quickEdit(kept.index, slideCell(kept.index))
                return 600
            },
            () => {
                check("it comes up again with the words as they now are", panel().open && testInput.plain(panel().canvas.liveText).endsWith(" Amen"))
                // A click outside the panel, at the window's lower left (clear of its edge, which resizes it)
                click(Qt.point(40, win.height - 40))
                return 600
            },
            () => {
                check("a click outside puts it away", !panel().open)
                check("and, with nothing typed, writes nothing", testInput.fileHash(kept.entry.file) === kept.hash)
                check("the log says nothing was changed", /put away; nothing was changed/.test(testInput.readText(Log.path)))
                quickEdit(kept.index, slideCell(kept.index))
                return 600
            },
            () => {
                // ---- Between boxes: a click where there is no text box stops the typing
                const canvas = panel().canvas
                let empty = null
                for (const p of [[0.02, 0.02], [0.98, 0.02], [0.02, 0.98], [0.98, 0.98], [0.5, 0.02], [0.5, 0.98]]) {
                    const at = canvas.mapToItem(null, canvas.originX + p[0] * canvas.slideWidth * canvas.u, canvas.originY + p[1] * canvas.slideHeight * canvas.u)
                    const local = canvas.mapFromItem(null, at.x, at.y)
                    if (!empty && canvas.typableAt(local.x, local.y) === null)
                        empty = at
                }
                kept.empty = empty
                if (empty)
                    click(empty)
                return 300
            },
            () => {
                const canvas = panel().canvas
                if (kept.empty) {
                    check("a click where there is no text box stops the typing", canvas.editingId === "" && canvas.selected === null)
                    testInput.key(Qt.Key_Delete)
                    testInput.key(Qt.Key_Right)
                } else {
                    check("(the text box covers the whole slide: nowhere to click between boxes)", true)
                }
                return 300
            },
            () => {
                const canvas = panel().canvas
                check("Delete and the arrows there do nothing to the slide", canvas.slide.elements.some(e => e.id === kept.box.id)
                      && canvas.slide.elements.find(e => e.id === kept.box.id).x === kept.box.x)
                testInput.key(Qt.Key_Tab)
                return 300
            },
            () => {
                const canvas = panel().canvas
                check("Tab goes back to typing in the text box", canvas.editingId === kept.box.id)
                testInput.key(Qt.Key_Escape)
                return 700
            },
            () => {
                check("and Esc puts it away, the file as it was", !panel().open && testInput.fileHash(kept.entry.file) === kept.hash)
                check("nothing went wrong on the way", !/PROBLEM|ERROR/.test(testInput.readText(Log.path)),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
