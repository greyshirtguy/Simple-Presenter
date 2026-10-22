import QtQuick
import QtQuick.Shapes

// The pictures of the audio bin, which are this app's own drawings: ProPresenter's
// pack has a picture of clearing the audio and none of the audio itself (see
// icons/README.md), so these are drawn to go with that one, which is two eighth notes
// joined by a beam with a crossed circle over them. (The tabs of the audio bin and of
// the music's transport have ProPresenter's own picture of the notes, which the
// maintainer gave: Audio, in icons/. These are for the rest: a row of the bin, and an
// action's icon.)
//
//   music   two eighth notes joined by a beam: a piece of music
//   sound   a jagged line, as a short loud sound is drawn: a sound effect
//   next    an arrowhead running up against a bar: a piece that is followed by the
//           next of its playlist
//
// (How a piece plays on from its end has two more pictures, a square for one that
// stops and an arrow going round for one that loops, which a video on a slide has
// already: see ActionGlyph.)
//
// It is drawn in `ink`, in a square `size` on a side, the drawing filling most of it.
Item {
    id: glyph

    property string kind: "music"
    property color ink: "#e3e5e9"
    property real size: 24

    width: size
    height: size

    // (Drawn in a box of 24 by 24, and made the size asked for.)
    Shape {
        width: 24
        height: 24
        scale: glyph.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        // What is filled in: the notes, their stems and their beam; the arrowhead and
        // its bar
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: glyph.kind === "music" || glyph.kind === "next" ? glyph.ink : "transparent"

            PathSvg {
                path: glyph.kind === "music"
                      ? "M 2.6 18.3 a 3.6 2.9 0 1 0 7.2 0 a 3.6 2.9 0 1 0 -7.2 0 Z M 8.4 18.3 V 5.2 H 9.8 V 18.3 Z "
                        + "M 13.8 15.8 a 3.6 2.9 0 1 0 7.2 0 a 3.6 2.9 0 1 0 -7.2 0 Z M 19.6 15.8 V 2.7 H 21 V 15.8 Z "
                        + "M 8.4 4.6 L 21 2.1 V 6.5 L 8.4 9 Z"
                      : glyph.kind === "next" ? "M 5 5 L 15.5 12 L 5 19 Z M 16.6 5 H 19.4 V 19 H 16.6 Z" : ""
            }
        }

        // What is a line: the jagged one
        ShapePath {
            strokeColor: glyph.kind === "sound" ? glyph.ink : "transparent"
            strokeWidth: 1.9
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: glyph.kind === "sound" ? "M 1.8 12 H 4.6 L 7 6.4 L 10 18.6 L 13 3.4 L 16 20.6 L 18.4 8.4 L 20 12 H 22.2" : ""
            }
        }
    }
}
