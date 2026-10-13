import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Shapes
import SimplePresenterApp

// The audio bin, a tab of the show controls: the workspace's playlists of audio and the
// folders they are kept in at the top, and under them, with a line between that can be
// dragged to share out the height, the pieces of audio of the playlist picked there.
// It is ProPresenter's audio bin, kept in ProPresenter's own file of it
// (Playlists/Audio: see playlistfile.h, and audiocue.h for what a row of it says).
//
// Click a piece to play it. A piece is music or a sound effect, which the picture at
// its left says: two joined notes, or a jagged line. Music is one at a time, a piece of
// music taking the place of the one that is playing; sound effects play over whatever
// is playing and over each other. What is playing is marked, the music with how long
// it has left.
//
// At a piece's right is how it plays on from its end: it stops (a square), it goes
// round (an arrow going round), or the next of the playlist follows it (an arrowhead
// against a bar). A right click has those three, and Music and Sound Effect. The list
// stays where it was scrolled to when a piece is changed so, moved, added or taken out.
//
// A piece can be dragged: onto a slide, which gives the slide an action that plays it;
// onto the tab of the macros, which comes forward after a moment, and there onto a
// macro, or onto the strip along the bottom of them for a macro of its own; and onto
// another piece of the playlist, to move it there. Sound files dragged in from another application are
// added, where they are dropped. They are referred to where they are on disk, not
// copied.
//
// Along the bottom: a switch for crossing one piece of music into the next, a box that
// narrows the list to the names that have what is typed in it, and how long a fade is,
// as a slider and as a number (AudioLayer says what the two do).
//
// The two areas are the colours of the two areas of the lists at the window's left:
// the lighter for the playlists, the darker for what is in them.
//
// It has not much room: the show controls are what is left of the window's right side
// under the previews, the clears and the transport, some two hundred points on a laptop.
// So its rows are close, and the + that adds a playlist (which the show controls put
// under the tabs for every tab) sits in the corner of the playlists and not on a line
// of its own.
//
// GUESSES at ProPresenter, whose audio bin this is laid out after from a description
// and not from a look at it: what is at the bottom and in what order is as described
// by the person this was made for; what a fade there applies to, and how long its
// crossfade is, are not known (see AudioLayer).
Item {
    id: panel

    // The operator window
    required property var win
    // What a playlist dragged out of the list of them is carried by
    required property Item playlistDrag
    // Whether its tab is the one showing. (It is kept, unseen, while one of its pieces
    // is dragged to another tab, and nothing is to be dropped on it then.)
    property bool forward: true
    // The playlist or folder being renamed in place, by id; "" for none
    readonly property string renaming: playlists.editingPath
    // The pieces of the playlist being browsed, and those of them the box at the bottom
    // leaves
    readonly property var pieces: win.audioFiles
    readonly property var shown: win.audioFilter.trim() === "" ? pieces : Search.narrowed(pieces, win.audioFilter)
    // (The list is given them by hand, to stay where it is: see its show().)
    onShownChanged: pieceList.show()
    // How high the playlists are, the pieces having the rest: what is asked for, kept
    // within what leaves both something
    readonly property real listHeight: Math.max(36, Math.min(win.audioSplit, height - 118))
    // The music that is playing, if it is one of this playlist's, by its row's id; and
    // the sound effects that are, by theirs
    readonly property string playingMusic: win.liveMusic !== null && win.liveMusicPlaylistId === win.audioPlaylistId ? win.liveMusic.id : ""
    readonly property var playingSounds: win.liveSounds.map(sound => sound.id)

    function renamePlaylist(id) {
        playlists.editingPath = id
    }

    // The menu of the + under the tabs: a folder, a playlist, or files for the playlist
    // being browsed
    function showAddMenu(item) {
        win.showAudioAddMenu(item)
    }

    // A length of time as minutes and seconds
    function clock(seconds) {
        const whole = Math.max(0, Math.round(seconds))
        return Math.floor(whole / 60) + ":" + String(whole % 60).padStart(2, "0")
    }

    // ---- The playlists, on the lighter ground

    Rectangle {
        id: listArea

        objectName: "audioPlaylistArea"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: panel.listHeight
        radius: 6
        color: panel.win.surfaceColor

        SidebarList {
            id: playlists

            objectName: "audioPlaylistList"
            anchors.fill: parent
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            // (The top right corner is the place of the + that adds a playlist or a
            // folder, which the show controls put there for every tab.)
            anchors.rightMargin: 30
            baseIndent: 8
            model: panel.win.catalog.audioPlaylists
            selectedPath: panel.win.selectedAudioNode
            livePath: panel.win.liveMusic !== null ? panel.win.liveMusicPlaylistId : ""
            dragProxy: panel.playlistDrag
            // A playlist or folder can be moved by dropping it on another; sound files
            // from another application go onto a playlist.
            dropKeys: !panel.forward ? [] : panel.win.takesDrops ? ["audioPlaylist", "text/uri-list"] : ["audioPlaylist"]
            dropZone: (node, source, urls) => source !== null ? (node.folder ? "both" : "between")
                                             : !node.folder && panel.win.catalog.audioAmong(urls).length > 0 ? "onto" : ""
            onPicked: (entry) => {
                if (entry.folder)
                    panel.win.selectedAudioNode = entry.path
                else
                    panel.win.openAudioPlaylist(entry.path)
            }
            onMenuRequested: (entry, item) => panel.win.showAudioNodeMenu(entry, item)
            onRenamed: (entry, name) => panel.win.report(panel.win.catalog.renameAudioPlaylist(entry.path, name))
            onEditingEnded: panel.win.takeFocus()
            onDropped: (node, source, where, drop) => {
                if (source !== null)
                    panel.win.report(panel.win.catalog.moveAudioPlaylist(source.entry.path, node.path, where))
                else
                    panel.win.dropOnAudioPlaylist(node.path, drop)
            }
        }

        EmptyNote {
            anchors.centerIn: parent
            width: parent.width - 24
            visible: playlists.count === 0
            font.pixelSize: 12
            text: "No audio playlists. Add one with the + above."
        }
    }

    // The line between the two, which is dragged to share out the height
    Item {
        id: splitter

        objectName: "audioSplit"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: listArea.bottom
        height: 7

        Rectangle {
            anchors.centerIn: parent
            width: 36
            height: 3
            radius: 1.5
            color: splitMouse.pressed ? panel.win.accentColor : splitMouse.containsMouse ? "#8d9097" : "#50535a"
        }

        MouseArea {
            id: splitMouse

            property real startedAt
            property real startedWith

            anchors.fill: parent
            anchors.topMargin: -2
            anchors.bottomMargin: -2
            hoverEnabled: true
            cursorShape: Qt.SplitVCursor
            preventStealing: true
            onPressed: (mouse) => {
                startedAt = mapToItem(panel, mouse.x, mouse.y).y
                startedWith = panel.listHeight
            }
            onPositionChanged: (mouse) => {
                if (pressed)
                    panel.win.audioSplit = Math.max(36, Math.min(startedWith + mapToItem(panel, mouse.x, mouse.y).y - startedAt, panel.height - 118))
            }
            onReleased: panel.win.takeFocus()
        }
    }

    // ---- The pieces, on the darker ground

    Rectangle {
        id: pieceArea

        objectName: "audioPieceArea"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: splitter.bottom
        anchors.bottom: parent.bottom
        radius: 6
        color: panel.win.panelColor

        // How many there are, and the + that browses for sound files to add
        Item {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 26

            Text {
                objectName: "audioCount"
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                color: panel.win.dimTextColor
                font.pixelSize: 12
                text: panel.win.audioPlaylistId === "" ? "" : panel.win.counted(panel.pieces.length, "item", "items")
                      + (panel.shown.length !== panel.pieces.length ? ", " + panel.shown.length + " shown" : "")
            }

            Rectangle {
                id: addButton

                objectName: "addAudioButton"
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 20
                radius: 5
                opacity: panel.win.audioPlaylistId === "" ? 0.4 : 1
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
                    enabled: panel.win.audioPlaylistId !== ""
                    onClicked: {
                        panel.win.takeFocus()
                        panel.win.browseForAudio()
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#2f3136"
            }
        }

        // Sound files dropped among the pieces but on none of them go at the end.
        DropArea {
            id: endDrop

            anchors.fill: pieceList
            keys: ["text/uri-list"]
            enabled: panel.forward && panel.win.takesDrops && panel.win.audioPlaylistId !== ""
            onEntered: (drag) => panel.win.acceptAudioDrag(drag)
            onPositionChanged: (drag) => panel.win.acceptAudioDrag(drag)
            onDropped: (drop) => panel.win.dropOnAudioPlaylist(panel.win.audioPlaylistId, drop)
        }

        Rectangle {
            anchors.fill: pieceList
            visible: endDrop.containsDrag
            radius: 4
            color: "transparent"
            border.width: 2
            border.color: "#c0ffffff"
        }

        ListView {
            id: pieceList

            objectName: "audioList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.margins: 3
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2

            // Which playlist the rows are of, and what the box at the bottom narrowed it
            // to: while that is the same, the list stays where it was scrolled to.
            property string shownOf
            property bool given: false
            readonly property real rowHeight: 27

            // Gives the list its rows. They are given here, and not by the list being
            // told to follow `shown`, so that it can be put back where it was. A list
            // that is given other rows starts again at its top, and the rows are other
            // rows after every change to the playlist, which is read again whole: a
            // piece far down the list that was made a sound effect, or set to go
            // round, was out of sight the moment it was. So where the rows are still
            // those of the same playlist, narrowed the same way, the list is put back
            // where it was, or as near as it can be if it has got shorter. Another
            // playlist, or other words in the box, starts at the top.
            function show() {
                const of = panel.win.audioPlaylistId + "\n" + panel.win.audioFilter.trim()
                const scrolledTo = contentY
                model = panel.shown
                if (of === shownOf) {
                    const all = count > 0 ? count * (rowHeight + spacing) - spacing : 0
                    contentY = Math.max(0, Math.min(scrolledTo, all - height))
                }
                shownOf = of
                given = true
            }

            // (Once, if it has not been told of the rows by then: they are not made twice.)
            Component.onCompleted: {
                if (!given)
                    show()
            }

            ScrollBar.vertical: ScrollBar {}

            KineticWheel {}

            delegate: Rectangle {
                id: piece

                required property var modelData
                readonly property bool music: panel.playingMusic === modelData.id
                readonly property bool sounding: music || panel.playingSounds.indexOf(modelData.id) >= 0
                readonly property color ink: modelData.missing ? "#77797f" : sounding ? "white" : "#d6d8dc"

                objectName: "audioRow"
                width: pieceList.width
                height: pieceList.rowHeight
                radius: 5
                // What is playing is the orange of what is live everywhere else.
                color: sounding ? "#7a4a17" : pieceMouse.containsMouse ? "#33353a" : "transparent"
                border.width: sounding ? 1 : 0
                border.color: panel.win.accentColor

                // Music, or a sound effect
                AudioGlyph {
                    id: typeGlyph

                    objectName: "audioRowType"
                    x: 7
                    anchors.verticalCenter: parent.verticalCenter
                    kind: piece.modelData.effect ? "sound" : "music"
                    ink: piece.ink
                    size: 16
                }

                Text {
                    anchors.left: typeGlyph.right
                    anchors.leftMargin: 8
                    anchors.right: length.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    color: piece.ink
                    font.pixelSize: 13
                    text: piece.modelData.name
                }

                // How long it is; for the music that is playing, how long it has left;
                // and for a file that is not on this computer, that
                Text {
                    id: length

                    objectName: "audioRowLength"
                    anchors.right: playbackGlyph.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: piece.modelData.missing ? "#d07070" : piece.sounding ? "#ffd9a8" : panel.win.dimTextColor
                    font.pixelSize: 11
                    text: piece.modelData.missing ? "missing"
                        : piece.music && AudioLayer.musicDuration > 0 ? "-" + panel.clock(AudioLayer.musicDuration - AudioLayer.musicPosition)
                        : piece.modelData.duration > 0 ? panel.clock(piece.modelData.duration) : ""
                }

                // How it plays on from its end
                ActionGlyph {
                    id: playbackGlyph

                    objectName: "audioRowPlayback"
                    anchors.right: parent.right
                    anchors.rightMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    kind: piece.modelData.playback
                    ink: piece.modelData.missing ? "#77797f" : "#a9acb3"
                }

                // Another of the playlist's pieces dropped here is moved to this place,
                // before it from the top half and after it from the bottom; sound files
                // from another application are added at the same place.
                DropArea {
                    id: reorderDrop

                    property bool after: false
                    property bool files: false
                    // (A piece of this playlist, and with the whole of the playlist
                    // listed: a place among some of its pieces is no place in it.)
                    readonly property bool ours: panel.win.draggedAudio() !== null && panel.win.draggedAudio().playlist === panel.win.audioPlaylistId
                                                 && panel.win.audioFilter.trim() === ""
                    readonly property bool moving: containsDrag && (files || (ours && panel.win.draggedAudio().id !== piece.modelData.id))

                    function follow(drag) {
                        after = drag.y > height / 2
                        files = drag.source === null
                        if (files)
                            panel.win.acceptAudioDrag(drag)
                        else
                            drag.accepted = ours
                    }

                    anchors.fill: parent
                    keys: ["action", "text/uri-list"]
                    enabled: panel.forward && panel.win.takesDrops
                    onEntered: (drag) => follow(drag)
                    onPositionChanged: (drag) => follow(drag)
                    // One call and nothing after it: either way the list is rebuilt, and
                    // this row with it.
                    onDropped: (drop) => {
                        if (drop.source === null)
                            panel.win.dropOnAudioPlaylist(panel.win.audioPlaylistId, drop, piece.modelData.id, after)
                        else if (ours && panel.win.draggedAudio().id !== piece.modelData.id)
                            panel.win.moveAudio(panel.win.draggedAudio().id, piece.modelData.id, after)
                    }
                }

                Rectangle {
                    y: reorderDrop.after ? parent.height - 1 : -1
                    width: parent.width
                    height: 2
                    visible: reorderDrop.moving
                    color: "white"
                }

                // A click plays it; it can be dragged onto a slide or a macro, or onto
                // another piece; a right click has what can be changed of it.
                DragSource {
                    id: pieceMouse

                    anchors.fill: parent
                    win: panel.win
                    payload: piece.modelData.missing ? null
                           : ({ kind: "audio", id: piece.modelData.id, name: piece.modelData.name, picture: piece.modelData.effect ? "SoundEffect" : "",
                                playlist: panel.win.audioPlaylistId })
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (mouse) => {
                        if (dragged)
                            return
                        panel.win.takeFocus()
                        if (mouse.button === Qt.RightButton)
                            panel.win.showAudioItemMenu(piece.modelData, piece)
                        else if (!piece.modelData.missing)
                            panel.win.playAudio(piece.modelData, panel.win.audioPlaylistId)
                    }
                }
            }
        }

        EmptyNote {
            anchors.centerIn: pieceList
            width: pieceList.width - 30
            visible: panel.shown.length === 0
            font.pixelSize: 12
            text: panel.win.audioPlaylistId === "" ? "Pick a playlist above, or add one with the + over it."
                : panel.pieces.length > 0 ? "None of this playlist's audio has that in its name."
                : "This playlist is empty. Add audio with the + at its right, or drag files in."
        }

        // ---- Along the bottom
        Item {
            id: footer

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 32

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#2f3136"
            }

            // Crossing one piece of music into the next: on, the piece that takes over
            // comes in as the one playing goes out. Two lines that cross.
            Rectangle {
                id: crossfade

                objectName: "audioCrossfade"
                x: 5
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
                width: 30
                height: 24
                radius: 5
                color: panel.win.audioCrossfade ? "#1e88e5" : crossMouse.pressed ? "#50535a" : crossMouse.containsMouse ? "#45484e" : "#2b2d31"

                // One going out, the other coming in
                Shape {
                    anchors.centerIn: parent
                    width: 18
                    height: 12
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: panel.win.audioCrossfade ? "white" : "#a9acb3"
                        strokeWidth: 1.6
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap

                        PathSvg {
                            path: "M 1 1.5 C 8 1.5 10 10.5 17 10.5 M 1 10.5 C 8 10.5 10 1.5 17 1.5"
                        }
                    }
                }

                MouseArea {
                    id: crossMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        panel.win.takeFocus()
                        panel.win.audioCrossfade = !panel.win.audioCrossfade
                        Log.note("audio", "crossfade " + (panel.win.audioCrossfade ? "on" : "off"))
                    }
                }
            }

            // Narrows the list to the names that have what is typed. Esc empties it.
            AppTextField {
                id: filterField

                objectName: "audioFilter"
                anchors.left: crossfade.right
                anchors.leftMargin: 5
                anchors.right: fadeSlider.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
                height: 26
                topPadding: 0
                bottomPadding: 0
                font.pixelSize: 12
                rightPadding: 22
                placeholderText: "Filter"
                placeholderTextColor: "#7d8088"
                text: panel.win.audioFilter
                onTextEdited: panel.win.audioFilter = text
                onAccepted: panel.win.takeFocus()
                Keys.onEscapePressed: {
                    panel.win.audioFilter = ""
                    panel.win.takeFocus()
                }

                Text {
                    objectName: "audioFilterClear"
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    visible: panel.win.audioFilter !== ""
                    color: clearMouse.containsMouse ? "#e6e6e6" : "#9a9da3"
                    font.pixelSize: 15
                    text: "×"

                    MouseArea {
                        id: clearMouse

                        anchors.fill: parent
                        anchors.margins: -5
                        hoverEnabled: true
                        onClicked: {
                            panel.win.audioFilter = ""
                            panel.win.takeFocus()
                        }
                    }
                }
            }

            // How long a fade is: dragged, or typed
            AppSlider {
                id: fadeSlider

                objectName: "audioFadeSlider"
                anchors.right: fadeField.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
                width: Math.max(50, Math.min(96, footer.width * 0.26))
                from: 0
                to: 10
                stepSize: 0.1
                value: panel.win.audioFade
                onMoved: panel.win.audioFade = Math.round(value * 10) / 10
                onPressedChanged: {
                    if (!pressed)
                        panel.win.takeFocus()
                }
            }

            AppTextField {
                id: fadeField

                objectName: "audioFadeField"
                anchors.right: fadeUnit.left
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
                width: 40
                height: 26
                topPadding: 0
                bottomPadding: 0
                leftPadding: 4
                rightPadding: 4
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                text: panel.win.audioFade.toFixed(1)
                validator: DoubleValidator {
                    bottom: 0
                    top: 30
                    decimals: 1
                    notation: DoubleValidator.StandardNotation
                }
                onEditingFinished: {
                    const typed = Number(text.replace(",", "."))
                    if (isFinite(typed))
                        panel.win.audioFade = Math.max(0, Math.min(30, Math.round(typed * 10) / 10))
                    // (What was typed is written as the length is written, whether or
                    // not it was another length.)
                    text = Qt.binding(() => panel.win.audioFade.toFixed(1))
                }
                onAccepted: panel.win.takeFocus()
                Keys.onEscapePressed: {
                    text = Qt.binding(() => panel.win.audioFade.toFixed(1))
                    panel.win.takeFocus()
                }
            }

            Text {
                id: fadeUnit

                anchors.right: parent.right
                anchors.rightMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 1
                color: panel.win.dimTextColor
                font.pixelSize: 11
                text: "s"
            }
        }
    }
}
