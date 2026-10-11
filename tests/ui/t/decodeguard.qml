import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The guard on hardware video decoding, on a computer it holds: an NVIDIA chip run by nouveau, without the firmware
// its video decoder needs. The computer is made up (run.py, nouveau_system), and stands in for the real one only in
// what the guard looks at. The app's first run on it: the guard holds without being asked; a notice says so, once,
// and leads to the settings, where the graphics chip can be chosen all the same; and video is decoded by the
// processor meanwhile. The notice is then put up once more, as nothing in the app does, to be put away by a key. What
// the settings file then holds is looked at when the test is over (after_decodeguard); the next start on the same
// computer is the test decodeguard2.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function part(c, i) {
        return parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
    }

    function shown(item) {
        for (let at = item; at; at = at.parent) {
            if (!at.visible)
                return false
        }
        return item !== null
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                check("on such a computer the guard holds without being asked: no hardware video decoder is to be tried", VideoDecoding.guarded && VideoDecoding.guardInForce
                      && VideoDecoding.softwareOnly && !VideoDecoding.overridden && videoDecoding === "auto", VideoDecoding.inWords)
                check("it says why in words: the driver, the chip, and what is missing", VideoDecoding.guardReason
                      === "the graphics card is run by the nouveau driver (its chip is NVE4), and the firmware that driver's video decoder needs is not installed",
                      VideoDecoding.guardReason)
                const lines = logged(/  video  /).map(line => line.substring(26))
                check("the log says what was found of the computer, and what was decided", lines.length === 2
                      && lines[0] === "a hardware decoder would be looked for on /dev/dri/renderD128, which the driver \"nouveau\" runs; its chip is NVE4; "
                                      + "the firmware its video decoder needs is not installed (nve4_fuc084, nve4_fuc085, in /lib/firmware/nouveau)"
                      && lines[1] === "video is decoded by the processor only, because " + VideoDecoding.guardReason + " (hardware decoding is not tried)", lines.join(" | "))
                // ---- The notice
                const sheet = named("decodingNotice")
                const title = named("decodingNoticeTitle")
                const words = named("decodingNoticeWords")
                check("a notice is up, over the window, and has the keyboard", decodingNoticeOpen && shown(sheet) && decodingNotice.activeFocus)
                const corner = sheet.mapToItem(null, 8, 8)
                check("it is drawn: its sheet, its heading, and words under it", String(testInput.windowPixel(corner.x, corner.y)) === "#2b2d31"
                      && title.text === "Video is being decoded by the processor" && strokes(title).light >= 3 && shown(words),
                      String(testInput.windowPixel(corner.x, corner.y)))
                check("which say what was found on this computer, what is done about it, and where the choice is", words.text.startsWith("On this computer the graphics card is run by the nouveau driver (its chip is NVE4)")
                      && words.text.includes("so it is left alone: video is decoded by the processor") && words.text.includes("see the settings, under Video")
                      && words.text.endsWith("This is said once."), words.text.substring(0, 120))
                check("with a button that leads to that part of the settings, and one that only puts it away", shown(named("decodingNoticeSettings"))
                      && named("decodingNoticeSettings").text === "Video Settings…" && shown(named("decodingNoticeOk")) && named("decodingNoticeOk").text === "OK")
                check("the log has a line for its being shown", logged(/  notice +told that hardware video decoding is held back on this computer$/).length === 1)
                testInput.grab("1-notice")
                // A key of the show's does nothing while it is up, and a click beside it does not put it away.
                kept.live = liveIndex
                testInput.key(Qt.Key_Right)
                click(Qt.point(40, 200))
                return 400
            },
            () => {
                check("while it is up the show's keys do nothing, and a click beside it does not put it away", decodingNoticeOpen && liveIndex === kept.live && !decodingNoticeSeen)
                click(centre(named("decodingNoticeSettings")))
                return 600
            },
            () => {
                check("its button for the settings puts it away for good", !decodingNoticeOpen && !shown(named("decodingNotice")) && decodingNoticeSeen === true)
                const part = named("videoSettings")
                const note = named("videoDecodingNote")
                check("and opens the settings at the part for video, with Automatically chosen", settingsOpen && part !== null && shown(part)
                      && named("decodingChoice:auto").chosen && !named("decodingChoice:processor").chosen && !named("decodingChoice:hardware").chosen)
                check("where it says how this run is decoding and why, once, and what would change it", note.text.includes("Decoding now: by the processor only, because the graphics card "
                      + "is run by the nouveau driver (its chip is NVE4), and the firmware that driver's video decoder needs is not installed.")
                      && note.text.split("its chip is NVE4").length === 2 && note.text.includes("which is why Automatically leaves it alone here")
                      && note.text.includes("With the firmware put in"), note.text.slice(-260))
                testInput.grab("2-settings")
                click(centre(named("decodingChoice:hardware")))
                return 500
            },
            () => {
                const at = named("decodingChoice:hardware").mapToItem(null, 9, 11)
                const drawn = String(testInput.windowPixel(at.x, at.y))
                check("the way to decide otherwise is there: the graphics chip, whatever the app makes of this computer; chosen, and drawn so",
                      videoDecoding === "hardware" && named("decodingChoice:hardware").chosen && part(drawn, 0) > 200 && part(drawn, 2) < 90, drawn)
                check("which is for the next start: this run is still held", VideoDecoding.softwareOnly && VideoDecoding.guardInForce)
                check("the log has a line for the change", logged(/  settings +video decoding set to the graphics chip where it can, whatever the guard says, from the next start$/).length === 1)
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts the settings away: the keys were handed on when the notice went", !settingsOpen)
                // ---- Video meanwhile
                kept.video = mediaFiles.find(m => m.video)
                showMedia(kept.video, mediaPlaylistId)
                return 3000
            },
            () => {
                const said = logged(/  media +video ".*": first picture after \d+ ms; /)
                check("a video plays", liveMedia !== null && liveMedia.path === kept.video.path && player() !== null && player().playbackState === MediaPlayer.PlayingState
                      && player().position > 0, player() ? player().position : "no player")
                check("decoded by the processor, as the log says without doubt, its pictures in the form FFmpeg's own decoder gives them in", said.length === 1
                      && /, YUV420P\w*, frames arriving in memory \(decoded by the processor: hardware decoding is not tried\)/.test(said[0]), said.join(" | "))
                check("and drawn on the output", testInput.lit(false, 0, 0, 1, 1) > 0.05, testInput.lit(false, 0, 0, 1, 1))
                clearMedia()
                // ---- Put away by a key
                showDecodingNotice()
                return 400
            },
            () => {
                kept.live = liveIndex
                check("put up once more (which only a test does), it has the keyboard again", decodingNoticeOpen && shown(named("decodingNotice")) && decodingNotice.activeFocus)
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts it away, with nothing opened", !decodingNoticeOpen && !shown(named("decodingNotice")) && !settingsOpen)
                testInput.key(Qt.Key_Right)
                return 400
            },
            () => {
                check("and the keys are the show's again: Right takes a slide live", liveIndex >= 0 && liveIndex !== kept.live, liveIndex + " from " + kept.live)
            }
        ]
        next()
    }
}
