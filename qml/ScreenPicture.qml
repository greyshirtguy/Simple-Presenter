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
//     A change is a cut: the transition the screen makes is not made here as well.
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
// screen changes. A screen that shows nothing new has a picture that is not drawn
// again either.
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
// itself, once or more.)
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
    // The slides of the picture, with or without pictures of their own
    readonly property var pictures: inner ? win : null

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

    Component.onCompleted: win.pictureMade(picture)
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

        // The slide that is live, as it is in the theme the look gives the screen. (Asked
        // of the operator window here, and not kept in a property of the picture's for
        // this to read: passed on that way, a slide cost the benchmark's slides of
        // shapes two milliseconds more a click, in the operator's preview alone.)
        Slide {
            objectName: picture.name("Slide")
            anchors.fill: parent
            visible: !picture.stage && picture.look.slide
            // (Handed the look, which follows the live look: asked for inside slideFor,
            // it is no part of the binding, and a look giving the screen another theme
            // while a slide is on show went unseen here until the next slide.)
            slide: picture.screen === null || picture.stage ? null : picture.win.slideFor(picture.screen.id, picture.win.liveSlide, picture.look)
            effects: picture.effects
            pictures: picture.pictures
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
