import QtQuick

// The audience output: the media layer with the slide layer over it, on black.
AuxWindow {
    id: win

    // Applied to the next change on either layer: a .qsb url, or "" to cut; milliseconds.
    property string shader: ""
    property int duration: 0
    // The QVideoSink of the video on the media layer, or null; lets a preview borrow its frames.
    readonly property var liveVideoSink: mediaLayer.currentItem ? mediaLayer.currentItem.videoSink : null

    // `slide` is a slide map from Catalog.open(); null clears the layer.
    function showSlide(slide) {
        slideLayer.show(slide)
    }

    // `media` is { source, video }; null clears the layer.
    function showMedia(media) {
        mediaLayer.show(media)
    }

    objectName: "output"
    title: "Output"

    TransitionLayer {
        id: mediaLayer

        anchors.fill: parent
        shader: win.shader
        duration: win.duration
        delegate: MediaContent {}
    }

    TransitionLayer {
        id: slideLayer

        anchors.fill: parent
        shader: win.shader
        duration: win.duration
        delegate: Slide {}
    }
}
