import QtQuick
import SimplePresenterApp

// What a MIDI note does, by ProPresenter's MIDI Map (see src/communications.h): heard from
// the sources of a MIDI device that is connected (Devices.midiCommand), each command is
// done here as a click or a key in this window does it, through the window's own
// functions and through Show, which is the keeper of what is live. Those "by index" take
// the note's velocity as which one, from 1: the 4th playlist, the 2nd prop.
//
// GUESSES at what ProPresenter's commands do, from their names and its help (which says
// only that a note selects or triggers): a playlist, a media playlist or an audio
// playlist "selected" is opened in this window; a playlist item selected is opened, and
// Trigger Slide takes a slide of the presentation open live; Next and Previous Playlist
// Item take the next presentation of the playlist open live at its first slide; Toggle
// Prop and Trigger Macro count in the collection selected, the first until one is.
// Bail to Logo and the timelines are not here: this app has no logo and no timelines.
QtObject {
    id: control

    required property var window

    // The commands there is nothing here to do, by their keys: greyed in the map
    readonly property var notHere: ["bailToLogo", "startTimeline", "stopTimeline", "rewindTimeline"]
    // The collections of props and of macros a note selected, by their places
    property int propCollection: 0
    property int macroCollection: 0

    readonly property var catalog: window.catalog

    function player() {
        return window.output ? window.output.livePlayer : null
    }

    // The presentation of the playlist open after (or before) the one open, its first
    // slide taken live
    function stepPlaylistItem(delta) {
        if (window.playlistId === "")
            return false
        const rows = catalog.playlistItems(window.playlistId).filter(row => window.openable(row))
        const at = rows.findIndex(row => row.path === window.documentKey)
        const row = at < 0 ? (delta > 0 ? rows[0] : undefined) : rows[at + delta]
        if (!row)
            return false
        window.openDocument(row.path)
        window.goLive(0)
        return true
    }

    // Does a command; says in the log what, and what of it could not be
    function run(key, index) {
        const nth = index - 1
        let done = true
        switch (key) {
        case "clearAll": Show.clearAll(); break
        case "clearSlide": Show.clearSlide(); break
        case "clearBackground": Show.clearMedia(); break
        case "clearAnnouncements": Show.clearAnnouncements(); break
        case "clearProps": Show.clearProps(); break
        case "clearMessages": Show.clearMessages(); break
        case "clearAudio": Show.clearAudio(); break
        case "goToBeginning":
        case "playPause":
        case "play":
        case "pause": {
            const media = player()
            if (!media || !Show.liveMedia) {
                done = false
                break
            }
            if (key === "goToBeginning")
                media.setPosition(0)
            else if (key === "play" || (key === "playPause" && media.playbackState !== 1))
                media.play()
            else
                media.pause()
            break
        }
        case "nextPlaylistItem": done = stepPlaylistItem(1); break
        case "previousPlaylistItem": done = stepPlaylistItem(-1); break
        case "nextSlide": window.step(1); break
        case "previousSlide": window.step(-1); break
        case "selectPlaylist": {
            const playlist = catalog.playlists.filter(p => !p.folder)[nth]
            if (playlist)
                window.openPlaylist(playlist.path)
            done = playlist !== undefined
            break
        }
        case "selectPlaylistItem": {
            const row = window.playlistId === "" ? undefined : catalog.playlistItems(window.playlistId)[nth]
            done = row !== undefined && window.openable(row)
            if (done)
                window.openDocument(row.path)
            break
        }
        case "triggerSlide":
            done = window.document !== null && nth >= 0 && nth < window.document.slides.length
            if (done)
                window.goLive(nth)
            break
        case "selectMediaPlaylist":
        case "selectAudioPlaylist": {
            const audio = key === "selectAudioPlaylist"
            const node = (audio ? catalog.audioPlaylists : catalog.mediaPlaylists).filter(n => !n.folder)[nth]
            done = node !== undefined
            if (done && audio)
                window.openAudioPlaylist(node.path)
            else if (done)
                window.openMediaPlaylist(node.path)
            break
        }
        case "triggerMedia": {
            const row = window.mediaPlaylistId === "" ? undefined : catalog.mediaIn(window.mediaPlaylistId)[nth]
            done = row !== undefined && !row.missing
            if (done)
                window.showMedia(row, window.mediaPlaylistId)
            break
        }
        case "triggerAudio": {
            const row = window.audioPlaylistId === "" ? undefined : catalog.audioIn(window.audioPlaylistId)[nth]
            done = row !== undefined && !row.missing
            if (done)
                window.playAudio(row, window.audioPlaylistId)
            break
        }
        case "selectPropCollection":
            done = nth >= 0 && nth < Props.collections.length
            if (done)
                propCollection = nth
            break
        case "toggleProp": {
            const collection = Props.collections[propCollection]
            const prop = collection ? collection.props[nth] : undefined
            done = prop !== undefined
            if (done)
                Show.toggleProp(prop.id)
            break
        }
        case "selectMacroCollection":
            done = nth >= 0 && nth < Macros.collections.length
            if (done)
                macroCollection = nth
            break
        case "triggerMacro": {
            const collection = Macros.collections[macroCollection]
            const macro = collection ? collection.macros[nth] : undefined
            done = macro !== undefined
            if (done)
                window.runMacro(macro.id)
            break
        }
        case "startTimer":
        case "stopTimer":
        case "resetTimer": {
            const timer = Timers.timers[nth]
            done = timer !== undefined
            if (done && key === "startTimer")
                Timers.start(timer.id)
            else if (done && key === "stopTimer")
                Timers.stop(timer.id)
            else if (done)
                Timers.reset(timer.id)
            break
        }
        default:
            done = false
        }
        if (!done)
            Log.note("devices", "MIDI asked for " + key + (index > 0 ? " " + index : "") + (notHere.includes(key) ? ": not done here" : ": there is no such one here"))
    }

    readonly property Connections heard: Connections {
        target: Devices

        function onMidiCommand(key, index) {
            control.run(key, index)
        }
    }
}
