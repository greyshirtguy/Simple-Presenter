import QtQuick
import QtQuick.Window

// Strips along the edges and corners of a frameless window that resize it. Fill the
// window with this, above its other content.
Item {
    id: grips

    required property Window target

    visible: target.visibility === Window.Windowed
    z: 100

    component Grip: MouseArea {
        required property int edges

        cursorShape: edges === Qt.LeftEdge || edges === Qt.RightEdge ? Qt.SizeHorCursor
                   : edges === Qt.TopEdge || edges === Qt.BottomEdge ? Qt.SizeVerCursor
                   : edges === (Qt.TopEdge | Qt.LeftEdge) || edges === (Qt.BottomEdge | Qt.RightEdge) ? Qt.SizeFDiagCursor
                   : Qt.SizeBDiagCursor
        onPressed: grips.target.startSystemResize(edges)
    }

    Grip { edges: Qt.LeftEdge; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 5 }
    Grip { edges: Qt.RightEdge; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 5 }
    Grip { edges: Qt.TopEdge; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 5 }
    Grip { edges: Qt.BottomEdge; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; height: 5 }
    Grip { edges: Qt.TopEdge | Qt.LeftEdge; anchors.top: parent.top; anchors.left: parent.left; width: 10; height: 10 }
    Grip { edges: Qt.TopEdge | Qt.RightEdge; anchors.top: parent.top; anchors.right: parent.right; width: 10; height: 10 }
    Grip { edges: Qt.BottomEdge | Qt.LeftEdge; anchors.bottom: parent.bottom; anchors.left: parent.left; width: 10; height: 10 }
    Grip { edges: Qt.BottomEdge | Qt.RightEdge; anchors.bottom: parent.bottom; anchors.right: parent.right; width: 10; height: 10 }
}
