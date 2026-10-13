import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Masks: made from the Looks window, where a screen is given one; drawn in the editor,
// which lists them at its left as it lists slides; named by a click on the name under
// one, and from its menu; shown over everything on the screen a look gives it to, and
// in the operator's preview; and removed, which takes it from the looks. And the same
// renaming for a slide of a presentation, where the name is the slide's label.
//
// The workspace has no set-up file to begin with, and so no masks. The presentations
// are the test's own (prepare_masks in run.py): "Plain", whose slides are a block of
// colour at the bottom left, and "Notices", whose blocks are at the top right.
QtObject {
    id: t

//COMMON
    readonly property point atNotice: Qt.point(0.78, 0.25)
    readonly property point atPlain: Qt.point(0.22, 0.75)

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

    function both(p, wanted) {
        return is(out(p), wanted) && is(seen(p), wanted)
    }

    function drawn() {
        return "output " + out(atNotice) + " " + out(atPlain) + ", preview " + seen(atNotice) + " " + seen(atPlain)
    }

    function open(name) {
        openEntry(entry(name))
    }

    // A row of the list at the left of the editor, and the point of the name under its picture
    function row(index) {
        const list = Lib.find(editScreen, i => i.model === editScreen.editor && i.positionViewAtIndex !== undefined)
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    function nameOf(index) {
        const item = row(index)
        return item.mapToItem(null, item.width / 2, item.height - 16)
    }

    function renameBox(index) {
        return Lib.find(row(index), i => i.selectAll !== undefined && i.visible === true)
    }

    function maskNames() {
        return Masks.masks.map(m => m.name).join(",")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                open("Plain")
                return 500
            },
            () => {
                kept.screen = Screens.audience[0].id
                check("the workspace has no masks, and the screen is given none", Masks.masks.length === 0 && !Looks.liveOf(kept.screen).mask
                      && previewMask === null && output.mask === null && !testInput.exists(catalog.workspacePath + "/Configuration/Workspace"))
                click(centre(slideCell(0)))
                click(centre(named("clearAnnouncements")))
                return 700
            },
            () => {
                check("a slide is on the output", both(atPlain, "#ffff00") && both(atNotice, "#000000"), drawn())
                openLooks()
                return 500
            },
            () => {
                const button = named("lookMask:" + kept.screen)
                check("the Looks window has a row for the masks, with the screen's mask to be chosen: none, which is drawn as a circle with a line through it",
                      looksOpen && button !== null && button.visible && button.name === "")
                click(centre(button))
                return 400
            },
            () => {
                check("its menu has None, ticked, and the way to the masks' editor", labels() === "[Mask], *None, --, Add New Mask, Edit Masks…(off)", labels())
                click(menuRow("Add New Mask"))
                return 800
            },
            () => {
                const editor = editScreen.editor
                check("Add New Mask makes a mask and brings up the editor on it, the Looks window put away", !looksOpen && editing && editor.kind === "masks"
                      && editor.count === 1 && editScreen.subject === "Masks" && maskNames() === "Mask" && editScreen.canvas.slide.label === "Mask",
                      editor.kind + " " + editor.count + " " + maskNames())
                check("it has nothing on it, and is drawn on grey so that what is black on it is seen", editScreen.canvas.elements.length === 0
                      && String(editScreen.canvas.pageColor) === "#6b6e75")
                check("the list at the left is of masks", Lib.find(editScreen, i => i.text === "Masks" && i.font !== undefined && i.visible) !== null
                      && row(0) !== null && row(0).slide.label === "Mask")
                editScreen.canvas.addShape("rectangle")
                return 500
            },
            () => {
                const shape = editScreen.canvas.elements[0]
                check("a shape drawn on a mask starts black", editScreen.canvas.elements.length === 1 && is(String(shape.fillColor), "#000000") && shape.fillEnabled,
                      String(shape.fillColor))
                // Over the bottom left quarter, and the top left corner
                check("it is put where it is to cover", editScreen.editor.setProperties(0, shape.id, { x: 0, y: 540, width: 960, height: 540 }) === "")
                testInput.grab("1-mask-editor")
                // Its name, by a click on the name under it
                click(nameOf(0))
                return 500
            },
            () => {
                const box = renameBox(0)
                check("a click on the name under a mask is to rename it: the name is there to type over", editScreen.renamingRow === row(0).slide.id && box !== null
                      && box.activeFocus && box.text === "Mask" && box.selectedText === "Mask", box ? box.text : "no box")
                testInput.type("Left Edge")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                check("typed, it is the mask's name, in the list and in the workspace", editScreen.renamingRow === "" && row(0).slide.label === "Left Edge"
                      && maskNames() === "Left Edge" && editScreen.canvas.elements.length === 1, maskNames())
                const at = row(0).mapToItem(null, row(0).width / 2, 40)
                click(at, Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on a mask has Rename and Remove…", labels() === "[Left Edge], Rename, Remove…", labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                const box = renameBox(0)
                check("Rename does the same", box !== null && box.activeFocus && box.text === "Left Edge")
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("and Esc leaves the name as it was", editScreen.renamingRow === "" && maskNames() === "Left Edge")
                editScreen.addRow()
                return 700
            },
            () => {
                check("the + makes another, which is the one being worked on", editScreen.editor.count === 2 && maskNames() === "Left Edge,Mask"
                      && editScreen.canvas.row === 1 && editScreen.canvas.elements.length === 0, maskNames())
                stopEditing()
                return 500
            },
            () => {
                check("the editor put away, the masks are the workspace's, each with its slide", !editing && Masks.masks.length === 2
                      && Masks.masks[0].slide.elements.length === 1 && Masks.masks[1].slide.elements.length === 0)
                check("nothing is masked yet: a mask is on a screen only when a look gives it one", output.mask === null && both(atPlain, "#ffff00"), drawn())
                openLooks()
                return 500
            },
            () => {
                click(centre(named("lookMask:" + kept.screen)))
                return 400
            },
            () => {
                check("the menu has the masks by name now", labels() === "[Mask], *None, --, Left Edge, Mask, --, Add New Mask, Edit Masks…", labels())
                click(menuRow("Left Edge"))
                return 800
            },
            () => {
                kept.mask = Masks.masks[0].id
                check("chosen, the live look gives the screen that mask, and the row says its name", Looks.liveOf(kept.screen).mask === kept.mask
                      && named("lookMask:" + kept.screen).name === "Left Edge" && output.mask !== null && output.mask.id === kept.mask)
                testInput.snapshot(catalog.workspacePath + "/Configuration/Workspace", "masks-given")
                testInput.grab("2-looks-with-mask")
                closeLooks()
                return 500
            },
            () => {
                check("the mask covers what is under it, on the output and in the operator's preview: the slide's block is black", both(atPlain, "#000000")
                      && !cleared && Show.liveIndex === 0, drawn())
                testInput.grabOutput("3-output-masked")
                testInput.grab("3-operator-masked")
                // It is over everything: a notice on the announcements layer, where the mask has nothing, shows; a slide under it does not
                check("the notices are sent to the announcements", catalog.setDestination(entry("Notices").file, "announcements") === "")
                return 400
            },
            () => {
                open("Notices")
                return 400
            },
            () => {
                click(centre(slideCell(0)))
                return 700
            },
            () => {
                check("where the mask has nothing the output shows through: a notice at the top right is seen", both(atNotice, "#ff0000") && both(atPlain, "#000000"),
                      drawn())
                // No clear takes a mask off
                testInput.key(Qt.Key_F1)
                return 700
            },
            () => {
                check("Clear All clears the show and leaves the mask: it is the screen's, not the show's", cleared && output.mask !== null
                      && Looks.liveOf(kept.screen).mask === kept.mask && named("previewMask").active, drawn())
                open("Plain")
                return 400
            },
            () => {
                click(centre(slideCell(1)))
                return 600
            },
            () => {
                check("the next slide is under the mask too", both(atPlain, "#000000") && Show.liveIndex === 1, drawn())
                // Removed in the editor, it goes from the look
                openLooks()
                return 400
            },
            () => {
                click(centre(named("lookMask:" + kept.screen)))
                return 400
            },
            () => {
                check("the screen's mask is ticked in the menu", labels() === "[Mask], None, --, *Left Edge, Mask, --, Add New Mask, Edit Masks…", labels())
                click(menuRow("Edit Masks…"))
                return 800
            },
            () => {
                check("Edit Masks… brings up the editor at the screen's mask", editing && editScreen.editor.kind === "masks" && editScreen.canvas.row === 0
                      && editScreen.canvas.slide.id === kept.mask && editScreen.canvas.elements.length === 1)
                editScreen.showRowMenu(row(0).slide, 40, 40)
                return 400
            },
            () => {
                click(menuRow("Remove…"))
                return 400
            },
            () => {
                check("removing asks first, and says the looks lose it", labels().indexOf("(note), Remove") === 0, labels())
                click(menuRow("Remove"))
                return 800
            },
            () => {
                check("removed, the other mask is left and no look gives the screen one", maskNames() === "Mask" && editScreen.editor.count === 1
                      && !Looks.liveOf(kept.screen).mask && output.mask === null, maskNames() + " / " + Looks.liveOf(kept.screen).mask)
                stopEditing()
                return 600
            },
            () => {
                check("and the slide is seen again", both(atPlain, "#00ffff") && previewMask === null, drawn())
                // ---- a slide of a presentation is renamed in the editor's list too: its label
                startEditing(entry("Plain"))
                return 800
            },
            () => {
                check("the editor is on the presentation", editing && editScreen.editor.kind === "presentation" && editScreen.editor.count === 3
                      && row(1).slide.label === "" && String(editScreen.canvas.pageColor) === "#000000")
                click(nameOf(1))
                return 500
            },
            () => {
                const box = renameBox(1)
                check("a click on the name under a slide is to rename it: its label, which it has none of yet", editScreen.renamingRow === row(1).slide.id
                      && box !== null && box.activeFocus && box.text === "" && editScreen.canvas.row === 1, box ? box.text : "no box")
                testInput.type("Welcome")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                check("typed, it is the slide's label", editScreen.renamingRow === "" && row(1).slide.label === "Welcome" && editScreen.editor.slideAt(1).label === "Welcome"
                      && editScreen.editor.canUndo && editScreen.editor.count === 3)
                const words = Lib.find(row(1), i => i.text !== undefined && i.elide !== undefined && i.visible === true && String(i.text).indexOf("Welcome") >= 0)
                check("and stands under the slide in the list", words !== null && words.text === "2  Welcome", words ? words.text : "none")
                editScreen.editor.undo()
                return 400
            },
            () => {
                check("it is a change like any other in the editor: undone", editScreen.editor.slideAt(1).label === "" && row(1).slide.label === "")
                editScreen.editor.redo()
                return 400
            },
            () => {
                check("and done again", editScreen.editor.slideAt(1).label === "Welcome")
                const at = row(1).mapToItem(null, row(1).width / 2, 40)
                click(at, Qt.RightButton)
                return 400
            },
            () => {
                check("a slide's menu in the list has Rename", labels().indexOf("[Slide 2], Rename, New Slide") === 0, labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                const box = renameBox(1)
                check("which has the label to type over", box !== null && box.activeFocus && box.text === "Welcome")
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc leaves it", editScreen.editor.slideAt(1).label === "Welcome" && editScreen.renamingRow === "")
                stopEditing()
                return 700
            },
            () => {
                check("out of the editor, the slide has its label among the slides", !editing && document.slides[1].label === "Welcome" && document.slides[0].label === "")
                testInput.key(Qt.Key_F1)
                check("nothing was reported", notice === "", notice)
            },
        ]
        next()
    }
}
