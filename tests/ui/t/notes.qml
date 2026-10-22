import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A slide's notes, and what a text box can be linked to. The notes: the button for them at the bottom right of the
// editor and the pane it puts up under the slide; notes ProPresenter wrote, read and drawn; typing, which is saved a
// moment later and, taken out again, leaves the file as it was; Ctrl+B, I and U; the panel a right click puts up, with
// a font, a size and a colour; the pane's top edge dragged; undo and redo; and that the editors of props, stage
// layouts and themes have neither button nor pane. The links: the one button that says what a box shows and the menu
// it opens; a slide's text, its notes (with their formatting or without) and its elements of a name; the clock and how
// it is written; a video's time. Then on the stage: the notes of the slide that is live, as words and as they were
// written, in the stage window and in what the operator sees of it; and the time a playing video has left. What the
// files then hold is looked at when the test is over, by after_notes in run.py.
QtObject {
    id: t

//COMMON
    readonly property string words: "Watch the band for the ending\nHold the last chord"

    function tabs() {
        return Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
    }

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

    function choice(name) {
        return Lib.findAll(editScreen.inspector, item => item.choice !== undefined && item.model !== undefined && item.chosen !== undefined)
                  .find(c => c.visible && c.objectName === name)
    }

    function shown(name) {
        const found = Lib.find(editScreen.inspector, item => item.objectName === name)
        return found !== null && found.visible ? found : null
    }

    // What the inspector's "Shows" button offers, as one list (see stage.qml)
    function showsChoice() {
        const button = Lib.find(editScreen.inspector, item => item.objectName === "linkChoice")
        if (!button || !button.visible)
            return undefined
        const rows = []
        for (const row of button.parent.parent.choices()) {
            if (row.items) {
                for (const sub of row.items) {
                    if (sub.label)
                        rows.push({ label: row.label + ": " + sub.label, run: sub.run, current: sub.current === true })
                }
            } else if (row.label) {
                rows.push({ label: row.label, run: row.run, current: row.current === true })
            }
        }
        return { shown: button.shown, model: rows.map(r => r.label), choice: rows.findIndex(r => r.current), chosen: (index) => rows[index].run() }
    }

    // The text box the test made, as the editor has it and as it is drawn on the editor's slide
    function made() {
        return editScreen.canvas.elements.find(e => e.id === kept.box) ?? null
    }

    function madeDrawn() {
        return Lib.findAll(editScreen.canvas, item => item.source !== undefined && item.liveLinkText !== undefined).find(d => d.source.id === kept.box)
    }

    // The stage as the operator sees it, and the boxes of the layout drawn in that
    function stagePreview() {
        return Lib.findAll(sidePanel, item => item.slide !== undefined && item.slideWidth !== undefined && item.showsNow !== undefined)
                  .find(item => item.visible && stageLayout !== null && item.slide !== null && item.slide.label === stageLayout.name)
    }

    function stageBox(test) {
        const preview = stagePreview()
        return preview ? Lib.findAll(preview, item => item.source !== undefined && item.textBleed !== undefined && item.boxX !== undefined).find(item => test(item.source)) : undefined
    }

    function seconds(text) {
        const parts = String(text).split(":").map(Number)
        return parts.length === 2 && parts.every(n => !isNaN(n)) ? parts[0] * 60 + parts[1] : -1
    }

    function run() {
        steps = [
            () => {
                // (The stage in the operator's preview, which is what these look at: the first audience screen is what it shows otherwise.)
                pickPreviewScreen(Screens.stage[0].id)
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
                // ---- Links: the one button, and its menu
                editScreen.canvas.addText()
                return 600
            },
            () => {
                kept.box = editScreen.canvas.selectedId
                editScreen.inspector.tab = "text"
                return 400
            },
            () => {
                const shows = showsChoice()
                check("what a text box shows is said on one button: its own text, to begin with", shows !== undefined && shows.shown === "Its Own Text" && shows.model[shows.choice] === "Its Own Text"
                      && made() !== null && made().linkKind === "none", shows ? shows.shown : "no button")
                click(centre(named("linkChoice")))
                return 500
            },
            () => {
                check("which opens a menu of what it can show, the elements and the timers each in a menu of their own",
                      labels() === "[Shows], *Its Own Text, --, Another Element, Current Slide, Next Slide, Timer, System Clock, Video Countdown, Playback Marker, Stage Message", labels())
                const timers = menu.shown.find(i => i.label === "Timer")
                check("the timers being the workspace's", timers.items.filter(i => i.label !== undefined).length === Timers.timers.length && Timers.timers.length > 0,
                      timers.items.map(i => i.label ?? i.header).join("|"))
                testInput.grab("6-shows-menu")
                click(menuRow("Current Slide"))
                return 500
            },
            () => {
                const source = choice("linkSlideSource")
                check("Current Slide links the box to the text of the slide that is live", made().linkKind === "slideText" && !made().linkSlideNext && made().linkSlideSource === Show.Words
                      && showsChoice().shown === "Current Slide", JSON.stringify([made().linkKind, made().linkSlideSource]))
                check("with a picker under it: Text, Notes, Match Element Name", source !== undefined && source.model.join("|") === "Text|Notes|Match Element Name" && source.choice === 0
                      && shown("linkSlideName") === null && shown("linkSlidePreserve") === null && shown("chordsCheck") !== null, source ? source.model.join("|") : "none")
                source.chosen(1)
                return 500
            },
            () => {
                check("Notes links it to the slide's notes, with a tick box for keeping their formatting, not ticked", made().linkSlideSource === Show.Notes && !made().linkSlidePreserve
                      && shown("linkSlidePreserve") !== null && !shown("linkSlidePreserve").checked && shown("linkSlideName") === null)
                check("and nothing about chords, which notes have none of", shown("chordsCheck") === null)
                testInput.snapshot(kept.path, "song-link-notes")
                testInput.grab("7-link-notes")
                click(centre(shown("linkSlidePreserve")))
                return 500
            },
            () => {
                check("ticking it keeps their formatting", made().linkSlidePreserve === true && shown("linkSlidePreserve").checked && onDisk(kept.row).elements.find(e => e.id === kept.box).linkSlidePreserve === true)
                testInput.snapshot(kept.path, "song-link-preserve")
                choice("linkSlideSource").chosen(2)
                return 500
            },
            () => {
                const name = shown("linkSlideName")
                check("Match Element Name gives a box to type an element's name in", made().linkSlideSource === Show.ElementNamed && name !== null && shown("linkSlidePreserve") === null)
                name.text = "Lyrics"
                name.editingFinished()
                return 500
            },
            () => {
                const disk = onDisk(kept.row).elements.find(e => e.id === kept.box)
                check("and the name typed is what it matches, what was said of the notes' formatting being left as it was", made().linkSlideName === "Lyrics" && disk.linkKind === "slideText"
                      && disk.linkSlideSource === Show.ElementNamed && disk.linkSlideName === "Lyrics" && disk.linkSlidePreserve === true,
                      JSON.stringify([disk.linkSlideSource, disk.linkSlideName, disk.linkSlidePreserve]))
                testInput.snapshot(kept.path, "song-link-name")
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("Next Slide"))
                return 500
            },
            () => {
                check("Next Slide is the same of the slide to come, set as it was", made().linkSlideNext === true && made().linkSlideSource === Show.ElementNamed && made().linkSlideName === "Lyrics"
                      && showsChoice().shown === "Next Slide")
                // ---- The clock
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("System Clock"))
                return 600
            },
            () => {
                const drawn = madeDrawn()
                check("System Clock links it to the time of day: the time alone, the short way, to begin with", made().linkKind === "clock" && made().linkClockDate === 0
                      && made().linkClockTime === 1 && made().linkClock24 === false && showsChoice().shown === "System Clock",
                      JSON.stringify([made().linkKind, made().linkClockDate, made().linkClockTime, made().linkClock24]))
                check("which the editor's slide shows", drawn !== undefined && /^\d{1,2}:\d\d \S+$/.test(drawn.liveLinkText), drawn ? drawn.liveLinkText : "not drawn")
                check("with how the date and the time are written under it, each one of five ways, and a tick box for the 24 hour clock", choice("linkClockDate") !== undefined
                      && choice("linkClockDate").model.join("|") === "None|Short|Medium|Long|Full" && choice("linkClockTime").choice === 1 && shown("linkClock24") !== null
                      && shown("linkSlideName") === null && choice("linkSlideSource") === undefined)
                choice("linkClockDate").chosen(4)
                click(centre(shown("linkClock24")))
                return 600
            },
            () => {
                const drawn = madeDrawn()
                const year = String(new Date().getFullYear())
                check("the date in full and the 24 hour clock are then what it shows", made().linkClockDate === 4 && made().linkClock24 === true && drawn.liveLinkText.includes(year)
                      && /\d\d:\d\d$/.test(drawn.liveLinkText) && !/[AaPp]\.?[Mm]\.?$/.test(drawn.liveLinkText), drawn.liveLinkText)
                testInput.snapshot(kept.path, "song-link-clock")
                testInput.grab("8-link-clock")
                // ---- A video's time
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("Video Countdown"))
                return 600
            },
            () => {
                const drawn = madeDrawn()
                check("Video Countdown links it to how long the video has left: minutes and seconds, to begin with", made().linkKind === "videoCountdown" && made().linkTimerMinutes === Timers.Long
                      && made().linkTimerSeconds === Timers.Long && made().linkTimerHours === Timers.None && showsChoice().shown === "Video Countdown",
                      JSON.stringify([made().linkKind, made().linkTimerHours, made().linkTimerMinutes, made().linkTimerSeconds]))
                check("which is nothing, with no video playing", drawn !== undefined && drawn.liveLinkText === "00:00", drawn ? drawn.liveLinkText : "not drawn")
                check("with the four parts of a time under it, and a tick box for videos that loop", shown("linkVideoIgnoreLooping") !== null && !shown("linkVideoIgnoreLooping").checked
                      && choice("linkClockDate") === undefined)
                click(centre(shown("linkVideoIgnoreLooping")))
                return 500
            },
            () => {
                check("which is kept when ticked", made().linkVideoIgnoreLooping === true && onDisk(kept.row).elements.find(e => e.id === kept.box).linkVideoIgnoreLooping === true)
                testInput.snapshot(kept.path, "song-link-video")
                editScreen.canvas.editText(kept.box)
                return 400
            },
            () => {
                check("a box that shows such a thing has no text of its own to type over, and the editor says what it shows", editScreen.canvas.editingId === ""
                      && editScreen.notice.includes("how long a video has left"), editScreen.notice)
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
                kept.stageHash = testInput.fileHash(StageLayouts.path)
                startEditingStage(layout("Slide Notes").id)
                return 700
            },
            () => {
                check("nor has the editor of the stage layouts", editing && editScreen.editor.kind === "stage" && !named("notesToggle").visible && pane() === null, editScreen.editor.kind)
                const notes = editScreen.canvas.elements.find(e => e.linkKind === "slideText" && e.linkSlideSource === Show.Notes)
                check("ProPresenter's own layout for the notes has a box linked to them", notes !== undefined && !notes.linkSlideNext && !notes.linkSlidePreserve)
                kept.notesBox = notes.id
                editScreen.canvas.pick(notes.id)
                editScreen.inspector.tab = "text"
                return 500
            },
            () => {
                check("which the inspector says: the current slide, its notes, their formatting not kept", showsChoice().shown === "Current Slide" && choice("linkSlideSource").choice === 1
                      && shown("linkSlidePreserve") !== null && !shown("linkSlidePreserve").checked && testInput.fileHash(StageLayouts.path) === kept.stageHash)
                stopEditing()
                return 600
            },
            () => {
                // ---- On the stage
                click(centre(tabs()[2]))
                return 400
            },
            () => {
                Show.setStageLayout(Show.stageScreens[0].id, layout("Slide Notes").id)
                openEntry(entry("Notes Song"))
                return 600
            },
            () => {
                goLive(kept.row)
                return 1500
            },
            () => {
                const notes = stageBox(e => e.id === kept.notesBox)
                check("with the slide live, the stage's box for the notes shows that slide's notes", stageLayout !== null && stageLayout.name === "Slide Notes" && notes !== undefined
                      && notes.liveLinkText === words, notes ? JSON.stringify(notes.liveLinkText) : "no box")
                check("as words in the box's own style, ProPresenter's layout not keeping their formatting", typeof notes.liveLinkText === "string")
                kept.stageRed = testInput.red(true, 0, 0, 1, 1)
                kept.previewRed = red(stagePreview())
                check("drawn in the stage window, and in what the operator sees of the stage, with nothing red in either", testInput.lit(true, 0, 0.15, 1, 0.55) > 0.03
                      && lighter(notes, 150) > 0.02 && kept.stageRed === 0 && kept.previewRed === 0,
                      testInput.lit(true, 0, 0.15, 1, 0.55) + " and " + lighter(notes, 150) + ", red " + kept.stageRed + " and " + kept.previewRed)
                testInput.grabStage("10-stage-notes-plain")
                testInput.grab("10-operator-notes-plain")
                goLive(kept.bare)
                return 1000
            },
            () => {
                // (A slide can say which layout the stage is to have, as the song's first does: the layout is asked for again.)
                clearMedia()
                Show.setStageLayout(Show.stageScreens[0].id, layout("Slide Notes").id)
                return 800
            },
            () => {
                const notes = stageBox(e => e.id === kept.notesBox)
                check("a slide with no notes shows none", stageLayout.name === "Slide Notes" && (notes === undefined || notes.liveLinkText === "") && testInput.lit(true, 0, 0.15, 1, 0.55) < 0.005,
                      (notes ? JSON.stringify(notes.liveLinkText) : "no box") + " " + testInput.lit(true, 0, 0.15, 1, 0.55))
                startEditingStage(layout("Slide Notes").id)
                return 700
            },
            () => {
                editScreen.canvas.pick(kept.notesBox)
                editScreen.inspector.tab = "text"
                return 400
            },
            () => {
                click(centre(shown("linkSlidePreserve")))
                return 500
            },
            () => {
                check("the layout's box set to keep the notes' formatting", editScreen.canvas.selected.linkSlidePreserve === true && testInput.fileHash(StageLayouts.path) !== kept.stageHash)
                stopEditing()
                return 600
            },
            () => {
                goLive(kept.row)
                return 1000
            },
            () => {
                Show.setStageLayout(Show.stageScreens[0].id, layout("Slide Notes").id)
                return 1000
            },
            () => {
                const notes = stageBox(e => e.id === kept.notesBox)
                const drawn = notes && typeof notes.liveLinkText === "object" ? testInput.runsOf(notes.liveLinkText) : []
                check("then shows them as they were written: the word that was made red, bigger and bold is so", drawn.length === 3 && drawn[0].text === "Watch" && String(drawn[0].color) === "#ff2a1a"
                      && drawn[0].size === 40 && drawn[0].bold === true && String(drawn[1].color) === "#ffffff", told(drawn))
                const stageRed = testInput.red(true, 0, 0, 1, 1)
                const previewRed = red(stagePreview())
                // (At their own sizes, which on a layout as big as a screen are small: a few points of red, where there were none.)
                check("which is what the stage window draws: red where there was none", stageRed > 0.0001, kept.stageRed + " then " + stageRed)
                check("and what the operator sees of the stage", previewRed > 0.0001, kept.previewRed + " then " + previewRed)
                testInput.grabStage("11-stage-notes-formatted")
                testInput.grab("11-operator-notes-formatted")
                // ---- A video's time, on ProPresenter's own layout that has one
                clearMedia()
                Show.setStageLayout(Show.stageScreens[0].id, layout("Singing").id)
                return 1200
            },
            () => {
                const video = stageBox(e => e.linkKind === "videoCountdown")
                kept.second = Clocks.second
                check("ProPresenter's layout for singing has a video's time, at nothing with none playing", video !== undefined && video.liveLinkText === "00:00" && player() === null
                      && Clocks.videoWatched && lighter(video, 60) > 0.01, video ? video.liveLinkText + " " + lighter(video, 60) : "no box")
                for (const list of catalog.mediaPlaylists.filter(p => !p.folder)) {
                    openMediaPlaylist(list.path)
                    if (mediaFiles.some(m => m.video))
                        break
                }
                kept.video = mediaFiles.find(m => m.video)
                showMedia(kept.video, mediaPlaylistId)
                return 2500
            },
            () => {
                const video = stageBox(e => e.linkKind === "videoCountdown")
                const left = player() ? (player().duration - player().position) / 1000 : -1
                kept.loops = liveMedia !== null && liveMedia.loops === true
                kept.ignores = video.source.linkVideoIgnoreLooping === true
                kept.left = seconds(video.liveLinkText)
                check("a video is playing", kept.video !== undefined && player() !== null && player().duration > 3000 && left > 0, left)
                check("and the box counts what it has left" + (kept.loops && kept.ignores ? " (not this one, which loops, and the layout ignores those)" : ""),
                      kept.loops && kept.ignores ? video.liveLinkText === "00:00" : kept.left > 0 && Math.abs(kept.left - left) <= 1.5, video.liveLinkText + " with " + left + " left")
                // The box the test made on the slide that is live, which was set to ignore videos that loop: as the output has it
                const onOutput = Lib.findAll(output.contentItem, item => item.source !== undefined && item.liveLinkText !== undefined).find(d => d.source.id === kept.box)
                check("the slide's own box, on the output, which ignores videos that loop, " + (kept.loops ? "has this one, which loops, at nothing" : "counts this one, which does not loop"),
                      onOutput !== undefined && (onOutput.liveLinkText === "00:00") === kept.loops, onOutput ? onOutput.liveLinkText : "not on the output")
                check("the clock on the same layout goes round with the seconds, while it is watched", Clocks.second !== kept.second, kept.second + " then " + Clocks.second)
                testInput.grabStage("12-stage-video")
                return 2000
            },
            () => {
                const video = stageBox(e => e.linkKind === "videoCountdown")
                const now = seconds(video.liveLinkText)
                const left = (player().duration - player().position) / 1000
                check("and goes on counting as the video plays", kept.loops && kept.ignores ? now === 0 : now > 0 && Math.abs(now - left) <= 1.5 && (now < kept.left || player().duration < 6000),
                      kept.left + " then " + now)
                clearMedia()
                return 1200
            },
            () => {
                const video = stageBox(e => e.linkKind === "videoCountdown")
                check("with the video cleared it is at nothing again", video.liveLinkText === "00:00", video.liveLinkText)
            }
        ]
        next()
    }
}
