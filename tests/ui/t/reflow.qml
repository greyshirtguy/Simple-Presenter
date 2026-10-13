import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// The reflow editor: the button and the key that lead to it, the slides' words as a table, a slide split in two at
// the caret and two joined, words typed, what the new slide is made of, undo, and (after, with protoc) what the
// presentation's file holds. On a presentation the test makes, with words of its own.
QtObject {
    id: t

//COMMON
    readonly property var editor: editScreen.editor
    readonly property var canvas: editScreen.canvas
    readonly property var view: named("reflowEditor")
    readonly property var table: named("reflowTable")

    // The rows of the table, from the top, and the box of words in one
    function rows() {
        return Lib.findAll(table, i => i.objectName === "reflowRow").sort((a, b) => a.index - b.index)
    }

    function box(index) {
        return Lib.find(rows()[index], i => i.objectName === "reflowWords")
    }

    // The words of every slide, as the editor has them, with a bar between slides
    function words() {
        return editor.reflow().map(r => r.text.replace(/\n/g, "/")).join(" | ")
    }

    // And as the table shows them
    function shown() {
        return rows().map(r => r.editable ? box(r.index).text.replace(/\n/g, "/") : "(none)").join(" | ")
    }

    // The text box of a row's slide that has its words, as the editor describes the slide
    function wordsBox(row) {
        const element = editor.reflow()[row].element
        return editor.slideAt(row).elements.find(e => e.id === element)
    }

    function sizes(row) {
        return testInput.runsOf(wordsBox(row).text).map(run => run.size).filter((size, i, all) => all.indexOf(size) === i).join()
    }

    // Puts the caret in a row's words at a place, by a click on the row and then the place
    function caretAt(index, place) {
        click(centre(rows()[index]))
        box(index).cursorPosition = place
    }

    function logged(what) {
        return testInput.readText(Log.path).split("\n").filter(line => line.includes("  edit  ") && line.includes(what)).length
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                arrangementsShown = false
                openLibrary(catalog.libraries[0].path)
                const made = catalog.createPresentation(libraryPath, "Reflowed")
                check("(a presentation is made for the test)", made.error === "", made.error)
                kept.path = made.path
                openLibrary(libraryPath)
                openEntry(entry("Reflowed"))
                return 600
            },
            () => {
                const button = named("reflowButton")
                const edit = named("editButton")
                check("the toolbar has a Reflow button straight after Edit, with a picture of its own",
                      button !== null && button.visible && button.label === "Reflow" && button.kind === "reflow" && !button.on
                      && button.mapToItem(null, 0, 0).x >= edit.mapToItem(null, edit.width, 0).x && button.mapToItem(null, 0, 0).x - edit.mapToItem(null, edit.width, 0).x < 8)
                // The words, in the editor proper: four lines in one text box, the second line bigger, and a shape
                startEditing(currentEntry())
                return 600
            },
            () => {
                canvas.addText()
                testInput.type("Bright morning")
                testInput.key(Qt.Key_Return)
                testInput.type("over the hill")
                testInput.key(Qt.Key_Return)
                testInput.type("Quiet evening")
                testInput.key(Qt.Key_Return)
                testInput.type("under the stars")
                testInput.key(Qt.Key_Escape)
                canvas.setProperties({ name: "Words", x: 200, y: 300, width: 1500, height: 500 }, false)
                return 400
            },
            () => {
                kept.words = canvas.selectedId
                editor.formatText(canvas.row, kept.words, 0, 1000, { size: 60, color: "#ffffff" })
                editor.formatText(canvas.row, kept.words, 15, 28, { size: 90, color: "#ffd000" })
                canvas.addShape("ellipse")
                canvas.setProperties({ name: "Mark", x: 60, y: 60, width: 100, height: 100 }, false)
                return 400
            },
            () => {
                check("(the slide has its words in two sizes, and a shape)", editor.count === 1 && editor.slideAt(0).elements.length === 2 && sizes(0) === "60,90"
                      && words() === "Bright morning/over the hill/Quiet evening/under the stars", words() + " " + sizes(0))
                stopEditing()
                return 500
            },
            () => {
                testInput.snapshot(kept.path, "reflow-before")
                check("(back at the show)", !editing && !reflowing && named("reflowButton").on === false)
                testInput.key(Qt.Key_R, Qt.ControlModifier)
                return 800
            },
            () => {
                check("Ctrl+R brings the reflow editor up on the presentation being viewed, its button lit and Edit not", editing && reflowing && editScreen.mode === "reflow"
                      && view !== null && view.visible && named("reflowButton").on && !named("editButton").on && logged("the reflow editor come to") === 1)
                check("it has the editor to itself: no list of slides at the left, no switch of the other ways of working, and one button of its own",
                      !Lib.find(editScreen, i => i.objectName === "editorModes").visible && named("reflowBreak").visible && named("reflowBreak").text === "Insert Slide Break"
                      && view.mapToItem(null, 0, 0).x < 2)
                check("the slides' words are a table at the left, a row to a slide, in a plain face", rows().length === 1 && shown() === words() && box(0).font.pixelSize === 15
                      && box(0).textFormat === TextEdit.PlainText && table.mapToItem(null, table.width, 0).x < win.width * 0.6, shown())
                check("with the slides themselves at the right, as they will be shown", Lib.findAll(named("reflowSlides"), i => i.objectName === "reflowSlide").length === 1
                      && named("reflowSlides").mapToItem(null, 0, 0).x > table.mapToItem(null, table.width, 0).x)
                check("the caret is in the row of the slide the editor was on, at the end of its words", box(0).activeFocus && box(0).cursorPosition === box(0).length)
                check("the line along the bottom says what the keys do", /Ctrl\+Enter splits the slide at the caret/.test(named("editorHintText").text), named("editorHintText").text)
                testInput.grab("1-reflow")
                // The caret after the second line, and Ctrl+Enter
                caretAt(0, 28)
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 800
            },
            () => {
                check("Ctrl+Enter splits the slide where the caret is: what is after it is on a new slide straight after, and the line break between goes to neither",
                      editor.count === 2 && words() === "Bright morning/over the hill | Quiet evening/under the stars" && shown() === words(), words())
                check("the caret is at the start of the new slide's words", box(1).activeFocus && box(1).cursorPosition === 0 && view.current === 1)
                const first = editor.slideAt(0)
                const second = editor.slideAt(1)
                const like = (a, b) => ["x", "y", "width", "height", "name", "shape"].every(key => a[key] === b[key])
                check("the new slide is the slide over again: the same elements in the same places, each with an id of its own",
                      second.elements.length === 2 && first.elements.every((e, i) => like(e, second.elements[i]) && e.id !== second.elements[i].id)
                      && first.id !== second.id && second.width === first.width && second.group === first.group, second.elements.map(e => e.name).join())
                check("the words keep how they were set on both: the bigger line is bigger where it now is, and the rest as it was", sizes(0) === "60,90" && sizes(1) === "60"
                      && testInput.runsOf(wordsBox(0).text).filter(run => run.size === 90).map(run => run.text).join("") === "over the hill",
                      sizes(0) + " | " + sizes(1))
                check("and the slides at the right are two", Lib.findAll(named("reflowSlides"), i => i.objectName === "reflowSlide").length === 2)
                check("the log says a slide was split", logged("reflow: slide 1 split in two") === 1)
                testInput.snapshot(kept.path, "reflow-split")
                testInput.grab("2-split")
                // Typing: a word corrected, and a line break put in
                caretAt(1, 5)
                testInput.type("er")
                return 1100
            },
            () => {
                check("words typed are in the row at once, and in the slide a moment after the typing stops", box(1).text === "Quieter evening\nunder the stars"
                      && words() === "Bright morning/over the hill | Quieter evening/under the stars" && box(1).cursorPosition === 7, words())
                check("set as the words around them are", sizes(1) === "60")
                // A line put in after the first, and then the caret to the very end of the words
                testInput.key(Qt.Key_End)
                testInput.key(Qt.Key_Return)
                testInput.type("and far")
                box(1).cursorPosition = box(1).length
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 900
            },
            () => {
                check("Enter is a line break within the slide; and a split with the caret at the end of the words makes a slide with none yet, like the others",
                      editor.count === 3 && words() === "Bright morning/over the hill | Quieter evening/and far/under the stars | " && box(2).activeFocus
                      && editor.slideAt(2).elements.length === 2, words())
                testInput.type("Home")
                return 1100
            },
            () => {
                check("which takes words typed into it, set as the words it was split from", words().endsWith(" | Home") && sizes(2) === "60", words() + " " + sizes(2))
                // Joining: Backspace at the very start of a row's words
                caretAt(2, 0)
                testInput.key(Qt.Key_Backspace)
                return 800
            },
            () => {
                check("Backspace at the very start of a slide's words joins the slide to the one before: its words go after that one's, on a new line",
                      editor.count === 2 && words() === "Bright morning/over the hill | Quieter evening/and far/under the stars/Home", words())
                check("the caret is where the joined words begin, in the row that has them now", box(1).activeFocus && box(1).cursorPosition === "Quieter evening\nand far\nunder the stars\n".length,
                      box(1).cursorPosition)
                check("the log says which slide was joined to which", logged("reflow: slide 3 joined to slide 2") === 1)
                // Backspace anywhere else is only Backspace
                testInput.key(Qt.Key_Backspace)
                return 1100
            },
            () => {
                check("Backspace anywhere else takes a character out, as anywhere", editor.count === 2 && words().endsWith("under the starsHome"), words())
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 700
            },
            () => {
                check("Ctrl+Z takes that back", words().endsWith("under the stars/Home") && editor.count === 2, words())
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 700
            },
            () => {
                check("and again takes the join back: the slide is there again with its words", editor.count === 3 && words().endsWith("under the stars | Home")
                      && shown() === words(), words())
                testInput.key(Qt.Key_Z, Qt.ControlModifier | Qt.ShiftModifier)
                return 700
            },
            () => {
                check("Redo joins them again", editor.count === 2 && words().endsWith("under the stars/Home"), words())
                // Delete at the very end of the first row's words joins the next slide to it
                caretAt(0, 28)
                testInput.key(Qt.Key_Delete)
                return 800
            },
            () => {
                check("Delete at the very end of a slide's words joins the next slide to it", editor.count === 1
                      && words() === "Bright morning/over the hill/Quieter evening/and far/under the stars/Home" && box(0).cursorPosition === 29, words())
                check("and the words keep how they were set through it all: the second line is still the bigger one", sizes(0) === "60,90"
                      && testInput.runsOf(wordsBox(0).text).filter(run => run.size === 90).map(run => run.text).join("") === "over the hill", sizes(0))
                // The button splits too, where the caret is: in the middle of a line, between two words
                caretAt(0, 6)
                click(centre(named("reflowBreak")))
                return 800
            },
            () => {
                check("Insert Slide Break splits where the caret is, as the key does; between two words, the space goes to neither slide", editor.count === 2
                      && words() === "Bright | morning/over the hill/Quieter evening/and far/under the stars/Home", words())
                // Up and Down go from row to row
                testInput.key(Qt.Key_Up)
                return 300
            },
            () => {
                check("Up from the first line of a row goes to the row before, and Down from its last line to the row after", box(0).activeFocus && view.current === 0)
                testInput.key(Qt.Key_Down)
                return 300
            },
            () => {
                check("(Down)", box(1).activeFocus && box(1).cursorPosition === 0)
                check("the slide whose row the caret is in is ringed among the slides at the right",
                      Lib.findAll(named("reflowSlides"), i => i.objectName === "reflowSlide").filter(cell => cell.current).map(cell => cell.index).join() === "1")
                testInput.snapshot(kept.path, "reflow-end")
                testInput.grab("3-end")
                // Edit leads to the editor proper on the same presentation, and Reflow back. (The toolbar's buttons
                // are asked and not clicked, here as in the other tests: the toolbar is also what the window is
                // moved by, and a press made up by a test does not get through to a button on it.)
                named("editButton").clicked()
                return 600
            },
            () => {
                check("Edit, from the reflow editor, is the editor proper on the same presentation, with what was typed saved", editing && !reflowing && editScreen.mode === "slides"
                      && named("editButton").on && !named("reflowButton").on && Lib.find(editScreen, i => i.objectName === "editorModes").visible && editor.count === 2,
                      [editing, reflowing, editScreen.mode, named("editButton").on, named("reflowButton").on, Lib.find(editScreen, i => i.objectName === "editorModes").visible, editor.count].join())
                named("reflowButton").clicked()
                return 600
            },
            () => {
                check("and Reflow, from the editor proper, is the reflow editor again", reflowing && shown() === words() && rows().length === 2)
                named("reflowButton").clicked()
                return 600
            },
            () => {
                check("Reflow again, while it is up, goes back to the show, where the presentation has its two slides",
                      !editing && !reflowing && document.slides.length === 2 && document.name === "Reflowed", [editing, reflowing, document.slides.length, document.name].join())
                // Not for the props: a prop is made, and the editor opened on it
                check("at the show, with a presentation viewed, the button is not dimmed", canReflow && named("reflowButton").opacity === 1)
                const prop = Props.add("")
                check("(a prop is made for the test)", prop.error === "", prop.error)
                startEditingProps(prop.id)
                return 700
            },
            () => {
                check("while the props are being edited the button is dimmed, and neither it nor the key does anything", editing && !canReflow && named("reflowButton").opacity < 0.5,
                      [editing, canReflow, named("reflowButton").opacity, editScreen.kind].join())
                testInput.key(Qt.Key_R, Qt.ControlModifier)
                return 300
            },
            () => {
                check("(nothing)", editing && !reflowing && editScreen.kind === "props")
                stopEditing()
                return 400
            },
            () => {
                const lines = testInput.readText(Log.path).split("\n")
                const wrong = lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("not an image"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.slice(0, 2).join(" | "))
            }
        ]
        next()
    }
}
