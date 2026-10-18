import QtQuick
import QtQuick.Effects
import QtMultimedia
import SimplePresenterApp

// A piece of media drawn as its Media Inspector sets it (src/mediasettings.h): fitted to
// this item's box, cropped, turned and flipped, on a blurred copy of itself for Scale +
// Blur, and with its effects. What shows the media itself (an Image, a VideoOutput set
// to stretch) is put in as its content, and fills the place worked out for it here.
// It is how media is drawn on the media layer (MediaContent.qml), in the operator's
// preview of a screen (ScreenPicture.qml), in an element filled with media
// (SlideElement.qml) and in the inspector itself.
//
// How it is laid out, from the outside in, all of it square to the screen:
//   - `placed`, the media as it is shown: turned, cropped and fitted, in the middle of
//     the box. Fit makes it as large as fits whole, Fill as large as covers the box (and
//     the box cuts off the rest), Stretch the box's size whatever the media's shape.
//     Cropping is `placed` cutting off what is outside it.
//   - inside it, the media turned, whole, and flipped: moved so that the part kept by
//     the crop is what `placed` shows. The flips are of the media as it is shown, once
//     turned, so that flipping across is across whichever way it has been turned.
//   - inside that, the media at its own proportions, turned about its middle.
// A crop is said in pixels of the media as it is in its file, before it is turned or
// flipped (the file's insets are of the media at its own size), and is turned and
// flipped with it here. A GUESS that ProPresenter means them so.
//
// Media that is set to nothing more than how it is fitted is `plain`: what shows it
// fills the box and fits the media itself (by the fill mode it is to bind to imageFill
// or videoFill), exactly as it did before there were settings, so that it is drawn
// pixel for pixel as it was. (Unless `fitsHere` is set: a still on the media layer was
// always fitted by its owner, and is here.)
//
// What it costs. Fitting, turning, flipping and cropping are where things are drawn and
// what is cut off, which the graphics chip does as it draws them anyway: media that is
// only that is drawn once, as it was before there were settings. A cut that is square
// to the screen is a scissor, the cheapest kind; that is why the media is turned inside
// the cut and not outside it. Scale + Blur draws the media once more, small, and blurs
// that across the box; each effect that is on is a pass over the media's pixels (see
// MediaEffects.qml). Those are made only for media that has them.
Item {
    id: look

    // The settings, as mediasettings::describe has them; null for media that has none,
    // which is fitted and nothing else
    property var settings: null
    // The media's own size in pixels, as what shows it has it: 0 until it is known
    property real sourceWidth: 0
    property real sourceHeight: 0
    // Whether the crop, the turn and the flips are in the picture already: a thumbnail
    // is made so (src/thumbnailprovider.h), and only its fitting, blur and effects are
    // left to do here
    property bool shaped: false
    // Whether the media goes on whole pixels, as a still always has on the media layer
    property bool wholePixels: false
    // Whether it is fitted here even when it is plain (see above)
    property bool fitsHere: false
    // Whether its effects that move by themselves do (not in a thumbnail: MediaEffects)
    property bool animated: true
    // What shows the media
    default property alias content: whole.data

    readonly property var s: settings ?? ({})
    // How it is fitted, as the settings say unless it is set otherwise (an element's
    // fill has its own way of reading it: see proconvert.h)
    property int fitting: s.scale ?? 0
    readonly property int turn: shaped ? 0 : (s.rotation ?? 0)
    readonly property bool sideways: turn === 90 || turn === 270
    readonly property bool flipAcross: !shaped && (s.flipH ?? false)
    readonly property bool flipDown: !shaped && (s.flipV ?? false)
    readonly property bool cropping: !shaped && (s.cropOn ?? false)
                                     && ((s.cropLeft ?? 0) > 0 || (s.cropRight ?? 0) > 0 || (s.cropTop ?? 0) > 0 || (s.cropBottom ?? 0) > 0)
    // The crop is in pixels of the media at the size the file says it is; what shows it
    // may have it at another (a still is read no larger than it is shown).
    readonly property real cropScaleX: (s.naturalWidth ?? 0) > 0 && sourceWidth > 0 ? sourceWidth / s.naturalWidth : 1
    readonly property real cropScaleY: (s.naturalHeight ?? 0) > 0 && sourceHeight > 0 ? sourceHeight / s.naturalHeight : 1
    // The crop's insets as the media is shown, turned and flipped: { l, t, r, b }
    readonly property var shown: {
        if (!cropping)
            return { l: 0, t: 0, r: 0, b: 0 }
        const left = s.cropLeft * cropScaleX
        const right = s.cropRight * cropScaleX
        const top = s.cropTop * cropScaleY
        const bottom = s.cropBottom * cropScaleY
        // Turned clockwise: by a quarter, what was at the media's left is at the top
        let edges = turn === 90 ? [bottom, left, top, right]
                  : turn === 180 ? [right, bottom, left, top]
                  : turn === 270 ? [top, right, bottom, left] : [left, top, right, bottom]
        if (flipAcross)
            edges = [edges[2], edges[1], edges[0], edges[3]]
        if (flipDown)
            edges = [edges[0], edges[3], edges[2], edges[1]]
        return { l: edges[0], t: edges[1], r: edges[2], b: edges[3] }
    }
    // The media turned, and then the part of it the crop keeps: what is fitted
    readonly property real turnedWidth: sideways ? sourceHeight : sourceWidth
    readonly property real turnedHeight: sideways ? sourceWidth : sourceHeight
    readonly property real keptWidth: Math.max(0, turnedWidth - shown.l - shown.r)
    readonly property real keptHeight: Math.max(0, turnedHeight - shown.t - shown.b)
    // Box pixels for each of the media's, across and down
    readonly property real fitted: keptWidth > 0 && keptHeight > 0
                                   ? (fitting === 1 ? Math.max(width / keptWidth, height / keptHeight)
                                                    : Math.min(width / keptWidth, height / keptHeight)) : 0
    readonly property real kx: fitting === 2 && keptWidth > 0 ? width / keptWidth : fitted
    readonly property real ky: fitting === 2 && keptHeight > 0 ? height / keptHeight : fitted
    // The effects that are on and are drawn here, in order
    readonly property var drawnEffects: (s.effects ?? []).filter(effect => effect.on && effect.shader !== "")
    // Set to nothing but how it is fitted, and fitted by what shows it: see above
    readonly property bool plain: !fitsHere && !cropping && turn === 0 && !flipAcross && !flipDown && !(s.blurred ?? false)
                                  && drawnEffects.length === 0
    // The fill mode for what shows the media: its own fitting when the media is plain,
    // and otherwise stretched to the place worked out here
    readonly property int imageFill: !plain ? Image.Stretch
                                   : fitting === 2 ? Image.Stretch : fitting === 1 ? Image.PreserveAspectCrop : Image.PreserveAspectFit
    readonly property int videoFill: !plain ? VideoOutput.Stretch
                                   : fitting === 2 ? VideoOutput.Stretch : fitting === 1 ? VideoOutput.PreserveAspectCrop
                                                                                         : VideoOutput.PreserveAspectFit

    // Filled, the media is larger than the box, which cuts it off.
    clip: fitting === 1

    // Scale + Blur: behind the media, fitted, a copy of it that covers the box, blurred.
    // The copy is taken at a quarter of the size, which the blur does not miss.
    Loader {
        anchors.fill: parent
        active: (look.s.blurred ?? false) && look.fitting === 0 && placed.width > 0 && placed.height > 0

        sourceComponent: Item {
            clip: true

            ShaderEffectSource {
                id: copy

                sourceItem: placed
                hideSource: false
                visible: false
                textureSize: Qt.size(Math.max(1, Math.round(placed.width / 4)), Math.max(1, Math.round(placed.height / 4)))
            }

            MultiEffect {
                readonly property real cover: Math.max(look.width / placed.width, look.height / placed.height)

                width: placed.width * cover
                height: placed.height * cover
                x: (look.width - width) / 2
                y: (look.height - height) / 2
                source: copy
                autoPaddingEnabled: false
                blurEnabled: true
                blur: 1
                blurMax: 32
            }
        }
    }

    Item {
        id: placed

        readonly property real shownWidth: look.keptWidth * look.kx
        readonly property real shownHeight: look.keptHeight * look.ky

        x: look.plain ? 0 : look.wholePixels ? Math.floor((look.width - shownWidth) / 2) : (look.width - shownWidth) / 2
        y: look.plain ? 0 : look.wholePixels ? Math.floor((look.height - shownHeight) / 2) : (look.height - shownHeight) / 2
        width: look.plain ? look.width : shownWidth
        height: look.plain ? look.height : shownHeight
        clip: look.cropping

        // The media turned, whole, and flipped
        Item {
            id: turned

            x: look.plain ? 0 : -look.shown.l * look.kx
            y: look.plain ? 0 : -look.shown.t * look.ky
            width: look.plain ? placed.width : look.turnedWidth * look.kx
            height: look.plain ? placed.height : look.turnedHeight * look.ky
            transform: Scale {
                origin.x: turned.width / 2
                origin.y: turned.height / 2
                xScale: look.flipAcross ? -1 : 1
                yScale: look.flipDown ? -1 : 1
            }

            // The media at its own proportions, turned about its middle
            Item {
                id: whole

                x: (turned.width - width) / 2
                y: (turned.height - height) / 2
                width: look.sideways ? turned.height : turned.width
                height: look.sideways ? turned.width : turned.height
                rotation: look.turn
            }
        }
    }

    // The effects, drawn over where the media is, from it
    Loader {
        x: placed.x
        y: placed.y
        width: placed.width
        height: placed.height
        active: look.drawnEffects.length > 0

        sourceComponent: MediaEffects {
            source: placed
            effects: look.drawnEffects
            animated: look.animated
        }
    }
}
