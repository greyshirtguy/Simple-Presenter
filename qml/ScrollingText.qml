import QtQuick
import QtQuick.Effects
import SimplePresenterApp

// A text box's Scrolling Text, as ProPresenter's (see src/textscroller.h for what the
// file says and the guesses): its words going left, right, up or down across the box,
// once or over and over, from where they are or from off the box, feathered at the ends
// of the box along the way they go. Made by SlideElement for a box that scrolls, on the
// screens and in the operator's preview; a thumbnail and the editor show the words still.
//
// How it is drawn, and what that costs. The words are laid out and drawn once, into a
// texture (StrokedText, as every slide's words are, and a ShaderEffectSource of it), as
// long as they are for words that go across and as wide as the box for words that go up
// or down. The box is then one shader over it (shaders/scroll.frag), which for each of
// its pixels finds which going of the words is there and where in them, and feathers the
// ends; a frame changes one number, how far the words have gone, and draws the box once,
// however many goings are in it. (A box of ProPresenter's own has thirteen of these on
// one slide, behind its lyrics.) While the words go, every frame of the screen it is on is
// drawn, which is what moving words are; words that do not repeat stop it once they have
// gone.
//
// The texture is at most 8192 pixels along the way the words go, which every graphics
// chip this is meant for can hold; words longer than that on the screen are drawn into it
// smaller and stretched, a little soft. (A GUESS that it will seldom be needed: a line of
// a verse at a usual size on a 1920 screen is some thousands of pixels.)
Item {
    id: scroller

    // The element's map: its `scroll`, its box and its text's settings (see proconvert)
    required property var source
    // What is drawn: its words, or what a link puts in their place
    property var content
    property var replacement: undefined
    property var replacementColor: undefined
    property real unit: 1
    // How far past the box the words' strokes and shadows are drawn (SlideElement's
    // textBleed); the box still cuts them off at its edges
    property real bleed: 0
    property bool effects: true

    // (Still, for the moment between a box's ceasing to scroll and this going)
    readonly property var settings: source.scroll ?? ({ direction: "left", rate: 0, repeat: false, distance: 0, offScreen: false, fadeLeft: 0, fadeRight: 0 })
    readonly property bool across: settings.direction === "left" || settings.direction === "right"
    // Right and down are left and up the other way round
    readonly property bool backwards: settings.direction === "right" || settings.direction === "down"
    // The box along the way the words go, in slide units
    readonly property real room: across ? source.width : source.height
    readonly property real insets: across ? source.marginLeft + source.marginRight : source.marginTop + source.marginBottom
    // The words as they are laid out: with room to spare, going across; in the box's width
    // (less its insets), going up or down. (A unit or two over, so that what is laid out
    // in that room again does not break a line.)
    readonly property size natural: TextScrolling.naturalSize(content, replacement,
                                                              across ? 100000 : source.width - source.marginLeft - source.marginRight)
    readonly property real length: (across ? natural.width : natural.height) + insets + 2
    readonly property real cycle: Math.max(1, length + settings.distance * room)
    readonly property real begin: TextScrolling.start(settings.offScreen, room)
    // How far they have gone, in slide units
    property real travelled: 0
    readonly property bool going: settings.repeat || travelled < begin + length + bleed
    // Where each going of the words is (as the shader works it out; for the tests)
    function places() {
        return TextScrolling.positions(travelled, begin, length, cycle - length, room, settings.repeat)
    }

    // The words, drawn once; shown only through the shader below
    StrokedText {
        id: words

        width: ((scroller.across ? scroller.length : scroller.source.width) + 2 * scroller.bleed) * scroller.unit
        height: ((scroller.across ? scroller.source.height : scroller.length) + 2 * scroller.bleed) * scroller.unit
        content: scroller.content
        replacement: scroller.replacement
        replacementColor: scroller.replacementColor
        lineFill: scroller.source.fillEnabled && scroller.source.fillLinesOnly ? scroller.source.fillColor : "transparent"
        lineFillStyle: scroller.source.lineMaskStyle
        lineFillWidthOffset: scroller.source.lineMaskWidthOffset
        lineFillHeightOffset: scroller.source.lineMaskHeightOffset
        lineFillHorizontalOffset: scroller.source.lineMaskHorizontalOffset
        lineFillVerticalOffset: scroller.source.lineMaskVerticalOffset
        unit: scroller.unit
        bleed: scroller.bleed
        verticalAlignment: scroller.across ? scroller.source.verticalAlignment : Qt.AlignTop
        fit: 0
        insetLeft: scroller.source.marginLeft
        insetTop: scroller.source.marginTop
        insetRight: scroller.source.marginRight
        insetBottom: scroller.source.marginBottom

        // Its shadow, as SlideElement gives a box's words theirs
        layer.enabled: scroller.effects && scroller.source.textShadowEnabled
        layer.effect: MultiEffect {
            autoPaddingEnabled: true
            shadowEnabled: true
            shadowColor: scroller.source.textShadowColor
            shadowHorizontalOffset: scroller.source.textShadowOffsetX * scroller.unit
            shadowVerticalOffset: scroller.source.textShadowOffsetY * scroller.unit
            blurMax: 64
            shadowBlur: Math.min(1, scroller.source.textShadowRadius * scroller.unit / 32)
        }
    }

    ShaderEffectSource {
        id: texture

        sourceItem: words
        hideSource: true
        visible: false
        // (See the top: at most 8192 along the way the words go)
        textureSize: scroller.across ? Qt.size(Math.min(8192, Math.ceil(words.width)), Math.ceil(words.height))
                                     : Qt.size(Math.ceil(words.width), Math.min(8192, Math.ceil(words.height)))
    }

    ShaderEffect {
        objectName: "scrollingWords"
        anchors.fill: parent
        fragmentShader: "qrc:/shaders/scroll.frag.qsb"

        property var source: texture
        property real across: scroller.across ? 1 : 0
        property real backwards: scroller.backwards ? 1 : 0
        property real repeating: scroller.settings.repeat ? 1 : 0
        property real room: scroller.room * scroller.unit
        property real words: scroller.length * scroller.unit
        property real cycle: scroller.cycle * scroller.unit
        property real begin: scroller.begin * scroller.unit
        // (Kept within one going once the words have come in, as far as the shader can
        // tell, so that its numbers stay exact however long they go)
        property real travelled: (scroller.settings.repeat && scroller.travelled > scroller.begin
                                  ? scroller.begin + (scroller.travelled - scroller.begin) % scroller.cycle
                                  : scroller.travelled) * scroller.unit
        property real bleed: scroller.bleed * scroller.unit
        property real span: (scroller.across ? scroller.source.height : scroller.source.width) * scroller.unit
        property real spanTexture: span + 2 * bleed
        property real fadeStart: scroller.settings.fadeLeft
        property real fadeEnd: scroller.settings.fadeRight
    }

    FrameAnimation {
        running: scroller.visible && scroller.going
        onTriggered: scroller.travelled += TextScrolling.speed(scroller.settings.rate) * frameTime
    }
}
