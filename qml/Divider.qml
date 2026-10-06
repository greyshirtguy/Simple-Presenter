import QtQuick

// A draggable line between two panes. `vertical` is a vertical line dragged sideways.
// It reports each movement in pixels and leaves it to whoever uses it to decide which
// pane grows: the panes' sizes belong to the operator window, which clamps and saves
// them.
MouseArea {
    id: divider

    property bool vertical: true
    property real last: 0

    signal moved(real delta)

    function position(mouse) {
        const point = mapToItem(null, mouse.x, mouse.y)
        return vertical ? point.x : point.y
    }

    width: vertical ? 7 : undefined
    height: vertical ? undefined : 7
    z: 1
    hoverEnabled: true
    preventStealing: true
    cursorShape: vertical ? Qt.SplitHCursor : Qt.SplitVCursor
    onPressed: (mouse) => last = position(mouse)
    onPositionChanged: (mouse) => {
        if (!pressed)
            return
        const now = position(mouse)
        moved(now - last)
        last = now
    }

    // Always drawn, so the panes' edges read as something to drag, but quietly.
    Rectangle {
        anchors.centerIn: parent
        width: divider.vertical ? 3 : parent.width
        height: divider.vertical ? parent.height : 3
        color: divider.containsMouse || divider.pressed ? "#a0a3aa" : "#5d6068"
    }
}
