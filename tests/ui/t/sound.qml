import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// A video's sound: a background is played silently and a foreground with its sound, handed to the app's mixer and
// heard, which rises and falls with the picture in a transition.
QtObject {
    id: t

//COMMON
    readonly property string clip: "@MADE@/i-with-sound.mp4"
    property var levels: []
    property Timer sampler: Timer {
        interval: 100
        repeat: true
        onTriggered: {
            // (The feed goes with its player once the dissolve is done.)
            if (t.kept.sound && t.kept.sound.gain !== undefined)
                t.levels.push(+t.kept.sound.gain.toFixed(2))
        }
    }

    function row() {
        return mediaFiles.find(m => m.name === "i-with-sound.mp4")
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // What hands the sound of the video `p` plays to the mixer, if anything does
    function feedOf(p) {
        const shown = Lib.findAll(output.contentItem, item => item.feed !== undefined && item.player === p)
        return shown.length > 0 ? shown[0].feed : null
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
                check("with nothing to be heard through: no sound output, and its sound not handed to the mixer", player().audioOutput === null && player().audioBufferOutput === null
                      && feedOf(player()) === null)
                check("though the file has sound in it", player().audioTracks.length === 1, player().audioTracks.length)
                setMediaItemForeground(row().id, true)
                check("made a foreground, it is to be played with its sound, at full", row().foreground === true && row().volume === 1 && row().loops === false, JSON.stringify([row().foreground, row().volume, row().loops]))
                showMedia(row(), mediaPlaylistId)
                return 1500
            },
            () => {
                check("the foreground plays", player() !== null && player().playbackState === MediaPlayer.PlayingState && player().position > 300, player() ? player().position : "none")
                check("its sound handed to the mixer at full, and no sound output of its own", player().audioOutput === null && player().audioBufferOutput !== null
                      && feedOf(player()) !== null && Math.abs(feedOf(player()).gain - 1) < 0.001, feedOf(player()) ? feedOf(player()).gain : "none")
                // The file's sound is of one channel, at its loudest 13 dB down: heard on both sides alike.
                check("and heard: the meter has it, both bars well up and level with each other", AudioMeter.sounding && AudioMeter.left > 0.5
                      && Math.abs(AudioMeter.left - AudioMeter.right) < 0.02, AudioMeter.left + " and " + AudioMeter.right)
                check("and the log says so", /i-with-sound\.mp4": first picture.*its sound played through Test sink/.test(testInput.readText(Log.path).split("\n").filter(l => l.includes("first picture")).pop()),
                      testInput.readText(Log.path).split("\n").filter(l => l.includes("first picture")).pop())
                check("and how far behind its picture it is heard", logged(/The sound of "i-with-sound\.mp4" is heard at least \d+ ms after it is played/).length === 1,
                      logged(/is heard at least/).join(" | "))
                // A long dissolve to a silent background: the foreground's sound goes down with its picture.
                kept.sound = feedOf(player())
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
                const now = kept.sound ? kept.sound.gain : -1
                check("half way through the dissolve its sound is part of the way down", now > 0.05 && now < 0.95, now + " after " + JSON.stringify(levels))
                return 1800
            },
            () => {
                sampler.stop()
                const falling = levels.every((level, i) => i === 0 || level <= levels[i - 1] + 0.001)
                check("having gone down all the way without going back up", falling && levels[0] > 0.9 && levels.some(level => level < 0.2), JSON.stringify(levels))
                check("and the background that took its place is silent", player() !== null && player() !== kept.foregroundPlayer && player().audioBufferOutput === null && liveMedia.name === kept.background)
                // And back: the foreground's sound comes up with its picture.
                kept.sound = null
                levels = []
                showMedia(row(), mediaPlaylistId)
                return 700
            },
            () => {
                const incoming = Lib.findAll(output.contentItem, item => item.content !== undefined && item.content !== null && item.content.name === "i-with-sound.mp4")
                check("the foreground is on its way in", incoming.length === 1 && incoming[0].player !== null && incoming[0].feed !== null, incoming.length)
                kept.sound = incoming[0].feed
                kept.level = incoming[0].level
                check("with its sound part of the way up, as its picture is", kept.sound.gain > 0.02 && kept.sound.gain < 0.9 && Math.abs(kept.sound.gain - kept.level) < 0.05, kept.sound.gain + " at level " + kept.level)
                return 2200
            },
            () => {
                check("and at full when the dissolve is done", Math.abs(kept.sound.gain - 1) < 0.001 && player() !== null && feedOf(player()) === kept.sound, kept.sound.gain)
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
                check("which the slide plays", player() !== null && feedOf(player()) !== null && feedOf(player()).gain === 1 && player().position > 200, player() ? player().position : "none")
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
