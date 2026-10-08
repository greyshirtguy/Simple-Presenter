import QtQuick
import QtQuick.Shapes

// The small picture of what an action does, for an ActionIcon and for the lists of
// actions: `kind` is "timer" (a stopwatch), "clear" (a crossed circle), "stage" (a
// screen on its stand), "prop" (something laid over a corner), "macro" (an M in
// brackets: see MacroGlyph) or "other" (three dots), as the kinds of action are named
// (see src/actions.h); or, for how a video plays on from its end, "stop" (a square) or
// "loop" (an arrow going round). Drawn in a box twelve high and twelve wide, sixteen
// for a macro, in `ink`.
Item {
    id: glyph

    property string kind
    property color ink: "#e3e5e9"

    width: kind === "macro" ? 16 : 12
    height: 12

    // A stopwatch
    Item {
        width: 12
        height: 12
        visible: glyph.kind === "timer"

        Rectangle {
            x: 4.5
            width: 3
            height: 1.5
            color: glyph.ink
        }

        Rectangle {
            y: 2
            width: 12
            height: 10
            radius: 5
            color: "transparent"
            border.width: 1.3
            border.color: glyph.ink
        }

        Rectangle {
            x: 5.4
            y: 4
            width: 1.2
            height: 3.6
            color: glyph.ink
        }

        Rectangle {
            x: 5.4
            y: 6.6
            width: 3
            height: 1.2
            color: glyph.ink
        }
    }

    // A crossed circle
    Rectangle {
        anchors.fill: parent
        visible: glyph.kind === "clear"
        radius: 6
        color: "transparent"
        border.width: 1.3
        border.color: glyph.ink

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 1.3
            rotation: 45
            color: glyph.ink
        }

        Rectangle {
            anchors.centerIn: parent
            width: 7
            height: 1.3
            rotation: -45
            color: glyph.ink
        }
    }

    // A screen on its stand
    Item {
        anchors.fill: parent
        visible: glyph.kind === "stage"

        Rectangle {
            width: 12
            height: 8
            radius: 1.5
            color: "transparent"
            border.width: 1.3
            border.color: glyph.ink
        }

        Rectangle {
            x: 5.4
            y: 8
            width: 1.2
            height: 2.6
            color: glyph.ink
        }

        Rectangle {
            x: 3
            y: 10.5
            width: 6
            height: 1.3
            color: glyph.ink
        }
    }

    // Something laid over the corner of the picture
    Rectangle {
        y: 1.5
        width: 12
        height: 9
        visible: glyph.kind === "prop"
        radius: 1.5
        color: "transparent"
        border.width: 1.3
        border.color: glyph.ink

        Rectangle {
            x: 6
            y: 4.3
            width: 4
            height: 2.6
            radius: 0.6
            color: glyph.ink
        }
    }

    Loader {
        active: glyph.kind === "macro"

        sourceComponent: MacroGlyph {
            ink: glyph.ink
        }
    }

    Text {
        anchors.centerIn: parent
        visible: glyph.kind === "other"
        color: glyph.ink
        font.pixelSize: 11
        font.bold: true
        text: "…"
    }

    // Plays once and stops
    Rectangle {
        anchors.centerIn: parent
        width: 7
        height: 7
        radius: 1
        visible: glyph.kind === "stop"
        color: glyph.ink
    }

    // Goes round: most of a circle, with an arrowhead where it closes
    Loader {
        anchors.fill: parent
        active: glyph.kind === "loop"

        sourceComponent: Shape {
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: glyph.ink
                strokeWidth: 1.4
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M 10.2 3.6 A 4.6 4.6 0 1 0 10.6 7.2 M 10.6 0.9 V 3.9 H 7.6"
                }
            }
        }
    }
}
