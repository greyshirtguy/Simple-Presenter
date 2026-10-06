import QtQuick

// The transitions the app has, and what can be adjusted about each.
//
// A transition is two things: a fragment shader (shaders/dissolve.frag says what one is
// given and has to give back), and its entry here, which names it, files it under a
// category for the menu, and lists its options. So adding one is a shader file, a line
// for it in CMakeLists.txt, and an entry here.
//
// The names, the categories and the options are ProPresenter's, for the transitions it
// has, so that someone coming from it finds what they know, and so that a transition a
// presentation names can one day be matched to one here (`pro` is the id ProPresenter
// knows it by). The shaders are not ProPresenter's, which are its own and are not to be
// copied: each is either written for this app to give the same kind of change, or
// ported from gl-transitions (MIT), from which ProPresenter adapted several of its own.
// The last category, More, is transitions ProPresenter does not have.
//
// An option is something about a transition that can be adjusted. It has a `name`, the
// `label` it is shown by, the `value` it has until it is changed, and a `kind`, which
// decides the rest and how the shader is handed it:
//
//   "number"     from `from` to `to`. The numbers of a transition go to its shader as
//                `options`, a vec4, in the order they are listed here.
//   "color"      a colour, written "#aarrggbb". It goes to the shader as `tint`.
//   "direction"  one of nine, numbered across and down a three by three grid by where
//                what is coming comes from: 0 the top left, 1 the top, 4 the centre, 8
//                the bottom right. `allowed` has a bit set for each one that can be
//                chosen, as ProPresenter's files have it. It goes to the shader as
//                `direction`, the way things travel: each part -1, 0 or 1.
QtObject {
    id: catalogue

    // The categories, in the order the menu shows them. Cut is in none.
    readonly property var categories: ["Dissolves", "Wipes", "Movements", "Objects", "Color", "Blurs", "More"]

    // Bits of `allowed`: every direction, every one but the centre, and the four sides.
    readonly property int anyDirection: 511
    readonly property int anySide: 495
    readonly property int straightSides: 170

    readonly property var transitions: [
        { name: "Cut", category: "", shader: "", pro: "AB29D07B-E9E2-4E0A-93BD-AD3EA58120FA", options: [] },

        { name: "Dissolve", category: "Dissolves", shader: "dissolve", pro: "EC52A828-AD85-4602-B70C-1DEE7C904DB6",
          options: [] },
        { name: "Fade", category: "Dissolves", shader: "fade", pro: "99BDD1C3-EE98-4E80-A8DE-3699CE9F338E", options: [] },
        { name: "Fade Black", category: "Dissolves", shader: "fadeBlack", pro: "01E95287-E84D-4638-9E28-18C8735ABE47",
          options: [] },
        { name: "Fade Bright", category: "Dissolves", shader: "fadeBright", pro: "802181F9-E290-4FA9-A9ED-06CAA2306A12",
          options: [] },
        { name: "Fade Dark", category: "Dissolves", shader: "fadeDark", pro: "E4D84DA7-E8E3-43B5-820E-8D883416D355",
          options: [] },
        { name: "Fade Gray", category: "Dissolves", shader: "fadeGray", pro: "C30699D4-3249-4B51-A809-191662294ED0",
          options: [] },
        { name: "Fade White", category: "Dissolves", shader: "fadeWhite", pro: "C2123A39-357E-4626-9257-783AF40918FC",
          options: [] },
        { name: "Warp Fade", category: "Dissolves", shader: "gl-transitions/crosswarp",
          pro: "72091328-AA13-4312-86FD-D54A5FC10A9C", options: [] },
        { name: "Wave Dissolve", category: "Dissolves", shader: "gl-transitions/Dreamy",
          pro: "0540598C-5CED-4F80-AB0E-3566FEA85BCF", options: [] },

        { name: "Amoeba", category: "Wipes", shader: "amoeba", pro: "B5E9AD29-DCEC-4AD2-A78D-3C49A101E1D2",
          options: [{ name: "scale", label: "Scale", note: "How fine the lumps of its outline are", kind: "number",
                      from: 10, to: 100, value: 40 }] },
        { name: "Reveal", category: "Wipes", shader: "reveal", pro: "3EA3BC78-9A06-480D-8E6E-3A51A47BD0D6",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anySide, value: 0 }] },
        { name: "Square Wipe", category: "Wipes", shader: "gl-transitions/squareswire",
          pro: "DF000B14-0F80-4A8B-86A5-903564032437",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anySide, value: 0 }] },
        { name: "Wipe", category: "Wipes", shader: "wipe", pro: "7084A71F-C12C-4EEA-8D90-A2D30FB0B256",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anySide, value: 0 }] },

        { name: "Flip", category: "Movements", shader: "flip", pro: "82DE5438-1851-4011-929C-D59A292D9C79", options: [] },
        { name: "Fly In", category: "Movements", shader: "flyIn", pro: "ADF02083-C3B8-4A47-BC9A-BA57A887C404",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anyDirection, value: 0 }] },
        { name: "Iris", category: "Movements", shader: "iris", pro: "F694B97E-2C1C-4888-B6AE-0619617A16CF", options: [] },
        { name: "Mosaic", category: "Movements", shader: "mosaic", pro: "9BE0AB3A-9239-4976-BA34-417EADB27995",
          options: [] },
        { name: "Move In", category: "Movements", shader: "moveIn", pro: "31B698F6-500B-4097-B209-C6F811A99A7C",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anySide, value: 0 }] },
        { name: "Push", category: "Movements", shader: "push", pro: "B400F82E-CF9D-4700-89FD-F3889DAA06C5",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: straightSides, value: 1 }] },
        { name: "Ripple", category: "Movements", shader: "ripple", pro: "47A0842A-E064-4282-854A-27224E48D7B0",
          options: [] },
        { name: "Swap", category: "Movements", shader: "swap", pro: "6E087942-135D-4239-9093-98AD4BA86274", options: [] },
        { name: "Zoom In", category: "Movements", shader: "zoomIn", pro: "928B442C-4912-42F6-B526-60921E0F0D40",
          options: [{ name: "direction", label: "Direction", kind: "direction", allowed: anyDirection, value: 0 }] },

        { name: "Cross Hatch", category: "Objects", shader: "gl-transitions/crosshatch",
          pro: "5584E51F-5C92-47B5-9B65-3C2540C1F20C", options: [] },
        { name: "Cube", category: "Objects", shader: "cube", pro: "CD545FC3-70FA-4120-8A48-29EFE903BD10", options: [] },
        { name: "Door", category: "Objects", shader: "door", pro: "DAA798F1-359A-46C3-AEC6-5C3DFE93635A", options: [] },
        { name: "Door Wipe", category: "Objects", shader: "doorWipe", pro: "645A6350-128C-48B9-AA9D-D7D85ABEF81E",
          options: [] },
        { name: "Kaleidoscope Wipe", category: "Objects", shader: "gl-transitions/kaleidoscope",
          pro: "5717B340-8ED9-457C-A6EE-74A8D2679E43", options: [] },
        { name: "Random Pixels", category: "Objects", shader: "randomPixels", pro: "DF2C1B6C-5B62-49A1-B72E-C80CB3C0CA75",
          options: [] },
        { name: "Random Squares", category: "Objects", shader: "gl-transitions/randomsquares",
          pro: "39030834-E94F-417E-A8B4-4870C9C7D413",
          options: [{ name: "squareSize", label: "Size", note: "How large the squares are", kind: "number",
                      from: 1, to: 10, value: 2.5 }] },
        { name: "Random Squares Flicker", category: "Objects", shader: "randomSquaresFlicker",
          pro: "E4650E1F-B42E-4492-9824-BE4D8E27E833", options: [] },

        { name: "Color Burn", category: "Color", shader: "colorBurn", pro: "2B855B65-6ABC-4F65-8198-A19A5EF97821",
          options: [{ name: "burnColor", label: "Burn Color", note: "The colour of the flash", kind: "color",
                      value: "#ff808080" }] },
        { name: "Color Push", category: "Color", shader: "colorPush", pro: "E958E7B0-E5C2-4C10-9B22-D1FE4FEA1E49",
          options: [] },
        { name: "Color Warp", category: "Color", shader: "gl-transitions/flyeye", pro: "1EC1B05C-4E79-460C-9429-4129D9094E73",
          options: [{ name: "zoom", label: "Zoom", note: "How many ripples there are across the picture", kind: "number",
                      from: 10.01, to: 40.1, value: 20.05 },
                    { name: "size", label: "Size", note: "How far the ripples pull the picture about", kind: "number",
                      from: 0.01, to: 1.1, value: 0.05 },
                    { name: "colorSeparation", label: "Color Separation", note: "How far its colours come apart",
                      kind: "number", from: 0.01, to: 4.1, value: 0.85 }] },
        { name: "Film Burn", category: "Color", shader: "gl-transitions/FilmBurn", pro: "2CB0361E-5E77-4E7F-AEDB-EA6673F743EF",
          options: [] },

        { name: "Dispersion Blur", category: "Blurs", shader: "dispersionBlur", pro: "F1475C78-329A-4897-B5C0-266C6433CCCC",
          options: [{ name: "size", label: "Radius", note: "How far the copies of the picture spread", kind: "number",
                      from: 0, to: 2, value: 0.5 },
                    { name: "angle", label: "Angle", note: "The angle from each copy to the next", kind: "number",
                      from: 0, to: 4, value: 2.3 }] },
        { name: "Noisy Zoom", category: "Blurs", shader: "noisyZoom", pro: "F6AF1CC0-F049-40DC-926E-8CA350C2909D",
          options: [] },

        { name: "Circle Open", category: "More", shader: "gl-transitions/circleopen", options: [] },
        { name: "Cross Zoom", category: "More", shader: "gl-transitions/CrossZoom", options: [] },
        { name: "Directional Warp", category: "More", shader: "gl-transitions/directionalwarp", options: [] },
        { name: "Heart", category: "More", shader: "gl-transitions/heart", options: [] },
        { name: "Linear Blur", category: "More", shader: "gl-transitions/LinearBlur", options: [] },
        { name: "Pinwheel", category: "More", shader: "gl-transitions/pinwheel", options: [] },
        { name: "Pixelize", category: "More", shader: "gl-transitions/pixelize", options: [] },
        { name: "Radial", category: "More", shader: "gl-transitions/Radial", options: [] },
        { name: "Ripple Wave", category: "More", shader: "gl-transitions/ripple",
          options: [{ name: "amplitude", label: "Amplitude", note: "How many waves there are", kind: "number",
                      from: 20, to: 200, value: 100 },
                    { name: "speed", label: "Speed", note: "How fast the waves run outwards", kind: "number",
                      from: 10, to: 100, value: 50 }] },
        { name: "Simple Zoom", category: "More", shader: "gl-transitions/SimpleZoom", options: [] },
        { name: "Swirl", category: "More", shader: "gl-transitions/Swirl", options: [] },
        { name: "Water Drop", category: "More", shader: "gl-transitions/WaterDrop", options: [] },
        { name: "Wind", category: "More", shader: "gl-transitions/wind", options: [] },
        { name: "Window Slice", category: "More", shader: "gl-transitions/windowslice", options: [] },
        { name: "Wipe Down", category: "More", shader: "gl-transitions/wipeDown", options: [] },
        { name: "Wipe Left", category: "More", shader: "gl-transitions/wipeLeft", options: [] },
        { name: "Wipe Right", category: "More", shader: "gl-transitions/wipeRight", options: [] },
        { name: "Wipe Up", category: "More", shader: "gl-transitions/wipeUp", options: [] }
    ]

    // Where a transition's compiled shader is, or "" for a cut.
    function shaderUrl(transition) {
        return transition.shader === "" ? "" : "qrc:/shaders/" + transition.shader + ".frag.qsb"
    }

    // The value of an option: what was chosen for it, if anything was. `chosen` is a map
    // of option names to values, or undefined.
    function valueOf(option, chosen) {
        return chosen !== undefined && chosen[option.name] !== undefined ? chosen[option.name] : option.value
    }

    // The way things travel for a direction, which is numbered by where they come from.
    function travel(direction) {
        return Qt.vector2d(1 - direction % 3, 1 - Math.floor(direction / 3))
    }

    // What a transition's shader is handed for its options: { options, tint, direction }.
    function uniforms(transition, chosen) {
        const numbers = [0, 0, 0, 0]
        let count = 0
        let tint = Qt.vector4d(0, 0, 0, 0)
        let direction = Qt.vector2d(0, 0)
        for (const option of transition.options) {
            const value = valueOf(option, chosen)
            if (option.kind === "number") {
                numbers[count++] = value
            } else if (option.kind === "color") {
                const color = Qt.color(value)
                tint = Qt.vector4d(color.r, color.g, color.b, color.a)
            } else if (option.kind === "direction") {
                direction = travel(value)
            }
        }
        return { options: Qt.vector4d(numbers[0], numbers[1], numbers[2], numbers[3]), tint: tint, direction: direction }
    }
}
