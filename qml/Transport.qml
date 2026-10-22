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
// Media with playback markers has them drawn on the bar in their colours, as
// ProPresenter's transport has, and a Marker menu at the right of the buttons, which
// lists them by time and name: one picked takes the media there and runs its actions
// (the operator window's showTransportMarkers).
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

    signal markersAsked(var item)
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

                // The playback markers, each a mark of its colour on the bar, outlined
                // so as to be seen on the part played as well. (Moved in the Media
                // Inspector, where they are made, and not here, where the media is live.)
                Repeater {
                    model: transport.working ? transport.markers : []

                    delegate: Rectangle {
                        required property var modelData

                        objectName: "transportMarkerMark"
                        x: slider.leftPadding + Math.min(1, modelData.time * 1000 / transport.duration) * slider.availableWidth - 2.5
                        y: slider.height / 2 - 8
                        width: 5
                        height: 16
                        radius: 1.5
                        color: modelData.color
                        border.width: 1
                        border.color: "#15161a"
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
