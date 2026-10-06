import QtQuick

// One slide, transparent wherever it has no content. Slides are authored against their
// own size (typically 1920x1080) and drawn in units of the output's height over that,
// so everything is rendered at the output's real resolution.
Item {
    id: root

    // A slide map from ProDocument, or null for a clear slide
    property var slide: null
    // The name TransitionLayer sets its delegates' content by
    property alias content: root.slide
    readonly property real slideWidth: slide?.width ?? 1920
    readonly property real slideHeight: slide?.height ?? 1080
    readonly property real u: Math.min(width / slideWidth, height / slideHeight)

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

        // Elements that are hidden, or that their visibility rules rule out, are not drawn.
        Repeater {
            model: root.slide?.elements ?? []

            delegate: SlideElement {
                required property var modelData

                source: modelData
                unit: root.u
                visible: modelData.visible
            }
        }
    }
}
