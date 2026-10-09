import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// A video's sound: a background is played silently and a foreground with its sound, which rises and falls with the
// picture in a transition.
QtObject {
    id: t

//COMMON
    readonly property string clip: "@MADE@/i-with-sound.mp4"
    property var levels: []
    property Timer sampler: Timer {
        interval: 100
        repeat: true
        onTriggered: {
            // (The output goes with its player once the dissolve is done.)
            if (t.kept.sound && t.kept.sound.volume !== undefined)
                t.levels.push(+t.kept.sound.volume.toFixed(2))
        }
    }

    function row() {
        return mediaFiles.find(m => m.name === "i-with-sound.mp4")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                const playlist = catalog.mediaPlaylists.find(n => !n.folder)
                openMediaPlaylist(playlist.path)
                kept.before = mediaFiles.length
                check("a video with sound is added to the media playlist", report(catalog.addMedia(mediaPlaylistId, ["file://" + clip])) && mediaFiles.length === kept.before + 1 && row() !== undefined)
                check("as a background, and so with no sound to play", row().foreground === false && row().volume === 0 && row().loops === true, JSON.stringify([row().foreground, row().volume, row().loops]))
                showMedia(row(), mediaPlaylistId)
                return 1500
            },
            () => {
                check("the background plays", player() !== null && player().playbackState === MediaPlayer.PlayingState && player().position > 300, player() ? player().position : "none")
                check("with nothing to be heard through", player().audioOutput === null)
                check("though the file has sound in it", player().audioTracks.length === 1, player().audioTracks.length)
                setMediaItemForeground(row().id, true)
                check("made a foreground, it is to be played with its sound, at full", row().foreground === true && row().volume === 1 && row().loops === false, JSON.stringify([row().foreground, row().volume, row().loops]))
                showMedia(row(), mediaPlaylistId)
                return 1500
            },
            () => {
                check("the foreground plays", player() !== null && player().playbackState === MediaPlayer.PlayingState && player().position > 300, player() ? player().position : "none")
                check("through a sound output at full volume", player().audioOutput !== null && Math.abs(player().audioOutput.volume - 1) < 0.001, player().audioOutput ? player().audioOutput.volume : "none")
                check("and the log says so", /i-with-sound\.mp4": first picture.*its sound played through/.test(testInput.readText(Log.path).split("\n").filter(l => l.includes("first picture")).pop()),
                      testInput.readText(Log.path).split("\n").filter(l => l.includes("first picture")).pop())
                // A long dissolve to a silent background: the foreground's sound goes down with its picture.
                kept.sound = player().audioOutput
                kept.foregroundPlayer = player()
                selectTransition("Dissolve")
                transitionDuration = 2
                levels = []
                sampler.start()
                const background = mediaFiles.find(m => m.video && !m.missing && m.name !== "i-with-sound.mp4")
                kept.background = background.name
                showMedia(background, mediaPlaylistId)
                return 1200
            },
            () => {
                const now = kept.sound ? kept.sound.volume : -1
                check("half way through the dissolve its sound is part of the way down", now > 0.05 && now < 0.95, now + " after " + JSON.stringify(levels))
                return 1800
            },
            () => {
                sampler.stop()
                const falling = levels.every((level, i) => i === 0 || level <= levels[i - 1] + 0.001)
                check("having gone down all the way without going back up", falling && levels[0] > 0.9 && levels.some(level => level < 0.2), JSON.stringify(levels))
                check("and the background that took its place is silent", player() !== null && player() !== kept.foregroundPlayer && player().audioOutput === null && liveMedia.name === kept.background)
                // And back: the foreground's sound comes up with its picture.
                kept.sound = null
                levels = []
                showMedia(row(), mediaPlaylistId)
                return 700
            },
            () => {
                const incoming = Lib.findAll(output.contentItem, item => item.content !== undefined && item.content !== null && item.content.name === "i-with-sound.mp4")
                check("the foreground is on its way in", incoming.length === 1 && incoming[0].player !== null && incoming[0].player.audioOutput !== null, incoming.length)
                kept.sound = incoming[0].player.audioOutput
                kept.level = incoming[0].level
                check("with its sound part of the way up, as its picture is", kept.sound.volume > 0.02 && kept.sound.volume < 0.9 && Math.abs(kept.sound.volume - kept.level) < 0.05, kept.sound.volume + " at level " + kept.level)
                return 2200
            },
            () => {
                check("and at full when the dissolve is done", Math.abs(kept.sound.volume - 1) < 0.001 && player() !== null && player().audioOutput === kept.sound, kept.sound.volume)
                transitionDuration = 0
                clearMedia()
                return 600
            },
            () => {
                check("cleared, nothing plays", player() === null && liveMedia === null)
                check("a still has no sound to it", mediaFiles.filter(m => !m.video).every(m => m.volume === 0))
                // The same file on a slide: a background there is silent, a foreground is heard.
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                kept.slide = document.slides.findIndex(s => s.mediaName === "")
                check("put on a slide as a background it has no sound", report(catalog.setSlideMedia(document.path, document.slides[kept.slide].id, clip, false)))
                reloadDocument()
                check("  (so the slide says)", document.slides[kept.slide].media.volume === 0 && document.slides[kept.slide].mediaForeground === false, JSON.stringify(document.slides[kept.slide].media))
                report(catalog.setSlideMediaForeground(document.path, document.slides[kept.slide].id, true))
                reloadDocument()
                check("and as a foreground it has", document.slides[kept.slide].media.volume === 1 && document.slides[kept.slide].mediaForeground === true, JSON.stringify(document.slides[kept.slide].media))
                goLive(kept.slide)
                return 1500
            },
            () => {
                check("which the slide plays", player() !== null && player().audioOutput !== null && player().audioOutput.volume === 1 && player().position > 200, player() ? player().position : "none")
                clearAll()
                return 400
            },
            () => {
                check("nothing went wrong on the way", !/PROBLEM|WARNING|ERROR/.test(testInput.readText(Log.path).split("\n").filter(l => !l.includes("qt.qpa.theme")).join("\n")),
                      testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
