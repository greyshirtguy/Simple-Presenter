import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The transport: follows the video on the output; play and pause, back to the start,
// fifteen seconds either way, and the slider. And what it costs.
QtObject {
    id: t

//COMMON
    property int positions: 0
    property int frames: 0
    property Connections positionWatch: Connections {
        target: output ? output.livePlayer : null
        function onPositionChanged() { ++t.positions }
    }
    property Connections frameWatch: Connections {
        target: win
        function onFrameSwapped() { ++t.frames }
    }

    function transport() {
        return Lib.find(win.contentItem, item => item.shown !== undefined && item.working !== undefined)
    }

    function button(kind) {
        return Lib.find(transport(), item => item.kind === kind && item.available !== undefined)
    }

    function textButton(text) {
        return Lib.find(transport(), item => item.text === text && item.available !== undefined)
    }

    function slider() {
        return Lib.find(transport(), item => item.visualPosition !== undefined && item.from !== undefined)
    }

    function measure(label) {
        const now = testInput.now()
        const cpu = testInput.cpu()
        if (kept.since !== undefined) {
            const seconds = (now - kept.since) / 1000
            testInput.say("  measure " + label + ": " + ((cpu - kept.cpu) / 10 / seconds).toFixed(1) + "% of a core, "
                          + (positions / seconds).toFixed(1) + " position changes/s, " + (frames / seconds).toFixed(1) + " operator frames/s  (over " + seconds.toFixed(1) + " s)")
        }
        kept.since = now
        kept.cpu = cpu
        positions = 0
        frames = 0
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Move Of God"))
                const tr = transport()
                check("there is a transport", tr !== null && button("play") !== null && button("restart") !== null && textButton("−15") !== null && textButton("+15") !== null && slider() !== null)
                check("with nothing on the output it has nothing to work", !tr.working && !button("play").available && !slider().enabled)
                testInput.grab("1-idle")
                // A still
                showMedia(mediaFiles.find(m => !m.video), mediaPlaylistId)
                return 1200
            },
            () => {
                check("a still gives it nothing to work either", liveMedia !== null && !transport().working && !button("play").available)
                goLive(6)
                return 4000
            },
            () => {
                const tr = transport()
                check("a video makes it work", tr.working && tr.playing && button("pause") !== null && button("pause").available && slider().enabled)
                check("the slider follows the video", Math.abs(slider().value - player().position) < 700 && slider().to === player().duration, slider().value + " / " + player().position)
                kept.position = player().position
                click(centre(button("pause")))
                return 700
            },
            () => {
                check("pause pauses", player().playbackState === MediaPlayer.PausedState && !transport().playing && button("play") !== null)
                kept.position = player().position
                check("and the slider shows exactly where", slider().value === player().position, slider().value + " / " + player().position)
                return 1200
            },
            () => {
                check("and it stays where it is", Math.abs(player().position - kept.position) < 50, kept.position + " -> " + player().position)
                testInput.grab("2-paused")
                click(centre(button("play")))
                return 1500
            },
            () => {
                check("play plays on from there", player().playbackState === MediaPlayer.PlayingState && player().position > kept.position + 700 && player().position < kept.position + 2600,
                      kept.position + " -> " + player().position)
                kept.position = player().position
                click(centre(textButton("+15")))
                return 1500
            },
            () => {
                check("+15 skips on fifteen seconds", player().position > kept.position + 14000 && player().position < kept.position + 18500, kept.position + " -> " + player().position)
                kept.position = player().position
                click(centre(textButton("−15")))
                return 1500
            },
            () => {
                check("−15 skips back fifteen", player().position > kept.position - 15500 && player().position < kept.position - 11500, kept.position + " -> " + player().position)
                click(centre(button("restart")))
                return 1500
            },
            () => {
                check("restart goes back to the start and plays", player().position < 2600 && player().playbackState === MediaPlayer.PlayingState, player().position)
                // Drag the slider to two thirds
                const s = slider()
                const from = s.mapToItem(null, s.leftPadding + s.visualPosition * (s.availableWidth - 16) + 8, s.height / 2)
                const to = s.mapToItem(null, s.leftPadding + s.availableWidth * 2 / 3, s.height / 2)
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y)
                kept.dragged = s.value
                kept.during = player().position
                check("while the slider is held the times show where it is held", s.pressed && s.value > player().duration * 0.55, s.value + " of " + player().duration)
                testInput.grab("3-dragging")
                testInput.mouse(2, to.x, to.y)
                check("let go, the slider stays where it was let go", Math.abs(slider().value - kept.dragged) < 700, kept.dragged + " -> " + slider().value)
                return 1500
            },
            () => {
                check("the video was not moved until the slider was let go", kept.during < 6000, kept.during)
                check("and then goes to where it was let go", player().position > kept.dragged - 500 && player().position < kept.dragged + 3000, kept.dragged + " -> " + player().position)
                check("with the slider following it again", !slider().pressed && Math.abs(slider().value - player().position) < 700, slider().value + " / " + player().position)
                // -15 at the start stops at the start; +15 near the end stops at the end
                player().position = 3000
                return 800
            },
            () => {
                click(centre(textButton("−15")))
                return 1000
            },
            () => {
                check("−15 near the start goes to the start", player().position < 2500, player().position)
                // ---- what it costs
                goLive(0)
                return 5000
            },
            () => {
                check("the 4K background is playing", liveMedia.name.startsWith("Hopeful Horizon") && player().playbackState === MediaPlayer.PlayingState)
                measure("start")
                return 10000
            },
            () => {
                measure("4K video, transport showing")
                transport().player = null
                return 1000
            },
            () => {
                measure("(settling)")
                return 10000
            },
            () => {
                measure("4K video, transport cut off from the player")
                transport().player = Qt.binding(() => output ? output.livePlayer : null)
                // A foreground, to its end: the transport then offers to play it again
                setMediaItemForeground(mediaFiles.find(m => m.name.startsWith("Lava Blast")).id, true)
                showMedia(mediaFiles.find(m => m.name.startsWith("Lava Blast")), mediaPlaylistId)
                return 3000
            },
            () => {
                player().position = player().duration - 1500
                return 3500
            },
            () => {
                check("a video that has played once leaves the transport stopped at its end", transport().working && !transport().playing && button("play") !== null, player().position)
                testInput.grab("4-ended")
                click(centre(button("play")))
                return 2000
            },
            () => {
                check("and play plays it again from the start", player().playbackState === MediaPlayer.PlayingState && player().position > 300 && player().position < 4000, player().position)
                setMediaItemForeground(mediaFiles.find(m => m.name.startsWith("Lava Blast")).id, false)
                clearMedia()
                return 1200
            },
            () => {
                check("clearing the media leaves it with nothing to work", !transport().working && output.livePlayer === null)
                return 100
            }
        ]
        next()
    }
}
