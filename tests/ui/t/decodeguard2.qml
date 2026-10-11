import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The guard on hardware video decoding, at the next start on the computer of the test decodeguard: the notice has been
// shown, and the graphics chip was chosen all the same (run.py, prepare_decodeguard2). So the guard does not hold,
// nothing is put up, and a video is decoded by whatever this computer has; and the settings still say what the app
// makes of the computer, for whoever wants the guard back. That is the most the Video part of the settings ever has
// to say, so it is here that a window as short as it can be made is tried: the words then scroll, and none is drawn
// past the sheet.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // The Video part of the settings as it is drawn: how far its words would go past the end of the sheet if they
    // were all drawn (less than nothing where they fit), how much of the sheet under where they start is words
    // (lighter than anything there that is not), and how much of the window below the sheet is as light as words
    // are. (The window behind the sheet is dimmed, which leaves nothing of it that light.) And how much of where a
    // scroll bar would start, at the right of the part, is as light as one.
    function wordsDrawn() {
        const column = named("videoSettings")
        const sheet = named("settingsSheet")
        const view = column.parent.parent
        const corner = column.mapToItem(null, 0, 0)
        const end = sheet.mapToItem(null, 0, sheet.height).y
        const edge = view.mapToItem(null, view.width, 0)
        return { over: corner.y + column.height - end, room: win.height - end - 2,
                 inside: testInput.windowLighter(corner.x, corner.y, column.width, end - corner.y - 2, 120),
                 below: testInput.windowLighter(corner.x, end + 2, column.width, win.height - end - 2, 120),
                 bar: testInput.windowLighter(edge.x - 8, edge.y + 4, 6, 40, 120) }
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                check("with the graphics chip chosen in the settings, the guard does not hold: hardware decoders are tried", VideoDecoding.guarded && !VideoDecoding.guardInForce
                      && !VideoDecoding.softwareOnly && VideoDecoding.overridden && videoDecoding === "hardware", VideoDecoding.inWords)
                const lines = logged(/  video  /).map(line => line.substring(26))
                check("the log says so, and that it is against what was found of the computer", lines.length === 2
                      && lines[1] === "video is decoded by the graphics chip where its driver offers that, and otherwise by the processor: the settings ask for that, "
                                      + "although the graphics card is run by the nouveau driver (its chip is NVE4), and the firmware that driver's video decoder needs is not installed",
                      lines.join(" | "))
                check("no notice is put up: it has been shown, and there is nothing held back to tell of", decodingNotice === null && !decodingNoticeOpen && decodingNoticeSeen)
                kept.video = mediaFiles.find(m => m.video)
                showMedia(kept.video, mediaPlaylistId)
                return 3000
            },
            () => {
                const said = logged(/  media +video ".*": first picture after \d+ ms; /)
                check("a video plays, decoded by whatever this computer has, the log no longer saying that hardware decoding is not tried", liveMedia !== null
                      && player() !== null && player().playbackState === MediaPlayer.PlayingState && said.length === 1 && !said[0].includes("hardware decoding is not tried"),
                      said.join(" | "))
                testInput.say("  " + (said.length ? said[0].substring(26) : "no line for the video"))
                clearMedia()
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "video"
                return 500
            },
            () => {
                const note = named("videoDecodingNote")
                check("the settings have the graphics chip chosen, and still say what the app makes of this computer", named("decodingChoice:hardware").chosen
                      && note.text.includes("Decoding now: by the graphics chip where its driver offers that, and otherwise by the processor: the settings ask for that, although "
                                            + "the graphics card is run by the nouveau driver (its chip is NVE4)")
                      && note.text.includes("which is why Automatically leaves it alone here"), note.text.slice(-260))
                testInput.grab("1-overridden")
                const drawn = wordsDrawn()
                check("in a window of the usual size the words all fit, and are drawn on the sheet and nowhere under it, with no scroll bar", drawn.over < 0
                      && drawn.inside > 0.005 && drawn.below === 0 && drawn.room > 20 && drawn.bar === 0,
                      drawn.inside.toFixed(4) + " inside, " + drawn.below.toFixed(4) + " below, over by " + drawn.over.toFixed(0) + ", bar " + drawn.bar.toFixed(2))
                kept.height = win.height
                win.height = win.minimumHeight
                return 700
            },
            () => {
                const drawn = wordsDrawn()
                check("in a window as short as it can be made they do not all fit", win.height === win.minimumHeight && drawn.over > 40, win.height + ": over by " + drawn.over.toFixed(0))
                check("and are still drawn on the sheet and nowhere under it", drawn.inside > 0.005 && drawn.below === 0 && drawn.room > 20,
                      drawn.inside.toFixed(4) + " inside, " + drawn.below.toFixed(4) + " below, in " + drawn.room.toFixed(0))
                check("the part scrolls, and a bar at its right is drawn to say so", drawn.bar > 0.5, "bar " + drawn.bar.toFixed(2))
                testInput.grab("2-short")
                win.height = kept.height
                return 700
            },
            () => {
                check("the window is as tall as it was", win.height === kept.height, win.height)
                click(centre(named("decodingChoice:auto")))
                return 500
            },
            () => {
                check("Automatically can be chosen again, which puts the guard back from the next start", videoDecoding === "auto"
                      && logged(/  settings +video decoding set to automatic, from the next start$/).length === 1)
                settingsOpen = false
                takeFocus()
            }
        ]
        next()
    }
}
