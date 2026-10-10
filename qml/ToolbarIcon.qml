import QtQuick
import QtQuick.Shapes

// A toolbar button drawn as a small icon with a caption under it. `kind` picks the icon.
//
// Three are ProPresenter's own pictures (see ProIcon), for the three things in this
// toolbar that are in ProPresenter's: "looks" is its glasses and moustache, orange
// while `on`; "screens" is its screen on a stand, which is green while `on` (the
// audience screens are being sent out); "stage" is its speaker at a lectern, green
// while `on` likewise.
//
// The rest are this app's, for what ProPresenter's pack has no picture of: "bin" is a
// window with its bottom pane filled when `on`; "settings" is a set of sliders; "edit"
// is a pencil, orange when `on`; "show" is the triangle that means play, orange when
// `on`; "search" is a magnifying glass; "themes" is one slide behind another; "expand"
// is four corners turned outwards, the sign for filling the screen, orange when `on`.
//
// A caption is as wide as it needs to be, up to `captionWidth`, and the button with it:
// most say one short word, and the Looks button says the name of the look that is live.
Item {
    id: button

    property string kind
    property string label
    property bool on: false
    // For a button whose work can also be done by holding a key: how far the hold has
    // got, from 0 to 1, shown as a line growing along the foot of the button
    property real progress: 0

    signal clicked

    readonly property color ink: "#e6e6e6"
    readonly property color accent: "#ff8a1f"
    // The green of ProPresenter's own picture of the screens switched on
    readonly property color lit: "#5ab53b"
    // ProPresenter's picture for this kind of button, if it has one, and what it is
    // drawn in. (The screens switched on are a picture with its own colours.)
    readonly property string picture: kind === "looks" ? "Looks" : kind === "stage" ? "Stage"
                                    : kind === "screens" ? (on ? "ScreensOn" : "Screens") : ""
    readonly property color pictureInk: kind === "looks" ? (on ? accent : ink) : kind === "stage" ? (on ? lit : ink)
                                      : on ? "transparent" : ink
    // The widest a caption gets before it is cut short with an ellipsis
    property real captionWidth: 120

    width: Math.max(52, Math.min(captionWidth, caption.implicitWidth) + 12)
    height: 40

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: mouse.pressed ? "#50535a" : mouse.containsMouse ? "#3a3c42" : "transparent"
    }

    Item {
        id: icon

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        width: 18
        height: 14

        // One of ProPresenter's pictures. Its drawing is the middle of the square, so
        // the square overhangs this box, above and below.
        ProIcon {
            anchors.centerIn: parent
            visible: button.picture !== ""
            name: button.picture
            ink: button.pictureInk
            size: 23
        }

        Rectangle {
            anchors.fill: parent
            visible: button.kind === "bin"
            radius: 2
            color: "transparent"
            border.width: 1.5
            border.color: button.ink

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 3
                height: 4
                color: button.on ? button.ink : "transparent"
                border.width: 1
                border.color: button.ink
            }
        }

        // The triangle that means play
        Shape {
            anchors.fill: parent
            visible: button.kind === "show"
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: button.on ? "#ff8a1f" : button.ink
                strokeColor: "transparent"

                PathPolyline {
                    path: [Qt.point(5, 0.5), Qt.point(15, 7), Qt.point(5, 13.5), Qt.point(5, 0.5)]
                }
            }
        }

        // A pencil, point down to the left
        Shape {
            anchors.fill: parent
            visible: button.kind === "edit"
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: button.on ? "#ff8a1f" : button.ink
                strokeColor: "transparent"

                PathPolyline {
                    path: [Qt.point(3, 12.5), Qt.point(4.2, 8.4), Qt.point(11.6, 1), Qt.point(15, 4.4),
                           Qt.point(7.6, 11.8), Qt.point(3, 12.5)]
                }
            }
        }

        // A magnifying glass
        Shape {
            anchors.fill: parent
            visible: button.kind === "search"
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: button.on ? "#ff8a1f" : button.ink
                strokeWidth: 1.7
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: 7.5
                    centerY: 6
                    radiusX: 4.6
                    radiusY: 4.6
                    startAngle: 0
                    sweepAngle: 360
                }

                PathMove {
                    x: 11
                    y: 9.5
                }

                PathLine {
                    x: 15
                    y: 13.5
                }
            }
        }

        // A slide with lines of text on it, behind another: the looks slides can be given
        Shape {
            anchors.fill: parent
            visible: button.kind === "themes"
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: button.on ? "#ff8a1f" : button.ink
                strokeWidth: 1.5
                joinStyle: ShapePath.RoundJoin

                PathMultiline {
                    paths: [
                        [Qt.point(1.5, 3.5), Qt.point(13, 3.5), Qt.point(13, 12.5), Qt.point(1.5, 12.5), Qt.point(1.5, 3.5)],
                        [Qt.point(4.5, 1), Qt.point(16.5, 1), Qt.point(16.5, 9.5)],
                        [Qt.point(4, 7), Qt.point(10.5, 7)],
                        [Qt.point(4, 9.7), Qt.point(8.5, 9.7)]
                    ]
                }
            }
        }

        // Four corners turned outwards
        Shape {
            anchors.fill: parent
            visible: button.kind === "expand"
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: "transparent"
                strokeColor: button.on ? "#ff8a1f" : button.ink
                strokeWidth: 1.6
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin

                PathMultiline {
                    paths: [
                        [Qt.point(2, 5), Qt.point(2, 1), Qt.point(6, 1)],
                        [Qt.point(12, 1), Qt.point(16, 1), Qt.point(16, 5)],
                        [Qt.point(2, 9), Qt.point(2, 13), Qt.point(6, 13)],
                        [Qt.point(12, 13), Qt.point(16, 13), Qt.point(16, 9)]
                    ]
                }
            }
        }

        Repeater {
            model: button.kind === "settings" ? [0.25, 0.7, 0.4] : []

            delegate: Item {
                id: line

                required property real modelData
                required property int index

                y: 1 + index * 5
                width: icon.width
                height: 3

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 1.5
                    color: button.ink
                }

                Rectangle {
                    x: line.modelData * (parent.width - width)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 5
                    height: 5
                    radius: 2.5
                    color: button.ink
                }
            }
        }
    }

    Text {
        id: caption

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        width: Math.min(implicitWidth, button.captionWidth)
        elide: Text.ElideRight
        color: "#c9cbd0"
        font.pixelSize: 10
        text: button.label
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        width: (parent.width - 8) * button.progress
        height: 2
        visible: button.progress > 0
        color: "#ff8a1f"
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.clicked()
    }
}
