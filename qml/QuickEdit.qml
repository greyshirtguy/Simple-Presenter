import QtQuick
import SimplePresenterApp

// Quick Edit: a slide put up on its own, over its own place in the grid, for its words to
// be put right without the editor (Quick Edit in a slide's menu, beside Edit), as
// ProPresenter's Quick Edit is for a typo seen in the middle of a service. Nothing of it
// goes to the output: the slide is not shown by being edited, and one that is live stays
// as it is on the screens until it is next shown, as after the editor.
//
// It is small, to hide as little of the other slides as it can: about twice the size of
// the slide's thumbnail, over it (kept inside the window), with a border of a pixel and a
// slim line along its top for what it is and a button to put it away; the window behind
// is not darkened. (ProPresenter's is the size of the thumbnail itself, which the
// maintainer finds can be too small to type in.)
//
// It is the editor's own canvas (EditorCanvas.qml) in its text-only mode, on an editor
// of its own (PresentationEditor), so the words are drawn as they will be shown as they
// are typed, and keep every bit of their formatting: a click on a text box types in it,
// Tab goes on to the next, and there is nothing to move, size or format. What is typed
// is saved to the file as the editor saves it (the file as it was is kept with the
// editor's backups first) and can be undone with Ctrl+Z while it is up. Esc, the button
// at its top right or a click anywhere outside it put it away, keeping what was typed.
//
// What ProPresenter's has, from its own description and the maintainer: Quick Edit in
// the slide's menu, a small editor of the one slide's words, the size of its thumbnail.
// A GUESS: the rest of how it looks, Tab, and that a click outside keeps what was typed.
Item {
    id: panel

    property bool open: false
    property string about: ""
    // Called when it is put away, with whether anything was changed
    property var done: null
    // Where the slide's thumbnail is, in this item's terms ({ x, y, width, height }), or
    // null to be in the middle
    property var near: null
    // Something that went wrong, shown in place of `about` until it is next put up
    property string problem: ""
    readonly property alias editor: quickEditor
    readonly property alias canvas: quickCanvas
    // The slide's own width in the thumbnail, and so about twice that here
    readonly property real thumbnailWidth: near ? Math.max(120, near.width) : 320
    readonly property real canvasWidth: Math.max(240, Math.min(width - 16, 2 * thumbnailWidth))

    signal closed

    // Opens a presentation file from a workspace folder at the slide with the given id,
    // over the thumbnail at `near` (see above), and gives the keyboard to its first text
    // box. `backdrop` is a picture to show behind the slide, as the editor shows the media
    // the slide triggers ("" for none). Returns an error message, empty on success.
    function show(path, workspace, slideId, about, backdrop, near, done) {
        const error = quickEditor.open(path, workspace)
        if (error !== "")
            return error
        const row = quickEditor.rowOf(slideId)
        if (row < 0) {
            quickEditor.close()
            return "That slide only triggers media, so there are no words on it to edit."
        }
        panel.about = about
        panel.near = near
        panel.done = done
        panel.problem = ""
        quickCanvas.backdrop = backdrop
        quickCanvas.showRow(row)
        open = true
        quickCanvas.forceActiveFocus()
        quickCanvas.typeNext(1)
        return ""
    }

    // Puts it away, keeping what was typed.
    function close() {
        if (!open)
            return
        // (Which finishes what was being typed and hands it to the editor first)
        quickCanvas.showRow(-1)
        const changed = quickEditor.changed
        quickEditor.close()
        open = false
        const finished = panel.done
        panel.done = null
        if (finished)
            finished(changed)
        closed()
    }

    visible: open

    // A click anywhere else puts it away; the window behind is not darkened.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    Rectangle {
        id: box

        objectName: "quickEdit"
        // Over the slide's thumbnail, its middle on the thumbnail's, and inside the window
        x: Math.max(4, Math.min(panel.width - width - 4, panel.near ? panel.near.x + panel.near.width / 2 - width / 2 : (panel.width - width) / 2))
        y: Math.max(4, Math.min(panel.height - height - 4, panel.near ? panel.near.y + panel.near.height / 2 - height / 2 : (panel.height - height) / 2))
        width: quickCanvas.width + 2
        height: titleLine.height + quickCanvas.height + 2
        radius: 3
        color: "#2b2d31"
        border.width: 1
        border.color: "#6b6e75"

        MouseArea {
            anchors.fill: parent
        }

        // What it is, and the button that puts it away
        Item {
            id: titleLine

            x: 1
            y: 1
            width: parent.width - 2
            height: 18

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.right: doneButton.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                color: panel.problem !== "" ? "#ff8a80" : "#b4b7bd"
                font.pixelSize: 11
                text: panel.problem !== "" ? panel.problem : "Quick Edit · " + panel.about + " · Esc to finish"
            }

            Rectangle {
                id: doneButton

                objectName: "quickEditDone"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 16
                radius: 2
                color: doneArea.containsMouse ? "#50535a" : "transparent"

                Text {
                    anchors.centerIn: parent
                    color: "#e6e6e6"
                    font.pixelSize: 12
                    text: "✕"
                }

                MouseArea {
                    id: doneArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: panel.close()
                }
            }
        }

        EditorCanvas {
            id: quickCanvas

            objectName: "quickEditCanvas"
            x: 1
            y: titleLine.y + titleLine.height
            width: panel.canvasWidth
            // The slide as wide as the canvas, less the canvas's own small margin
            height: Math.round((width - 2 * margin) * slideHeight / slideWidth + 2 * margin)
            margin: 3
            clip: true
            textOnly: true
            editor: quickEditor
            bridge: quickBridge
            onFailed: (error) => panel.problem = error
            onTold: (message) => panel.problem = message
            onDismissed: panel.close()
        }
    }

    PresentationEditor {
        id: quickEditor

        // (Undo only ever has this one slide to go back to.)
        onRestored: (row) => quickCanvas.showRow(row)
    }

    RichTextBridge {
        id: quickBridge
    }
}
