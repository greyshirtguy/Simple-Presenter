import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window
import SimplePresenterApp

// ProPresenter's fills that the graphics chip draws for a slide element: of its letters,
// other than a colour or a gradient (which the text draws itself: StrokedText), and of its
// box, made from what is behind it. src/textfills.h says what each is and what
// ProPresenter draws for it; src/proconvert.cpp puts them in the element's map, as `fills`
// and `boxFill`, only for an element that has them. Made in two places, each only for an
// element that has its part (every element costs every slide it is on, everywhere, and
// few have these):
//
//   - `part: "letters"`, over the words, by SlideElement's Loader for what its words need
//     beyond the text: for each fill of the letters, the same text is drawn again with
//     only that fill's letters, in white (StrokedText's `maskOf`), and the fill is drawn
//     through it. A picture (media fill): the picture, fitted to the whole box as its
//     media says and as a shape's picture fill is drawn (MediaPicture). A blur or an
//     invert: what is behind (see below), blurred by MultiEffect or inverted
//     (shaders/behindfill.frag).
//   - `part: "box"`, as the box's fill, in the shape, under its outline and its words (by
//     SlideElement's placeBehind()): a cut-out, the box's own colour, in its shape, with
//     the letters of that fill as holes in it, through which what is under the element
//     shows; or a blur or an invert of what is behind, through the shape.
//
// What is behind: what is under the slide where the element is, `backdrop` (the screen's
// media layer and video input, the operator's preview's, a thumbnail's picture, the
// editor's backdrop: whichever the slide is drawn over, as the Slide, or the editor's page,
// round the element says; for the layers over the slides, the slides too), over black (a
// screen where nothing is drawn), with the slide's own background colour over it where it
// has one, as it is drawn.
//
// Chosen for speed: only an element that has such a fill takes a texture of what is
// under it, and only of the part it covers; a still picture under it is taken once, a
// video under it at each of its frames, as the screen draws it (the cost of the effect,
// as in ProPresenter, which blurs what plays behind). A blur's margin of three times its
// size round the box is taken with it, so that its edges blur with what is beside them.
// The letters' masks are drawn once, by the processor, as the words are, and again only
// when the words change.
//
// GUESSES and choices of this app's: what is behind is the layers under the element's
// layer, not the slide's own elements behind this one (which ProPresenter blurs too, A
// GUESS): an element over another on the same slide blurs what is under both. A turned
// element takes what is under its box as if it were not turned. Where the slide is drawn
// over nothing (a thumbnail of a slide with no media, a stage layout), a blur of nothing
// is nothing, see-through, which on a screen is black, and an invert of it white, as a
// screen with nothing on it would give; in the editor, where there is nothing under the
// slide, a blur's letters are drawn half see-through white, so that they can be seen and
// placed. How hard MultiEffect blurs was measured (effectBlur), and matched to
// ProPresenter's measured blur (textfills::blurSigma): on a screen of the size of the
// maintainer's demo, within a fraction of a pixel of it, which is more than the "same
// ballpark" they asked for.
Item {
    id: fills

    // The SlideElement this is part of (`owner` and not `element`, which the element's own id
    // is, where this is made)
    required property Item owner
    // Which of its fills this draws: "letters" or "box" (see above)
    property string part: "letters"
    // The nearest item round the element that has a `backdrop`, what the slide is drawn
    // over here, and a `slide`, its map (Slide, or the editor's page). Looked up from here,
    // once, and not handed to every element, which every slide's every element would pay for.
    readonly property Item holder: {
        let around = owner.parent
        while (around && around.backdrop === undefined)
            around = around.parent
        return around
    }
    readonly property var backdrop: holder ? holder.backdrop : null
    // The slide's own background colour, where it has one: drawn over the media (Slide), so
    // part of what is behind the element, and all of it where it is not see-through
    readonly property color slideColour: holder && holder.slide && holder.slide.drawsBackground
                                         ? holder.slide.backgroundColor : "transparent"
    // Whether the slide is drawn over something here, to blur or invert, that its own
    // background does not hide
    readonly property bool overSomething: backdrop !== null && backdrop.visible && slideColour.a < 1
    // The element's box in the backdrop's own terms, `margin` pixels larger all round:
    // what a fill of what is behind takes of it. (The element's place and size are read
    // for the binding to follow them.)
    function behindRect(margin) {
        if (!overSomething)
            return Qt.rect(0, 0, 1, 1)
        const moved = owner.x + owner.y + owner.width + owner.height + backdrop.width + backdrop.height
        const r = owner.mapToItem(backdrop, -margin, -margin, owner.width + 2 * margin, owner.height + 2 * margin)
        return moved === moved ? Qt.rect(r.x, r.y, r.width, r.height) : r
    }

    // MultiEffect's blur for a Gaussian blur of `sigma` pixels (its standard deviation):
    // [blur, blurMultiplier] at a blurMax of 64. MEASURED (2026-10-29, Qt 6.10, a picture
    // blurred by MultiEffect at fifteen settings, each fitted to the picture blurred by a
    // Gaussian): its blur depends only on blur times blurMax, its "radius", and not in
    // proportion to it; these are the sigmas it gave at radii of 3.2 to 64, between which
    // the radius is read off in a straight line. Past its largest, 18.75 at a blur of 1, the
    // multiplier, which Qt says spreads the blur further, coarser (A GUESS that it spreads
    // it in proportion, by one and the multiplier).
    readonly property var measuredBlurs: [[0, 0], [3.2, 0.75], [6.4, 1.25], [9.6, 1.75], [12.8, 2.25], [16, 2.75],
                                          [19.2, 3.25], [25.6, 4.75], [32, 6.75], [38.4, 8.75], [51.2, 13.5], [64, 18.75]]
    function effectBlur(sigma) {
        const table = measuredBlurs
        const widest = table[table.length - 1]
        if (sigma >= widest[1])
            return [1, sigma / widest[1] - 1]
        let k = 1
        while (table[k][1] < sigma)
            ++k
        const [r0, s0] = table[k - 1]
        const [r1, s1] = table[k]
        return [(r0 + (r1 - r0) * (sigma - s0) / (s1 - s0)) / 64, 0]
    }

    anchors.fill: parent

    // The letters of one of the text's fills, `maskOf` its number, in white, drawn as the
    // element's words are, where they are; for a mask, so not shown
    component Letters: StrokedText {
        anchors.fill: parent
        visible: false
        content: fills.owner.textOverride !== undefined ? fills.owner.textOverride : fills.owner.source.displayText
        replacement: fills.owner.textOverride !== undefined ? undefined : fills.owner.liveLinkText
        unit: fills.owner.unit
        bleed: 0
        verticalAlignment: fills.owner.source.verticalAlignment
        fit: fills.owner.fitted ? fills.owner.source.textScale : 0
        insetLeft: fills.owner.source.marginLeft
        insetTop: fills.owner.source.marginTop
        insetRight: fills.owner.source.marginRight
        insetBottom: fills.owner.source.marginBottom
        layer.enabled: true
    }

    // The element's shape, filled with `color`: its box, or its outline where it is not a
    // plain rectangle (SlideElement's `outline`, as its picture fill is cut to)
    component ShapeFill: Item {
        id: shapeFill

        property color color: "white"

        Rectangle {
            anchors.fill: parent
            visible: fills.owner.plain
            color: shapeFill.color
        }

        Shape {
            anchors.fill: parent
            visible: !fills.owner.plain
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: "transparent"
                strokeWidth: 0
                fillColor: shapeFill.color

                PathSvg {
                    path: fills.owner.outline
                }
            }
        }
    }

    // What is behind drawn through a mask: blurred (`sigma` pixels, its standard
    // deviation) or not, inverted or not, with its saturation; `mask` the letters or the
    // shape, as big as the element. (Put in an Item where a Loader makes it, and not made
    // the Loader's item: a Loader makes its item its own size, and this is larger, by its
    // margin. Made the element's size, it drew what is behind squeezed, and out of place by
    // the margin.)
    component Behind: Item {
        id: through

        required property Item mask
        property real sigma: 0
        property bool invert: false
        property real saturation: 1
        readonly property real margin: Math.ceil(sigma * 3)
        readonly property var settings: fills.effectBlur(sigma)

        x: -margin
        y: -margin
        width: fills.width + 2 * margin
        height: fills.height + 2 * margin

        ShaderEffectSource {
            id: under

            anchors.fill: parent
            visible: false
            live: true
            hideSource: false
            sourceItem: fills.overSomething ? fills.backdrop : null
            sourceRect: fills.behindRect(through.margin)
        }

        // The mask in the same, larger, frame
        ShaderEffectSource {
            id: maskFrame

            anchors.fill: parent
            visible: false
            sourceItem: through.mask
            sourceRect: Qt.rect(-through.margin, -through.margin, through.width, through.height)
        }

        // A blur: by MultiEffect, which draws it through the mask itself, in one pass, and
        // not into a layer of its own for the shader below to take, which would be one more
        // texture as big as the element. (So what is not under it stays see-through: over
        // the slide's own background where it has one, which a blur of leaves as it is, and
        // the screen's black, where the shader lays it over black.)
        MultiEffect {
            anchors.fill: parent
            visible: through.sigma > 0
            source: under
            autoPaddingEnabled: false
            blurEnabled: true
            blurMax: 64
            blur: through.settings[0]
            blurMultiplier: through.settings[1]
            // (MultiEffect's saturation runs from -1, grey, through 0, the colours as they
            // are: taken here to be added to 1 for a multiple of their own, A GUESS at its
            // formula that only a saturation other than 1 would show)
            saturation: through.saturation - 1
            maskEnabled: true
            maskSource: maskFrame
        }

        // And over it the slide's own background, where it is partly see-through, as it is
        // laid over the media (a blur of a colour all over being the colour). Made only then.
        Loader {
            anchors.fill: parent
            active: through.sigma > 0 && fills.overSomething && fills.slideColour.a > 0

            sourceComponent: Item {
                Rectangle {
                    id: tint

                    anchors.fill: parent
                    visible: false
                    color: fills.slideColour
                    layer.enabled: true
                }

                MultiEffect {
                    anchors.fill: parent
                    source: tint
                    maskEnabled: true
                    maskSource: maskFrame
                }
            }
        }

        // Not a blur: what is under it, over black, inverted or not, by the shader
        ShaderEffect {
            anchors.fill: parent
            visible: through.sigma <= 0

            property var source: under
            property var mask: maskFrame
            property real invert: through.invert ? 1 : 0
            property real saturation: through.saturation
            property color base: "black"
            // (Premultiplied, here, and not left to how a colour is handed to a shader)
            property vector4d over: Qt.vector4d(fills.slideColour.r * fills.slideColour.a, fills.slideColour.g * fills.slideColour.a,
                                                fills.slideColour.b * fills.slideColour.a, fills.slideColour.a)

            fragmentShader: "qrc:/shaders/behindfill.frag.qsb"
        }
    }

    // ---- The letters' fills, over the words
    Repeater {
        model: fills.part === "letters" ? fills.owner.source.fills?.text ?? [] : []

        Item {
            id: one

            required property var modelData
            readonly property string kind: modelData.kind
            // A blur in the editor, over nothing: drawn half see-through white (see above)
            readonly property bool standIn: kind === "blur" && !fills.overSomething && fills.owner.standIns

            anchors.fill: parent

            Letters {
                id: letters

                maskOf: one.modelData.index
            }

            Loader {
                anchors.fill: parent
                sourceComponent: one.standIn ? standInFill
                               : one.kind === "media" ? mediaFill
                               : one.kind === "blur" || one.kind === "invert" ? behindFill : null
            }

            Component {
                id: mediaFill

                Item {
                    MediaPicture {
                        id: picture

                        anchors.fill: parent
                        visible: false
                        source: one.modelData.source
                        settings: one.modelData.settings ?? null
                        fitting: one.modelData.scale
                        clip: true
                        readSize: Qt.size(Math.max(1, Math.ceil(width * Screen.devicePixelRatio)),
                                          Math.max(1, Math.ceil(height * Screen.devicePixelRatio)))
                        layer.enabled: true
                    }

                    MultiEffect {
                        anchors.fill: parent
                        source: picture
                        maskEnabled: true
                        maskSource: letters
                    }
                }
            }

            Component {
                id: behindFill

                Item {
                    Behind {
                        mask: letters
                        sigma: one.kind === "blur" ? one.modelData.sigma * fills.owner.unit : 0
                        invert: one.kind === "invert"
                        saturation: one.modelData.saturation ?? 1
                    }
                }
            }

            Component {
                id: standInFill

                Item {
                    Rectangle {
                        id: white

                        anchors.fill: parent
                        visible: false
                        color: "#80ffffff"
                        layer.enabled: true
                    }

                    MultiEffect {
                        anchors.fill: parent
                        source: white
                        maskEnabled: true
                        maskSource: letters
                    }
                }
            }
        }
    }

    // ---- The box's fill, in the shape
    Loader {
        anchors.fill: parent
        active: fills.part === "box" && fills.owner.source.boxFill !== undefined
        sourceComponent: !active ? null : fills.owner.source.boxFill.kind === "cutOut" ? cutOutBox : behindBox
    }

    // The box's colour, in its shape, with the letters of the cut-out as holes in it
    Component {
        id: cutOutBox

        Item {
            Letters {
                id: holes

                maskOf: fills.owner.source.boxFill.index
            }

            ShapeFill {
                id: box

                anchors.fill: parent
                visible: false
                color: fills.owner.source.boxFill.color
                layer.enabled: true
            }

            MultiEffect {
                anchors.fill: parent
                source: box
                maskEnabled: true
                maskInverted: true
                maskSource: holes
            }
        }
    }

    // What is behind the box, blurred or inverted, in its shape
    Component {
        id: behindBox

        Item {
            ShapeFill {
                id: shape

                anchors.fill: parent
                visible: false
                layer.enabled: true
            }

            Behind {
                mask: shape
                sigma: fills.owner.source.boxFill.kind === "blur" ? fills.owner.source.boxFill.sigma * fills.owner.unit : 0
                invert: fills.owner.source.boxFill.kind === "invert"
                saturation: fills.owner.source.boxFill.saturation ?? 1
            }
        }
    }
}
