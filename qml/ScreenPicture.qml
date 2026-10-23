import QtQuick
import QtQuick.Window
import QtMultimedia
import SimplePresenterApp

// A picture of what one of the workspace's screens is showing: the operator window's
// preview of the output is one, and so is a shape on a slide or a stage layout whose
// fill is a Screen Preview, which is ProPresenter's name for it (see SlideElement).
//
// How it is made. Not by copying the pixels of the screen's own window: each window is
// drawn by itself, and nothing a window has drawn can be read by another without a trip
// through the computer's memory that would cost more than drawing it again. So a
// picture draws the screen again, from the same things the screen is told, and from
// cheap parts of them:
//
//   - The slide is drawn again, at this size, which is little more than its words.
//     Its builds run as the screen's do. On a screen (a stage layout's Screen Preview,
//     say), it changes as the screen's does, by the screen's transition when the live
//     slide changes, made again at this size (a cut for any other change, such as a look
//     giving the screen another theme), and its moving parts move (scrolling text, a
//     video input in a shape: see below). In the operator window the same, the preview
//     and the pictures in what it shows (the Screen Previews of a stage layout), unless
//     the settings for them are off, when a change is a cut (see `moving`).
//   - A still picture is the small one the thumbnails already have, and read from its
//     file at this size only for a picture too large for that to look right.
//   - A video is not played again. Its frames are borrowed from the decoder that is
//     playing it for the screens (see FrameRelay), `videoInterval` apart: a handful a
//     second in the operator's preview; for a picture on a screen, as many as are
//     asked for, every one of them by default. Each borrowed frame is a redraw of the
//     window the picture is in, which is the dear part of all this: see SlideElement
//     for what is chosen there and why.
//   - The props, the messages and the announcements are their layers again, as the
//     screens have them, with nothing of their own to play.
// So a picture costs roughly a slide more to draw, and to draw again as often as the
// screen changes: at every frame while a transition, a build or scrolling text moves
// on the screen, as the screen itself is. A screen that shows nothing new has a picture
// that is not drawn again either. (Measured: see benchmark/README.md.)
//
// It is of a screen by its id, or failing that by its name, which is how ProPresenter's
// file names it; with neither found, it shows nothing. Of an audience screen it shows
// what the live look gives that screen (its layers, its theme, its mask), as the
// screen does; of a stage screen, the stage layout it has. The screen is fitted in,
// whole, on black where it shows nothing, and the picture is empty around it.
//
// A picture's own slides do not have pictures of their own unless `inner` says so:
// a screen whose slide has a picture of itself would otherwise have a picture inside a
// picture inside a picture, without end. So the pictures stop at one: the operator's
// preview, which stands for a screen, shows the pictures the screen shows (`inner`),
// and a picture on a screen shows its screen without any. (A GUESS at ProPresenter,
// whose screens are drawn in another way, and which may well show the picture inside
// itself, once or more.) Where it moves (`moving`), its slides are told all the same that
// they are on show, by `onShow` in place of the operator window: what moves on a screen
// moves in them, and a Screen Preview in them draws nothing (see SlideElement).
Item {
    id: picture

    // The operator window: what each screen is being told is its to say. A picture tells
    // it that it is there, so that it is told what the screens are told of the
    // announcements, which are not something it can look up (see pictureMade in
    // Main.qml).
    required property var win
    // Which screen: its id, and its name, by which it is found where the id is not
    // (a workspace whose screens were made again, in ProPresenter or here)
    property string screenId: ""
    property string screenName: ""
    // Whether its slides draw the Screen Previews in them (see above)
    property bool inner: false
    // Whether to draw shadows and faded edges (see SlideElement)
    property bool effects: false
    // How many milliseconds apart the frames of a playing video are borrowed: 0 for
    // every one of them
    property int videoInterval: 0
    // What the parts are called, for the tests to find them by: the operator's preview
    // gives its own ("preview" makes previewSlide, previewVideo and the rest). A picture
    // of a screen has none, there being any number of them.
    property string namePrefix: ""

    // What passes the video's frames on, which says each time it does: what else in
    // the window follows the video can change then, at no cost of its own (the
    // operator's transport does: see FrameRelay)
    readonly property alias relay: frames

    // The screen, or null
    readonly property var screen: {
        const screens = Screens.screens
        return (screenId !== "" ? screens.find(candidate => candidate.id === screenId) : undefined)
            ?? (screenName !== "" ? screens.find(candidate => candidate.name === screenName) : undefined) ?? null
    }
    readonly property bool stage: screen !== null && screen.kind === "stage"
    // What the live look gives an audience screen, and the mask it gives it, as the
    // operator window works them out for its preview (previewLook and previewMask in
    // Main.qml). The slide is worked out where it is drawn: see the Slide below.
    readonly property var look: screen !== null && !stage && Looks.live !== undefined ? Looks.liveOf(screen.id)
                                                                                      : ({ slide: true, media: true, props: true, messages: true, theme: "" })
    readonly property var mask: screen !== null && !stage ? win.maskSlide(look.mask) : null
    // The slides of the picture, with pictures of their own or only told that they are on
    // show (see the top)
    readonly property var onShow: ({ inPicture: true })
    readonly property var pictures: inner ? win : moving ? onShow : null
    // Whether the picture is on a screen, and not in the operator window (see the top).
    // (Set as the picture is put in a window. Not called onScreen, which QML takes for a
    // handler of `screen`'s changes.)
    property bool drawnOnScreen: false
    // Whether it moves as the screen does: on a screen, always; in the operator window, as
    // the settings there say, one for the preview itself (movingPreview in Main.qml) and one
    // for the pictures in what the preview shows, the Screen Previews of a stage layout, say
    // (movingPreviews). The preview moving costs the output some milliseconds at every slide
    // change and transition, the operator window drawing beside it (benchmark, 2026-10-23).
    readonly property bool moving: drawnOnScreen || (inner ? win.movingPreview : win.movingPreviews)
    Window.onWindowChanged: {
        picture.drawnOnScreen = picture.Window.window !== null && picture.Window.window !== picture.win
    }
    // The live slide as the screen has it, for the slide layer below. (Asked of the
    // operator window here, and not kept in a property of the operator window's for this
    // to read: passed on that way, a slide cost the benchmark's slides of shapes two
    // milliseconds more a click, in the operator's preview alone. Handed the look, which
    // follows the live look: asked for inside slideFor, it is no part of the binding, and
    // a look giving the screen another theme while a slide is on show went unseen here
    // until the next slide.)
    readonly property var shownSlide: !moving || screen === null || stage ? null : win.slideFor(screen.id, win.liveSlide, look)
    // The last change of the live slide this has made (see Main's slideChange)
    property int changesSeen: -1

    // Brings in the slide as it now is: by the transition the screens make, if the live
    // slide has changed since the last time, and otherwise at once. (Called a moment after
    // the slide is known to have changed, by when the operator window has said whether
    // that was the live slide changing: see slideChange, which it says as the screens
    // are told, after the slide has changed.)
    function present() {
        if (!movingSlide.item)
            return
        const change = win.slideChange
        const live = change.serial !== changesSeen
        changesSeen = change.serial
        movingSlide.item.show(shownSlide, { buildTimes: screen !== null ? win.buildTimesFor(screen.id) : null },
                        live ? change.transition : { name: "", shader: "", duration: 0 })
    }

    onShownSlideChanged: presenting.restart()

    // (A timer of its own rather than Qt.callLater, which can come after the picture is
    // gone: one of a shape's fill goes when the shape does)
    property Timer presenting: Timer {
        interval: 0
        onTriggered: picture.present()
    }

    function name(part) {
        return namePrefix === "" ? "" : namePrefix + part
    }

    // What the screens are told of the announcements (see showAnnouncement() and
    // clearAnnouncements() in OutputScene.qml): the layer is made at the first, and kept.
    function showAnnouncement(slide, showing, media) {
        if (stage)
            return
        announcements.active = true
        const layer = announcements.item
        if (showing === 2)
            layer.showSlideWithMedia(slide, media)
        else if (showing === 1)
            layer.showSlideOverMedia(slide)
        else
            layer.showSlide(slide)
    }

    function clearAnnouncements() {
        if (announcements.item)
            announcements.item.clear()
    }

    Component.onCompleted: {
        drawnOnScreen = Window.window !== null && Window.window !== win
        win.pictureMade(picture)
    }
    Component.onDestruction: win.pictureGone(picture)

    // The screen. Each layer fills the picture and fits itself in it, as it does on the
    // screen; only the black of the screen, and a stage screen's plain view, are cut to
    // the screen's own proportions, here.
    Item {
        id: area

        // Whether the media layer has a still picture on it that this shows
        readonly property bool still: !picture.stage && picture.look.media && picture.win.liveMedia !== null && !picture.win.liveMedia.video

        anchors.fill: parent
        visible: picture.screen !== null

        // The screen, as large as it fits, in its own proportions
        Rectangle {
            id: frame

            readonly property real aspect: picture.screen !== null && picture.screen.width > 0 && picture.screen.height > 0
                                           ? picture.screen.width / picture.screen.height : 16 / 9
            anchors.centerIn: parent
            width: Math.min(picture.width, picture.height * aspect)
            height: Math.min(picture.height, picture.width / aspect)
            color: "black"
        }

        // ---- An audience screen: its layers, in the order the screen has them

        // The video input layer, under the media: the input on it, while the look gives
        // the screen the layer, `videoInterval` apart as a video is, and without its
        // sound, which is the screen's own to play (see OutputScene). Made only while
        // there is one, so a picture of a screen with none costs nothing for it.
        Loader {
            objectName: picture.name("VideoInput")
            anchors.fill: frame
            active: !picture.stage && picture.look.videoInput !== false && Show.liveVideoInput >= 0

            sourceComponent: VideoInputView {
                index: Show.liveVideoInput
                interval: picture.videoInterval
            }
        }

        // A still picture: its thumbnail, or its file at this size where the thumbnail
        // is too small for it (the thumbnails are a few hundred pixels across). Each is
        // drawn as the media layer draws it, as its Media Inspector sets it (MediaPicture,
        // MediaLook): the thumbnail is made cropped, turned and flipped already.
        MediaPicture {
            anchors.fill: parent
            visible: area.still && frame.width <= 400
            source: visible ? picture.win.thumbnailUrl(picture.win.liveMedia.path, picture.win.liveMedia.settings) : ""
            settings: visible ? picture.win.liveMedia.settings ?? null : null
            shaped: true
        }

        MediaPicture {
            anchors.fill: parent
            visible: area.still && frame.width > 400
            source: visible ? picture.win.liveMedia.source : ""
            settings: visible ? picture.win.liveMedia.settings ?? null : null
            readSize: Qt.size(Math.max(1, Math.ceil(frame.width * Screen.devicePixelRatio)),
                              Math.max(1, Math.ceil(frame.height * Screen.devicePixelRatio)))
        }

        MediaLook {
            id: videoLook

            anchors.fill: parent
            visible: !picture.stage && picture.look.media && picture.win.liveMedia !== null && picture.win.liveMedia.video
            settings: visible ? picture.win.liveMedia.settings ?? null : null
            sourceWidth: video.sourceRect.width
            sourceHeight: video.sourceRect.height

            VideoOutput {
                id: video

                objectName: picture.name("Video")
                anchors.fill: parent
                fillMode: videoLook.videoFill
            }
        }

        // (No frames are borrowed for a video the look keeps from the screen, nor for a
        // stage screen.)
        FrameRelay {
            id: frames

            source: videoLook.visible && picture.win.output ? picture.win.output.liveVideoSink : null
            target: video.videoSink
            interval: picture.videoInterval
        }

        // The slide that is live, as it is in the theme the look gives the screen. In the
        // operator window: drawn as it is, each change a cut. (Asked of the operator window
        // here, and not kept in a property of the picture's for this to read: passed on that
        // way, a slide cost the benchmark's slides of shapes two milliseconds more a click,
        // in the operator's preview alone. Handed the look, which follows the live look:
        // asked for inside slideFor, it is no part of the binding, and a look giving the
        // screen another theme while a slide is on show went unseen here until the next
        // slide.)
        Loader {
            anchors.fill: parent
            active: !picture.moving
            visible: active && !picture.stage && picture.look.slide

            sourceComponent: Slide {
                objectName: picture.name("Slide")
                slide: picture.screen === null || picture.stage ? null : picture.win.slideFor(picture.screen.id, picture.win.liveSlide, picture.look)
                // Its builds run as the screen's do, from the same times
                buildTimes: picture.screen !== null ? picture.win.buildTimesFor(picture.screen.id) : null
                effects: picture.effects
                pictures: picture.pictures
            }
        }

        // On a screen: changed as the screen changes it, through the same transition, drawn
        // at this size (shownSlide, present()). While one runs, the two slides are drawn into
        // textures and mixed by its shader, and the screen this is on is drawn at every frame
        // of it, as the screen it is of is; between them the slide is drawn as it is.
        Loader {
            id: movingSlide

            anchors.fill: parent
            active: picture.moving
            visible: active && !picture.stage && picture.look.slide
            onLoaded: {
                picture.changesSeen = picture.win.slideChange.serial
                picture.present()
            }

            sourceComponent: TransitionLayer {
                // (The slide on show, as the plain Slide above has it, for the tests)
                readonly property var slide: currentItem ? currentItem.slide : null

                objectName: picture.name("Slide")
                name: "screen preview"
                shaderName: picture.win.transition.name
                shader: picture.win.transitionShader
                options: picture.win.transitionUniforms.options
                tint: picture.win.transitionUniforms.tint
                direction: picture.win.transitionUniforms.direction
                duration: Math.round(picture.win.transitionDuration * 1000)
                delegate: Slide {
                    effects: picture.effects
                    pictures: picture.pictures
                    logName: "screen preview"
                }
            }
        }

        // Its builds run as the screen's do, from the same times, which change as each
        // step of them is begun
        Binding {
            target: movingSlide.item ? movingSlide.item.currentItem : null
            when: movingSlide.item !== null && movingSlide.item.currentItem !== null
            property: "buildTimes"
            value: picture.screen !== null ? picture.win.buildTimesFor(picture.screen.id) : null
        }

        // The copyright display's line over it, as the screens have it (see OutputScene)
        Loader {
            objectName: picture.name("Copyright")
            anchors.fill: parent
            active: !picture.stage && picture.look.slide && Show.copyrightText !== ""
            visible: active

            sourceComponent: Slide {
                slide: CopyrightDisplay.templateSlide
                effects: picture.effects
                pictures: picture.pictures
            }
        }

        // The announcements, over the slide and under the props: their layer again,
        // fed the frames of a video that the output plays (it does not lead), and told
        // what the screens are told. Made when the first announcement is shown, and
        // kept.
        Loader {
            id: announcements

            objectName: picture.name("Announcements")
            anchors.fill: parent
            visible: !picture.stage && picture.look.announcements !== false
            active: false
            sourceComponent: AnnouncementLayer {
                leads: false
                pictures: picture.pictures
            }
        }

        PropsLayer {
            objectName: picture.name("Props")
            anchors.fill: parent
            visible: !picture.stage && picture.look.props
            props: picture.stage ? [] : picture.win.shownProps
            duration: Math.round(Props.transitionDuration * 1000)
            effects: picture.effects
            pictures: picture.pictures
        }

        // The messages, over the props: made when the first is shown, and kept
        Loader {
            id: messages

            property bool wanted: !picture.stage && picture.win.shownMessages.length > 0

            objectName: picture.name("Messages")
            anchors.fill: parent
            visible: picture.look.messages
            active: false
            onWantedChanged: {
                if (wanted)
                    active = true
            }
            Component.onCompleted: {
                if (wanted)
                    active = true
            }

            sourceComponent: PropsLayer {
                props: picture.win.shownMessages
                duration: Math.round(Props.transitionDuration * 1000)
                effects: picture.effects
                pictures: picture.pictures
            }
        }

        // The mask the live look gives the screen, over everything
        Loader {
            objectName: picture.name("Mask")
            anchors.fill: parent
            active: !picture.stage && picture.mask !== null

            sourceComponent: Slide {
                slide: picture.mask
                effects: picture.effects
                pictures: picture.pictures
            }
        }

        // ---- A stage screen: its layout, or with none the plain view
        Loader {
            anchors.fill: frame
            active: picture.stage

            sourceComponent: StageScene {
                layout: picture.win.stageSlideOf(picture.screen.id)
                currentText: picture.win.stageCurrentText
                nextText: picture.win.stageNextText
                effects: picture.effects
                pictures: picture.pictures
            }
        }
    }
}
