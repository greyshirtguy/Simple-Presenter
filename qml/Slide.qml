import QtQuick

// One slide, transparent wherever it has no content. Slides are authored against their
// own size (typically 1920x1080) and drawn in units of the output's height over that,
// so everything is rendered at the output's real resolution.
//
// This one component draws a slide everywhere a slide appears: on the output, in the
// thumbnails of the grid, in the preview, and in the editor's list. There is no second,
// simplified renderer for the small ones, so a thumbnail cannot show something
// different from what will be shown; it is the same drawing, smaller.
Item {
    id: root

    // A slide map from ProDocument, or null for a clear slide
    property var slide: null
    // The name TransitionLayer sets its delegates' content by
    property alias content: root.slide
    // Whether to draw shadows; thumbnails go without (see SlideElement)
    property bool effects: true
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
                effects: root.effects
                visible: modelData.visible
            }
        }
    }
}
