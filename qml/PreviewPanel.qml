import QtQuick
import QtMultimedia
import SimplePresenterApp

// The right of the operator window, from the toolbar to the bottom: what the audience
// is being shown (with, at its right edge while anything is sounding, how loud that is)
// and under it what the stage is, the buttons that clear the output (a
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
    // What a playlist dragged out of the audio bin's list of them is carried by
    required property Item audioPlaylistDrag

    // Starts renaming a playlist or folder of the audio bin in place.
    function renameAudioPlaylist(id) {
        showControl.renameAudioPlaylist(id)
    }
    // Whether something in the show controls is being renamed in place, and so has the
    // keyboard
    readonly property bool renaming: showControl.renaming

    // The announcements in the preview: what a screen's scene is told, the preview is
    // told too (see showAnnouncement() and clearAnnouncements() in OutputScene.qml).
    function showAnnouncement(slide, showing, media) {
        previewAnnouncements.active = true
        const layer = previewAnnouncements.item
        if (showing === 2)
            layer.showSlideWithMedia(slide, media)
        else if (showing === 1)
            layer.showSlideOverMedia(slide)
        else
            layer.showSlide(slide)
    }

    function clearAnnouncements() {
        if (previewAnnouncements.item)
            previewAnnouncements.item.clear()
    }
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
               || (group.layers.indexOf("messages") >= 0 && win.liveMessages.length > 0)
               || (group.layers.indexOf("announcements") >= 0 && Show.announcementsOn)
               || (group.layers.indexOf("audio") >= 0 && win.audioLive)
               || (group.layers.indexOf("audioEffects") >= 0 && win.liveSounds.length > 0)
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

        // "slide", "media", "props", "audio", "messages" or "announcements"
        property string kind
        property bool live: false
        // The layer it clears, as an action's file numbers them, and what an action
        // that clears it is called. (The announcements are not one of the file's
        // layers but a destination that everything is cleared of: 0, and said beside
        // it. See clearfile.h.)
        readonly property int clears: ({ "slide": 5, "media": 2, "props": 4, "audio": 1, "messages": 7, "announcements": 0 })[kind] ?? 5
        readonly property string title: ({ "slide": "Clear the Slide", "media": "Clear the Media", "props": "Clear the Props",
                                           "audio": "Clear the Audio", "messages": "Clear the Messages",
                                           "announcements": "Clear the Announcements" })[kind] ?? ""
        readonly property string picture: ({ "slide": "ClearPresentation", "media": "ClearMedia", "props": "ClearProps", "audio": "ClearAudio",
                                             "messages": "ClearMessages", "announcements": "ClearAnnouncements" })[kind] ?? "Clear"
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        signal clicked

        width: (layerClears.width - 5 * layerClears.spacing) / 6
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
            payload: ({ kind: "clear", id: "", layer: clearButton.clears, announcements: clearButton.kind === "announcements",
                        name: clearButton.title, picture: clearButton.picture })
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
    // group, which is what a workspace starts with, has a button as wide as the row
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

            // The announcements, over the slide and under the props, as the output
            // has them: their layer again, fed the frames of a video that the output
            // plays (it does not lead), and told what the screens are told. Made when
            // the first announcement is shown, and kept.
            Loader {
                id: previewAnnouncements

                objectName: "previewAnnouncements"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.announcements !== false
                active: false
                sourceComponent: AnnouncementLayer {
                    leads: false
                }
            }

            PropsLayer {
                objectName: "previewProps"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.props
                props: sidePanel.win.shownProps
                duration: Math.round(Props.transitionDuration * 1000)
                effects: false
            }

            // The messages, over the props, as the output has them: made when the
            // first is shown, and kept (see OutputScene).
            Loader {
                id: previewMessages

                property bool wanted: sidePanel.win.shownMessages.length > 0

                objectName: "previewMessages"
                anchors.fill: parent
                visible: sidePanel.win.previewLook.messages
                active: false
                onWantedChanged: {
                    if (wanted)
                        active = true
                }

                sourceComponent: PropsLayer {
                    props: sidePanel.win.shownMessages
                    duration: Math.round(Props.transitionDuration * 1000)
                    effects: false
                }
            }

            // The mask the live look gives the screen the preview stands for, over
            // everything, as the output has it (see OutputScene)
            Loader {
                objectName: "previewMask"
                anchors.fill: parent
                active: sidePanel.win.previewMask !== null

                sourceComponent: Slide {
                    slide: sidePanel.win.previewMask
                    effects: false
                }
            }

            // How loud what the app is playing is, on the left and on the right: two
            // bars up the preview's right edge, there only while something is sounding
            // (a piece of audio, a video with its sound) and for the moment after in
            // which they fall, and made only then. They go by decibels, each tenth of
            // their height being 6 dB (see AudioMeter): green, then yellow for the top
            // 12 dB, then red for the top 3, where sound is near as loud as it can be.
            //
            // A GUESS at ProPresenter, whose preview has meters at its side: how many
            // bars, where the colours change and how fast they fall are this app's own.
            Loader {
                objectName: "audioMeter"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 13
                active: AudioMeter.sounding

                sourceComponent: Rectangle {
                    color: "#b0000000"

                    component Bar: Item {
                        id: bar

                        // How far up it is, from 0 to 1
                        property real level: 0

                        width: 4

                        Rectangle {
                            anchors.fill: parent
                            color: "#30ffffff"
                        }

                        // Three stretches, each as much of itself as the level reaches
                        Rectangle {
                            objectName: "audioMeterGreen"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Math.min(bar.level, 0.8) * parent.height
                            color: "#35c463"
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            y: parent.height * (1 - Math.min(bar.level, 0.95))
                            height: Math.max(0, Math.min(bar.level, 0.95) - 0.8) * parent.height
                            color: "#e6c229"
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            y: parent.height * (1 - bar.level)
                            height: Math.max(0, bar.level - 0.95) * parent.height
                            color: "#e5484d"
                        }
                    }

                    Bar {
                        objectName: "audioMeterLeft"
                        x: 2
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 3
                        level: AudioMeter.left
                    }

                    Bar {
                        objectName: "audioMeterRight"
                        x: 7
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 3
                        level: AudioMeter.right
                    }
                }
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

        // The layers: the slide (F2), the media (F3), the props (F4), the audio (F5),
        // which is the music and the sound effects of the audio bin, the messages (F6),
        // and the announcements, which have no key here.
        //
        // A GUESS: that F6 is ProPresenter's key for clearing the messages. Its keys
        // for the first four are known; this one follows on from them. Which key it
        // has for the announcements is not known, so they are given none.
        //
        // TO COME: a button here for the layer this app has not yet (video inputs),
        // which is also when that layer is to be switched on among the layers a clear
        // group can be given (see `ours` in src/clearfile.cpp) and cleared by
        // State::clearGroup (src/showstate.cpp).
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

            ClearButton {
                objectName: "clearAudio"
                kind: "audio"
                live: sidePanel.win.audioLive
                onClicked: sidePanel.win.clearAudio()
            }

            ClearButton {
                objectName: "clearMessages"
                kind: "messages"
                live: sidePanel.win.liveMessages.length > 0
                onClicked: sidePanel.win.clearMessages()
            }

            // The announcements: ProPresenter's megaphone. Red while their layer has
            // anything on it, a slide or media.
            ClearButton {
                objectName: "clearAnnouncements"
                kind: "announcements"
                live: Show.announcementsOn
                onClicked: sidePanel.win.clearAnnouncements()
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
        audioPlaylistDrag: sidePanel.audioPlaylistDrag
    }
}
