import QtQuick
import SimplePresenterApp

// The slide being edited, drawn as large as fits, with its elements there to be picked,
// dragged about and resized, and their text edited where it stands.
//
// A click picks the element under the pointer (the one in front, if several are); a drag
// moves it, and the handles round a picked element resize it. Both snap to the slide's
// edges and middle and to the other elements, showing the line snapped to; holding Ctrl
// turns that off, and Shift keeps a move to one direction or a corner's resize in
// proportion. A double click edits an element's text in place.
//
// Text is edited by a TextEdit laid exactly over the element, which draws nothing but
// the caret and the selection: the text itself goes on being drawn as it will be shown,
// from what the TextEdit holds, so strokes, shadows and capitals are all there as it is
// typed. That works because both lay text out the same way (see textlayout.h).
//
// An element whose text is linked to something (another element, a timer, the slide
// that is live) has a yellow outline, and in small yellow print at its foot what it is
// linked to, as ProPresenter marks them. Where there is nothing of that to show (no
// slide is live; or it is something this app does not follow, such as the clock) the
// element is drawn here with its own text, which is then a sample of it to set the
// look by. Only here: shown for real, such a box has nothing in it.
Item {
    id: canvas

    required property PresentationEditor editor
    required property RichTextBridge bridge
    // The slide's row in the editor, changed with showRow(), and the slide itself: a
    // map, as proconvert describes
    property int row: -1
    property var slide: null
    // An image to show behind the slide, such as the media its cue triggers; "" for none
    property string backdrop
    property string selectedId
    // The element whose text is being edited in place, "" if none, and that text as it
    // is now, typed or not
    property string editingId
    property var liveText: undefined
    // Whether what has been typed has yet to reach the editor
    property bool typed: false
    // While an element is being dragged or resized: where it would land, in slide units
    property var dragBox: null
    // The lines snapped to: [{ vertical, at }], in slide units
    property var guides: []

    readonly property var elements: slide ? slide.elements : []
    readonly property var selected: elements.find(e => e.id === selectedId) ?? null
    readonly property var editing: editingId === "" ? null : (elements.find(e => e.id === editingId) ?? null)
    // The format at the caret or of the selection while text is edited, else that of the
    // start of the picked element's text: a map as RichText::formatAt gives, or null.
    readonly property var format: {
        if (!selected)
            return null
        const found = editing ? bridge.formatAt(liveText, textEdit.selectionStart, textEdit.selectionEnd)
                              : bridge.formatAt(selected.text, 0, 0)
        return found.family === undefined ? null : found
    }
    // Whether formatting would go to a selection, rather than to all of the text
    readonly property bool selection: editing !== null && textEdit.selectionStart !== textEdit.selectionEnd
    readonly property real slideWidth: slide ? slide.width : 1920
    readonly property real slideHeight: slide ? slide.height : 1080
    // Canvas pixels per slide unit, and where the slide's corner is
    readonly property real u: Math.max(0.01, Math.min((width - 2 * margin) / slideWidth, (height - 2 * margin) / slideHeight))
    readonly property real margin: 36
    readonly property real originX: Math.round((width - slideWidth * u) / 2)
    readonly property real originY: Math.round((height - slideHeight * u) / 2)
    readonly property real smallest: 20
    // How close, in canvas pixels, an edge has to come to a line to snap to it
    readonly property real snapDistance: 7
    readonly property int everything: 1073741824

    // Something went wrong with a change
    signal failed(string error)
    // Something the user should know, that is not a failure
    signal told(string message)
    // A right click, on an element (which is then the picked one) or on none
    signal menuRequested(real x, real y)
    // A key asked for the slide before (-1) or after (1) this one
    signal slideStepRequested(int delta)

    function report(error) {
        if (error !== "")
            failed(error)
        return error === ""
    }

    // What an element's text is linked to, in a few words; "" for one whose text is
    // its own. A timer goes by the name it has now, which may not be the name the link
    // was made with.
    function linkCaption(element) {
        switch (element.linkKind) {
        case "element":
            return "Text of " + (element.linkElementName !== "" ? "“" + element.linkElementName + "”" : "another element")
        case "timer": {
            const id = Timers.linkedTimer(element.linkTimerId, element.linkTimerName)
            const timer = Timers.timers.find(candidate => candidate.id === id)
            return "Timer: " + (timer ? timer.name : element.linkTimerName + " (not here)")
        }
        case "slideText":
            return (element.linkSlideNext ? "Next Slide: " : "Current Slide: ")
                 + (element.linkSlideSource === Show.Notes ? "Notes"
                    : element.linkSlideSource === Show.ElementNamed ? "“" + element.linkSlideName + "”" : "Text")
        case "other":
            return element.linkLabel
        default:
            return ""
        }
    }

    function reload() {
        slide = row >= 0 && row < editor.count ? editor.slideAt(row) : null
    }

    // Shows another slide. Whatever was under way on this one is finished first, while
    // it is still the one being shown.
    function showRow(next) {
        finishText()
        settle()
        selectedId = ""
        dragBox = null
        guides = []
        row = next
        reload()
    }

    // Adds a text box, set like the picked element's text if there is one, and starts
    // editing its text, all selected, so that typing replaces it.
    function addText() {
        finishText()
        const made = editor.addText(row, selectedId)
        if (report(made.error))
            editText(made.id)
    }

    // Adds a shape, or an element filled with a media file, and picks it.
    function addShape(shape) {
        finishText()
        const made = editor.addShape(row, shape)
        if (report(made.error))
            selectedId = made.id
    }

    function addMedia(file) {
        finishText()
        const made = editor.addMedia(row, file)
        if (report(made.error))
            selectedId = made.id
    }

    function duplicate() {
        if (!selected)
            return
        finishText()
        const made = editor.duplicate(row, selectedId)
        if (report(made.error))
            selectedId = made.id
    }

    function removeSelected() {
        if (!selected)
            return
        const id = selectedId
        // Whatever was being typed into it goes with it.
        typed = false
        finishText()
        selectedId = ""
        report(editor.remove(row, id))
    }

    // Moves the picked element in the order the elements are drawn in: by `steps`
    // places towards the front, or the back if negative.
    function reorder(steps) {
        const index = elements.findIndex(e => e.id === selectedId)
        if (index < 0)
            return
        finishText()
        report(editor.move(row, selectedId, Math.max(0, Math.min(elements.length - 1, index + steps))))
    }

    // Picks the element after the picked one, or before it, going round.
    function pickNext(delta) {
        if (elements.length === 0)
            return
        const index = elements.findIndex(e => e.id === selectedId)
        const next = index < 0 ? (delta > 0 ? 0 : elements.length - 1) : (index + delta + elements.length) % elements.length
        pick(elements[next].id)
    }

    function pick(id) {
        if (id !== editingId)
            finishText()
        selectedId = id
    }

    // The element in front at a point of the canvas, among those that can be picked
    // there: not the hidden or the locked ones.
    function elementAt(x, y) {
        const sx = (x - originX) / u
        const sy = (y - originY) / u
        for (let i = elements.length - 1; i >= 0; --i) {
            const e = elements[i]
            if (!e.hidden && !e.locked && sx >= e.x && sx <= e.x + e.width && sy >= e.y && sy <= e.y + e.height)
                return e
        }
        return null
    }

    function holds(element, x, y) {
        const sx = (x - originX) / u
        const sy = (y - originY) / u
        return sx >= element.x && sx <= element.x + element.width && sy >= element.y && sy <= element.y + element.height
    }

    // Changes to the picked element. `interim` ones are shown but not saved, until one
    // that is not interim, or settle().
    function setProperties(changes, interim) {
        if (!selected)
            return
        // An element given its text from elsewhere has none of its own to go on typing.
        if (changes.linkKind !== undefined && changes.linkKind !== "none" && editingId === selectedId)
            finishText()
        if (interim)
            editor.previewProperties(row, selectedId, changes)
        else
            report(editor.commitPreview()) && report(editor.setProperties(row, selectedId, changes))
    }

    // Formats the selected text while text is being edited, and otherwise, or with
    // nothing selected, all the text of the picked element.
    function setFormat(format, interim) {
        if (!selected)
            return
        let start = 0
        let end = everything
        const caret = textEdit.cursorPosition
        if (editing) {
            sendText()
            if (textEdit.selectionStart !== textEdit.selectionEnd) {
                start = textEdit.selectionStart
                end = textEdit.selectionEnd
            }
        }
        if (interim)
            editor.previewFormat(row, selectedId, start, end, format)
        else
            report(editor.commitPreview()) && report(editor.formatText(row, selectedId, start, end, format))
        if (editing)
            loadText(end === everything ? caret : start, end === everything ? caret : end)
    }

    function settle() {
        report(editor.commitPreview())
    }

    // Starts editing the text of an element in place, with the caret at a point of the
    // canvas if one is given and otherwise with all of the text selected.
    function editText(id, x, y) {
        const element = elements.find(e => e.id === id)
        if (!element || element.locked || element.hidden)
            return
        if (element.linkKind !== "none") {
            told((element.name !== "" ? "“" + element.name + "”" : "This element") + " shows "
                 + (element.linkKind === "element" ? "the text of “" + element.linkElementName + "”"
                    : element.linkKind === "timer" ? "the timer “" + element.linkTimerName + "”"
                    : "a " + element.linkLabel.toLowerCase() + ", set up in ProPresenter")
                 + ", so it has no text of its own to edit. Its font and colour can still be changed.")
            return
        }
        settle()
        selectedId = id
        editingId = id
        loadText(0, 0)
        if (x === undefined) {
            textEdit.selectAll()
        } else {
            const at = textEdit.mapFromItem(canvas, x, y)
            textEdit.cursorPosition = textEdit.positionAt(at.x, at.y)
        }
        textEdit.forceActiveFocus()
    }

    // Puts the element's text, as the editor has it, into the TextEdit.
    function loadText(start, end) {
        const element = elements.find(e => e.id === editingId)
        if (!element)
            return
        textEdit.loading = true
        bridge.load(textEdit.textDocument, element.text)
        textEdit.loading = false
        liveText = element.text
        typed = false
        const length = textEdit.length
        if (start === end)
            textEdit.cursorPosition = Math.min(start, length)
        else
            textEdit.select(Math.min(start, length), Math.min(end, length))
    }

    // Hands what has been typed to the editor.
    function sendText() {
        if (editingId === "" || !typed)
            return
        typed = false
        report(editor.setText(row, editingId, bridge.save(textEdit.textDocument)))
    }

    function finishText() {
        if (editingId === "")
            return
        // The keyboard goes back to the canvas, if the text had it.
        const typing = textEdit.activeFocus
        sendText()
        editingId = ""
        liveText = undefined
        textEdit.deselect()
        if (typing)
            forceActiveFocus()
    }

    // Moves the picked element by slide units, one arrow key at a time; the moves are
    // saved together a moment after the last.
    function nudge(dx, dy) {
        if (!selected || selected.locked)
            return
        editor.previewProperties(row, selectedId, { x: selected.x + dx, y: selected.y + dy })
        nudgeTimer.restart()
    }

    // The nearest of `targets` to any of `values`, if within reach: how far to move to
    // meet it, and which it is.
    function snap(values, targets, reach) {
        let best = null
        for (const value of values) {
            for (const target of targets) {
                const distance = target - value
                if (Math.abs(distance) <= reach && (best === null || Math.abs(distance) < Math.abs(best.by)))
                    best = { by: distance, at: target }
            }
        }
        return best
    }

    // The lines an element being moved can snap to: the slide's edges and middle, and
    // the edges and middles of the other elements that show.
    function snapLines(vertical) {
        const lines = vertical ? [0, slideWidth / 2, slideWidth] : [0, slideHeight / 2, slideHeight]
        for (const e of elements) {
            if (e.id === selectedId || e.hidden)
                continue
            if (vertical)
                lines.push(e.x, e.x + e.width / 2, e.x + e.width)
            else
                lines.push(e.y, e.y + e.height / 2, e.y + e.height)
        }
        return lines
    }

    // Where a drag has taken the picked element. `from` is its box when the drag began,
    // `dx` and `dy` how far the pointer has gone in slide units, and `hx` and `hy` say
    // what is being dragged: 0, 0.5 or 1 for the left edge, neither or the right edge,
    // and likewise down, or -1 for the whole element.
    function drag(from, dx, dy, hx, hy, modifiers) {
        const snapping = !(modifiers & Qt.ControlModifier)
        const reach = snapDistance / u
        const lines = []
        let left = from.x
        let top = from.y
        let right = from.x + from.width
        let bottom = from.y + from.height

        if (hx < 0) {
            // A move. With Shift, along whichever way the pointer has gone further.
            if (modifiers & Qt.ShiftModifier) {
                if (Math.abs(dx) >= Math.abs(dy))
                    dy = 0
                else
                    dx = 0
            }
            // To whole units, but only in a direction it has moved in.
            left = dx === 0 ? from.x : Math.round(from.x + dx)
            top = dy === 0 ? from.y : Math.round(from.y + dy)
            if (snapping) {
                const across = dx === 0 ? null : snap([left, left + from.width / 2, left + from.width], snapLines(true), reach)
                if (across) {
                    left += across.by
                    lines.push({ vertical: true, at: across.at })
                }
                const down = dy === 0 ? null : snap([top, top + from.height / 2, top + from.height], snapLines(false), reach)
                if (down) {
                    top += down.by
                    lines.push({ vertical: false, at: down.at })
                }
            }
            right = left + from.width
            bottom = top + from.height
        } else {
            const proportional = (modifiers & Qt.ShiftModifier) && hx !== 0.5 && hy !== 0.5
            // Each edge being dragged follows the pointer, and may snap.
            const edge = (start, by, vertical) => {
                if (by === 0)
                    return start
                let value = Math.round(start + by)
                const line = snapping && !proportional ? snap([value], snapLines(vertical), reach) : null
                if (line) {
                    value += line.by
                    lines.push({ vertical: vertical, at: line.at })
                }
                return value
            }
            if (hx === 0)
                left = Math.min(edge(left, dx, true), right - smallest)
            else if (hx === 1)
                right = Math.max(edge(right, dx, true), left + smallest)
            if (hy === 0)
                top = Math.min(edge(top, dy, false), bottom - smallest)
            else if (hy === 1)
                bottom = Math.max(edge(bottom, dy, false), top + smallest)
            if (proportional) {
                // The side that has grown more, in proportion, sets the other.
                const scale = Math.max((right - left) / from.width, (bottom - top) / from.height)
                const width = Math.max(smallest, Math.round(from.width * scale))
                const height = Math.max(smallest, Math.round(from.height * scale))
                if (hx === 0)
                    left = right - width
                else
                    right = left + width
                if (hy === 0)
                    top = bottom - height
                else
                    bottom = top + height
            }
        }
        guides = lines
        dragBox = { x: left, y: top, width: right - left, height: bottom - top }
    }

    // Ends a drag: saves where the element landed, if it went anywhere.
    function drop(from) {
        const box = dragBox
        dragBox = null
        guides = []
        if (box && (box.x !== from.x || box.y !== from.y || box.width !== from.width || box.height !== from.height))
            setProperties(box, false)
    }

    // The keys, while no text is being edited. Arrows move the picked element a unit
    // at a time, ten with Shift, and with nothing picked go from slide to slide.
    Keys.onPressed: (event) => {
        // A key the text being edited had no use for, such as Up on its first line,
        // comes here next. It is not for the element.
        if (editingId !== "") {
            event.accepted = event.key < Qt.Key_F1 || event.key > Qt.Key_F35
            return
        }
        const shift = event.modifiers & Qt.ShiftModifier
        const step = shift ? 10 : 1
        if (event.modifiers & Qt.ControlModifier) {
            switch (event.key) {
            case Qt.Key_Z:
                if (shift)
                    redo()
                else
                    undo()
                break
            case Qt.Key_Y:
                redo()
                break
            case Qt.Key_D:
                duplicate()
                break
            case Qt.Key_B:
                if (format)
                    setFormat({ bold: !format.bold }, false)
                break
            case Qt.Key_I:
                if (format)
                    setFormat({ italic: !format.italic }, false)
                break
            case Qt.Key_U:
                if (format)
                    setFormat({ underline: !format.underline }, false)
                break
            default:
                return
            }
            event.accepted = true
            return
        }
        switch (event.key) {
        case Qt.Key_Left:
            if (selected)
                nudge(-step, 0)
            else
                slideStepRequested(-1)
            break
        case Qt.Key_Right:
            if (selected)
                nudge(step, 0)
            else
                slideStepRequested(1)
            break
        case Qt.Key_Up:
            if (selected)
                nudge(0, -step)
            else
                slideStepRequested(-1)
            break
        case Qt.Key_Down:
            if (selected)
                nudge(0, step)
            else
                slideStepRequested(1)
            break
        case Qt.Key_PageUp:
            slideStepRequested(-1)
            break
        case Qt.Key_PageDown:
            slideStepRequested(1)
            break
        case Qt.Key_Delete:
        case Qt.Key_Backspace:
            removeSelected()
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (selected)
                editText(selectedId)
            break
        case Qt.Key_Escape:
            selectedId = ""
            break
        case Qt.Key_Tab:
            pickNext(1)
            break
        case Qt.Key_Backtab:
            pickNext(-1)
            break
        default:
            return
        }
        event.accepted = true
    }

    Connections {
        target: canvas.editor

        function onSlideChanged(row) {
            if (row === canvas.row)
                canvas.reload()
        }

        function onDocumentChanged() {
            canvas.editingId = ""
            canvas.liveText = undefined
            canvas.selectedId = ""
            canvas.reload()
        }
    }

    Timer {
        id: nudgeTimer

        interval: 400
        onTriggered: canvas.settle()
    }

    Rectangle {
        anchors.fill: parent
        color: "#131417"
    }

    // The slide
    Rectangle {
        id: page

        x: canvas.originX
        y: canvas.originY
        width: Math.round(canvas.slideWidth * canvas.u)
        height: Math.round(canvas.slideHeight * canvas.u)
        color: canvas.slide && canvas.slide.drawsBackground ? canvas.slide.backgroundColor : "black"
        clip: true

        Image {
            anchors.fill: parent
            visible: canvas.backdrop !== ""
            source: canvas.backdrop
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            opacity: 0.85
        }

        // An element that is switched off is not drawn; one that its visibility rules
        // would hide is, since those depend on what the slide holds when it is shown.
        Repeater {
            model: canvas.elements

            delegate: SlideElement {
                required property var modelData
                readonly property bool dragged: canvas.dragBox !== null && modelData.id === canvas.selectedId
                // Linked to something there is nothing of to show just now: its own
                // text stands in, as a sample.
                readonly property bool sampled: modelData.linkKind === "other"
                                                || (modelData.linkKind === "slideText" && liveLinkText === "")

                source: modelData
                unit: canvas.u
                visible: !modelData.hidden
                boxX: dragged ? canvas.dragBox.x : modelData.x
                boxY: dragged ? canvas.dragBox.y : modelData.y
                boxWidth: dragged ? canvas.dragBox.width : modelData.width
                boxHeight: dragged ? canvas.dragBox.height : modelData.height
                textOverride: modelData.id === canvas.editingId ? canvas.liveText : sampled ? modelData.text : undefined
                fitted: modelData.id !== canvas.editingId
                standIns: true
            }
        }
    }

    // The slide's edge, drawn over anything that reaches it
    Rectangle {
        anchors.fill: page
        anchors.margins: -1
        color: "transparent"
        border.width: 1
        border.color: "#4a4d54"
    }

    // Picking and moving
    MouseArea {
        id: pointer

        property point pressedAt
        property var from: null
        property bool moving: false
        // What a click that turns out not to be a drag picks
        property string clicked
        // Whether the pointer is over the picked element, where a drag would move it
        property bool overPicked: false
        // The element a click where the pointer is would pick, by id; "" for none
        property string hovered

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        hoverEnabled: true
        cursorShape: moving || overPicked ? Qt.SizeAllCursor : Qt.ArrowCursor
        onPressed: (mouse) => {
            canvas.forceActiveFocus()
            canvas.finishText()
            canvas.settle()
            const hit = canvas.elementAt(mouse.x, mouse.y)
            pressedAt = Qt.point(mouse.x, mouse.y)
            moving = false
            from = null
            clicked = ""
            if (mouse.button === Qt.RightButton) {
                canvas.selectedId = hit ? hit.id : ""
                canvas.menuRequested(mouse.x, mouse.y)
                return
            }
            // A press on the picked element may be the start of moving it, even where
            // another lies over it; only a click there picks the one in front.
            const picked = canvas.selected
            if (picked && !picked.locked && !picked.hidden && canvas.holds(picked, mouse.x, mouse.y)) {
                clicked = hit ? hit.id : ""
            } else {
                canvas.selectedId = hit ? hit.id : ""
            }
            const target = canvas.selected
            if (target && !target.locked && canvas.holds(target, mouse.x, mouse.y))
                from = { x: target.x, y: target.y, width: target.width, height: target.height }
        }
        onPositionChanged: (mouse) => {
            const picked = canvas.selected
            overPicked = picked !== null && !picked.locked && !picked.hidden && canvas.holds(picked, mouse.x, mouse.y)
            const under = pressed ? null : canvas.elementAt(mouse.x, mouse.y)
            hovered = under ? under.id : ""
            if (!(pressedButtons & Qt.LeftButton) || !from)
                return
            const dx = mouse.x - pressedAt.x
            const dy = mouse.y - pressedAt.y
            if (!moving && Math.abs(dx) + Math.abs(dy) > 4)
                moving = true
            if (moving)
                canvas.drag(from, dx / canvas.u, dy / canvas.u, -1, -1, mouse.modifiers)
        }
        onReleased: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            if (moving)
                canvas.drop(from)
            else if (clicked !== "" && clicked !== canvas.selectedId)
                canvas.selectedId = clicked
            moving = false
            from = null
        }
        onCanceled: {
            canvas.dragBox = null
            canvas.guides = []
            moving = false
            from = null
        }
        onExited: {
            overPicked = false
            hovered = ""
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            const hit = canvas.elementAt(mouse.x, mouse.y)
            if (hit)
                canvas.editText(hit.id, mouse.x, mouse.y)
        }
    }

    // Where each element is, faintly, so that one with nothing in it yet can be found;
    // and clearly for the one a click would pick.
    Repeater {
        model: canvas.elements

        delegate: Rectangle {
            required property var modelData
            readonly property bool hovered: modelData.id === pointer.hovered

            x: canvas.originX + modelData.x * canvas.u
            y: canvas.originY + modelData.y * canvas.u
            width: modelData.width * canvas.u
            height: modelData.height * canvas.u
            visible: !modelData.hidden && modelData.id !== canvas.selectedId
            color: "transparent"
            border.width: 1
            border.color: hovered ? "#c0ffffff" : "#30ffffff"
        }
    }

    // An element whose text is linked to something: a yellow outline, and in small
    // print at its foot what it is linked to. It goes with the element as that is
    // dragged, and stays when the element is picked, inside the frame that shows that.
    Repeater {
        model: canvas.elements

        delegate: Rectangle {
            id: linkMark

            required property var modelData
            readonly property var box: canvas.dragBox !== null && modelData.id === canvas.selectedId ? canvas.dragBox
                                                                                                    : modelData
            readonly property string caption: modelData.linkKind !== "none" ? canvas.linkCaption(modelData) : ""

            objectName: "linkMark"
            x: canvas.originX + box.x * canvas.u
            y: canvas.originY + box.y * canvas.u
            width: box.width * canvas.u
            height: box.height * canvas.u
            visible: !modelData.hidden && modelData.linkKind !== "none"
            color: "transparent"
            border.width: 1.5
            border.color: "#ffd400"

            Text {
                x: 5
                y: parent.height - height - 3
                width: parent.width - 10
                // Not in a box too small to hold it
                visible: parent.height >= 22 && parent.width >= 40
                elide: Text.ElideRight
                color: "#ffd400"
                style: Text.Outline
                styleColor: "#c0000000"
                font.pixelSize: 10
                text: linkMark.caption
            }
        }
    }

    // The lines snapped to
    Repeater {
        model: canvas.guides

        delegate: Rectangle {
            required property var modelData

            x: modelData.vertical ? canvas.originX + modelData.at * canvas.u - 0.5 : canvas.originX - 12
            y: modelData.vertical ? canvas.originY - 12 : canvas.originY + modelData.at * canvas.u - 0.5
            width: modelData.vertical ? 1 : page.width + 24
            height: modelData.vertical ? page.height + 24 : 1
            color: "#3df2ff"
        }
    }

    // The picked element's frame, and the handles that resize it
    Item {
        id: frame

        readonly property var box: canvas.dragBox ?? canvas.selected

        visible: canvas.selected !== null
        x: box ? canvas.originX + box.x * canvas.u : 0
        y: box ? canvas.originY + box.y * canvas.u : 0
        width: box ? box.width * canvas.u : 0
        height: box ? box.height * canvas.u : 0

        // Dark under light, so the frame shows against anything.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1.5
            color: "transparent"
            border.width: 3
            border.color: "#90000000"
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            color: "transparent"
            border.width: canvas.editing ? 1 : 1.5
            border.color: canvas.editing ? "#4da3ff" : "#ff8a1f"
        }

        // Where it is and how big, while it is being dragged
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height + 10
            width: readout.implicitWidth + 16
            height: 22
            radius: 5
            visible: canvas.dragBox !== null
            color: "#e615161a"
            border.width: 1
            border.color: "#5c5f66"

            Text {
                id: readout

                anchors.centerIn: parent
                color: "#e6e6e6"
                font.pixelSize: 12
                text: canvas.dragBox ? Math.round(canvas.dragBox.x) + ", " + Math.round(canvas.dragBox.y) + "   "
                                       + Math.round(canvas.dragBox.width) + " × " + Math.round(canvas.dragBox.height) : ""
            }
        }

        Repeater {
            model: canvas.selected && !canvas.selected.locked && !canvas.editing
                   ? [[0, 0], [0.5, 0], [1, 0], [0, 0.5], [1, 0.5], [0, 1], [0.5, 1], [1, 1]] : []

            delegate: MouseArea {
                id: handle

                required property var modelData
                readonly property real hx: modelData[0]
                readonly property real hy: modelData[1]
                property point pressedAt
                property var from: null

                x: hx * frame.width - width / 2
                y: hy * frame.height - height / 2
                width: 18
                height: 18
                // The middle handles go when the element is too small to hold them apart.
                visible: (hx !== 0.5 || frame.width > 44) && (hy !== 0.5 || frame.height > 44)
                preventStealing: true
                cursorShape: hx === 0.5 ? Qt.SizeVerCursor : hy === 0.5 ? Qt.SizeHorCursor
                           : hx === hy ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                onPressed: (mouse) => {
                    canvas.forceActiveFocus()
                    canvas.settle()
                    const element = canvas.selected
                    pressedAt = mapToItem(canvas, mouse.x, mouse.y)
                    from = { x: element.x, y: element.y, width: element.width, height: element.height }
                }
                onPositionChanged: (mouse) => {
                    if (!from)
                        return
                    const at = mapToItem(canvas, mouse.x, mouse.y)
                    canvas.drag(from, (at.x - pressedAt.x) / canvas.u, (at.y - pressedAt.y) / canvas.u, hx, hy, mouse.modifiers)
                }
                onReleased: {
                    if (from)
                        canvas.drop(from)
                    from = null
                }
                onCanceled: {
                    canvas.dragBox = null
                    canvas.guides = []
                    from = null
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 9
                    height: 9
                    radius: 2
                    color: handle.pressed ? "#ff8a1f" : "white"
                    border.width: 1
                    border.color: "#15161a"
                }
            }
        }

        // A rounded rectangle has one handle more: a round one on its top edge, as far
        // in from the corner as the corners are round, which is dragged along the edge
        // to make them more so or less. (It keeps clear of the corner's own handle.)
        MouseArea {
            id: cornerHandle

            objectName: "cornerHandle"
            readonly property var element: canvas.selected
            readonly property real shorter: element ? Math.min(element.width, element.height) : 1
            // The roundness while it is being dragged, or -1
            property real dragged: -1
            property real pressedX: 0
            property real from: 0

            visible: element !== null && element.shape === "roundedRectangle" && !element.locked && !canvas.editing
                     && frame.width > 60
            x: Math.max(16, (dragged >= 0 ? dragged : element ? element.roundness : 0) * shorter * canvas.u) - width / 2
            y: -height / 2
            width: 18
            height: 18
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: (mouse) => {
                canvas.forceActiveFocus()
                canvas.settle()
                pressedX = mapToItem(frame, mouse.x, mouse.y).x
                from = element.roundness
                dragged = from
            }
            onPositionChanged: (mouse) => {
                if (dragged < 0)
                    return
                // Along the element's own top edge, whichever way that is turned
                const moved = (mapToItem(frame, mouse.x, mouse.y).x - pressedX) / canvas.u
                dragged = Math.max(0, Math.min(0.5, from + moved / shorter))
                canvas.setProperties({ roundness: Math.round(dragged * 1000) / 1000 }, true)
            }
            onReleased: {
                if (dragged >= 0)
                    canvas.settle()
                dragged = -1
            }
            onCanceled: dragged = -1

            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 10
                radius: 5
                color: cornerHandle.pressed ? "#ff8a1f" : "#ffd24a"
                border.width: 1
                border.color: "#15161a"
            }
        }
    }

    // The text being edited. In slide units, scaled to the canvas, so that it lays its
    // text out exactly as the element is drawn.
    Item {
        id: textFrame

        readonly property var element: canvas.editing

        visible: element !== null
        x: element ? canvas.originX + (element.x + element.marginLeft) * canvas.u : 0
        y: element ? canvas.originY + (element.y + element.marginTop) * canvas.u : 0

        TextEdit {
            id: textEdit

            // Set while the text is being put in, which is not typing.
            property bool loading: false

            width: textFrame.element ? Math.max(1, textFrame.element.width - textFrame.element.marginLeft - textFrame.element.marginRight) : 1
            height: textFrame.element ? Math.max(1, textFrame.element.height - textFrame.element.marginTop - textFrame.element.marginBottom) : 1
            scale: canvas.u
            transformOrigin: Item.TopLeft
            enabled: textFrame.element !== null
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.WordWrap
            verticalAlignment: !textFrame.element ? TextEdit.AlignTop
                             : textFrame.element.verticalAlignment & Qt.AlignTop ? TextEdit.AlignTop
                             : textFrame.element.verticalAlignment & Qt.AlignBottom ? TextEdit.AlignBottom : TextEdit.AlignVCenter
            // Only the caret and the selection are seen; the element draws the text.
            color: "transparent"
            selectedTextColor: "transparent"
            selectionColor: "#804da3ff"
            selectByMouse: true
            persistentSelection: true
            textMargin: 0
            onTextChanged: {
                if (loading || canvas.editingId === "")
                    return
                canvas.typed = true
                canvas.liveText = canvas.bridge.save(textDocument)
            }
            // What the keys do while typing: Esc ends it, and the usual keys set bold,
            // italic and underline. Undo first undoes typing, then earlier changes.
            Keys.onPressed: (event) => {
                const control = event.modifiers & Qt.ControlModifier
                if (event.key === Qt.Key_Escape) {
                    canvas.finishText()
                    canvas.forceActiveFocus()
                } else if (control && event.key === Qt.Key_B) {
                    canvas.setFormat({ bold: !canvas.format.bold }, false)
                } else if (control && event.key === Qt.Key_I) {
                    canvas.setFormat({ italic: !canvas.format.italic }, false)
                } else if (control && event.key === Qt.Key_U) {
                    canvas.setFormat({ underline: !canvas.format.underline }, false)
                } else if (control && event.key === Qt.Key_Z && !(event.modifiers & Qt.ShiftModifier) && !canUndo) {
                    canvas.undo()
                } else if (control && (event.key === Qt.Key_Y || (event.key === Qt.Key_Z && (event.modifiers & Qt.ShiftModifier)))
                           && !canRedo) {
                    canvas.redo()
                } else {
                    return
                }
                event.accepted = true
            }

            cursorDelegate: Rectangle {
                // Two pixels of the screen wide, whatever the slide is scaled to.
                width: 2 / canvas.u
                color: "#ff8a1f"
                visible: textEdit.activeFocus && textEdit.cursorVisible

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: textEdit.activeFocus

                    NumberAnimation { to: 1; duration: 0 }
                    PauseAnimation { duration: 550 }
                    NumberAnimation { to: 0; duration: 0 }
                    PauseAnimation { duration: 450 }
                }
            }
        }
    }

    // Undoing and redoing while text is being edited brings the text back with it.
    function undo() {
        sendText()
        if (!report(editor.undo()))
            return
        afterRestore()
    }

    function redo() {
        sendText()
        if (!report(editor.redo()))
            return
        afterRestore()
    }

    function afterRestore() {
        if (editingId === "")
            return
        if (elements.some(e => e.id === editingId)) {
            loadText(textEdit.cursorPosition, textEdit.cursorPosition)
        } else {
            editingId = ""
            liveText = undefined
        }
    }

    // Gives the keyboard to whatever should have it: the text being edited, or the canvas.
    function takeFocus() {
        if (editingId !== "")
            textEdit.forceActiveFocus()
        else
            forceActiveFocus()
    }
}
