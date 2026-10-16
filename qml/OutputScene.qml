import QtQuick

// What an audience screen shows: the media layer, the slide layer over it, the
// announcements over those, the props over all three, the messages over them and the
// screen's mask, if the look gives it one, over everything, on black.
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
    property int lookFade: 0
    // The mask the live look gives this screen: its slide, to be laid over everything
    // else, or null for none (see Masks)
    property var mask: null
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
    // The messages that are shown, the same: [{ id, slide }], each a message as a slide
    // (see Messages.slideOf). They come and go as the props do, and as fast.
    property var messages: []
    // A slide held back until the media that goes with it can be shown: see
    // showSlideWithMedia(). Undefined when none is; null is a slide layer to be cleared.
    property var heldSlide: undefined
    // The operator window, for the Screen Previews on the slides here (see Slide)
    property var pictures: null

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
