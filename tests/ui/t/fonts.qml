import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Fonts, on a family made up for the test (run.py's prepare_test_fonts): "Test Sans", its Regular from one foundry and
// its Light from another, as the maintainer's CMG Sans is, which Qt lists only as "Test Sans [TSTA]" and "Test Sans
// [TSTB]" (src/fontnames.h). A box's words given Light by its PostScript name, as ProPresenter names a face, are drawn
// lighter than Regular: in the editor, on the output and in the operator's preview (Qt asked for "Test Sans" in Light
// draws the other foundry's Regular). The editor's font box calls the family by its name and not missing, and lists it
// once; picking it, or a family in Qt's form as a file was once given one, writes the family's own name and the face's
// real PostScript name. And a face named by another maker's PostScript name is drawn in the family's face of its style.
QtObject {
    id: t

//COMMON
    function worded(slide) {
        return slide.elements.filter(e => testInput.plain(e.text).trim() !== "")
    }

    function runs(entry, index, id) {
        const box = catalog.open(entry.file).slides[index].elements.find(e => e.id === id)
        return box ? testInput.runsOf(box.text) : []
    }

    // How much of the slide in the editor is lit: its words, on a dark ground
    function canvasLit() {
        const c = editScreen.canvas
        const at = c.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, c.width, c.height, 100)
    }

    function previewLit() {
        const p = named("previewSlide")
        const at = p.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, p.width, p.height, 100)
    }

    // The editor opened on the slide, and its box given a format
    function format(change) {
        const editor = editScreen.editor
        const row = editor.rowOf(kept.id)
        return editor.formatText(row, kept.box.id, 0, testInput.plain(kept.box.text).length, change)
    }

    function run() {
        steps = [
            () => {
                const families = Qt.fontFamilies()
                check("(Test Sans is installed for the test, which Qt lists once for each of its two foundries)",
                      families.includes("Test Sans [TSTA]") && families.includes("Test Sans [TSTB]") && !families.includes("Test Sans"),
                      JSON.stringify(families.filter(f => f.startsWith("Test"))))
                // A slide of one box with words, nothing else to do, on a dark ground
                for (const d of documents.filter(openable)) {
                    const slides = catalog.open(d.file).slides
                    const index = slides.findIndex(s => worded(s).length === 1 && s.elements.length === 1 && s.media === undefined
                                                        && s.actions.length === 0 && (!s.drawsBackground || Qt.colorEqual(s.backgroundColor, "black")))
                    if (index >= 0) {
                        kept.entry = d
                        kept.index = index
                        kept.id = slides[index].id
                        kept.box = worded(slides[index])[0]
                        break
                    }
                }
                check("(a slide of one text box with words on a dark ground)", kept.entry !== undefined, kept.entry ? kept.entry.name : "none")
                const plain = Looks.looks.find(look => Looks.of(look.id, Screens.audience[0].id).theme === "")
                if (plain)
                    Show.lookId = plain.id
                openEntry(kept.entry)
                return 600
            },
            () => {
                startEditing(kept.entry, kept.id)
                return 900
            },
            () => {
                check("(its words given Test Sans Regular, by its PostScript name)", format({ fontName: "TestSans-Regular" }) === "")
                return 600
            },
            () => {
                kept.canvas = { regular: canvasLit() }
                const r = runs(kept.entry, kept.index, kept.box.id)
                check("the file names the face by its PostScript name, and the family by its own name", r.length > 0
                      && r.every(x => x.fontName === "TestSans-Regular" && x.family === "Test Sans"), JSON.stringify(r.map(x => [x.fontName, x.family])))
                stopEditing()
                return 800
            },
            () => {
                goLive(kept.index)
                return 1500
            },
            () => {
                kept.output = { regular: testInput.lit(false, 0, 0, 1, 1) }
                kept.preview = { regular: previewLit() }
                clearSlide()
                startEditing(kept.entry, kept.id)
                return 900
            },
            () => {
                check("(its words given Test Sans Light, by its PostScript name, as ProPresenter names a face)", format({ fontName: "TestSans-Light" }) === "")
                return 600
            },
            () => {
                kept.canvas.light = canvasLit()
                const r = runs(kept.entry, kept.index, kept.box.id)
                check("(the file has it so)", r.length > 0 && r.every(x => x.fontName === "TestSans-Light" && x.family === "Test Sans" && !x.bold),
                      JSON.stringify(r.map(x => [x.fontName, x.family, x.bold])))
                check("Light, the other foundry's, is drawn lighter than Regular in the editor", kept.canvas.light < kept.canvas.regular * 0.93,
                      JSON.stringify(kept.canvas))
                stopEditing()
                return 800
            },
            () => {
                goLive(kept.index)
                return 1500
            },
            () => {
                kept.output.light = testInput.lit(false, 0, 0, 1, 1)
                kept.preview.light = previewLit()
                testInput.grabOutput("1-light")
                check("and on the output, and in the operator's preview", kept.output.light < kept.output.regular * 0.95 && kept.preview.light < kept.preview.regular * 0.97,
                      JSON.stringify([kept.output, kept.preview]))
                clearSlide()
                startEditing(kept.entry, kept.id)
                return 900
            },
            () => {
                editScreen.canvas.pick(kept.box.id)
                editScreen.inspector.tab = "text"
                return 500
            },
            () => {
                const picker = named("fontPicker")
                const label = picker ? Lib.find(picker, item => item.text !== undefined && String(item.text).startsWith("Test Sans")) : null
                check("the editor's font box calls the family by its name, and not missing", picker !== null && picker.installed && label !== null
                      && label.text === "Test Sans" && String(label.color) !== "#ffb300", label ? label.text + " " + label.color : "none")
                check("and lists it once, by its own name", picker.families.filter(f => f === "Test Sans").length === 1
                      && !picker.families.some(f => f.startsWith("Test Sans [")), JSON.stringify(picker.families.filter(f => f.startsWith("Test"))))
                picker.picked("Test Sans")
                return 600
            },
            () => {
                const r = runs(kept.entry, kept.index, kept.box.id)
                check("picking it writes the family's own name and its Regular's real PostScript name", r.length > 0
                      && r.every(x => x.family === "Test Sans" && x.fontName === "TestSans-Regular"), JSON.stringify(r.map(x => [x.fontName, x.family])))
                check("(a family in Qt's form, as a file was once given one)", format({ family: "Test Sans [TSTB]" }) === "")
                return 600
            },
            () => {
                const r = runs(kept.entry, kept.index, kept.box.id)
                check("is written by its own name too, and the face by its real name", r.length > 0
                      && r.every(x => x.family === "Test Sans" && x.fontName === "TestSans-Regular"), JSON.stringify(r.map(x => [x.fontName, x.family])))
                check("(a face named by another maker's PostScript name)", format({ fontName: "TestSansPro-Light" }) === "")
                editScreen.canvas.pick("")
                return 600
            },
            () => {
                const r = runs(kept.entry, kept.index, kept.box.id)
                const lit = canvasLit()
                check("keeps its name in the file, and is drawn in the family's face of its style, Light", r.length > 0
                      && r.every(x => x.fontName === "TestSansPro-Light" && x.family === "Test Sans")
                      && Math.abs(lit - kept.canvas.light) < kept.canvas.light * 0.03, JSON.stringify([r.map(x => [x.fontName, x.family]), lit, kept.canvas]))
                stopEditing()
                return 800
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
