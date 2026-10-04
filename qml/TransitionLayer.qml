import QtQuick

// One output layer. Holds two instances of `delegate`, each rendered offscreen into a
// texture; the only thing drawn is the transition shader, which shows the front one at
// progress 0 and the incoming one at progress 1.
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
    // The delegate instance holding the content most recently shown, from the moment its
    // transition starts; null until something is shown.
    property Item currentItem: null
    // Stays fixed while a transition runs, and draws the resting content between them.
    property string activeShader: "qrc:/shaders/dissolve.frag.qsb"
    property ShaderEffectSource textureA: ShaderEffectSource {
        sourceItem: holderA
        hideSource: true
    }
    property ShaderEffectSource textureB: ShaderEffectSource {
        sourceItem: holderB
        hideSource: true
    }

    // Transitions to `content`, or to nothing if it is null.
    function show(content) {
        if (transition.running) {
            transition.stop()
            commit()
        }
        currentItem = (aIsFront ? holderB : holderA).item
        currentItem.content = content
        if (shader === "" || duration <= 0) {
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
        // ripple.frag only
        property real amplitude: 100
        property real speed: 50

        anchors.fill: parent
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
