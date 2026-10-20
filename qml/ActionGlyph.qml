import QtQuick
import QtQuick.Shapes

// The small picture of what an action does, for an ActionIcon and for the lists of
// actions. `kind` is the kind of action, as they are named (see src/actions.h):
// "timer", "clear", "look", "stage", "prop", "macro", "message", "mediaPlaylist",
// "audioPlaylist" or "other".
//
// For the first seven ProPresenter has a picture of its own, and that is what is
// drawn (see ProIcon): its timer, its crossed circle, its glasses and moustache for a
// look, its speaker at a lectern for the stage, its pile of layers for a prop, its M
// in brackets, its speech bubble for a message. Its pack has none for a playlist of
// either bin (see icons/README.md), so those two are this app's own: what the bin
// holds beside three short lines, as a list is drawn in the bins' own lists. A
// picture in a frame, with a play mark in it, for the media bin; the audio bin's two
// joined notes for the audio bin. An action can say which picture is its own more exactly than its kind
// does, in `picture`: one that clears the media has ProPresenter's picture of clearing
// the media and not the plain crossed circle (see actions::describe, which chooses).
// "other" is three dots, and so, for now, is "communication": ProPresenter has a picture
// for an action that tells a device something (a mast sending out its rings), but the
// pack of its pictures here does not (see icons/README.md).
//
// And, for how a video or a piece of audio plays on from its end, "stop" (a square),
// "loop" (an arrow going round) or "next" (an arrowhead against a bar: the next of its
// playlist follows), which are this app's own. So are the two for an action that plays
// a piece of audio, "audio": two joined notes for music, and a jagged line for one
// whose `picture` says it is a sound effect ("SoundEffect"). See AudioGlyph.
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
    readonly property var pictures: ({ timer: "Countdown", clear: "Clear", look: "Looks", stage: "Stage", prop: "Prop", macro: "Macro", message: "Message" })
    // Which of the audio bin's pictures it is, if it is one of those: "" for none
    readonly property string audio: kind === "next" ? "next" : picture === "SoundEffect" || kind === "sound" ? "sound"
                                  : kind === "audio" || kind === "music" ? "music" : ""
    readonly property string shown: audio !== "" ? "" : picture !== "" ? picture : (pictures[kind] ?? "")
    readonly property bool list: kind === "mediaPlaylist" || kind === "audioPlaylist"
    readonly property bool wide: list || shown === "Looks" || shown === "Macro" || shown === "ClearVideoInput" || shown === "ClearPresentation"

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
        visible: glyph.kind === "other" || glyph.kind === "communication"
        color: glyph.ink
        font.pixelSize: 11
        font.bold: true
        text: "…"
    }

    // One of the audio bin's own pictures
    Loader {
        anchors.centerIn: parent
        active: glyph.audio !== ""

        sourceComponent: AudioGlyph {
            kind: glyph.audio
            ink: glyph.ink
            size: 14
        }
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

    // A playlist of either bin: what it holds at the left, three lines at the right
    Loader {
        anchors.fill: parent
        active: glyph.list

        sourceComponent: Item {
            // The media bin's: a picture in a frame, with a play mark in it
            Shape {
                visible: glyph.kind === "mediaPlaylist"
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: glyph.ink
                    strokeWidth: 1.2
                    fillColor: "transparent"
                    joinStyle: ShapePath.RoundJoin

                    PathSvg {
                        path: "M 1.4 2.2 H 9.4 V 9.8 H 1.4 Z"
                    }
                }

                ShapePath {
                    strokeColor: "transparent"
                    fillColor: glyph.ink

                    PathSvg {
                        path: "M 4.2 4.0 L 7.4 6.0 L 4.2 8.0 Z"
                    }
                }
            }

            // The audio bin's: its two joined notes, as big as the picture of a piece of
            // audio draws them (and overhanging the box as that does), which at any
            // smaller the stems are lost
            AudioGlyph {
                visible: glyph.kind === "audioPlaylist"
                x: -2.5
                y: -1
                kind: "music"
                ink: glyph.ink
                size: 14
            }

            Repeater {
                model: 3

                delegate: Rectangle {
                    required property int index

                    x: 11.5
                    y: 2.6 + index * 3.1
                    width: 4.5
                    height: 1.4
                    radius: 0.7
                    color: glyph.ink
                }
            }
        }
    }
}
