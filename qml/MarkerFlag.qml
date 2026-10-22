import QtQuick
import QtQuick.Shapes

// A playback marker's flag (see playbackmarkers.h), as the transport and the Media
// Inspector draw it over their bars: a little shield of the marker's colour, wider
// than it is tall above its shoulders, pointed at the foot, the point being the
// marker's place on the bar. It sits over the bar with its point at the top of the
// slider's handle, so that it covers neither the handle nor the bar, and a click on
// the slider anywhere else is the slider's. Outlined in dark, or in white while it is
// under the pointer or held (`lit`), so that it is seen on any colour and on the bar's
// orange.
//
// Its size is its user's: small on the transport, where it is fine to be hard to hit,
// and larger in the Inspector, where markers are made and moved. (Drawn here: the icon
// pack has no picture of ProPresenter's marker flag.)
Item {
    id: flag

    property color color: "#3d8be0"
    property bool lit: false
    // Where its sides end and it narrows to its point, from the top
    readonly property real shoulder: Math.round(height * 0.5)

    implicitWidth: 9
    implicitHeight: 10

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: flag.color
            strokeColor: flag.lit ? "white" : "#15161a"
            strokeWidth: 1
            joinStyle: ShapePath.MiterJoin
            startX: 0.5
            startY: 0.5

            PathLine { x: flag.width - 0.5; y: 0.5 }
            PathLine { x: flag.width - 0.5; y: flag.shoulder }
            PathLine { x: flag.width / 2; y: flag.height - 0.5 }
            PathLine { x: 0.5; y: flag.shoulder }
            PathLine { x: 0.5; y: 0.5 }
        }
    }
}
