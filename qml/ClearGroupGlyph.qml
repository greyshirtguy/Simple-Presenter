import QtQuick
import QtQuick.Shapes
import "clearglyphs.js" as Glyphs

// The picture on a clear group's button. ProPresenter has thirty-two of them to choose
// from, which the file names by number (see src/clearfile.h): the digits, the crossed
// circle called All, and thirty things from a megaphone to an exclamation mark; and a
// group can have a picture of the user's own instead, kept in the file.
//
// `icon` is the number, `image` the picture of the user's own (an address an Image can
// be given) for number 0, and it is drawn in `ink` in a square `size` on a side.
//
// What is drawn. For the two that ProPresenter's pack has a picture of (All, and the
// message) it is that picture (see ProIcon). A digit, and the exclamation mark, is the
// character. The rest are DRAWN HERE, as plain line drawings of the thing each is
// called: a bell is a bell. They are this app's own and are not ProPresenter's
// drawings, which the pack does not have; should the pack come to have them, they
// belong with `pack` below. A number this does not know, and a picture of the user's own
// that cannot be shown, is the crossed circle.
//
// The drawings are on a grid of twenty-four, the thing about seventeen high in the
// middle of it, which is how big ProPresenter's own picture is in its square: so one
// of each, side by side, are of a size.
Item {
    id: glyph

    property int icon: 11
    property string image: ""
    property color ink: "#ececec"
    property real size: 24

    readonly property string character: icon >= 1 && icon <= 9 ? String(icon) : icon === 10 ? "0" : icon === 32 ? "!" : ""
    readonly property bool own: icon === 0 && image !== ""
    // One of the drawings made here, { lines, solid }, or null (see clearglyphs.js)
    readonly property var drawing: Glyphs.drawing(icon)
    // ProPresenter's own picture of it, by name, where the pack has one, which is for
    // All (11) and the message (29); and the crossed circle for what there is nothing
    // else to draw for
    readonly property string pack: own || character !== "" || drawing !== null ? "" : icon === 29 ? "Message" : "Clear"

    width: size
    height: size

    // ProPresenter's own picture
    ProIcon {
        anchors.centerIn: parent
        visible: glyph.pack !== ""
        name: glyph.pack
        ink: glyph.ink
        size: glyph.size
    }

    // A digit, or the exclamation mark
    Text {
        anchors.centerIn: parent
        visible: glyph.character !== ""
        color: glyph.ink
        font.pixelSize: glyph.size * 0.7
        font.bold: true
        text: glyph.character
    }

    // One of the drawings. (Made only for a picture that is one: the group every
    // workspace starts with has ProPresenter's own picture, and most windows never
    // draw another.)
    Loader {
        active: glyph.drawing !== null

        sourceComponent: Shape {
            width: 24
            height: 24
            scale: glyph.size / 24
            transformOrigin: Item.TopLeft
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: glyph.ink
                strokeWidth: 1.7
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: glyph.drawing ? glyph.drawing.lines : ""
                }
            }

            ShapePath {
                strokeColor: "transparent"
                strokeWidth: 0
                fillColor: glyph.ink

                PathSvg {
                    path: glyph.drawing ? glyph.drawing.solid : ""
                }
            }
        }
    }

    // A picture of the user's own, as it is: it has its own colours
    Loader {
        anchors.centerIn: parent
        active: glyph.own

        sourceComponent: Image {
            width: glyph.size * 0.75
            height: glyph.size * 0.75
            source: glyph.image
            sourceSize: Qt.size(Math.round(glyph.size * 2), Math.round(glyph.size * 2))
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }
    }
}
