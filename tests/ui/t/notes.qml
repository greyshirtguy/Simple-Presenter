import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A slide's notes: the button for them at the bottom right of the editor and the pane it puts up under the slide;
// notes ProPresenter wrote, read and drawn; typing, which is saved a moment later and, taken out again, leaves the
// file as it was; Ctrl+B, I and U; the panel a right click puts up, with a font, a size and a colour; the pane's top
// edge dragged; undo and redo; and that the editors of props, stage layouts and themes have neither button nor pane.
// What the files then hold is looked at when the test is over, by after_notes in run.py.
QtObject {
    id: t

//COMMON
    readonly property string words: "Watch the band for the ending\nHold the last chord"

    function layout(name) {
        return StageLayouts.layouts.find(l => l.name === name)
    }

    function pane() {
        return named("slideNotes")
    }

    function box() {
        return named("notesEdit")
    }

    // A slide of the song as the file has it now
    function onDisk(row) {
        return catalog.open(kept.path).slides[row]
    }

    function runs(row) {
        return testInput.runsOf(onDisk(row).notes)
    }

    function told(found) {
        return JSON.stringify(found.map(r => [r.text, r.bold, r.italic, r.underline, r.size, String(r.color)]))
    }

    // How much of an item is red, as the operator window draws it
    function red(item) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowRed(at.x, at.y, item.width, item.height)
    }

    function lighter(item, grey) {
        const at = item.mapToItem(null, 0, 0)
        return testInput.windowLighter(at.x, at.y, item.width, item.height, grey)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "MainHall").path)
                kept.path = entry("Notes Song").path
                kept.hash = testInput.fileHash(kept.path)
                testInput.snapshot(kept.path, "song-before")
                const read = catalog.open(kept.path)
                // The slide that has notes, and one that has none: by their places among the song's slides as they are shown
                kept.row = read.slides.findIndex(s => s.notesText !== undefined)
                kept.bare = kept.row === 0 ? 1 : 0
                check("a slide's notes are read from the file, and a slide without any has none", kept.row >= 0 && read.slides[kept.row].notesText === words
                      && read.slides.filter(s => s.notes !== undefined).length === 1 && read.slides[kept.bare].notes === undefined,
                      "slide " + (kept.row + 1) + " of " + read.slides.length + ": " + JSON.stringify(kept.row >= 0 ? read.slides[kept.row].notesText : ""))
                const found = testInput.runsOf(read.slides[kept.row].notes)
                check("as ProPresenter wrote them: at 24 points, in white", found.length === 2 && found.every(r => r.size === 24 && String(r.color) === "#ffffff" && !r.bold), told(found))
                startEditing(entry("Notes Song"))
                return 800
            },
            () => {
                // ---- The button and the pane
                const toggle = named("notesToggle")
                const bar = toggle.parent
                const at = toggle.mapToItem(editScreen, 0, 0)
                check("the editor has a button for the notes at the bottom right, under the slide", editing && toggle.visible
                      && at.y + toggle.height <= editScreen.height && at.y >= editScreen.height - bar.height
                      && Math.abs(at.x + toggle.width + 8 - editScreen.inspector.x) <= 1, at.x + "," + at.y + " of " + editScreen.width + "x" + editScreen.height)
                check("the pane is not up until it is asked for", pane() === null && !editorNotes)
                kept.canvasHeight = editScreen.canvas.height
                kept.offColour = String(testInput.windowPixel(at.x + toggle.width - 4, at.y + 4))
                click(centre(toggle))
                return 600
            },
            () => {
                const toggle = named("notesToggle")
                const at = toggle.mapToItem(null, 0, 0)
                const canvas = editScreen.canvas
                check("a click puts the pane up, under the slide and over the line at the bottom", pane() !== null && pane().visible && editorNotes
                      && Math.abs(pane().height - 150) < 1 && Math.abs(canvas.height - (kept.canvasHeight - pane().height)) < 1
                      && Math.abs(pane().mapToItem(editScreen, 0, 0).y - (canvas.y + canvas.height)) < 1
                      && Math.abs(pane().mapToItem(editScreen, 0, pane().height).y - toggle.parent.y) < 1 && Math.abs(pane().width - canvas.width) < 1,
                      pane() ? pane().height + " of " + kept.canvasHeight : "no pane")
                const onColour = String(testInput.windowPixel(at.x + toggle.width - 4, at.y + 4))
                check("and the button is filled in while it is up", onColour === "#1e88e5" && kept.offColour !== onColour, kept.offColour + " then " + onColour)
                const empty = named("notesPlaceholder")
                check("a slide with no notes has an empty box, which says what it is for", box().text === "" && empty.visible && lighter(empty, 70) > 0.02, lighter(empty, 70))
                testInput.grab("1-pane-empty")
                editScreen.showRow(kept.row)
                return 600
            },
            () => {
                check("turning to a slide that has notes shows them", editScreen.canvas.row === kept.row && box().text === words && !named("notesPlaceholder").visible, JSON.stringify(box().text))
                kept.white = lighter(box(), 200)
                // (White letters have a little colour at their edges, as the screen's own points are lit: hence not none at all.)
                check("drawn as they were written: white, on the dark of the pane", kept.white > 0.01 && red(box()) < 0.001, kept.white + " white, " + red(box()) + " red")
                testInput.grab("2-pane-notes")
                // ---- Typing
                editScreen.showRow(kept.bare)
                return 500
            },
            () => {
                click(box().mapToItem(null, 20, 12))
                return 300
            },
            () => {
                check("a click in the pane gives it the keys", pane().typing && box().activeFocus)
                testInput.type("Hi")
                return 300
            },
            () => {
                check("what is typed is in the box at once, and the file is not written at every key", box().text === "Hi" && testInput.fileHash(kept.path) === kept.hash, box().text)
                return 1600
            },
            () => {
                const found = runs(kept.bare)
                check("a moment after the typing stops it is in the file", onDisk(kept.bare).notesText === "Hi" && testInput.fileHash(kept.path) !== kept.hash, JSON.stringify(onDisk(kept.bare).notesText))
                check("as ProPresenter's own notes start out: Helvetica Neue at 24 points, in white", found.length === 1 && found[0].fontName === "HelveticaNeue" && found[0].size === 24
                      && String(found[0].color) === "#ffffff", JSON.stringify(found.map(r => [r.fontName, r.size, String(r.color)])))
                check("which is a step that Undo takes back", editScreen.editor.canUndo)
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.key(Qt.Key_Backspace)
                return 200
            },
            () => {
                testInput.key(Qt.Key_Escape)
                return 500
            },
            () => {
                check("Esc gives the keys back to the slide", !pane().typing && box().text === "")
                check("notes typed onto a slide that had none, and taken out again, leave the file as it was, byte for byte", testInput.fileHash(kept.path) === kept.hash
                      && onDisk(kept.bare).notes === undefined)
                // ---- Formatting, by the keys
                editScreen.showRow(kept.row)
                return 500
            },
            () => {
                click(box().mapToItem(null, 20, 12))
                return 300
            },
            () => {
                box().select(0, 5)
                testInput.key(Qt.Key_B, Qt.ControlModifier)
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("Ctrl+B makes what is selected bold, and nothing else, in the file at once", found.length === 3 && found[0].text === "Watch" && found[0].bold === true
                      && !found[1].bold && !found[2].bold && onDisk(kept.row).notesText === words, told(found))
                check("and leaves it selected", box().selectionStart === 0 && box().selectionEnd === 5 && box().text === words, box().selectionStart + " to " + box().selectionEnd)
                testInput.snapshot(kept.path, "song-bold")
                testInput.key(Qt.Key_I, Qt.ControlModifier)
                testInput.key(Qt.Key_U, Qt.ControlModifier)
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("Ctrl+I and Ctrl+U make it italic and underlined too", found[0].text === "Watch" && found[0].bold && found[0].italic && found[0].underline
                      && !found[1].italic && !found[1].underline, told(found))
                testInput.key(Qt.Key_I, Qt.ControlModifier)
                return 400
            },
            () => {
                const found = runs(kept.row)
                check("and the same key again takes it off", found[0].bold && !found[0].italic && found[0].underline, told(found))
                // ---- The panel
                click(box().mapToItem(null, 30, 12), Qt.RightButton)
                return 500
            },
            () => {
                const panel = named("notesFontPanel")
                const at = panel.mapToItem(null, 0, 0)
                check("a right click puts up a panel with a font, a size and a colour, all of it in the window", panel.visible && named("notesFont").visible && named("notesSize").visible
                      && named("notesColor").visible && at.y >= 0 && at.y + panel.height <= win.height && at.x >= 0, at.x + "," + at.y)
                check("which have what is selected: its size", named("notesSize").value === 24 && box().selectionStart === 0 && box().selectionEnd === 5, named("notesSize").value)
                testInput.grab("3-font-panel")
                named("notesSize").edited(40)
                named("notesColor").picked("#ff2a1a")
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("a size and a colour picked there are what is selected's, in the file", found[0].text === "Watch" && found[0].size === 40 && String(found[0].color) === "#ff2a1a"
                      && found[1].size === 24 && String(found[1].color) === "#ffffff", told(found))
                // (A family whose name is plain: some are listed with their maker after them.)
                const family = editScreen.families.find(f => /^[A-Za-z ]+$/.test(f) && f !== named("notesFont").family && f !== found[0].family)
                kept.family = family
                named("notesFont").picked(family)
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("and so is a font", found[0].family === kept.family && found[1].family !== kept.family, kept.family + ": " + found.map(r => r.family).join("|"))
                testInput.grab("4-formatted")
                // a press on the slide
                click(editScreen.canvas.mapToItem(null, 30, 30))
                return 400
            },
            () => {
                check("a press anywhere else puts the panel away", !named("notesFontPanel").visible)
                // (What is selected is drawn as selected text is, so the colour is looked for with nothing selected.)
                box().deselect()
                return 300
            },
            () => {
                kept.red = red(box())
                check("the colour is drawn in the pane", kept.red > 0.002, kept.red)
                // ---- The top edge
                kept.height = pane().height
                const edge = centre(named("notesEdge"))
                drag(edge, Qt.point(edge.x, edge.y - 70))
                return 500
            },
            () => {
                check("dragging the pane's top edge up makes it taller, and the slide's part shorter by as much", Math.abs(pane().height - (kept.height + 70)) < 2
                      && Math.abs(editorNotesHeight - (kept.height + 70)) < 2 && Math.abs(editScreen.canvas.height - (kept.canvasHeight - pane().height)) < 1,
                      pane().height + " from " + kept.height)
                testInput.grab("5-taller")
                // ---- Undo and redo
                editScreen.editor.undo()
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("Undo takes back the last of it: the font", found[0].family !== kept.family && String(found[0].color) === "#ff2a1a", told(found))
                editScreen.editor.undo()
                return 500
            },
            () => {
                const found = runs(kept.row)
                check("and then the colour, in the file and in the pane", String(found[0].color) === "#ffffff" && found[0].size === 40 && red(box()) < 0.001, told(found) + " " + red(box()))
                editScreen.editor.redo()
                return 500
            },
            () => {
                check("Redo puts it back, in both", String(runs(kept.row)[0].color) === "#ff2a1a" && red(box()) > 0.002, told(runs(kept.row)) + " " + red(box()))
                stopEditing()
                return 700
            },
            () => {
                // ---- The other editors have no notes
                check("the pane is still wanted, as it was left", !editing && editorNotes)
                startEditingProps("")
                return 700
            },
            () => {
                check("the editor of the props has neither the button nor the pane", editing && editScreen.editor.kind === "props" && !named("notesToggle").visible && pane() === null,
                      editScreen.editor.kind)
                testInput.grab("9-props-no-notes")
                stopEditing()
                return 600
            },
            () => {
                startEditingTheme("New Life Chapel")
                return 700
            },
            () => {
                check("nor has the editor of a theme", editing && editScreen.editor.kind === "theme" && !named("notesToggle").visible && pane() === null, editScreen.editor.kind)
                stopEditing()
                return 600
            },
            () => {
                startEditingStage(layout("Slide Notes").id)
                return 700
            },
            () => {
                check("nor has the editor of the stage layouts", editing && editScreen.editor.kind === "stage" && !named("notesToggle").visible && pane() === null, editScreen.editor.kind)
                stopEditing()
            }
        ]
        next()
    }
}
