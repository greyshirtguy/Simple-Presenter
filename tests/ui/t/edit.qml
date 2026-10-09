import QtQuick
import SimplePresenterApp

Item {
    id: holder

    function run() {
        editTest()
    }

    // TEMPORARY: a scripted run of the editor with the mouse and the keyboard.
    property var editTestSteps: []
    property int editTestStep: 0
    property int editTestFailures: 0

    function check(what, ok, detail) {
        if (!ok)
            ++editTestFailures
        testInput.say((ok ? "  ok      " : "  FAILED  ") + what + (detail !== undefined ? "   [" + detail + "]" : ""))
    }

    function editTest() {
        const canvas = editScreen.canvas
        const editor = editScreen.editor
        const sel = () => canvas.selected
        // A point of the picked element, as fractions of its box, in window coordinates
        const at = (fx, fy) => canvas.mapToItem(null, canvas.originX + (sel().x + fx * sel().width) * canvas.u,
                                                canvas.originY + (sel().y + fy * sel().height) * canvas.u)
        // `pressed` is what is held as the button goes down, where that differs: Ctrl held from then on a corner handle turns the element.
        const drag = (from, dx, dy, modifiers, grabName, pressed) => {
            testInput.mouse(0, from.x, from.y, pressed ?? modifiers ?? 0)
            for (let i = 1; i <= 8; ++i)
                testInput.mouse(1, from.x + dx * i / 8, from.y + dy * i / 8, modifiers ?? 0)
            if (grabName)
                testInput.grab(grabName)
            testInput.mouse(2, from.x + dx, from.y + dy, modifiers ?? 0)
        }
        const onDisk = () => catalog.open(editor.path).slides.find(s => s.id === canvas.slide.id)
        let start = null
        let file = ""
        let originalText = ""
        let firstCount = 0
        let lyricsId = ""

        editTestSteps = [
            () => {
                const entry = documents.find(d => d.name === "Abandoned")
                check("found the presentation", entry !== undefined)
                openEntry(entry)
                startEditing(entry)
                check("editor is up", editing && editor.count > 3, editor.count + " slides")
                editScreen.showRow(1)
                file = editor.path
            },
            () => {
                testInput.grab("e01-slide")
                // Click in the middle of the slide: picks the element in front there.
                const middle = canvas.mapToItem(null, canvas.originX + canvas.slideWidth / 2 * canvas.u, canvas.originY + canvas.slideHeight / 2 * canvas.u)
                testInput.mouse(0, middle.x, middle.y)
                testInput.mouse(2, middle.x, middle.y)
                check("a click picks an element", sel() !== null, sel() ? sel().name : "")
                lyricsId = canvas.selectedId
                start = { x: sel().x, y: sel().y, width: sel().width, height: sel().height }
                originalText = bridgeText(sel().text)
                firstCount = canvas.elements.length
                check("nothing changed by picking", !editor.changed && !editor.canUndo)
            },
            () => {
                // Drag it 80 px right and 48 px down, with Ctrl so that nothing snaps.
                drag(at(0.5, 0.5), 80, 48, Qt.ControlModifier, "e02-dragging")
                const dx = Math.round(80 / canvas.u), dy = Math.round(48 / canvas.u)
                check("a drag moves it", Math.abs(sel().x - Math.round(start.x + 80 / canvas.u)) <= 1 && Math.abs(sel().y - Math.round(start.y + 48 / canvas.u)) <= 1,
                      start.x + "," + start.y + " -> " + sel().x + "," + sel().y)
                check("the size is untouched", sel().width === start.width && sel().height === start.height)
                check("the move is in the file", Math.abs(onDisk().elements.find(e => e.id === lyricsId).x - sel().x) < 0.001)
                check("one change to undo", editor.canUndo && editor.changed)
            },
            () => {
                testInput.grab("e03-moved")
                // Without Ctrl, a drag that ends near the middle of the slide snaps to it.
                const centre = sel().x + sel().width / 2
                const target = canvas.slideWidth / 2
                drag(at(0.5, 0.5), (target - centre) * canvas.u + 4, 0, 0, "e04-snapping")
                check("a drag snaps its middle to the slide's", Math.abs(sel().x + sel().width / 2 - target) < 0.01, (sel().x + sel().width / 2) + " vs " + target)
                check("guides are gone after the drop", canvas.guides.length === 0 && canvas.dragBox === null)
            },
            () => {
                // Resize by the bottom right handle.
                const before = { width: sel().width, height: sel().height, x: sel().x, y: sel().y }
                drag(at(1, 1), -60, -30, Qt.ControlModifier, "e05-resizing", 0)
                check("the corner handle resizes", Math.abs(sel().width - Math.round(before.width - 60 / canvas.u)) <= 1.5 && Math.abs(sel().height - Math.round(before.height - 30 / canvas.u)) <= 1.5,
                      before.width + "x" + before.height + " -> " + sel().width + "x" + sel().height)
                check("the opposite corner stays", sel().x === before.x && sel().y === before.y)
                // And by the left edge.
                const b2 = { width: sel().width, x: sel().x, right: sel().x + sel().width }
                drag(at(0, 0.5), 40, 0, Qt.ControlModifier)
                check("the left handle moves the left edge only", Math.abs(sel().x + sel().width - b2.right) < 0.01 && sel().x > b2.x, b2.x + " -> " + sel().x)
            },
            () => {
                // Arrow keys nudge, and are saved together a moment later.
                const x = sel().x
                testInput.key(Qt.Key_Right)
                testInput.key(Qt.Key_Right)
                testInput.key(Qt.Key_Down, Qt.ShiftModifier)
                check("arrows nudge", sel().x === x + 2, x + " -> " + sel().x)
            },
            () => { },
            () => {
                check("the nudges reach the file", Math.abs(onDisk().elements.find(e => e.id === lyricsId).x - sel().x) < 0.001)
                // Double click: edit the text in place.
                const p = at(0.5, 0.5)
                testInput.mouse(3, p.x, p.y)
                check("a double click edits the text", canvas.editingId === lyricsId)
            },
            () => {
                testInput.grab("e06-editing")
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Hello brave new world\nsecond line")
                check("typing replaces the selection", bridgeText(canvas.liveText) === "Hello brave new world\nsecond line", bridgeText(canvas.liveText))
                check("not saved while typing", bridgeText(onDisk().elements.find(e => e.id === lyricsId).text) === originalText)
            },
            () => {
                testInput.grab("e07-typed")
                // Select "brave" (characters 6 to 11) with Shift and the arrows, and format it.
                testInput.key(Qt.Key_Home, Qt.ControlModifier)
                for (let i = 0; i < 6; ++i)
                    testInput.key(Qt.Key_Right)
                for (let i = 0; i < 5; ++i)
                    testInput.key(Qt.Key_Right, Qt.ShiftModifier)
                check("selection is where it should be", canvas.selection && bridgeText(canvas.liveText).substring(6, 11) === "brave")
                testInput.key(Qt.Key_B, Qt.ControlModifier)
                canvas.setFormat({ color: "#ffd000" }, true)
                canvas.setFormat({ color: "#ff4000" }, true)
                canvas.setFormat({ color: "#ff8000", underline: true }, false)
            },
            () => {
                testInput.grab("e08-formatted")
                const runs = testInput.runsOf(canvas.selected.text)
                check("only the selected word is formatted", runs.length === 4 && runs[1].text === "brave" && runs[1].bold && runs[1].underline
                      && !runs[0].bold && !runs[2].bold && !runs[3].underline, JSON.stringify(runs.map(r => [r.text, r.bold, r.underline, r.color])))
                check("the typing and the format are in the file", bridgeText(onDisk().elements.find(e => e.id === lyricsId).text) === "Hello brave new world\nsecond line")
                check("the selection is kept", canvas.selection)
                // Esc ends the editing.
                testInput.key(Qt.Key_Escape)
                check("Esc ends text editing", canvas.editingId === "" && canvas.selectedId === lyricsId)
            },
            () => {
                // Whole-box formatting from the inspector's side, while not editing.
                canvas.setFormat({ size: 90, capitalization: 1, alignment: Qt.AlignLeft }, false)
                canvas.setProperties({ fillOn: true, fillColor: "#a0102040", fillLinesOnly: true, verticalAlignment: Qt.AlignTop }, false)
                const runs = testInput.runsOf(canvas.selected.text)
                check("box formatting reaches every run", runs.every(r => r.size === 90 && r.capitalization === 1), JSON.stringify(runs.map(r => r.size)))
            },
            () => {
                editScreen.inspector.tab = "text"
            },
            () => {
                testInput.grab("e09a-texttab")
                testInput.mouse(0, 1084, 130)
                testInput.mouse(2, 1084, 130)
            },
            () => {
                testInput.grab("e09b-fonts")
                testInput.type("dej")
            },
            () => {
                testInput.grab("e09c-fonts-filtered")
                testInput.key(Qt.Key_Return)
            },
            () => {
                check("a font picked from the list is applied", testInput.runsOf(canvas.selected.text).every(r => r.family.startsWith("DejaVu")),
                      testInput.runsOf(canvas.selected.text)[0].family + " / " + testInput.runsOf(canvas.selected.text)[0].fontName)
                testInput.mouse(0, 1126, 162)
                testInput.mouse(2, 1126, 162)
            },
            () => {
                testInput.grab("e09d-colours")
                testInput.mouse(0, 1000, 220)
                for (let i = 1; i <= 6; ++i)
                    testInput.mouse(1, 1000 + i * 12, 220 + i * 6)
            },
            () => {
                testInput.grab("e09e-colour-dragging")
                check("nothing is saved while a colour is dragged", !editor.canRedo && testInput.runsOf(catalog.open(editor.path).slides[1].elements[0].text)[0].color === "#ffffff",
                      testInput.runsOf(catalog.open(editor.path).slides[1].elements[0].text)[0].color)
                testInput.mouse(2, 1072, 256)
                testInput.key(Qt.Key_Escape)
            },
            () => {
                const color = testInput.runsOf(canvas.selected.text)[0].color
                check("the colour settled on is applied and saved", color !== "#ffffff" && testInput.runsOf(catalog.open(editor.path).slides[1].elements[0].text)[0].color === color, color)
                testInput.grab("e09f-coloured")
                editScreen.inspector.tab = "shape"
            },
            () => {
                testInput.grab("e09-boxformat")
                // Add a text box: it comes up being edited with its text selected.
                canvas.addText()
                check("a text box is added and being edited", canvas.elements.length === firstCount + 1 && canvas.editingId !== "" && canvas.editingId === canvas.selectedId)
                testInput.type("Second box")
                testInput.key(Qt.Key_Escape)
                check("typing replaced its placeholder", bridgeText(canvas.selected.text) === "Second box", bridgeText(canvas.selected.text))
            },
            () => {
                // Link it to the lyrics, and give the lyrics a rule about it.
                const added = canvas.selectedId
                canvas.setProperties({ linkKind: "element", linkElementId: lyricsId, linkTransform: 1 }, false)
                check("linked text shows the other element's text", bridgeText(canvas.selected.displayText) === "Hello brave new world second line", bridgeText(canvas.selected.displayText))
                const p = at(0.5, 0.5)
                testInput.mouse(3, p.x, p.y)
                check("a linked box's text is not edited", canvas.editingId === "" && editScreen.notice !== "", editScreen.notice)
                canvas.setProperties({ visibilityRules: true, visibilityCriterion: 0,
                                       visibilityConditions: [{ kind: "element", elementId: lyricsId, hasText: false }] }, false)
                check("ruled out when shown", onDisk().elements.find(e => e.id === added).visible === false)
                check("but still drawn in the editor", canvas.selected.hidden === false)
            },
            () => {
                testInput.grab("e10-linked")
                // Delete it with the key, then undo everything.
                testInput.key(Qt.Key_Delete)
                check("Delete removes the picked element", canvas.elements.length === firstCount && canvas.selectedId === "")
                let undone = 0
                while (editor.canUndo && undone < 100) {
                    testInput.key(Qt.Key_Z, Qt.ControlModifier)
                    ++undone
                }
                const back = canvas.elements.find(e => e.id === lyricsId)
                check("undo takes everything back", back.x === start.x && back.y === start.y && back.width === start.width && back.height === start.height
                      && bridgeText(back.text) === originalText && canvas.elements.length === firstCount, undone + " steps")
            },
            () => {
                testInput.grab("e11-undone")
                stopEditing()
                check("back to showing", !editing && document !== null && document.slides.length > 0)
                check("the grid shows the presentation as it is", document.slides[1].plainText === catalog.open(file).slides[1].plainText)
            },
            // ---------------- second run: the lists, locks, Shift, slides, leaving
            () => {
                testInput.say("second run")
                startEditing(currentEntry())
                editScreen.showRow(2)
            },
            () => {
                const middle = canvas.mapToItem(null, canvas.originX + canvas.slideWidth / 2 * canvas.u, canvas.originY + canvas.slideHeight / 2 * canvas.u)
                testInput.mouse(0, middle.x, middle.y)
                testInput.mouse(2, middle.x, middle.y)
                lyricsId = canvas.selectedId
                check("picked on the third slide", sel() !== null && canvas.row === 2)
                start = { x: sel().x, y: sel().y, width: sel().width, height: sel().height }
                originalText = bridgeText(sel().text)
                // The eye in the list hides it.
                testInput.mouse(0, 20, 441)
                testInput.mouse(2, 20, 441)
                check("the eye hides the element", sel() !== null && sel().hidden === true)
            },
            () => {
                testInput.grab("f01-hidden")
                const middle = canvas.mapToItem(null, canvas.originX + canvas.slideWidth / 2 * canvas.u, canvas.originY + canvas.slideHeight / 2 * canvas.u)
                testInput.mouse(0, middle.x, middle.y)
                testInput.mouse(2, middle.x, middle.y)
                check("a hidden element is not picked on the slide", canvas.selectedId === "")
                testInput.mouse(0, 20, 441)
                testInput.mouse(2, 20, 441)
                check("the eye shows it again, and picks it", sel() !== null && sel().hidden === false)
                // The padlock locks it.
                testInput.mouse(0, 237, 441)
                testInput.mouse(2, 237, 441)
                check("the padlock locks the element", sel() !== null && sel().locked === true)
                const steps = editor.canUndo
                drag(at(0.5, 0.5), 60, 40, Qt.ControlModifier)
                check("a locked element does not move", canvas.elements.find(e => e.id === lyricsId).x === start.x)
            },
            () => {
                canvas.pick(lyricsId)
                testInput.grab("f02-locked")
                testInput.mouse(0, 237, 441)
                testInput.mouse(2, 237, 441)
                check("unlocked again", sel() !== null && sel().locked === false)
                // Shift keeps a move to one direction.
                drag(at(0.5, 0.5), 80, 30, Qt.ShiftModifier | Qt.ControlModifier)
                check("Shift keeps a move straight", sel().y === start.y && sel().x > start.x, sel().x + "," + sel().y)
                // Shift keeps a corner's resize in proportion.
                const ratio = sel().width / sel().height
                drag(at(1, 1), -100, -10, Qt.ShiftModifier)
                check("Shift keeps a resize in proportion", Math.abs(sel().width / sel().height - ratio) < 0.02, sel().width + "x" + sel().height + " ratio " + ratio)
            },
            () => {
                // Type, and go to another slide without finishing.
                const p = at(0.5, 0.5)
                testInput.mouse(3, p.x, p.y)
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Typed then left")
                editScreen.showRow(3)
                check("the next slide is shown", canvas.row === 3 && canvas.editingId === "")
                const saved = catalog.open(editor.path).slides
                check("what was typed went to the slide it was typed on", saved[2].plainText.toLowerCase() === "typed then left" && saved[3].plainText.toLowerCase() !== "typed then left",
                      saved[2].plainText + " | " + saved[3].plainText)
            },
            () => {
                // Undo, from this slide, a change made to the other: it comes into view.
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                check("undoing a change on another slide shows that slide", canvas.row === 2)
                check("and the change is undone", catalog.open(editor.path).slides[2].plainText.toLowerCase() === originalText.toLowerCase(), originalText)
                testInput.key(Qt.Key_Z, Qt.ControlModifier | Qt.ShiftModifier)
                check("redo puts it back", catalog.open(editor.path).slides[2].plainText.toLowerCase() === "typed then left")
            },
            () => {
                // The right-click menu, on the element.
                canvas.pick(lyricsId)
                const p = at(0.5, 0.5)
                testInput.mouse(0, p.x, p.y, 0, Qt.RightButton)
                testInput.mouse(2, p.x, p.y, 0, Qt.RightButton)
            },
            () => {
                testInput.grab("f03-menu")
                check("a right click opens the menu", menu.opened && menu.items.some(i => i.label === "Duplicate"))
                testInput.key(Qt.Key_Escape)
            },
            () => {
                check("Esc closes the menu", !menu.opened)
                // Rename from the list, with the keyboard.
                editScreen.renamingId = lyricsId
            },
            () => {
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Words")
                testInput.key(Qt.Key_Return)
                check("renamed in the list", sel() !== null && sel().name === "Words" && editScreen.renamingId === "", sel() ? sel().name : "")
                // Up on the first line of text being edited must not nudge the box.
                const y = sel().y
                canvas.editText(lyricsId)
                testInput.key(Qt.Key_Home, Qt.ControlModifier)
                testInput.key(Qt.Key_Up)
                testInput.key(Qt.Key_Left)
                check("arrows at the edge of the text leave the box alone", sel().y === y && canvas.editingId === lyricsId)
                // Type, then leave the editor without finishing.
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Left the editor")
                stopEditing()
                check("what was typed is saved on leaving", catalog.open(file).slides[2].plainText.toLowerCase() === "left the editor", catalog.open(file).slides[2].plainText)
                check("and the grid shows it", document.slides[2].plainText.toLowerCase() === "left the editor")
            },
            () => {
                testInput.grab("e12-show")
                testInput.say(editTestFailures === 0 ? "ALL PASSED" : editTestFailures + " FAILED")
                testInput.quit()
            }
        ]
        editTestStep = 0
        editTestTimer.start()
    }

    function bridgeText(richText) {
        return testInput.plain(richText)
    }

    Timer {
        id: editTestTimer

        interval: 500
        repeat: true
        onTriggered: {
            if (holder.editTestStep >= holder.editTestSteps.length) {
                stop()
                return
            }
            try {
                testInput.focus()
                holder.editTestSteps[holder.editTestStep]()
            } catch (e) {
                testInput.say("  EXCEPTION " + e + " at " + e.lineNumber)
                holder.editTestFailures++
            }
            holder.editTestStep++
        }
    }




}
