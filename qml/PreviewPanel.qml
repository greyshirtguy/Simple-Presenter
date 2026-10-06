import QtQuick
import QtQuick.Shapes
import QtMultimedia
import SimplePresenterApp

// The right of the operator window, from the toolbar to the bottom: what the audience
// is being shown and under it what the stage is, the buttons that clear the output, the
// transport for a video that is playing, and the show controls (timers and props, and
// in time the stage display). A line each side of the transport sets the three lots of
// controls apart.
//
// The previews are built from cheap parts instead of second copies of the outputs. The
// slide is drawn again at this small size; a still image comes from its cached
// thumbnail; and video borrows a few frames a second from the output's own decoder (see
// FrameRelay), so previewing a video costs a handful of small texture uploads, not a
// second decode of the file.
Rectangle {
    id: sidePanel

    // The operator window: what this shows is its state, and what the buttons here do is
    // call its functions.
    required property var win
    // Where the output's playing video delivers its frames, or null: the preview borrows
    // a few of them a second.
    property var liveVideoSink: null
    // The player of the output's video, or null: what the transport works
    property var livePlayer: null
    // Which tab of the show controls is showing
    property alias showControlTab: showControl.tab
    // Whether something in the show controls is being renamed in place, and so has the
    // keyboard
    readonly property bool renaming: showControl.renaming

    color: "black"

    // The space between one thing and the next, and at the panel's edges
    readonly property real gap: 12
    // Both previews are 16:9 and as wide as the panel, unless the panel is too short
    // for that with everything under them, in which case they shrink to fit and stay
    // centred. What they shrink for is the show controls, which are left room for four
    // timers; but previews too small to make anything out in are no use, so below a
    // width that is still worth having it is the show controls that give way, down to
    // room for two.
    readonly property real showControlRoom: 216
    readonly property real leastShowControlRoom: 136
    readonly property real leastUsefulPreview: 150
    readonly property real previewWidth: Math.max(80, Math.min(
        width - 2 * gap, Math.max(previewWidthLeaving(showControlRoom),
                                  Math.min(leastUsefulPreview, previewWidthLeaving(leastShowControlRoom)))))

    // How wide the previews can be if this much height is to be left under everything
    // else for the show controls.
    function previewWidthLeaving(room) {
        const others = 8 + clearButtons.height + transport.height + 2 * line.height + 7 * gap
        return (height - others - room) / 2 * 16 / 9
    }

    // A line across the panel, between one lot of controls and the next
    component Rule: Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: sidePanel.gap
        height: 1
        color: "#3a3c42"
    }

    // A button that clears a layer of the output, or all of them: red while there is
    // something there for it to clear, and grey once there is not. What it clears is
    // drawn on it in thin lines, which leave the red to be seen.
    component ClearButton: Rectangle {
        id: clearButton

        // "all", "slide", "media" or "props"
        property string kind
        property bool live: false
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        signal clicked

        width: (clearButtons.width - 3 * clearButtons.spacing) / 4
        height: 30
        radius: 6
        color: !live ? "#2b2d31" : clearMouse.pressed ? "#e25555" : clearMouse.containsMouse ? "#d84343" : "#c62828"

        Shape {
            anchors.centerIn: parent
            width: 18
            height: 16
            preferredRendererType: Shape.CurveRenderer

            // All: a cross in a circle
            ShapePath {
                strokeColor: clearButton.kind === "all" ? clearButton.ink : "transparent"
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathSvg {
                    path: "M 9 1 A 7 7 0 1 1 8.99 1 Z M 6.3 5.3 L 11.7 10.7 M 11.7 5.3 L 6.3 10.7"
                }
            }

            // The slide: a square with three lines of words in it
            ShapePath {
                strokeColor: clearButton.kind === "slide" ? clearButton.ink : "transparent"
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M 2.5 1 L 15.5 1 L 15.5 15 L 2.5 15 Z M 5.5 4.8 L 12.5 4.8 M 5.5 8 L 12.5 8 M 5.5 11.2 L 10.5 11.2"
                }
            }

            // The media: two mountains, and the sun over them
            ShapePath {
                strokeColor: clearButton.kind === "media" ? clearButton.ink : "transparent"
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M 0.8 14.5 L 6.2 5.5 L 9.6 11.2 L 11.8 7.6 L 17.2 14.5 Z M 14.2 1.4 A 1.5 1.5 0 1 1 14.19 1.4 Z"
                }
            }

            // The props: the picture, with something laid over its corner
            ShapePath {
                strokeColor: clearButton.kind === "props" ? clearButton.ink : "transparent"
                strokeWidth: 1.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin

                PathSvg {
                    path: "M 1 2 L 17 2 L 17 14 L 1 14 Z M 9 7.5 L 14 7.5 L 14 11 L 9 11 Z"
                }
            }
        }

        MouseArea {
            id: clearMouse

            anchors.fill: parent
            hoverEnabled: true
            enabled: clearButton.live
            onClicked: clearButton.clicked()
        }
    }

    // The output, small: its media with its slide over it, and the props over both
    Rectangle {
        id: preview

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: sidePanel.gap
        width: sidePanel.previewWidth
        height: width * 9 / 16
        color: "black"
        border.width: 1
        border.color: "#3a3c42"

        Item {
            anchors.fill: parent
            anchors.margins: 1

            Image {
                anchors.fill: parent
                visible: sidePanel.win.liveMedia !== null && !sidePanel.win.liveMedia.video
                source: visible ? sidePanel.win.thumbnailUrl(sidePanel.win.liveMedia.path) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            VideoOutput {
                id: previewVideo

                anchors.fill: parent
                visible: sidePanel.win.liveMedia !== null && sidePanel.win.liveMedia.video
                fillMode: VideoOutput.PreserveAspectFit
            }

            FrameRelay {
                id: relay

                source: sidePanel.liveVideoSink
                target: previewVideo.videoSink
                interval: 100
            }

            Slide {
                anchors.fill: parent
                slide: sidePanel.win.liveSlide
                effects: false
            }

            PropsLayer {
                anchors.fill: parent
                props: sidePanel.win.shownProps
                duration: Math.round(Props.transitionDuration * 1000)
                effects: false
            }
        }
    }

    // The stage display, small
    Rectangle {
        id: stagePreview

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: preview.bottom
        anchors.topMargin: 8
        width: sidePanel.previewWidth
        height: width * 9 / 16
        color: "black"
        border.width: 1
        border.color: "#3a3c42"

        StageView {
            anchors.fill: parent
            anchors.margins: 1
            currentText: sidePanel.win.stageCurrentText
            nextText: sidePanel.win.stageNextText
        }
    }

    // The clears, across the width like the tabs of the show controls: everything (F1),
    // the slide (F2), the media (F3), the props (F4)
    Row {
        id: clearButtons

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: stagePreview.bottom
        anchors.margins: sidePanel.gap
        spacing: 4

        ClearButton {
            objectName: "clearAll"
            kind: "all"
            live: !sidePanel.win.cleared || sidePanel.win.liveMedia !== null || sidePanel.win.liveProps.length > 0
            onClicked: sidePanel.win.clearAll()
        }

        ClearButton {
            objectName: "clearSlide"
            kind: "slide"
            live: !sidePanel.win.cleared
            onClicked: sidePanel.win.clearSlide()
        }

        ClearButton {
            objectName: "clearMedia"
            kind: "media"
            live: sidePanel.win.liveMedia !== null
            onClicked: sidePanel.win.clearMedia()
        }

        ClearButton {
            objectName: "clearProps"
            kind: "props"
            live: sidePanel.win.liveProps.length > 0
            onClicked: sidePanel.win.clearProps()
        }
    }

    Rule {
        id: line

        anchors.top: clearButtons.bottom
    }

    Transport {
        id: transport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: line.bottom
        anchors.margins: sidePanel.gap
        player: sidePanel.livePlayer
        media: sidePanel.win.liveMedia
        pulse: relay
    }

    Rule {
        id: secondLine

        anchors.top: transport.bottom
    }

    ShowControl {
        id: showControl

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: secondLine.bottom
        anchors.bottom: parent.bottom
        anchors.margins: sidePanel.gap
        win: sidePanel.win
    }
}
