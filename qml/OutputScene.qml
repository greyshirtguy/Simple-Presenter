import QtQuick

// What an audience screen shows: the media layer, the slide layer over it and the props
// over both, on black.
//
// Layers are what make a presenter more than a slide show. A background video on the
// media layer keeps playing while the slides over it change, and either layer can be
// changed or cleared without touching the other. Each is a TransitionLayer, so each
// makes its own transition; the slide layer is transparent wherever a slide has
// nothing, and the media shows through. The props are not one thing shown in place of
// another but any number at once, each coming and going by itself (see PropsLayer).
//
// There is one of these for every audience screen, whatever the screen is sent out
// through: in a window of its own (Output), or drawn where nobody sees it and sent over
// the network (NdiScreen). Each is told the same things and draws them for itself, at
// its own size. That each screen has layers of its own is also what will let a look
// give one screen the slides and another only the media.
//
// It knows nothing about presentations. It is handed things to show (a slide map, or a
// media file) and shows them; what is live is Show's business.
Item {
    id: scene

    // Applied to the next change on either layer: a .qsb url, or "" to cut; what the
    // shader is handed for the transition's options (see TransitionLayer); milliseconds.
    property string shader: ""
    property vector4d options
    property vector4d tint
    property vector2d direction
    property int duration: 0
    // Whether this scene plays the media itself. One scene does, for all of them: a
    // video is decoded once, and the others are handed its frames (see MediaContent).
    property bool leads: true
    // What that transition is called, for the log
    property string transitionName: ""
    // The QVideoSink of the video on the media layer, or null; lets a preview borrow its frames.
    readonly property var liveVideoSink: mediaLayer.currentItem ? mediaLayer.currentItem.videoSink : null
    // The MediaPlayer of the video on the media layer, or null; lets a transport control work it.
    readonly property var livePlayer: mediaLayer.currentItem ? mediaLayer.currentItem.player : null
    // The props that are on, the first at the bottom: [{ id, slide }]; and how long one
    // takes to come or go, in milliseconds
    property alias props: propsLayer.props
    property alias propsDuration: propsLayer.duration
    // A slide held back until the media that goes with it can be shown: see
    // showSlideWithMedia(). Undefined when none is; null is a slide layer to be cleared.
    property var heldSlide: undefined

    // `slide` is a slide map from Catalog.open(); null clears the layer.
    function showSlide(slide) {
        heldSlide = undefined
        slideLayer.show(slide)
    }

    // `media` is { source, video, loops, volume }; null clears the layer.
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

    // A slide whose media is the media already there, which is left as it is. If that
    // media is still being waited for, having only just been asked for, the slide waits
    // with it as the slide that asked for it would have.
    function showSlideOverMedia(slide) {
        if (mediaLayer.waiting)
            heldSlide = slide
        else
            showSlide(slide)
    }

    TransitionLayer {
        id: mediaLayer

        anchors.fill: parent
        name: "media layer"
        shaderName: scene.transitionName
        shader: scene.shader
        options: scene.options
        tint: scene.tint
        direction: scene.direction
        duration: scene.duration
        delegate: MediaContent {
            leads: scene.leads
        }
        // The media layer has stopped waiting, to make its change or because it was
        // given something else to show: either way a slide held for it goes now.
        onWaitingChanged: {
            if (!waiting && scene.heldSlide !== undefined)
                scene.showSlide(scene.heldSlide)
        }
    }

    TransitionLayer {
        id: slideLayer

        anchors.fill: parent
        name: "slide layer"
        shaderName: scene.transitionName
        shader: scene.shader
        options: scene.options
        tint: scene.tint
        direction: scene.direction
        duration: scene.duration
        delegate: Slide {}
    }

    PropsLayer {
        id: propsLayer

        anchors.fill: parent
    }
}
