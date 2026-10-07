import QtQuick
import QtQuick.Shapes

// The way back out of Simple View: a small button that floats over the top of the slides.
//
// Simple View takes away everything round the slides, the toolbar with its button for
// the view included. A view that can be got into by a key held a moment too long, and
// that hides its own way out, is one to be lost in, so this is made hard to miss and
// then easy to ignore:
//
//   - When the view is entered it is large, in the app's one bright colour, and says in
//     words what has happened and how to undo it. That is what a web browser does on
//     going full screen, and for the same reason. The change of size is itself the cue:
//     of everything on the screen it is the one thing that moves.
//   - After a few seconds the words fade and it shrinks to a small button, faint enough
//     not to draw the eye from the slides. Like the other controls that sit over the
//     slides it comes up in full under the pointer, and there says again what it does.
//   - It is where the toolbar's own button for the view is, so the one place on the
//     screen switches the view on and off, and a second click undoes the first.
//
// While the key that switches the view is being held, a line along its foot shows how
// far the hold has got (as the toolbar's button does on the way in).
Item {
    id: toggle

    // Whether it is making itself known: large, with its words
    property bool announcing: false
    // How far a hold of the key has got, from 0 to 1
    property real progress: 0
    // How long it makes itself known for, in milliseconds
    property int announceFor: 5000
    readonly property bool hovered: mouse.containsMouse
    // At rest, when it is neither making itself known nor under the pointer
    readonly property bool resting: !announcing && !hovered && progress === 0
    readonly property color bright: "#ff8a1f"

    signal clicked

    // Makes itself known, for a while: called when the view is entered.
    function announce() {
        announcing = true
        quiet.restart()
        arrive.restart()
    }

    width: pill.width
    height: pill.height

    Timer {
        id: quiet

        interval: toggle.announceFor
        onTriggered: toggle.announcing = false
    }

    Rectangle {
        id: pill

        anchors.right: parent.right
        width: toggle.announcing ? words.implicitWidth + 58 : toggle.hovered ? tip.implicitWidth + 46 : 30
        height: toggle.announcing ? 32 : 26
        radius: height / 2
        color: toggle.announcing ? toggle.bright : mouse.pressed ? "#6a6d75" : "#3a3c42"
        border.width: 1
        border.color: toggle.announcing ? toggle.bright : "#6c6f75"
        opacity: toggle.resting ? 0.5 : 1
        clip: true
        transformOrigin: Item.Right

        Behavior on width {
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }
        Behavior on height {
            NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
        }
        Behavior on color {
            ColorAnimation { duration: 260 }
        }
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }

        // It arrives larger still, and settles: the eye goes to what moves.
        NumberAnimation {
            id: arrive

            target: pill
            property: "scale"
            from: 1.35
            to: 1
            duration: 320
            easing.type: Easing.OutCubic
        }

        // What it says while it is making itself known
        Text {
            id: words

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            opacity: toggle.announcing ? 1 : 0
            visible: opacity > 0
            color: "#15161a"
            font.pixelSize: 14
            textFormat: Text.StyledText
            text: "<b>Simple View</b> &nbsp;·&nbsp; click here, or hold the ~ key, to go back"

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }

        // And what it says under the pointer
        Text {
            id: tip

            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            opacity: toggle.hovered && !toggle.announcing ? 1 : 0
            visible: opacity > 0
            color: "#e6e6e6"
            font.pixelSize: 12
            text: "Back to the normal view (or hold ~)"

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }

        // Four corners turned in on the middle: the sign for leaving full screen, which
        // is what this is the like of
        Shape {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 12
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: toggle.announcing ? "#15161a" : "#e6e6e6"
                strokeWidth: 1.6
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin

                PathMultiline {
                    paths: [
                        [Qt.point(0, 4), Qt.point(4, 4), Qt.point(4, 0)],
                        [Qt.point(14, 4), Qt.point(10, 4), Qt.point(10, 0)],
                        [Qt.point(0, 8), Qt.point(4, 8), Qt.point(4, 12)],
                        [Qt.point(14, 8), Qt.point(10, 8), Qt.point(10, 12)]
                    ]
                }
            }
        }

    }

    // How far a hold of the key has got: a line under it, as under the toolbar's button
    Rectangle {
        anchors.left: pill.left
        anchors.top: pill.bottom
        anchors.leftMargin: 4
        anchors.topMargin: 2
        width: (pill.width - 8) * toggle.progress
        height: 2
        visible: toggle.progress > 0
        color: toggle.bright
    }

    MouseArea {
        id: mouse

        anchors.fill: pill
        hoverEnabled: true
        onClicked: toggle.clicked()
    }
}
