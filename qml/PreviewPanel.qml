import QtQuick
import QtMultimedia
import SimplePresenterApp

// The right of the operator window, from the toolbar to the bottom: what the audience
// is being shown and under it what the stage is, the buttons that clear the output, the
// transport for a video that is playing, and the show controls (timers, props and the
// stage display). A line each side of the transport sets the three lots of controls
// apart.
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
    // on it as ProPresenter's own picture of clearing that layer (see ProIcon).
    component ClearButton: Rectangle {
        id: clearButton

        // "all", "slide", "media" or "props"
        property string kind
        property bool live: false
        // The layer it clears, as an action's file numbers them, and what an action
        // that clears it is called
        readonly property int clears: ({ "all": 0, "slide": 5, "media": 2, "props": 4 })[kind] ?? 0
        readonly property string title: ({ "all": "Clear Everything", "slide": "Clear the Slide", "media": "Clear the Media",
                                           "props": "Clear the Props" })[kind] ?? ""
        readonly property string picture: ({ "all": "Clear", "slide": "ClearPresentation", "media": "ClearMedia",
                                             "props": "ClearProps" })[kind] ?? "Clear"
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        signal clicked

        width: (clearButtons.width - 3 * clearButtons.spacing) / 4
        height: 30
        radius: 6
        color: !live ? "#2b2d31" : clearMouse.pressed ? "#e25555" : clearMouse.containsMouse ? "#d84343" : "#c62828"

        // ProPresenter's own picture of clearing that layer
        ProIcon {
            anchors.centerIn: parent
            name: clearButton.picture
            ink: clearButton.ink
            size: 26
        }

        // A click clears, when there is something to clear. And the button can be
        // dragged, whether there is or not, onto a slide or a macro, which gives that
        // the action that clears this layer (the app's own way of adding one).
        DragSource {
            id: clearMouse

            anchors.fill: parent
            win: sidePanel.win
            payload: ({ kind: "clear", id: "", layer: clearButton.clears, name: clearButton.title, picture: clearButton.picture })
            hoverEnabled: true
            onClicked: {
                if (!dragged && clearButton.live)
                    clearButton.clicked()
            }
        }
    }

    // The output, small: its media with its slide over it, and the props over both. It
    // stands for the first audience screen, and so has what the live look gives that
    // screen and no more (see previewLook in Main.qml): a layer the look keeps from
    // the screen is not in the preview either, and the slide is in the screen's theme.
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
                visible: sidePanel.win.previewLook.media && sidePanel.win.liveMedia !== null && !sidePanel.win.liveMedia.video
                source: visible ? sidePanel.win.thumbnailUrl(sidePanel.win.liveMedia.path) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            VideoOutput {
                id: previewVideo

                objectName: "previewVideo"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.media && sidePanel.win.liveMedia !== null && sidePanel.win.liveMedia.video
                fillMode: VideoOutput.PreserveAspectFit
            }

            // (No frames are borrowed for a video the look keeps from the screen.)
            FrameRelay {
                id: relay

                source: sidePanel.win.previewLook.media ? sidePanel.liveVideoSink : null
                target: previewVideo.videoSink
                interval: 100
            }

            Slide {
                objectName: "previewSlide"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.slide
                slide: sidePanel.win.previewSlide
                effects: false
            }

            PropsLayer {
                objectName: "previewProps"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.props
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

        // The layout the stage has, or with none the plain view
        StageView {
            anchors.fill: parent
            anchors.margins: 1
            visible: sidePanel.win.stageLayout === null
            currentText: sidePanel.win.stageCurrentText
            nextText: sidePanel.win.stageNextText
        }

        Slide {
            anchors.fill: parent
            anchors.margins: 1
            visible: sidePanel.win.stageLayout !== null
            slide: sidePanel.win.stageLayout ? sidePanel.win.stageLayout.slide : null
            effects: false
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
