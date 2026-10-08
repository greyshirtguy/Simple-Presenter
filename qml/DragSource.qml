import QtQuick

// A mouse area whose rows can also be dragged somewhere: onto a slide, to give the slide
// the action that goes with what is dragged (see beginActionDrag() in Main.qml).
//
// It is used as a MouseArea is, with `payload` set to what a drag from it carries, a
// map saying what kind of thing it is and which. A press that then moves a little
// becomes a drag, and the release drops it. A release that follows a drag is still
// reported as a click, as a MouseArea reports one, so whoever handles the click asks
// `dragged` first.
MouseArea {
    id: source

    // The operator window, which shows what is being dragged and takes the drop
    required property var win
    // What a drag from here carries, or null for nothing to drag
    property var payload: null
    // Whether the press that is under way, or has just ended, became a drag
    property bool dragged: false
    property point pressedAt

    // A list would otherwise take the drag for itself, to scroll by.
    preventStealing: payload !== null
    onPressed: (mouse) => {
        pressedAt = Qt.point(mouse.x, mouse.y)
        dragged = false
    }
    onPositionChanged: (mouse) => {
        if (payload === null || !(pressedButtons & Qt.LeftButton))
            return
        if (!dragged && Math.abs(mouse.x - pressedAt.x) + Math.abs(mouse.y - pressedAt.y) > 10) {
            dragged = true
            win.beginActionDrag(payload)
        }
        if (dragged)
            win.moveActionDrag(mapToItem(null, mouse.x, mouse.y))
    }
    onReleased: {
        if (dragged)
            win.endActionDrag(true)
    }
    onCanceled: {
        if (dragged)
            win.endActionDrag(false)
    }
}
