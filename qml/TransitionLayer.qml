import QtQuick

// One layer of the output: the media layer, or the slide layer over it.
//
// How it works. The layer holds two instances of `delegate`, A and B. One of them, the
// front one, has the content that is on show; the other is empty. Showing something new
// gives it to the empty one and then swaps which is the front, either at once (a cut) or
// by way of a transition.
//
// A transition is a fragment shader. For as long as one runs, neither instance is drawn
// to the window: each is drawn into a texture instead, and the only thing drawn is the
// shader, which is handed both textures and a `progress` that an animation takes from 0
// to 1, and decides for every pixel what mix of the outgoing and incoming pictures to
// show. That is all a transition is, so adding one is adding a shader file, and it costs
// the same whatever is on the layer: text, a still, or two videos playing at once.
//
// When no transition is running there is nothing to mix, and the front instance is
// simply drawn. That matters on modest graphics hardware: going through the textures
// and a shader would cost two more passes over the whole output for every frame of a
// playing video, to show exactly what drawing it directly shows.
Item {
    id: layer

    // An Item with a `content` property; null content must render as nothing.
    required property Component delegate
    // Transition shader to use for the next change, or "" to cut.
    property string shader: ""
    // Milliseconds; 0 also cuts.
    property int duration: 0

    property bool aIsFront: true
    property real progress: 0
    // Whether a transition is running, and so whether the instances are being drawn into
    // textures for the shader rather than to the window
    property bool blending: false
    // The delegate instance holding the content most recently shown, from the moment its
    // transition starts; null until something is shown.
    property Item currentItem: null
    // The shader in use. It is fixed for the length of a transition, so that changing
    // the choice half way through one does not change the one that is running. Until
    // the first transition it is the plainest of them, only so that there is one: with
    // none, a ShaderEffect falls back on a shader of Qt's own, which expects a `source`
    // that is not here, and says so.
    property string activeShader: "qrc:/shaders/dissolve.frag.qsb"
    // The two instances as textures. Nothing draws these but the shader below, so while
    // it is hidden they cost nothing.
    property ShaderEffectSource textureA: ShaderEffectSource {
        sourceItem: holderA
        hideSource: layer.blending
    }
    property ShaderEffectSource textureB: ShaderEffectSource {
        sourceItem: holderB
        hideSource: layer.blending
    }

    // Transitions to `content`, or to nothing if it is null.
    function show(content) {
        if (transition.running) {
            transition.stop()
            commit()
        }
        const animated = shader !== "" && duration > 0
        // Before the incoming instance is given anything to draw, or it would be drawn
        // straight over the outgoing one.
        blending = animated
        currentItem = (aIsFront ? holderB : holderA).item
        currentItem.content = content
        if (!animated) {
            commit()
            return
        }
        activeShader = shader
        transition.duration = duration
        transition.start()
    }

    function commit() {
        const outgoing = aIsFront ? holderA : holderB
        aIsFront = !aIsFront
        progress = 0
        // Release what is no longer shown, so an outgoing video stops decoding.
        outgoing.item.content = null
        blending = false
    }

    Loader {
        id: holderA

        anchors.fill: parent
        sourceComponent: layer.delegate
    }

    Loader {
        id: holderB

        anchors.fill: parent
        sourceComponent: layer.delegate
    }

    ShaderEffect {
        property var fromTex: layer.aIsFront ? layer.textureA : layer.textureB
        property var toTex: layer.aIsFront ? layer.textureB : layer.textureA
        property real progress: layer.progress
        // Width over height, which the gl-transitions shaders use to keep shapes round
        property real ratio: height > 0 ? width / height : 1
        // ripple.frag only
        property real amplitude: 100
        property real speed: 50

        anchors.fill: parent
        visible: layer.blending
        fragmentShader: layer.activeShader
    }

    NumberAnimation {
        id: transition

        target: layer
        property: "progress"
        from: 0
        to: 1
        easing.type: Easing.InOutQuad
        onFinished: layer.commit()
    }
}
