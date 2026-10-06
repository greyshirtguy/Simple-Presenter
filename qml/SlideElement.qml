import QtQuick
import QtQuick.Effects
import SimplePresenterApp

// One element of a slide: its shape (fill and stroke), with its text over it, each with
// its own shadow. Geometry comes in slide units and is multiplied by `unit`, so text is
// rasterised at the size it is shown at rather than scaled up from a fixed-size texture.
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
    // Room around the box for what the text draws outside it (strokes, the bars of a
    // fill that is only behind the text's lines), in slide units
    readonly property real textBleed: 60
    // A fill that is only behind the lines of the text is drawn with the text.
    readonly property bool boxFilled: source.fillEnabled && !source.fillLinesOnly

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

        layer.enabled: element.source.shadowEnabled
        layer.effect: Shadow {
            which: "shadow"
        }
    }

    StrokedText {
        anchors.fill: parent
        anchors.margins: -element.textBleed * element.unit
        visible: element.source.hasText || element.textOverride !== undefined
        content: element.textOverride !== undefined ? element.textOverride : element.source.displayText
        lineFill: element.source.fillEnabled && element.source.fillLinesOnly ? element.source.fillColor : "transparent"
        lineFillStyle: element.source.lineMaskStyle
        lineFillWidthOffset: element.source.lineMaskWidthOffset
        lineFillHeightOffset: element.source.lineMaskHeightOffset
        lineFillHorizontalOffset: element.source.lineMaskHorizontalOffset
        lineFillVerticalOffset: element.source.lineMaskVerticalOffset
        unit: element.unit
        bleed: element.textBleed
        verticalAlignment: element.source.verticalAlignment
        insetLeft: element.source.marginLeft
        insetTop: element.source.marginTop
        insetRight: element.source.marginRight
        insetBottom: element.source.marginBottom

        layer.enabled: element.source.textShadowEnabled
        layer.effect: Shadow {
            which: "textShadow"
        }
    }
}
