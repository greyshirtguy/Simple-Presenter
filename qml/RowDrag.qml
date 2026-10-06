import QtQuick

// What is being dragged out of a list: a point that follows the pointer, with the name
// of the dragged entry beside it. A list is given one of these as its `dragProxy`
// (see SidebarList) and moves it; the things that can be dropped on look at its
// `entry` and its Drag.keys. It has to be a child of something that covers both where
// drags start and where they end, so that one set of coordinates serves both.
Item {
    id: rowDrag

    property var entry: null

    z: 50

    Rectangle {
        x: 12
        y: 8
        width: dragLabel.implicitWidth + 20
        height: 28
        radius: 6
        visible: rowDrag.Drag.active
        color: "#2b2d31"
        border.width: 1
        border.color: "#ff8a1f"
        opacity: 0.95

        Text {
            id: dragLabel

            anchors.centerIn: parent
            color: "#e6e6e6"
            font.pixelSize: 13
            text: rowDrag.entry ? rowDrag.entry.name : ""
        }
    }
}
