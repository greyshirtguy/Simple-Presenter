import QtQuick
import SimplePresenterApp

// An audience screen in a window: an OutputScene, in a small window of its own that can
// fill a display, or filling the display the screen is set to (see AuxWindow, and
// Screens for what a screen is sent out through).
//
// What it is asked and what it says are the scene's, passed straight through.
AuxWindow {
    id: win

    signal mediaEnded(int playId)
    signal markerReached(int playId, var marker, string layer)
    // The markers a playing goes by now (see MediaContent.markersOf)
    property var markersOf: null

    property alias shader: scene.shader
    property alias options: scene.options
    property alias tint: scene.tint
    property alias direction: scene.direction
    property alias duration: scene.duration
    property alias transitionName: scene.transitionName
    property alias leads: scene.leads
    property alias slideOn: scene.slideOn
    property alias mediaOn: scene.mediaOn
    property alias announcementsOn: scene.announcementsOn
    property alias propsOn: scene.propsOn
    property alias messagesOn: scene.messagesOn
    property alias videoInputOn: scene.videoInputOn
    property alias videoInput: scene.videoInput
    property alias lookFade: scene.lookFade
    property alias props: scene.props
    property alias propsDuration: scene.propsDuration
    property alias messages: scene.messages
    property alias mask: scene.mask
    property alias pictures: scene.pictures
    // The screen's video delay, in its frames (see OutputScene); a frame being one of the
    // display's refreshes
    property alias videoDelay: scene.videoDelay
    // How long a frame of it is, in milliseconds: a refresh of the display it is on
    readonly property real frameInterval: scene.frameInterval
    // Its statistics (see OutputScene)
    property alias showStatistics: scene.showStatistics
    property alias statisticsTitle: scene.statisticsTitle
    // The part of a screen of several outputs this window draws (see OutputScene)
    property alias columns: scene.columns
    property alias rows: scene.rows
    property alias column: scene.column
    property alias row: scene.row
    readonly property var liveVideoSink: scene.liveVideoSink
    readonly property var livePlayer: scene.livePlayer
    readonly property var liveAnnouncementPlayer: scene.liveAnnouncementPlayer
    readonly property var heldSlide: scene.heldSlide

    function showSlide(slide, transition, times) {
        scene.showSlide(slide, transition, times)
    }

    function stepBuilds(times) {
        scene.stepBuilds(times)
    }

    function showMedia(media) {
        scene.showMedia(media)
    }

    function showSlideWithMedia(slide, media, transition, times) {
        scene.showSlideWithMedia(slide, media, transition, times)
    }

    function showSlideOverMedia(slide, transition, times) {
        scene.showSlideOverMedia(slide, transition, times)
    }

    function showAnnouncement(slide, showing, media) {
        scene.showAnnouncement(slide, showing, media)
    }

    function clearAnnouncements() {
        scene.clearAnnouncements()
    }

    objectName: "output"
    title: "Output"

    OutputScene {
        id: scene

        anchors.fill: parent
        onMediaEnded: (playId) => win.mediaEnded(playId)
        onMarkerReached: (playId, marker, layer) => win.markerReached(playId, marker, layer)
        markersOf: win.markersOf
        frameInterval: Screens.refreshIntervalOf(win.screen ? win.screen.name : "")
        drawnBy: "the refreshes of the display it is on"
    }
}
