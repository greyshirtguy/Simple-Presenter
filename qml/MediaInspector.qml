import QtQuick
import QtQuick.Dialogs
import QtMultimedia
import SimplePresenterApp

// The Media Inspector: what can be set about one piece of media, as ProPresenter's
// inspector sets it (src/mediasettings.h has what each setting is in the file). It is
// for a slide's media (Inspector… in a slide's menu, and in the menu of its media
// badge), a row of the media bin (in its menu) and an element filled with media (the
// editor's Fill section), and is put up over the operator window.
//
// On the left, the media as it will be shown, in the proportions of what it is shown in
// (the screen, or the element), with every setting as it is set, effects and all; and,
// for a video, a player of its own, silent, with play and pause, a bar to move through
// it on which the part that plays and the moment its picture is taken are marked, and
// the time gone and the time left. On the right, three tabs:
//
//  - Information: the file, where it is (Show in Folder), Choose… for another file in
//    its place (which keeps how it is fitted, turned and flipped, and its effects:
//    mediasettings::carryOver), its picture, for a video set to the frame the player is
//    on (Set to Current Frame) or left to the app, and what the file is.
//  - Properties: how it is fitted (Scale to Fit, Scale to Fill, Stretch to Fill and
//    Scale + Blur, as ProPresenter names them), turned (a quarter at a time), flipped
//    and cropped; and for a video whether it stops at its end or goes round, and the
//    part of it that plays (its in and out points, set from the player or typed).
//  - Effects: the effects it is drawn with, in order, each switched on and off, taken
//    off, and with what can be adjusted about it; Add Effect lists ProPresenter's.
//  - Playback Markers, for a video or a piece of audio (src/playbackmarkers.h): its
//    markers in the order of their times, each with its colour, its time, its name and
//    the icons of its actions; Add Marker puts one where the player is, and a right click
//    on the bar puts one where it was clicked; a click on a marker takes the player
//    there, and a right click has what can be done to it (Rename, Color, Add Action,
//    Remove Action, Duplicate, Move to Playhead, Delete: the operator window's
//    showMarkerMenu). The markers are drawn over the bar too, each a flag of its colour,
//    which is dragged along it to move the marker (the player following, to show where it
//    is going), clicked to take the player there, and right-clicked for its menu. A piece
//    of the audio bin is inspected for its markers alone: it has that tab and no other.
//
// Every change is kept as it is made, in the file, as the menus' changes are (a
// slider's when it is let go: until then it is only shown). Done, Esc or a click outside
// put the panel away. What is on the screens is left as it is: a slide or a row shows
// its media as now set the next time it is shown (show::State, which takes a background
// with other settings to be other media), as after the editor.
//
// What ProPresenter's has, from its own help (support.renewedvision.com, "What is the
// Media Inspector" and "Media Scaling Options"): the preview with the time gone and left
// and transport controls; tabs for Information (name, thumbnail with a custom one,
// details), Properties (alignment, scaling, cropping, rotation, playback behaviour,
// retrigger, in and out points, delay, play rate), Transition, Effects, Audio and
// Playback Markers. Only what the maintainer asked for is here: not alignment, delay,
// play rate, retrigger, the transition, the audio tab or markers. A GUESS: how all of it
// is laid out, the words on the buttons, and that its own player is silent.
Rectangle {
    id: panel

    property bool open: false
    // The operator window, for its catalog and its thumbnails. (Not `win`: in here that
    // would be found before the window's own name, and hide it.)
    property var owner: null
    // The media: { name, path, source, video, missing, settings, box }, `box` being the
    // size of what it is shown in ({ width, height }), for the preview's proportions
    property var media: null
    // Where it is, in a few words ("Slide 3 of “Abandoned”")
    property string about: ""
    // How changes are kept: { save(changes) → error message, "" when saved;
    // replace(file) → error message; reread() → the media as the file now has it;
    // place, where the media is, for its playback markers (see Catalog.addMarker) }
    property var keeper: null
    // The effects that can be added (Catalog.mediaEffects()), and the dialog's filter and
    // folder for another file
    property var effectChoices: []
    property string mediaFilter: "All files (*)"
    property string mediaFolder: ""
    // The settings as they are shown: the media's, with any change being dragged out
    property var settings: ({})
    // Changes shown and not yet kept (a slider being dragged)
    property var pending: ({})
    property string tab: "information"
    property string problem: ""
    // The effect whose adjustments are shown, by id; and whether the list to add one
    // from is up
    property string pickedEffect: ""
    property bool adding: false
    // Set while a video's player is being started to show its first frame
    property bool settling: false
    // The playback marker whose name is being typed, by id; "" for none
    property string renaming: ""

    // The preview's own player (for the scripted test, which sets where it is)
    readonly property alias player: previewPlayer
    readonly property bool isVideo: media !== null && media.video === true
    // A piece of the audio bin, inspected for its markers alone
    readonly property bool isAudio: media !== null && media.audio === true
    // Whether it plays for a time, and so has a bar to move along and markers on it
    readonly property bool timed: isVideo || isAudio
    readonly property var markers: media && timed ? (media.markers ?? []) : []
    readonly property var tabChoices: isAudio ? [["markers", "Playback Markers"]]
                                    : [["information", "Information"], ["properties", "Properties"], ["effects", "Effects"]]
                                      .concat(isVideo ? [["markers", "Playback Markers"]] : [])
    readonly property real duration: previewPlayer.duration
    readonly property real boxAspect: media && media.box && media.box.width > 0 && media.box.height > 0
                                      ? media.box.width / media.box.height : 16 / 9
    readonly property var effects: settings.effects ?? []
    readonly property var picked: effects.find(effect => effect.id === pickedEffect) ?? null
    readonly property bool trimmedIn: (settings.inPoint ?? 0) > 0
    readonly property real playsFrom: (settings.inPoint ?? 0) * 1000
    readonly property real playsTo: settings.trimmedOut ? settings.outPoint * 1000 : duration

    signal closed

    function show(media, about, keeper) {
        panel.media = media
        panel.about = about
        panel.keeper = keeper
        settings = media.settings ?? ({})
        pending = ({})
        problem = ""
        renaming = ""
        tab = media.audio === true ? "markers" : "information"
        adding = false
        pickedEffect = effects.length > 0 ? effects[0].id : ""
        open = true
        focusTaker.forceActiveFocus()
        showFirstFrame()
        Log.note("media", "the Media Inspector opened on " + (media.name ?? "") + " (" + about + ")")
    }

    function close() {
        if (!open)
            return
        keep()
        previewPlayer.stop()
        open = false
        media = null
        keeper = null
        closed()
    }

    // A change: shown at once, and kept unless it is `interim`, which a slider's is
    // while it is dragged.
    function change(changes, interim) {
        settings = Object.assign({}, settings, changes)
        pending = Object.assign({}, pending, changes)
        if (!interim)
            keep()
    }

    // Keeps what has been changed and not yet kept, and shows the settings as the file
    // then has them.
    function keep() {
        const changes = pending
        pending = ({})
        if (!keeper || Object.keys(changes).length === 0)
            return
        const error = keeper.save(changes)
        problem = error
        Log.note("media", "Media Inspector: " + Object.keys(changes).join(", ") + " of " + (media?.name ?? "") + (error === "" ? " kept" : " not kept: " + error))
        reread()
    }

    function reread() {
        const again = keeper ? keeper.reread() : null
        if (again) {
            const otherFile = !media || again.path !== media.path
            media = again
            settings = again.settings ?? ({})
            if (otherFile)
                showFirstFrame()
        }
    }

    // A video's player started, to be paused as soon as it has moved on from its start:
    // a player that has only been paused shows nothing, and the preview then has no
    // picture and no size to fit. (Not by the first frame to arrive, which can be one
    // that was there from the file before.)
    function showFirstFrame() {
        settling = isVideo
        if (isVideo)
            previewPlayer.play()
    }

    // The effects with one changed by `edit`, a function given a copy of it to change
    function withEffect(id, edit) {
        return effects.map(effect => {
            if (effect.id !== id)
                return effect
            const copy = Object.assign({}, effect)
            copy.variables = effect.variables.map(variable => Object.assign({}, variable))
            copy.numbers = effect.numbers.slice()
            copy.colors = effect.colors.slice()
            edit(copy)
            return copy
        })
    }

    // A variable of an effect set: shown at once, its shader given it too, and kept
    // unless `interim`.
    function setVariable(effectId, name, value, interim) {
        change({ effects: withEffect(effectId, effect => {
            const variable = effect.variables.find(v => v.name === name)
            if (!variable)
                return
            variable.value = value
            if (variable.slot >= 0) {
                if (variable.kind === "color")
                    effect.colors[variable.slot] = value
                else
                    effect.numbers[variable.slot] = Number(value)
            }
        }) }, interim)
    }

    function addEffect(renderId) {
        const made = owner.catalog.newMediaEffect(renderId)
        if (!made.id)
            return
        adding = false
        change({ effects: effects.concat([made]) }, false)
        pickedEffect = made.id
    }

    function removeEffect(id) {
        const index = effects.findIndex(effect => effect.id === id)
        change({ effects: effects.filter(effect => effect.id !== id) }, false)
        pickedEffect = effects.length > 0 ? effects[Math.min(index, effects.length - 1)].id : ""
    }

    // A playback marker where the player is (or at `seconds`), and its name typed
    function addMarker(seconds) {
        if (!keeper || !keeper.place)
            return
        const made = owner.catalog.addMarker(keeper.place, Math.max(0, seconds))
        problem = made.error
        Log.note("media", "Media Inspector: a marker added to " + (media?.name ?? "") + " at " + seconds.toFixed(2) + " s"
                 + (made.error === "" ? "" : ", which failed: " + made.error))
        afterMarkerChange()
    }

    // A marker being dragged along the bar, and where it is so far, in seconds; "" for none
    property string draggedMarker: ""
    property real draggedTime: 0

    // Where along the bar a marker's flag is (where it is being dragged to, or its time):
    // over where the middle of the slider's handle is at that time, so that a marker at
    // the player has its flag right over the handle; and the time at a point of the bar.
    function markerX(marker) {
        const time = draggedMarker === marker.id ? draggedTime : marker.time
        return bar.leftPadding + bar.handle.width / 2
               + Math.max(0, Math.min(1, time * 1000 / Math.max(1, duration))) * (bar.availableWidth - bar.handle.width)
    }

    function timeAt(x) {
        const along = (x - bar.leftPadding - bar.handle.width / 2) / Math.max(1, bar.availableWidth - bar.handle.width)
        return Math.max(0, Math.min(1, along)) * duration / 1000
    }

    // The markers have been changed: read again, and whatever is playing that media goes
    // by them as they now are (the operator window's markersEdited)
    function afterMarkerChange() {
        reread()
        if (keeper && keeper.place)
            owner.markersEdited(keeper.place, false)
    }

    // A marker let go of where it was dragged to: kept at its new time
    function moveMarker(id, seconds) {
        draggedMarker = ""
        if (!keeper || !keeper.place)
            return
        problem = owner.catalog.changeMarker(keeper.place, id, { time: seconds })
        Log.note("media", "Media Inspector: a marker of " + (media?.name ?? "") + " dragged to " + seconds.toFixed(2) + " s"
                 + (problem === "" ? "" : ", which failed: " + problem))
        afterMarkerChange()
    }

    function renameMarker(id, name) {
        renaming = ""
        if (!keeper || !keeper.place)
            return
        problem = owner.catalog.changeMarker(keeper.place, id, { name: name.trim() })
        afterMarkerChange()
        focusTaker.forceActiveFocus()
    }

    // A time in milliseconds as minutes, seconds and tenths
    function clock(ms) {
        const tenths = Math.max(0, Math.round(ms / 100))
        const minutes = Math.floor(tenths / 600)
        const seconds = (tenths % 600) / 10
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds.toFixed(1)
    }

    // Seconds typed as "90", "1:30" or "1:30.5"; -1 for what is not a time
    function typedSeconds(text) {
        const parts = text.trim().replace(",", ".").split(":")
        if (parts.length > 3 || parts.some(part => !/^[0-9]*\.?[0-9]*$/.test(part) || part === "" || part === "."))
            return -1
        return parts.reduce((sum, part) => sum * 60 + parseFloat(part), 0)
    }

    // The in point and the out point set, each in seconds; the out point at the end is
    // no out point, and is written as the file had it when it was not trimmed (at the
    // length, or not at all: see mediasettings.h).
    function setTrim(inSeconds, outSeconds) {
        const length = duration / 1000
        const changes = {}
        if (inSeconds !== undefined)
            changes.inPoint = Math.max(0, Math.min(inSeconds, length > 0 ? length : inSeconds))
        if (outSeconds !== undefined) {
            const end = settings.endPoint > 0 ? settings.endPoint : 0
            const atEnd = length > 0 && outSeconds >= length - 0.05
            changes.outPoint = atEnd ? end : Math.max(0, outSeconds)
            // An out point needs the length beside it, if the file does not say it, so
            // that it can be told from one at the end.
            if (!atEnd && end <= 0 && length > 0)
                changes.endPoint = length
        }
        change(changes, false)
    }

    // A button that is one of a few to choose between, lit when it is the one chosen
    component Choice: Rectangle {
        id: choice

        property string text
        property bool on: false

        signal clicked

        width: Math.max(52, label.implicitWidth + 24)
        height: 30
        radius: 6
        color: on ? "#ff8a1f" : choiceMouse.containsMouse ? "#45484e" : "#3a3c42"

        Text {
            id: label

            anchors.centerIn: parent
            color: choice.on ? "black" : "#e6e6e6"
            font.pixelSize: 13
            text: choice.text
        }

        MouseArea {
            id: choiceMouse

            anchors.fill: parent
            hoverEnabled: true
            onClicked: choice.clicked()
        }
    }

    visible: open
    color: "#99000000"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: (wheel) => wheel.accepted = true
        onClicked: panel.close()
    }

    // The keyboard, while the panel is up: Esc puts it away, the space bar plays and
    // pauses a video.
    Item {
        id: focusTaker

        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                panel.close()
            } else if (event.key === Qt.Key_Space && panel.isVideo) {
                panel.settling = false
                if (previewPlayer.playbackState === MediaPlayer.PlayingState)
                    previewPlayer.pause()
                else
                    previewPlayer.play()
            } else {
                return
            }
            event.accepted = true
        }
    }

    Rectangle {
        id: box

        objectName: "mediaInspector"
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 1080)
        height: Math.min(parent.height - 40, 640)
        radius: 10
        color: "#2b2d31"
        border.width: 1
        border.color: "#45484e"

        MouseArea {
            anchors.fill: parent
            onClicked: focusTaker.forceActiveFocus()
        }

        // ---- The heading
        Item {
            id: heading

            x: 20
            y: 16
            width: parent.width - 40
            height: done.height

            Column {
                anchors.left: parent.left
                anchors.right: done.left
                anchors.rightMargin: 12
                spacing: 3

                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    color: "#f0f0f0"
                    font.pixelSize: 17
                    font.bold: true
                    text: "Media Inspector" + (panel.media ? ": " + panel.media.name : "")
                }

                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    color: panel.problem !== "" ? "#ff8a80" : "#b4b7bd"
                    font.pixelSize: 13
                    text: panel.problem !== "" ? panel.problem : panel.about
                }
            }

            AppButton {
                id: done

                objectName: "inspectorDone"
                anchors.right: parent.right
                text: "Done"
                onClicked: panel.close()
            }
        }

        // ---- The preview, and a video's transport
        Item {
            id: left

            x: 20
            anchors.top: heading.bottom
            anchors.topMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: Math.round((box.width - 60) * 0.55)

            Rectangle {
                id: stageBox

                objectName: "inspectorPreview"
                width: Math.min(left.width, (left.height - transport.height - 12) * panel.boxAspect)
                height: width / panel.boxAspect
                anchors.horizontalCenter: parent.horizontalCenter
                color: "black"
                clip: true

                MediaLook {
                    id: previewLook

                    anchors.fill: parent
                    visible: panel.media !== null && !panel.media.missing && !panel.isAudio
                    settings: panel.settings
                    sourceWidth: panel.isVideo ? previewVideo.sourceRect.width : previewStill.implicitWidth
                    sourceHeight: panel.isVideo ? previewVideo.sourceRect.height : previewStill.implicitHeight

                    Image {
                        id: previewStill

                        anchors.fill: parent
                        visible: !panel.isVideo
                        fillMode: previewLook.imageFill
                        source: panel.open && panel.media && !panel.media.video && !panel.isAudio ? panel.media.source : ""
                        asynchronous: true
                        sourceSize: Qt.size(1920, 1920)
                    }

                    VideoOutput {
                        id: previewVideo

                        anchors.fill: parent
                        visible: panel.isVideo
                        fillMode: previewLook.videoFill
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: panel.media !== null && panel.media.missing === true
                    color: "#b4b7bd"
                    font.pixelSize: 13
                    text: "The file is not on this computer"
                }

                // A piece of audio: its notes and its name, there being nothing to see
                Column {
                    anchors.centerIn: parent
                    visible: panel.isAudio && panel.media !== null && !panel.media.missing
                    spacing: 10

                    ProIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: "Audio"
                        ink: "#8d9097"
                        size: 64
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#d6d8dc"
                        font.pixelSize: 14
                        text: panel.media ? panel.media.name ?? "" : ""
                    }
                }
            }

            MediaPlayer {
                id: previewPlayer

                // Its own, and silent: the room hears only what is on the screens.
                source: panel.open && panel.timed && !panel.media.missing ? panel.media.source : ""
                videoOutput: previewVideo
                onPositionChanged: {
                    if (panel.settling && position > 0) {
                        panel.settling = false
                        pause()
                    }
                }
            }

            Column {
                id: transport

                anchors.top: stageBox.bottom
                anchors.topMargin: 12
                width: left.width
                spacing: 6
                visible: panel.timed

                // The bar: where the player is, with the part that plays marked in
                // blue under it, and where the picture is taken from as a white mark;
                // over it, a band for the flags of the playback markers, whose points
                // are at the top of the handle (see MarkerFlag).
                Item {
                    width: parent.width
                    height: 26 + markerBand
                    readonly property int markerBand: panel.markers.length > 0 ? 12 : 0

                    Rectangle {
                        objectName: "inspectorTrimBar"
                        x: bar.leftPadding + (panel.duration > 0 ? panel.playsFrom / panel.duration : 0) * bar.availableWidth
                        width: Math.max(2, (panel.duration > 0 ? (panel.playsTo - panel.playsFrom) / panel.duration : 1) * bar.availableWidth)
                        y: parent.height - 4
                        height: 3
                        radius: 1.5
                        // (Blue, and not the orange of the part played, above it)
                        color: "#4da3ff"
                        opacity: panel.trimmedIn || panel.settings.trimmedOut ? 1 : 0.35
                    }

                    Rectangle {
                        visible: (panel.settings.thumbnail ?? -1) > 0 && panel.duration > 0
                        x: bar.leftPadding + Math.min(1, (panel.settings.thumbnail ?? 0) * 1000 / Math.max(1, panel.duration)) * bar.availableWidth - 1
                        y: parent.markerBand
                        width: 2
                        height: 7
                        color: "white"
                    }

                    AppSlider {
                        id: bar

                        objectName: "inspectorScrub"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 26
                        from: 0
                        to: Math.max(1, panel.duration)
                        value: pressed ? value : previewPlayer.position
                        onMoved: {
                            panel.settling = false
                            previewPlayer.position = value
                        }
                    }

                    // A right click on the bar puts a marker where it was clicked.
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        enabled: panel.timed && panel.duration > 0 && panel.keeper !== null && panel.keeper.place !== undefined
                        onClicked: (mouse) => {
                            const seconds = panel.timeAt(mapToItem(bar, mouse.x, 0).x)
                            panel.owner.showMenu([{ label: "Add Marker Here", run: () => panel.addMarker(seconds) }], this, mouse.x, mouse.y)
                        }
                    }

                    // The markers' flags (see MarkerFlag), the transport's shape and larger:
                    // each dragged along the bar to move its marker, the player following it
                    // to show where it is going (let go, it is kept there); clicked, the
                    // player taken to it; right-clicked, its menu. (ProPresenter's help says
                    // a marker is moved with Move to Playhead; that it can be dragged as well
                    // is A GUESS, asked for here.)
                    Repeater {
                        model: panel.duration > 0 ? panel.markers : []

                        delegate: MarkerFlag {
                            id: flag

                            required property var modelData
                            readonly property bool dragged: panel.draggedMarker === modelData.id

                            objectName: "inspectorMarkerMark"
                            x: panel.markerX(modelData) - width / 2
                            // (its point touching the top of the handle)
                            y: bar.y + bar.handle.y - height + 1
                            z: dragged ? 1 : 0
                            width: 14
                            height: 16
                            color: modelData.color
                            lit: dragged || flagMouse.containsMouse

                            // Where it is going, while it is dragged
                            Rectangle {
                                visible: flag.dragged
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: -height - 4
                                width: draggedTimeText.implicitWidth + 12
                                height: 20
                                radius: 4
                                color: "#15161a"
                                border.width: 1
                                border.color: "#5c5f66"

                                Text {
                                    id: draggedTimeText

                                    anchors.centerIn: parent
                                    color: "#e6e6e6"
                                    font.pixelSize: 12
                                    text: panel.clock(panel.draggedTime * 1000)
                                }
                            }

                            MouseArea {
                                id: flagMouse

                                property real pressedAt: 0
                                // Whether this press has dragged the marker (and so is no click)
                                property bool moved: false

                                objectName: "inspectorMarkerHandle"
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: flag.dragged ? Qt.ClosedHandCursor : Qt.SizeHorCursor
                                preventStealing: true
                                onPressed: (mouse) => {
                                    pressedAt = mapToItem(bar, mouse.x, 0).x
                                    moved = false
                                }
                                onPositionChanged: (mouse) => {
                                    if (!pressed || !(pressedButtons & Qt.LeftButton))
                                        return
                                    const along = mapToItem(bar, mouse.x, 0).x
                                    if (!flag.dragged && Math.abs(along - pressedAt) < 4)
                                        return
                                    const seconds = panel.timeAt(along)
                                    panel.draggedTime = seconds
                                    panel.draggedMarker = flag.modelData.id
                                    panel.settling = false
                                    previewPlayer.position = seconds * 1000
                                }
                                // Kept once the press is over: keeping it reads the markers
                                // again, which makes this flag anew.
                                onReleased: (mouse) => {
                                    if (mouse.button !== Qt.LeftButton || !flag.dragged)
                                        return
                                    moved = true
                                    const owner = panel
                                    const id = flag.modelData.id
                                    const seconds = owner.draggedTime
                                    Qt.callLater(() => owner.moveMarker(id, seconds))
                                }
                                onCanceled: panel.draggedMarker = ""
                                onClicked: (mouse) => {
                                    if (moved)
                                        return
                                    if (mouse.button === Qt.RightButton) {
                                        panel.owner.showMarkerMenu(panel.keeper.place, flag.modelData, flag, mouse.x, mouse.y,
                                                                   previewPlayer.position / 1000, () => panel.afterMarkerChange(),
                                                                   id => panel.renaming = id)
                                        return
                                    }
                                    panel.settling = false
                                    previewPlayer.position = flag.modelData.time * 1000
                                    focusTaker.forceActiveFocus()
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 30

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#d6d8dc"
                        font.pixelSize: 13
                        font.family: "monospace"
                        text: panel.clock(previewPlayer.position)
                    }

                    IconButton {
                        objectName: "inspectorPlay"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 40
                        height: 30
                        kind: previewPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                        onClicked: {
                            panel.settling = false
                            if (previewPlayer.playbackState === MediaPlayer.PlayingState)
                                previewPlayer.pause()
                            else
                                previewPlayer.play()
                            focusTaker.forceActiveFocus()
                        }
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#d6d8dc"
                        font.pixelSize: 13
                        font.family: "monospace"
                        text: "-" + panel.clock(Math.max(0, panel.duration - previewPlayer.position))
                    }
                }
            }
        }

        // ---- The tabs
        Item {
            id: right

            anchors.left: left.right
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.top: heading.bottom
            anchors.topMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18

            Row {
                id: tabs

                width: parent.width
                height: 32

                Repeater {
                    model: panel.tabChoices

                    delegate: Rectangle {
                        required property var modelData

                        objectName: "inspectorTab-" + modelData[0]
                        width: tabs.width / panel.tabChoices.length
                        height: tabs.height
                        color: panel.tab === modelData[0] ? "#45484e" : tabArea.containsMouse ? "#383a40" : "#323439"
                        border.width: 1
                        border.color: "#26282c"

                        Text {
                            anchors.centerIn: parent
                            color: panel.tab === parent.modelData[0] ? "#ffffff" : "#b4b7bd"
                            font.pixelSize: 13
                            font.bold: panel.tab === parent.modelData[0]
                            text: parent.modelData[1]
                        }

                        MouseArea {
                            id: tabArea

                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                panel.keep()
                                panel.tab = parent.modelData[0]
                                focusTaker.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            Flickable {
                id: page

                anchors.top: tabs.bottom
                anchors.topMargin: 14
                anchors.bottom: parent.bottom
                width: parent.width
                clip: true
                contentHeight: (panel.tab === "information" ? information : panel.tab === "properties" ? properties
                                : panel.tab === "markers" ? markersPage : effectsPage).height
                boundsBehavior: Flickable.StopAtBounds

                // -------- Information
                Column {
                    id: information

                    width: page.width
                    spacing: 10
                    visible: panel.tab === "information"

                    SectionTitle { text: "File" }

                    Text {
                        objectName: "inspectorPath"
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        color: "#e6e6e6"
                        font.pixelSize: 13
                        text: panel.media ? (panel.media.path !== "" ? panel.media.path : panel.media.name + " (not on this computer)") : ""
                    }

                    Row {
                        spacing: 10

                        AppButton {
                            objectName: "inspectorShowInFolder"
                            text: "Show in Folder"
                            enabled: panel.media !== null && panel.media.path !== ""
                            opacity: enabled ? 1 : 0.45
                            onClicked: panel.owner.catalog.showInFolder(panel.media.path)
                        }

                        AppButton {
                            objectName: "inspectorChoose"
                            text: "Choose…"
                            onClicked: fileDialog.open()
                        }
                    }

                    SectionTitle {
                        visible: panel.isVideo
                        text: "Thumbnail"
                    }

                    Row {
                        visible: panel.isVideo
                        spacing: 12

                        Rectangle {
                            width: 160
                            height: 90
                            color: "black"

                            // As the grid and the media bin show it: with its effects, held still
                            MediaPicture {
                                objectName: "inspectorThumbnail"
                                anchors.fill: parent
                                source: panel.open && panel.media && panel.media.path !== "" ? panel.owner.thumbnailUrl(panel.media.path, panel.settings) : ""
                                settings: panel.settings
                                shaped: true
                                animated: false
                            }
                        }

                        Column {
                            spacing: 8

                            Text {
                                color: "#b4b7bd"
                                font.pixelSize: 12
                                text: (panel.settings.thumbnail ?? -1) > 0 ? "From " + panel.clock(panel.settings.thumbnail * 1000)
                                                                           : "Chosen by the app"
                            }

                            AppButton {
                                objectName: "inspectorSetThumbnail"
                                text: "Set to Current Frame"
                                // (A moment of nought is no moment chosen: see mediasettings.h.)
                                onClicked: panel.change({ thumbnail: Math.max(0.001, previewPlayer.position / 1000) }, false)
                            }

                            AppButton {
                                objectName: "inspectorAutoThumbnail"
                                text: "Automatic"
                                enabled: (panel.settings.thumbnail ?? -1) > 0
                                opacity: enabled ? 1 : 0.45
                                // As ProPresenter writes no moment chosen
                                onClicked: panel.change({ thumbnail: -1 }, false)
                            }
                        }
                    }

                    SectionTitle { text: "Details" }

                    Grid {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6

                        Text { color: "#8d9097"; font.pixelSize: 13; text: "Kind" }
                        Text { color: "#e6e6e6"; font.pixelSize: 13; text: panel.isVideo ? "Video" : "Picture" }
                        Text { color: "#8d9097"; font.pixelSize: 13; text: "Size" }
                        Text {
                            objectName: "inspectorSize"
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            text: previewLook.sourceWidth > 0 ? Math.round(previewLook.sourceWidth) + " × " + Math.round(previewLook.sourceHeight) : "—"
                        }
                        Text { color: "#8d9097"; font.pixelSize: 13; text: "Length"; visible: panel.isVideo }
                        Text { color: "#e6e6e6"; font.pixelSize: 13; visible: panel.isVideo; text: panel.duration > 0 ? panel.clock(panel.duration) : "—" }
                        Text { color: "#8d9097"; font.pixelSize: 13; text: "Format" }
                        Text {
                            color: "#e6e6e6"
                            font.pixelSize: 13
                            text: panel.media ? (panel.media.name.includes(".") ? panel.media.name.split(".").pop().toUpperCase() : "—") : ""
                        }
                    }
                }

                // -------- Properties
                Column {
                    id: properties

                    width: page.width
                    spacing: 10
                    visible: panel.tab === "properties"

                    SectionTitle { text: "Scaling" }

                    AppComboBox {
                        objectName: "inspectorScaling"
                        width: Math.min(parent.width, 240)
                        model: ["Scale to Fit", "Scale to Fill", "Stretch to Fill", "Scale + Blur"]
                        currentIndex: panel.settings.blurred && (panel.settings.scale ?? 0) === 0 ? 3 : (panel.settings.scale ?? 0)
                        onActivated: (index) => panel.change(index === 3 ? { scale: 0, blurred: true } : { scale: index, blurred: false }, false)
                    }

                    SectionTitle { text: "Rotation" }

                    Row {
                        spacing: 6

                        Repeater {
                            model: [0, 90, 180, 270]

                            delegate: Choice {
                                required property int modelData

                                objectName: "inspectorRotate" + modelData
                                text: modelData + "°"
                                on: (panel.settings.rotation ?? 0) === modelData
                                onClicked: panel.change({ rotation: modelData }, false)
                            }
                        }
                    }

                    SectionTitle { text: "Flip" }

                    Row {
                        spacing: 14

                        AppCheck {
                            objectName: "inspectorFlipH"
                            text: "Horizontal"
                            checked: panel.settings.flipH ?? false
                            onToggled: (checked) => panel.change({ flipH: checked }, false)
                        }

                        AppCheck {
                            objectName: "inspectorFlipV"
                            text: "Vertical"
                            checked: panel.settings.flipV ?? false
                            onToggled: (checked) => panel.change({ flipV: checked }, false)
                        }
                    }

                    SectionTitle { text: "Cropping" }

                    AppCheck {
                        objectName: "inspectorCrop"
                        text: "Crop, by pixels of the media at its own size"
                        checked: panel.settings.cropOn ?? false
                        onToggled: (checked) => panel.change({ cropOn: checked }, false)
                    }

                    Grid {
                        columns: 4
                        columnSpacing: 10
                        rowSpacing: 6
                        opacity: panel.settings.cropOn ? 1 : 0.45

                        Repeater {
                            model: [["cropLeft", "Left"], ["cropRight", "Right"], ["cropTop", "Top"], ["cropBottom", "Bottom"]]

                            delegate: Column {
                                required property var modelData

                                spacing: 3

                                Text {
                                    color: "#8d9097"
                                    font.pixelSize: 12
                                    text: parent.modelData[1]
                                }

                                NumberField {
                                    objectName: "inspector-" + parent.modelData[0]
                                    width: 70
                                    from: 0
                                    to: 100000
                                    value: panel.settings[parent.modelData[0]] ?? 0
                                    onEdited: (value) => {
                                        const changes = {}
                                        changes[parent.modelData[0]] = value
                                        // Typing a crop is wanting one.
                                        if (!panel.settings.cropOn)
                                            changes.cropOn = true
                                        panel.change(changes, false)
                                    }
                                    onFinished: focusTaker.forceActiveFocus()
                                }
                            }
                        }
                    }

                    SectionTitle {
                        visible: panel.isVideo
                        text: "Playback"
                    }

                    AppComboBox {
                        objectName: "inspectorPlayback"
                        visible: panel.isVideo
                        width: Math.min(parent.width, 240)
                        // (A video that goes round a number of times or for a time, as the
                        // slide's playback menu sets it, is one that goes round.)
                        model: ["Stop", "Loop"]
                        currentIndex: (panel.settings.playback ?? 0) === 0 ? 0 : 1
                        onActivated: (index) => panel.change({ playback: index }, false)
                    }

                    SectionTitle {
                        visible: panel.isVideo
                        text: "Trim"
                    }

                    Grid {
                        visible: panel.isVideo
                        columns: 3
                        columnSpacing: 10
                        rowSpacing: 8
                        verticalItemAlignment: Grid.AlignVCenter

                        Text { color: "#8d9097"; font.pixelSize: 13; text: "In" }

                        AppTextField {
                            id: inField

                            objectName: "inspectorIn"
                            width: 90
                            text: panel.clock((panel.settings.inPoint ?? 0) * 1000)
                            onEditingFinished: {
                                const seconds = panel.typedSeconds(text)
                                if (seconds >= 0)
                                    panel.setTrim(seconds, undefined)
                                text = Qt.binding(() => panel.clock((panel.settings.inPoint ?? 0) * 1000))
                                focusTaker.forceActiveFocus()
                            }
                        }

                        AppButton {
                            objectName: "inspectorSetIn"
                            text: "Set to Current Time"
                            onClicked: panel.setTrim(previewPlayer.position / 1000, undefined)
                        }

                        Text { color: "#8d9097"; font.pixelSize: 13; text: "Out" }

                        AppTextField {
                            id: outField

                            objectName: "inspectorOut"
                            width: 90
                            text: panel.clock(panel.playsTo)
                            onEditingFinished: {
                                const seconds = panel.typedSeconds(text)
                                if (seconds >= 0)
                                    panel.setTrim(undefined, seconds)
                                text = Qt.binding(() => panel.clock(panel.playsTo))
                                focusTaker.forceActiveFocus()
                            }
                        }

                        AppButton {
                            objectName: "inspectorSetOut"
                            text: "Set to Current Time"
                            onClicked: panel.setTrim(undefined, previewPlayer.position / 1000)
                        }
                    }

                    Row {
                        visible: panel.isVideo
                        spacing: 12

                        AppButton {
                            objectName: "inspectorResetTrim"
                            text: "Reset Trim"
                            enabled: panel.trimmedIn || (panel.settings.trimmedOut ?? false)
                            opacity: enabled ? 1 : 0.45
                            onClicked: panel.setTrim(0, panel.duration / 1000)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#8d9097"
                            font.pixelSize: 12
                            text: panel.duration > 0 ? "Plays " + panel.clock(Math.max(0, panel.playsTo - panel.playsFrom)) + " of " + panel.clock(panel.duration) : ""
                        }
                    }
                }

                // -------- Playback Markers
                Column {
                    id: markersPage

                    width: page.width
                    spacing: 8
                    visible: panel.tab === "markers"

                    Item {
                        width: parent.width
                        height: addMarker.height

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#b4b7bd"
                            font.pixelSize: 13
                            text: panel.markers.length === 0 ? "No markers" : panel.markers.length + (panel.markers.length === 1 ? " marker" : " markers")
                        }

                        AppButton {
                            id: addMarker

                            objectName: "inspectorAddMarker"
                            anchors.right: parent.right
                            text: "Add Marker"
                            enabled: panel.media !== null && panel.keeper !== null && panel.keeper.place !== undefined && !panel.media.missing
                            opacity: enabled ? 1 : 0.45
                            onClicked: {
                                panel.settling = false
                                panel.addMarker(previewPlayer.position / 1000)
                            }
                        }
                    }

                    Repeater {
                        model: panel.markers

                        delegate: Rectangle {
                            id: markerRow

                            required property var modelData
                            required property int index
                            readonly property bool typing: panel.renaming === modelData.id

                            objectName: "inspectorMarker:" + index
                            width: markersPage.width
                            height: 38
                            radius: 6
                            color: markerMouse.containsMouse ? "#3a3c42" : "#323439"

                            MouseArea {
                                id: markerMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: (mouse) => {
                                    if (mouse.button === Qt.RightButton) {
                                        panel.owner.showMarkerMenu(panel.keeper.place, markerRow.modelData, markerRow, mouse.x, mouse.y,
                                                                   previewPlayer.position / 1000, () => panel.afterMarkerChange(),
                                                                   id => panel.renaming = id)
                                        return
                                    }
                                    // The player taken to it
                                    panel.settling = false
                                    previewPlayer.position = markerRow.modelData.time * 1000
                                    focusTaker.forceActiveFocus()
                                }
                            }

                            // Its colour, as a marker's mark is drawn
                            Rectangle {
                                id: markerColor

                                x: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: 10
                                height: 10
                                rotation: 45
                                color: markerRow.modelData.color
                            }

                            Text {
                                id: markerTime

                                x: 32
                                anchors.verticalCenter: parent.verticalCenter
                                width: 62
                                color: "#d6d8dc"
                                font.pixelSize: 13
                                font.family: "monospace"
                                text: panel.clock(markerRow.modelData.time * 1000)
                            }

                            Text {
                                anchors.left: markerTime.right
                                anchors.leftMargin: 8
                                anchors.right: markerActions.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: !markerRow.typing
                                elide: Text.ElideRight
                                color: markerRow.modelData.name !== "" ? "#e6e6e6" : "#8d9097"
                                font.pixelSize: 13
                                text: markerRow.modelData.name !== "" ? markerRow.modelData.name : "Marker"
                            }

                            AppTextField {
                                objectName: "inspectorMarkerName"
                                anchors.left: markerTime.right
                                anchors.leftMargin: 8
                                anchors.right: markerActions.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: markerRow.typing
                                height: 28
                                font.pixelSize: 13
                                text: markerRow.modelData.name
                                onVisibleChanged: {
                                    if (visible) {
                                        forceActiveFocus()
                                        selectAll()
                                    }
                                }
                                onAccepted: panel.renameMarker(markerRow.modelData.id, text)
                                Keys.onEscapePressed: {
                                    panel.renaming = ""
                                    focusTaker.forceActiveFocus()
                                }
                            }

                            // Its actions, each by its icon
                            Row {
                                id: markerActions

                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3

                                Repeater {
                                    model: markerRow.modelData.actions

                                    delegate: ActionGlyph {
                                        required property var modelData

                                        objectName: "inspectorMarkerAction"
                                        anchors.verticalCenter: parent.verticalCenter
                                        kind: modelData.kind ?? "other"
                                        picture: modelData.picture ?? ""
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: panel.markers.length === 0
                        wrapMode: Text.Wrap
                        color: "#8d9097"
                        font.pixelSize: 12
                        text: "A marker is a point in the media with actions that are run when playback reaches it, or when it is "
                              + "picked from the transport's Marker menu. Add Marker puts one where the player is; a right click on "
                              + "the bar puts one where it was clicked."
                    }
                }

                // -------- Effects
                Column {
                    id: effectsPage

                    width: page.width
                    spacing: 10
                    visible: panel.tab === "effects"

                    Item {
                        width: parent.width
                        height: addButton.height

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#b4b7bd"
                            font.pixelSize: 13
                            text: panel.effects.length === 0 ? "No effects" : panel.effects.length + (panel.effects.length === 1 ? " effect" : " effects")
                        }

                        AppButton {
                            id: addButton

                            objectName: "inspectorAddEffect"
                            anchors.right: parent.right
                            text: panel.adding ? "Cancel" : "Add Effect"
                            onClicked: panel.adding = !panel.adding
                        }
                    }

                    // The effects there are to add, by name: two columns of them
                    Grid {
                        visible: panel.adding
                        width: parent.width
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 4

                        Repeater {
                            model: panel.adding ? panel.effectChoices.filter(choice => choice.drawn) : []

                            delegate: Rectangle {
                                required property var modelData

                                objectName: "inspectorChoice-" + modelData.name
                                width: (effectsPage.width - 6) / 2
                                height: 26
                                radius: 4
                                color: choiceArea.containsMouse ? "#45484e" : "#34363b"

                                Text {
                                    x: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 16
                                    elide: Text.ElideRight
                                    color: "#e6e6e6"
                                    font.pixelSize: 13
                                    text: parent.modelData.name
                                }

                                MouseArea {
                                    id: choiceArea

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: panel.addEffect(parent.modelData.renderId)
                                }
                            }
                        }
                    }

                    // The media's effects, in order
                    Repeater {
                        model: panel.adding ? [] : panel.effects

                        delegate: Rectangle {
                            id: effectRow

                            required property var modelData
                            required property int index

                            objectName: "inspectorEffect-" + modelData.name
                            width: effectsPage.width
                            height: 34
                            radius: 5
                            color: panel.pickedEffect === modelData.id ? "#45484e" : "#34363b"

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    panel.keep()
                                    panel.pickedEffect = effectRow.modelData.id
                                }
                            }

                            AppCheck {
                                x: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: effectRow.modelData.name + (effectRow.modelData.shader === "" ? " (not drawn here)" : "")
                                checked: effectRow.modelData.on
                                onToggled: (checked) => panel.change({ effects: panel.withEffect(effectRow.modelData.id, effect => effect.on = checked) }, false)
                            }

                            AppButton {
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                height: 26
                                text: "Remove"
                                onClicked: panel.removeEffect(effectRow.modelData.id)
                            }
                        }
                    }

                    // What can be adjusted about the effect picked
                    Column {
                        visible: !panel.adding && panel.picked !== null
                        width: parent.width
                        spacing: 8

                        SectionTitle { text: panel.picked ? panel.picked.name : "" }

                        // A row for each variable, kept for as long as there are as many: the
                        // effect is a new map at every step of a slider's drag, and rows made
                        // again from it would drop the slider being dragged.
                        Repeater {
                            model: panel.picked ? panel.picked.variables.length : 0

                            delegate: Column {
                                id: variableRow

                                required property int index
                                readonly property var modelData: panel.picked && index < panel.picked.variables.length
                                                                 ? panel.picked.variables[index] : ({ name: "", kind: "other", value: 0, least: 0, most: 1 })

                                visible: modelData.kind !== "other"
                                width: effectsPage.width
                                spacing: 2

                                Item {
                                    width: parent.width
                                    height: 18

                                    Text {
                                        color: "#d6d8dc"
                                        font.pixelSize: 12
                                        text: variableRow.modelData.name
                                    }

                                    Text {
                                        anchors.right: parent.right
                                        visible: variableRow.modelData.kind !== "color"
                                        color: "#8d9097"
                                        font.pixelSize: 12
                                        text: variableRow.modelData.kind === "int" ? Math.round(variableRow.modelData.value)
                                                                                  : Number(variableRow.modelData.value).toFixed(2)
                                    }
                                }

                                AppSlider {
                                    objectName: "inspectorVariable-" + variableRow.modelData.name
                                    visible: variableRow.modelData.kind !== "color"
                                    width: parent.width
                                    from: variableRow.modelData.least
                                    to: variableRow.modelData.most > variableRow.modelData.least ? variableRow.modelData.most : variableRow.modelData.least + 1
                                    stepSize: variableRow.modelData.kind === "int" ? 1 : 0
                                    value: variableRow.modelData.value
                                    onMoved: panel.setVariable(panel.pickedEffect, variableRow.modelData.name, value, true)
                                    onPressedChanged: if (!pressed) panel.keep()
                                }

                                ColorButton {
                                    visible: variableRow.modelData.kind === "color"
                                    value: variableRow.modelData.kind === "color" ? variableRow.modelData.value : "white"
                                    onChanging: (value) => panel.setVariable(panel.pickedEffect, variableRow.modelData.name, value, true)
                                    onPicked: (value) => panel.setVariable(panel.pickedEffect, variableRow.modelData.name, value, false)
                                }
                            }
                        }

                        AppButton {
                            objectName: "inspectorResetEffect"
                            text: "Reset Effect"
                            onClicked: panel.change({ effects: panel.withEffect(panel.pickedEffect, effect => {
                                for (const variable of effect.variables) {
                                    if (variable.kind === "other")
                                        continue
                                    variable.value = variable.initial
                                    if (variable.slot >= 0) {
                                        if (variable.kind === "color")
                                            effect.colors[variable.slot] = variable.initial
                                        else
                                            effect.numbers[variable.slot] = Number(variable.initial)
                                    }
                                }
                            }) }, false)
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog

        title: "Choose Media"
        currentFolder: panel.mediaFolder !== "" ? "file://" + panel.mediaFolder : ""
        nameFilters: [panel.mediaFilter, "All files (*)"]
        onAccepted: {
            const file = decodeURIComponent(String(selectedFile).replace(/^file:\/\//, ""))
            const error = panel.keeper ? panel.keeper.replace(file) : ""
            panel.problem = error
            Log.note("media", "Media Inspector: " + (panel.media?.name ?? "") + " replaced by " + file.split("/").pop()
                     + (error === "" ? "" : ", which failed: " + error))
            panel.reread()
            focusTaker.forceActiveFocus()
        }
        onRejected: focusTaker.forceActiveFocus()
    }
}
