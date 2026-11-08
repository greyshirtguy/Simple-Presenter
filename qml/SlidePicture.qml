import QtQuick
import SimplePresenterApp

// A picture of the slide that is live, or of the one after it, as a shape's fill: what
// ProPresenter's stage layouts call Current Slide and Next Slide, which its file keeps as
// a shape whose fill only stands in, with a link to the one or the other beside it
// (Slide.Element.DataLink.SlideImage: see proconvert, which reads it as the fill
// "slideImage"). Asked for by the maintainer on 2026-11-08, for stage layouts: "This is
// not live video - just a still image...(for videos, the thumbnail)".
//
// So it is drawn as the slide's thumbnail is drawn, and is as cheap: the slide, drawn
// again at this size, over the media its cue brings, a picture by its thumbnail and a
// video by its own, which is a still; fitted in whole, in the slide's own proportions, on
// black. It changes, at once, as the live slide changes: the live one is the one on the
// slide layer, and the next the one Next would go to (as Main's liveSlide and nextSlide
// have them, which the stage's words go by too).
//
// GUESSES at ProPresenter, whose stage layouts this was not watched against: that the
// picture is of the slide as its presentation has it, in no theme a look gives a screen;
// that it has the media its own cue brings, and not whatever is on the media layer (a
// slide of a song after the first, which brings none, has none: as its thumbnail); and
// that it is black when there is no such slide (the slide layer cleared, the last slide
// live, nothing live at all).
//
// Made by SlideElement only where it is wanted, from this file by its address, as a
// Screen Preview is (a slide is made of elements, and QML will not have an element made,
// however far down, of a slide). Its own slide has no pictures in it (`pictures` null),
// as a thumbnail has none.
Item {
    id: picture

    // The operator window, which says what is live
    required property var win
    // Whether it is of the next slide; otherwise of the one that is live
    property bool next: false
    // Shadows on its words, where the shape is drawn with them (see SlideElement)
    property bool effects: false

    // The slide, or null
    readonly property var slide: next ? win.nextSlide : win.liveSlide
    readonly property var media: slide !== null && slide.media !== undefined ? slide.media : null

    // The slide, as large as it fits, in its own proportions (1920 by 1080 for one that
    // says none), on black
    Item {
        id: area

        readonly property real aspect: picture.slide !== null && picture.slide.width > 0 && picture.slide.height > 0
                                       ? picture.slide.width / picture.slide.height : 16 / 9

        anchors.centerIn: parent
        width: Math.min(picture.width, picture.height * aspect)
        height: Math.min(picture.height, picture.width / aspect)

        Rectangle {
            anchors.fill: parent
            color: "black"
        }

        // Its media, as its thumbnail has it: the thumbnail, and for media set to anything in
        // its Media Inspector, drawn as it is set (as the slides' grid draws it)
        Item {
            id: under

            anchors.fill: parent

            Image {
                anchors.fill: parent
                visible: picture.media !== null && picture.media.settings === undefined
                source: visible ? picture.win.thumbnailUrl(picture.media.path) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            Loader {
                anchors.fill: parent
                active: picture.media !== null && picture.media.settings !== undefined

                sourceComponent: MediaPicture {
                    source: picture.win.thumbnailUrl(picture.media.path, picture.media.settings)
                    settings: picture.media.settings
                    shaped: true
                    animated: false
                }
            }
        }

        Slide {
            objectName: "slidePicture"
            anchors.fill: parent
            visible: picture.slide !== null
            slide: picture.slide
            effects: picture.effects
            backdrop: under
            // (Its words painted on a thread of their own, as the stage's are: changed a
            // frame after the rest, and not waited for)
            paintsAhead: true
        }
    }
}
