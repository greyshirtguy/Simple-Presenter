import QtQuick
import QtQuick.Shapes

// A small square button drawn as a glyph, in the app's dark style. `kind` picks the
// glyph: "bold", "italic", "underline" and "strike" are letters; "alignLeft",
// "alignCenter", "alignRight" and "alignJustify" are lines of text; "alignTop",
// "alignMiddle" and "alignBottom" are a block against an edge; "eye" and "lock" are for
// the rows of a list; "sliders" is three sliders, for things to adjust; "play", "pause",
// "stop" and "restart" (back to the start) are for something that runs; anything else
// shows `text`. `on` draws it as switched on. Never takes keyboard focus.
Rectangle {
    id: button

    property string kind
    property string text
    property bool on: false
    property bool available: true
    // Without a background until hovered, for use in a list row
    property bool flat: false
    // What the glyph is drawn in
    readonly property color ink: !available ? "#6c6f75" : on && !flat ? "black" : on || !flat ? "#e6e6e6" : "#7d8088"

    signal clicked

    width: 28
    height: 26
    radius: 5
    color: !available ? (flat ? "transparent" : "#2b2d31")
         : on && !flat ? "#ff8a1f"
         : mouse.pressed ? "#50535a" : mouse.containsMouse ? "#45484e" : flat ? "transparent" : "#3a3c42"

    // Letters, and anything given as text
    Text {
        anchors.centerIn: parent
        visible: text !== ""
        color: button.ink
        font.pixelSize: 14
        font.bold: button.kind === "bold"
        font.italic: button.kind === "italic"
        font.underline: button.kind === "underline"
        font.strikeout: button.kind === "strike"
        font.family: button.kind === "italic" ? "serif" : Qt.application.font.family
        text: button.kind === "bold" ? "B" : button.kind === "italic" ? "I" : button.kind === "underline" ? "U"
            : button.kind === "strike" ? "S" : button.text
    }

    // Lines of text, set left, centred, right or to both edges
    Column {
        anchors.centerIn: parent
        spacing: 2
        visible: button.kind === "alignLeft" || button.kind === "alignCenter" || button.kind === "alignRight"
                 || button.kind === "alignJustify"

        Repeater {
            model: button.kind === "alignJustify" ? [14, 14, 14, 14] : [14, 8, 12, 6]

            delegate: Item {
                required property int modelData

                width: 14
                height: 2

                Rectangle {
                    x: button.kind === "alignCenter" ? (14 - width) / 2 : button.kind === "alignRight" ? 14 - width : 0
                    width: parent.modelData
                    height: 2
                    color: button.ink
                }
            }
        }
    }

    // A block of text against the top, the middle or the bottom of its box
    Item {
        anchors.centerIn: parent
        width: 14
        height: 14
        visible: button.kind === "alignTop" || button.kind === "alignMiddle" || button.kind === "alignBottom"

        Rectangle {
            y: button.kind === "alignTop" ? 0 : button.kind === "alignMiddle" ? 6.25 : 12.5
            width: 14
            height: 1.5
            color: button.ink
        }

        Rectangle {
            x: 4
            y: button.kind === "alignTop" ? 3.5 : button.kind === "alignMiddle" ? 3 : 2.5
            width: 6
            height: 8
            radius: 1
            color: button.ink
            opacity: button.kind === "alignMiddle" ? 0.75 : 1
        }
    }

    // Three sliders, their knobs at different places
    Column {
        anchors.centerIn: parent
        spacing: 2.5
        visible: button.kind === "sliders"

        Repeater {
            model: [0.2, 0.7, 0.4]

            delegate: Item {
                id: slider

                required property real modelData

                width: 15
                height: 3

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 1.5
                    color: button.ink
                }

                Rectangle {
                    x: slider.modelData * (parent.width - width)
                    width: 3
                    height: 3
                    radius: 1.5
                    color: button.ink
                }
            }
        }
    }

    // For something that runs: a triangle to play, two bars to pause, a square to stop,
    // and a bar with a triangle up against it to go back to the start. Only made for a
    // button of one of those kinds, the triangles being shapes and not rectangles.
    Loader {
        anchors.centerIn: parent
        active: button.kind === "play" || button.kind === "pause" || button.kind === "stop" || button.kind === "restart"

        sourceComponent: Item {
            width: 12
            height: 12

            Shape {
                anchors.fill: parent
                visible: button.kind === "play" || button.kind === "restart"
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    fillColor: button.ink
                    strokeColor: "transparent"

                    PathPolyline {
                        path: button.kind === "play" ? [Qt.point(1.5, 0), Qt.point(11.5, 6), Qt.point(1.5, 12), Qt.point(1.5, 0)]
                                                     : [Qt.point(12, 0), Qt.point(3.5, 6), Qt.point(12, 12), Qt.point(12, 0)]
                    }
                }
            }

            Rectangle {
                visible: button.kind === "restart"
                width: 2.5
                height: 12
                color: button.ink
            }

            Repeater {
                model: button.kind === "pause" ? [1, 7.5] : []

                delegate: Rectangle {
                    required property real modelData

                    x: modelData
                    width: 3.5
                    height: 12
                    color: button.ink
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                visible: button.kind === "stop"
                radius: 1
                color: button.ink
            }
        }
    }

    // An eye, struck through when off
    Item {
        anchors.centerIn: parent
        width: 16
        height: 10
        visible: button.kind === "eye"

        Rectangle {
            anchors.fill: parent
            radius: 5
            color: "transparent"
            border.width: 1.5
            border.color: button.ink
        }

        Rectangle {
            anchors.centerIn: parent
            width: 4.5
            height: 4.5
            radius: 2.25
            color: button.ink
        }

        Rectangle {
            anchors.centerIn: parent
            width: 19
            height: 1.5
            rotation: -35
            visible: !button.on
            color: button.ink
        }
    }

    // A padlock, its shackle swung open when off
    Item {
        anchors.centerIn: parent
        width: 12
        height: 14
        visible: button.kind === "lock"

        Rectangle {
            x: button.on ? 2.5 : 6
            y: 0
            width: 7
            height: 10
            radius: 3.5
            color: "transparent"
            border.width: 1.5
            border.color: button.ink
        }

        Rectangle {
            x: 0
            y: 6
            width: 12
            height: 8
            radius: 2
            color: button.ink
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: button.available
        onClicked: button.clicked()
    }
}
