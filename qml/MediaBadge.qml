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
    // How large it is drawn, as a part of its full size
    property real size: 1
    readonly property color ink: missing ? "#ffb300" : "#f2f2f2"
    readonly property color faint: missing ? "#99ffb300" : "#99f2f2f2"

    width: 26 * size
    height: 22 * size
    radius: 4 * size
    color: "#c8000000"

    // The layer behind
    Rectangle {
        x: 4 * badge.size
        y: 4 * badge.size
        width: 13 * badge.size
        height: 9 * badge.size
        radius: 1.5 * badge.size
        color: badge.foreground ? "transparent" : badge.ink
        border.width: badge.foreground ? 1 : 0
        border.color: badge.faint
    }

    // The layer in front, which hides what it covers of the one behind
    Rectangle {
        x: 9 * badge.size
        y: 9 * badge.size
        width: 13 * badge.size
        height: 9 * badge.size
        radius: 1.5 * badge.size
        color: badge.foreground ? badge.ink : "#101010"
        border.width: badge.foreground ? 0 : 1
        border.color: badge.faint
    }
}
