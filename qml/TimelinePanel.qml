import QtQuick
import SimplePresenterApp

// The timeline of the presentation being viewed (ProPresenter's: see src/timeline.h),
// under the line over the slides, which its Timeline button opens and shuts (see
// SlideGrid.qml).
//
// It is laid out as ProPresenter's is, as the maintainer's picture of it (ProPresenter
// 22, 2026-11-14) has it:
//   - a ruler of minutes and seconds across the top, with the playhead, a white line
//     down through it and the tracks;
//   - four tracks, each with its picture at the left: the slides, as blocks from each
//     slide's time to the next, in the colour of the slide's group, the group's name
//     on the first of a run of it and each cue's name under that; the media, and the
//     audio, as bars as long as each piece is, named; and the actions, a block each;
//   - under them, Rewind, Play (Pause while it plays), Record and Loop at the left, and
//     at the right the Duration, typed, and a slider that spreads the time out or
//     draws it in.
// ProPresenter has its Timecode beside the Duration, which is to come (the maintainer,
// 2026-11-14): it is not here until it does something.
//
// What is done with it, as ProPresenter's help has it where it says, and as this app
// chooses where it does not (A GUESS each, said where they are done):
//   - a press on the ruler or on a track where it has nothing moves the playhead there,
//     and dragging along drags it; played, the timeline goes from there;
//   - a slide of the presentation is carried up from the slides, a picture or a video
//     dragged out of the media bin, or a piece of the audio bin, and let go on the
//     timeline, where it goes on its own track at that time (Main's timelineCarried and
//     dropOnTimeline); while it is carried that track is outlined, and while it is over
//     the timeline, where it would land is drawn, with the time on the ruler; held near
//     either end of what is in view, the view scrolls on, for a time out of sight;
//   - a cue is dragged along its track to another time; a click on it puts the
//     playhead at it; its own menu (the right button) takes it off;
//   - the right button on the actions track adds an action there, as a slide's are
//     added; on the media track and the audio track, a picture or video, or a piece of
//     audio, of the bins;
//   - with Record on, what is triggered by hand is put on the timeline where the
//     playhead is (TimelinePlayer), and shown at once, a little paler, until Record
//     is turned off and it is written to the file.
//
// What it shows is the timeline the player has, where the player has this
// presentation's (so that what is being recorded shows); otherwise the file's, with a
// playhead of its own (Main's timelineCursor) that Play starts from.
//
// What it costs. Nothing until the button is first pressed (it is behind a Loader).
// Open, the cues are laid out when the timeline changes, and the playhead moves when the
// player says where it is: ten times a second while it plays (see TimelinePlayer), and
// nothing is drawn between. The scrolling while what is carried is held near an end goes
// by a FrameClock, a frame at each refresh, and only while it is held there.
Item {
    id: area

    // The operator window
    required property var win
    readonly property var player: win.timelineHere
    readonly property bool mine: win.timelineMine
    readonly property var timeline: (mine ? player.timeline : win.document?.timeline) ?? ({})
    readonly property real duration: Number(timeline.duration ?? 300)
    readonly property var cues: timeline.cues ?? []
    readonly property real position: mine ? player.position : win.timelineCursor
    readonly property bool playing: mine && player.playing
    readonly property bool recording: mine && player.recording

    // Pixels a second, which the slider at the right sets (ProPresenter's starts at
    // about eighteen, as its picture has it)
    readonly property real pps: win.timelineZoom
    readonly property real rulerHeight: 22
    readonly property var tracks: [
        { key: "slide", height: 40 },
        { key: "media", height: 24 },
        { key: "audio", height: 24 },
        { key: "action", height: 24 }
    ]
    readonly property real tracksHeight: 112
    readonly property real iconsWidth: 34
    readonly property real controlsHeight: 30

    // What is being carried that the timeline takes, anywhere in the window (Main's
    // timelineCarried: its track, its length, a slide's colour), or null; where its track
    // is, down from the top of the tracks; and the colour its block would be, as the track
    // draws one (a slide's group's; the media's purple; the audio's green)
    readonly property var carried: win.timelineCarried
    readonly property var carriedSpan: trackSpan(carried ? carried.track : "slide")
    readonly property color carriedFill: !carried ? "#5a5d66" : carried.track === "slide" ? carried.fill
                                         : carried.track === "media" ? "#8a6bb3" : "#96cc9e"

    function trackSpan(key) {
        let top = 0
        for (const track of tracks) {
            if (track.key === key)
                return { top: top, height: track.height }
            top += track.height
        }
        return { top: 0, height: 0 }
    }

    height: rulerHeight + tracksHeight + controlsHeight + 8

    function x0(seconds) {
        return seconds * pps
    }

    // A time as the ruler says it: minutes and seconds, and hours where there are any
    function clock(seconds, tenths) {
        const whole = Math.max(0, seconds)
        const h = Math.floor(whole / 3600)
        const m = Math.floor(whole / 60) % 60
        const s = whole % 60
        const ss = tenths ? (s < 10 ? "0" : "") + s.toFixed(1) : (Math.floor(s) < 10 ? "0" : "") + Math.floor(s)
        return (h > 0 ? h + ":" + (m < 10 ? "0" : "") + m : (m < 10 ? "0" : "") + m) + ":" + ss
    }

    // The slides' blocks: each slide cue, by time, from its time to the next slide
    // cue's (or the end), and whether its group's name is said on it (the first of a run
    // of the same group, as ProPresenter's picture has it)
    readonly property var slideBlocks: {
        const slides = cues.filter(cue => cue.track === "slide")
                           .map((cue, order) => ({ cue: cue, order: order }))
        slides.sort((a, b) => a.cue.time - b.cue.time || a.order - b.order)
        return slides.map((entry, i) => {
            const next = slides[i + 1]
            const before = slides[i - 1]
            return {
                cue: entry.cue,
                start: entry.cue.time,
                end: next ? next.cue.time : Math.max(entry.cue.time, duration),
                named: !before || before.cue.group !== entry.cue.group || before.cue.group === ""
            }
        })
    }
    // The rest, a list for each track
    function onTrack(key) {
        return cues.filter(cue => cue.track === key)
    }

    // The ruler's marks: a label every so many seconds, at least seventy pixels apart,
    // and small marks between, at least twelve apart; and no more than fifteen hundred
    // of those, for a long timeline spread out (each is an item, made as the timeline
    // is opened or its length or spread changes)
    readonly property real major: [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600].find(step => step * pps >= 70) ?? 3600
    readonly property real minor: [major / 10, major / 5, major / 2, major].find(step => step * pps >= 12 && duration / step <= 1500) ?? major

    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: 2
        radius: 6
        color: "#161719"
        border.color: "#2c2e33"
    }

    // The tracks' pictures, at the left
    Column {
        x: 4
        y: area.rulerHeight

        Repeater {
            model: area.tracks

            delegate: Item {
                required property var modelData

                width: area.iconsWidth - 6
                height: modelData.height

                ProIcon {
                    anchors.centerIn: parent
                    visible: parent.modelData.key === "slide"
                    name: "Presentation"
                    size: 22
                    ink: "#c9ccd2"
                }

                ActionGlyph {
                    anchors.centerIn: parent
                    visible: parent.modelData.key === "media" || parent.modelData.key === "audio"
                    kind: parent.modelData.key === "media" ? "media" : "music"
                    ink: "#c9ccd2"
                }

                // A cog, for the actions (as ProPresenter has; its pack has no picture of
                // one, so it is drawn here: a ring with eight teeth)
                Item {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    visible: parent.modelData.key === "action"

                    Repeater {
                        model: 4

                        delegate: Rectangle {
                            required property int index

                            x: 5
                            y: 0
                            width: 4
                            height: 14
                            color: "#c9ccd2"
                            rotation: index * 45
                            transformOrigin: Item.Center
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 10
                        radius: 5
                        color: "#c9ccd2"
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 5
                        height: 5
                        radius: 2.5
                        color: "#161719"
                    }
                }
            }
        }
    }

    // The ruler and the tracks, which scroll sideways together
    Flickable {
        id: view

        objectName: "timelineView"
        x: area.iconsWidth
        y: 0
        width: parent.width - area.iconsWidth - 6
        height: area.rulerHeight + area.tracksHeight
        contentWidth: area.x0(area.duration) + 40
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: false
        clip: true

        // Kept in sight while it plays: when the playhead nears the right, the view
        // moves on a page, as a score is turned
        Connections {
            // Where the playhead was when last looked at here
            property real was: -1

            target: area

            function onPositionChanged() {
                // (The player says where it is again when the timeline is read again after
                // a change, without its having moved: the view is left where it is then, at
                // a slide just dropped far along, say.)
                if (area.position === was)
                    return
                was = area.position
                const at = area.x0(area.position)
                if (at > view.contentX + view.width - 30 || at < view.contentX)
                    view.contentX = Math.max(0, Math.min(view.contentWidth - view.width, at - 30))
            }
        }

        // The wheel scrolls along it
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                const by = event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y
                view.contentX = Math.max(0, Math.min(Math.max(0, view.contentWidth - view.width), view.contentX - by))
            }
        }

        // Where a press on the ruler or an empty place moves the playhead to; dragging
        // along drags it
        MouseArea {
            objectName: "timelineScrub"
            width: view.contentWidth
            height: view.height
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    const track = area.trackAt(mouse.y)
                    if (track !== "")
                        area.win.showTimelineTrackMenu(track, Math.max(0, Math.min(area.duration, mouse.x / area.pps)), this, mouse.x, mouse.y)
                    return
                }
                area.win.seekTimeline(mouse.x / area.pps)
            }
            onPositionChanged: (mouse) => {
                if (pressedButtons & Qt.LeftButton)
                    area.win.seekTimeline(Math.max(0, Math.min(area.duration, mouse.x / area.pps)))
            }
        }

        // The ruler
        Item {
            width: view.contentWidth
            height: area.rulerHeight

            Repeater {
                model: Math.min(1500, Math.floor(area.duration / area.minor) + 1)

                delegate: Rectangle {
                    required property int index

                    x: Math.round(area.x0(index * area.minor))
                    y: 14
                    width: 1
                    height: 6
                    color: "#5a5d63"
                }
            }

            // The labelled marks, over the small ones
            Repeater {
                model: Math.floor(area.duration / area.major) + 1

                delegate: Rectangle {
                    required property int index

                    x: Math.round(area.x0(index * area.major))
                    y: 4
                    width: 1
                    height: area.rulerHeight - 4
                    color: "#8d9096"

                    Text {
                        x: 4
                        y: -1
                        color: "#a9acb2"
                        font.pixelSize: 10
                        text: area.clock(parent.index * area.major, false)
                    }
                }
            }

            // The end of the timeline
            Rectangle {
                x: Math.round(area.x0(area.duration))
                width: 2
                height: parent.height + area.tracksHeight
                color: "#5a5d63"
            }
        }

        // The tracks' grounds
        Column {
            y: area.rulerHeight
            width: area.x0(area.duration)

            Repeater {
                model: area.tracks

                delegate: Rectangle {
                    required property var modelData

                    width: parent.width
                    height: modelData.height
                    // (ProPresenter's media track is a dark purple where it is empty, as in
                    // its picture; the others are as dark as the panel)
                    color: modelData.key === "media" ? "#3a3140" : modelData.key === "action" ? "transparent" : "#1d1e21"
                    border.width: 0

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#0e0e10"
                    }
                }
            }
        }

        // The slides
        Repeater {
            model: area.slideBlocks

            delegate: TimelineCueBlock {
                required property var modelData

                panel: area
                cue: modelData.cue
                y: area.rulerHeight
                height: 40
                length: modelData.end - modelData.start
                fill: area.win.groupColor(modelData.cue)
                ink: "white"
                upper: modelData.named ? modelData.cue.group : ""
                lower: modelData.cue.name
            }
        }

        // The media, the audio and the actions
        Repeater {
            model: area.onTrack("media")

            delegate: TimelineCueBlock {
                required property var modelData

                panel: area
                cue: modelData
                y: area.rulerHeight + 40
                height: 24
                length: modelData.length > 0 ? modelData.length : 10
                fill: "#8a6bb3"
                ink: "white"
                lower: modelData.name
            }
        }

        Repeater {
            model: area.onTrack("audio")

            delegate: TimelineCueBlock {
                required property var modelData

                panel: area
                cue: modelData
                y: area.rulerHeight + 64
                height: 24
                length: modelData.length > 0 ? modelData.length : 10
                fill: "#96cc9e"
                ink: "#17261a"
                lower: modelData.name
            }
        }

        Repeater {
            model: area.onTrack("action")

            delegate: TimelineCueBlock {
                required property var modelData

                panel: area
                cue: modelData
                y: area.rulerHeight + 88
                height: 24
                length: Math.max(1, 90 / area.pps)
                fill: "#4b5563"
                ink: "white"
                lower: modelData.name !== "" ? modelData.name : (modelData.action?.label ?? "")
                glyph: modelData.action?.kind ?? ""
                picture: modelData.action?.picture ?? ""
            }
        }

        // While something the timeline takes is being carried, from anywhere, the track it
        // would go on is outlined, for where it can go
        Rectangle {
            objectName: "timelineTrackOutline"
            y: area.rulerHeight + area.carriedSpan.top
            width: area.x0(area.duration)
            height: area.carriedSpan.height
            visible: area.carried !== null
            color: "transparent"
            border.width: 1.5
            border.color: area.win.accentColor
        }

        // Where what is carried over the timeline would land: a white line at the time,
        // across the ruler and down to its track, and the block it would be there, paler
        // (a slide's as far as the next slide's; a picture's, a video's or a piece of
        // audio's as long as it lasts, or ten seconds where that is not known, as the track
        // draws it); and the time, on the ruler
        Item {
            objectName: "timelineLanding"
            readonly property real end: {
                if (area.carried === null)
                    return 0
                if (area.carried.track !== "slide")
                    return carriedDrop.landing + (area.carried.length > 0 ? area.carried.length : 10)
                const later = area.slideBlocks.find(block => block.start > carriedDrop.landing)
                return later ? later.start : area.duration
            }

            x: Math.round(area.x0(carriedDrop.landing))
            width: Math.max(4, Math.round(area.x0(end)) - x)
            height: view.height
            visible: carriedDrop.containsDrag && area.carried !== null

            Rectangle {
                y: area.rulerHeight + area.carriedSpan.top
                width: parent.width
                height: area.carriedSpan.height
                color: area.carriedFill
                opacity: 0.55
            }

            Rectangle {
                width: 2
                height: area.rulerHeight + area.carriedSpan.top + area.carriedSpan.height
                color: "white"
            }

            Rectangle {
                x: 4
                y: 2
                width: landingTime.implicitWidth + 8
                height: area.rulerHeight - 4
                radius: 3
                color: "#2b2d31"
                border.color: "white"

                Text {
                    id: landingTime

                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: 10
                    font.family: "monospace"
                    text: area.clock(carriedDrop.landing, true)
                }
            }
        }

        // The playhead
        Rectangle {
            objectName: "timelinePlayhead"
            x: Math.round(area.x0(area.position))
            width: 1
            height: view.height
            color: "white"

            Rectangle {
                x: -4
                y: 0
                width: 9
                height: 6
                color: "white"
                radius: 1
            }
        }
    }

    // What is carried onto the timeline (Main's timelineCarried) and let go there goes on
    // its own track at the time where it is let go, on whichever track or the ruler that
    // is: they all say a time, and each thing has one track. A slide carried up from the
    // slides (Main's beginSlideDrag) goes on the slides track; a picture or a video
    // dragged out of the media bin, on the media track; a piece of the audio bin, on the
    // audio track. Anything else carried (a macro, a clear button) is not taken. (A GUESS:
    // ProPresenter's help says that slides, media and audio are dragged onto the
    // timeline, and not what a drop on another of its tracks does.)
    DropArea {
        id: carriedDrop

        // Where the pointer is across what is in view, and the time it would land at
        property real pointerX: 0
        property real landing: 0
        // Held within this of either end of what is in view, the view scrolls on that
        // way, the faster the nearer the end, to this many pixels a second at it
        readonly property real edge: 40
        readonly property real fastest: 800
        readonly property real furthest: Math.max(0, view.contentWidth - view.width)
        readonly property real scrolling: !containsDrag || area.carried === null ? 0
                                          : pointerX < edge && view.contentX > 0 ? -fastest * (edge - Math.max(0, pointerX)) / edge
                                          : pointerX > width - edge && view.contentX < furthest ? fastest * (Math.min(width, pointerX) - (width - edge)) / edge
                                          : 0

        function aim() {
            landing = Math.max(0, Math.min(area.duration, (pointerX + view.contentX) / area.pps))
        }

        function follow(drag) {
            pointerX = drag.x
            aim()
            drag.accepted = area.win.timelineTakes(drag.source)
        }

        objectName: "timelineDrop"
        x: view.x
        width: view.width
        height: view.height
        // (A slide; media; and the drags that stand for actions, of which only a piece
        // of the audio bin is taken: see follow)
        keys: ["slide", "media", "action"]
        enabled: area.win.takesDrops
        onEntered: (drag) => follow(drag)
        onPositionChanged: (drag) => follow(drag)
        onDropped: (drop) => area.win.dropOnTimeline(drop.source, landing)
    }

    // The scrolling while what is carried is held near an end: by the time that has
    // passed between frames, so that it goes at one speed however often they come; the
    // time under the pointer, which has not moved, is worked out again at each
    FrameClock {
        property real last: 0

        running: carriedDrop.scrolling !== 0
        onRunningChanged: last = 0
        onTicked: (now) => {
            if (last > 0) {
                view.contentX = Math.max(0, Math.min(carriedDrop.furthest, view.contentX + carriedDrop.scrolling * Math.min(100, now - last) / 1000))
                carriedDrop.aim()
            }
            last = now
        }
    }

    function trackAt(y) {
        let top = rulerHeight
        for (const track of tracks) {
            if (y >= top && y < top + track.height)
                return track.key
            top += track.height
        }
        return ""
    }

    // ---- The controls under the tracks
    Item {
        id: controls

        x: 8
        y: area.rulerHeight + area.tracksHeight + 4
        width: parent.width - 16
        height: area.controlsHeight

        Row {
            id: transport

            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            IconButton {
                objectName: "timelineRewind"
                kind: "restart"
                width: 30
                height: 24
                onClicked: area.win.rewindTimeline()
            }

            IconButton {
                objectName: "timelinePlay"
                kind: area.playing ? "pause" : "play"
                width: 30
                height: 24
                onClicked: area.playing ? area.win.pauseTimeline() : area.win.playTimeline()
            }

            // Record: a red dot, on a lit ground while it is on
            Rectangle {
                objectName: "timelineRecord"
                width: 30
                height: 24
                radius: 5
                color: area.recording ? "#5c1d1d" : recordMouse.pressed ? "#50535a" : recordMouse.containsMouse ? "#45484e" : "#3a3c42"
                border.color: area.recording ? "#ff4d4d" : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    color: "#ff4d4d"
                }

                MouseArea {
                    id: recordMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: area.win.toggleTimelineRecording()
                }
            }

            // Loop
            Rectangle {
                objectName: "timelineLoop"
                readonly property bool on: area.timeline.loop === true

                width: 30
                height: 24
                radius: 5
                color: on ? "#4da3ff" : loopMouse.pressed ? "#50535a" : loopMouse.containsMouse ? "#45484e" : "#3a3c42"

                ActionGlyph {
                    anchors.centerIn: parent
                    kind: "loop"
                    ink: parent.on ? "black" : "#e6e6e6"
                }

                MouseArea {
                    id: loopMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: area.win.setTimelineLoop(!parent.on)
                }
            }

            // Where the playhead is (this app's own: ProPresenter's picture shows none
            // but its Timecode's)
            Text {
                objectName: "timelineTime"
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 8
                color: area.recording ? "#ff7a7a" : "#c9ccd2"
                font.pixelSize: 12
                font.family: "monospace"
                text: area.clock(area.position, true)
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 6
                visible: area.mine && area.player.recordedCount > 0
                color: "#ff7a7a"
                font.pixelSize: 11
                text: visible ? area.player.recordedCount + " recorded, saved when Record is turned off" : ""
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "#c9ccd2"
                font.pixelSize: 12
                text: "Duration"
            }

            NumberField {
                objectName: "timelineDuration"
                width: 64
                height: 24
                value: area.duration
                from: 1
                to: 36000
                suffix: "s"
                onEdited: (value) => area.win.setTimelineDuration(value)
                onFinished: area.win.takeFocus()
            }

            Item {
                width: 10
                height: 1
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "#c9ccd2"
                font.pixelSize: 14
                text: "↔"
            }

            AppSlider {
                objectName: "timelineZoom"
                width: 100
                anchors.verticalCenter: parent.verticalCenter
                from: 4
                to: 80
                value: area.pps
                onMoved: area.win.timelineZoom = value
            }
        }
    }
}
