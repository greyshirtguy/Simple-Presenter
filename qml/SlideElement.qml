import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window
import SimplePresenterApp

// One element of a slide: its shape (fill and stroke), with its text over it, each with
// its own shadow. Geometry comes in slide units and is multiplied by `unit`, so text is
// rasterised at the size it is shown at rather than scaled up from a fixed-size texture.
//
// The shape. Nearly every element there is is a rectangle that is filled with a plain
// colour or not at all, and that is drawn as the one rectangle it is. Anything more is
// drawn by the parts further down, which are only made for an element that needs them:
// an outline that is not a rectangle (a rounded rectangle, an ellipse, an arrow, or
// any other shape of ProPresenter's, each drawn from the points of its outline), a
// fill that is a gradient or a picture, edges that fade out. A picture is cut to a
// shape that is not a rectangle by masking it, and edges are faded by a shader
// (shaders/feather.frag), each of which takes a texture of its own; so they too are
// only set up where they are called for.
//
// It draws what the element map says and works nothing out: which elements show, and
// what text each shows, was settled when the map was made (src/proconvert.h). The same
// component is the element on the output, in a thumbnail and under the editor's handles.
//
// The one thing not settled then is text that changes while the slide is on show, which
// an element linked to a timer has, or to the clock, or to how long a video has left,
// or to what the stage is being told, or to the words or the notes of the slide that is
// live, and that the text box of a message has: that is asked for here, as
// `liveLinkText`, and drawn in the element's own style. It is drawn again only when the
// words change, which for a timer is once a second while it runs, unless the element
// shows the timer's hundredths: then it is thirty times a second where they can be
// read, and five in a small picture. (A slide's notes can also be shown as they were
// written, in their own fonts and colours: `liveLinkText` is then that text itself.)
//
// A fill can also be a picture of what one of the screens is showing, which is
// ProPresenter's Screen Preview: see ScreenPicture, which draws it, and `pictures`. And
// it can be one of the workspace's video inputs, live (see VideoInputView), on a screen,
// in the operator's preview and in the editor, and in a thumbnail the input's own
// thumbnail, for the same reason as a Screen Preview is nothing there.
//
// A shadow is the one dear thing here. The shape or the text is drawn into a texture of
// its own, which a blur then turns into the shadow under it: an extra pass, and an extra
// texture, for each shadow. So nothing is set up for a shadow unless the element has
// one, and `effects` turns them off altogether where they would not be seen.
Item {
    id: element

    // An element map, as proconvert describes it
    required property var source
    // Output pixels per slide unit
    property real unit: 1
    // The element's box, in slide units. It follows the source unless something, such as
    // a drag in the editor, holds it elsewhere for a while.
    property real boxX: source.x
    property real boxY: source.y
    property real boxWidth: source.width
    property real boxHeight: source.height
    // Text to draw in place of the element's own: a RichText value, or undefined. The
    // editor draws what is being typed this way.
    property var textOverride: undefined
    // Whether text that is set to suit its size to its box does. Not while it is being
    // typed, when each letter has to stand where the caret believes it is.
    property bool fitted: true
    // Whether an element linked to a picture this app does not show (of a slide, of an
    // output) is drawn with the fill it has in the file, which is only there to stand
    // in for the picture. In the editor it is, so that the element can be seen and
    // placed; shown for real, a block of colour where a picture was meant would be
    // worse than nothing.
    property bool standIns: false
    // The operator window, where the element is on show (on a screen, or in the
    // operator's preview of one): what a fill that is a Screen Preview asks what the
    // screen is showing. Null where it is only being looked at (a thumbnail, the
    // editor), where such a fill draws nothing, or its stand-in if standIns says so:
    // a grid of thumbnails, each drawing a screen and borrowing a video's frames, would
    // cost more than the show itself.
    property var pictures: null
    // The words of an element whose words change while it is shown, or undefined for an
    // element that shows what its map says: a timer's time (see Timers); the words or
    // the notes of the slide that is live or of the one after it (see Show), which are
    // what a stage layout is made of; the time of day, or how long a video has left
    // (see Clocks). Anything else of the kind belongs here as another case.
    readonly property var liveLinkText: {
        switch (source.linkKind) {
        case "timer":
            // Reading the tick is what has this follow the timers, and reading a beat
            // what has it follow one whose hundredths it shows.
            return Timers.tick >= 0 && (source.linkTimerHundredths === 0 || (effects ? Timers.beat : Timers.slowBeat) >= 0)
                ? Timers.linkedText(source.linkTimerId, source.linkTimerName, source.linkTimerHours,
                                    source.linkTimerMinutes, source.linkTimerSeconds, source.linkTimerHundredths,
                                    source.linkTimerHundredthsUnderMinute, source.linkTimerPattern)
                : ""
        case "slideText":
            // Reading the revision is what has this follow what is live.
            //
            // A slide's notes that are to keep their formatting are answered as the
            // text they are, fonts, sizes, colours and all (a RichText), and not as
            // words: what draws the text (StrokedText) then draws that, with
            // nothing of the element's own style put on it. A slide with no notes
            // has no words. (It is answered here, and not in a property of its own,
            // because a property is something every element of every slide would
            // have to have and work out, for the sake of very few.)
            //
            // A GUESS at ProPresenter: that keeping the formatting keeps the sizes
            // too, so that notes written at 24 points are 24 points on a layout as
            // big as the screen, which is small; and that where the lines stand in
            // the box is still the box's own.
            return Show.revision < 0 ? ""
                : source.linkSlidePreserve === true && source.linkSlideSource === Show.Notes ? Show.slideNotes(source.linkSlideNext) ?? ""
                : Show.slideText(source.linkSlideNext, source.linkSlideSource, source.linkSlideName, source.linkTransform)
        case "clock":
            // Reading the second is what has this asked again as the clock goes on,
            // and asking is what keeps the clock going (see Clocks): nothing else is
            // said or made for it. (A clock that shows no seconds answers the same
            // for sixty of them, and nothing is drawn for that.)
            return Clocks.second >= 0
                ? Clocks.clockText(source.linkClockDate, source.linkClockTime, source.linkClock24, source.linkClockPattern)
                : ""
        case "videoCountdown":
            return Clocks.videoRevision >= 0
                ? Clocks.videoText(source.linkTimerHours, source.linkTimerMinutes, source.linkTimerSeconds, source.linkTimerHundredths,
                                   source.linkTimerHundredthsUnderMinute, source.linkVideoIgnoreLooping, source.linkTimerPattern)
                : ""
        case "copyright":
            // The copyright of the presentation that is live, as the workspace's display
            // has it (see Show.copyrightText): nothing while it is not shown.
            return Show.copyrightText
        case "marker":
            // A playback marker's name, or the time to it, of one layer's media (see
            // Clocks.markerText): asked again as the revision goes up, which it does by
            // the second while the media plays and a box asks.
            return Clocks.markerRevision >= 0
                ? Clocks.markerText(source.linkMarkerDestination, source.linkMarkerWhich, source.linkMarkerName, source.linkMarkerTime,
                                    source.linkTimerHours, source.linkTimerMinutes, source.linkTimerSeconds, source.linkTimerHundredths,
                                    source.linkTimerHundredthsUnderMinute)
                : ""
        case "stageMessage":
            // What the stage is being told, which is nothing until it is told
            // something (see Show).
            return Show.stageMessage
        case "message":
            // The words of a message of the messages' layer, as they are at the
            // moment: they are worked out elsewhere, once for every screen, and
            // reading the revision is what has this read them again when they have
            // changed (see Messages).
            return Messages.revision >= 0 ? Messages.words(source.linkMessageId) : ""
        default:
            return undefined
        }
    }
    // The colour a timer's (or a video's) time is to be in now, by the link's Color
    // Triggers (src/colortriggers.h), or undefined for the box's own: read with the
    // same tick and beat as the words, so that it changes with them. Only a box linked
    // to a timer has `linkColorTriggers` at all (proconvert.cpp), so for any other this
    // is undefined at once.
    readonly property var liveLinkColor: {
        // (A playback marker's text drawn in the marker's colour, where its link says)
        if (source.linkKind === "marker")
            return source.linkMarkerColor === true && Clocks.markerRevision >= 0
                ? Clocks.markerColor(source.linkMarkerDestination, source.linkMarkerWhich, source.linkMarkerName) : undefined
        const triggers = source.linkColorTriggers
        if (!triggers || triggers.length === 0)
            return undefined
        if (source.linkKind === "timer")
            return Timers.tick >= 0 && (source.linkTimerHundredths === 0 || (effects ? Timers.beat : Timers.slowBeat) >= 0)
                ? Timers.linkedColor(source.linkTimerId, source.linkTimerName, triggers) : undefined
        if (source.linkKind === "videoCountdown")
            return Clocks.videoRevision >= 0 ? Clocks.videoColor(triggers, source.linkVideoIgnoreLooping) : undefined
        return undefined
    }
    // Whether this is a text box that draws the chords of the slide whose words it
    // shows: then the words are drawn line by line with a row of chords over each
    // (ChordedText), in place of the usual drawing. Only a stage layout has such a
    // box, and nothing is set up for it anywhere else.
    readonly property bool chorded: source.chordsOn === true && source.linkKind === "slideText" && textOverride === undefined
    // Whether to draw shadows. Each one is an extra texture and a blur, which is nothing
    // for the one slide on the output and adds up for a grid of thumbnails, where a
    // shadow is a pixel wide and cannot be seen anyway. It is also taken to say whether
    // this is a picture large enough to read hundredths of a second in.
    property bool effects: true
    // Room around the box for what the text draws outside it (strokes, the bars of a
    // fill that is only behind the text's lines), in slide units
    readonly property real textBleed: 60
    // A fill that is only behind the lines of the text is drawn with the text.
    readonly property bool boxFilled: source.fillShown && !source.fillLinesOnly && (standIns || !source.linkPicture)
    // Whether the shape is more than a rectangle with a plain fill, or none
    readonly property bool plain: source.shape === "rectangle" && (!boxFilled || source.fillKind === "color") && !feathered
    readonly property bool feathered: effects && source.featherOn && boxFilled && source.shape !== "arrow" && source.shape !== "other"
    // The shape's outline as an SVG path, in pixels of this item
    readonly property string outline: plain ? "" : outlinePath(width, height)

    function outlinePath(w, h) {
        const round = (n) => Math.round(n * 100) / 100
        if (source.shape === "roundedRectangle") {
            // Round in the element's own proportions, whatever the points in the file say
            const r = round(Math.max(0, Math.min(0.5, source.roundness)) * Math.min(w, h))
            if (r <= 0)
                return "M 0 0 H " + w + " V " + h + " H 0 Z"
            const arc = " A " + r + " " + r + " 0 0 1 "
            return "M " + r + " 0 H " + round(w - r) + arc + w + " " + r + " V " + round(h - r) + arc + round(w - r) + " " + h
                 + " H " + r + arc + "0 " + round(h - r) + " V " + r + arc + r + " 0 Z"
        }
        const points = source.outline
        if (!points || points.length < 2)
            return "M 0 0 H " + w + " V " + h + " H 0 Z"
        // From each point to the next by a curve, which is a straight line where the
        // bending points are the points themselves
        let path = "M " + round(points[0][0] * w) + " " + round(points[0][1] * h)
        for (let i = 1; i <= points.length; ++i) {
            const from = points[i - 1]
            const to = points[i % points.length]
            path += " C " + round(from[4] * w) + " " + round(from[5] * h) + " " + round(to[2] * w) + " " + round(to[3] * h)
                  + " " + round(to[0] * w) + " " + round(to[1] * h)
        }
        return path + " Z"
    }

    component Shadow: MultiEffect {
        property string which

        autoPaddingEnabled: true
        shadowEnabled: true
        shadowColor: element.source[which + "Color"]
        shadowHorizontalOffset: element.source[which + "OffsetX"] * element.unit
        shadowVerticalOffset: element.source[which + "OffsetY"] * element.unit
        blurMax: 64
        shadowBlur: Math.min(1, element.source[which + "Radius"] * element.unit / 32)
    }

    x: boxX * unit
    y: boxY * unit
    width: boxWidth * unit
    height: boxHeight * unit
    rotation: source.rotation
    opacity: source.opacity

    // (A box linked to the stage message and set to flash has its flash colour for a
    // fill, on and off, for two seconds after the stage is told something: see Show's
    // stageFlash. It is asked here and where the plain box takes its colour, and of no
    // element but one that has `linkFlash`, which only such a box has; so no other
    // element is put to anything by it. Only a plain box flashes: one whose fill is a
    // gradient or a picture, or whose shape is more than a rectangle, does not.)
    Loader {
        anchors.fill: parent
        visible: element.boxFilled || element.source.strokeEnabled || (element.source.linkFlash === true && Show.stageFlash)
        sourceComponent: element.plain ? plainBox : richShape

        layer.enabled: element.effects && element.source.shadowEnabled
        layer.effect: Shadow {
            which: "shadow"
        }
    }

    Component {
        id: plainBox

        Rectangle {
            color: element.source.linkFlash === true && Show.stageFlash ? element.source.linkFlashColor
                 : element.boxFilled ? element.source.fillColor : "transparent"
            border.color: element.source.strokeColor
            border.width: element.source.strokeEnabled ? element.source.strokeWidth * element.unit : 0
        }
    }

    Component {
        id: richShape

        Item {
            id: rich

            // (A Screen Preview that is not drawn here is drawn as its stand-in, the
            // fill the file has for it, where stand-ins are, and not at all elsewhere.)
            readonly property string kind: !element.boxFilled ? "none"
                                         : element.source.fillKind === "input" ? "input"
                                         : element.source.fillKind !== "screen" ? element.source.fillKind
                                         : element.pictures !== null ? "screen" : element.standIns ? "color" : "none"
            readonly property bool cut: element.source.shape !== "rectangle"
            // Which way a gradient runs: its angle is anticlockwise from pointing right
            readonly property real along: element.source.fillGradientAngle * Math.PI / 180
            readonly property real reach: Math.abs(width / 2 * Math.cos(along)) + Math.abs(height / 2 * Math.sin(along))
            // The picture of a Screen Preview, while the fill is one: see placeScreen(); and
            // the live picture of a video input, while it is one: see placeInput()
            property var screenPicture: null
            property var inputPicture: null

            // A Screen Preview: what one of the screens is showing, fitted in, and cut to
            // the shape by the same mask as a picture is.
            //
            // Chosen for speed, and of this app's own: its slides have shadows only where
            // the element is a quarter of an output's height or more, where they can be
            // seen; and a video under them is in it frame for frame (borrowed, not played
            // again: see ScreenPicture), each frame being a redraw of the window it is in.
            // The benchmark measures what that costs, with a picture of the output as big
            // as most of the stage screen.
            //
            // Made here, from its file by its address, only while the fill is one. Not by
            // its name, as everything else here is made: a picture is made of slides and a
            // slide of these elements, and QML will not have a component that is made,
            // however far down, of itself (it refuses it without a word, and the app has
            // no window); made this way, the one is not part of the other until a picture
            // is wanted, and then not for ever, the picture's own slides having none (see
            // ScreenPicture). And not by a Loader standing by in every shape: every shape
            // of every slide drawn anywhere, each thumbnail's among them, would carry one
            // for the few that ever have a Screen Preview.
            function placeScreen() {
                const wanted = kind === "screen"
                if (wanted === (screenPicture !== null))
                    return
                if (!wanted) {
                    screenPicture.destroy()
                    screenPicture = null
                    return
                }
                const made = Qt.createComponent(Qt.resolvedUrl("ScreenPicture.qml")).createObject(fill, {
                    win: element.pictures,
                    screenId: element.source.fillScreenId ?? "",
                    screenName: element.source.fillScreenName ?? "",
                    effects: element.effects && element.boxHeight >= 270,
                    videoInterval: 0,
                })
                made.anchors.fill = fill
                made.screenId = Qt.binding(() => element.source.fillScreenId ?? "")
                made.screenName = Qt.binding(() => element.source.fillScreenName ?? "")
                made.effects = Qt.binding(() => element.effects && element.boxHeight >= 270)
                made.layer.effect = picture.layer.effect
                made.layer.enabled = Qt.binding(() => rich.cut)
                screenPicture = made
            }

            // A video input: one of the workspace's inputs, live, fitted as the fill's media
            // says (Scale to Fit, Scale to Fill, Stretch), cut to the shape as a picture is.
            // Made as a Screen Preview is, here only while the fill is one, so that no
            // shape that is not carries anything for it.
            //
            // Chosen for speed: in the operator window (its preview, the editor) a frame
            // is taken a tenth of a second apart, as the preview takes a video's, each
            // frame being a redraw of that whole window; on a screen, every frame; and in
            // a thumbnail (where `pictures` is null and there are no stand-ins) none at
            // all, but the input's thumbnail, still. Its sound is the video input layer's
            // to play, and not a shape's (A GUESS at ProPresenter, whose help says an
            // input's sound plays while it is on an audience screen).
            function placeInput() {
                const wanted = kind === "input"
                if (wanted === (inputPicture !== null))
                    return
                if (!wanted) {
                    inputPicture.destroy()
                    inputPicture = null
                    return
                }
                const made = Qt.createComponent(Qt.resolvedUrl("VideoInputView.qml")).createObject(fill, {
                    index: element.source.fillInputIndex ?? 0,
                    still: element.pictures === null && !element.standIns,
                })
                made.anchors.fill = fill
                made.index = Qt.binding(() => element.source.fillInputIndex ?? 0)
                made.fitting = Qt.binding(() => element.source.fillMediaScale ?? 0)
                made.still = Qt.binding(() => element.pictures === null && !element.standIns)
                made.interval = Qt.binding(() => element.pictures !== null && element.Window.window === element.pictures || element.standIns ? 100 : 0)
                made.layer.effect = picture.layer.effect
                made.layer.enabled = Qt.binding(() => rich.cut)
                inputPicture = made
            }

            onKindChanged: {
                placeScreen()
                placeInput()
            }
            Component.onCompleted: {
                placeScreen()
                placeInput()
            }

            // The fill, which is the part whose edges fade if they do
            Item {
                id: fill

                anchors.fill: parent

                layer.enabled: element.feathered
                layer.effect: ShaderEffect {
                    property real kind: element.source.shape === "ellipse" ? 2 : element.source.shape === "roundedRectangle" ? 1 : 0
                    property real corner: Math.max(0, Math.min(0.5, element.source.roundness)) * Math.min(rich.width, rich.height)
                    property real feather: element.source.featherRadius * Math.min(rich.width, rich.height)
                    property vector2d extent: Qt.vector2d(rich.width, rich.height)

                    fragmentShader: "qrc:/shaders/feather.frag.qsb"
                }

                // A colour or a gradient, in the shape
                Shape {
                    anchors.fill: parent
                    visible: rich.kind === "color" || rich.kind === "gradient"
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: "transparent"
                        strokeWidth: 0
                        fillColor: rich.kind === "color" ? element.source.fillColor : "transparent"
                        fillGradient: rich.kind === "gradient" ? ramp : null

                        PathSvg {
                            path: element.outline
                        }
                    }
                }

                LinearGradient {
                    id: ramp

                    x1: rich.width / 2 - Math.cos(rich.along) * rich.reach
                    y1: rich.height / 2 + Math.sin(rich.along) * rich.reach
                    x2: rich.width / 2 + Math.cos(rich.along) * rich.reach
                    y2: rich.height / 2 - Math.sin(rich.along) * rich.reach

                    GradientStop {
                        position: 0
                        color: element.source.fillGradientFrom
                    }
                    GradientStop {
                        position: 1
                        color: element.source.fillGradientTo
                    }
                }

                // A picture: made to fit inside the element, to fill it and be cut off
                // at its edges, or stretched to it, and cropped, turned, flipped and
                // given effects as its Media Inspector sets it (MediaLook). In a shape
                // that is not a rectangle it is cut to the shape by a mask. (A Screen
                // Preview is cut by the same, and is put in after this, by placeScreen().)
                // (Made only for an element that is filled with media: every element has
                // this, and most are not pictures. See MediaPicture.qml.)
                Loader {
                    id: picture

                    anchors.fill: parent
                    active: rich.kind === "media"

                    layer.enabled: rich.cut && rich.kind === "media"
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: mask.item
                    }

                    sourceComponent: MediaPicture {
                        source: element.source.fillMediaSource
                        settings: element.source.fillMediaSettings ?? null
                        fitting: element.source.fillMediaScale
                        clip: true
                        // No larger in memory than it is shown
                        readSize: Qt.size(Math.max(1, Math.ceil(width * Screen.devicePixelRatio)),
                                          Math.max(1, Math.ceil(height * Screen.devicePixelRatio)))
                    }
                }

                Loader {
                    id: mask

                    anchors.fill: parent
                    active: rich.cut && (rich.kind === "media" || rich.kind === "screen" || rich.kind === "input")

                    sourceComponent: Shape {
                        visible: false
                        preferredRendererType: Shape.CurveRenderer
                        layer.enabled: true

                        ShapePath {
                            strokeColor: "transparent"
                            strokeWidth: 0
                            fillColor: "white"

                            PathSvg {
                                path: element.outline
                            }
                        }
                    }
                }
            }

            // The stroke, along the outline
            Shape {
                anchors.fill: parent
                visible: element.source.strokeEnabled
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: element.source.strokeColor
                    strokeWidth: element.source.strokeWidth * element.unit
                    fillColor: "transparent"
                    joinStyle: ShapePath.MiterJoin

                    PathSvg {
                        path: element.outline
                    }
                }
            }
        }
    }

    // (A box whose words scroll has them drawn by ScrollingText, below, where it is on
    // show; elsewhere, still, here.)
    StrokedText {
        anchors.fill: parent
        anchors.margins: -element.textBleed * element.unit
        visible: !element.chorded && (element.source.hasText || element.textOverride !== undefined)
                 && (element.source.scroll === undefined || element.pictures === null)
        content: element.textOverride !== undefined ? element.textOverride : element.source.displayText
        replacement: element.textOverride !== undefined ? undefined : element.liveLinkText
        replacementColor: element.textOverride !== undefined ? undefined : element.liveLinkColor
        lineFill: element.source.fillEnabled && element.source.fillLinesOnly ? element.source.fillColor : "transparent"
        lineFillStyle: element.source.lineMaskStyle
        lineFillWidthOffset: element.source.lineMaskWidthOffset
        lineFillHeightOffset: element.source.lineMaskHeightOffset
        lineFillHorizontalOffset: element.source.lineMaskHorizontalOffset
        lineFillVerticalOffset: element.source.lineMaskVerticalOffset
        unit: element.unit
        bleed: element.textBleed
        verticalAlignment: element.source.verticalAlignment
        fit: element.fitted ? element.source.textScale : 0
        insetLeft: element.source.marginLeft
        insetTop: element.source.marginTop
        insetRight: element.source.marginRight
        insetBottom: element.source.marginBottom

        layer.enabled: element.effects && element.source.textShadowEnabled
        layer.effect: Shadow {
            which: "textShadow"
        }
    }

    // A box's words drawn otherwise than by the StrokedText above: with their chords over
    // them (ChordedText), or going across or up or down the box (Scrolling Text: see
    // ScrollingText), which it does on a screen or in the operator's preview of one and
    // nowhere else. (One Loader for the two, made for every element, and nothing made in
    // it for one that is neither: an element costs every slide it is on, everywhere.)
    Loader {
        anchors.fill: parent
        active: element.chorded || (element.source.scroll !== undefined && element.pictures !== null
                                    && (element.source.hasText || element.textOverride !== undefined))
        // (Each only for its own: the two are not both told at once that a box has changed)
        sourceComponent: element.chorded ? chordedWords : element.source.scroll !== undefined ? scrollingWords : null
    }

    Component {
        id: scrollingWords

        ScrollingText {
            source: element.source
            content: element.textOverride !== undefined ? element.textOverride : element.source.displayText
            replacement: element.textOverride !== undefined ? undefined : element.liveLinkText
            replacementColor: element.textOverride !== undefined ? undefined : element.liveLinkColor
            unit: element.unit
            bleed: element.textBleed
            effects: element.effects
        }
    }

    Component {
        id: chordedWords

        ChordedText {
            // Reading the revision is what has this follow what is live, and the key.
            lines: Show.revision >= 0
                   ? Show.chordLines(element.source.linkSlideNext, element.source.linkSlideSource, element.source.linkSlideName,
                                     element.source.linkTransform, element.source.chordNotation)
                   : []
            style: element.source.chordStyle ?? ({})
            chordColor: element.source.chordColor
            unit: element.unit
            fit: element.fitted ? element.source.textScale : 0
            verticalAlignment: element.source.verticalAlignment
            insetLeft: element.source.marginLeft
            insetTop: element.source.marginTop
            insetRight: element.source.marginRight
            insetBottom: element.source.marginBottom

            layer.enabled: element.effects && element.source.textShadowEnabled
            layer.effect: Shadow {
                which: "textShadow"
            }
        }
    }
}
