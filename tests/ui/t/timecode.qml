import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// Timecode (src/timecode.h, Timecode, TimecodeWindow.qml, Main's "The timecode"). A
// timeline's Timecode box and the timecode it starts at, written as ProPresenter keeps
// them, and what is typed that is no timecode refused. The Timecode window, opened by
// the clock beside them: its figures, its word (Not Engaged, Stopped, Playing), the
// playlist chosen and its presentations by the timecodes they start at, its settings.
// Then following, with the timecode made here: played, the timeline of the presentation
// it is in plays from where the timecode is, its slide live and its video started part
// way; a jump, and the timeline is there, the same video sent there and not started
// again, in step; a jump within one slide's time does not trigger the slide again; a
// jump into another presentation's timeline, and out of both; a slide of another
// presentation taken live by hand stops the timeline until the timecode next jumps;
// stopped, the timeline pauses and its video is held, and played on, both go on in
// step; Not Engaged lets go. Then the same from sound: timecode played into a sound
// input of the private desktop (run.py's `play`), on its second channel, started, jumped
// and stopped. And a text box set to show ProPresenter's Timecode, in the editor,
// written to the file, and drawn on the output.
QtObject {
    id: t

//COMMON
    // How many times a slide has been shown on the slide layer, by anything
    property int shown: 0
    property Connections showWatch: Connections {
        target: Show
        function onSlideShown() { ++t.shown }
    }

    function panel() {
        return named("timelinePanel")
    }

    function timelineOf(path) {
        return catalog.open(path).timeline
    }

    function rgbOf(text) {
        const c = String(text).toLowerCase()
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
    }

    function rgbAt(p) {
        return rgbOf(testInput.windowPixel(p.x, p.y))
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
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

    // The Timecode window and what it holds
    function tcw() {
        return timecodeLoader.item
    }

    function inTc(name) {
        return Lib.find(tcw().contentItem, item => item.objectName === name)
    }

    function tcClick(item) {
        const p = item.mapToItem(null, item.width / 2, item.height / 2)
        testInput.mouseIn("timecodeWindow", 0, p.x, p.y)
        testInput.mouseIn("timecodeWindow", 2, p.x, p.y)
    }

    function tcPixel(item, fx, fy) {
        const p = item.mapToItem(null, item.width * fx, item.height * fy)
        return rgbOf(testInput.pixelIn("timecodeWindow", p.x, p.y))
    }

    function figures() {
        return inTc("timecodeFigures").text
    }

    // The rows of the window's list, as "01:00:00;00 Name" each
    function tcRows() {
        return Lib.findAll(inTc("timecodeRows").contentItem, item => item.modelData !== undefined && item.lit !== undefined)
                  .sort((a, b) => a.y - b.y)
                  .map(row => Lib.findAll(row, item => item.text !== undefined && item.elide !== undefined).map(text => text.text).join(" ") + (row.lit ? " *" : ""))
    }

    // Where the timeline that follows should be, by the figures: seconds past `start`
    function byFigures(start) {
        return Timecode.typed(figures()) - start
    }

    // How far the video is from where its timeline is in it, in milliseconds (its cue is at one second)
    function videoOut() {
        return output.livePlayer.position - (presentationTimeline.position - 1) * 1000
    }

    function showsChoice() {
        const button = Lib.find(editScreen.inspector, item => item.objectName === "linkChoice")
        const rows = []
        for (const row of button.parent.parent.choices()) {
            if (!row.items && row.label)
                rows.push(row)
        }
        return { shown: button.shown, labels: rows.map(row => row.label), pick: (label) => rows.find(row => row.label === label).run() }
    }

    function run() {
        steps = [
            // ---- A timeline's Timecode box, and the timecode it starts at
            () => {
                transitionDuration = 0
                mediaTransitionDuration = 0
                timelineZoom = 20
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Build My Life"))
                kept.fileA = document.path
                kept.fileB = entry("Cornerstone").file
                click(centre(named("timelineToggle")))
                return 700
            },
            () => {
                const box = named("timelineTimecode"), start = named("timelineTimecodeStart")
                check("the timeline has ProPresenter's Timecode box, not ticked, and the timecode it starts at beside it, as ProPresenter writes one",
                      box !== null && box.visible && !box.checked && start !== null && start.visible && start.text === "00:00:00;00", start ? start.text : "none")
                check("they stand left of the Duration", box.mapToItem(null, 0, 0).x < start.mapToItem(null, 0, 0).x
                      && start.mapToItem(null, 0, 0).x < named("timelineDuration").mapToItem(null, 0, 0).x)
                check("the file says nothing of timecode yet", timelineOf(kept.fileA).timecode === false && timelineOf(kept.fileA).timecodeOffset === 0)
                kept.unticked = rgbAt(box.mapToItem(null, 8, box.height / 2))
                click(box.mapToItem(null, 8, box.height / 2))
                return 500
            },
            () => {
                const box = named("timelineTimecode")
                check("ticked, the file's timeline follows timecode", box.checked && timelineOf(kept.fileA).timecode === true && timelineOf(kept.fileA).timecodeOffset === 0)
                const ticked = rgbAt(box.mapToItem(null, 3, box.height / 2 - 5))
                check("drawn: the box orange where it was dark", ticked.r > 180 && ticked.g > 60 && ticked.g < 190 && ticked.b < 100 && kept.unticked.r < 80,
                      ticked.text + " " + kept.unticked.text)
                const start = named("timelineTimecodeStart")
                start.text = "01:00:00;00"
                start.editingFinished()
                return 500
            },
            () => {
                const start = named("timelineTimecodeStart")
                check("a timecode typed is the start, kept as seconds of it", timelineOf(kept.fileA).timecodeOffset === 3600 && timelineOf(kept.fileA).timecode === true
                      && start.text === "01:00:00;00", timelineOf(kept.fileA).timecodeOffset + " " + start.text)
                start.text = "soon"
                start.editingFinished()
                return 300
            },
            () => {
                const start = named("timelineTimecodeStart")
                check("what is no timecode is refused, and said; the box says the start again", notice.includes("is not a timecode") && start.text === "01:00:00;00"
                      && timelineOf(kept.fileA).timecodeOffset === 3600, notice + " | " + start.text)
                start.text = "00:00:00;30"
                start.editingFinished()
                return 300
            },
            () => {
                const start = named("timelineTimecodeStart")
                check("nor is a thirtieth frame of thirty", notice.includes("is not a timecode") && start.text === "01:00:00;00" && timelineOf(kept.fileA).timecodeOffset === 3600, start.text)
                start.text = "10012"
                start.editingFinished()
                return 400
            },
            () => {
                const start = named("timelineTimecodeStart")
                check("digits alone are read in pairs from the right", timelineOf(kept.fileA).timecodeOffset === 60.4 && start.text === "00:01:00;12",
                      timelineOf(kept.fileA).timecodeOffset + " " + start.text)
                start.text = "1:00:00"
                start.editingFinished()
                return 400
            },
            () => {
                check("and an hour typed as 1:00:00", timelineOf(kept.fileA).timecodeOffset === 3600 && named("timelineTimecodeStart").text === "01:00:00;00")
                // ---- Two presentations of a playlist, each with a timeline that follows timecode: the first
                // from 01:00:00;00 for twenty seconds, with three slides and the video on it; the second from
                // 01:00:20;00 for ten, with two slides
                const slides = document.slides.map(slide => slide.id)
                const media = mediaFiles.find(file => file.name === "Run.mp4")
                check("(the video is in the bin)", media !== undefined, mediaFiles.map(file => file.name).join())
                kept.problems = [catalog.setTimelineDuration(kept.fileA, 20),
                                 catalog.addTimelineCues(kept.fileA, [{ time: 0, slideId: slides[0] }, { time: 2, slideId: slides[1] }, { time: 4, slideId: slides[2] },
                                                                      { time: 1, wanted: { kind: "media", item: media.id }, name: "" }])].filter(error => error !== "")
                const other = catalog.open(kept.fileB)
                kept.slidesB = other.slides.map(slide => slide.id)
                kept.problems = kept.problems.concat([catalog.setTimelineDuration(kept.fileB, 10), catalog.setTimelineTimecode(kept.fileB, true, 3620),
                                                      catalog.addTimelineCues(kept.fileB, [{ time: 0, slideId: kept.slidesB[0] }, { time: 3, slideId: kept.slidesB[1] }])]
                                                     .filter(error => error !== ""))
                check("(their timelines were written)", kept.problems.length === 0, kept.problems.join("; "))
                const made = catalog.createPlaylist("Timecoded", "")
                kept.playlist = made.id
                catalog.addToPlaylist(made.id, kept.fileA)
                catalog.addToPlaylist(made.id, kept.fileB)
                catalog.addToPlaylist(made.id, entry("Abandoned").file)
                reloadDocument()
                return 600
            },
            // ---- The Timecode window
            () => {
                const rows = catalog.timecodeRows(kept.playlist)
                kept.keyA = rows[0].key
                kept.keyB = rows[1].key
                check("the playlist's presentations, each with what its timeline says of timecode", rows.length === 3 && rows[0].follows && rows[0].start === 3600
                      && rows[0].duration === 20 && rows[1].follows && rows[1].start === 3620 && rows[1].duration === 10 && !rows[2].follows && rows[2].duration === 300,
                      JSON.stringify(rows.map(row => [row.name, row.follows, row.start, row.duration])))
                check("the window is not made until it is first opened", tcw() === null && !timecodeOpen)
                const button = named("timecodeWindowButton")
                check("the clock beside the start opens it", button !== null && button.visible)
                click(centre(button))
                return 800
            },
            () => {
                check("the Timecode window is open", timecodeOpen && tcw() !== null && tcw().visible)
                const lit = rgbAt(named("timecodeWindowButton").mapToItem(null, 3, 12))
                check("drawn: the clock lit blue while it is open", lit.b > 200 && lit.r < 120, lit.text)
                check("nothing is listened for until it is asked: Not Engaged, with no device chosen", !Timecode.engaged && Timecode.status === "Not Engaged"
                      && Timecode.device === "" && !Timecode.running && inTc("timecodeStatus").children.length > 0, Timecode.status + " " + Timecode.device)
                check("its figures are at nothing", figures() === "00:00:00;00", figures())
                check("and no playlist is chosen", Timecode.playlist === "" && tcRows().length === 0 && inTc("timecodePlaylist").currentText === "None", inTc("timecodePlaylist").currentText)
                const names = Timecode.devices.map(device => device.name)
                check("the devices: the one made here first, and the desktop's sound input, of two channels", names[0] === "Made here (to try it)" && names.includes("Test source")
                      && Timecode.devices.find(device => device.name === "Test source").channels === 2, names.join(" | "))
                check("the formats are ProPresenter's four, by its numbers", Timecode.formats.map(format => format.value + "=" + format.name).join() === "0=24 fps,1=25 fps,2=29.97 fps,3=30 fps"
                      && Timecode.format === 3, Timecode.formats.map(format => format.value + "=" + format.name).join())
                check("the settings are behind the cog", !inTc("timecodeSettings").visible)
                tcClick(inTc("timecodeCog"))
                Timecode.playlist = kept.playlist
                return 500
            },
            () => {
                check("the cog shows them: the device, the format and the offset", inTc("timecodeSettings").visible && inTc("timecodeDevice").visible && inTc("timecodeFormat").currentText === "30 fps"
                      && inTc("timecodeOffset").text === "0 s", inTc("timecodeFormat").currentText + " | " + inTc("timecodeOffset").text)
                check("the playlist chosen is listed: each presentation that follows timecode by the timecode it starts at, the one that does not without one",
                      inTc("timecodePlaylist").currentText === "Timecoded"
                      && tcRows().join(" | ") === "01:00:00;00 Build My Life | 01:00:20;00 Cornerstone | — Abandoned", tcRows().join(" | "))
                Timecode.device = "made-here"
                return 300
            },
            () => {
                check("with the one made here chosen there is a row to play it, stop it and send it somewhere, and no channel to choose",
                      Timecode.madeHere && inTc("timecodeMadeHere").visible && inTc("timecodePlay").visible && !inTc("timecodeChannel").visible)
                tcClick(inTc("timecodeStatus"))
                return 400
            },
            () => {
                check("a click on the word listens: Stopped", Timecode.engaged && Timecode.status === "Stopped" && !Timecode.running, Timecode.status)
                const pill = tcPixel(inTc("timecodeStatus"), 0.12, 0.5)
                check("drawn: the word on grey", Math.abs(pill.r - pill.g) < 12 && pill.g < 110, pill.text)
                Timecode.jump(3601)
                return 500
            },
            () => {
                check("sent to a timecode while stopped, the figures say it", figures() === "01:00:01;00" && !Timecode.running, figures())
                check("nothing follows a timecode that is not running", !presentationTimeline.following && !presentationTimeline.playing)
                kept.shownBefore = t.shown
                tcClick(inTc("timecodePlay"))
                return 100
            },
            // ---- Following
            until(() => Timecode.running && presentationTimeline.following, 4000, () => {
                check("played, it starts within a few frames: Playing", Timecode.running && Timecode.status === "Playing" && kept.waited < 600, kept.waited + " ms")
                const pill = tcPixel(inTc("timecodeStatus"), 0.12, 0.5)
                check("drawn: the word on green", pill.g > pill.r + 40 && pill.g > pill.b + 30, pill.text)
                check("the timeline of the presentation the timecode is in is given to its player, and follows", presentationTimeline.following && presentationTimeline.playing
                      && presentationTimeline.path === kept.fileA && presentationTimeline.holds(kept.keyA, kept.playlist), presentationTimeline.path)
                check("where the timecode says: a second past its start", Math.abs(presentationTimeline.position - byFigures(3600)) < 0.12 && presentationTimeline.position > 1
                      && presentationTimeline.position < 1.6, presentationTimeline.position.toFixed(3) + " by the figures " + byFigures(3600).toFixed(3))
                check("its slide in force is live, from the playlist's row", Show.cueLive && Show.liveIndex === 0 && Show.liveKey === kept.keyA && Show.livePlaylistId === kept.playlist,
                      Show.liveIndex + " " + Show.liveKey)
                check("the window's list lights the row the timecode is in", tcRows()[0].endsWith("*") && !tcRows()[1].endsWith("*"), tcRows().join(" | "))
                check("the timeline over the slides says it is following", named("timelineFollowing") !== null && (named("timelineFollowing").visible || !timelineMine))
                check("the operator's own view is left where it was", document.path === kept.fileA && playlistId === "")
                testInput.grabIn("timecodeWindow", "1-timecode-window")
                testInput.grab("2-timeline-following")
                return 900
            }),
            () => {
                check("its video, whose cue is at one second, is playing", Show.liveMedia !== null && Show.liveMedia.name === "Run.mp4" && output.livePlayer !== null
                      && output.livePlayer.playbackState === MediaPlayer.PlayingState, Show.liveMedia ? Show.liveMedia.name : "none")
                check("drawn: the output shows it", testInput.lit(false, 0, 0, 1, 1) > 0.2, testInput.lit(false, 0, 0, 1, 1))
                kept.playId = Show.liveMedia.playId
                return 100
            },
            until(() => Show.liveIndex === 1, 3000, () => {
                check("the next slide goes live when the timecode comes to its cue", Show.liveIndex === 1 && Math.abs(byFigures(3600) - 2) < 0.4, byFigures(3600).toFixed(2))
                check("(the video in step with its timeline, by where its player says it is, which is said a few frames coarsely)", Math.abs(videoOut()) < 150, videoOut().toFixed(0) + " ms")
                kept.shownBefore = t.shown
                // A jump
                Timecode.jump(3610)
                return 100
            }),
            until(() => logged(/timecode jumped, to 01:00:10/).length === 1 && presentationTimeline.position > 9.9, 2000, () => {
                check("the timecode jumps: believed within a few frames", logged(/timecode jumped, to 01:00:10/).length === 1 && kept.waited < 500, kept.waited + " ms")
                check("the timeline is where it went, still following", presentationTimeline.following && presentationTimeline.playing
                      && Math.abs(presentationTimeline.position - byFigures(3600)) < 0.12 && presentationTimeline.position < 10.6, presentationTimeline.position.toFixed(3))
                check("the slide in force there is live", Show.liveIndex === 2 && t.shown === kept.shownBefore + 1, Show.liveIndex + " shown " + (t.shown - kept.shownBefore))
                return 700
            }),
            () => {
                const chaser = output.liveChaser
                check("its video was sent there, and is the same playing: not taken off and started again", Show.liveMedia.playId === kept.playId && chaser !== null && chaser.landings === 1
                      && logged(/its video is playing, and is sent to 9\.\d\d s in/).length === 1, chaser ? chaser.landings : "no chaser")
                check("and was in step with its timeline once it had settled, to a frame and a half, as measured from its frames", Math.abs(chaser.last.out) < 50 && !chaser.last.held
                      && chaser.last.again === 0 && Math.abs(videoOut()) < 150, videoOut().toFixed(0) + " ms; " + JSON.stringify(chaser.last))
                check("the log says how long its picture took", logged(/video "Run\.mp4" sent to 9\.\d\d s by its timeline: its picture came after \d+ ms, and it was then in step/).length === 1,
                      logged(/sent to .* by its timeline/).join(" | "))
                testInput.say("  measure the jump: " + JSON.stringify(chaser.last))
                kept.shownBefore = t.shown
                // A jump within the same slide's time
                Timecode.jump(3613)
                return 100
            },
            until(() => logged(/timecode jumped, to 01:00:13/).length === 1 && presentationTimeline.position > 12.9, 2000, () => {
                return 600
            }),
            () => {
                check("a jump within one slide's time does not trigger the slide again; the video goes there", t.shown === kept.shownBefore && Show.liveIndex === 2
                      && output.liveChaser.landings === 2 && Math.abs(output.liveChaser.last.out) < 50,
                      (t.shown - kept.shownBefore) + " shown; " + output.liveChaser.landings + " landings; " + JSON.stringify(output.liveChaser.last))
                // Back, to before the video's cue: the first slide
                Timecode.jump(3600.5)
                return 100
            },
            until(() => logged(/timecode jumped, to 01:00:00/).length === 1 && Show.liveIndex === 0, 2000, () => {
                check("a jump back: the first slide is live again", Show.liveIndex === 0 && presentationTimeline.position < 1.2, presentationTimeline.position.toFixed(2))
                // Into the other presentation's timeline
                Timecode.jump(3623.5)
                return 100
            }),
            until(() => presentationTimeline.path === kept.fileB && presentationTimeline.following, 2000, () => {
                check("a jump into another presentation's timeline: that one follows, from where the timecode is in it", presentationTimeline.holds(kept.keyB, kept.playlist)
                      && presentationTimeline.playing && Math.abs(presentationTimeline.position - byFigures(3620)) < 0.12 && presentationTimeline.position > 3.4,
                      presentationTimeline.position.toFixed(2))
                check("its slide in force is live", Show.liveKey === kept.keyB && Show.liveIndex === 1 && liveDocument.path === kept.fileB, Show.liveKey + " " + Show.liveIndex)
                check("the list lights it", tcRows()[1].endsWith("*") && !tcRows()[0].endsWith("*"), tcRows().join(" | "))
                check("the video of the one it left plays on, as it does when a timeline is paused by hand", output.livePlayer.playbackState === MediaPlayer.PlayingState)
                // Out of both
                Timecode.jump(100)
                return 100
            }),
            until(() => !presentationTimeline.following, 2000, () => {
                check("a jump out of every timeline: the one that followed is paused, and let go", !presentationTimeline.following && !presentationTimeline.playing && Timecode.running
                      && logged(/the timeline of "Cornerstone" paused: the timecode has gone out of it/).length === 1)
                check("and no row is lit", tcRows().every(row => !row.endsWith("*")), tcRows().join(" | "))
                Timecode.jump(3605)
                return 100
            }),
            until(() => presentationTimeline.following && presentationTimeline.path === kept.fileA, 2000, () => {
                check("back into the first: it follows again", presentationTimeline.playing && Show.liveKey === kept.keyA && Show.liveIndex === 2, Show.liveKey + " " + Show.liveIndex)
                return 700
            }),
            () => {
                check("(its video in step again)", Math.abs(output.liveChaser.last.out) < 50 && Math.abs(videoOut()) < 150, videoOut().toFixed(0) + " ms; " + JSON.stringify(output.liveChaser.last))
                // ---- By hand, while it follows: another presentation's slide
                openEntry(entry("Abandoned"))
                return 500
            },
            () => {
                click(centre(slideCell(0)))
                return 1500
            },
            () => {
                check("a slide of another presentation taken live stops the timeline, and the timecode going on does not start it again",
                      Timecode.running && !presentationTimeline.following && !presentationTimeline.playing && Show.liveKey !== kept.keyA
                      && logged(/was stopped by something else, and is left until the timecode next starts or jumps/).length === 1, Show.liveKey)
                Timecode.jump(3606)
                return 100
            },
            until(() => presentationTimeline.following, 2000, () => {
                check("until the timecode next jumps: then it follows again, and its slide is live", presentationTimeline.playing && presentationTimeline.path === kept.fileA
                      && Show.liveKey === kept.keyA && Show.liveIndex === 2, Show.liveKey + " " + Show.liveIndex)
                return 800
            }),
            // ---- Stopped, and played on
            () => {
                kept.landings = output.liveChaser.landings
                tcClick(inTc("timecodeStop"))
                return 100
            },
            until(() => !Timecode.running, 2000, () => {
                check("stopped: known within a few frames, and the word says Stopped", !Timecode.running && Timecode.status === "Stopped" && kept.waited < 500, kept.waited + " ms")
                check("the timeline is paused and let go", !presentationTimeline.following && !presentationTimeline.playing
                      && logged(/the timeline of "Build My Life" paused: the timecode has stopped/).length === 1)
                kept.stoppedAt = Timecode.typed(figures())
                return 400
            }),
            () => {
                check("its video is held where it is", output.livePlayer.playbackState === MediaPlayer.PausedState, output.livePlayer.playbackState)
                check("the figures stand at where it stopped", Timecode.typed(figures()) === kept.stoppedAt && kept.stoppedAt > 3606 && kept.stoppedAt < 3609, figures())
                tcClick(inTc("timecodePlay"))
                return 100
            },
            until(() => Timecode.running && presentationTimeline.following, 2000, () => {
                return 700
            }),
            () => {
                check("played on from where it stopped: the timeline follows again, and its video plays on from where it was held, in step, without being sent anywhere",
                      presentationTimeline.playing && output.livePlayer.playbackState === MediaPlayer.PlayingState && output.liveChaser.landings === kept.landings
                      && Math.abs(videoOut()) < 150, output.liveChaser.landings - kept.landings + " more landings; " + videoOut().toFixed(0) + " ms")
                // ---- Not Engaged
                tcClick(inTc("timecodeStatus"))
                return 500
            },
            () => {
                check("a click on the word: Not Engaged, and nothing follows", !Timecode.engaged && Timecode.status === "Not Engaged" && !Timecode.running
                      && !presentationTimeline.following && !presentationTimeline.playing
                      && logged(/the timeline of "Build My Life" paused: timecode is no longer listened for/).length === 1, Timecode.status)
                clearAll()
                // ---- From sound: the desktop's input, its second channel
                Timecode.device = Timecode.devices.find(device => device.name === "Test source").id
                Timecode.channel = 1
                return 300
            },
            () => {
                check("a sound device chosen: its channels to choose from, and no row for the one made here", !Timecode.madeHere && inTc("timecodeChannel").visible
                      && inTc("timecodeChannel").count === 2 && inTc("timecodeChannel").currentText === "2" && !inTc("timecodeMadeHere").visible, inTc("timecodeChannel").currentText)
                tcClick(inTc("timecodeStatus"))
                return 300
            },
            () => {
                check("listening to it: Stopped, with nothing wrong", Timecode.engaged && Timecode.status === "Stopped" && Timecode.problem === ""
                      && logged(/listening to channel 2 of Test source: 48000 Hz, 2 channels/).length === 1, Timecode.problem)
                return 100
            },
            // (The timecode is played into the input once the log has that line: a second and a half of silence, then
            // four seconds from 01:00:02;00.)
            until(() => Timecode.running && presentationTimeline.following, 15000, () => {
                check("timecode heard on the second channel: Playing", Timecode.running && Timecode.status === "Playing" && logged(/timecode started, at 01:00:02;0/).length === 1,
                      logged(/timecode started/).join(" | "))
                check("the timeline it is in follows, from two seconds in, its slide live", presentationTimeline.path === kept.fileA && presentationTimeline.playing
                      && Math.abs(presentationTimeline.position - byFigures(3600)) < 0.15 && presentationTimeline.position > 2 && presentationTimeline.position < 2.8
                      && Show.liveKey === kept.keyA && Show.liveIndex === 1, presentationTimeline.position.toFixed(3) + " " + Show.liveIndex)
                return 1500
            }),
            () => {
                testInput.grabIn("timecodeWindow", "3-timecode-from-sound")
                check("the signal is said to be timecode at thirty frames a second, and how loud it is", Timecode.measured === "30 fps" && Timecode.level > 0.2 && Timecode.level < 0.6
                      && inTc("timecodeLevel").width > 10, Timecode.measured + " " + Timecode.level.toFixed(2) + " watched " + Timecode.watched)
                check("the figures go on with it", byFigures(3600) > 3.3 && byFigures(3600) < 4.6 && Math.abs(presentationTimeline.position - byFigures(3600)) < 0.15, figures())
                return 100
            },
            // (Then three seconds from 01:00:21;00, which is in the second presentation's timeline.)
            until(() => logged(/timecode jumped, to 01:00:21/).length === 1 && presentationTimeline.path === kept.fileB, 6000, () => {
                check("the timecode in the sound jumps: the other presentation's timeline follows, a second in", logged(/timecode jumped, to 01:00:21;0/).length === 1
                      && presentationTimeline.following && presentationTimeline.playing && presentationTimeline.position > 1 && presentationTimeline.position < 1.8
                      && Show.liveKey === kept.keyB && Show.liveIndex === 0, presentationTimeline.position.toFixed(3) + " " + Show.liveIndex)
                return 100
            }),
            until(() => !Timecode.running, 6000, () => {
                check("the timecode in the sound ends: Stopped, at the frame after its last", !Timecode.running && Timecode.status === "Stopped" && figures() === "01:00:24;00"
                      && !presentationTimeline.following && !presentationTimeline.playing, figures())
                check("the timeline stands where it stopped, to a few frames", Math.abs(presentationTimeline.position - 4) < 0.2, presentationTimeline.position.toFixed(3))
                // (But the timecodes typed that were none, which were said; and the fixture's own song naming
                // media that the fixture has not.)
                const problems = logged(/PROBLEM/).filter(line => !/is not a timecode|was not found in the workspace/.test(line))
                check("nothing was a problem", problems.length === 0, problems.join(" | "))
                // The first channel has a tone on it, which is no timecode
                Timecode.channel = 0
                return 800
            }),
            () => {
                check("a channel with sound that is not timecode: Stopped, and the signal said to be none", Timecode.status === "Stopped" && Timecode.measured === ""
                      && logged(/listening to channel 1 of Test source/).length === 1, Timecode.measured)
                tcClick(inTc("timecodeStatus"))
                clearAll()
                // ---- A text box that shows the timecode
                Timecode.device = "made-here"
                openEntry(entry("Abandoned"))
                return 500
            },
            () => {
                kept.entry = entry("Abandoned")
                const words = (element) => element.linkKind === "none" && testInput.plain(element.text).trim() !== ""
                kept.slideAt = document.slides.findIndex(slide => slide.elements.some(words))
                kept.slide = document.slides[Math.max(0, kept.slideAt)]
                kept.box = kept.slide.elements.find(words)
                check("(a slide with a text box of its own words)", kept.box !== undefined, kept.slideAt)
                startEditing(kept.entry, kept.slide.id)
                return 900
            },
            () => {
                editScreen.canvas.pick(kept.box.id)
                editScreen.inspector.tab = "text"
                return 400
            },
            () => {
                const shows = showsChoice()
                check("what a box shows can be ProPresenter's Timecode, or its Timecode Status", shows.labels.includes("Timecode") && shows.labels.includes("Timecode Status"), shows.labels.join(" | "))
                shows.pick("Timecode")
                return 500
            },
            () => {
                const disk = catalog.open(kept.entry.file).slides[kept.slideAt].elements.find(element => element.id === kept.box.id)
                check("chosen, the box shows the timecode, and the file has the link", editScreen.canvas.selected.linkKind === "timecode" && disk.linkKind === "timecode"
                      && showsChoice().shown === "Timecode", disk.linkKind)
                stopEditing()
                return 800
            },
            () => {
                Timecode.engaged = true
                Timecode.jump(7384.5)
                click(centre(slideCell(kept.slideAt)))
                return 700
            },
            () => {
                const texts = Lib.findAll(output.contentItem, item => item.replacement !== undefined && typeof item.replacement === "string" && item.visible)
                check("live, the box says the timecode on the output", texts.some(text => text.replacement === "02:03:04;15"), texts.map(text => text.replacement).join(" | "))
                check("drawn: the output is not empty", testInput.lit(false, 0, 0, 1, 1) > 0.001, testInput.lit(false, 0, 0, 1, 1))
                Timecode.play()
                return 1300
            },
            () => {
                const texts = Lib.findAll(output.contentItem, item => item.replacement !== undefined && typeof item.replacement === "string" && item.visible)
                const said = texts.map(text => text.replacement).find(text => /^02:03:0[56];\d\d$/.test(text))
                check("and goes on as the timecode does", said !== undefined, texts.map(text => text.replacement).join(" | "))
                Timecode.stop()
                Timecode.engaged = false
                clearAll()
                click(centre(named("timecodeWindowButton")))
                return 500
            },
            () => {
                check("the clock shuts the window again", !timecodeOpen && !tcw().visible, timecodeOpen + " " + tcw().visible)
            }
        ]
        next()
    }
}
