import QtQuick
import QtMultimedia

// The transport: where a video (or the music) has got to, with a slider that can be
// dragged to move it, and buttons to take it back to its start, to play or pause it,
// and to skip fifteen seconds back or on.
//
// It works the player itself, the output's own (see MediaContent) or the audio layer's
// (see AudioLayer.musicPlayer), so there is nothing here to keep in step. With nothing
// playing there is nothing to work, and it is greyed. Which player it is the tabs over
// it pick (see TransportTabs, in PreviewPanel).
//
// Media with playback markers has them drawn over the bar in their colours, as
// ProPresenter's transport has, each a small flag whose point is at the top of the
// slider's handle (MarkerFlag): there it covers neither the handle nor the bar, and a
// click on the slider anywhere else is the slider's, the flags being small and fine to
// be hard to hit. A flag is dragged along the bar to move its marker, kept where it is
// let go (the operator window's moveLiveMarker); the media, being live, is not moved
// with it. A right click has the Media Inspector's menu of the marker (Rename typed
// over the media's name here). Under the pointer or held, a flag shows its time and
// name. And a Marker menu at the right of the buttons lists them by time and name: one
// picked takes the media there and runs its actions (the operator window's
// showTransportMarkers).
//
// What it costs. A player says where it has got to twenty times a second, and anything
// drawn straight from that is drawn twenty times a second, with the whole operator
// window around it: measured, that was over half as much work again as playing the
// video. So the transport looks at the player a few times a second instead, which is as
// often as what it shows changes by anything that can be seen; and it looks when the
// preview above it is about to be drawn again for a new frame of the same video
// (`pulse`), so that the window is drawn once for both and the transport costs next to
// nothing.
Item {
    id: transport

    // The MediaPlayer playing the video on the output, or null
    property var player: null
    // What is on the layer it works, as the operator window has it, or null: for its
    // name and, where `badged`, whether it is a background or a foreground; and what it
    // says with nothing there
    property var media: null
    property bool badged: true
    property string nothing: "No media"
    // Its playback markers (see playbackmarkers.h), drawn on the bar; and the Marker menu
    // asked for, by its button (`item`), which the operator window puts up
    readonly property var markers: media !== null ? media.markers ?? [] : []
    // Whether they can be changed here: the media has a file they are kept in (see
    // Show.livePlace); otherwise they are drawn and no more
    property bool editable: false
    // A marker being dragged and where it is so far, in seconds ("" for none); and one
    // whose name is being typed
    property string draggedMarker: ""
    property real draggedTime: 0
    property string renaming: ""

    signal markersAsked(var item)
    // A marker dragged to `seconds`; one's menu asked for, on its flag `item` at a point
    // of it; and one renamed
    signal markerMoved(string id, real seconds)
    signal markerMenuAsked(var marker, var item, real x, real y)
    signal markerRenamed(string id, string name)
    // Something that says `relayed` whenever the window is about to be drawn again for
    // the video's sake anyway, or null: the preview's FrameRelay
    property var pulse: null
    // How long the video is, in milliseconds: nothing with no video, and until the
    // player has found out
    readonly property real duration: player !== null ? player.duration : 0
    readonly property bool working: duration > 0
    readonly property bool playing: player !== null && duration > 0 && player.playbackState === MediaPlayer.PlayingState
    // Where the video has got to, as last looked at, in milliseconds, and when that was
    property real position: 0
    property real looked: 0
    // How long to leave between looks while the video plays. A long video is looked at
    // less often than a short one: the slider's handle moves a pixel at a time at most,
    // and the times a second at a time.
    readonly property real patience: Math.max(250, Math.min(500, duration / Math.max(1, slider.width)))
    // What the slider is being dragged to while it is held, and otherwise that
    readonly property real shown: slider.pressed ? slider.value : position
    readonly property color textColor: "#e6e6e6"
    readonly property color dimTextColor: "#9a9da3"

    // Minutes and seconds, with hours if there are any.
    function clock(seconds) {
        const hours = Math.floor(seconds / 3600)
        const minutes = Math.floor(seconds / 60) % 60
        const rest = String(seconds % 60).padStart(2, "0")
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + rest : minutes + ":" + rest
    }

    // Where along the slider a marker's flag is: over where the middle of the handle is
    // at that time, so that a marker at the playhead has its flag right over the handle;
    // and the time at a point of the slider
    function markerX(seconds) {
        return slider.leftPadding + slider.handle.width / 2
               + Math.max(0, Math.min(1, seconds * 1000 / Math.max(1, duration))) * (slider.availableWidth - slider.handle.width)
    }

    function timeAt(x) {
        const along = (x - slider.leftPadding - slider.handle.width / 2) / Math.max(1, slider.availableWidth - slider.handle.width)
        return Math.max(0, Math.min(1, along)) * duration / 1000
    }

    // A marker's time to the tenth of a second
    function markerClock(seconds) {
        return clock(Math.floor(seconds)) + "." + Math.floor((seconds % 1) * 10)
    }

    function look() {
        looked = Date.now()
        position = player !== null && player.duration > 0 ? player.position : 0
    }

    function seek(milliseconds) {
        if (player === null || player.duration <= 0)
            return
        player.position = Math.max(0, Math.min(milliseconds, player.duration))
        look()
    }

    implicitHeight: column.height
    onPlayerChanged: look()
    onWorkingChanged: look()

    // While the video plays: with the preview's frames
    Connections {
        target: transport.pulse
        enabled: transport.playing

        function onRelayed() {
            if (Date.now() - transport.looked >= transport.patience)
                transport.look()
        }
    }

    // and, should no frames be coming to go by, once a second regardless.
    Timer {
        interval: 1000
        repeat: true
        running: transport.playing
        onTriggered: {
            if (Date.now() - transport.looked >= 900)
                transport.look()
        }
    }

    Connections {
        target: transport.player

        function onPlaybackStateChanged() {
            transport.look()
        }
    }

    // A video that is not playing only moves when it is moved, which is worth seeing
    // at once.
    Connections {
        target: transport.player
        enabled: !transport.playing

        function onPositionChanged() {
            transport.look()
        }
    }

    // A marker's name typed (Rename in its menu), over the media's name: Enter keeps it,
    // Esc gives it up. Made only while it is typed: the transport is made at start.
    Loader {
        x: 0
        y: 0
        z: 3
        width: parent.width
        height: 22
        active: transport.renaming !== ""

        sourceComponent: AppTextField {
            objectName: "transportMarkerName"
            font.pixelSize: 12
            Component.onCompleted: {
                const marker = transport.markers.find(m => m.id === transport.renaming)
                text = marker ? marker.name : ""
                forceActiveFocus()
                selectAll()
            }
            onAccepted: {
                const id = transport.renaming
                const name = text.trim()
                transport.renaming = ""
                transport.markerRenamed(id, name)
            }
            Keys.onEscapePressed: {
                transport.renaming = ""
                transport.markerRenamed("", "")
            }
        }
    }

    Column {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 4

        // What is playing, and how
        Row {
            width: parent.width
            height: 22
            spacing: 6

            MediaBadge {
                id: badge

                anchors.verticalCenter: parent.verticalCenter
                visible: transport.media !== null && transport.badged
                foreground: transport.media !== null && transport.media.foreground === true
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - (badge.visible ? badge.width + parent.spacing : 0)
                elide: Text.ElideMiddle
                color: transport.media !== null ? transport.textColor : transport.dimTextColor
                font.pixelSize: 12
                text: transport.media !== null ? transport.media.name : transport.nothing
            }
        }

        // How far in, the slider, and how much is left
        Row {
            width: parent.width
            height: 22
            spacing: 6

            Text {
                id: elapsed

                anchors.verticalCenter: parent.verticalCenter
                width: 40
                color: transport.working ? transport.textColor : transport.dimTextColor
                font.pixelSize: 12
                text: transport.clock(Math.floor(transport.shown / 1000))
            }

            AppSlider {
                id: slider

                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - elapsed.width - remaining.width - 2 * parent.spacing
                enabled: transport.working
                from: 0
                to: transport.working ? transport.duration : 1
                // The video is moved when the slider is let go, not all the way along a
                // drag: every move makes the player find its place in the file again,
                // which for a large video takes longer than a drag gives it.
                onPressedChanged: {
                    if (!pressed)
                        transport.seek(value)
                }

                // It follows the video, except while it is held.
                Binding on value {
                    when: !slider.pressed
                    value: transport.position
                }

                // The playback markers' flags (see the top of this file)
                Repeater {
                    model: transport.working ? transport.markers : []

                    delegate: MarkerFlag {
                        id: flag

                        required property var modelData
                        readonly property bool dragged: transport.draggedMarker === modelData.id

                        objectName: "transportMarkerMark"
                        x: transport.markerX(dragged ? transport.draggedTime : modelData.time) - width / 2
                        // (its point touching the top of the handle)
                        y: slider.handle.y - height + 1
                        z: dragged ? 2 : 1
                        width: 9
                        height: 10
                        color: modelData.color
                        lit: dragged || flagMouse.containsMouse

                        // Its time and name, while it is under the pointer or held
                        Rectangle {
                            objectName: "transportMarkerTag"
                            visible: flag.dragged || flagMouse.containsMouse
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: -height - 3
                            width: tagText.implicitWidth + 10
                            height: 18
                            radius: 4
                            color: "#15161a"
                            border.width: 1
                            border.color: "#5c5f66"

                            Text {
                                id: tagText

                                anchors.centerIn: parent
                                color: "#e6e6e6"
                                font.pixelSize: 11
                                text: transport.markerClock(flag.dragged ? transport.draggedTime : flag.modelData.time)
                                      + (flag.modelData.name !== "" ? "  " + flag.modelData.name : "")
                            }
                        }

                        MouseArea {
                            id: flagMouse

                            property real pressedAt: 0
                            // Whether this press has dragged the marker (and so is no click)
                            property bool moved: false

                            objectName: "transportMarkerHandle"
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: transport.editable ? Qt.LeftButton | Qt.RightButton : Qt.NoButton
                            cursorShape: !transport.editable ? Qt.ArrowCursor : flag.dragged ? Qt.ClosedHandCursor : Qt.SizeHorCursor
                            preventStealing: true
                            onPressed: (mouse) => {
                                pressedAt = mapToItem(slider, mouse.x, 0).x
                                moved = false
                            }
                            onPositionChanged: (mouse) => {
                                if (!pressed || !(pressedButtons & Qt.LeftButton))
                                    return
                                const along = mapToItem(slider, mouse.x, 0).x
                                if (!flag.dragged && Math.abs(along - pressedAt) < 3)
                                    return
                                transport.draggedTime = transport.timeAt(along)
                                transport.draggedMarker = flag.modelData.id
                            }
                            // Kept once the press is over: keeping it gives the transport
                            // the markers again, which makes this flag anew, so what is done
                            // then is done by the transport, which stays.
                            onReleased: (mouse) => {
                                if (mouse.button !== Qt.LeftButton || !flag.dragged)
                                    return
                                moved = true
                                const owner = transport
                                const id = flag.modelData.id
                                const seconds = owner.draggedTime
                                Qt.callLater(() => {
                                    owner.draggedMarker = ""
                                    owner.markerMoved(id, seconds)
                                })
                            }
                            onCanceled: transport.draggedMarker = ""
                            onClicked: (mouse) => {
                                if (!moved && mouse.button === Qt.RightButton)
                                    transport.markerMenuAsked(flag.modelData, flag, mouse.x, mouse.y)
                            }
                        }
                    }
                }
            }

            Text {
                id: remaining

                anchors.verticalCenter: parent.verticalCenter
                width: 44
                horizontalAlignment: Text.AlignRight
                color: transport.working ? transport.textColor : transport.dimTextColor
                font.pixelSize: 12
                // Whole seconds, so that the two times always add up to the length
                text: "-" + transport.clock(Math.max(0, Math.floor(transport.duration / 1000) - Math.floor(transport.shown / 1000)))
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            IconButton {
                kind: "restart"
                available: transport.working
                onClicked: {
                    transport.seek(0)
                    transport.player.play()
                }
            }

            IconButton {
                width: 38
                text: "−15"
                available: transport.working
                onClicked: transport.seek(transport.player.position - 15000)
            }

            IconButton {
                width: 44
                kind: transport.playing ? "pause" : "play"
                available: transport.working
                onClicked: transport.playing ? transport.player.pause() : transport.player.play()
            }

            IconButton {
                width: 38
                text: "+15"
                available: transport.working
                onClicked: transport.seek(transport.player.position + 15000)
            }

            // The Marker menu, for media that has markers: a marker's mark on a button
            IconButton {
                id: markerButton

                objectName: "transportMarkers"
                width: 30
                visible: transport.markers.length > 0
                available: transport.working
                onClicked: transport.markersAsked(markerButton)

                Rectangle {
                    anchors.centerIn: parent
                    width: 9
                    height: 9
                    rotation: 45
                    color: markerButton.ink
                }
            }
        }
    }
}
