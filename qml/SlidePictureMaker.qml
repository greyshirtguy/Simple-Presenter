import QtQuick
import SimplePresenterApp
import "medialook.js" as MediaLook

// A picture of one slide, for ProPresenter's API (GET .../thumbnail/{index}: see
// src/slidepictures.h), made in the process that makes them and nowhere else (see
// src/slidepictureworker.h): the root of a window that is never shown, drawn out of
// sight and read back.
//
// It is drawn as the grid draws a slide's thumbnail, with the same Slide and the same
// picture of its media: so the picture is the slide as it is shown, smaller, with what
// the grid draws in black drawn as ProPresenter draws it in its pictures, a checkerboard
// where the slide is transparent. (Not the grid's marks of what the slide does, its
// number, group or label: ProPresenter's pictures have none.)
Item {
    id: maker

    // A slide map from ProDocument, and the media its cue triggers (the slide's `media`),
    // or null
    property var slide: null
    property var media: null
    // The checkerboard's square, in pixels
    property int square: 8
    // Whether everything in it is drawn: the slide's pictures and the media's
    readonly property bool ready: picture.ready && (mediaImage.status !== Image.Loading)
                                  && (!looked.item || looked.item.status === undefined || looked.item.status !== Image.Loading)

    // ProPresenter's checkerboard (#222222 and #0a0a0a), painted once for each size
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const context = getContext("2d")
            context.fillStyle = "#0a0a0a"
            context.fillRect(0, 0, width, height)
            context.fillStyle = "#222222"
            for (let y = 0; y < height; y += maker.square) {
                for (let x = ((y / maker.square) % 2) * maker.square; x < width; x += 2 * maker.square)
                    context.fillRect(x, y, maker.square, maker.square)
            }
        }
    }

    // The media, as the grid's thumbnail draws it (SlideGrid's cellMedia)
    Item {
        id: mediaItem

        anchors.fill: parent

        Image {
            id: mediaImage

            anchors.fill: parent
            visible: maker.media !== null && maker.media !== undefined && maker.media.settings === undefined
            source: visible ? "image://thumbnail/" + encodeURIComponent(maker.media.path) + "?0" : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        Loader {
            id: looked

            anchors.fill: parent
            active: maker.media !== null && maker.media !== undefined && maker.media.settings !== undefined

            sourceComponent: MediaPicture {
                source: "image://thumbnail/" + encodeURIComponent(maker.media.path) + "?0" + MediaLook.thumbnailLook(maker.media.settings)
                settings: maker.media.settings
                shaped: true
                animated: false
            }
        }
    }

    Slide {
        id: picture

        anchors.fill: parent
        slide: maker.slide
        effects: false
        backdrop: mediaItem
    }
}
