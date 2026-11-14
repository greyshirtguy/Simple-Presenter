import QtQuick
import SimplePresenterApp

// One cue of a timeline, drawn on its track (see TimelinePanel.qml): a block from its
// time, as long as it lasts, in its colour, with a line or two of words on it and, for
// an action, the action's picture.
//
// It is dragged along its track to another time, which is written to the presentation's
// file when it is let go (Main.moveTimelineCue); while it is held it is drawn where it
// would go, and the file is not touched. A press becomes that drag as DragStart says
// (src/dragstart.h: some way moved, the button held a moment), so that a click made in
// passing does not move a cue in the file. A click that does not drag puts the playhead
// at its time. Its own menu (the right button) is Main's (showTimelineCueMenu). A cue
// that has been recorded and not yet written is drawn paler, and is not moved: it is
// not in the file yet to be moved in it.
Item {
    id: block

    required property var panel
    required property var cue
    // How long it lasts on the timeline, in seconds
    property real length: 0
    property color fill: "#5a5d66"
    property color ink: "white"
    // Its words: a line at the top (a group's name), and one under it (its own)
    property string upper: ""
    property string lower: ""
    // An action's picture, by its kind and its own (see ActionGlyph)
    property string glyph: ""
    property string picture: ""

    // How far it has been dragged, in seconds
    property real dragged: 0
    readonly property bool movable: (cue.place ?? -1) >= 0
    readonly property real at: Math.max(0, Math.min(panel.duration, cue.time + dragged))

    objectName: "timelineCue"
    x: Math.round(panel.x0(at))
    width: Math.max(3, Math.round(panel.x0(at + length)) - x)
    opacity: cue.recorded === true ? 0.6 : 1
    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 1
        color: block.fill
        border.width: mouse.pressed ? 1 : 0
        border.color: "white"
    }

    // Where one cue ends and the next begins, on a run of one colour
    Rectangle {
        width: 1
        height: parent.height - 1
        color: Qt.rgba(1, 1, 1, 0.75)
    }

    ActionGlyph {
        id: picture

        x: 5
        anchors.verticalCenter: parent.verticalCenter
        visible: block.glyph !== ""
        kind: block.glyph
        picture: block.picture
        ink: block.ink
    }

    Column {
        x: block.glyph !== "" ? picture.x + picture.width + 5 : 6
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - 4
        spacing: 1

        Text {
            width: parent.width
            visible: block.upper !== ""
            elide: Text.ElideRight
            color: block.ink
            font.pixelSize: 12
            font.weight: Font.DemiBold
            text: block.upper
        }

        Text {
            width: parent.width
            elide: Text.ElideRight
            color: block.ink
            font.pixelSize: 12
            text: block.lower
        }
    }

    MouseArea {
        id: mouse

        // Where it was pressed, in the panel, and when (DragStart.now())
        property point pressedAt
        property real pressedTime: 0
        property bool moved: false

        // Follows the pointer, at `event`, once the press has become a drag. (Only the way
        // along the track counts: it moves no other way.)
        function follow(event) {
            const at = mapToItem(block.panel, event.x, event.y)
            if (!moved && !DragStart.begins(pressedAt, Qt.point(at.x, pressedAt.y), pressedTime))
                return
            moved = true
            block.dragged = (at.x - pressedAt.x) / block.panel.pps
        }

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: block.movable ? Qt.SizeHorCursor : Qt.ArrowCursor
        onPressed: (event) => {
            if (event.button === Qt.RightButton) {
                block.panel.win.showTimelineCueMenu(block.cue, block, event.x, event.y)
                return
            }
            pressedAt = mapToItem(block.panel, event.x, event.y)
            pressedTime = DragStart.now()
            moved = false
        }
        onPositionChanged: (event) => {
            if ((pressedButtons & Qt.LeftButton) && block.movable)
                follow(event)
        }
        onReleased: (event) => {
            if (event.button !== Qt.LeftButton)
                return
            // (Let go after the moment and the distance, without a move since: moved there)
            if (block.movable)
                follow(event)
            if (moved) {
                const to = block.at
                block.dragged = 0
                // (Brought back to where it was, or held against an end it was at: the
                // file is left alone)
                if (to !== block.cue.time)
                    block.panel.win.moveTimelineCue(block.cue, to)
            } else {
                block.panel.win.seekTimeline(block.cue.time)
            }
        }
        onCanceled: block.dragged = 0
    }
}
