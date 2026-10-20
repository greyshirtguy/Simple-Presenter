import QtQuick
import QtMultimedia
import SimplePresenterApp

// The right of the operator window, from the toolbar to the bottom, laid out as
// ProPresenter's is: the preview of one of the screens (with, at its left edge while
// anything is sounding, how loud that is), and under it the picker of which screen that
// is; at its right the buttons that clear the output, in two columns up against the
// window's edge, the workspace's clear groups and then the layers, each column as high
// as the preview's area; then the transport for a video that is playing, and the show
// controls (timers, props, the stage display and the rest). A line each side of the
// transport sets the three lots of controls apart.
//
// One preview, of whichever screen is picked (an audience screen, as the live look has
// it, or a stage screen, with its layout), and not one of the output and one of the
// stage as it was until 0.103: the maintainer's asking, 2026-10-21, for the height it
// gives the show controls, and so that any screen can be looked at. A stage layout of
// shapes filled with Screen Previews, on a stage screen that is sent nowhere, makes a
// preview of several screens at once.
//
// The preview is built from cheap parts instead of a second copy of the screen (see
// ScreenPicture). The slide is drawn again at this small size; a still image comes from
// its cached thumbnail; and video borrows a few frames a second from the output's own
// decoder (see FrameRelay), so previewing a video costs a handful of small texture
// uploads, not a second decode of the file. Only the one screen picked is drawn here.
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

    // The clear groups that have a button: all but those the workspace keeps out of
    // sight (see ClearGroups)
    readonly property var shownGroups: ClearGroups.groups.filter(group => !group.hidden)

    color: "black"

    // The space between one thing and the next, and at the panel's edges
    readonly property real gap: 12
    // The two columns of clears: slim, as ProPresenter's are, the layers' a little the
    // wider for their pictures, with a narrow space between the buttons; and the least
    // the layers' column is high, six buttons that can still be hit
    readonly property real groupColumnWidth: 30
    readonly property real layerColumnWidth: 40
    readonly property real columnGap: 4
    readonly property real clearsWidth: (shownGroups.length > 0 ? groupColumnWidth + columnGap : 0) + layerColumnWidth
    readonly property real leastClearsHeight: 6 * 24 + 5 * columnGap
    // The preview is 16:9 and as wide as the panel leaves it beside the clears, unless
    // the panel is too short for that with everything under it, in which case it
    // shrinks to fit and stays centred. What it shrinks for is the show controls, which
    // are left room for four timers; but a preview too small to make anything out in is
    // no use, so below a width that is still worth having it is the show controls that
    // give way, down to room for two.
    readonly property real showControlRoom: 216
    readonly property real leastShowControlRoom: 136
    readonly property real leastUsefulPreview: 150
    readonly property real previewWidth: Math.max(80, Math.min(
        width - 2 * gap - 8 - clearsWidth, Math.max(previewWidthLeaving(showControlRoom),
                                                    Math.min(leastUsefulPreview, previewWidthLeaving(leastShowControlRoom)))))
    // The preview's area, which the clears are as high as. Left as it comes, it is the
    // preview, or as much as the clears need, if that is more, the preview centred in
    // it. Dragged taller or shorter by the line under it (the operator's
    // `previewHeight`), the preview fills it, as wide as there is room for, and the
    // screen is fitted in that: a stage screen with a tall layout of Screen Previews,
    // one over another, which is a way of seeing several screens at once, can be made
    // as tall as it wants. It is never less than the clears need, nor more than leaves
    // the show controls room for two timers.
    readonly property bool sized: win.previewHeight > 0
    readonly property real mostAreaHeight: Math.max(leastClearsHeight,
        height - (4 + picker.height + transport.height + 2 * line.height + 6 * gap) - leastShowControlRoom)
    readonly property real areaHeight: sized ? Math.max(leastClearsHeight, Math.min(win.previewHeight, mostAreaHeight))
                                             : Math.max(previewWidth * 9 / 16 + 2, leastClearsHeight)

    // How wide the preview can be if this much height is to be left under everything
    // else for the show controls.
    function previewWidthLeaving(room) {
        const others = 4 + picker.height + transport.height + 2 * line.height + 6 * gap
        return (height - others - room) * 16 / 9
    }

    // The screens the preview can show, as the rows of a menu: the audience screens and
    // then the stage screens, each under ProPresenter's own heading for them, the one
    // shown ticked
    function screenItems() {
        const row = screen => ({ label: screen.name, current: screen.id === win.previewScreenId, run: () => win.pickPreviewScreen(screen.id) })
        const items = [{ header: "Audience Screens" }].concat(Screens.audience.map(row))
        if (Screens.stage.length > 0)
            return items.concat([{ header: "Stage Screens" }]).concat(Screens.stage.map(row))
        return items
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

        width: layerColumn.width
        height: (layerColumn.height - 5 * layerColumn.spacing) / 6
        radius: 5
        color: !live ? "#2b2d31" : clearMouse.pressed ? "#e25555" : clearMouse.containsMouse ? "#d84343" : "#c62828"

        // ProPresenter's own picture of clearing that layer, and nothing else
        ProIcon {
            anchors.centerIn: parent
            name: clearButton.picture
            ink: clearButton.ink
            size: Math.min(26, clearButton.height + 2, clearButton.width - 6)
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
    // click: red while any of them has something on it, grey once none has. On it is
    // the group's picture, in its tint if it has one, and nothing else.
    //
    // The buttons of the groups share the height of their column between them, so that
    // one group, which is what a workspace starts with (Clear All), has a button as
    // high as the preview's area, as ProPresenter's has.
    component ClearGroupButton: Rectangle {
        id: groupButton

        // The group: { id, name, layers, icon, image, tinted, tint } (see ClearGroups)
        required property var modelData
        readonly property bool live: sidePanel.groupLive(modelData)
        readonly property bool tinted: modelData.tinted && modelData.tint !== ""
        readonly property color ink: live ? "#ececec" : "#6c6f75"

        objectName: "clearGroup"
        width: groupColumn.width
        height: (groupColumn.height - (groupColumn.count - 1) * groupColumn.spacing) / Math.max(1, groupColumn.count)
        radius: 5
        color: !live ? "#2b2d31" : groupMouse.pressed ? "#e25555" : groupMouse.containsMouse ? "#d84343" : "#c62828"

        // (A tint is the colour of the picture whether or not there is anything to
        // clear, and is drawn fainter when there is not: a picture is made for each
        // colour it is asked for in, so the colour itself is not what fades.)
        ClearGroupGlyph {
            objectName: "clearGroupPicture"
            anchors.centerIn: parent
            icon: groupButton.modelData.icon
            image: groupButton.modelData.image
            ink: groupButton.tinted ? groupButton.modelData.tint : groupButton.ink
            opacity: groupButton.tinted && !groupButton.live ? 0.45 : 1
            size: Math.max(12, Math.min(24, groupButton.height - 4, groupButton.width - 6))
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

    // The preview: one of the screens, small, as ScreenPicture draws it. An audience
    // screen has what the live look gives it and no more (its layers, its theme, its
    // mask), and a stage screen its layout or the plain view; its slides have the
    // Screen Previews in them. It is 16:9 whatever the screen is, as ProPresenter's is,
    // with the screen fitted in it, unless its area has been made taller or shorter
    // (see areaHeight), which it then fills.
    Item {
        id: previewArea

        anchors.left: parent.left
        anchors.leftMargin: sidePanel.gap
        anchors.right: clearColumns.left
        anchors.rightMargin: 8
        anchors.top: parent.top
        anchors.topMargin: sidePanel.gap
        height: sidePanel.areaHeight

        Rectangle {
            id: preview

            anchors.centerIn: parent
            width: sidePanel.sized ? previewArea.width : sidePanel.previewWidth
            height: sidePanel.sized ? previewArea.height : width * 9 / 16
            color: "black"
            border.width: 1
            border.color: "#3a3c42"

            Item {
                anchors.fill: parent
                anchors.margins: 1

                // A video's frames a few a second
                ScreenPicture {
                    id: previewPicture

                    anchors.fill: parent
                    win: sidePanel.win
                    screenId: sidePanel.win.previewScreenId
                    inner: true
                    videoInterval: 100
                    namePrefix: "preview"
                }

            // How loud what the app is playing is, on the left and on the right: two
            // bars up the preview's left edge, as ProPresenter has its meters, there
            // only while something is sounding (a piece of audio, a video with its
            // sound) and for the moment after in which they fall, and made only then. They go by decibels, each tenth of
            // their height being 6 dB (see AudioMeter): green, then yellow for the top
            // 12 dB, then red for the top 3, where sound is near as loud as it can be.
            //
            // A GUESS at ProPresenter, whose preview has meters at its side: how many
            // bars, where the colours change and how fast they fall are this app's own.
            Loader {
                objectName: "audioMeter"
                anchors.left: parent.left
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
    }

    // Which screen the preview shows: its name, under the preview's left corner, which
    // opens the menu of the screens there are (see screenItems), as ProPresenter's
    // picker under its preview does. What is picked is kept for the workspace (see
    // pickPreviewScreen in Main.qml).
    Rectangle {
        id: picker

        objectName: "previewScreenPicker"
        x: previewArea.x + preview.x
        anchors.top: previewArea.bottom
        anchors.topMargin: 4
        width: Math.min(pickedName.implicitWidth + 30, previewArea.width)
        height: 22
        radius: 6
        color: pickerMouse.pressed ? "#50535a" : pickerMouse.containsMouse ? "#45484e" : "#2b2d31"

        Text {
            id: pickedName

            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: "#c9ccd1"
            font.pixelSize: 12
            text: (Screens.screens.find(screen => screen.id === sidePanel.win.previewScreenId) ?? { name: "No screen" }).name
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: "#9a9da3"
            font.pixelSize: 10
            text: "▾"
        }

        MouseArea {
            id: pickerMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                sidePanel.win.takeFocus()
                sidePanel.win.showMenu(sidePanel.screenItems(), picker, 0, picker.height + 2)
            }
        }
    }

    // The clears, in two columns at the right of the preview, as high as the preview's
    // area, at the window's edge: the workspace's clear groups, sharing their column's
    // height, and the layers. A right click on any of them has the way to the window
    // the clear groups are set up in.
    Row {
        id: clearColumns

        anchors.right: parent.right
        anchors.rightMargin: sidePanel.gap
        anchors.top: previewArea.top
        height: sidePanel.areaHeight
        spacing: sidePanel.columnGap

        // The clear groups, a button each, in the workspace's order. The first is what
        // F1 clears. (A workspace all of whose groups are out of sight, or that has
        // none, has no such column.)
        Column {
            id: groupColumn

            readonly property int count: sidePanel.shownGroups.length

            objectName: "clearGroups"
            width: sidePanel.groupColumnWidth
            height: parent.height
            spacing: sidePanel.columnGap
            visible: count > 0

            Repeater {
                model: sidePanel.shownGroups

                ClearGroupButton {}
            }
        }

        // The layers, in ProPresenter's order down its column: the audio (F5), which is
        // the music and the sound effects of the audio bin, the messages (F6), the props
        // (F4), the announcements, which have no key here, the slide (F2) and the media
        // (F3).
        //
        // A GUESS: that F6 is ProPresenter's key for clearing the messages. Its keys
        // for the first four are known; this one follows on from them. Which key it
        // has for the announcements is not known, so they are given none.
        //
        // TO COME: a button here, under the media's, for the layer this app has not yet
        // (video inputs), which ProPresenter's column has last; that is also when that
        // layer is to be switched on among the layers a clear group can be given (see
        // `ours` in src/clearfile.cpp) and cleared by State::clearGroup
        // (src/showstate.cpp).
        Column {
            id: layerColumn

            width: sidePanel.layerColumnWidth
            height: parent.height
            spacing: sidePanel.columnGap

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

            ClearButton {
                objectName: "clearProps"
                kind: "props"
                live: sidePanel.win.liveProps.length > 0
                onClicked: sidePanel.win.clearProps()
            }

            // The announcements: ProPresenter's megaphone. Red while their layer has
            // anything on it, a slide or media.
            ClearButton {
                objectName: "clearAnnouncements"
                kind: "announcements"
                live: Show.announcementsOn
                onClicked: sidePanel.win.clearAnnouncements()
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
        }
    }

    // The line under the preview and its picker, which is dragged up or down to make the
    // preview's area shorter or taller, as the panes are made wider or narrower by their
    // edges; a double click gives the preview back its 16:9.
    Divider {
        id: line

        objectName: "previewDivider"
        vertical: false
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: picker.bottom
        anchors.topMargin: sidePanel.gap - 3
        onMoved: (delta) => sidePanel.win.previewHeight = Math.max(sidePanel.leastClearsHeight,
                                                                    Math.min(sidePanel.areaHeight + delta, sidePanel.mostAreaHeight))
        onDoubleClicked: sidePanel.win.previewHeight = 0
    }

    Transport {
        id: transport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: line.bottom
        anchors.margins: sidePanel.gap
        anchors.topMargin: sidePanel.gap - 3
        player: sidePanel.livePlayer
        media: sidePanel.win.liveMedia
        pulse: previewPicture.relay
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
