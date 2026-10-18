import QtQuick
import SimplePresenterApp

// A still picture drawn as its Media Inspector sets it (src/mediasettings.h), as cheaply
// as it can be: nothing at all while there is no picture; an Image of its own, as
// pictures were always drawn, when it is set to nothing but how it is fitted; and only
// when it is set to more (cropped, turned, flipped, blurred, given effects) the Image
// inside a MediaLook, which works those out.
//
// It is what an element filled with a picture has (SlideElement.qml), and so every
// element of every slide drawn anywhere, most of which are not pictures; the stills of
// the media layer (MediaContent.qml) and of the operator's preview of a screen
// (ScreenPicture.qml); and a thumbnail of media set to anything, in the slide grid, the
// media bin and the inspector, given the media's fitting, blur and effects over the
// crop, turn and flip it is made with (src/thumbnailprovider.h). A MediaLook is a few items and a score of bindings: made for each
// of those, it cost the benchmark's slides of shapes a tenth more to open, and every
// slide change a millisecond or more. This makes it only where it is needed.
Item {
    id: still

    property url source
    // The settings, as mediasettings::describe has them; null for none
    property var settings: null
    // How it is fitted, as the settings say unless it is set otherwise (MediaLook.fitting)
    property int fitting: settings?.scale ?? 0
    // Whether the picture is cropped, turned and flipped already (a thumbnail: see
    // MediaLook.shaped), and whether its effects that move by themselves do (not in a
    // thumbnail)
    property bool shaped: false
    property bool animated: true
    // The size it is read at, at most (an Image's sourceSize); undefined to read it whole
    property var readSize: undefined
    // Whether it is fitted by the rule of the media layer, in the middle on a whole pixel
    // and worked out here rather than by a fill mode, which would also have a small
    // picture decoded *up* to the size it is read at (see MediaContent)
    property bool fitsHere: false
    // The picture's Image.status; Loading until there is an Image to ask
    readonly property int status: loader.item ? loader.item.status : String(source) === "" ? Image.Null : Image.Loading
    // Whether it is set to more than how it is fitted, and so drawn through MediaLook
    readonly property bool looked: (settings?.drawn ?? false) || (fitsHere && fitting !== 0)

    Loader {
        id: loader

        anchors.fill: parent
        active: String(still.source) !== ""
        sourceComponent: still.looked ? lookedPicture : still.fitsHere ? fittedPicture : plainPicture
    }

    Component {
        id: plainPicture

        Image {
            source: still.source
            asynchronous: true
            sourceSize: still.readSize !== undefined ? still.readSize : undefined
            fillMode: still.fitting === 2 ? Image.Stretch : still.fitting === 1 ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        }
    }

    Component {
        id: fittedPicture

        Item {
            readonly property alias status: fitted.status

            Image {
                id: fitted

                // What the picture is scaled by to fit, keeping its shape
                readonly property real fit: implicitWidth > 0 && implicitHeight > 0
                                            ? Math.min(parent.width / implicitWidth, parent.height / implicitHeight) : 0

                // In the middle, on a whole pixel.
                x: Math.floor((parent.width - width) / 2)
                y: Math.floor((parent.height - height) / 2)
                width: implicitWidth * fit
                height: implicitHeight * fit
                source: still.source
                asynchronous: true
                sourceSize: still.readSize !== undefined ? still.readSize : undefined
            }
        }
    }

    Component {
        id: lookedPicture

        MediaLook {
            id: look

            readonly property alias status: lookedImage.status

            settings: still.settings
            fitting: still.fitting
            shaped: still.shaped
            animated: still.animated
            wholePixels: still.fitsHere
            fitsHere: still.fitsHere
            sourceWidth: lookedImage.implicitWidth
            sourceHeight: lookedImage.implicitHeight

            Image {
                id: lookedImage

                anchors.fill: parent
                fillMode: look.imageFill
                source: still.source
                asynchronous: true
                sourceSize: still.readSize !== undefined ? still.readSize : undefined
            }
        }
    }
}
