import QtQuick
import QtQuick.Effects
import SimplePresenterApp

// One element of a slide: its shape (fill and stroke), with its text over it, each with
// its own shadow. Geometry comes in slide units and is multiplied by `unit`, so text is
// rasterised at the size it is shown at rather than scaled up from a fixed-size texture.
//
// It draws what the element map says and works nothing out: which elements show, and
// what text each shows, was settled when the map was made (src/proconvert.h). The same
// component is the element on the output, in a thumbnail and under the editor's handles.
//
// The one thing not settled then is text that changes while the slide is on show, which
// an element linked to a timer has: that is asked for here, as `liveLinkText`, and drawn
// in the element's own style. It is drawn again only when the words change, which for a
// timer is once a second while it runs, unless the element shows the timer's hundredths:
// then it is thirty times a second where they can be read, and five in a small picture.
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
    // element that shows what its map says. A timer's time is the one such thing so far
    // (see Timers); the others a stage layout needs, such as the words of the live
    // slide, belong here as further kinds of link.
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
        default:
            return undefined
        }
    }
    // Whether to draw shadows. Each one is an extra texture and a blur, which is nothing
    // for the one slide on the output and adds up for a grid of thumbnails, where a
    // shadow is a pixel wide and cannot be seen anyway. It is also taken to say whether
    // this is a picture large enough to read hundredths of a second in.
    property bool effects: true
    // Room around the box for what the text draws outside it (strokes, the bars of a
    // fill that is only behind the text's lines), in slide units
    readonly property real textBleed: 60
    // A fill that is only behind the lines of the text is drawn with the text.
    readonly property bool boxFilled: source.fillEnabled && !source.fillLinesOnly && (standIns || !source.linkPicture)

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

    Rectangle {
        anchors.fill: parent
        visible: element.boxFilled || element.source.strokeEnabled
        color: element.boxFilled ? element.source.fillColor : "transparent"
        border.color: element.source.strokeColor
        border.width: element.source.strokeEnabled ? element.source.strokeWidth * element.unit : 0

        layer.enabled: element.effects && element.source.shadowEnabled
        layer.effect: Shadow {
            which: "shadow"
        }
    }

    StrokedText {
        anchors.fill: parent
        anchors.margins: -element.textBleed * element.unit
        visible: element.source.hasText || element.textOverride !== undefined
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
}
