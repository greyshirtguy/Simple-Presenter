import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Video decoded by the processor alone. The app's settings are given that choice before it starts (run.py,
// prepare_softdecode), which is the only time the setting is acted on; the test then plays one video and changes it
// for another, looking at what the log says of how each one's pictures arrived and at what the output draws, and
// chooses otherwise in the settings, which is for the next start. What the settings file then holds is looked at when
// the test is over (after_softdecode).
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function pictures() {
        return logged(/  media +video ".*": first picture after \d+ ms; /)
    }

    function part(c, i) {
        return parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                check("the setting that was saved is acted on as the app starts: no hardware video decoder is to be tried", VideoDecoding.softwareOnly
                      && VideoDecoding.inWords === "by the processor only, as the settings ask", VideoDecoding.inWords)
                check("the log says so, near its start", logged(/  video +video is decoded by the processor only, as the settings ask \(hardware decoding is not tried\)$/).length === 1,
                      logged(/  video  /).join(" | "))
                check("and the window has the setting as it was saved", videoDecoding === "processor" && !VideoDecoding.guarded && !VideoDecoding.guardInForce)
                check("nothing is put up about it: it was asked for", decodingNotice === null)
                const videos = mediaFiles.filter(m => m.video)
                kept.first = videos[0]
                kept.second = videos.find(m => m.path !== videos[0].path)
                check("(the media bin has two videos to play)", kept.first !== undefined && kept.second !== undefined, videos.length + " videos")
                showMedia(kept.first, mediaPlaylistId)
                return 3000
            },
            () => {
                const said = pictures()
                check("a video plays", liveMedia !== null && liveMedia.path === kept.first.path && player() !== null && player().playbackState === MediaPlayer.PlayingState
                      && player().position > 0, player() ? player().position : "no player")
                check("its pictures arrive in memory, in the form FFmpeg's own decoder gives them in, and the log says there is no doubt who worked them out",
                      said.length === 1 && /, YUV420P\w*, frames arriving in memory \(decoded by the processor: hardware decoding is not tried\)/.test(said[0]), said.join(" | "))
                check("and are drawn on the output, by the graphics chip as ever", testInput.lit(false, 0, 0, 1, 1) > 0.05, testInput.lit(false, 0, 0, 1, 1))
                testInput.grabOutput("1-first-video")
                showMedia(kept.second, mediaPlaylistId)
                return 3000
            },
            () => {
                const said = pictures()
                check("changed for another video, that one plays too", liveMedia !== null && liveMedia.path === kept.second.path && player().playbackState === MediaPlayer.PlayingState
                      && player().position > 0, player() ? player().position : "no player")
                check("with its pictures arriving the same way, and neither kept waiting for", said.length === 2
                      && said[1].includes("frames arriving in memory (decoded by the processor: hardware decoding is not tried)")
                      && logged(/has no picture after a second/).length === 0, said.join(" | "))
                check("and drawn", testInput.lit(false, 0, 0, 1, 1) > 0.05, testInput.lit(false, 0, 0, 1, 1))
                testInput.grabOutput("2-second-video")
                clearMedia()
                // ---- The setting
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "video"
                return 500
            },
            () => {
                const part = named("videoSettings")
                const note = named("videoDecodingNote")
                const chosen = ["auto", "processor", "hardware"].filter(mode => named("decodingChoice:" + mode).chosen)
                // (The round mark of a choice: orange in its middle when it is the one chosen.)
                const mark = (mode) => { const at = named("decodingChoice:" + mode).mapToItem(null, 9, 11); return String(testInput.windowPixel(at.x, at.y)) }
                kept.mark = mark
                check("the settings have a part for video, with three ways it can be decoded and the one that was saved chosen, drawn so", part !== null && part.visible
                      && chosen.join() === "processor" && t.part(mark("processor"), 0) > 200 && t.part(mark("processor"), 2) < 90
                      && t.part(mark("auto"), 0) < 90 && t.part(mark("hardware"), 0) < 90, chosen.join() + " " + mark("processor") + " " + mark("auto"))
                const title = Lib.find(named("decodingChoice:processor"), item => item.font !== undefined && item.text === "By the processor only")
                check("each says what it is, in words that are drawn", title !== null && strokes(title).light >= 3
                      && Lib.find(named("decodingChoice:auto"), item => item.font !== undefined && item.text === "Automatically") !== null
                      && Lib.find(named("decodingChoice:hardware"), item => item.font !== undefined && String(item.text).startsWith("By the graphics chip")) !== null)
                check("and under them how this run is decoding, with nothing said of the computer, the guard having nothing to say of it",
                      note.visible && note.text.includes("A change takes effect the next time the app starts.")
                      && note.text.endsWith("Decoding now: by the processor only, as the settings ask."), note.text.slice(-120))
                testInput.grab("3-setting")
                click(centre(named("decodingChoice:auto")))
                return 500
            },
            () => {
                check("a click on another chooses that one, drawn so", videoDecoding === "auto" && named("decodingChoice:auto").chosen && !named("decodingChoice:processor").chosen
                      && t.part(kept.mark("auto"), 0) > 200 && t.part(kept.mark("processor"), 0) < 90, kept.mark("auto"))
                check("which is for the next start: this run goes on decoding as it began, and its words still say so", VideoDecoding.softwareOnly
                      && named("videoDecodingNote").text.endsWith("Decoding now: by the processor only, as the settings ask."))
                check("the log has a line for the change", logged(/  settings +video decoding set to automatic, from the next start$/).length === 1)
                settingsOpen = false
                takeFocus()
            }
        ]
        next()
    }
}
