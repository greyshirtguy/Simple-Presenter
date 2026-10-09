import QtQuick
import QtQuick.Shapes

// A button of the editor's toolbar, drawn as a plain light-grey glyph. `kind` picks it:
// "text" is a T, for a text box; "shapes" is a square and a circle, with a mark that it
// opens a list; "media" is a picture of two mountains; "duplicate" is one sheet over
// another; "delete" is a bin; "undo" and "redo" are arrows that turn back and forward.
// A glyph says less than a word does, so each button also has a few words (`hint`)
// that the editor shows along its bottom edge while the pointer is on it.
Rectangle {
    id: tool

    property string kind
    property string hint
    property bool available: true
    // A tool that is drawn as a short word and not a glyph (the editor's three ways of
    // working, which no picture says), and whether it is the one that is on
    property string label
    property bool on: false
    // Whether it opens a list of things to choose from
    readonly property bool opens: kind === "shapes"
    readonly property bool hovered: mouse.containsMouse
    readonly property color ink: available ? "#d0d2d6" : "#5c5f66"
    // Each glyph is lines to draw, and for some a part to fill in, on an 18 by 16 grid
    readonly property var glyphs: ({
        "shapes": { lines: "M 1.5 5.5 H 10.5 V 14.5 H 1.5 Z M 16.5 6 A 4.5 4.5 0 1 1 7.5 6 A 4.5 4.5 0 1 1 16.5 6 Z" },
        "media": { lines: "M 1.5 2.5 H 16.5 V 13.5 H 1.5 Z", solid: "M 3 12 L 7 6.5 L 9.8 10 L 11.8 7.8 L 15 12 Z M 13.6 5.2 A 1.1 1.1 0 1 1 11.4 5.2 A 1.1 1.1 0 1 1 13.6 5.2 Z" },
        "duplicate": { lines: "M 5.5 5.5 H 15.5 V 14.5 H 5.5 Z M 2.5 11 V 1.5 H 12" },
        "delete": { lines: "M 3 4.5 H 15 M 7 4.5 V 2.5 H 11 V 4.5 M 4.5 4.5 L 5.5 14.5 H 12.5 L 13.5 4.5 M 7.5 7 V 12 M 10.5 7 V 12" },
        "undo": { lines: "M 6 3 L 2.5 6.5 L 6 10 M 2.5 6.5 H 11 A 4 4 0 0 1 11 14.5 H 7" },
        "redo": { lines: "M 12 3 L 15.5 6.5 L 12 10 M 15.5 6.5 H 7 A 4 4 0 0 0 7 14.5 H 11" }
    })
    readonly property var glyph: glyphs[kind] ?? ({})

    signal clicked

    width: label !== "" ? word.implicitWidth + 22 : opens ? 44 : 34
    height: 30
    radius: 6
    color: on ? "#ff8a1f" : !available ? "#2b2d31" : mouse.pressed ? "#50535a" : mouse.containsMouse ? "#45484e" : "#3a3c42"

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -0.5
        visible: tool.kind === "text"
        color: tool.ink
        font.pixelSize: 18
        font.weight: Font.Medium
        text: "T"
    }

    Text {
        id: word

        anchors.centerIn: parent
        visible: tool.label !== ""
        color: tool.on ? "#1b1c1f" : tool.ink
        font.pixelSize: 13
        font.weight: Font.Medium
        text: tool.label
    }

    Shape {
        x: tool.opens ? 8 : (parent.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 18
        height: 16
        visible: tool.kind !== "text" && tool.label === ""
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: tool.ink
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: tool.glyph.lines ?? ""
            }
        }

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: tool.glyph.solid ? tool.ink : "transparent"

            PathSvg {
                path: tool.glyph.solid ?? ""
            }
        }
    }

    // The mark of a list: a small arrowhead pointing down
    Shape {
        x: parent.width - 13
        anchors.verticalCenter: parent.verticalCenter
        width: 8
        height: 5
        visible: tool.opens
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: tool.ink
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: "M 1 1 L 4 4 L 7 1"
            }
        }
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: tool.available
        onClicked: tool.clicked()
    }
}
