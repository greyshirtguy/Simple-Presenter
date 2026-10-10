import QtQuick
import QtQuick.Shapes

// The small picture of what an action does, for an ActionIcon and for the lists of
// actions. `kind` is the kind of action, as they are named (see src/actions.h):
// "timer", "clear", "look", "stage", "prop", "macro" or "other".
//
// For all but the last ProPresenter has a picture of its own, and that is what is
// drawn (see ProIcon): its timer, its crossed circle, its glasses and moustache for a
// look, its speaker at a lectern for the stage, its pile of layers for a prop, its M
// in brackets. An action can say which picture is its own more exactly than its kind
// does, in `picture`: one that clears the media has ProPresenter's picture of clearing
// the media and not the plain crossed circle (see actions::describe, which chooses).
// "other" is three dots.
//
// And, for how a video plays on from its end, "stop" (a square) or "loop" (an arrow
// going round), which are this app's own.
//
// It takes up a box twelve high and twelve wide, sixteen wide for a picture that is
// wider than it is high, and is drawn in `ink`.
Item {
    id: glyph

    property string kind
    // ProPresenter's picture for it, by name, where the action has said (see above)
    property string picture: ""
    property color ink: "#e3e5e9"

    // The picture for a kind that has not said which is its own
    readonly property var pictures: ({ timer: "Countdown", clear: "Clear", look: "Looks", stage: "Stage", prop: "Prop", macro: "Macro" })
    readonly property string shown: picture !== "" ? picture : (pictures[kind] ?? "")
    readonly property bool wide: shown === "Looks" || shown === "Macro" || shown === "ClearVideoInput" || shown === "ClearPresentation"

    width: wide ? 16 : 12
    height: 12

    // ProPresenter's picture. Its drawing is the middle of the square it is asked for
    // in, so the square overhangs this box all round and the drawing fills it.
    ProIcon {
        anchors.centerIn: parent
        visible: glyph.shown !== ""
        name: glyph.shown
        ink: glyph.ink
        size: 18
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
