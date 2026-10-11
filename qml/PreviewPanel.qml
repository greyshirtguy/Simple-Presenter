import QtQuick
import QtMultimedia
import SimplePresenterApp

// The right of the operator window, from the toolbar to the bottom: what the audience
// is being shown and under it what the stage is, the buttons that clear the output (a
// row for the layers, and under it a row for the workspace's clear groups), the
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
    // The clear groups that have a button: all but those the workspace keeps out of
    // sight (see ClearGroups)
    readonly property var shownGroups: ClearGroups.groups.filter(group => !group.hidden)

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

    // Whether a clear group has anything to clear: whether any of its layers that
    // there are here has something on it.
    function groupLive(group) {
        return (group.layers.indexOf("slide") >= 0 && !win.cleared) || (group.layers.indexOf("media") >= 0 && win.liveMedia !== null)
               || (group.layers.indexOf("props") >= 0 && win.liveProps.length > 0)
    }

    // A line across the panel, between one lot of controls and the next
    component Rule: Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: sidePanel.gap
        height: 1
        color: "#3a3c42"
    }

    // A button that clears a layer of the output: red while there is something there
    // for it to clear, and grey once there is not. What it clears is on it as
    // ProPresenter's own picture of clearing that layer (see ProIcon).
    component ClearButton: Rectangle {
        id: clearButton

        // "slide", "media" or "props"
        property string kind
        property bool live: false
        // The layer it clears, as an action's file numbers them, and what an action
        // that clears it is called
        readonly property int clears: ({ "slide": 5, "media": 2, "props": 4 })[kind] ?? 5
        readonly property string title: ({ "slide": "Clear the Slide", "media": "Clear the Media", "props": "Clear the Props" })[kind] ?? ""
        readonly property string picture: ({ "slide": "ClearPresentation", "media": "ClearMedia", "props": "ClearProps" })[kind] ?? "Clear"
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        signal clicked

        width: (layerClears.width - 2 * layerClears.spacing) / 3
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                // (A right click has the menu the clears have.)
                if (mouse.button === Qt.RightButton)
                    sidePanel.win.showClearMenu(clearButton, "")
                else if (!dragged && clearButton.live)
                    clearButton.clicked()
            }
        }
    }

    // The button of a clear group, which clears the layers the group names, all at one
    // click: red while any of them has something on it, grey once none has. On it are
    // the group's picture, in its tint if it has one, and its name where there is room
    // for it (a row of several has room for the pictures alone).
    //
    // The buttons of the groups share the width of the row between them, so that one
    // group, which is what a workspace starts with, has a button as wide as the three
    // of the layers above it.
    component ClearGroupButton: Rectangle {
        id: groupButton

        // The group: { id, name, layers, icon, image, tinted, tint } (see ClearGroups)
        required property var modelData
        readonly property bool live: sidePanel.groupLive(modelData)
        readonly property bool tinted: modelData.tinted && modelData.tint !== ""
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        objectName: "clearGroup"
        width: (groupClears.width - (groupClears.count - 1) * groupClears.spacing) / Math.max(1, groupClears.count)
        height: 30
        radius: 6
        color: !live ? "#2b2d31" : groupMouse.pressed ? "#e25555" : groupMouse.containsMouse ? "#d84343" : "#c62828"

        Row {
            anchors.centerIn: parent
            spacing: 5

            // (A tint is the colour of the picture whether or not there is anything to
            // clear, and is drawn fainter when there is not: a picture is made for each
            // colour it is asked for in, so the colour itself is not what fades.)
            ClearGroupGlyph {
                objectName: "clearGroupPicture"
                anchors.verticalCenter: parent.verticalCenter
                icon: groupButton.modelData.icon
                image: groupButton.modelData.image
                ink: groupButton.tinted ? groupButton.modelData.tint : groupButton.ink
                opacity: groupButton.tinted && !groupButton.live ? 0.45 : 1
                size: 26
            }

            // (Its width is worked out from the name, and not taken from the text as it
            // is laid out, which a text that is cut short to fit cannot tell.)
            Text {
                objectName: "clearGroupName"
                anchors.verticalCenter: parent.verticalCenter
                visible: groupButton.width >= 110
                width: Math.min(Math.ceil(nameWidth.advanceWidth) + 1, Math.max(0, groupButton.width - 26 - 5 - 16))
                elide: Text.ElideRight
                color: groupButton.ink
                font.pixelSize: 13
                text: groupButton.modelData.name

                TextMetrics {
                    id: nameWidth

                    font.pixelSize: 13
                    text: groupButton.modelData.name
                }
            }
        }

        // A click clears, when there is something to clear. And the button can be
        // dragged, as those of the layers can, onto a slide or a macro, which gives that
        // the action that clears this group.
        DragSource {
            id: groupMouse

            anchors.fill: parent
            win: sidePanel.win
            payload: ({ kind: "clearGroup", id: groupButton.modelData.id, name: "Clear the group “" + groupButton.modelData.name + "”",
                        groupName: groupButton.modelData.name, picture: "Clear" })
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton)
                    sidePanel.win.showClearMenu(groupButton, groupButton.modelData.id)
                else if (!dragged && groupButton.live)
                    sidePanel.win.clearGroup(groupButton.modelData)
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

    // The clears, across the width like the tabs of the show controls, in two rows.
    // A right click on any of them has the way to the window the clear groups are set
    // up in.
    Column {
        id: clearButtons

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: stagePreview.bottom
        anchors.margins: sidePanel.gap
        spacing: 4

        // The layers: the slide (F2), the media (F3), the props (F4).
        //
        // TO COME: a button here for each layer this app gains (audio, messages,
        // announcements, video inputs), which is also when that layer is to be switched
        // on among the layers a clear group can be given (see `ours` in
        // src/clearfile.cpp) and cleared by State::clearGroup (src/showstate.cpp).
        Row {
            id: layerClears

            width: parent.width
            spacing: 4

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

        // The clear groups, a button each, in the workspace's order. The first is what
        // F1 clears. (A workspace all of whose groups are out of sight, or that has
        // none, has no second row.)
        Row {
            id: groupClears

            readonly property int count: sidePanel.shownGroups.length

            objectName: "clearGroups"
            width: parent.width
            spacing: 4
            visible: count > 0

            Repeater {
                model: sidePanel.shownGroups

                ClearGroupButton {}
            }
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
