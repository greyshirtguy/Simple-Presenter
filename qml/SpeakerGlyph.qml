import QtQuick
import QtQuick.Shapes

// Whether a background video is heard, for the icon under a slide's other media icons: a
// speaker, sending out its sound while it is heard, and struck through, corner to corner,
// while it is muted, which is what a background is unless its sound is turned on (this
// app's own: see src/backgroundsoundfile.h). ProPresenter's pack of pictures has no
// speaker (its audio is its two notes, see icons/README.md), so this is drawn here.
//
// A Shape and nothing else, not an ActionGlyph with its Loaders for every kind: a song
// with a background video on each of its slides has one of these on every thumbnail.
// It takes up a box twelve square, as an ActionGlyph does, drawn in `ink`; the line
// struck through has a line of the icon's own grey either side, so that it reads over
// the speaker.
Shape {
    property bool muted: true
    property color ink: "#e3e5e9"

    width: 12
    height: 12
    preferredRendererType: Shape.CurveRenderer

    // The speaker
    ShapePath {
        strokeColor: "transparent"
        fillColor: ink
        PathSvg {
            path: "M 1 4.3 H 3.6 L 7 1.4 V 10.6 L 3.6 7.7 H 1 Z"
        }
    }
    // Its sound, going out of it while it is heard
    ShapePath {
        strokeColor: muted ? "transparent" : ink
        strokeWidth: 1.2
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        PathSvg {
            path: "M 8.7 4.3 A 2.3 2.3 0 0 1 8.7 7.7 M 9.9 2.6 A 4.3 4.3 0 0 1 9.9 9.4"
        }
    }
    // Struck through while it is muted: the icon's grey (ActionIcon's colour), and the line
    ShapePath {
        strokeColor: muted ? "#383b42" : "transparent"
        strokeWidth: 3.4
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        PathSvg {
            path: "M 1.2 11 L 11 1.2"
        }
    }
    ShapePath {
        strokeColor: muted ? ink : "transparent"
        strokeWidth: 1.4
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        PathSvg {
            path: "M 1.2 11 L 11 1.2"
        }
    }
}
