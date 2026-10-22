import QtQuick
import QtQuick.Shapes

// A calendar's page, for the Calendar: its two rings at the top, a line under its
// heading, and four days marked. This app's own drawing, ProPresenter's pack having no
// picture of its calendar (see icons/README.md). Drawn in `ink`, in a box eighteen wide
// and fourteen high (the toolbar's), and to any other size by `scale`: the toolbar's
// button, and the mark beside a playlist an event goes live from.
Item {
    id: glyph

    property color ink: "#e6e6e6"

    implicitWidth: 18
    implicitHeight: 14

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: glyph.ink
            strokeWidth: 1.5
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            // The page
            startX: 3.5
            startY: 2.5
            PathLine { x: 14.5; y: 2.5 }
            PathArc { x: 16; y: 4; radiusX: 1.5; radiusY: 1.5 }
            PathLine { x: 16; y: 12 }
            PathArc { x: 14.5; y: 13.5; radiusX: 1.5; radiusY: 1.5 }
            PathLine { x: 3.5; y: 13.5 }
            PathArc { x: 2; y: 12; radiusX: 1.5; radiusY: 1.5 }
            PathLine { x: 2; y: 4 }
            PathArc { x: 3.5; y: 2.5; radiusX: 1.5; radiusY: 1.5 }
            // Under its heading
            PathMove { x: 2; y: 5.8 }
            PathLine { x: 16; y: 5.8 }
            // Its two rings
            PathMove { x: 6; y: 0.8 }
            PathLine { x: 6; y: 3.8 }
            PathMove { x: 12; y: 0.8 }
            PathLine { x: 12; y: 3.8 }
        }

        // Four days
        ShapePath {
            fillColor: glyph.ink
            strokeColor: "transparent"

            startX: 5
            startY: 8
            PathLine { x: 7; y: 8 }
            PathLine { x: 7; y: 9.6 }
            PathLine { x: 5; y: 9.6 }
            PathLine { x: 5; y: 8 }
            PathMove { x: 8; y: 8 }
            PathLine { x: 10; y: 8 }
            PathLine { x: 10; y: 9.6 }
            PathLine { x: 8; y: 9.6 }
            PathLine { x: 8; y: 8 }
            PathMove { x: 11; y: 8 }
            PathLine { x: 13; y: 8 }
            PathLine { x: 13; y: 9.6 }
            PathLine { x: 11; y: 9.6 }
            PathLine { x: 11; y: 8 }
            PathMove { x: 5; y: 10.8 }
            PathLine { x: 7; y: 10.8 }
            PathLine { x: 7; y: 12.2 }
            PathLine { x: 5; y: 12.2 }
            PathLine { x: 5; y: 10.8 }
        }
    }
}
