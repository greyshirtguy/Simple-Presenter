import QtQuick
import QtQuick.Window
import SimplePresenterApp

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
//
// Some content is more than a picture: a video has sound, which no shader mixes. A
// delegate like that has a `level` property, and the layer keeps it at how much of the
// instance is on show: 1 for the one that is, 0 for the other, and while a transition
// runs, falling from 1 to 0 for the outgoing one as it rises for the incoming.
//
// A shader is the one part of the app that is handed to the graphics driver to run, and
// a driver that does not agree with one can take the whole app down with it. So each
// transition is put in the log before its shader is used, by name and by file, and
// again when it is over, with how many frames it was drawn in: the first says what was
// being tried if nothing follows it, and the second how well the hardware keeps up.
Item {
    id: layer

    // An Item with a `content` property; null content must render as nothing. It may
    // have a `ready` property and a `level` property, as described above.
    required property Component delegate
    // What the layer is called in the log, and what the transition is that `shader` is
    property string name: "layer"
    property string shaderName: ""
    // Transition shader to use for the next change, or "" to cut.
    property string shader: ""
    // What that shader is handed besides the two pictures and how far the transition has
    // got, for whatever about it can be adjusted: up to four numbers, a colour (red,
    // green, blue and opacity, not premultiplied) and the way things travel (each part
    // -1, 0 or 1; x to the right, y down). TransitionCatalogue says which transition
    // makes what of them.
    property vector4d options
    property vector4d tint
    property vector2d direction
    // Milliseconds; 0 also cuts.
    property int duration: 0

    property bool aIsFront: true
    property real progress: 0
    // Whether a transition is running, and so whether the instances are being drawn into
    // textures for the shader rather than to the window
    property bool blending: false
    // Whether the incoming instance has been given content that it cannot show yet
    property bool waiting: false
    // How much of each instance is on show, from 0 to 1, for the delegates that want to
    // know. The one on show is all there until a transition takes it away, and the
    // other is not there at all until one brings it in, however long it has been ready
    // and waiting. They are set together, at each step of a transition and when it is
    // over, so that neither is ever heard at a level it is not shown at.
    property real levelA: 1
    property real levelB: 0
    // The delegate instance holding the content most recently shown, from the moment its
    // transition starts; null until something is shown.
    property Item currentItem: null
    // The shader in use. It is fixed for the length of a transition, so that changing
    // the choice half way through one does not change the one that is running. Until
    // the first transition it is the plainest of them, only so that there is one: with
    // none, a ShaderEffect falls back on a shader of Qt's own, which expects a `source`
    // that is not here, and says so.
    property string activeShader: "qrc:/shaders/dissolve.frag.qsb"
    // What it is handed, fixed for the length of a transition likewise
    property vector4d activeOptions
    property vector4d activeTint
    property vector2d activeDirection
    // For the log: what the transition under way is called, and when it began and how
    // many frames the window had shown by then
    property string activeName: ""
    property double beganAt: 0
    property int framesBefore: 0
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
            commit("cut short at " + Math.round(progress * 100) + "% by the next change")
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
            commit("")
            return
        }
        // Into the log first, and only then to the graphics driver.
        activeName = shaderName !== "" ? shaderName : "a transition"
        beganAt = Date.now()
        framesBefore = Log.frames(Window.window)
        Log.note("transition", name + ": " + activeName + " over " + (duration / 1000).toFixed(2) + " s, " + shader.replace("qrc:/", "")
                 + adjustments())
        activeShader = shader
        activeOptions = options
        activeTint = tint
        activeDirection = direction
        transition.duration = duration
        transition.start()
    }

    // What the transition has been set to, where that is anything but nothing, for the log
    function adjustments() {
        const plain = (numbers) => numbers.map(number => +number.toFixed(3)).join(", ")
        const parts = []
        if (options.x !== 0 || options.y !== 0 || options.z !== 0 || options.w !== 0)
            parts.push("options " + plain([options.x, options.y, options.z, options.w]))
        if (tint.x !== 0 || tint.y !== 0 || tint.z !== 0 || tint.w !== 0)
            parts.push("colour " + plain([tint.x, tint.y, tint.z, tint.w]))
        if (direction.x !== 0 || direction.y !== 0)
            parts.push("direction " + plain([direction.x, direction.y]))
        return parts.length > 0 ? "; " + parts.join("; ") : ""
    }

    // Makes the incoming instance the one on show. `how` says how a transition came to
    // its end, for the log, or is "" if there was none.
    function commit(how) {
        if (how !== "") {
            const frames = Log.frames(Window.window) - framesBefore
            const seconds = (Date.now() - beganAt) / 1000
            Log.note("transition", name + ": " + activeName + " " + how + ": " + frames + " frames in " + seconds.toFixed(2) + " s"
                     + (seconds >= 0.1 ? ", " + Math.round(frames / seconds) + " a second" : ""))
        }
        const outgoing = aIsFront ? holderA : holderB
        aIsFront = !aIsFront
        levelA = aIsFront ? 1 : 0
        levelB = aIsFront ? 0 : 1
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

    onProgressChanged: {
        if (!blending)
            return
        levelA = aIsFront ? 1 - progress : progress
        levelB = aIsFront ? progress : 1 - progress
    }

    Binding {
        target: holderA.item
        property: "level"
        when: holderA.item !== null && holderA.item.level !== undefined
        value: layer.levelA
    }

    Binding {
        target: holderB.item
        property: "level"
        when: holderB.item !== null && holderB.item.level !== undefined
        value: layer.levelB
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
        onTriggered: {
            Log.note("media", layer.name + ": what was asked for has no picture after a second, and is not waited for any longer")
            layer.begin()
        }
    }

    ShaderEffect {
        property var fromTex: layer.aIsFront ? layer.textureA : layer.textureB
        property var toTex: layer.aIsFront ? layer.textureB : layer.textureA
        property real progress: layer.progress
        // Width over height, which shaders use to keep shapes round, and the size in
        // pixels, which one uses to work in whole pixels
        property real ratio: height > 0 ? width / height : 1
        property size resolution: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
        property vector4d options: layer.activeOptions
        property vector4d tint: layer.activeTint
        property vector2d direction: layer.activeDirection

        anchors.fill: parent
        visible: layer.blending
        fragmentShader: layer.activeShader
        onStatusChanged: {
            if (status === ShaderEffect.Error)
                Log.problem(layer.name + ": the graphics driver would not take the shader " + layer.activeShader + ". It says: " + log)
        }
    }

    NumberAnimation {
        id: transition

        target: layer
        property: "progress"
        from: 0
        to: 1
        easing.type: Easing.InOutQuad
        onFinished: layer.commit("done")
    }
}
