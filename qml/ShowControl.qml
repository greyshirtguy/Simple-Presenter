import QtQuick

// The show controls: what is worked during a show besides the slides and the media, a
// tab for each kind: the timers, the props, the stage screens, the macros, the audio
// bin, and the messages.
//
// Whatever a tab lists can also be dragged onto a slide, which gives the slide the
// action that goes with it: do this to that timer, show that prop, give the stage
// that layout, run that macro (see DragSource, and dropActionOnSlide in Main.qml).
//
// The tabs are a row of buttons across the whole width, pictures and not words, as
// ProPresenter's are, and the pictures are ProPresenter's own: its timer, its pile of
// layers for the props, its speaker at a lectern for the stage, its M in brackets for
// the macros, its paper plane for the messages. (The audio bin's, two joined notes, is
// drawn here: ProPresenter's pack has no picture of it. See AudioGlyph.) The one whose
// tab is showing is blue. (A GUESS: where the messages' tab stands among ProPresenter's
// own. It is the last here, being the last that was made.) Under them,
// at the right, is the button that adds to what the tab holds.
//
// A piece of audio dragged out of the audio bin and held over a tab brings that tab
// forward after a moment, so that it can be dropped on what the tab holds: on a macro.
// (Only a piece of audio. What is dragged is carried by the row it was dragged from,
// and a row that is put out of sight lets go of it: the audio bin is kept, unseen,
// while one of its pieces is on its way, which the other tabs are not.)
Item {
    id: control

    // The operator window
    required property var win
    // Which tab is showing: "timers", "props", "stage", "macros", "audio" or "messages"
    property string tab: "timers"
    // What a playlist dragged out of the audio bin's list of them is carried by
    required property Item audioPlaylistDrag
    // Whether something here is being renamed in place, and so has the keyboard
    readonly property bool renaming: props.renaming !== "" || macros.renaming !== "" || (audio.item !== null && audio.item.renaming !== "")
                                     || (messages.item !== null && messages.item.renaming !== "")
    readonly property var tabs: [
        { id: "timers", name: "Timers", picture: "Countdown" },
        { id: "props", name: "Props", picture: "Prop" },
        { id: "stage", name: "Stage", picture: "Stage" },
        { id: "macros", name: "Macros", picture: "Macro" },
        { id: "audio", name: "Audio", picture: "" },
        { id: "messages", name: "Messages", picture: "Message" }
    ]

    // Starts renaming a playlist or folder of the audio bin in place.
    function renameAudioPlaylist(id) {
        tab = "audio"
        if (audio.item)
            audio.item.renamePlaylist(id)
    }

    Row {
        id: buttons

        width: parent.width
        spacing: 4

        Repeater {
            model: control.tabs

            delegate: Rectangle {
                id: button

                required property var modelData
                readonly property bool chosen: control.tab === modelData.id
                readonly property color ink: chosen ? "white" : "#c9cdd6"

                width: (buttons.width - (control.tabs.length - 1) * buttons.spacing) / control.tabs.length
                height: 28
                radius: 6
                // The tab that is showing is blue.
                color: chosen ? "#1e88e5" : mouse.pressed ? "#50535a" : mouse.containsMouse ? "#45484e" : "#3a3c42"

                // ProPresenter's own picture for the tab (see ProIcon)
                ProIcon {
                    anchors.centerIn: parent
                    visible: button.modelData.picture !== ""
                    name: button.modelData.picture
                    ink: button.ink
                    size: 25
                }

                // The audio bin's, which is drawn here
                Loader {
                    anchors.centerIn: parent
                    active: button.modelData.id === "audio"

                    sourceComponent: AudioGlyph {
                        kind: "music"
                        ink: button.ink
                        size: 19
                    }
                }

                // A piece of audio dragged out of the audio bin, held over the tab,
                // brings the tab forward. (Let go here, it is dropped on nothing: the
                // drag is meant to go on to what the tab holds.)
                DropArea {
                    id: over

                    anchors.fill: parent
                    keys: ["action"]
                    enabled: control.win.audioDragged !== null
                }

                Timer {
                    interval: 550
                    running: over.containsDrag && !button.chosen
                    onTriggered: control.tab = button.modelData.id
                }

                MouseArea {
                    id: mouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        // Out of whatever box of the tab being left was being typed in
                        control.win.takeFocus()
                        control.tab = button.modelData.id
                    }
                }
            }
        }
    }

    // Adds to what the tab holds: a timer, a prop or a collection of them, a stage
    // layout, a macro or a collection of them. Small, and out of the way of the tabs.
    Rectangle {
        id: add

        objectName: "showControlAdd"
        z: 1
        anchors.right: parent.right
        anchors.top: buttons.bottom
        anchors.topMargin: 6
        width: 24
        height: 20
        radius: 5
        color: addMouse.pressed ? "#50535a" : addMouse.containsMouse ? "#45484e" : "#3a3c42"

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            color: "#e6e6e6"
            font.pixelSize: 15
            text: "+"
        }

        MouseArea {
            id: addMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                control.win.takeFocus()
                if (control.tab === "timers")
                    timers.add()
                else if (control.tab === "props")
                    props.showAddMenu(add)
                else if (control.tab === "macros")
                    macros.showAddMenu(add)
                else if (control.tab === "audio")
                    audio.item.showAddMenu(add)
                else if (control.tab === "messages")
                    messages.item.add()
                else
                    stage.add()
            }
        }
    }

    // What the tab holds, under the button that adds to it. (The audio bin and the
    // messages, which each have two things to find room for, start beside that button
    // and leave its corner free for it: see AudioPanel.)
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: control.tab === "audio" || control.tab === "messages" ? buttons.bottom : add.bottom
        anchors.topMargin: 6
        anchors.bottom: parent.bottom

        TimersPanel {
            id: timers

            anchors.fill: parent
            visible: control.tab === "timers"
            win: control.win
        }

        PropsPanel {
            id: props

            anchors.fill: parent
            visible: control.tab === "props"
            win: control.win
        }

        StagePanel {
            id: stage

            anchors.fill: parent
            visible: control.tab === "stage"
            win: control.win
        }

        MacrosPanel {
            id: macros

            anchors.fill: parent
            visible: control.tab === "macros"
            win: control.win
        }

        // (Made when its tab is first come to, and kept: nothing of it is made as the
        // app starts. While one of its pieces is being dragged it is kept there though
        // another tab has come forward, under that tab and not drawn: the row the
        // piece was dragged from is what carries it, and would let go if it were put
        // out of sight.)
        Loader {
            id: audio

            property bool wanted: false
            readonly property bool forward: control.tab === "audio"

            anchors.fill: parent
            visible: forward || control.win.audioDragged !== null
            opacity: forward ? 1 : 0
            z: forward ? 0 : -1
            active: wanted
            onForwardChanged: {
                if (forward)
                    wanted = true
            }
            Component.onCompleted: wanted = forward

            sourceComponent: AudioPanel {
                win: control.win
                playlistDrag: control.audioPlaylistDrag
                forward: audio.forward
            }
        }

        // (Made when its tab is first come to, and kept, as the audio bin is.)
        Loader {
            id: messages

            property bool wanted: false
            readonly property bool forward: control.tab === "messages"

            anchors.fill: parent
            visible: forward
            active: wanted
            onForwardChanged: {
                if (forward)
                    wanted = true
            }
            Component.onCompleted: wanted = forward

            sourceComponent: MessagesPanel {
                win: control.win
            }
        }
    }
}
