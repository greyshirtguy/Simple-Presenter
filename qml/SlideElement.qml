import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window
import SimplePresenterApp

// One element of a slide: its shape (fill and stroke), with its text over it, each with
// its own shadow. Geometry comes in slide units and is multiplied by `unit`, so text is
// rasterised at the size it is shown at rather than scaled up from a fixed-size texture.
//
// The shape. Nearly every element there is is a rectangle that is filled with a plain
// colour or not at all, and that is drawn as the one rectangle it is. Anything more is
// drawn by the parts further down, which are only made for an element that needs them:
// an outline that is not a rectangle (a rounded rectangle, an ellipse, an arrow, or
// any other shape of ProPresenter's, each drawn from the points of its outline), a
// fill that is a gradient or a picture, edges that fade out. A picture is cut to a
// shape that is not a rectangle by masking it, and edges are faded by a shader
// (shaders/feather.frag), each of which takes a texture of its own; so they too are
// only set up where they are called for.
//
// It draws what the element map says and works nothing out: which elements show, and
// what text each shows, was settled when the map was made (src/proconvert.h). The same
// component is the element on the output, in a thumbnail and under the editor's handles.
//
// The one thing not settled then is text that changes while the slide is on show, which
// an element linked to a timer has, or one linked to the words of the slide that is
// live: that is asked for here, as `liveLinkText`, and drawn in the element's own style.
// It is drawn again only when the words change, which for a timer is once a second while
// it runs, unless the element shows the timer's hundredths: then it is thirty times a
// second where they can be read, and five in a small picture.
//
// A shadow is the one dear thing here. The shape or the text is drawn into a texture of
// its own, which a blur then turns into the shadow under it: an extra pass, and an extra
// texture, for each shadow. So nothing is set up for a shadow unless the element has
// one, and `effects` turns them off altogether where they would not be seen.
Item {
    id: element

    // An element map, as proconvert describes it
    required property var source
    // Output pixels per slide unit
    property real unit: 1
    // The element's box, in slide units. It follows the source unless something, such as
    // a drag in the editor, holds it elsewhere for a while.
    property real boxX: source.x
    property real boxY: source.y
    property real boxWidth: source.width
    property real boxHeight: source.height
    // Text to draw in place of the element's own: a RichText value, or undefined. The
    // editor draws what is being typed this way.
    property var textOverride: undefined
    // Whether text that is set to suit its size to its box does. Not while it is being
    // typed, when each letter has to stand where the caret believes it is.
    property bool fitted: true
    // Whether an element linked to a picture this app does not show (of a slide, of an
    // output) is drawn with the fill it has in the file, which is only there to stand
    // in for the picture. In the editor it is, so that the element can be seen and
    // placed; shown for real, a block of colour where a picture was meant would be
    // worse than nothing.
    property bool standIns: false
    // The words of an element whose words change while it is shown, or undefined for an
    // element that shows what its map says: a timer's time (see Timers), or the words
    // of the slide that is live or of the one after it (see Show), which are what a
    // stage layout is made of. Anything else of the kind belongs here as another case.
    readonly property var liveLinkText: {
        switch (source.linkKind) {
        case "timer":
            // Reading the tick is what has this follow the timers, and reading a beat
            // what has it follow one whose hundredths it shows.
            return Timers.tick >= 0 && (source.linkTimerHundredths === 0 || (effects ? Timers.beat : Timers.slowBeat) >= 0)
                ? Timers.linkedText(source.linkTimerId, source.linkTimerName, source.linkTimerHours,
                                    source.linkTimerMinutes, source.linkTimerSeconds, source.linkTimerHundredths,
                                    source.linkTimerHundredthsUnderMinute, source.linkTimerPattern)
                : ""
        case "slideText":
            // Reading the revision is what has this follow what is live.
            return Show.revision >= 0
                ? Show.slideText(source.linkSlideNext, source.linkSlideSource, source.linkSlideName, source.linkTransform)
                : ""
        default:
            return undefined
        }
    }
    // Whether this is a text box that draws the chords of the slide whose words it
    // shows: then the words are drawn line by line with a row of chords over each
    // (ChordedText), in place of the usual drawing. Only a stage layout has such a
    // box, and nothing is set up for it anywhere else.
    readonly property bool chorded: source.chordsOn === true && source.linkKind === "slideText" && textOverride === undefined
    // Whether to draw shadows. Each one is an extra texture and a blur, which is nothing
    // for the one slide on the output and adds up for a grid of thumbnails, where a
    // shadow is a pixel wide and cannot be seen anyway. It is also taken to say whether
    // this is a picture large enough to read hundredths of a second in.
    property bool effects: true
    // Room around the box for what the text draws outside it (strokes, the bars of a
    // fill that is only behind the text's lines), in slide units
    readonly property real textBleed: 60
    // A fill that is only behind the lines of the text is drawn with the text.
    readonly property bool boxFilled: source.fillShown && !source.fillLinesOnly && (standIns || !source.linkPicture)
    // Whether the shape is more than a rectangle with a plain fill, or none
    readonly property bool plain: source.shape === "rectangle" && (!boxFilled || source.fillKind === "color") && !feathered
    readonly property bool feathered: effects && source.featherOn && boxFilled && source.shape !== "arrow" && source.shape !== "other"
    // The shape's outline as an SVG path, in pixels of this item
    readonly property string outline: plain ? "" : outlinePath(width, height)

    function outlinePath(w, h) {
        const round = (n) => Math.round(n * 100) / 100
        if (source.shape === "roundedRectangle") {
            // Round in the element's own proportions, whatever the points in the file say
            const r = round(Math.max(0, Math.min(0.5, source.roundness)) * Math.min(w, h))
            if (r <= 0)
                return "M 0 0 H " + w + " V " + h + " H 0 Z"
            const arc = " A " + r + " " + r + " 0 0 1 "
            return "M " + r + " 0 H " + round(w - r) + arc + w + " " + r + " V " + round(h - r) + arc + round(w - r) + " " + h
                 + " H " + r + arc + "0 " + round(h - r) + " V " + r + arc + r + " 0 Z"
        }
        const points = source.outline
        if (!points || points.length < 2)
            return "M 0 0 H " + w + " V " + h + " H 0 Z"
        // From each point to the next by a curve, which is a straight line where the
        // bending points are the points themselves
        let path = "M " + round(points[0][0] * w) + " " + round(points[0][1] * h)
        for (let i = 1; i <= points.length; ++i) {
            const from = points[i - 1]
            const to = points[i % points.length]
            path += " C " + round(from[4] * w) + " " + round(from[5] * h) + " " + round(to[2] * w) + " " + round(to[3] * h)
                  + " " + round(to[0] * w) + " " + round(to[1] * h)
        }
        return path + " Z"
    }

    component Shadow: MultiEffect {
        property string which

        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: element.source[which + "Color"]
        shadowHorizontalOffset: element.source[which + "OffsetX"] * element.unit
        shadowVerticalOffset: element.source[which + "OffsetY"] * element.unit
        blurMax: 64
        shadowBlur: Math.min(1, element.source[which + "Radius"] * element.unit / 32)
    }

    x: boxX * unit
    y: boxY * unit
    width: boxWidth * unit
    height: boxHeight * unit
    rotation: source.rotation
    opacity: source.opacity

    Loader {
        anchors.fill: parent
        visible: element.boxFilled || element.source.strokeEnabled
        sourceComponent: element.plain ? plainBox : richShape

        layer.enabled: element.effects && element.source.shadowEnabled
        layer.effect: Shadow {
            which: "shadow"
        }
    }

    Component {
        id: plainBox

        Rectangle {
            color: element.boxFilled ? element.source.fillColor : "transparent"
            border.color: element.source.strokeColor
            border.width: element.source.strokeEnabled ? element.source.strokeWidth * element.unit : 0
        }
    }

    Component {
        id: richShape

        Item {
            id: rich

            readonly property string kind: element.boxFilled ? element.source.fillKind : "none"
            readonly property bool cut: element.source.shape !== "rectangle"
            // Which way a gradient runs: its angle is anticlockwise from pointing right
            readonly property real along: element.source.fillGradientAngle * Math.PI / 180
            readonly property real reach: Math.abs(width / 2 * Math.cos(along)) + Math.abs(height / 2 * Math.sin(along))

            // The fill, which is the part whose edges fade if they do
            Item {
                anchors.fill: parent

                layer.enabled: element.feathered
                layer.effect: ShaderEffect {
                    property real kind: element.source.shape === "ellipse" ? 2 : element.source.shape === "roundedRectangle" ? 1 : 0
                    property real corner: Math.max(0, Math.min(0.5, element.source.roundness)) * Math.min(rich.width, rich.height)
                    property real feather: element.source.featherRadius * Math.min(rich.width, rich.height)
                    property vector2d extent: Qt.vector2d(rich.width, rich.height)

                    fragmentShader: "qrc:/shaders/feather.frag.qsb"
                }

                // A colour or a gradient, in the shape
                Shape {
                    anchors.fill: parent
                    visible: rich.kind === "color" || rich.kind === "gradient"
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: "transparent"
                        strokeWidth: 0
                        fillColor: rich.kind === "color" ? element.source.fillColor : "transparent"
                        fillGradient: rich.kind === "gradient" ? ramp : null

                        PathSvg {
                            path: element.outline
                        }
                    }
                }

                LinearGradient {
                    id: ramp

                    x1: rich.width / 2 - Math.cos(rich.along) * rich.reach
                    y1: rich.height / 2 + Math.sin(rich.along) * rich.reach
                    x2: rich.width / 2 + Math.cos(rich.along) * rich.reach
                    y2: rich.height / 2 - Math.sin(rich.along) * rich.reach

                    GradientStop {
                        position: 0
                        color: element.source.fillGradientFrom
                    }
                    GradientStop {
                        position: 1
                        color: element.source.fillGradientTo
                    }
                }

                // A picture: made to fit inside the element, to fill it and be cut off
                // at its edges, or stretched to it. In a shape that is not a rectangle
                // it is cut to the shape by a mask.
                Image {
                    anchors.fill: parent
                    visible: rich.kind === "media"
                    source: rich.kind === "media" ? element.source.fillMediaSource : ""
                    asynchronous: true
                    clip: true
                    fillMode: element.source.fillMediaScale === 2 ? Image.Stretch
                            : element.source.fillMediaScale === 1 ? Image.PreserveAspectCrop : Image.PreserveAspectFit
                    // No larger in memory than it is shown
                    sourceSize: Qt.size(Math.max(1, Math.ceil(width * Screen.devicePixelRatio)),
                                        Math.max(1, Math.ceil(height * Screen.devicePixelRatio)))

                    layer.enabled: rich.cut && rich.kind === "media"
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: mask.item
                    }
                }

                Loader {
                    id: mask

                    anchors.fill: parent
                    active: rich.cut && rich.kind === "media"

                    sourceComponent: Shape {
                        visible: false
                        preferredRendererType: Shape.CurveRenderer
                        layer.enabled: true

                        ShapePath {
                            strokeColor: "transparent"
                            strokeWidth: 0
                            fillColor: "white"

                            PathSvg {
                                path: element.outline
                            }
                        }
                    }
                }
            }

            // The stroke, along the outline
            Shape {
                anchors.fill: parent
                visible: element.source.strokeEnabled
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: element.source.strokeColor
                    strokeWidth: element.source.strokeWidth * element.unit
                    fillColor: "transparent"
                    joinStyle: ShapePath.MiterJoin

                    PathSvg {
                        path: element.outline
                    }
                }
            }
        }
    }

    StrokedText {
        anchors.fill: parent
        anchors.margins: -element.textBleed * element.unit
        visible: !element.chorded && (element.source.hasText || element.textOverride !== undefined)
        content: element.textOverride !== undefined ? element.textOverride : element.source.displayText
        replacement: element.textOverride !== undefined ? undefined : element.liveLinkText
        lineFill: element.source.fillEnabled && element.source.fillLinesOnly ? element.source.fillColor : "transparent"
        lineFillStyle: element.source.lineMaskStyle
        lineFillWidthOffset: element.source.lineMaskWidthOffset
        lineFillHeightOffset: element.source.lineMaskHeightOffset
        lineFillHorizontalOffset: element.source.lineMaskHorizontalOffset
        lineFillVerticalOffset: element.source.lineMaskVerticalOffset
        unit: element.unit
        bleed: element.textBleed
        verticalAlignment: element.source.verticalAlignment
        fit: element.fitted ? element.source.textScale : 0
        insetLeft: element.source.marginLeft
        insetTop: element.source.marginTop
        insetRight: element.source.marginRight
        insetBottom: element.source.marginBottom

        layer.enabled: element.effects && element.source.textShadowEnabled
        layer.effect: Shadow {
            which: "textShadow"
        }
    }

    Loader {
        anchors.fill: parent
        active: element.chorded

        sourceComponent: ChordedText {
            // Reading the revision is what has this follow what is live, and the key.
            lines: Show.revision >= 0
                   ? Show.chordLines(element.source.linkSlideNext, element.source.linkSlideSource, element.source.linkSlideName,
                                     element.source.linkTransform, element.source.chordNotation)
                   : []
            style: element.source.chordStyle ?? ({})
            chordColor: element.source.chordColor
            unit: element.unit
            fit: element.fitted ? element.source.textScale : 0
            verticalAlignment: element.source.verticalAlignment
            insetLeft: element.source.marginLeft
            insetTop: element.source.marginTop
            insetRight: element.source.marginRight
            insetBottom: element.source.marginBottom

            layer.enabled: element.effects && element.source.textShadowEnabled
            layer.effect: Shadow {
                which: "textShadow"
            }
        }
    }
}
