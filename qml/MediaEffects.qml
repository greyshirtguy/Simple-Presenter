import QtQuick
import QtQuick.Effects
import SimplePresenterApp

// The effects of a piece of media (its Media Inspector's Effects: see
// src/mediaeffects.h), drawn one after another in the order the media has them: the
// first from the media, each after it from the one before, and the last is what is
// seen. MediaLook.qml makes this only for media with an effect that is on.
//
// Each effect is a pass over the media's pixels: the picture so far is drawn into a
// texture of its own (a ShaderEffectSource, which also keeps it from being drawn where
// it stands), and a shader of this app's (shaders/effects/<name>.frag) draws the effect
// from it. What each shader is given, by name in its uniform block:
//   source           the picture so far, its colours premultiplied by their opacity,
//                    which what the shader puts out has to keep them
//   time             seconds since the effects began, for one that moves
//   size             the picture's size, in pixels
//   values, more     the effect's numbers, in the order its settings have them
//                    (src/mediaeffects.h): the first four, then the fifth and sixth
//   color1, color2   its colours, in the same order, not premultiplied
// Each says at its top which of them is which of its settings. To add one: the shader,
// its line in qt_add_shaders in CMakeLists.txt, and its shader's name in the catalogue.
//
// Blur is Qt's own blur (MultiEffect), not a shader of this app's, as the blur of Scale
// + Blur is. Those that move by themselves (ripples, stars, an old film's flicker) are
// given the time, and are drawn again at every frame while they are shown, which is the
// cost of a moving effect; the rest are drawn only when what is under them changes, as
// a still is.
Item {
    id: chain

    // What the effects are drawn from
    property Item source: null
    // The effects, as mediasettings::describe has them, that are on and drawn here
    property var effects: []
    // Whether those that move by themselves do: not in a thumbnail, which is drawn once,
    // at a moment a second and a bit in (past a strobe's first flash)
    property bool animated: true
    // Seconds since the effects began, for the ones that move
    property real time: animated ? 0 : 1.25
    readonly property bool moving: animated && effects.some(effect => effect.animated)

    FrameAnimation {
        running: chain.moving && chain.visible
        onTriggered: chain.time += frameTime
    }

    Repeater {
        id: stages

        model: chain.effects.length

        delegate: Item {
            id: stage

            required property int index
            readonly property var effect: chain.effects[Math.min(index, chain.effects.length - 1)]
            // What this one is drawn from: the media, or the effect before it
            readonly property Item input: index === 0 ? chain.source : stages.itemAt(index - 1)
            // The effect's numbers and colours, in the order its shader takes them
            function number(i) {
                const numbers = stage.effect ? stage.effect.numbers : []
                return i < numbers.length ? Number(numbers[i]) : 0
            }
            function colour(i) {
                const colors = stage.effect ? stage.effect.colors : []
                return i < colors.length ? colors[i] : "white"
            }

            anchors.fill: parent

            ShaderEffectSource {
                id: feed

                sourceItem: stage.input
                hideSource: true
                visible: false
            }

            Loader {
                anchors.fill: parent
                sourceComponent: !stage.effect ? null : stage.effect.shader === "blur" ? blurred : shaded
            }

            Component {
                id: shaded

                ShaderEffect {
                    property var source: feed
                    property real time: chain.time
                    property size size: Qt.size(width, height)
                    property vector4d values: Qt.vector4d(stage.number(0), stage.number(1), stage.number(2), stage.number(3))
                    property vector4d more: Qt.vector4d(stage.number(4), stage.number(5), 0, 0)
                    property color color1: stage.colour(0)
                    property color color2: stage.colour(1)

                    fragmentShader: "qrc:/shaders/effects/" + stage.effect.shader + ".frag.qsb"
                }
            }

            // ProPresenter's Blur, its Intensity from nothing to the most Qt's blur does
            Component {
                id: blurred

                MultiEffect {
                    source: feed
                    autoPaddingEnabled: false
                    blurEnabled: true
                    blur: Math.max(0, Math.min(1, stage.number(0)))
                    blurMax: 64
                }
            }
        }
    }
}
