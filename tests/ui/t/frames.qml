import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// How often the output is drawn as slides change (qml/TransitionLayer.qml), on the Demo workspace's "All Hail King
// Jesus", on slides whose words have no shadow (a shadow is drawn through layers of Qt's, which have Qt draw two
// frames more after a change, whatever this app does): a slide shown by a cut, with nothing playing, is drawn once,
// and not a second time a moment later for the textures a transition would be drawn from, which on a screen playing
// a video took the video's turn and had it stutter; five cuts while a video from the media bin plays under them draw
// a frame of their own each at most (VideoTiming, src/benchmark.h, counts them), and not two each; and a dissolve
// with nothing playing is drawn at the screen's refreshes, a frame at each for as long as it lasts, and ends when it
// should.
QtObject {
    id: t

//COMMON
    // A slide of the open presentation, from `from` on, that has no media, no actions (which may start some), no
    // scrolling words and no shadows, and whose next has none either: -1 if there is none
    function stillPair(from) {
        const still = slide => slide && !slide.media && slide.actions.length === 0
                               && !slide.elements.some(e => e.scroll !== undefined || e.textShadowEnabled || e.shadowEnabled)
        for (let i = from; i + 1 < document.slides.length; ++i) {
            if (still(document.slides[i]) && still(document.slides[i + 1]))
                return i
        }
        return -1
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "Demo").path)
                openEntry(entry("All Hail King Jesus"))
                return 700
            },
            () => {
                kept.index = stillPair(1)
                check("the song has two slides together with nothing on them that moves, and no shadows", kept.index >= 0, document ? document.slides.length : "none")
                selectTransition("Cut")
                clearMedia()
                goLive(kept.index)
                return 800
            },
            () => {
                check("(and nothing playing on the output)", output.liveVideoSink === null)
                kept.frames = Log.frames(output)
                goLive(kept.index + 1)
                return 600
            },
            () => {
                const drawn = Log.frames(output) - kept.frames
                check("a slide shown by a cut is drawn on the output once", drawn === 1, drawn)
                // A video from the media bin, under the slides
                for (const list of catalog.mediaPlaylists.filter(p => !p.folder)) {
                    openMediaPlaylist(list.path)
                    if (mediaFiles.some(m => m.video))
                        break
                }
                kept.video = mediaFiles.find(m => m.video)
                if (kept.video)
                    showMedia(kept.video, mediaPlaylistId)
                return 2500
            },
            () => {
                check("(a video from the media bin is playing on the output)", output.liveVideoSink !== null, kept.video ? kept.video.name : "none")
                testInput.watchVideo()
                // Five cuts in this one step, a little apart, between slides with nothing of their own that moves
                kept.cuts = 0
                kept.timer = Qt.createQmlObject("import QtQuick; Timer { interval: 400; repeat: true }", t)
                kept.cutAt = []
                kept.timer.triggered.connect(() => {
                    kept.cutAt.push(testInput.sinceStart())
                    goLive(kept.cuts % 2 === 0 ? kept.index : kept.index + 1)
                    if (++kept.cuts === 5)
                        kept.timer.stop()
                })
                kept.timer.start()
                return 2600
            },
            () => {
                const video = testInput.videoSoFar()
                const after = (video.othersAt ?? []).map(at => {
                    const before = kept.cutAt.filter(c => c <= at)
                    return before.length > 0 ? "+" + (at - before[before.length - 1]).toFixed(1) : "-"
                })
                check("five cuts over a playing video draw a frame of their own each at most (and perhaps one for something else): not two each",
                      kept.cuts === 5 && video.frames > 50 && video.others <= 6, kept.cuts + " cuts; others, ms after their cut: " + after.join(" ")
                      + "; held " + video.held + ", not shown " + video.notShown)
                kept.timer.destroy()
                clearMedia()
                goLive(kept.index)
                return 800
            },
            () => {
                check("(and nothing playing on the output again)", output.liveVideoSink === null)
                selectTransition("Dissolve")
                transitionDuration = 0.5
                kept.frames = Log.frames(output)
                goLive(kept.index + 1)
                return 900
            },
            () => {
                const drawn = Log.frames(output) - kept.frames
                const done = testInput.readText(Log.path).split("\n").filter(l => l.includes("slide layer: Dissolve done:"))
                const last = done.length > 0 ? done[done.length - 1] : ""
                const parts = last.match(/done: (\d+) frames in ([\d.]+) s/) ?? []
                check("a dissolve of half a second is drawn at the screen's refreshes: a frame at each, and no more", drawn >= 25 && drawn <= 35, drawn)
                check("and ends when it should, in as many", Number(parts[2]) >= 0.48 && Number(parts[2]) <= 0.56 && Number(parts[1]) >= 25 && Number(parts[1]) <= 34,
                      last.slice(-60))
                selectTransition("Cut")
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
