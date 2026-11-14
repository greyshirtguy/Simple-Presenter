import QtQuick
import SimplePresenterApp

// A mouse area whose rows can also be dragged somewhere: onto a slide, to give the slide
// the action that goes with what is dragged (see beginActionDrag() in Main.qml).
//
// It is used as a MouseArea is, with `payload` set to what a drag from it carries, a
// map saying what kind of thing it is and which. A press becomes a drag as DragStart
// says (src/dragstart.h: the pointer a little way off, the button held a moment, so that
// a click made in passing is not one), and the release drops it; one let go past both
// without having moved since is dragged there and dropped at once. A release that
// follows a drag is still reported as a click, as a MouseArea reports one, so whoever
// handles the click asks `dragged` first.
MouseArea {
    id: source

    // The operator window, which shows what is being dragged and takes the drop
    required property var win
    // What a drag from here carries, or null for nothing to drag
    property var payload: null
    // Whether the press that is under way, or has just ended, became a drag
    property bool dragged: false
    property point pressedAt
    property real pressedTime: 0

    // Whether the press has become a drag, with the pointer at `mouse`: from the first
    // move that makes it one, on
    function carry(mouse) {
        if (!dragged && DragStart.begins(pressedAt, Qt.point(mouse.x, mouse.y), pressedTime)) {
            dragged = true
            win.beginActionDrag(payload, mapToItem(null, mouse.x, mouse.y))
        }
        if (dragged)
            win.moveActionDrag(mapToItem(null, mouse.x, mouse.y))
        return dragged
    }

    // A list would otherwise take the drag for itself, to scroll by.
    preventStealing: payload !== null
    onPressed: (mouse) => {
        pressedAt = Qt.point(mouse.x, mouse.y)
        pressedTime = DragStart.now()
        dragged = false
    }
    onPositionChanged: (mouse) => {
        if (payload !== null && (pressedButtons & Qt.LeftButton))
            carry(mouse)
    }
    onReleased: (mouse) => {
        if (payload !== null && mouse.button === Qt.LeftButton && carry(mouse))
            win.endActionDrag(true)
    }
    onCanceled: {
        if (dragged)
            win.endActionDrag(false)
    }
}
