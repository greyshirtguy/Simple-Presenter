import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The tabs over the transport (qml/TransportTabs.qml) and the media bin's Next Cue (src/mediacue.h, the show's rule in
// show::State::mediaFinished), on the test's own media bin (prepare_transports in run.py): a playlist "Run" of three
// videos of three seconds (red going round, green and blue stopping), a long white one going round, and a still; an
// audio bin with a long tune and a sound effect; and a presentation "Notices".
//
// Next Cue: the menu of a video row, its choices and the playlist's other rows; Red given Next (and so made to stop at
// its end), Green given Blue, Blue given First; a still has none. Then Red clicked, and the run on the output and in the
// operator's preview: red, then green, then blue (the one Green names), then red again (the first); Red then given
// Random, and something else following it. The transport: its three tabs, pictures and no words; the media's working
// the long video, paused and played from it; Notices sent to the announcements with the long video under its slide,
// the announcements' tab lit and working that video and not the other; the long tune from the audio bin, its tab
// working it, a sound effect over it not taking its place, and the tune paused and played again; and with everything
// cleared, nothing to work. after_transports in run.py reads the media bin's file.
QtObject {
    id: t

//COMMON
    readonly property string runList: "0E0E0E0E-0000-0000-0000-000000000001"
    readonly property string tunes: "0A0A0A0A-0000-0000-0000-000000000001"
    readonly property string effects: "0A0A0A0A-0000-0000-0000-000000000002"

    function texts(root) {
        return Lib.findAll(root, item => item.text !== undefined && item.font !== undefined && item.visible && typeof item.text === "string" && item.text !== "").map(item => item.text)
    }

    function hover(label) {
        const row = menuRow(label)
        testInput.mouse(4, row.x, row.y)
    }

    function row(name) {
        return mediaFiles.find(m => m.name === name)
    }

    function cell(name) {
        return binCell(m => m.name === name)
    }

    function tab(layer) {
        return named("transportTab:" + layer)
    }

    function transport() {
        return named("transport")
    }

    // Runs again a tenth of a second later until `ready()` holds, or for at most `ms`; then `then()`
    function until(ready, ms, then) {
        return () => {
            if (kept.since === undefined)
                kept.since = testInput.now()
            if (!ready() && testInput.now() - kept.since < ms) {
                --t.at
                return 100
            }
            kept.waited = testInput.now() - kept.since
            kept.since = undefined
            return then()
        }
    }

    function outputAt(fx, fy) {
        return String(testInput.pixel(false, fx, fy))
    }

    function previewAt(fx, fy) {
        const picture = Lib.find(win.contentItem, item => item.namePrefix === "preview" && item.screenId !== undefined)
        const aspect = output.width / output.height
        const w = Math.min(picture.width, picture.height * aspect)
        const h = w / aspect
        const p = picture.mapToItem(null, (picture.width - w) / 2 + fx * w, (picture.height - h) / 2 + fy * h)
        return String(testInput.windowPixel(p.x, p.y))
    }

    function part(c, i) {
        return parseInt(String(c).substring(1 + 2 * i, 3 + 2 * i), 16)
    }

    // Whether a colour is near this one, each part within 60
    function like(c, r, g, b) {
        return Math.abs(part(c, 0) - r) < 60 && Math.abs(part(c, 1) - g) < 60 && Math.abs(part(c, 2) - b) < 60
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function buttonOf(kind) {
        return Lib.find(transport(), item => item.kind === kind && item.available !== undefined && item.visible)
    }

    // Plays a row of the audio bin, by its file, as a click on it does
    function playRow(playlist, file) {
        playAudio(catalog.audioIn(playlist).find(r => String(r.path).endsWith("/" + file)), playlist)
    }

    function run() {
        steps = [
            // ---- The tabs as they start
            () => {
                const tabs = ["media", "announcements", "audio"].map(tab)
                check("over the transport are three tabs, pictures and no words", tabs.every(x => x !== null && x.visible && texts(x).length === 0)
                      && tabs.every(x => x.mapToItem(null, 0, 0).y < transport().mapToItem(null, 0, 0).y))
                check("the media's, a picture in a frame drawn here; the announcements', ProPresenter's megaphone; the audio's, the audio bin's notes",
                      Lib.findAll(tabs[0], i => i.preferredRendererType !== undefined && i.visible).length === 1
                      && Lib.find(tabs[1], i => i.inkName !== undefined && i.name === "Announcements" && i.status === Image.Ready && i.visible) !== null
                      && Lib.find(tabs[2], i => i.kind === "music" && i.size !== undefined && i.visible) !== null)
                check("the media's is picked, and none has anything playing", tabs[0].picked && !tabs[1].picked && !tabs[2].picked && tabs.every(x => !x.live)
                      && texts(transport()).includes("No media"), texts(transport()).join(" | "))
                const corner = tabs[0].mapToItem(null, 3, tabs[0].height / 2)
                const other = tabs[1].mapToItem(null, 3, tabs[1].height / 2)
                const ground = String(testInput.windowPixel(corner.x, corner.y))
                const otherGround = String(testInput.windowPixel(other.x, other.y))
                check("drawn so: the picked one on a lighter ground than the others", part(ground, 0) > part(otherGround, 0) + 20, ground + " " + otherGround)
                openMediaPlaylist(t.runList)
                return 800
            },
            // ---- Next Cue
            () => {
                check("(the test's playlist, with its five rows)", mediaFiles.map(m => m.name).join() === "Red.mp4,Green.mp4,Blue.mp4,Long.mp4,Still.png",
                      mediaFiles.map(m => m.name).join())
                check("Red goes round, and is followed by nothing", row("Red.mp4").loops && (row("Red.mp4").next ?? "") === "")
                click(centre(cell("Red.mp4")), Qt.RightButton)
                return 400
            },
            () => {
                check("a video's menu has Next Cue, after the Inspector", /Inspector, Next Cue, \[Playlist\]/.test(labels()), labels())
                hover("Next Cue")
                return 500
            },
            () => {
                check("which has None (ticked), Next, Random and First, a line, and the playlist's other rows",
                      labels() === "[Next Cue], *None, Next, Random, First, --, Green.mp4, Blue.mp4, Long.mp4, Still.png", labels())
                click(menuRow("Next"))
                return 700
            },
            () => {
                const red = row("Red.mp4")
                check("Next chosen: Red goes on to the next, and stops at its end now", red.next === "next" && red.playback === 0 && !red.loops, JSON.stringify(red))
                check("what was done is in the log", logged(/ "Red.mp4" in the media bin goes on to the next/).length === 1)
                click(centre(cell("Green.mp4")), Qt.RightButton)
                return 400
            },
            () => {
                hover("Next Cue")
                return 500
            },
            () => {
                check("(Green's: its other rows are the playlist's but its own)", labels() === "[Next Cue], *None, Next, Random, First, --, Red.mp4, Blue.mp4, Long.mp4, Still.png", labels())
                click(menuRow("Blue.mp4"))
                return 700
            },
            () => {
                const green = row("Green.mp4")
                check("a row chosen: Green goes on to Blue, named by Blue's cue", green.next === "cue" && green.nextCue === row("Blue.mp4").cueId, JSON.stringify(green))
                click(centre(cell("Blue.mp4")), Qt.RightButton)
                return 400
            },
            () => {
                hover("Next Cue")
                return 500
            },
            () => {
                click(menuRow("First"))
                return 700
            },
            () => {
                check("Blue goes on to the first", row("Blue.mp4").next === "first")
                click(centre(cell("Green.mp4")), Qt.RightButton)
                return 400
            },
            () => {
                hover("Next Cue")
                return 500
            },
            () => {
                check("the menu ticks what a row has", labels().includes("*Blue.mp4") && !labels().includes("*None"), labels())
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                click(centre(cell("Still.png")), Qt.RightButton)
                return 400
            },
            () => {
                check("a still has no Next Cue: it never comes to an end", labels() !== "" && !labels().includes("Next Cue"), labels())
                testInput.key(Qt.Key_Escape)
                return 300
            },
            // ---- The run on the output
            () => {
                click(centre(cell("Red.mp4")))
                return 1200
            },
            () => {
                check("Red clicked is on the output, from its playlist", liveMedia !== null && liveMedia.name === "Red.mp4" && Show.liveMediaPlaylistId === t.runList
                      && like(outputAt(0.5, 0.5), 255, 0, 0), (liveMedia ? liveMedia.name : "none") + " " + outputAt(0.5, 0.5))
                return 100
            },
            until(() => liveMedia !== null && liveMedia.name === "Green.mp4", 6000, () => 900),
            () => {
                check("at its end Green follows it, on the output and in the operator's preview", liveMedia.name === "Green.mp4" && like(outputAt(0.5, 0.5), 0, 255, 0)
                      && like(previewAt(0.5, 0.5), 0, 255, 0), liveMedia.name + " " + outputAt(0.5, 0.5) + " " + previewAt(0.5, 0.5))
                check("and the log says so", logged(/"Red.mp4" has played to its end, and goes on to the next/).length === 1)
                testInput.grabOutput("1-green")
                testInput.grab("1-operator")
                return 100
            },
            until(() => liveMedia !== null && liveMedia.name !== "Green.mp4", 6000, () => 900),
            () => {
                check("then Blue, which Green names", liveMedia.name === "Blue.mp4" && like(outputAt(0.5, 0.5), 0, 0, 255), liveMedia.name + " " + outputAt(0.5, 0.5))
                return 100
            },
            until(() => liveMedia !== null && liveMedia.name !== "Blue.mp4", 6000, () => 900),
            () => {
                check("then Red again, the first", liveMedia.name === "Red.mp4" && like(outputAt(0.5, 0.5), 255, 0, 0) && Show.liveMediaPlaylistId === t.runList,
                      liveMedia.name + " " + outputAt(0.5, 0.5))
                // Red to Random (from its menu), to see something else follow it
                click(centre(cell("Long.mp4")))
                return 800
            },
            () => {
                click(centre(cell("Red.mp4")), Qt.RightButton)
                return 400
            },
            () => {
                hover("Next Cue")
                return 500
            },
            () => {
                click(menuRow("Random"))
                return 700
            },
            () => {
                check("(Red to Random)", row("Red.mp4").next === "random")
                click(centre(cell("Red.mp4")))
                return 1200
            },
            until(() => liveMedia !== null && liveMedia.name !== "Red.mp4", 6000, () => 600),
            () => {
                check("Random: another of the playlist's rows follows it", liveMedia !== null && liveMedia.name !== "Red.mp4"
                      && ["Green.mp4", "Blue.mp4", "Long.mp4", "Still.png"].includes(liveMedia.name), liveMedia ? liveMedia.name : "none")
                // ---- The transport: the media's, working the long video
                click(centre(cell("Long.mp4")))
                return 1500
            },
            () => {
                check("the long video on the media layer: the media's tab is lit, and the transport works it", tab("media").live && transport().player === output.livePlayer
                      && transport().working && texts(transport()).includes("Long.mp4"), texts(transport()).join(" | "))
                click(centre(buttonOf("pause")))
                return 600
            },
            () => {
                check("its pause pauses the video", output.livePlayer.playbackState === MediaPlayer.PausedState)
                click(centre(buttonOf("play")))
                return 500
            },
            () => {
                check("and play plays it again", output.livePlayer.playbackState === MediaPlayer.PlayingState)
                // ---- The announcements': Notices, with the long video under its first slide
                openEntry(entry("Notices"))
                return 600
            },
            () => {
                setDestination(entry("Notices"), "announcements")
                return 600
            },
            () => {
                assignMedia(0, row("Long.mp4").path)
                return 800
            },
            () => {
                click(centre(slideCell(0)))
                return 100
            },
            until(() => sidePanel.liveAnnouncementPlayer !== null && Show.liveAnnouncementMedia !== null, 5000, () => 800),
            () => {
                check("Notices on the announcements layer, with the long video: the announcements' tab is lit too", tab("announcements").live && tab("media").live
                      && Show.liveAnnouncementMedia.name === "Long.mp4", Show.liveAnnouncementMedia ? Show.liveAnnouncementMedia.name : "none")
                check("but the transport still works the media layer's, its tab being picked", transport().player === output.livePlayer)
                click(centre(tab("announcements")))
                return 400
            },
            () => {
                check("the announcements' tab picked, the transport works their video", tab("announcements").picked && transport().player === sidePanel.liveAnnouncementPlayer
                      && transport().player !== output.livePlayer && transport().working && texts(transport()).includes("Long.mp4"), texts(transport()).join(" | "))
                testInput.grab("2-announcements")
                click(centre(buttonOf("pause")))
                return 600
            },
            () => {
                check("its pause pauses their video, and not the media layer's", sidePanel.liveAnnouncementPlayer.playbackState === MediaPlayer.PausedState
                      && output.livePlayer.playbackState === MediaPlayer.PlayingState)
                click(centre(buttonOf("play")))
                return 500
            },
            () => {
                check("(and play plays it again)", sidePanel.liveAnnouncementPlayer.playbackState === MediaPlayer.PlayingState)
                // ---- The audio's: the long tune
                playRow(t.tunes, "Long Tune.wav")
                return 100
            },
            until(() => AudioLayer.musicPlayer !== null && AudioLayer.musicDuration > 0, 5000, () => 600),
            () => {
                check("the long tune playing: the audio's tab is lit", tab("audio").live && Show.liveMusic !== null && String(Show.liveMusic.path).endsWith("/Long Tune.wav"))
                click(centre(tab("audio")))
                return 1200
            },
            () => {
                check("its tab picked, the transport works the music", tab("audio").picked && transport().player === AudioLayer.musicPlayer && transport().working
                      && texts(transport()).includes(Show.liveMusic.name) && Math.abs(transport().duration - 30000) < 500, texts(transport()).join(" | ") + " " + transport().duration)
                testInput.grab("3-audio")
                playRow(t.effects, "Bell.wav")
                return 600
            },
            () => {
                check("a sound effect over it does not take its place in the transport", Show.liveSounds.length === 1 && texts(transport()).includes(Show.liveMusic.name),
                      Show.liveSounds.length + " " + texts(transport()).join(" | "))
                click(centre(buttonOf("pause")))
                return 500
            },
            () => {
                check("its pause pauses the music", AudioLayer.musicPlayer.playbackState === MediaPlayer.PausedState)
                kept.at = AudioLayer.musicPosition
                return 1000
            },
            () => {
                check("which then stays where it is", Math.abs(AudioLayer.musicPosition - kept.at) < 0.05 && Show.liveMusic !== null, kept.at + " then " + AudioLayer.musicPosition)
                click(centre(buttonOf("play")))
                return 800
            },
            () => {
                check("and play plays it on from there", AudioLayer.musicPlayer.playbackState === MediaPlayer.PlayingState && AudioLayer.musicPosition > kept.at + 0.2,
                      kept.at + " then " + AudioLayer.musicPosition)
                // ---- Everything cleared
                click(centre(named("clearGroup")))
                return 2500
            },
            () => {
                check("with everything cleared, no tab is lit, and the transport has nothing to work", ["media", "announcements", "audio"].every(k => !tab(k).live)
                      && !transport().working && texts(transport()).includes("No music"), texts(transport()).join(" | "))
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
