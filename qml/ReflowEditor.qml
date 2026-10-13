import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// The reflow editor: the words of a presentation's slides as a table, a row to a slide,
// for moving the words between slides. It is one of the ways the editor works on a
// presentation (Editor.qml, `mode`), come to by the Reflow button in the window's
// toolbar or Ctrl+R.
//
// Getting a song's words spread well over its slides is most of the work of setting it
// up, and in the editor proper it is done a slide at a time: cut the words out of one
// text box, make a slide like it, paste them in. Here the words are one column that is
// typed in as a page is, and a slide is a row of it:
//
//   - Ctrl+Enter, or the Insert Slide Break button, splits the slide where the caret
//     is: what is after the caret goes to a new slide straight after, which is the
//     slide over again (the same text boxes in the same places, set the same) with
//     those words in it.
//   - Backspace at the very start of a slide's words joins the slide to the one before
//     it, the words going after that one's on a new line; Delete at the very end joins
//     the next slide to this one the same way.
//   - Enter is a line break within the slide, and words are typed and corrected as
//     anywhere. What is typed is saved a moment after the typing stops.
//
// The words keep their fonts, sizes and colours through all of that, and their chords,
// though none of it is shown or set here: the column is in one plain face that is easy
// to read. (ProPresenter's reflow editor shows the words in their own fonts and has
// controls for them. That was left out at the maintainer's asking: this is for
// reflowing, and the editor proper is for how the words look.) At the right are the
// slides themselves, as they will be shown, so that what a split or a join has made of
// them can be seen at once; the slider under them sets their size.
//
// How it is put together. PresentationEditor does the work (reflow(), reflowType(),
// reflowSplit(), reflowJoin(): src/presentationeditor.h, and src/reflow.h for the
// rules of the words), and undo is its undo. The rows are kept in a ListModel that is
// brought in step with the editor's rows in place, and not made again: a row that is
// being typed in must go on being the same row, with its caret where it was, when the
// words it has just been given are saved and the slide is described afresh.
//
// GUESSES, where ProPresenter's was not looked at: that Up and Down go from the first
// and last line of a row to the row before and after; that Tab goes to the next row;
// and that a slide with no text box of its own is a row that says so and takes no
// typing. The rest of what is a guess is in the two headers named above. (The
// maintainer has since said that these are as wanted; and that the editor's own switch
// of its other three ways of working is rightly not there while this is up.)
//
// It is for a presentation only. Props, stage layouts and themes are not reflowed, and
// are not meant to be.
Item {
    id: reflow

    // The PresentationEditor that has the presentation open
    required property var editor
    // The colour of a slide's group, as the slide grid has it
    property var groupColor: (slide) => "#2b2d31"
    // The url of a picture to show behind the slide with this id, or ""
    property var backdropFor: (slideId) => ""
    // The row of the slide the editor is on: where the caret is put when this comes up
    property int row: 0
    // How wide a slide is drawn at the right
    property real thumbnail: 220
    // What the line along the bottom of the editor says while this is up
    readonly property string hint: "Ctrl+Enter splits the slide at the caret  ·  Backspace at the start of a slide's words joins it to the one before, Delete at their end the one after  ·  Enter is a line break"
    readonly property color accentColor: "#ff8a1f"
    // The row the caret is in, or was last in: -1 before it has been in any
    property int current: -1
    // A row to give the caret to once the rows have been read again, and where in its
    // words: after a split or a join, the row is not there to be given it until then.
    property int pendingRow: -1
    property int pendingCaret: 0

    signal failed(string error)
    signal rowPicked(int row)
    signal thumbnailEdited(real width)

    // Gives the keyboard to the row the editor is on, at the end of its words.
    function takeKeys() {
        focusRow(current >= 0 ? current : row, -1)
    }

    // Saves what has been typed and not yet saved.
    function settle() {
        saving.stop()
        const item = current >= 0 ? table.itemAtIndex(current) : null
        if (item)
            item.send()
    }

    // Brings the rows in step with the editor's: each as the editor has it now, more or
    // fewer of them if there are, none made again that is still there.
    function reload() {
        const now = editor.reflow()
        while (rows.count > now.length)
            rows.remove(rows.count - 1)
        for (let i = 0; i < now.length; ++i) {
            const entry = { slideId: now[i].slideId, group: now[i].group, groupColor: now[i].groupColor, groupStart: now[i].groupStart,
                            label: now[i].label, words: now[i].text, editable: now[i].editable }
            if (i >= rows.count) {
                rows.append(entry)
                continue
            }
            const was = rows.get(i)
            for (const key in entry) {
                if (was[key] !== entry[key])
                    rows.setProperty(i, key, entry[key])
            }
        }
        if (pendingRow >= 0) {
            const to = pendingRow
            const caret = pendingCaret
            pendingRow = -1
            // (After the list has laid the rows out: a row that has just been made is
            // not there to be given the keyboard until then.)
            Qt.callLater(() => reflow.focusRow(to, caret))
        }
    }

    // Puts the caret in a row: at a place in its words, or with -1 at their end.
    function focusRow(index, caret) {
        if (rows.count === 0) {
            reflow.forceActiveFocus()
            return
        }
        const to = Math.max(0, Math.min(rows.count - 1, index))
        table.positionViewAtIndex(to, ListView.Contain)
        const item = table.itemAtIndex(to)
        if (item)
            item.take(caret)
        else
            reflow.forceActiveFocus()
    }

    // Splits a row's slide where the caret is in its words.
    function split(index, caret) {
        settle()
        const made = editor.reflowSplit(index, caret)
        if (made.error !== "") {
            failed(made.error)
            return
        }
        Log.note("edit", "reflow: slide " + (index + 1) + " split in two")
        pendingRow = made.row
        pendingCaret = 0
        reload()
    }

    // Joins a row's slide to the one before it.
    function join(index) {
        if (index < 1 || index >= rows.count)
            return
        settle()
        const made = editor.reflowJoin(index)
        if (made.error !== "") {
            failed(made.error)
            return
        }
        Log.note("edit", "reflow: slide " + (index + 1) + " joined to slide " + index)
        pendingRow = made.row
        pendingCaret = made.caret
        reload()
    }

    // The Insert Slide Break button: where the caret is, in the row it is in.
    function splitHere() {
        const item = current >= 0 ? table.itemAtIndex(current) : null
        if (item && item.typing)
            split(current, item.caret)
    }

    Component.onCompleted: reload()

    Connections {
        target: reflow.editor

        // A slide's words were changed (typed here and saved, or by an undo), or the
        // slides are other slides (a split, a join, or that undone).
        function onSlideChanged(row) {
            reflow.reload()
        }

        function onRestructured() {
            reflow.reload()
        }

        // An undo or a redo of a change to one slide: the caret goes to its row.
        function onRestored(row) {
            reflow.pendingRow = row
            reflow.pendingCaret = -1
            reflow.reload()
        }
    }

    ListModel {
        id: rows
    }

    // What has been typed is saved a moment after the typing stops, so that the slides
    // at the right follow it, and so that it is one step to undo and not a letter.
    Timer {
        id: saving

        interval: 700
        onTriggered: reflow.settle()
    }

    Rectangle {
        anchors.fill: parent
        color: "#1b1c1f"
    }

    // ---- The table
    Rectangle {
        id: sheet

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.round(Math.max(320, Math.min(parent.width - 260, parent.width * 0.46)))
        color: "#202226"

        ListView {
            id: table

            objectName: "reflowTable"
            anchors.fill: parent
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // (Every row is kept made, whether or not it is in sight: a row is given
            // the caret by being asked for, and one the list had not made could not
            // be. A song is some tens of rows of plain text.)
            cacheBuffer: 100000
            model: rows

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Item {
                id: line

                required property int index
                required property string slideId
                required property string group
                required property string groupColor
                required property bool groupStart
                required property string label
                required property string words
                required property bool editable
                readonly property color frame: reflow.groupColor({ group: group, groupColor: groupColor })
                readonly property bool typing: box.activeFocus
                readonly property int caret: box.cursorPosition
                // Typed in since it was last saved
                property bool unsaved: false

                // Takes the keyboard, with the caret at a place in its words, or with
                // -1 at their end.
                function take(caret) {
                    if (!editable) {
                        reflow.current = index
                        reflow.forceActiveFocus()
                        return
                    }
                    box.forceActiveFocus()
                    box.cursorPosition = caret < 0 ? box.length : Math.min(caret, box.length)
                }

                // Saves what has been typed in it.
                function send() {
                    if (!unsaved)
                        return
                    unsaved = false
                    const error = reflow.editor.reflowType(index, box.text)
                    if (error !== "")
                        reflow.failed(error)
                }

                objectName: "reflowRow"
                width: table.width
                height: Math.max(40, box.contentHeight + 20)

                // The words as the editor has them, taken up unless they are being
                // typed over here: what is typed is what the editor is given, and
                // comes back the same.
                onWordsChanged: {
                    if (box.text !== words) {
                        const at = box.cursorPosition
                        box.text = words
                        box.cursorPosition = Math.min(at, box.length)
                    }
                    unsaved = false
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 12
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    radius: 5
                    color: line.typing ? "#2e3138" : lineMouse.containsMouse ? "#282a30" : "#25272b"
                    border.width: line.typing ? 1 : 0
                    border.color: reflow.accentColor
                }

                // Its group's colour down its left edge, as the slide's frame has it
                Rectangle {
                    x: 8
                    y: 2
                    width: 5
                    height: parent.height - 4
                    radius: 2
                    color: line.frame
                }

                // Its number, and its group's name where the group begins
                Text {
                    x: 20
                    y: 11
                    width: 26
                    horizontalAlignment: Text.AlignRight
                    color: "#9a9da3"
                    font.pixelSize: 12
                    text: line.index + 1
                }

                Text {
                    id: groupName

                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    y: 11
                    width: Math.min(implicitWidth, 110)
                    visible: text !== ""
                    elide: Text.ElideRight
                    color: "#9a9da3"
                    font.pixelSize: 11
                    text: (line.groupStart ? line.group : "") + (line.label !== "" && line.label !== line.group ? (line.groupStart && line.group !== "" ? "  ·  " : "") + line.label : "")
                }

                MouseArea {
                    id: lineMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: line.take(-1)
                }

                TextEdit {
                    id: box

                    objectName: "reflowWords"
                    x: 56
                    y: 10
                    width: parent.width - 56 - 24 - (groupName.visible ? groupName.width + 10 : 0)
                    visible: line.editable
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    selectByMouse: true
                    color: "#eceded"
                    selectionColor: reflow.accentColor
                    selectedTextColor: "black"
                    font.pixelSize: 15
                    text: line.words
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            reflow.current = line.index
                            reflow.rowPicked(line.index)
                        } else {
                            // The caret has left the row: what was typed in it is saved.
                            line.send()
                        }
                    }
                    onTextChanged: {
                        if (activeFocus && text !== line.words) {
                            line.unsaved = true
                            saving.restart()
                        }
                    }
                    Keys.onPressed: (event) => {
                        const control = (event.modifiers & Qt.ControlModifier) !== 0
                        const plain = (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.ShiftModifier)) === 0
                        const none = selectionStart === selectionEnd
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && control) {
                            reflow.split(line.index, cursorPosition)
                        } else if (event.key === Qt.Key_Backspace && plain && none && cursorPosition === 0 && line.index > 0) {
                            reflow.join(line.index)
                        } else if (event.key === Qt.Key_Delete && plain && none && cursorPosition === length && line.index < rows.count - 1) {
                            reflow.join(line.index + 1)
                        } else if (event.key === Qt.Key_Up && plain && cursorRectangle.y < 2 && line.index > 0) {
                            reflow.focusRow(line.index - 1, -1)
                        } else if (event.key === Qt.Key_Down && plain && cursorRectangle.y + cursorRectangle.height > contentHeight - 2
                                   && line.index < rows.count - 1) {
                            reflow.focusRow(line.index + 1, 0)
                        } else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
                            reflow.focusRow(line.index + (event.key === Qt.Key_Tab ? 1 : -1), -1)
                        } else if (event.key === Qt.Key_Z && control) {
                            // Undo is the editor's, of what has been saved: what is
                            // being typed is saved first, and is then the last step.
                            reflow.settle()
                            const error = (event.modifiers & Qt.ShiftModifier) ? reflow.editor.redo() : reflow.editor.undo()
                            if (error !== "")
                                reflow.failed(error)
                        } else if (event.key === Qt.Key_Escape) {
                            reflow.forceActiveFocus()
                        } else {
                            return
                        }
                        event.accepted = true
                    }
                }

                Text {
                    x: 56
                    y: 11
                    visible: !line.editable
                    color: "#7d8088"
                    font.pixelSize: 13
                    font.italic: true
                    text: "No text box of its own"
                }
            }
        }
    }

    // ---- The slides, as they will be shown
    GridView {
        id: slides

        objectName: "reflowSlides"
        anchors.left: sheet.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: sizeBar.top
        anchors.leftMargin: 10
        anchors.topMargin: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        readonly property int columns: Math.max(1, Math.floor(width / reflow.thumbnail))
        cellWidth: Math.floor(width / columns)
        cellHeight: Math.round((cellWidth - 16) * 9 / 16) + 30
        model: reflow.editor

        ScrollBar.vertical: ScrollBar {}

        KineticWheel {}

        delegate: Item {
            id: cell

            required property var slide
            required property int index
            readonly property bool current: index === reflow.current
            readonly property color frame: reflow.groupColor(slide)
            readonly property bool lightFrame: 0.299 * frame.r + 0.587 * frame.g + 0.114 * frame.b > 0.6

            objectName: "reflowSlide"
            width: slides.cellWidth
            height: slides.cellHeight

            // The slide whose row the caret is in is ringed.
            Rectangle {
                anchors.fill: frameRect
                anchors.margins: -3
                radius: 7
                visible: cell.current
                color: "transparent"
                border.width: 2
                border.color: reflow.accentColor
            }

            Rectangle {
                id: frameRect

                x: 5
                y: 4
                width: parent.width - 12
                height: parent.height - 8
                radius: 4
                color: cell.frame

                Rectangle {
                    id: picture

                    x: 3
                    y: 3
                    width: parent.width - 6
                    height: Math.round(width * 9 / 16)
                    color: "black"

                    Image {
                        anchors.fill: parent
                        source: reflow.backdropFor(cell.slide.id)
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
                    anchors.top: picture.bottom
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    color: cell.lightFrame ? "black" : "white"
                    font.pixelSize: 11
                    text: (cell.index + 1) + (cell.slide.groupStart && cell.slide.group !== "" ? "  " + cell.slide.group : "")
                }
            }

            // A click on a slide puts the caret in its row.
            MouseArea {
                anchors.fill: parent
                onClicked: reflow.focusRow(cell.index, -1)
            }
        }
    }

    // The size the slides are drawn at
    Item {
        id: sizeBar

        anchors.left: sheet.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 30

        AppSlider {
            objectName: "reflowSize"
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 150
            from: 120
            to: 480
            value: reflow.thumbnail
            onMoved: reflow.thumbnailEdited(Math.round(value))
        }
    }

    // The row the caret is in is in sight among the slides at the right too.
    onCurrentChanged: {
        if (current >= 0)
            slides.positionViewAtIndex(current, GridView.Contain)
    }
}
