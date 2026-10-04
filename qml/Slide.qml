import QtQuick
import QtQuick.Effects
import SimplePresenterApp

// One slide, transparent wherever it has no content. Geometry comes in slide units and
// is multiplied by `u`, so text is rasterised at the output's real resolution rather
// than scaled up from a fixed-size texture.
Item {
    id: root

    // A slide map from ProDocument, or null for a clear slide
    property var slide: null
    // The name TransitionLayer sets its delegates' content by
    property alias content: root.slide
    readonly property real slideWidth: slide?.width ?? 1920
    readonly property real slideHeight: slide?.height ?? 1080
    readonly property real u: Math.min(width / slideWidth, height / slideHeight)
    // Room for text strokes that overflow their box, in slide units
    readonly property real textBleed: 40

    Item {
        anchors.centerIn: parent
        width: root.slideWidth * root.u
        height: root.slideHeight * root.u
        clip: true

        Rectangle {
            anchors.fill: parent
            visible: root.slide?.drawsBackground ?? false
            color: root.slide?.backgroundColor ?? "transparent"
        }

        Repeater {
            model: root.slide?.elements ?? []

            delegate: Item {
                id: element

                required property var modelData

                x: modelData.x * root.u
                y: modelData.y * root.u
                width: modelData.width * root.u
                height: modelData.height * root.u
                rotation: modelData.rotation
                opacity: modelData.opacity

                layer.enabled: modelData.shadowEnabled
                layer.effect: MultiEffect {
                    autoPaddingEnabled: true
                    shadowEnabled: true
                    shadowColor: element.modelData.shadowColor
                    shadowHorizontalOffset: element.modelData.shadowOffsetX * root.u
                    shadowVerticalOffset: element.modelData.shadowOffsetY * root.u
                    blurMax: 64
                    shadowBlur: Math.min(1, element.modelData.shadowRadius * root.u / 32)
                }

                Rectangle {
                    anchors.fill: parent
                    visible: element.modelData.fillEnabled || element.modelData.strokeEnabled
                    color: element.modelData.fillEnabled ? element.modelData.fillColor : "transparent"
                    border.color: element.modelData.strokeColor
                    border.width: element.modelData.strokeEnabled ? element.modelData.strokeWidth * root.u : 0
                }

                StrokedText {
                    anchors.fill: parent
                    anchors.margins: -root.textBleed * root.u
                    visible: element.modelData.hasText
                    content: element.modelData.text
                    unit: root.u
                    bleed: root.textBleed
                    verticalAlignment: element.modelData.verticalAlignment
                }
            }
        }
    }
}
