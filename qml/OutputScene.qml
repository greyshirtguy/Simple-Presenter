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
// the network (NdiScreen) or out of a card (DeckLinkScreen); and for a screen of several
// outputs drawn as one, one for each output, which draws its part of the whole. Each is told the same things and draws them for itself, at
// its own size. That each screen has layers of its own is also what will let a look
// give one screen the slides and another only the media.
//
// It knows nothing about presentations. It is handed things to show (a slide map, or a
// media file) and shows them; what is live is Show's business.
//
// A screen can be given a video delay, as ProPresenter's outputs can: all of it then shown
// so many of its frames late (`videoDelay`; FrameDelay says how), whether it is in a
// window or sent out of sight.
//
// Left as it was, for now: the fades of the layers as a look changes (lookFade), and of
// the props and messages as they come and go, are Qt's own animations (a Behavior, a
// NumberAnimation), which Qt steps by a timer of its own while two windows are showing,
// out of time with the screen, where a transition, scrolling text and builds keep to the
// screen's refreshes (see src/frameclock.h). They come seldom, beside the slides'
// changes; moved to a FrameClock, they would not take a playing video's turn either.
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
    // The screen's video delay: so many of its frames late (see FrameDelay), and how long
    // a frame of the screen is, in milliseconds (its display's refresh, or the rate it is
    // sent at). 0 for none, which is every screen until one is set.
    property int videoDelay: 0
    property real frameInterval: 1000 / 60
    // The part of its screen this draws, for a screen of several outputs drawn as one (see
    // Screens): so many columns and rows of outputs, and which this one is, from the top
    // left. Each part is a scene of its own, drawing the whole moved over itself, so
    // that what goes out through each output is its share of one picture. One and one for
    // a screen of one output.
    property int columns: 1
    property int rows: 1
    property int column: 0
    property int row: 0
    // Its performance statistics, shown over all of it while asked for (ProPresenter's Show
    // Performance Statistics On Screen; Screens.showStatistics): what the screen is, and
    // what it is drawn by, as whoever has it says
    property bool showStatistics: false
    property string statisticsTitle: ""
    property string drawnBy: ""

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

    onMessagesChanged: {
        if (messages.length > 0)
            messagesLayer.wanted = true
    }

    // The whole of the screen, of which this draws its part (see `columns`): the size of
    // all of its outputs together, moved so that this one's part is over this item, and
    // what this is drawn into (its window, or what is drawn out of sight for NDI or a
    // card) is that part. For a screen of one output it is this item, as it always was.
    Item {
        id: view

        x: -scene.column * scene.width
        y: -scene.row * scene.height
        width: scene.width * scene.columns
        height: scene.height * scene.rows

        // The layers, in one item, so that they can be drawn into a layer of Qt's for the
        // screen's video delay (FrameDelay, below): while there is one, the layer is moved out
        // of the window, where Qt still draws it and nothing of it is seen, and what is seen
        // is the delay's copy of it from so many frames before. Without a delay it is drawn
        // as it always was: no layer is made, and the move is none.
        Item {
            id: content

            anchors.fill: parent
            layer.enabled: frameDelay.active
            transform: Translate { x: frameDelay.active ? -4 * (view.width + 100) : 0 }

            // What is under the layers over the slides, for their fills that are a blur or an
            // invert of what is behind them (Slide's `backdrop`, TextFills): the slides and
            // what is under them. (A GUESS for the copyright line, announcements, props,
            // messages and masks alike: each takes what is under the slides and the slides,
            // and not the layers between it and them.) Nothing is drawn for it but what is
            // in it, as before; it is taken only by an element that has such a fill.
            Item {
                id: underLayers

                anchors.fill: parent

                // What is under the slides: the video input and the media, which the slides'
                // fills take. (Shown by its layers' opacities, not their `visible`, which is
                // false while this is hidden, so that it would never be shown again.)
                Item {
                    id: underSlide

                    anchors.fill: parent
                    visible: videoInputLayer.opacity > 0 || mediaLayer.opacity > 0

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

                    // The video on the media layer, while one plays, is what the screen's frames keep to
                    // (see src/frameclock.h)
                    FramePace {
                        video: scene.liveVideoSink
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
                        // What a fill of the slide's that is a blur or an invert of what is behind
                        // it takes (TextFills): the layers under it
                        backdrop: underSlide
                    }
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
                    backdrop: underLayers
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
                    backdrop: underLayers
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
                backdrop: underLayers
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
                    backdrop: underLayers
                }
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
                    backdrop: underLayers
                }
            }
        }

        FrameDelay {
            id: frameDelay

            objectName: "frameDelay"
            anchors.fill: parent
            visible: active
            source: content
            frames: scene.videoDelay
            frameInterval: scene.frameInterval
        }
    }

    // The statistics: over the delay's copy and not in it, so that they are of now. Made
    // only while asked for, and nothing of them otherwise; while they are, the screen is
    // drawn once a second more, for their numbers. What the app can see of the sound is
    // its own share of its delay (AudioMixer.heardAfter): Qt does not ask the system's
    // sound service how late it plays, so the rest is not known, and is said not to be.
    Loader {
        anchors.fill: parent
        active: scene.showStatistics
        sourceComponent: Component {
            Item {
                id: statistics

                readonly property real margin: Math.max(6, Math.round(scene.height / 60))
                readonly property var heard: AudioMixer.heardAfter
                // A rate in a few figures: 60, 59.94, 29.97
                function rate(perSecond) {
                    return String(Number(perSecond.toFixed(2)))
                }

                FramesDrawn {
                    id: drawn

                    counting: true
                }

                Rectangle {
                    x: statistics.margin
                    y: statistics.margin
                    width: lines.width + 2 * statistics.margin
                    height: lines.height + statistics.margin
                    radius: statistics.margin / 2
                    color: "#d8000000"

                    Text {
                        id: lines

                        objectName: "statistics"
                        x: statistics.margin
                        y: statistics.margin / 2
                        width: Math.min(implicitWidth, scene.width - 4 * statistics.margin)
                        wrapMode: Text.Wrap
                        color: "white"
                        font.pixelSize: Math.max(11, Math.round(scene.height / 54))
                        text: {
                            const frame = scene.frameInterval
                            const heard = statistics.heard
                            const size = Math.round(scene.width * Screen.devicePixelRatio) + " × " + Math.round(scene.height * Screen.devicePixelRatio)
                            return [(scene.statisticsTitle !== "" ? scene.statisticsTitle + " · " : "") + size,
                                    "Drawn " + drawn.perSecond + (drawn.perSecond === 1 ? " time" : " times") + " in the last second, at most "
                                    + Math.round(drawn.longestGap) + " ms apart; its own frames come " + statistics.rate(1000 / frame) + " times a second",
                                    "Drawn by " + scene.drawnBy,
                                    "Video delay: " + (scene.videoDelay > 0 ? scene.videoDelay + (scene.videoDelay === 1 ? " frame, " : " frames, ")
                                                                              + Math.round(scene.videoDelay * frame) + " ms" : "none"),
                                    "Sound: " + (heard.total === undefined ? "none heard yet"
                                                 : "heard at least " + heard.total + " ms after it is played, " + (heard.total / frame).toFixed(1)
                                                   + " of these frames (" + heard.mixer + " ms in the app's mixer, up to " + heard.qt
                                                   + " in Qt's; the system's sound service and the card hold more, which the app is not told)")
                                   ].join("\n")
                        }
                    }
                }
            }
        }
    }
}
