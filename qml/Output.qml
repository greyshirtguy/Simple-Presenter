import QtQuick

// An audience screen in a window: an OutputScene, in a small window of its own that can
// fill a display, or filling the display the screen is set to (see AuxWindow, and
// Screens for what a screen is sent out through).
//
// What it is asked and what it says are the scene's, passed straight through.
AuxWindow {
    id: win

    property alias shader: scene.shader
    property alias options: scene.options
    property alias tint: scene.tint
    property alias direction: scene.direction
    property alias duration: scene.duration
    property alias transitionName: scene.transitionName
    property alias leads: scene.leads
    property alias slideOn: scene.slideOn
    property alias mediaOn: scene.mediaOn
    property alias propsOn: scene.propsOn
    property alias lookFade: scene.lookFade
    property alias props: scene.props
    property alias propsDuration: scene.propsDuration
    readonly property var liveVideoSink: scene.liveVideoSink
    readonly property var livePlayer: scene.livePlayer
    readonly property var heldSlide: scene.heldSlide

    function showSlide(slide) {
        scene.showSlide(slide)
    }

    function showMedia(media) {
        scene.showMedia(media)
    }

    function showSlideWithMedia(slide, media) {
        scene.showSlideWithMedia(slide, media)
    }

    function showSlideOverMedia(slide) {
        scene.showSlideOverMedia(slide)
    }

    objectName: "output"
    title: "Output"

    OutputScene {
        id: scene

        anchors.fill: parent
    }
}
