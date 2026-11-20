import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import SimplePresenterApp

// The Timecode window (ProPresenter's View, Timecode: src/timecode.h has what its help
// says of it): the timecode coming in, in large figures; under it whether it is Playing
// or Stopped, a word that is clicked to "Not Engaged", when nothing is listened for and
// nothing is triggered, and clicked again to listen; the playlist it triggers, chosen
// here, with that playlist's presentations whose timelines follow timecode listed by
// the timecodes they start at, the one the timecode is in lit; and, behind the cog, its
// settings: the input device, the channel of it, the format, and an offset from the
// signal, with a speaker that plays the signal, quietly, while it is held.
//
// No picture of ProPresenter's window has been seen: it is laid out here from what its
// help says is in it, and how it looks is A GUESS throughout. So are the words on it
// that the help does not give ("Playlist", "None", the settings' names are the help's).
//
// This app's own, in it: a line under the settings that says what the signal looks
// like (how loud, and how many frames a second it is going at, which is what to set the
// format to); and, while the device chosen is the one made here, which is a timecode of
// the app's own to try things with (see Timecode), a row to play it, stop it, and send
// it to a timecode typed.
//
// What it costs: nothing until it is first opened: Main makes it then, from this file
// by its name, so that not even the file is read before. Open, the figures are asked
// for thirty times a second while timecode runs, and four while it does not; shut, not
// at all.
Window {
    id: timecodeWindow

    // The operator window: its catalog, its log, and the rows of the playlist chosen
    required property var owner

    property bool settingsShown: false
    // The playlists there are to choose from, None first
    readonly property var playlists: owner.catalog.playlists.filter(node => !node.folder)
    readonly property var rows: owner.timecodeRows
    // The row the timecode is in, by its key ("" for none): asked as the figures are
    readonly property string under: visible && Timecode.revision >= 0 && Timecode.running ? (Timecode.under(owner.timecodeFollowing).key ?? "") : ""

    function deviceIndex() {
        return Timecode.devices.findIndex(device => device.id === Timecode.device)
    }

    title: "Timecode"
    // Open while the operator window says so. Closed by its own button (or by the app
    // quitting), it is let close, and is put away: not refused, Qt asking every window to
    // close before it quits, and one that will not keeping the app running with no
    // windows. (Its `visible` keeps its binding, closing being no assignment.)
    visible: owner.timecodeOpen
    onClosing: owner.timecodeOpen = false
    width: remembered.width
    height: remembered.height
    minimumWidth: 380
    minimumHeight: 300
    color: "#1e1f23"
    flags: Qt.Window
    // In front of the operator window, as its other windows are
    transientParent: owner

    onWidthChanged: if (visible) remembered.width = width
    onHeightChanged: if (visible) remembered.height = height
    onVisibleChanged: Timecode.watched = visible && settingsShown
    onSettingsShownChanged: Timecode.watched = visible && settingsShown

    Settings {
        id: remembered

        category: "timecodeWindow"
        property real width: 420
        property real height: 460
    }

    // ---- The figures, the word under them, and the cog
    Item {
        id: head

        anchors.left: parent.left
        anchors.right: parent.right
        height: 96

        Text {
            objectName: "timecodeFigures"
            x: 20
            y: 12
            color: !Timecode.engaged ? "#6c6f75" : Timecode.running ? "#f0f0f0" : "#b9bcc3"
            font.pixelSize: 40
            font.family: "monospace"
            // (Reading the revision is what has this asked again as the timecode goes on,
            // and asking is what keeps it told: see Timecode.)
            text: timecodeWindow.visible && Timecode.revision >= 0 ? Timecode.text() : ""
        }

        // ProPresenter's word for how it is, which is clicked to stop listening, and to
        // listen again
        Rectangle {
            id: statusWord

            objectName: "timecodeStatus"
            x: 20
            y: 62
            width: statusText.implicitWidth + 20
            height: 24
            radius: 12
            color: !Timecode.engaged ? "#3a3c42" : Timecode.running ? "#1f6f3a" : "#45484e"
            border.width: 1
            border.color: statusMouse.containsMouse ? "#8b8f98" : "transparent"

            Text {
                id: statusText

                anchors.centerIn: parent
                color: Timecode.engaged ? "#f0f0f0" : "#c9ccd2"
                font.pixelSize: 13
                text: Timecode.status
            }

            MouseArea {
                id: statusMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    Timecode.engaged = !Timecode.engaged
                    Log.note("timecode", Timecode.engaged ? "listening, from the Timecode window" : "set to Not Engaged, from the Timecode window")
                }
            }
        }

        Text {
            anchors.left: statusWord.right
            anchors.leftMargin: 10
            anchors.right: cog.left
            anchors.rightMargin: 8
            anchors.verticalCenter: statusWord.verticalCenter
            elide: Text.ElideRight
            color: Timecode.problem !== "" ? "#ff9a8a" : "#8d9097"
            font.pixelSize: 12
            text: Timecode.problem !== "" ? Timecode.problem
                : !Timecode.engaged ? "Click to listen for timecode"
                : Timecode.device === "" ? "" : "Click to stop listening"
        }

        // The cog: the settings, under the figures
        Rectangle {
            id: cog

            objectName: "timecodeCog"
            anchors.right: parent.right
            anchors.rightMargin: 14
            y: 18
            width: 32
            height: 28
            radius: 6
            color: timecodeWindow.settingsShown ? "#4da3ff" : cogMouse.pressed ? "#50535a" : cogMouse.containsMouse ? "#45484e" : "#3a3c42"

            Text {
                anchors.centerIn: parent
                color: timecodeWindow.settingsShown ? "black" : "#e6e6e6"
                font.pixelSize: 17
                text: "⚙"
            }

            MouseArea {
                id: cogMouse

                anchors.fill: parent
                hoverEnabled: true
                onClicked: timecodeWindow.settingsShown = !timecodeWindow.settingsShown
            }
        }
    }

    // ---- The settings, shown by the cog
    Rectangle {
        id: settingsPane

        objectName: "timecodeSettings"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: head.bottom
        visible: timecodeWindow.settingsShown
        height: visible ? settingsColumn.implicitHeight + 24 : 0
        color: "#26272c"

        component Named: Text {
            width: 96
            height: 30
            verticalAlignment: Text.AlignVCenter
            color: "#c9ccd2"
            font.pixelSize: 13
        }

        Column {
            id: settingsColumn

            x: 20
            y: 12
            width: parent.width - 40
            spacing: 6

            Row {
                spacing: 8

                Named {
                    text: "Input Device"
                }

                AppComboBox {
                    objectName: "timecodeDevice"
                    width: settingsColumn.width - 104
                    height: 30
                    model: Timecode.devices.map(device => device.name)
                    currentIndex: timecodeWindow.deviceIndex()
                    displayText: currentIndex >= 0 ? currentText : Timecode.device === "" ? "None chosen" : "Not connected"
                    onActivated: (index) => {
                        Timecode.device = Timecode.devices[index].id
                        Log.note("timecode", "the input device is " + Timecode.devices[index].name)
                    }
                }
            }

            Row {
                spacing: 8
                visible: !Timecode.madeHere

                Named {
                    text: "Channel"
                }

                AppComboBox {
                    objectName: "timecodeChannel"
                    width: 90
                    height: 30
                    model: Array.from({ length: Math.max(1, Timecode.channels) }, (unused, i) => String(i + 1))
                    currentIndex: Timecode.channel
                    onActivated: (index) => Timecode.channel = index
                }
            }

            Row {
                spacing: 8

                Named {
                    text: "Format"
                }

                AppComboBox {
                    objectName: "timecodeFormat"
                    width: 120
                    height: 30
                    model: Timecode.formats.map(format => format.name)
                    currentIndex: Timecode.formats.findIndex(format => format.value === Timecode.format)
                    onActivated: (index) => {
                        Timecode.format = Timecode.formats[index].value
                        Log.note("timecode", "the format is " + Timecode.formats[index].name)
                    }
                }
            }

            Row {
                spacing: 8

                Named {
                    text: "Offset"
                }

                NumberField {
                    objectName: "timecodeOffset"
                    width: 90
                    height: 30
                    value: Timecode.offset
                    decimals: 2
                    step: 0.01
                    from: -86400
                    to: 86400
                    suffix: " s"
                    onEdited: (value) => {
                        Timecode.offset = value
                        Log.note("timecode", "the offset is " + value + " s")
                    }
                }

                Text {
                    height: 30
                    verticalAlignment: Text.AlignVCenter
                    color: "#8d9097"
                    font.pixelSize: 12
                    text: "added to the incoming timecode"
                }
            }

            // The signal: the speaker, held to hear it; how loud it is; and what it is
            // going at (this app's own, but for the speaker)
            Row {
                spacing: 8
                visible: !Timecode.madeHere

                Named {
                    text: "Signal"
                }

                Rectangle {
                    objectName: "timecodeSpeaker"
                    width: 34
                    height: 28
                    radius: 6
                    color: speakerMouse.pressed ? "#4da3ff" : speakerMouse.containsMouse ? "#45484e" : "#3a3c42"

                    SpeakerGlyph {
                        anchors.centerIn: parent
                        muted: false
                    }

                    MouseArea {
                        id: speakerMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onPressedChanged: Timecode.audible = pressed
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 90
                    height: 8
                    radius: 4
                    color: "#15161a"

                    Rectangle {
                        objectName: "timecodeLevel"
                        width: parent.width * Math.min(1, Timecode.level)
                        height: parent.height
                        radius: 4
                        color: Timecode.running ? "#5ab53b" : "#8b8f98"
                    }
                }

                Text {
                    height: 28
                    verticalAlignment: Text.AlignVCenter
                    color: Timecode.measured !== "" && Timecode.formats[Timecode.formats.findIndex(format => format.value === Timecode.format)].name !== Timecode.measured
                           ? "#ffb35c" : "#8d9097"
                    font.pixelSize: 12
                    text: !Timecode.engaged ? "not listening" : Timecode.level < 0.01 ? "no signal"
                        : Timecode.measured !== "" ? "timecode at " + Timecode.measured : "a signal that is not timecode"
                }
            }
        }
    }

    // ---- The one made here: played, stopped, and sent somewhere
    Rectangle {
        id: madeHereRow

        objectName: "timecodeMadeHere"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: settingsPane.bottom
        visible: Timecode.madeHere
        height: visible ? 44 : 0
        color: "#222327"

        Row {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            AppButton {
                objectName: "timecodePlay"
                height: 28
                text: "Play"
                enabled: Timecode.engaged
                onClicked: Timecode.play()
            }

            AppButton {
                objectName: "timecodeStop"
                height: 28
                text: "Stop"
                enabled: Timecode.engaged
                onClicked: Timecode.stop()
            }

            Text {
                height: 28
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
                color: "#c9ccd2"
                font.pixelSize: 13
                text: "Go to"
            }

            AppTextField {
                id: goTo

                objectName: "timecodeGoTo"
                width: 120
                height: 28
                font.pixelSize: 13
                font.family: "monospace"
                placeholderText: "00:00:00;00"
                onAccepted: {
                    const seconds = Timecode.typed(text)
                    if (seconds < 0) {
                        timecodeWindow.owner.report("“" + text + "” is not a timecode: hours, minutes, seconds and frames, as 01:00:00;00")
                        return
                    }
                    Timecode.jump(seconds)
                    Log.note("timecode", "the timecode made here sent to " + Timecode.faceText(seconds))
                    text = Timecode.faceText(seconds)
                }
            }
        }
    }

    // ---- The playlist it triggers, and that playlist's presentations
    Item {
        id: playlistRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: madeHereRow.bottom
        height: 46

        Text {
            x: 20
            anchors.verticalCenter: parent.verticalCenter
            color: "#c9ccd2"
            font.pixelSize: 13
            text: "Playlist"
        }

        AppComboBox {
            objectName: "timecodePlaylist"
            x: 84
            width: parent.width - 104
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            model: ["None"].concat(timecodeWindow.playlists.map(node => node.name))
            currentIndex: timecodeWindow.playlists.findIndex(node => node.path === Timecode.playlist) + 1
            onActivated: (index) => {
                Timecode.playlist = index === 0 ? "" : timecodeWindow.playlists[index - 1].path
                Log.note("timecode", index === 0 ? "no playlist is triggered by timecode" : "the playlist " + timecodeWindow.playlists[index - 1].name + " is triggered by timecode")
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: playlistRow.bottom
        anchors.bottom: parent.bottom
        color: "#17181b"

        ListView {
            id: list

            objectName: "timecodeRows"
            anchors.fill: parent
            anchors.topMargin: 4
            clip: true
            model: timecodeWindow.rows
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: row

                required property var modelData

                readonly property bool lit: modelData.key === timecodeWindow.under

                width: ListView.view.width
                height: 30
                color: lit ? "#1f5fa8" : "transparent"

                Text {
                    x: 20
                    width: 130
                    anchors.verticalCenter: parent.verticalCenter
                    color: row.modelData.follows ? "#f0f0f0" : "#6c6f75"
                    font.pixelSize: 13
                    font.family: "monospace"
                    // (Read with the format, which is what says how many frames a second has)
                    text: row.modelData.follows && Timecode.format >= 0 ? Timecode.faceText(row.modelData.start) : "—"
                }

                Text {
                    x: 150
                    width: parent.width - 170
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    color: row.modelData.follows ? "#f0f0f0" : "#8d9097"
                    font.pixelSize: 13
                    text: row.modelData.name
                }
            }
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 40
            visible: list.count === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#8d9097"
            font.pixelSize: 13
            text: Timecode.playlist === "" ? "Choose the playlist that timecode is to trigger."
                                           : "This playlist has no presentations."
        }
    }
}
