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
//
// Some content needs a moment before it can be shown: a still has to be read from its
// file, which is done on another thread so that nothing stands still meanwhile, and a
// video has no picture until its first frame has been decoded (see MediaContent). A
// delegate like that has a `ready` property, and a change to content that is not ready
// waits for it: the layer goes on showing what it showed, with the incoming instance
// out of sight, and the cut or the transition is made when the content can be seen.
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
    // Whether the incoming instance has been given content that it cannot show yet
    property bool waiting: false
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

    // Transitions to `content`, or to nothing if it is null: at once if the content can
    // be shown at once, and otherwise when it can.
    function show(content) {
        if (transition.running) {
            transition.stop()
            commit()
        }
        // Whatever was being waited for is given up for this.
        patience.stop()
        waiting = false
        const incoming = (aIsFront ? holderB : holderA).item
        incoming.content = content
        if (incoming.ready === false) {
            waiting = true
            patience.start()
        } else {
            begin()
        }
    }

    // Brings in what the incoming instance has been given.
    function begin() {
        patience.stop()
        const animated = shader !== "" && duration > 0
        // Before the incoming instance is let be seen, or it would be drawn straight
        // over the outgoing one.
        blending = animated
        waiting = false
        currentItem = (aIsFront ? holderB : holderA).item
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

    // The incoming one of the two is kept out of sight while it is being waited for.
    Loader {
        id: holderA

        anchors.fill: parent
        visible: layer.aIsFront || !layer.waiting
        sourceComponent: layer.delegate
    }

    Loader {
        id: holderB

        anchors.fill: parent
        visible: !layer.aIsFront || !layer.waiting
        sourceComponent: layer.delegate
    }

    // The incoming instance saying that it can show what it was given
    Connections {
        target: layer.waiting ? (layer.aIsFront ? holderB : holderA).item : null

        function onReadyChanged() {
            if (target.ready)
                layer.begin()
        }
    }

    // A file that takes longer than this to give a picture is not waited for any more:
    // the change is made, to nothing at first, and the picture shows when it arrives. A
    // show is not held up by one slow file.
    Timer {
        id: patience

        interval: 1000
        onTriggered: layer.begin()
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
