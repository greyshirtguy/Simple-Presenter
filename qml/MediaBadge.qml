import QtQuick

// Marks a thumbnail as media that is triggered, and says how it behaves: two layers, one
// behind the other, with the one that the media plays on drawn solid and the other in
// outline. A background (which stays from slide to slide, and loops) has the layer behind
// solid; a foreground (which plays once and gives way to the next slide) has the one in
// front. See workspace::MediaBehaviour in src/workspacefiles.h. Amber when the media file
// cannot be found.
Rectangle {
    id: badge

    property bool foreground: false
    property bool missing: false
    readonly property color ink: missing ? "#ffb300" : "#f2f2f2"
    readonly property color faint: missing ? "#99ffb300" : "#99f2f2f2"

    width: 26
    height: 22
    radius: 4
    color: "#c8000000"

    // The layer behind
    Rectangle {
        x: 4
        y: 4
        width: 13
        height: 9
        radius: 1.5
        color: badge.foreground ? "transparent" : badge.ink
        border.width: badge.foreground ? 1 : 0
        border.color: badge.faint
    }

    // The layer in front, which hides what it covers of the one behind
    Rectangle {
        x: 9
        y: 9
        width: 13
        height: 9
        radius: 1.5
        color: badge.foreground ? badge.ink : "#101010"
        border.width: badge.foreground ? 0 : 1
        border.color: badge.faint
    }
}
