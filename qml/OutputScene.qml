import QtQuick

// What an audience screen shows: the video input layer, the media layer over it, the
// slide layer over that, the announcements over those, the props over them all, the
// messages over them and the screen's mask, if the look gives it one, over everything,
// on black.
//
// Layers are what make a presenter more than a slide show. A background video on the
// media layer keeps playing while the slides over it change, and either layer can be
// changed or cleared without touching the other. Each is a TransitionLayer, so each
// makes its own transition; the slide layer is transparent wherever a slide has
// nothing, and the media shows through. The props are not one thing shown in place of
// another but any number at once, each coming and going by itself (see PropsLayer); and
// the messages are the same, over them.
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
    // Which of the layers this screen gets, as the look that is live has it (see
    // Looks): a layer it does not get is still kept up to date, out of sight, so that it
    // is there at once when a look gives it back; and how long a layer takes to come
    // or go when the look changes, in milliseconds.
    property bool slideOn: true
    property bool mediaOn: true
    property bool announcementsOn: true
    property bool propsOn: true
    property bool messagesOn: true
    property bool videoInputOn: true
    property int lookFade: 0
    // The video input layer: the input on it, by its place in the workspace's list of
    // them (as Show has it: see VideoInputs), -1 for none
    property int videoInput: -1
    // The mask the live look gives this screen: its slide, to be laid over everything
    // else, or null for none (see Masks)
    property var mask: null
    // What that transition is called, for the log
    property string transitionName: ""
    // The QVideoSink of the video on the media layer, or null; lets a preview borrow its frames.
    readonly property var liveVideoSink: mediaLayer.currentItem ? mediaLayer.currentItem.videoSink : null
    // The MediaPlayer of the video on the media layer, or null; lets a transport control work it.
    readonly property var livePlayer: mediaLayer.currentItem ? mediaLayer.currentItem.player : null
    // The same for the video on the announcements layer
    readonly property var liveAnnouncementPlayer: announcementsLayer.item ? announcementsLayer.item.livePlayer : null
    // The video on the media layer has played to its end, by the number of its playing:
    // said only by the scene that plays it (see MediaContent.ended)
    signal mediaEnded(int playId)
    // A playback marker of the video on the media layer ("media") or on the announcements
    // layer ("announcements") has been reached (see MediaContent.markerReached)
    signal markerReached(int playId, var marker, string layer)
    // The markers a playing goes by now (see MediaContent.markersOf)
    property var markersOf: null
    // The props that are on, the first at the bottom: [{ id, slide }]; and how long one
    // takes to come or go, in milliseconds
    property alias props: propsLayer.props
    property alias propsDuration: propsLayer.duration
    // The messages that are shown, the same: [{ id, slide }], each a message as a slide
    // (see Messages.slideOf). They come and go as the props do, and as fast.
    property var messages: []
    // A slide held back until the media that goes with it can be shown: see
    // showSlideWithMedia(). Undefined when none is; null is a slide layer to be cleared.
    // And how it is to be shown when it is: { transition, times } (see showSlide).
    property var heldSlide: undefined
    property var heldHow: ({})
    // The operator window, for the Screen Previews on the slides here (see Slide)
    property var pictures: null

    // `slide` is a slide map from Catalog.open(); null clears the layer. `transition`
    // is the slide's own, or its presentation's, to be made in place of the one above
    // (as TransitionCatalogue.drawn gives it), and `times` when the slide's builds run
    // (see Slide.buildTimes); either can be left out, for the one above and for a
    // slide shown whole.
    function showSlide(slide, transition, times) {
        heldSlide = undefined
        slideLayer.show(slide, { buildTimes: times ?? null }, transition)
    }

    // The slide on show has begun another step of its builds: `times` are its steps'
    // times as they are now (see Slide.buildTimes).
    function stepBuilds(times) {
        if (heldSlide !== undefined)
            heldHow = Object.assign({}, heldHow, { times: times })
        else if (slideLayer.currentItem)
            slideLayer.currentItem.buildTimes = times
    }

    // `media` is { source, video, loops, volume }; null clears the layer.
    function showMedia(media) {
        mediaLayer.show(media)
    }

    // A slide and the media its cue triggers, which are to change as one. Media takes a
    // moment to have a picture to show (see MediaContent); while the media layer waits
    // for it the slide waits as well, and the two layers then make their change in the
    // same frame.
    function showSlideWithMedia(slide, media, transition, times) {
        // A slide still held for other media has been passed over.
        heldSlide = undefined
        mediaLayer.show(media)
        hold(slide, transition, times)
    }

    // A slide whose media is the media already there, which is left as it is. If that
    // media is still being waited for, having only just been asked for, the slide waits
    // with it as the slide that asked for it would have.
    function showSlideOverMedia(slide, transition, times) {
        hold(slide, transition, times)
    }

    // A slide shown now, or held for the media layer if it is waiting
    function hold(slide, transition, times) {
        if (mediaLayer.waiting) {
            heldSlide = slide
            heldHow = { transition: transition, times: times }
        } else {
            showSlide(slide, transition, times)
        }
    }

    // The announcements layer (see AnnouncementLayer): a slide of the presentation
    // that is sent there, shown in one of the three ways a slide is (Show.Alone,
    // Show.OverMedia or Show.WithMedia, `media` being what is started under it for the
    // last, or null for none); and the layer taken off.
    function showAnnouncement(slide, showing, media) {
        announcementsLayer.wanted = true
        const layer = announcementsLayer.item
        if (showing === 2)
            layer.showSlideWithMedia(slide, media)
        else if (showing === 1)
            layer.showSlideOverMedia(slide)
        else
            layer.showSlide(slide)
    }

    function clearAnnouncements() {
        if (announcementsLayer.item)
            announcementsLayer.item.clear()
    }

    // The video input layer, at the bottom: made only while an input is on it, which
    // is what has the input captured (see VideoInputView). Its sound plays while the
    // look gives this screen the layer, and not while it does not: an input's sound is
    // played while it is on an audience screen (see VideoInputs). An input that takes
    // the place of another is a cut. (A GUESS: ProPresenter has a transition of its own
    // for video inputs, which is not done here yet.)
    Loader {
        id: videoInputLayer

        objectName: "videoInputLayer"
        anchors.fill: parent
        active: scene.videoInput >= 0
        opacity: scene.videoInputOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }

        sourceComponent: VideoInputView {
            index: scene.videoInput
            sound: scene.videoInputOn
            shown: videoInputLayer.visible
        }
    }

    TransitionLayer {
        id: mediaLayer

        anchors.fill: parent
        opacity: scene.mediaOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }
        name: "media layer"
        shaderName: scene.transitionName
        shader: scene.shader
        options: scene.options
        tint: scene.tint
        direction: scene.direction
        duration: scene.duration
        delegate: MediaContent {
            leads: scene.leads
            onEnded: (playId) => scene.mediaEnded(playId)
            onMarkerReached: (playId, marker) => scene.markerReached(playId, marker, "media")
            markersOf: scene.markersOf
        }
        // The media layer has stopped waiting, to make its change or because it was
        // given something else to show: either way a slide held for it goes now.
        onWaitingChanged: {
            if (!waiting && scene.heldSlide !== undefined)
                scene.showSlide(scene.heldSlide, scene.heldHow.transition, scene.heldHow.times)
        }
    }

    TransitionLayer {
        id: slideLayer

        anchors.fill: parent
        opacity: scene.slideOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }
        name: "slide layer"
        shaderName: scene.transitionName
        shader: scene.shader
        options: scene.options
        tint: scene.tint
        direction: scene.direction
        duration: scene.duration
        delegate: Slide {
            pictures: scene.pictures
            logName: "slide layer"
        }
    }

    // The copyright display's line over the live slide (see Show.copyrightText,
    // CopyrightDisplay): its template slide, whose box shows the line, while there is one.
    // Of the slide layer, so a look that keeps the slides from the screen keeps it too.
    // Made only while there is a line: most slides of most workspaces have none. (A GUESS
    // at ProPresenter: that it comes and goes with its slide, at once and not by the
    // slide's transition.)
    Loader {
        id: copyrightLayer

        objectName: "copyrightLayer"
        anchors.fill: parent
        active: Show.copyrightText !== "" && scene.slideOn
        visible: active

        sourceComponent: Slide {
            slide: CopyrightDisplay.templateSlide
            pictures: scene.pictures
            logName: "copyright"
        }
    }

    // The announcements, over the slides and under the props. Made when the first
    // announcement is shown, and kept: a show that has none has nothing made for them.
    Loader {
        id: announcementsLayer

        property bool wanted: false

        anchors.fill: parent
        active: wanted
        opacity: scene.announcementsOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }

        sourceComponent: AnnouncementLayer {
            leads: scene.leads
            onMarkerReached: (playId, marker) => scene.markerReached(playId, marker, "announcements")
            markersOf: scene.markersOf
            pictures: scene.pictures
            transitionName: scene.transitionName
            shader: scene.shader
            options: scene.options
            tint: scene.tint
            direction: scene.direction
            duration: scene.duration
        }
    }

    PropsLayer {
        id: propsLayer

        anchors.fill: parent
        pictures: scene.pictures
        opacity: scene.propsOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }
    }

    // The messages' layer, over everything. It is the props' layer again, a message
    // being a slide laid over the rest as a prop is. It is made when the first message
    // is shown, and kept from then on (the last message to go fades in it): a show in
    // which no message is ever shown has nothing made for them.
    Loader {
        id: messagesLayer

        property bool wanted: false

        anchors.fill: parent
        active: wanted
        opacity: scene.messagesOn ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: scene.lookFade }
        }

        sourceComponent: PropsLayer {
            props: scene.messages
            duration: propsLayer.duration
            pictures: scene.pictures
        }
    }

    onMessagesChanged: {
        if (messages.length > 0)
            messagesLayer.wanted = true
    }

    // The mask: the top of everything. A slide like any other, which covers what is
    // under it wherever something is drawn on it, and is there for as long as the look
    // gives the screen one: it has no transition and nothing clears it. Made only for a
    // screen that has one.
    Loader {
        id: maskLayer

        anchors.fill: parent
        active: scene.mask !== null

        sourceComponent: Slide {
            slide: scene.mask
            pictures: scene.pictures
        }
    }
}
