import QtQuick

// The audience output: the media layer with the slide layer over it, on black.
//
// Layers are what make a presenter more than a slide show. A background video on the
// media layer keeps playing while the slides over it change, and either layer can be
// changed or cleared without touching the other. Each is a TransitionLayer, so each
// makes its own transition; the slide layer is transparent wherever a slide has
// nothing, and the media shows through.
//
// The window knows nothing about presentations. It is handed things to show (a slide
// map, or a media file) and shows them; what is live is the operator window's business.
AuxWindow {
    id: win

    // Applied to the next change on either layer: a .qsb url, or "" to cut; what the
    // shader is handed for the transition's options (see TransitionLayer); milliseconds.
    property string shader: ""
    property vector4d options
    property vector4d tint
    property vector2d direction
    property int duration: 0
    // The QVideoSink of the video on the media layer, or null; lets a preview borrow its frames.
    readonly property var liveVideoSink: mediaLayer.currentItem ? mediaLayer.currentItem.videoSink : null
    // A slide held back until the media that goes with it can be shown: see
    // showSlideWithMedia(). Undefined when none is; null is a slide layer to be cleared.
    property var heldSlide: undefined

    // `slide` is a slide map from Catalog.open(); null clears the layer.
    function showSlide(slide) {
        heldSlide = undefined
        slideLayer.show(slide)
    }

    // `media` is { source, video }; null clears the layer.
    function showMedia(media) {
        mediaLayer.show(media)
    }

    // A slide and the media its cue triggers, which are to change as one. Media takes a
    // moment to have a picture to show (see MediaContent); while the media layer waits
    // for it the slide waits as well, and the two layers then make their change in the
    // same frame.
    function showSlideWithMedia(slide, media) {
        // A slide still held for other media has been passed over.
        heldSlide = undefined
        mediaLayer.show(media)
        if (mediaLayer.waiting)
            heldSlide = slide
        else
            showSlide(slide)
    }

    objectName: "output"
    title: "Output"

    TransitionLayer {
        id: mediaLayer

        anchors.fill: parent
        shader: win.shader
        options: win.options
        tint: win.tint
        direction: win.direction
        duration: win.duration
        delegate: MediaContent {}
        // The media layer has stopped waiting, to make its change or because it was
        // given something else to show: either way a slide held for it goes now.
        onWaitingChanged: {
            if (!waiting && win.heldSlide !== undefined)
                win.showSlide(win.heldSlide)
        }
    }

    TransitionLayer {
        id: slideLayer

        anchors.fill: parent
        shader: win.shader
        options: win.options
        tint: win.tint
        direction: win.direction
        duration: win.duration
        delegate: Slide {}
    }
}
