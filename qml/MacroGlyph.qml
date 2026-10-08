import QtQuick
import QtQuick.Shapes

// The picture of a macro: a letter between square brackets that hug it, their corners a
// little rounded. The letter is M unless the macro has a letter or a digit of its own
// for a picture. It is drawn to fill a box sixteen wide and twelve high, times `size`.
Item {
    id: glyph

    property color ink: "#e3e5e9"
    property string letter: "M"
    property real size: 1

    width: 16 * size
    height: 12 * size

    Shape {
        width: 16
        height: 12
        scale: glyph.size
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: glyph.ink
            strokeWidth: 1.3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M 3.4 0.8 H 2 Q 0.8 0.8 0.8 2 V 10 Q 0.8 11.2 2 11.2 H 3.4 M 12.6 0.8 H 14 Q 15.2 0.8 15.2 2 V 10 Q 15.2 11.2 14 11.2 H 12.6"
            }
        }
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 0.3 * glyph.size
        color: glyph.ink
        font.pixelSize: 9.5 * glyph.size
        font.bold: true
        text: glyph.letter !== "" ? glyph.letter : "M"
    }
}
