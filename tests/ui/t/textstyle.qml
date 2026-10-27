import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Copy Text Style and Paste Text Style (Themes, themefile::dressText), on slides ProPresenter wrote: a slide's text made
// red and moved, its style copied from the show's slide menu and pasted on a live slide of another presentation, whose
// text box keeps its words and its id and takes the red, the place and the name, with everything else in the file as
// it was, and the red drawn at once on the output and in the operator's preview; in the editor's slide menu, pasted
// on a slide with a chord, which stays, and undone, which leaves the file byte for byte as it was; a style copied in
// the editor pasted in the show; a theme put on a slide with a chord, which stays. And pasting into a text box
// (Ctrl+V, Ctrl+Shift+V, and into a box emptied first): only the words come in, in the box's own format and
// alignment, though the clipboard has them green, bold and in Courier.
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

    // An element as the file has it, but for what changes when its words are put back (its text and what is worked
    // out from it)
    function restOf(e) {
        const rest = {}
        for (const key of Object.keys(e).sort()) {
            if (key !== "text" && key !== "displayText" && key !== "visible")
                rest[key] = e[key]
        }
        return JSON.stringify(rest) + testInput.plain(e.text)
    }

    function colours(e) {
        return testInput.runsOf(e.text).map(r => String(r.color))
    }

    function looks(e) {
        return testInput.runsOf(e.text).map(r => [r.family, r.size, r.bold, String(r.color)].join(" "))
    }

    function boxOn(entry, index, id) {
        return slidesOf(entry)[index].elements.find(e => e.id === id)
    }

    function chordsOf(e) {
        return (e && e.chords ? e.chords : []).map(c => c.at + ":" + c.name).join(" ")
    }

    function run() {
        steps = [
            () => {
                // ---- Two presentations' slides, each with one text box with words, and nothing else to do
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = slidesOf(d)
                        const ones = slides.map((s, i) => ({ index: i, box: oneBox(s), id: s.id })).filter(x => x.box !== null)
                        if (!kept.source && ones.length >= 1) {
                            kept.source = { entry: d, index: ones[0].index, id: ones[0].id, box: ones[0].box }
                        } else if (kept.source && !kept.target && d.file !== kept.source.entry.file && ones.length >= 2) {
                            kept.target = { entry: d, index: ones[0].index, id: ones[0].id, box: ones[0].box }
                            kept.other = { entry: d, index: ones[1].index, id: ones[1].id, box: ones[1].box }
                        }
                    }
                    if (kept.target)
                        break
                }
                check("(a slide to copy from, and two slides of another presentation to paste on)", kept.source !== undefined && kept.target !== undefined,
                      kept.source ? kept.source.entry.name + " / " + (kept.target ? kept.target.entry.name : "none") : "none")
                // The source's text red, and its box moved
                startEditing(kept.source.entry, kept.source.id)
                return 800
            },
            () => {
                const editor = editScreen.editor
                const row = editor.rowOf(kept.source.id)
                const length = testInput.plain(kept.source.box.text).length
                check("(the slide copied from is given red words and a place of its own)",
                      editor.formatText(row, kept.source.box.id, 0, length, { color: "#ff0000" }) === ""
                      && editor.setProperties(row, kept.source.box.id, { x: 120, y: 640, width: 1680, height: 360 }) === "")
                kept.sourceName = editor.slideAt(row).elements.find(e => e.id === kept.source.box.id).name
                stopEditing()
                return 800
            },
            () => {
                openEntry(kept.source.entry)
                return 600
            },
            () => {
                showSlideMenu(kept.source.index, slideCell(kept.source.index), 10, 10)
                return 300
            },
            () => {
                check("a slide's menu has Copy Text Style, and Paste Text Style waiting for one", /Copy Text Style, Apply This Text Style to All(\(off\))?, Paste Text Style\(off\)/.test(labels()), labels())
                click(menuRow("Copy Text Style"))
                return 300
            },
            () => {
                check("Copy Text Style keeps it", Themes.hasCopiedTextStyle)
                kept.before = slidesOf(kept.target.entry)
                kept.beforeHash = testInput.fileHash(kept.target.entry.file)
                openEntry(kept.target.entry)
                return 600
            },
            () => {
                // The slide to paste on taken live first, so that what the screens show of a change to it can be seen.
                // (Under a look that gives the output no theme: a theme's boxes are drawn in place of the slide's own.)
                const plain = Looks.looks.find(look => Looks.of(look.id, Screens.audience[0].id).theme === "")
                if (plain)
                    Show.lookId = plain.id
                goLive(kept.target.index)
                return 1500
            },
            () => {
                showSlideMenu(kept.target.index, slideCell(kept.target.index), 10, 10)
                return 300
            },
            () => {
                check("on another presentation's slide, Paste Text Style can be chosen", labels().includes("Copy Text Style, Apply This Text Style to All, Paste Text Style,"), labels())
                click(menuRow("Paste Text Style"))
                return 800
            },
            () => {
                // ---- What is in the file
                const after = slidesOf(kept.target.entry)
                const slide = after[kept.target.index]
                const box = slide.elements.find(e => e.id === kept.target.box.id)
                check("the slide's text box keeps its words and its id", box !== undefined && testInput.plain(box.text) === testInput.plain(kept.target.box.text))
                check("and takes the copied one's look: its red, its place and size, and its name", box !== undefined && colours(box).every(c => c === "#ff0000")
                      && box.x === 120 && box.y === 640 && box.width === 1680 && box.height === 360 && box.name === kept.sourceName,
                      box ? JSON.stringify([colours(box), box.x, box.y, box.width, box.height, box.name, kept.sourceName]) : "none")
                const was = kept.before[kept.target.index]
                check("nothing else on the slide changes, nor its background, nor what it does", slide.elements.length === was.elements.length
                      && slide.elements.filter(e => e.id !== kept.target.box.id).every((e, i) => restOf(e) === restOf(was.elements.filter(x => x.id !== kept.target.box.id)[i]))
                      && JSON.stringify(slide.actions) === JSON.stringify(was.actions) && String(slide.backgroundColor) === String(was.backgroundColor)
                      && slide.drawsBackground === was.drawsBackground)
                check("nor does any other slide of the presentation", after.length === kept.before.length
                      && after.every((s, i) => i === kept.target.index || s.elements.map(restOf).join("|") === kept.before[i].elements.map(restOf).join("|")))
                check("the operator is told, and that the file as it was is kept", notice.includes("copied text style") && notice.includes("backups") && !noticeIsError, notice)
                // ---- What is drawn: the slide is live, and is not shown again
                return 1500
            },
            () => {
                const red = testInput.red(false, 120 / 1920, 640 / 1080, 1680 / 1920, 360 / 1080)
                const preview = named("previewSlide")
                const at = preview.mapToItem(null, 0, 0)
                const seen = testInput.windowRed(at.x, at.y, preview.width, preview.height)
                check("the live slide's words are red on the output at once, where the copied box was, and in the operator's preview", red > 0.003 && seen > 0.002,
                      red + " and " + seen)
                testInput.grabOutput("1-pasted")
                clearSlide()
                // ---- In the editor: a chord on the other slide first
                startEditing(kept.target.entry, kept.other.id)
                return 800
            },
            () => {
                const editor = editScreen.editor
                kept.row = editor.rowOf(kept.other.id)
                check("(a chord put on the other slide)", editor.setChords(kept.row, kept.other.box.id, [{ at: 0, name: "G" }]) === "")
                kept.hash = testInput.fileHash(kept.target.entry.file)
                editScreen.showRowMenu(editor.slideAt(kept.row), 30, 30)
                return 300
            },
            () => {
                check("in the editor a slide's menu has them too", /, Copy, Paste(\(off\))?, Copy Text Style, Apply This Text Style to All, Paste Text Style, Delete Slide…$/.test(labels()), labels())
                click(menuRow("Paste Text Style"))
                return 600
            },
            () => {
                const editor = editScreen.editor
                const box = editor.slideAt(kept.row).elements.find(e => e.id === kept.other.box.id)
                check("pasted in the editor, the slide's box takes the look and keeps its words", box !== undefined && colours(box).every(c => c === "#ff0000")
                      && box.x === 120 && box.y === 640 && testInput.plain(box.text) === testInput.plain(kept.other.box.text),
                      box ? JSON.stringify([colours(box), box.x, box.y]) : "none")
                check("and the chord over its words", chordsOf(box) === "0:G" && chordsOf(boxOn(kept.target.entry, kept.other.index, kept.other.box.id)) === "0:G",
                      chordsOf(box))
                check("it is a step that can be undone", editor.canUndo)
                editor.undo()
                return 600
            },
            () => {
                check("undone, the file is byte for byte as it was", testInput.fileHash(kept.target.entry.file) === kept.hash)
                // Copied in the editor, from the slide as it is now
                editScreen.showRowMenu(editScreen.editor.slideAt(kept.row), 30, 30)
                return 300
            },
            () => {
                click(menuRow("Copy Text Style"))
                return 300
            },
            () => {
                stopEditing()
                return 800
            },
            () => {
                openEntry(kept.target.entry)
                return 600
            },
            () => {
                showSlideMenu(kept.target.index, slideCell(kept.target.index), 10, 10)
                return 300
            },
            () => {
                click(menuRow("Paste Text Style"))
                return 800
            },
            () => {
                const box = boxOn(kept.target.entry, kept.target.index, kept.target.box.id)
                const own = boxOn(kept.target.entry, kept.other.index, kept.other.box.id)
                check("a style copied in the editor is pasted in the show: the first slide's box looks as the other's does again", box !== undefined
                      && own !== undefined && looks(box).join() === looks(own).join() && box.x === own.x && box.y === own.y && box.width === own.width,
                      box ? JSON.stringify([looks(box), looks(own), box.x, own.x]) : "none")
                // ---- A theme put on the slide with the chord
                check("(the workspace has a theme)", Themes.themes.length > 0 && Themes.themes[0].slides.length > 0)
                applyTheme([kept.other.id], Themes.themes[0].place, Themes.themes[0].slides[0].id)
                return 800
            },
            () => {
                const box = boxOn(kept.target.entry, kept.other.index, kept.other.box.id)
                check("a slide dressed in a theme keeps the chords over its words", box !== undefined && chordsOf(box) === "0:G", box ? chordsOf(box) : "none")
                // ---- Pasting into a text box
                startEditing(kept.target.entry, kept.other.id)
                return 800
            },
            () => {
                const box = editScreen.canvas.elements.find(e => e.id === kept.other.box.id)
                kept.look = looks(box)[0]
                kept.alignment = testInput.alignmentsOf(box.text)[0]
                editScreen.canvas.editText(kept.other.box.id)
                testInput.setClipboard("Pasted One\nPasted Two", "<p style=\"color:#00ff00;font-family:Courier;font-size:40px\"><b>Pasted One</b></p><p>Pasted Two</p>")
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 400
            },
            () => {
                editScreen.canvas.finishText()
                return 500
            },
            () => {
                const box = editScreen.canvas.elements.find(e => e.id === kept.other.box.id)
                check("pasted words come in as words, line by line, in the box's own format and alignment, whatever the clipboard had",
                      box !== undefined && testInput.plain(box.text) === "Pasted One\nPasted Two" && looks(box).every(l => l === kept.look)
                      && testInput.alignmentsOf(box.text).every(a => a === kept.alignment),
                      box ? JSON.stringify([testInput.plain(box.text), looks(box), kept.look, testInput.alignmentsOf(box.text), kept.alignment]) : "none")
                editScreen.canvas.editText(kept.other.box.id)
                testInput.setClipboard("Again", "<span style=\"color:#00ff00;font-family:Courier\">Again</span>")
                testInput.key(Qt.Key_V, Qt.ControlModifier | Qt.ShiftModifier)
                return 400
            },
            () => {
                editScreen.canvas.finishText()
                return 500
            },
            () => {
                const box = editScreen.canvas.elements.find(e => e.id === kept.other.box.id)
                check("Ctrl+Shift+V pastes the same way", box !== undefined && testInput.plain(box.text) === "Again" && looks(box).every(l => l === kept.look),
                      box ? JSON.stringify([testInput.plain(box.text), looks(box)]) : "none")
                editScreen.canvas.editText(kept.other.box.id)
                testInput.key(Qt.Key_Backspace)
                testInput.setClipboard("Into Nothing", "<span style=\"color:#00ff00;font-family:Courier\">Into Nothing</span>")
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 400
            },
            () => {
                editScreen.canvas.finishText()
                return 500
            },
            () => {
                const box = editScreen.canvas.elements.find(e => e.id === kept.other.box.id)
                check("and into a box emptied first, still in the box's format", box !== undefined && testInput.plain(box.text) === "Into Nothing"
                      && looks(box).every(l => l === kept.look), box ? JSON.stringify([testInput.plain(box.text), looks(box), kept.look]) : "none")
                stopEditing()
                return 500
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
