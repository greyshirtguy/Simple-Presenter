import QtQuick
import QtQuick.Effects

// Marks a thumbnail as live: an orange ring outside its frame with a soft glow, set off
// from the frame by a gap of the background colour so it reads even against an orange
// frame. Fill the frame with this, behind it. The glow is a single cheap shader, not a
// blur pass, and a view should only make one of these for the thumbnail that is live
// (a Loader does it), not one for every thumbnail.
Item {
    id: liveRing

    // The colour behind the thumbnail, for the gap
    property color background: "#1e1f22"

    anchors.margins: -5

    RectangularShadow {
        anchors.fill: parent
        radius: ring.radius
        blur: 12
        spread: 1
        color: "#ff8a1f"
    }

    Rectangle {
        id: ring

        anchors.fill: parent
        radius: 8
        color: liveRing.background
        border.width: 3
        border.color: "#ff8a1f"
    }
}
