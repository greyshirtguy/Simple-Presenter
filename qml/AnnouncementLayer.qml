import QtQuick

// The announcements layer of an audience screen: a second presentation, shown at the
// same time as the one on the slide and media layers and over both (see
// show::State::announcement in src/showstate.h for what it is and the rules it goes
// by). It is one layer to the looks and to its clear button, and inside it is the pair
// that the screen itself is: media, and a slide over it, each making its own
// transition, so that a notice with a video under its words is a notice here too.
//
// It is told what to show as a scene is (OutputScene.qml), and by the same three ways
// of showing a slide: by itself, over the media that is playing, or with media started
// under it. A slide that is to change with its media waits for the media to have a
// picture, as it does there.
//
// One of these is made for a screen when the first announcement is shown, and kept: a
// show that has no announcements has nothing made for them.
Item {
    // (Not `layer`, which is a property every Item has: inside a delegate that is
    // found before an id of the file around it.)
    id: announcements

    // The transition its changes are made by, as the scene's are (see TransitionLayer)
    property string shader: ""
    property vector4d options
    property vector4d tint
    property vector2d direction
    property int duration: 0
    property string transitionName: ""
    // Whether this plays a video itself, or is fed the frames of the scene that does
    // (see MediaContent)
    property bool leads: true
    // The operator window, for Screen Previews (see Slide)
    property var pictures: null
    // A slide held back until the media that goes with it can be shown; undefined
    // when none is
    property var heldSlide: undefined
    // The MediaPlayer of the video under its slide, or null: what the transport for the
    // announcements works
    readonly property var livePlayer: mediaLayer.currentItem ? mediaLayer.currentItem.player : null

    function showSlide(slide) {
        heldSlide = undefined
        slideLayer.show(slide)
    }

    // `media` is as a scene is given it, or null for none: the media that was there
    // goes, and the slide is shown with nothing under it.
    function showSlideWithMedia(slide, media) {
        heldSlide = undefined
        mediaLayer.show(media)
        if (mediaLayer.waiting)
            heldSlide = slide
        else
            showSlide(slide)
    }

    function showSlideOverMedia(slide) {
        if (mediaLayer.waiting)
            heldSlide = slide
        else
            showSlide(slide)
    }

    // Takes the whole layer off: the slide, and the media under it.
    function clear() {
        heldSlide = undefined
        slideLayer.show(null)
        mediaLayer.show(null)
    }

    TransitionLayer {
        id: mediaLayer

        anchors.fill: parent
        name: "announcements' media"
        shaderName: announcements.transitionName
        shader: announcements.shader
        options: announcements.options
        tint: announcements.tint
        direction: announcements.direction
        duration: announcements.duration
        delegate: MediaContent {
            leads: announcements.leads
        }
        onWaitingChanged: {
            if (!waiting && announcements.heldSlide !== undefined)
                announcements.showSlide(announcements.heldSlide)
        }
    }

    TransitionLayer {
        id: slideLayer

        anchors.fill: parent
        name: "announcements' slide"
        shaderName: announcements.transitionName
        shader: announcements.shader
        options: announcements.options
        tint: announcements.tint
        direction: announcements.direction
        duration: announcements.duration
        delegate: Slide {
            pictures: announcements.pictures
        }
    }
}
