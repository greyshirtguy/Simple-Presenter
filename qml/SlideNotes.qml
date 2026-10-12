import QtQuick
import QtQuick.Controls.Basic
import SimplePresenterApp

// A slide's notes, under the slide in the editor: words for whoever runs the show or
// stands on the stage, kept with the slide in ProPresenter's file and never shown to
// the audience (see proconvert::readNotes for how the file keeps them). A text box of
// a stage layout can be set to show them.
//
// It is a plain box of text, as ProPresenter's is, with no controls of its own. What
// is typed is in the format of the text before it. Ctrl+B, Ctrl+I and Ctrl+U make what
// is selected bold, italic or underlined (or all of the notes, with nothing selected);
// a right click puts up a small panel with the font, its size and its colour. The
// notes are drawn here as they are written: in their own fonts, sizes and colours.
// (ProPresenter's start out white, on a dark ground like this one.)
//
// Nothing is saved by hand. What is typed goes into the file when the keyboard leaves
// the box, when another slide is turned to, and a moment after the typing stops; a
// change of format goes at once. Each is a step that Undo takes back. Notes typed onto
// a slide that had none and taken out again leave the file as it was.
//
// The top edge is dragged to make the pane taller or shorter.
//
// GUESSES at ProPresenter, whose notes pane was described and not looked at: that its
// keys are these three, and that its panel has these three things in it.
Rectangle {
    id: pane

    // The presentation being edited (a PresentationEditor)
    required property var editor
    // The slide whose notes these are, by its row; -1 for none
    property int row: -1
    // The installed font families, for the panel
    property var families: []
    // Whether the notes are being typed in: the box has the keyboard
    readonly property bool typing: edit.activeFocus
    // The row whose notes are in the box, which is the row above until it changes
    property int loadedRow: -1
    // Something has been typed that has not gone to the file yet
    property bool typed: false
    // Set while the box is being filled, which is not typing; and while what was typed
    // is being handed over, which comes back as a change to the slide
    property bool loading: false
    property bool sending: false
    // The format at the caret or of what is selected, for the panel: a map as
    // RichText::formatAt gives, read when the panel is put up
    property var current: ({})

    // Something could not be saved
    signal failed(string error)
    // The top edge was dragged: this is how tall the pane is asked to be
    signal resized(real height)
    // The keyboard is given back (Esc)
    signal finished
    // Undo or redo was asked for with nothing of the typing left to undo
    signal undoRequested
    signal redoRequested

    // Puts the row's notes into the box, with the caret where it was if it can be.
    function load() {
        const at = edit.cursorPosition
        loading = true
        bridge.loadShown(edit.textDocument, editor.notes(row))
        loading = false
        loadedRow = row
        typed = false
        idle.stop()
        edit.cursorPosition = Math.min(at, edit.length)
    }

    // Hands what has been typed to the file.
    function send() {
        idle.stop()
        if (!typed || loadedRow < 0)
            return
        typed = false
        sending = true
        const error = editor.setNotes(loadedRow, bridge.save(edit.textDocument))
        sending = false
        if (error !== "")
            failed(error)
    }

    // Gives what is selected a format, or all of the notes if nothing is: `change` is
    // as RichText::formatted takes. With `interim`, only here, for a colour that is
    // still being dragged to; without, into the file as well.
    function applyFormat(change, interim) {
        const whole = edit.selectionStart === edit.selectionEnd
        const start = whole ? 0 : edit.selectionStart
        const end = whole ? 1000000000 : edit.selectionEnd
        const caret = edit.cursorPosition
        const formatted = bridge.formatted(bridge.save(edit.textDocument), start, end, change)
        loading = true
        bridge.loadShown(edit.textDocument, formatted)
        loading = false
        if (whole)
            edit.cursorPosition = Math.min(caret, edit.length)
        else
            edit.select(start, end)
        typed = true
        if (!interim)
            send()
        readFormat()
    }

    function readFormat() {
        current = bridge.formatAt(bridge.save(edit.textDocument), edit.selectionStart, edit.selectionEnd)
    }

    function takeKeys() {
        edit.forceActiveFocus()
    }

    // Between formatted text and a box of text
    RichTextBridge {
        id: bridge
    }

    color: "#1e1f22"
    onRowChanged: {
        // What was typed for the slide that is being left is that slide's.
        send()
        fontPanel.visible = false
        load()
    }
    Component.onCompleted: load()
    Component.onDestruction: send()

    Connections {
        target: pane.editor

        // The slide was changed from elsewhere (an undo from the toolbar, say): what
        // is in the box is what the file has, unless it is being typed in.
        function onSlideChanged(row) {
            if (row === pane.loadedRow && !pane.sending && !pane.typed)
                pane.load()
        }

        function onRestored(row) {
            if (row === pane.loadedRow)
                pane.load()
        }

        function onDocumentChanged() {
            pane.load()
        }
    }

    // A moment after the typing stops, it is saved.
    Timer {
        id: idle

        interval: 1200
        onTriggered: pane.send()
    }

    // The top edge, which is dragged to share out the height with the slide
    Rectangle {
        id: edge

        objectName: "notesEdge"
        anchors.left: parent.left
        anchors.right: parent.right
        height: 6
        color: edgeMouse.pressed ? "#ff8a1f" : edgeMouse.containsMouse ? "#50535a" : "#2b2d31"

        MouseArea {
            id: edgeMouse

            property real startedAt
            property real startedWith

            anchors.fill: parent
            anchors.topMargin: -2
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            preventStealing: true
            onPressed: (mouse) => {
                startedAt = mapToItem(null, mouse.x, mouse.y).y
                startedWith = pane.height
            }
            onPositionChanged: (mouse) => {
                if (pressed)
                    pane.resized(startedWith - (mapToItem(null, mouse.x, mouse.y).y - startedAt))
            }
        }
    }

    Flickable {
        id: flick

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: edge.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 8
        anchors.topMargin: 6
        clip: true
        contentHeight: edit.height
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        // Keeps the caret in sight as it moves.
        function follow(caret) {
            if (caret.y < contentY)
                contentY = Math.max(0, caret.y)
            else if (caret.y + caret.height > contentY + height)
                contentY = caret.y + caret.height - height
        }

        TextEdit {
            id: edit

            objectName: "notesEdit"
            width: flick.width - 10
            height: Math.max(flick.height, implicitHeight)
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            // (Text that says nothing of its colour, which notes written here never
            // are: each stretch has its own.)
            color: "#e6e6e6"
            selectionColor: "#4d6f9a"
            selectByMouse: true
            persistentSelection: true
            onCursorRectangleChanged: flick.follow(cursorRectangle)
            onTextChanged: {
                if (pane.loading)
                    return
                pane.typed = true
                idle.restart()
            }
            onActiveFocusChanged: {
                if (!activeFocus)
                    pane.send()
            }
            // What the keys do while typing: the usual three set bold, italic and
            // underline; Esc gives the keys back to the slide. Undo takes back the
            // typing first, and then, with none of that left, earlier changes. What is
            // pasted comes in as words, in the format of the text before it.
            Keys.onPressed: (event) => {
                const control = event.modifiers & Qt.ControlModifier
                const shift = event.modifiers & Qt.ShiftModifier
                if (event.key === Qt.Key_Escape) {
                    fontPanel.visible = false
                    pane.send()
                    pane.finished()
                } else if (control && event.key === Qt.Key_B) {
                    pane.readFormat()
                    pane.applyFormat({ bold: !pane.current.bold }, false)
                } else if (control && event.key === Qt.Key_I) {
                    pane.readFormat()
                    pane.applyFormat({ italic: !pane.current.italic }, false)
                } else if (control && event.key === Qt.Key_U) {
                    pane.readFormat()
                    pane.applyFormat({ underline: !pane.current.underline }, false)
                } else if (control && event.key === Qt.Key_V) {
                    cursorPosition = bridge.paste(textDocument, selectionStart, selectionEnd)
                } else if (control && event.key === Qt.Key_Z && !shift && !canUndo) {
                    pane.send()
                    pane.undoRequested()
                } else if (control && (event.key === Qt.Key_Y || (event.key === Qt.Key_Z && shift)) && !canRedo) {
                    pane.send()
                    pane.redoRequested()
                } else {
                    return
                }
                event.accepted = true
            }

            // A right click puts the panel up, by the pointer.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                cursorShape: Qt.IBeamCursor
                onClicked: (mouse) => {
                    edit.forceActiveFocus()
                    pane.readFormat()
                    const at = mapToItem(pane, mouse.x, mouse.y)
                    fontPanel.x = Math.max(6, Math.min(at.x, pane.width - fontPanel.width - 6))
                    // Over the pointer if there is no room under it, which in a pane
                    // this short there seldom is: it then stands over the slide.
                    fontPanel.y = at.y + fontPanel.height + 6 <= pane.height ? at.y + 4 : at.y - fontPanel.height - 4
                    fontPanel.visible = true
                }
            }
        }
    }

    // What an empty box is for
    Text {
        objectName: "notesPlaceholder"
        anchors.left: flick.left
        anchors.top: flick.top
        visible: edit.length === 0
        color: "#6c6f75"
        font.pixelSize: 14
        text: "Notes for this slide"
    }

    // A press anywhere but on the panel puts it away, and is then whatever it would
    // have been (a press on the slide picks what is there). It is watched for over the
    // whole window, since the panel can stand higher than the pane does; the lists
    // the panel's own controls open are above this, and are not its business.
    MouseArea {
        parent: pane.Window.contentItem ?? pane
        anchors.fill: parent
        z: 1000
        visible: fontPanel.visible
        acceptedButtons: Qt.AllButtons
        onPressed: (mouse) => {
            const at = mapToItem(fontPanel, mouse.x, mouse.y)
            if (!fontPanel.contains(Qt.point(at.x, at.y)))
                fontPanel.visible = false
            mouse.accepted = false
        }
    }

    // The panel a right click puts up: the font of what is selected (or of all the
    // notes, with nothing selected), its size and its colour.
    Rectangle {
        id: fontPanel

        objectName: "notesFontPanel"
        visible: false
        z: 10
        width: 252
        height: rows.height + 20
        radius: 8
        color: "#2b2d31"
        border.width: 1
        border.color: "#4a4d54"

        // (Clicks on the panel itself are its own.)
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
        }

        Column {
            id: rows

            x: 10
            y: 10
            width: parent.width - 20
            spacing: 6

            component Row3: Item {
                property alias caption: label.text
                default property alias controls: holder.data

                width: parent.width
                height: 28

                Text {
                    id: label

                    width: 52
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#9a9da3"
                    font.pixelSize: 11
                }

                Row {
                    id: holder

                    x: 56
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                }
            }

            Row3 {
                caption: "Font"

                FontPicker {
                    objectName: "notesFont"
                    width: rows.width - 56
                    family: pane.current.family ?? ""
                    families: pane.families
                    onPicked: (family) => pane.applyFormat({ family: family }, false)
                    onClosed: edit.forceActiveFocus()
                }
            }

            Row3 {
                caption: "Size"

                NumberField {
                    objectName: "notesSize"
                    width: 60
                    from: 4
                    to: 400
                    value: pane.current.size ?? 24
                    onEdited: (value) => pane.applyFormat({ size: value }, false)
                    onFinished: edit.forceActiveFocus()
                }
            }

            Row3 {
                caption: "Colour"

                ColorButton {
                    objectName: "notesColor"
                    hasAlpha: false
                    value: pane.current.color ?? "white"
                    onChanging: (value) => pane.applyFormat({ color: value }, true)
                    onPicked: (value) => pane.applyFormat({ color: value }, false)
                    onClosed: edit.forceActiveFocus()
                }
            }
        }
    }
}
