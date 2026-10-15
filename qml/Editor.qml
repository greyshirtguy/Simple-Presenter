import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SimplePresenterApp

// The editor, laid over the operator window below its toolbar. On the left are the
// presentation's slides and, under them, the elements of the slide being worked on, front
// one first; in the middle is that slide, where elements are picked, moved, resized and
// their text edited in place; on the right are the picked element's properties.
//
// The order of that list is the order the elements lie in on the slide, which is what
// says which of two that overlap is seen. A row of it is dragged up or down to change
// that, as a row of ProPresenter's list is (see carry(), below); the menu of an element
// has the same a step at a time, and to the front or the back. How it looks while a row
// is held (the line where it would land, its name with the pointer) is this app's own,
// after its other lists, and not copied from ProPresenter's.
//
// Every change is saved to the presentation file as it is made, and can be undone.
//
// The same editor works on the workspace's props and on its stage layouts, which are
// slides too: the list on the left is then of the props or the layouts in place of a
// presentation's slides, and has what a presentation's list does not, a way to add one
// and to rename, copy or remove it (a presentation's slides are added to in
// ProPresenter). Those are done by what keeps the props and the layouts (Props,
// StageLayouts), which changes the file; the editor then opens the file again.
//
// How it is put together.
//
//   PresentationEditor (src/presentationeditor.h) holds the file, parsed, and is the
//   only thing that changes it; it is also the list model of the slides. A change is a
//   call to it, which alters the one slide, writes the file, and describes that slide
//   afresh for whatever draws it. Undo is a list of that slide's cue as it was before
//   and after each change (and, for a change of a song's key, of its two keys). It
//   also keeps the file as it was when it was opened, which Revert puts back whole.
//
//   EditorModes is the switch, in the middle of the toolbar, between the editor's
//   three ways of working on a presentation (`mode`, below); ChordSheet and
//   ChordProEditor are the two that are for a song's chords. ReflowEditor is a
//   fourth way, the slides' words as a table, which the window's own toolbar leads
//   to.
//
//   EditorCanvas draws the slide being worked on with the same SlideElement that draws
//   the output, and lays over it what editing needs: the handles, the lines snapped
//   to, and a TextEdit for typing into a text box where it stands.
//
//   EditorInspector shows the properties of the picked element and reports what its
//   controls are set to. It changes nothing itself.
//
//   This file joins them: the lists on the left, the menus and the keys, and handing
//   what the inspector reports to the canvas, which applies it to the picked element.
//
// Dragging something (an element, a slider, a colour) would save a change and leave a
// step to undo for every pixel of the way. So a change comes in two kinds: one that is
// shown but not saved ("interim" here, a preview in PresentationEditor), of which any
// number may follow one another, and settling, which saves where it ended up as one
// change.
Rectangle {
    id: screen

    property real sidebarWidth: 260
    // The colour a slide's frame takes from its group, as the slide grid has it
    property var groupColor: (slide) => "#2b2d31"
    // The url of an image to show behind the slide with this id, or ""
    property var backdropFor: (slideId) => ""
    // Opens a menu of rows, as the operator window's menu takes them, at a point of an item
    property var showMenu: (items, item, x, y) => {}
    // A timer link's Color Triggers kept as the default, and how one is kept (the
    // window's, in the app's own settings): for the inspector
    property var colorTriggerDefault: []
    signal colorTriggerDefaultKept(var list)
    // Deletes a slide from a presentation file, answering with an error message, empty
    // if it worked: the operator window's doing, as every change to which slides a
    // presentation has is.
    property var removeSlide: (path, slideId) => ""
    // Adds a slide with nothing on it after the one with this id, and pastes the copied
    // slide there: each answers { id, error }. And copies a slide, answering an error.
    property var insertSlide: (path, slideId) => ({ id: "", error: "" })
    property var pasteSlide: (path, slideId) => ({ id: "", error: "" })
    property var copySlide: (path, slideId) => ""
    // Whether a slide has been copied
    property bool canPaste: false
    // How many times its usual size the chord sheet is drawn and the ChordPro text is
    // set, and the colour of a chord in each ("" for the one it comes with): the
    // operator window's, which keeps them. A change asked for here is told to it.
    property real sheetZoom: 0.8
    property real textZoom: 1
    property string sheetChordColor: ""
    property string textChordColor: ""
    // And of the words of the chord sheet's highlighted line ("" for the one it comes
    // with), and whether the sheet and the song's keys draw sharps and flats as ♯ and ♭
    property string sheetLineColor: ""
    property bool chordSigns: true
    // Whether the pane for a slide's notes is up under the slide, and how tall it is
    // asked to be: the window's to keep, as the zooms are (notesShownEdited,
    // notesHeightEdited say what was asked for here).
    property bool notesShown: false
    property real notesHeight: 150
    // Something to tell the user, and whether it is a failure
    property string notice
    property bool noticeIsError: true
    // The element being renamed in the list, by id; "" for none
    property string renamingId
    // An element whose row is being dragged up or down the list, by id, "" while none
    // is; where the pointer has it, as a point of the list; and where it would land if
    // it were let go: the row of the list it would be put before, or the number of rows
    // for under the last, which is the back (see src/elementorder.h). -1 is nowhere:
    // it is held off to one side of the list, and letting go there leaves it as it was.
    //
    // (None of this is in the rows themselves, and what is shown of it is not made
    // until a row is dragged: the list's rows are all made again at every step of a
    // property being dragged in the inspector, so a row is kept to what it must have.
    // An opacity bound in each row for the one that is held cost those drags 2 ms a
    // step on the benchmark's slide of seven elements.)
    property string carriedId
    property point carriedAt
    property int landing: -1
    // Which row of the list it is, and where the press that may become a drag was
    property int carriedRow: -1
    property point listPressedAt
    // How high a row of that list is
    readonly property real elementRowHeight: 32
    // Which way the list is going under an element held near its top or its bottom, to
    // bring the rows beyond into reach, and how fast, in points a second (downwards
    // counted up); 0 where it is not held near either, or there is no further to go.
    readonly property real carriedScroll: {
        if (carriedId === "" || landing < 0)
            return 0
        const edge = 24
        const fastest = 420
        const furthest = Math.max(0, elementList.contentHeight - elementList.height)
        if (carriedAt.y > elementList.height - edge && elementList.contentY < furthest)
            return fastest * Math.min(1, Math.max(0.15, (carriedAt.y - (elementList.height - edge)) / edge))
        if (carriedAt.y < edge && elementList.contentY > 0)
            return -fastest * Math.min(1, Math.max(0.15, (edge - carriedAt.y) / edge))
        return 0
    }
    // The row being renamed in the list on the left, by id; "" for none: a prop, a
    // stage layout, a theme's slide or a mask, whose name it is, or a slide of a
    // presentation, whose label it is
    property string renamingRow
    // The workspace folder what is open is in
    property string workspace

    // What is open: "presentation", "props", "stage", "theme" or "masks"; and what a
    // row of the list on the left is then called
    readonly property string kind: editor.kind
    // Which of the editor's three ways of working is on. "slides" is the editor as
    // ProPresenter has it: a slide at a time, its elements moved and styled. The other
    // two are for a song's chords and are this app's own: "chords", the song as one
    // sheet with its chords in bubbles (ChordSheet), and "chordpro", the same as
    // ChordPro text (ChordProEditor). They are for a presentation only: props, stage
    // layouts and themes have no chords.
    //
    // And a fourth, "reflow": the slides' words as a table, for moving them between
    // slides (ReflowEditor). It is come to from the window's own toolbar and not from
    // the switch of the other three, and has the whole of the editor to itself: the
    // list of slides at the left gives way to it, the table being that list.
    property string mode: "slides"
    readonly property bool chordable: kind === "presentation"
    // How wide the reflow editor draws a slide, as it was left
    property real reflowThumbnail: 220
    // Whether what is being edited has notes: the slides of a presentation do, and
    // nothing else that is edited here (a prop, a stage layout and a theme's slide
    // have nowhere in their files for any). And whether their pane is up.
    readonly property bool notable: kind === "presentation" && mode === "slides"
    readonly property bool notesUp: notable && notesShown
    readonly property string rowWord: kind === "props" ? "Prop" : kind === "stage" ? "Layout" : kind === "theme" ? "Theme Slide"
                                    : kind === "masks" ? "Mask" : "Slide"
    // For a theme: its place under the workspace's Themes folder (see Themes)
    readonly property string themePlace: kind === "theme" ? editor.path.replace(/^.*\/Themes\//, "").replace(/\/Theme$/, "") : ""

    readonly property alias editor: editor
    readonly property alias canvas: canvas
    readonly property alias inspector: inspector
    readonly property var families: bridge.fontFamilies()
    readonly property color accentColor: "#ff8a1f"

    // What is being edited, by name: a presentation, or the workspace's props or stage
    // layouts
    readonly property string subject: kind === "props" ? "Props" : kind === "stage" ? "Stage Layouts" : kind === "theme" ? "Theme: " + themePlace.split("/").pop()
                                    : kind === "masks" ? "Masks" : editor.path.replace(/^.*\//, "").replace(/\.pro$/i, "")
    // What the toolbar button under the pointer does, in words, or ""
    readonly property string toolHint: {
        if (notesToggle.hovered)
            return notesToggle.hint
        // (One of the key's chords, in the strip over the chord sheet)
        if (mode === "chords" && chordView.item && chordView.item.hint)
            return chordView.item.hint
        if (slideBreak.hovered)
            return slideBreak.hint
        for (const tools of [actions.children, songKeys.item ? songKeys.item.children : [], saving.item ? saving.item.children : []]) {
            for (let i = 0; i < tools.length; ++i) {
                const tool = tools[i]
                if (tool.hovered === true && tool.hint)
                    return tool.hint
            }
        }
        return modes.hint
    }
    // The keys a song can be said to be in: every usual one (chords.h)
    readonly property var usualKeys: Chords.majorKeys.concat(Chords.minorKeys)
    // What the line along the bottom says after a revert, while it can be taken back
    readonly property string revertedNotice: "The song is as it was when it was opened here."
    // A filter for a file dialog showing the media files the app can use, and the
    // folder such a dialog starts in
    property string mediaFilter: "All files (*)"
    property string mediaFolder: ""

    // Asks for a media file: to "add" as an element of its own, or to "fill" the picked
    // element with.
    function chooseMedia(purpose) {
        finish()
        mediaDialog.purpose = purpose
        mediaDialog.open()
    }

    // A key that belongs to the operator window, not the editor, was pressed
    signal keyPassed(var event)
    signal sheetZoomEdited(real zoom)
    signal reflowThumbnailEdited(real width)
    signal textZoomEdited(real zoom)
    signal notesShownEdited(bool shown)
    signal notesHeightEdited(real height)

    // Opens a presentation file from a workspace folder, at the slide with the given id
    // if it has one. Returns an error message, empty on success.
    function open(path, workspace, slideId) {
        return opened(editor.open(path, workspace), workspace, slideId)
    }

    // Opens the props of a workspace, or its stage layouts, at the one with the given
    // id, or failing that at the given row: `path` is the file they are in.
    function openProps(path, workspace, id, row) {
        return opened(editor.openProps(path, workspace), workspace, id, row)
    }

    function openStage(path, workspace, id, row) {
        return opened(editor.openStageLayouts(path, workspace), workspace, id, row)
    }

    // A theme, at the theme slide with the given id: `path` is its Theme file.
    function openTheme(path, workspace, id, row) {
        return opened(editor.openTheme(path, workspace), workspace, id, row)
    }

    // The workspace's masks, at the one with the given id: `path` is the workspace's
    // set-up file, which they are a part of.
    function openMasks(path, workspace, id, row) {
        return opened(editor.openMasks(path, workspace), workspace, id, row)
    }

    // What follows opening any of them: the row with the given id is shown, or failing
    // that the one at `row`, or the first.
    function opened(error, workspace, id, row) {
        if (error !== "")
            return error
        screen.workspace = workspace
        mode = "slides"
        notice = ""
        renamingId = ""
        renamingRow = ""
        const found = id ? editor.rowOf(id) : -1
        canvas.showRow(found >= 0 ? found : Math.max(0, Math.min(row ?? 0, editor.count - 1)))
        slideList.positionViewAtIndex(canvas.row, ListView.Center)
        return ""
    }

    // Makes a change to which props or stage layouts there are, or to what one is
    // called: `change` is a call to what keeps them, and answers with an error message
    // or with { id, error } for something it made. The file is then opened again, at
    // what was made, or at what was being shown, or at the row that was. Such a change
    // is not one of the editor's own: it cannot be undone here, and what could be
    // undone before it no longer can.
    function restructure(change) {
        finish()
        const row = canvas.row
        const shown = canvas.slide ? canvas.slide.id : ""
        const result = change()
        const error = typeof result === "string" ? result : result.error
        const made = typeof result === "string" ? "" : result.id
        const reopened = kind === "props" ? openProps(editor.path, workspace, made !== "" ? made : shown, row)
                       : kind === "stage" ? openStage(editor.path, workspace, made !== "" ? made : shown, row)
                       : kind === "theme" ? openTheme(editor.path, workspace, made !== "" ? made : shown, row)
                       : kind === "masks" ? openMasks(editor.path, workspace, made !== "" ? made : shown, row)
                       : opened(editor.open(editor.path, workspace), workspace, made !== "" ? made : shown, row)
        report(error !== "" ? error : reopened)
        takeFocus()
        return error === "" ? made : ""
    }

    // Adds a prop, to the collection of the one being shown, or a stage layout, or a
    // slide with nothing on it after the one being shown.
    //
    // TODO, when there are themes: a new slide is then a choice, of a theme's slide or
    // a blank one, and everything that makes a slide (this, New Slide in the list's
    // menu, media dropped between slides) is to offer it.
    function addRow() {
        if (kind === "props") {
            const beside = canvas.slide ? Props.find(canvas.slide.id) : ({})
            restructure(() => Props.add(beside.collection ?? ""))
        } else if (kind === "stage") {
            restructure(() => StageLayouts.add())
        } else if (kind === "theme") {
            restructure(() => Themes.addSlide(themePlace))
        } else if (kind === "masks") {
            restructure(() => Masks.add(""))
        } else {
            restructure(() => screen.insertSlide(editor.path, canvas.slide ? canvas.slide.id : ""))
        }
    }

    // Ends a rename in the list on the left, with the name typed (`given` is false
    // for one that was given up with Esc).
    //
    // For a slide of a presentation the name is the slide's label, and typing none
    // takes the label's words away. That is a change of the editor's own, undone like
    // any other. What a prop, a layout, a theme's slide and a mask are called is
    // changed by what keeps them, and none of those can be called nothing.
    function renameRow(id, name, given) {
        const row = editor.rowOf(id)
        renamingRow = ""
        if (row < 0 || given === false || name === editor.slideAt(row).label) {
            takeFocus()
            return
        }
        if (kind === "presentation") {
            finish()
            report(editor.setLabel(row, name))
            takeFocus()
        } else if (name !== "") {
            restructure(() => kind === "props" ? Props.rename(id, name) : kind === "theme" ? Themes.renameSlide(themePlace, id, name)
                              : kind === "masks" ? Masks.rename(id, name) : StageLayouts.rename(id, name))
        } else {
            takeFocus()
        }
    }

    // The menu of a prop or a stage layout in the list on the left, at a point of the
    // list. (The list, and not the row, is what it is opened on: the changes it offers
    // rebuild the rows.)
    function showRowMenu(slide, x, y) {
        if (kind === "presentation") {
            // A slide can be deleted, unless it is the only one. Slides that only
            // trigger media are not in this list, so the count here is of the others.
            const number = editor.rowOf(slide.id) + 1
            showMenu([
                { header: "Slide " + number },
                // Its label, typed where it stands under the slide
                { label: "Rename", run: () => renamingRow = slide.id },
                // A new slide, and a pasted one, go after this one, and are then the
                // slide being worked on.
                { label: "New Slide", run: () => restructure(() => screen.insertSlide(editor.path, slide.id)) },
                { label: "Copy", run: () => {
                    finish()
                    report(screen.copySlide(editor.path, slide.id))
                } },
                { label: "Paste", disabled: !screen.canPaste, run: () => restructure(() => screen.pasteSlide(editor.path, slide.id)) },
                { label: "Delete Slide…", danger: true, disabled: editor.count < 2, run: () => showMenu([
                    { note: "Slide " + number + " will be deleted. That cannot be undone, and neither can what was done before it." },
                    { label: "Delete", danger: true, run: () => restructure(() => screen.removeSlide(editor.path, slide.id)) },
                    { label: "Cancel", run: () => {} }
                ], slideList, x, y) }
            ], slideList, x, y)
            return
        }
        if (kind === "masks") {
            showMenu([
                { header: slide.label !== "" ? slide.label : rowWord },
                { label: "Rename", run: () => renamingRow = slide.id },
                // A screen that a look gives the mask to has none after this, so the
                // looks are read again with it gone from them.
                { label: "Remove…", danger: true, run: () => showMenu([
                    { note: "“" + slide.label + "” will be removed, and taken from every look that gives it to a screen. That cannot be undone." },
                    { label: "Remove", danger: true, run: () => restructure(() => {
                        const error = Masks.remove(slide.id)
                        Looks.open(screen.workspace)
                        return error
                    }) }
                ], slideList, x, y) }
            ], slideList, x, y)
            return
        }
        if (kind === "theme") {
            showMenu([
                { header: slide.label !== "" ? slide.label : rowWord },
                { label: "Rename", run: () => renamingRow = slide.id },
                { label: "Remove…", danger: true, disabled: editor.count < 2, run: () => showMenu([
                    { note: "“" + slide.label + "” will be removed from the theme. That cannot be undone." },
                    { label: "Remove", danger: true, run: () => restructure(() => Themes.removeSlide(themePlace, slide.id)) }
                ], slideList, x, y) }
            ], slideList, x, y)
            return
        }
        const props = kind === "props"
        showMenu([
            { header: slide.label !== "" ? slide.label : rowWord },
            { label: "Rename", run: () => renamingRow = slide.id },
            { label: "Duplicate", run: () => restructure(() => props ? Props.duplicate(slide.id)
                                                                       : StageLayouts.duplicate(slide.id)) },
            { label: "Remove…", danger: true, run: () => showMenu([
                { note: "“" + slide.label + "” will be removed. That cannot be undone." },
                { label: "Remove", danger: true,
                  run: () => restructure(() => props ? Props.remove(slide.id) : StageLayouts.remove(slide.id)) }
            ], slideList, x, y) }
        ], slideList, x, y)
    }

    // Finishes whatever is under way, so that all of it is in the file.
    function finish() {
        canvas.finishText()
        canvas.settle()
        if (notes.item)
            notes.item.send()
        // Whichever chord editor is up keeps what was being typed in it: a chord in its
        // bubble, or ChordPro text that is saved a moment after the typing stops.
        if (mode !== "slides" && chordView.item)
            chordView.item.settle()
    }

    function close() {
        finish()
        // The chord editors go before what they were editing does.
        mode = "slides"
        editor.close()
    }

    function takeFocus() {
        if (mode !== "slides" && chordView.item)
            chordView.item.takeKeys()
        else
            canvas.takeFocus()
    }

    function setMode(next) {
        if (next === mode || (next !== "slides" && !chordable))
            return
        // What was being typed on the slide is finished first.
        finish()
        mode = next
        takeFocus()
    }

    function report(error) {
        notice = error
        noticeIsError = true
        if (error !== "") {
            noticeTimer.restart()
            Log.problem("Shown in the editor: " + error)
        }
        return error === ""
    }

    function tell(message) {
        notice = message
        noticeIsError = false
        noticeTimer.restart()
    }

    // Revert asks first. It takes back everything done to the song since it was opened
    // here, which may be an hour's work, and its name alone could be read as taking
    // back the last change, which is Undo's. So the question says which: all of them.
    function askToRevert(item) {
        if (!editor.canRevert)
            return
        // (The question is the menu's note and not its heading: a heading is one line,
        // cut short where it is too long, and this is to be read whole.)
        showMenu([
            { header: "Revert" },
            { note: "Revert “" + subject + "” to how it was when it was opened? This takes back EVERY change made to this song since it was opened in this editor." },
            { label: "Revert All Changes", danger: true, run: () => screen.revert() },
            { label: "Cancel", run: () => screen.takeFocus() }
        ], item, 0, -150)
    }

    // The answer was yes: the song's file is made what it was when it was opened here
    // (PresentationEditor::revertToOpened, which keeps what it is now first), and the
    // line along the bottom offers to take that back for as long as it says so.
    function revert() {
        finish()
        const row = canvas.row
        if (!report(editor.revertToOpened()))
            return
        Log.note("edit", "the presentation was reverted to as it was opened in the editor")
        canvas.showRow(Math.max(0, Math.min(row, editor.count - 1)))
        slideList.positionViewAtIndex(canvas.row, ListView.Contain)
        tell(revertedNotice)
        takeFocus()
    }

    function takeBackRevert() {
        finish()
        const row = canvas.row
        if (!report(editor.takeBackRevert()))
            return
        Log.note("edit", "the revert was taken back")
        canvas.showRow(Math.max(0, Math.min(row, editor.count - 1)))
        slideList.positionViewAtIndex(canvas.row, ListView.Contain)
        tell("The song is as it was before it was reverted.")
        takeFocus()
    }

    // Sets one of the song's two keys from its picker. Whatever was being typed in a
    // chord editor is kept first: nothing else is done to the song before that.
    function pickKey(key, shown) {
        finish()
        if (key !== "None")
            report(shown ? editor.setShownKey(key) : editor.setKey(key))
        takeFocus()
    }

    function showRow(row) {
        if (row < 0 || row >= editor.count || row === canvas.row)
            return
        canvas.showRow(row)
        slideList.positionViewAtIndex(row, ListView.Contain)
    }

    // A press on an element's row in the list: picks it, and with the right button
    // opens its menu at that point of the list.
    function listPressed(id, menu, x, y) {
        listPressedAt = Qt.point(x, y)
        canvas.pick(id)
        takeFocus()
        if (menu)
            showElementMenu(elementList, x, y)
    }

    // A row of the list has been moved with the button down, to this point of the list.
    // Once that is a little way from where it was pressed the row is being dragged, as
    // a row of the other lists is (see DragSource).
    function listDragged(id, at) {
        if (renamingId !== "")
            return
        if (carriedId !== "" || Math.abs(at.x - listPressedAt.x) + Math.abs(at.y - listPressedAt.y) > 10)
            carry(id, at)
    }

    // An element's row is being dragged up or down the list: it is held at this point
    // of the list, and would land in the gap between two rows that is nearest (over
    // the upper half of a row is before that row, over the lower half after it). Held
    // above the list it would be the front one, and below it the back one.
    function carry(id, at) {
        if (carriedId !== id)
            carriedRow = canvas.elements.length - 1 - canvas.elements.findIndex(e => e.id === id)
        carriedId = id
        carriedAt = at
        landing = at.x < 0 || at.x > elementList.width ? -1
            : Math.max(0, Math.min(elementList.count, Math.round((elementList.contentY - elementList.originY + at.y) / elementRowHeight)))
    }

    // It has been let go: it goes where it would land, in the list and so in the order
    // the elements are drawn in.
    function dropCarried() {
        const id = carriedId
        const place = landing
        letGo()
        if (id !== "" && place >= 0)
            canvas.place(id, place)
    }

    // Nothing is being dragged in the list any more, and nothing is moved.
    function letGo() {
        carriedId = ""
        carriedRow = -1
        landing = -1
    }

    // Ends a rename in the list, with the name typed.
    function rename(id, name) {
        const element = canvas.elements.find(e => e.id === id)
        renamingId = ""
        if (element && name !== "" && name !== element.name)
            report(editor.setProperties(canvas.row, id, { name: name }))
        takeFocus()
    }

    // The menu for the picked element, or for the slide if none is.
    function showElementMenu(item, x, y) {
        const element = canvas.selected
        const items = []
        if (element) {
            const index = canvas.elements.findIndex(e => e.id === element.id)
            const last = canvas.elements.length - 1
            items.push({ header: element.name !== "" ? element.name : "Element" })
            if (!element.locked && !element.hidden)
                items.push({ label: "Edit Text", run: () => canvas.editText(element.id) })
            items.push({ label: "Rename", run: () => renamingId = element.id })
            items.push({ label: "Duplicate", run: () => canvas.duplicate() })
            if (last > 0)
                items.push({ header: "Order" })
            if (index < last) {
                items.push({ label: "Bring to Front", run: () => canvas.reorder(last) })
                items.push({ label: "Bring Forward", run: () => canvas.reorder(1) })
            }
            if (index > 0) {
                items.push({ label: "Send Backward", run: () => canvas.reorder(-1) })
                items.push({ label: "Send to Back", run: () => canvas.reorder(-last) })
            }
            items.push({ header: "Element" })
            items.push({ label: element.hidden ? "Show" : "Hide",
                         run: () => canvas.setProperties({ hidden: !element.hidden }, false) })
            items.push({ label: element.locked ? "Unlock" : "Lock",
                         run: () => canvas.setProperties({ locked: !element.locked }, false) })
            items.push({ label: "Delete", danger: true, run: () => canvas.removeSelected() })
        } else {
            items.push({ label: "Add Text", run: () => canvas.addText() })
        }
        showMenu(items, item, x, y)
    }

    color: "#1e1f22"
    // Keys nothing in the editor took: the clears still work from here.
    Keys.onPressed: (event) => keyPassed(event)

    PresentationEditor {
        id: editor

        // Undoing a change made to another slide shows that slide.
        onRestored: (row) => screen.showRow(row)
    }

    RichTextBridge {
        id: bridge
    }

    Timer {
        id: noticeTimer

        interval: 8000
        onTriggered: screen.notice = ""
    }

    // Swallows clicks and scrolling that nothing here takes, so that the operator
    // window underneath does not.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onPressed: screen.takeFocus()
    }

    component PanelTitle: Text {
        leftPadding: 12
        topPadding: 10
        bottomPadding: 6
        color: "#9a9da3"
        font.pixelSize: 12
        font.bold: true
        font.capitalization: Font.AllUppercase
    }

    // Slides, and the elements of the one being worked on
    Rectangle {
        id: left

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        // (Not there while the reflow editor is up, which has the slides as its rows:
        // no width, so that what is set against it begins at the window's edge. The
        // width goes by the way of working and not by `visible`: an item's `visible`
        // is false for as long as anything it is in is out of sight, which the whole
        // editor is until it is opened, and a width that went by it was nothing until
        // then and the editor was laid out twice over as it opened, the slide drawn at
        // one size and then at another. The benchmark showed 110 ms of that.)
        visible: screen.mode !== "reflow"
        width: screen.mode !== "reflow" ? Math.max(200, Math.min(screen.sidebarWidth, 360)) : 0
        color: "#2b2d31"

        PanelTitle {
            id: slidesTitle

            color: "#4da3ff"
            text: screen.kind === "props" ? "Props" : screen.kind === "stage" ? "Stage Layouts" : screen.kind === "theme" ? "Theme Slides"
                : screen.kind === "masks" ? "Masks" : "Slides"
        }

        // Adds a slide, a prop or a stage layout: see addRow().
        AppButton {
            objectName: "editorAddRow"
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: slidesTitle.verticalCenter
            anchors.verticalCenterOffset: 2
            width: 26
            height: 22
            leftPadding: 0
            rightPadding: 0
            font.pixelSize: 14
            text: "+"
            onClicked: screen.addRow()
        }

        ListView {
            id: slideList

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: slidesTitle.bottom
            anchors.bottom: elementsHeader.top
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: editor

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Item {
                id: cell

                required property var slide
                required property int index
                readonly property bool current: index === canvas.row
                readonly property bool renaming: screen.renamingRow !== "" && screen.renamingRow === slide.id
                readonly property color frame: screen.groupColor(slide)
                readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6

                width: ListView.view.width
                height: (width - 36 - 6) * 9 / 16 + 3 + 22 + 10

                // The slide being worked on is ringed.
                Rectangle {
                    anchors.fill: frameRect
                    anchors.margins: -4
                    radius: 7
                    visible: cell.current
                    color: "transparent"
                    border.width: 2.5
                    border.color: screen.accentColor
                }

                Rectangle {
                    id: frameRect

                    x: 18
                    y: 5
                    width: parent.width - 36
                    height: parent.height - 10
                    radius: 4
                    color: cell.frame

                    Rectangle {
                        id: thumbnail

                        x: 3
                        y: 3
                        width: parent.width - 6
                        height: width * 9 / 16
                        // (A mask is drawn on grey: see the canvas's pageColor.)
                        color: canvas.pageColor

                        Image {
                            anchors.fill: parent
                            source: screen.backdropFor(cell.slide.id)
                            visible: source != ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        // As it will be shown, its visibility rules applied
                        Slide {
                            anchors.fill: parent
                            slide: cell.slide
                            effects: false
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: thumbnail.bottom
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: !cell.renaming
                        color: cell.lightFrame ? "black" : "white"
                        font.pixelSize: 11
                        text: (cell.index + 1)
                            + (cell.slide.groupStart && cell.slide.group !== "" ? "  " + cell.slide.group : "")
                            + (cell.slide.label !== "" && cell.slide.label !== cell.slide.group ? "  " + cell.slide.label : "")
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        screen.showRow(cell.index)
                        screen.takeFocus()
                        if (mouse.button === Qt.RightButton) {
                            const at = mapToItem(slideList, mouse.x, mouse.y)
                            screen.showRowMenu(cell.slide, at.x, at.y)
                        } else if (mouse.y > frameRect.y + thumbnail.y + thumbnail.height) {
                            // A click on the name under the picture is to rename it
                            // (for a slide of a presentation, its label).
                            screen.renamingRow = cell.slide.id
                        }
                    }
                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton && screen.kind !== "presentation")
                            screen.renamingRow = cell.slide.id
                    }
                }

                // Renaming it where its name is: a prop's, a layout's, a theme slide's
                // or a mask's name, or the label of a presentation's slide
                AppTextField {
                    x: frameRect.x + 3
                    y: frameRect.y + frameRect.height - 23
                    width: frameRect.width - 6
                    height: 22
                    leftPadding: 5
                    rightPadding: 5
                    font.pixelSize: 12
                    visible: cell.renaming
                    onVisibleChanged: {
                        if (visible) {
                            text = cell.slide.label
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    Component.onCompleted: {
                        if (visible) {
                            text = cell.slide.label
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    // One call and nothing after it: the change rebuilds the list,
                    // and this row with it.
                    onEditingFinished: {
                        if (cell.renaming)
                            screen.renameRow(cell.slide.id, text.trim(), true)
                    }
                    Keys.onEscapePressed: screen.renameRow(cell.slide.id, "", false)
                }
            }
        }

        Text {
            anchors.centerIn: slideList
            width: slideList.width - 40
            visible: screen.kind !== "presentation" && editor.count === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 13
            text: screen.kind === "props" ? "No props. Add one with the + above."
                : screen.kind === "masks" ? "No masks. Add one with the + above."
                                          : "No stage layouts. Add one with the + above."
        }

        // The elements of the slide, the front one first
        Rectangle {
            id: elementsHeader

            anchors.left: parent.left
            anchors.right: parent.right
            y: Math.round(parent.height * 0.56)
            height: 34
            color: "#4f5665"

            Text {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: 12
                font.bold: true
                font.capitalization: Font.AllUppercase
                text: "Elements"
            }
        }

        ListView {
            id: elementList

            objectName: "elementList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: elementsHeader.bottom
            anchors.bottom: parent.bottom
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // The last element is drawn in front, so it comes first here.
            model: canvas.elements.slice().reverse()

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Rectangle {
                id: entry

                required property var modelData
                readonly property bool selected: modelData.id === canvas.selectedId
                readonly property bool renaming: screen.renamingId !== "" && screen.renamingId === modelData.id

                width: ListView.view.width
                height: 32
                color: selected ? "#22ffffff" : entryMouse.containsMouse ? "#10ffffff" : "transparent"

                Rectangle {
                    width: parent.width
                    height: 1
                    visible: entry.selected
                    color: "#55ffffff"
                }

                Rectangle {
                    y: parent.height - 1
                    width: parent.width
                    height: 1
                    visible: entry.selected
                    color: "#55ffffff"
                }

                MouseArea {
                    id: entryMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // A row can be dragged up or down the list. The list would otherwise
                    // take the drag for itself, to scroll by.
                    preventStealing: true
                    // Picking can rebuild the list (it finishes any text being typed),
                    // so the list, not this row, is what a menu is opened on; and
                    // nothing of this row is counted on after it.
                    onPressed: (mouse) => {
                        const at = mapToItem(elementList, mouse.x, mouse.y)
                        screen.listPressed(entry.modelData.id, mouse.button === Qt.RightButton, at.x, at.y)
                    }
                    onPositionChanged: (mouse) => {
                        if (pressedButtons & Qt.LeftButton)
                            screen.listDragged(entry.modelData.id, mapToItem(elementList, mouse.x, mouse.y))
                    }
                    onReleased: screen.dropCarried()
                    onCanceled: screen.letGo()
                    onDoubleClicked: (mouse) => {
                        if (mouse.button === Qt.LeftButton)
                            screen.renamingId = entry.modelData.id
                    }
                }

                IconButton {
                    id: eye

                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    kind: "eye"
                    flat: true
                    on: !entry.modelData.hidden
                    onClicked: {
                        canvas.pick(entry.modelData.id)
                        canvas.setProperties({ hidden: !entry.modelData.hidden }, false)
                    }
                }

                Text {
                    anchors.left: eye.right
                    anchors.leftMargin: 6
                    anchors.right: marks.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !entry.renaming
                    elide: Text.ElideRight
                    color: entry.modelData.hidden ? "#7d8088" : "#e6e6e6"
                    font.pixelSize: 13
                    // An element with no name goes by its words, as in ProPresenter
                    text: entry.modelData.name !== "" ? entry.modelData.name
                        : entry.modelData.words !== "" ? entry.modelData.words
                        : entry.modelData.fillKind === "media" && entry.modelData.fillMediaName !== "" ? entry.modelData.fillMediaName
                        : "(unnamed)"
                }

                // What sets it apart: its text comes from elsewhere, or it has rules for
                // when it shows.
                Text {
                    id: marks

                    anchors.right: lock.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !entry.renaming
                    color: "#9a9da3"
                    font.pixelSize: 11
                    text: (entry.modelData.linkKind !== "none" ? "linked" : "")
                        + (entry.modelData.linkKind !== "none" && entry.modelData.visibilityRules ? " · " : "")
                        + (entry.modelData.visibilityRules ? "rules" : "")
                }

                AppTextField {
                    anchors.left: eye.right
                    anchors.leftMargin: 2
                    anchors.right: lock.left
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 26
                    font.pixelSize: 13
                    visible: entry.renaming
                    onVisibleChanged: {
                        if (visible) {
                            text = entry.modelData.name
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    Component.onCompleted: {
                        if (visible) {
                            text = entry.modelData.name
                            forceActiveFocus()
                            selectAll()
                        }
                    }
                    // One call and nothing after it: the change rebuilds the list,
                    // and this row with it.
                    onEditingFinished: {
                        if (entry.renaming)
                            screen.rename(entry.modelData.id, text.trim())
                    }
                    Keys.onEscapePressed: {
                        screen.renamingId = ""
                        screen.takeFocus()
                    }
                }

                IconButton {
                    id: lock

                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    kind: "lock"
                    flat: true
                    on: entry.modelData.locked
                    onClicked: {
                        canvas.pick(entry.modelData.id)
                        canvas.setProperties({ locked: !entry.modelData.locked }, false)
                    }
                }
            }
        }

        // What is shown while a row of the list is being dragged, and what goes on then.
        // It is made when a row is dragged and is gone when the row is let go: nothing
        // of it is there while the editor is only being worked in. It is laid over the
        // list and is no part of the rows, which are made again whenever the elements
        // change.
        Loader {
            anchors.fill: parent
            z: 50
            active: screen.carriedId !== ""
            // (It fills the pane, and goes by its own size for the pane's: in here the
            // pane's name, `left`, is an item's own left edge.)
            sourceComponent: Item {
                id: held

                // Over the list, and cut off at its edges as the rows are
                Item {
                    x: elementList.x
                    y: elementList.y
                    width: elementList.width
                    height: elementList.height
                    clip: true

                    // The row being dragged is fainter where it was, until it is let
                    // go: the pane's own colour, most of the way, over it.
                    Rectangle {
                        objectName: "elementHeld"
                        y: screen.carriedRow * screen.elementRowHeight - (elementList.contentY - elementList.originY)
                        width: parent.width
                        height: screen.elementRowHeight
                        color: "#2b2d31"
                        opacity: 0.6
                    }

                    // Where it would land: a line in the gap between two rows, over
                    // the first or under the last
                    Rectangle {
                        objectName: "elementLanding"
                        x: 4
                        y: Math.max(0, Math.min(parent.height - height,
                               screen.landing * screen.elementRowHeight - (elementList.contentY - elementList.originY) - 1))
                        width: parent.width - 8
                        height: 2
                        radius: 1
                        visible: screen.landing >= 0
                        color: "#ff8a1f"
                    }
                }

                // The name of the element being dragged, which goes with the pointer, as
                // what is dragged out of the other lists has (RowDrag)
                Rectangle {
                    objectName: "elementCarried"
                    x: Math.max(4, Math.min(held.width - width - 4, elementList.x + screen.carriedAt.x + 12))
                    y: elementList.y + screen.carriedAt.y + 8
                    width: Math.min(held.width - 8, carriedName.implicitWidth + 20)
                    height: 28
                    radius: 6
                    color: "#2b2d31"
                    border.width: 1
                    border.color: "#ff8a1f"
                    opacity: 0.95

                    Text {
                        id: carriedName

                        readonly property var element: canvas.elements.find(e => e.id === screen.carriedId) ?? null

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 10
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: !element ? "" : element.name !== "" ? element.name : element.words !== "" ? element.words
                            : element.fillKind === "media" && element.fillMediaName !== "" ? element.fillMediaName : "(unnamed)"
                    }
                }

                // Scrolls the list under a row held near its top or its bottom, at the
                // speed carriedScroll says, by the time that has really passed; and
                // where the row would land, the pointer not having moved, is worked out
                // again each time. Nothing runs while no row is held there.
                Timer {
                    property real last: 0

                    interval: 16
                    repeat: true
                    running: screen.carriedScroll !== 0
                    onRunningChanged: last = Date.now()
                    onTriggered: {
                        const now = Date.now()
                        const furthest = Math.max(0, elementList.contentHeight - elementList.height)
                        elementList.contentY = Math.max(0, Math.min(furthest, elementList.contentY + screen.carriedScroll * Math.min(100, now - last) / 1000))
                        last = now
                        screen.carry(screen.carriedId, screen.carriedAt)
                    }
                }

                // A change to the slide's elements while one is held (Delete, or Undo,
                // from the keyboard) makes the list again, and the row that was held
                // with it: so it is let go of, and nothing is moved.
                Connections {
                    target: canvas

                    function onElementsChanged() {
                        screen.letGo()
                    }
                }
            }
        }

        Text {
            anchors.centerIn: elementList
            width: elementList.width - 40
            visible: canvas.slide !== null && canvas.elements.length === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#9a9da3"
            font.pixelSize: 13
            text: "This " + screen.rowWord.toLowerCase() + " has nothing on it. Add something to it with the buttons over the slide."
        }
    }

    // The picked element's properties
    // A picture or a video: to be an element of its own, or what the picked one is
    // filled with. The file is referred to where it is, not copied.
    FileDialog {
        id: mediaDialog

        property string purpose: "add"

        title: purpose === "add" ? "Add Media" : "Fill With Media"
        currentFolder: screen.mediaFolder !== "" ? "file://" + screen.mediaFolder : ""
        nameFilters: [screen.mediaFilter, "All files (*)"]
        onAccepted: {
            if (purpose === "add")
                canvas.addMedia(selectedFile)
            else
                canvas.setProperties({ fillMediaPath: decodeURIComponent(String(selectedFile).replace(/^file:\/\//, "")), fillOn: true }, false)
            screen.takeFocus()
        }
        onRejected: screen.takeFocus()
    }

    EditorInspector {
        id: inspector

        visible: screen.mode === "slides"

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 308
        element: canvas.selected
        elements: canvas.elements
        format: canvas.format
        selection: canvas.selection
        families: screen.families
        setProperties: (changes, interim) => canvas.setProperties(changes, interim)
        showMenu: (items, item, x, y) => screen.showMenu(items, item, x, y)
        chooseMedia: () => screen.chooseMedia("fill")
        setFormat: (format, interim) => canvas.setFormat(format, interim)
        settle: () => canvas.settle()
        colorTriggerDefault: screen.colorTriggerDefault
        saveColorTriggerDefault: (list) => screen.colorTriggerDefaultKept(list)
        onFinished: screen.takeFocus()
    }

    // What can be done to the slide, and the way back
    Rectangle {
        id: strip

        anchors.left: left.right
        anchors.right: screen.mode === "slides" ? inspector.left : parent.right
        anchors.top: parent.top
        height: 46
        color: "#23252b"

        Row {
            id: actions

            x: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            // What can be added: a text box, a shape from a short list of them, and
            // a picture or a video. Underneath they are all the one kind of element:
            // any of them can be given words by double-clicking it, and any fill.
            // (These, and the two after them, are for the slides and are not there
            // over the chord editors, which have the song's keys in their place.)
            EditorTool {
                objectName: "addTextTool"
                kind: "text"
                hint: "Add a text box"
                visible: screen.mode === "slides"
                available: canvas.slide !== null
                onClicked: canvas.addText()
            }

            EditorTool {
                id: shapesTool

                objectName: "addShapeTool"
                kind: "shapes"
                hint: "Add a shape: a rectangle, a rounded rectangle, an ellipse or an arrow"
                visible: screen.mode === "slides"
                available: canvas.slide !== null
                onClicked: screen.showMenu([
                    { header: "Shapes" },
                    { label: "Rectangle", run: () => canvas.addShape("rectangle") },
                    { label: "Rounded Rectangle", run: () => canvas.addShape("roundedRectangle") },
                    { label: "Ellipse", run: () => canvas.addShape("ellipse") },
                    { label: "Arrow", run: () => canvas.addShape("arrow") }
                ], shapesTool, 0, shapesTool.height + 4)
            }

            EditorTool {
                objectName: "addMediaTool"
                kind: "media"
                hint: "Add a picture or a video from a file"
                visible: screen.mode === "slides"
                available: canvas.slide !== null
                onClicked: screen.chooseMedia("add")
            }

            Item {
                visible: screen.mode === "slides"
                width: 10
                height: 1
            }

            EditorTool {
                kind: "duplicate"
                hint: "Duplicate the picked element (Ctrl+D)"
                visible: screen.mode === "slides"
                available: canvas.selected !== null
                onClicked: canvas.duplicate()
            }

            EditorTool {
                kind: "delete"
                hint: "Delete the picked element (Delete)"
                visible: screen.mode === "slides"
                available: canvas.selected !== null
                onClicked: canvas.removeSelected()
            }

            Item {
                visible: screen.mode === "slides"
                width: 10
                height: 1
            }

            EditorTool {
                kind: "undo"
                hint: "Undo (Ctrl+Z)"
                available: editor.canUndo || canvas.typed
                onClicked: {
                    // (In the reflow editor what is being typed is saved first, and is
                    // then the last thing done.)
                    if (screen.mode === "reflow")
                        screen.finish()
                    if (screen.mode === "slides")
                        canvas.undo()
                    else
                        screen.report(editor.undo())
                    screen.takeFocus()
                }
            }

            EditorTool {
                kind: "redo"
                hint: "Redo (Ctrl+Shift+Z)"
                available: editor.canRedo
                onClicked: {
                    if (screen.mode === "slides")
                        canvas.redo()
                    else
                        screen.report(editor.redo())
                    screen.takeFocus()
                }
            }

            // The reflow editor's one button: splits the slide where the caret is,
            // as Ctrl+Enter does. (The name is ProPresenter's.)
            AppButton {
                id: slideBreak

                objectName: "reflowBreak"
                readonly property string hint: "Split the slide in two where the caret is (Ctrl+Enter)"

                anchors.verticalCenter: parent.verticalCenter
                visible: screen.mode === "reflow"
                height: 28
                font.pixelSize: 12
                text: "Insert Slide Break"
                onClicked: {
                    if (chordView.item)
                        chordView.item.splitHere()
                }
            }
        }

        // The song's two keys, where the chord editors are: the key its chords are
        // written in and, quieter beside it, the key they are shown in. Neither changes
        // any chord in the file. They are here, over both chord editors, and not in
        // the chord sheet's own strip, where the first of them was: the ChordPro text
        // had no way to say what key a song is in.
        // (Like the chord editors themselves they are made when one of those is turned
        // to, and are gone when it is left: over the slides they cost nothing.)
        Loader {
            id: songKeys

            anchors.left: actions.right
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            active: (screen.mode === "chords" || screen.mode === "chordpro") && screen.chordable
            visible: active

            sourceComponent: Row {
                spacing: 8

                // (In a narrow window the two words go, and the pickers say what they
                // are on the line along the bottom when the pointer is on them.)
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: strip.width >= 760
                    color: "#9a9da3"
                    font.pixelSize: 13
                    text: "Key"
                }

                AppComboBox {
                    objectName: "chordKeyBox"
                    readonly property string hint: editor.key === "" ? "This song does not say what key it is in. Pick the key its chords are written in."
                                                                     : "The key the song's chords are written in. Changing it changes no chord."
                    anchors.verticalCenter: parent.verticalCenter
                    width: 82
                    height: 28
                    font.pixelSize: 13
                    // With ♯ and ♭, as the chord sheet draws a chord, while that is on
                    // (the Mac and Windows editors' rule 12): only drawn.
                    shownAs: screen.chordSigns ? (key) => Chords.withSigns(key) : null
                    // Every usual key, with the song's own before them if it is not one of
                    // them; and for a song that names none, "None".
                    model: editor.key === "" ? ["None"].concat(screen.usualKeys)
                         : screen.usualKeys.includes(editor.key) ? screen.usualKeys : [editor.key].concat(screen.usualKeys)
                    currentIndex: editor.key === "" ? 0 : model.indexOf(editor.key)
                    onActivated: (index) => screen.pickKey(model[index], false)
                }

                Item {
                    width: 4
                    height: 1
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: strip.width >= 760
                    color: "#7d8088"
                    font.pixelSize: 13
                    text: "Shown in"
                }

                AppComboBox {
                    objectName: "chordShownKeyBox"
                    readonly property string hint: editor.key === "" ? "Set the song's key first, and then the key it is to be shown in."
                        : "The key ProPresenter shows this song's chords in, and this app's stage until another is picked there. They are moved to it as they are drawn; the chords here stay as they are written."
                    anchors.verticalCenter: parent.verticalCenter
                    width: 82
                    height: 28
                    font.pixelSize: 13
                    shownAs: screen.chordSigns ? (key) => Chords.withSigns(key) : null
                    enabled: editor.key !== ""
                    opacity: enabled ? 1 : 0.45
                    model: editor.shownKey === "" ? ["None"].concat(screen.usualKeys)
                         : screen.usualKeys.includes(editor.shownKey) ? screen.usualKeys : [editor.shownKey].concat(screen.usualKeys)
                    currentIndex: editor.shownKey === "" ? 0 : model.indexOf(editor.shownKey)
                    onActivated: (index) => screen.pickKey(model[index], true)
                }
            }
        }

        // The three ways of working on a presentation (see `mode`), in the middle of
        // the toolbar. The middle is that of the toolbar as it is over the chord
        // editors, which is the wider (there is no inspector beside them), so that the
        // switch does not move from under the pointer when it is clicked; and where
        // there is not room for it there, it is as near as what is at the toolbar's
        // left lets it be. In a window too narrow for all of it, the switch is drawn
        // without its pictures; and narrower still, it keeps to the toolbar's right end
        // and covers what is at the left, as the three small buttons it was once did.
        EditorModes {
            id: modes

            objectName: "editorModes"
            readonly property real leftEnd: Math.max(actions.x + actions.width, songKeys.visible ? songKeys.x + songKeys.width : 0) + 16

            anchors.verticalCenter: parent.verticalCenter
            x: Math.min(strip.width - width - 12, Math.max(leftEnd, Math.round((screen.width - left.width - width) / 2)))
            compact: strip.width < leftEnd + widest + 12
            visible: screen.chordable && screen.mode !== "reflow"
            mode: screen.mode
            onPicked: (mode) => screen.setMode(mode)
        }

        // What is being edited, and which of it this is: after the switch, where there
        // is room for it there. Over the slides, with the inspector beside them, a
        // window of the usual size has none, and it is said on the line along the
        // bottom instead (see `subjectBelow`).
        Text {
            id: subjectText

            readonly property real from: modes.visible ? modes.x + modes.width + 16 : actions.x + actions.width + 14
            readonly property real room: Math.max(0, parent.width - 14 - from)

            x: from
            width: room
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: modes.visible ? Text.AlignRight : Text.AlignLeft
            // Here if all of it can be read here. (Over the chord editors the line
            // along the bottom has other things at its right, and it is here cut
            // short if it must be.)
            visible: implicitWidth <= room || (saving.visible && room > 70)
            elide: Text.ElideRight
            color: "#9a9da3"
            font.pixelSize: 13
            text: screen.subject + (canvas.slide ? "   ·   " + screen.rowWord + " " + (canvas.row + 1) + " of " + editor.count : "")
        }
    }

    EditorCanvas {
        id: canvas

        anchors.left: left.right
        anchors.right: inspector.left
        anchors.top: strip.bottom
        anchors.bottom: notes.active ? notes.top : hint.top
        visible: screen.mode === "slides"
        clip: true
        editor: editor
        bridge: bridge
        backdrop: slide ? screen.backdropFor(slide.id) : ""
        // A mask is drawn on grey, not black: what it is made of is mostly black, and
        // where it has nothing the output shows through.
        pageColor: screen.kind === "masks" ? "#6b6e75" : "black"
        onFailed: (error) => screen.report(error)
        onTold: (message) => screen.tell(message)
        onMenuRequested: (x, y) => screen.showElementMenu(canvas, x, y)
        onSlideStepRequested: (delta) => screen.showRow(canvas.row + delta)
    }

    // The slide's notes, in a pane under the slide: only for a presentation's slides,
    // and only made while it is up (the button that puts it up and away is at the right
    // of the line under it). Its top edge is dragged to share out the height; the slide
    // is always left the more of it.
    Loader {
        id: notes

        anchors.left: left.right
        anchors.right: inspector.left
        anchors.bottom: hint.top
        height: Math.max(60, Math.min(screen.notesHeight, (hint.y - strip.y - strip.height) * 0.6))
        active: screen.notesUp
        visible: active

        sourceComponent: SlideNotes {
            objectName: "slideNotes"
            editor: screen.editor
            row: canvas.row
            families: screen.families
            onFailed: (error) => screen.report(error)
            onResized: (height) => screen.notesHeightEdited(Math.max(60, Math.min(height, (hint.y - strip.y - strip.height) * 0.6)))
            onFinished: canvas.takeFocus()
            onUndoRequested: canvas.undo()
            onRedoRequested: canvas.redo()
        }
    }

    // Whatever there is to tell the user, and otherwise how to work the canvas, for
    // whatever is going on
    Rectangle {
        id: hint

        anchors.left: left.right
        anchors.right: screen.mode === "slides" ? inspector.left : parent.right
        anchors.bottom: parent.bottom
        height: Math.max(26, hintText.implicitHeight + 10)
        color: screen.notice === "" ? "#191a1d" : screen.noticeIsError ? "#4a1f1f" : "#23324a"

        Text {
            id: hintText

            objectName: "editorHintText"
            anchors.left: parent.left
            anchors.right: saving.visible ? saving.left : subjectBelow.visible ? subjectBelow.left : notesToggle.visible ? notesToggle.left : parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: screen.notice === "" ? Text.NoWrap : Text.Wrap
            elide: screen.notice === "" ? Text.ElideRight : Text.ElideNone
            color: screen.notice === "" ? "#7d8088" : "#f0f0f0"
            font.pixelSize: 12
            text: screen.notice !== "" ? screen.notice
                : screen.toolHint !== "" ? screen.toolHint
                : screen.carriedId !== "" ? "Let go between two rows to put it there: the top of the list is the front  ·  Let go off to one side of the list to leave it where it was"
                // (Over the chord sheet, what the keys do is said on the line of hints,
                // over this one, where the spot is: see HintLine.qml.)
                : screen.mode === "chords" ? (chordView.item && chordView.item.howMuch ? chordView.item.howMuch : "")
                : screen.mode === "chordpro" ? "[ opens a chord and closes it  ·  Backspace on a bracket removes the chord  ·  The words cannot be changed here"
                : screen.mode === "reflow" ? (chordView.item ? chordView.item.hint : "")
                : notes.item && notes.item.typing ? "Notes: select text to format part of it  ·  Ctrl+B, I, U  ·  Right click: font, size and colour  ·  Esc gives the keys back to the slide"
                : canvas.editing ? "Select text to format part of it  ·  Esc or a click elsewhere finishes  ·  Ctrl+B, I, U"
                : canvas.selected ? "Drag to move, handles to resize  ·  Shift: straight, or in proportion  ·  Ctrl: no snapping  ·  Arrows nudge  ·  Double-click or Enter edits the text"
                : "Click an element to pick it  ·  Double-click text to edit it  ·  Arrows change slide"
        }

        // What is being edited, where the toolbar had no room to say it
        Text {
            id: subjectBelow

            objectName: "editorSubjectBelow"
            anchors.right: notesToggle.visible ? notesToggle.left : parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: !subjectText.visible && !saving.visible && screen.notice === ""
            color: "#7d8088"
            font.pixelSize: 12
            text: subjectText.text
        }

        // Puts the pane for the slide's notes up, and away: at the bottom right, under
        // where the pane is. Only where there are notes to be had: over a
        // presentation's slides. It is filled in while the pane is up.
        Rectangle {
            id: notesToggle

            objectName: "notesToggle"
            readonly property bool hovered: notesMouse.containsMouse
            readonly property string hint: screen.notesShown ? "Put the slide's notes away" : "Show the slide's notes, under the slide"
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: notesWord.implicitWidth + 30
            height: 20
            radius: 5
            visible: screen.notable
            color: screen.notesShown ? "#1e88e5" : notesMouse.pressed ? "#50535a" : notesMouse.containsMouse ? "#45484e" : "#34363c"

            // A page with lines on it
            Item {
                x: 7
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 12

                Rectangle {
                    anchors.fill: parent
                    radius: 1.5
                    color: "transparent"
                    border.width: 1.2
                    border.color: screen.notesShown ? "white" : "#c9cdd6"
                }

                Repeater {
                    model: 3

                    Rectangle {
                        required property int index

                        x: 2.5
                        y: 3 + index * 2.6
                        width: index === 2 ? 3 : 5
                        height: 1
                        color: screen.notesShown ? "white" : "#c9cdd6"
                    }
                }
            }

            Text {
                id: notesWord

                x: 22
                anchors.verticalCenter: parent.verticalCenter
                color: screen.notesShown ? "white" : "#c9cdd6"
                font.pixelSize: 12
                text: "Notes"
            }

            MouseArea {
                id: notesMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    // (What was being typed in the pane goes into the file as it goes away.)
                    if (notes.item)
                        notes.item.send()
                    screen.notesShownEdited(!screen.notesShown)
                    canvas.takeFocus()
                }
            }
        }

        // Nothing is saved by hand here, and over the chord editors the line says so.
        // Revert makes the song what it was when it was opened, after asking; and where
        // a Save button would be are the words that say why there is none, for the eye
        // that looks for one. After a revert, for as long as the line says it was made,
        // it can be taken back. (Only over the chord editors, which are this app's own:
        // the slides are edited as ProPresenter edits them, which has neither.)
        Loader {
            id: saving

            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            active: screen.mode !== "slides" && screen.chordable
            visible: active

            sourceComponent: Row {
                spacing: 10

                AppButton {
                    objectName: "revertUndoButton"
                    readonly property string hint: "Put the song back as it was just before it was reverted"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: editor.canTakeBackRevert && screen.notice === screen.revertedNotice
                    height: 20
                    leftPadding: 9
                    rightPadding: 9
                    font.pixelSize: 12
                    text: "Undo"
                    onClicked: screen.takeBackRevert()
                }

                AppButton {
                    id: revertButton

                    objectName: "revertButton"
                    readonly property string hint: "Take back every change made to this song since it was opened here, after asking. What the song is now is kept as a copy first."
                    anchors.verticalCenter: parent.verticalCenter
                    height: 20
                    leftPadding: 9
                    rightPadding: 9
                    font.pixelSize: 12
                    enabled: editor.canRevert
                    text: "Revert…"
                    onClicked: screen.askToRevert(revertButton)
                }

                Text {
                    objectName: "autosaveNote"
                    readonly property bool hovered: autosaveMouse.containsMouse
                    readonly property string hint: "There is nothing to save by hand: every change is saved as it is made, with a copy of the song as it was kept first."
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#8d9097"
                    font.pixelSize: 12
                    text: "Autosave on"

                    MouseArea {
                        id: autosaveMouse

                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }
        }
    }

    // The two chord editors, each made when it is turned to and gone when it is left,
    // so that an editor nobody has asked for costs nothing.
    Loader {
        id: chordView

        anchors.left: left.right
        anchors.right: parent.right
        anchors.top: strip.bottom
        anchors.bottom: hint.top
        active: screen.mode !== "slides" && screen.chordable
        sourceComponent: screen.mode === "chords" ? chordSheet : screen.mode === "chordpro" ? chordPro : reflowView
        onLoaded: item.takeKeys()
    }

    Component {
        id: reflowView

        ReflowEditor {
            objectName: "reflowEditor"
            editor: screen.editor
            groupColor: screen.groupColor
            backdropFor: screen.backdropFor
            row: canvas.row
            thumbnail: screen.reflowThumbnail
            onFailed: (error) => screen.report(error)
            onRowPicked: (row) => screen.showRow(row)
            onThumbnailEdited: (width) => screen.reflowThumbnailEdited(width)
        }
    }

    Component {
        id: chordSheet

        ChordSheet {
            objectName: "chordSheet"
            editor: screen.editor
            groupColor: screen.groupColor
            row: canvas.row
            zoom: screen.sheetZoom
            chordColor: screen.sheetChordColor !== "" ? screen.sheetChordColor : "#ff8a1f"
            lineColor: screen.sheetLineColor !== "" ? screen.sheetLineColor : "#ff8a1f"
            signs: screen.chordSigns
            onFailed: (error) => screen.report(error)
            onTold: (message) => screen.tell(message)
            onRowPicked: (row) => screen.showRow(row)
            onZoomEdited: (zoom) => screen.sheetZoomEdited(zoom)
        }
    }

    Component {
        id: chordPro

        ChordProEditor {
            objectName: "chordProEditor"
            editor: screen.editor
            row: canvas.row
            zoom: screen.textZoom
            chordColor: screen.textChordColor !== "" ? screen.textChordColor : "#ffb45e"
            onFailed: (error) => screen.report(error)
            onRowPicked: (row) => screen.showRow(row)
            onZoomEdited: (zoom) => screen.textZoomEdited(zoom)
        }
    }
}
