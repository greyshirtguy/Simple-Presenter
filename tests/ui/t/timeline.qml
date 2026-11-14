import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// ProPresenter's timeline (src/timeline.h, TimelinePlayer, TimelinePanel.qml): opened by its
// button at the right of the Arrangements button, under the line over the slides; recorded
// by clicking slides and playing a piece of the audio bin with Record on, and written when
// Record goes off, as ProPresenter writes one (each slide "Cue" and its place, the audio by
// its file); played, its slides go live at their times and its audio plays; started part
// way, the slide before and the audio where the playhead is in it; paused, its audio stops;
// stopped by a clear group's Stop Timeline and by another presentation's slide; round
// again with Loop; an action put on its actions track from the right button's menu, and
// done; a cue dragged to another time and taken off by its menu. Then the API's play, pause,
// rewind and status, and what it says of the presentation; MIDI's Start and Stop Timeline;
// a slide carried up onto the timeline, the timeline scrolling while one is held near its
// end; a picture of the media bin and a piece of the audio bin dragged onto it, each to its
// own track; and the clear groups' Stop Timeline ticks. And the guard on drags
// (src/dragstart.h): a cue, a slide or a picture swept over in a click, all at once, is not
// dragged (nor the picture put on); a press that shifts a few pixels is a click; and with
// the timeline shut no slide is carried.
QtObject {
    id: t

//COMMON
    readonly property int port: 50126

    function panel() {
        return named("timelinePanel")
    }

    function cues() {
        return Lib.findAll(panel(), item => item.objectName === "timelineCue" && item.visible)
    }

    function written() {
        return catalog.open(kept.path).timeline
    }

    // A point of the timeline's tracks, at a time and a height in them, in the window
    function onTracks(seconds, y) {
        const view = named("timelineView")
        return view.contentItem.mapToItem(null, panel().x0(seconds), panel().rulerHeight + y)
    }

    function rgbAt(p) {
        const c = String(testInput.windowPixel(p.x, p.y)).toLowerCase()
        return { r: parseInt(c.substring(1, 3), 16), g: parseInt(c.substring(3, 5), 16), b: parseInt(c.substring(5, 7), 16), text: c }
    }

    function outputLit() {
        return testInput.lit(false, 0, 0, 1, 1)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // Pressed, moved there and let go all at once, as a click made with the pointer still on its way: no drag
    function sweep(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 10; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
        testInput.mouse(2, to.x, to.y)
    }

    function send(path) {
        return testInput.httpSend(port, "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" + port + "\r\n\r\n")
    }

    function response(id) {
        const raw = testInput.httpReceived(id)
        const end = raw.indexOf("\r\n\r\n")
        if (end < 0)
            return { status: 0, body: "" }
        return { status: Number(raw.slice(0, end).split("\r\n")[0].split(" ")[1]), body: raw.slice(end + 4) }
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                mediaTransitionDuration = 0
                // (The time spread out, for cues a fraction of a second apart to be pointed at)
                timelineZoom = 80
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Build My Life"))
                return 800
            },
            () => {
                kept.path = document.path
                check("a presentation of several slides, with no timeline on it", document.slides.length >= 4
                      && (document.timeline.cues ?? []).length === 0 && document.timeline.duration === 300, document.slides.length)
                const toggle = named("timelineToggle"), arranging = named("arrangementToggle")
                check("the Timeline button is at the right of the Arrangements button, where ProPresenter has it",
                      toggle !== null && toggle.visible && toggle.mapToItem(null, 0, 0).x > arranging.mapToItem(null, 0, 0).x)
                check("the timeline is not made until it is first opened", panel() === null)
                click(centre(toggle))
                return 600
            },
            () => {
                check("its button opens it under the line over the slides, and is lit", timelineShown && panel() !== null && panel().visible
                      && named("timelineToggle").on)
                const lit = rgbAt(named("timelineToggle").mapToItem(null, 3, 11))
                check("drawn: the lit button blue", lit.b > 200 && lit.r < 120, lit.text)
                check("its length is ProPresenter's 300 seconds, and nothing is on it", named("timelineDuration").text === "300s" && cues().length === 0,
                      named("timelineDuration").text)
                check("the grid is under it", named("slideGrid").mapToItem(null, 0, 0).y > panel().mapToItem(null, 0, panel().height).y - 2)
                const ground = rgbAt(onTracks(1, 52))
                check("drawn: the media track a dark purple, as ProPresenter's", ground.r > ground.g && ground.b > ground.g && ground.r < 90, ground.text)
                click(centre(named("timelineRecord")))
                return 300
            },
            () => {
                check("Record gives the player this presentation's timeline, and records", presentationTimeline.recording && timelineMine
                          && presentationTimeline.path === kept.path)
                const lit = rgbAt(named("timelineRecord").mapToItem(null, 3, 12))
                check("drawn: Record lit red", lit.r > 70 && lit.g < 50, lit.text)
                click(centre(slideCell(0)))
                return 300
            },
            () => {
                seekTimeline(0.4)
                click(centre(slideCell(1)))
                return 300
            },
            () => {
                seekTimeline(0.8)
                click(centre(slideCell(2)))
                return 300
            },
            () => {
                check("each slide clicked is recorded where the playhead is, and the slides went live as clicked",
                      presentationTimeline.recordedCount === 3 && Show.liveIndex === 2, presentationTimeline.recordedCount + " " + Show.liveIndex)
                check("what is recorded is drawn at once, paler", cues().length === 3 && cues().every(cue => cue.opacity < 1), cues().length)
                check("and is not in the file yet", (written().cues ?? []).length === 0)
                seekTimeline(0)
                const list = catalog.audioPlaylists.find(node => !node.folder && catalog.audioIn(node.path).some(row => row.name === "Walk In"))
                kept.audioList = list.path
                playAudio(catalog.audioIn(list.path).find(row => row.name === "Walk In"), list.path)
                return 400
            },
            () => {
                check("a piece of the audio bin played is recorded too", presentationTimeline.recordedCount === 4)
                click(centre(named("timelineRecord")))
                return 800
            },
            () => {
                const timeline = written()
                const slides = timeline.cues.filter(cue => cue.track === "slide")
                const audio = timeline.cues.filter(cue => cue.track === "audio")
                check("Record off writes it all to the file, in the new list", !presentationTimeline.recording && presentationTimeline.recordedCount === 0
                      && timeline.cues.length === 4 && !timeline.oldList, JSON.stringify(timeline.cues.map(cue => [cue.track, cue.time, cue.name])))
                check("the slides at their times, each named as ProPresenter names it", slides.length === 3
                      && slides.map(cue => cue.time).join() === "0,0.4,0.8"
                      && slides.every((cue, i) => cue.slideId === document.slides[i].id && cue.name === timeline.slides[cue.slideId].name
                                      && /^Cue \d+$/.test(cue.name)), slides.map(cue => cue.name).join())
                check("the audio at its time, by its file, as long as it is", audio.length === 1 && audio[0].time === 0 && audio[0].name === "Walk In.wav"
                      && audio[0].length === 3, audio.length ? audio[0].name + " " + audio[0].length : "none")
                check("drawn: the cues on their tracks, written", cues().length === 4 && cues().every(cue => cue.opacity === 1))
                const block = cues().find(cue => cue.cue.track === "slide" && cue.cue.time === 0.4)
                const fill = rgbAt(block.mapToItem(null, block.width - 3, 3))
                check("drawn: a slide's block in its group's colour", fill.text === String(groupColor(block.cue)).toLowerCase()
                      || Math.abs(fill.r - parseInt(String(groupColor(block.cue)).substring(1, 3), 16)) < 8, fill.text + " " + groupColor(block.cue))
                const bar = rgbAt(onTracks(2.5, 64 + 12))
                check("drawn: the audio a green bar as long as it is", bar.g > 150 && bar.g > bar.r && bar.g > bar.b, bar.text)
                setTimelineDuration(2)
                clearAudio()
                clearSlide()
                return 500
            },
            () => {
                check("the length typed is written", written().duration === 2 && named("timelineDuration").text === "2s", named("timelineDuration").text)
                kept.dark = outputLit()
                rewindTimeline()
                click(centre(named("timelinePlay")))
                return 150
            },
            () => {
                check("Play plays it: the slide at the start goes live, and its audio plays", presentationTimeline.playing && Show.liveIndex === 0
                      && Show.cueLive && Show.liveKey === documentKey && Show.liveMusic !== null && Show.liveMusic.name === "Walk In",
                      Show.liveIndex + " " + (Show.liveMusic ? Show.liveMusic.name : "no music"))
                check("drawn: the slide on the output", outputLit() > kept.dark, outputLit() + " " + kept.dark)
                testInput.grab("timeline-playing")
                kept.playhead = named("timelinePlayhead").x
                return 400
            },
            () => {
                check("at its time the next slide goes live", Show.liveIndex === 1, Show.liveIndex)
                check("drawn: the playhead moves as it plays", named("timelinePlayhead").x > kept.playhead + 3,
                      kept.playhead + " " + named("timelinePlayhead").x)
                return 1700
            },
            () => {
                check("it goes on to the last, and stops at its end", Show.liveIndex === 2 && !presentationTimeline.playing
                      && presentationTimeline.position === 2, Show.liveIndex + " " + presentationTimeline.position)
                check("the time under the controls says where the playhead is", named("timelineTime").text === "00:02.0", named("timelineTime").text)
                clearAudio()
                clearSlide()
                seekTimeline(0.5)
                click(centre(named("timelinePlay")))
                return 100
            },
            () => {
                check("started part way, the slide before the playhead goes live", presentationTimeline.playing && Show.liveIndex === 1
                      && Show.cueLive, Show.liveIndex)
                check("and the audio under it plays from where the playhead is in it", Show.liveMusic !== null
                      && Math.abs(Number(Show.liveMusic.startAt) - 0.5) < 0.01, Show.liveMusic ? Show.liveMusic.startAt : "no music")
                check("as the log says", logged(/"Walk In" started 0\.50 s in/).length === 1, logged(/Walk In/).slice(-1)[0])
                click(centre(named("timelinePlay")))
                return 300
            },
            () => {
                check("Pause stops it where it is, and its audio with it", !presentationTimeline.playing && Show.liveMusic === null
                      && presentationTimeline.position > 0.5 && presentationTimeline.position < 1.2, presentationTimeline.position)
                check("the first clear group is ProPresenter's Clear All, which stops both timelines", ClearGroups.groups.length > 0
                      && ClearGroups.groups[0].timelines.indexOf("presentation") >= 0, JSON.stringify(ClearGroups.groups[0] ?? null))
                click(centre(named("timelinePlay")))
                return 200
            },
            () => {
                clearFirstGroup()
                return 200
            },
            () => {
                check("a clear group with Stop Timeline stops it", !presentationTimeline.playing)
                kept.other = documents.find(d => openable(d) && d.name !== "Build My Life").name
                rewindTimeline()
                click(centre(named("timelinePlay")))
                return 200
            },
            () => {
                openEntry(entry(kept.other))
                return 700
            },
            () => {
                check("opening another presentation leaves it playing", presentationTimeline.playing)
                check("whose timeline is shown, not the one playing", !timelineMine && cues().length === 0)
                click(centre(slideCell(0)))
                return 300
            },
            () => {
                check("another presentation's slide taken live stops it", !presentationTimeline.playing && Show.liveKey === documentKey)
                openEntry(entry("Build My Life"))
                return 700
            },
            () => {
                check("back at its presentation, its timeline is shown again", timelineMine && cues().length === 4)
                click(centre(named("timelineLoop")))
                return 500
            },
            () => {
                check("Loop is written, and lit", written().loop === true && named("timelineLoop").on)
                clearAudio()
                rewindTimeline()
                click(centre(named("timelinePlay")))
                return 2600
            },
            () => {
                check("with Loop it goes round, past its end, from the start", presentationTimeline.playing && presentationTimeline.position < 1.5,
                      presentationTimeline.position)
                click(centre(named("timelinePlay")))
                setTimelineLoop(false)
                clearAudio()
                return 400
            },
            () => {
                click(onTracks(1.5, 88 + 12), Qt.RightButton)
                return 300
            },
            () => {
                check("the right button on the actions track offers ProPresenter's actions", labels().startsWith("[Add Action]"), labels())
                click(menuRow("Clear"))
                return 300
            },
            () => {
                click(menuRow("The Slide"))
                return 700
            },
            () => {
                const actions = written().cues.filter(cue => cue.track === "action")
                check("the action is put on at that time", actions.length === 1 && Math.abs(actions[0].time - 1.5) < 0.1
                      && actions[0].action.kind === "clear", JSON.stringify(actions))
                check("drawn: on the actions track", cues().some(cue => cue.cue.track === "action" && Math.abs(cue.y - (panel().rulerHeight + 88)) < 1))
                rewindTimeline()
                click(centre(named("timelinePlay")))
                return 1800
            },
            () => {
                check("at its time the action is done: the slide is cleared", Show.cleared && !Show.clearedByCue && presentationTimeline.playing,
                      Show.cleared + " " + Show.liveIndex)
                click(centre(named("timelinePlay")))
                clearAudio()
                const block = cues().find(cue => cue.cue.track === "slide" && cue.cue.time === 0.8)
                kept.cueFrom = block.mapToItem(null, 4, 20)
                kept.cueTo = Qt.point(kept.cueFrom.x + panel().x0(0.4), kept.cueFrom.y)
                sweep(kept.cueFrom, kept.cueTo)
                return 400
            },
            () => {
                const still = written().cues.find(cue => cue.track === "slide" && cue.slideId === document.slides[2].id)
                check("a cue swept over in a click is not moved: the click puts the playhead at it", still !== undefined && still.time === 0.8
                      && Math.abs(presentationTimeline.position - 0.8) < 0.01, (still ? still.time : "gone") + " " + presentationTimeline.position)
                testInput.mouse(0, kept.cueFrom.x, kept.cueFrom.y)
                return 250
            },
            () => {
                // Held a moment, and dragged
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.cueFrom.x + (kept.cueTo.x - kept.cueFrom.x) * i / 10, kept.cueFrom.y)
                testInput.mouse(2, kept.cueTo.x, kept.cueTo.y)
                return 700
            },
            () => {
                const moved = written().cues.find(cue => cue.track === "slide" && cue.slideId === document.slides[2].id)
                check("a cue dragged along its track is moved in the file", moved !== undefined && Math.abs(moved.time - 1.2) < 0.1,
                      moved ? moved.time : "gone")
                const block = cues().find(cue => cue.cue.track === "slide" && cue.cue.slideId === document.slides[2].id)
                click(block.mapToItem(null, 6, 20), Qt.RightButton)
                return 300
            },
            () => {
                check("its menu has Delete", labels().indexOf("Delete") >= 0, labels())
                click(menuRow("Delete"))
                return 600
            },
            () => {
                check("taken off by its menu", written().cues.length === 4 && !written().cues.some(cue => cue.slideId === document.slides[2].id),
                      written().cues.length)
                rewindTimeline()
                networkPort = t.port
                networkEnabled = true
                return 800
            },
            () => {
                kept.play = send("/v1/presentation/focused/timeline/play")
                return 300
            },
            () => {
                check("the API's play plays the timeline of the presentation open", response(kept.play).status === 204 && presentationTimeline.playing,
                      response(kept.play).status)
                kept.status = send("/v1/presentation/focused/timeline")
                kept.activeStatus = send("/v1/presentation/active/timeline")
                return 300
            },
            () => {
                const status = JSON.parse(response(kept.status).body || "{}")
                check("and says it is running, and where", status.is_running === true && status.current_time > 0, response(kept.status).body)
                check("the live presentation's is the same one", JSON.parse(response(kept.activeStatus).body || "{}").is_running === true,
                      response(kept.activeStatus).body)
                kept.pause = send("/v1/presentation/focused/timeline/pause")
                return 300
            },
            () => {
                check("its pause pauses it", response(kept.pause).status === 204 && !presentationTimeline.playing, response(kept.pause).status)
                kept.rewind = send("/v1/presentation/focused/timeline/rewind")
                kept.wrong = send("/v1/presentation/focused/timeline/stop")
                kept.about = send("/v1/presentation/" + document.uuid)
                return 400
            },
            () => {
                check("its rewind puts the playhead at the start", response(kept.rewind).status === 204 && presentationTimeline.position === 0)
                check("an operation ProPresenter has not is not found", response(kept.wrong).status === 404, response(kept.wrong).status)
                check("the presentation is said to have a timeline", /"has_timeline":true/.test(response(kept.about).body),
                      response(kept.about).body.slice(0, 200))
                midiControl.run("startTimeline", 0)
                return 300
            },
            () => {
                check("MIDI's Start Timeline plays it", presentationTimeline.playing)
                midiControl.run("stopTimeline", 0)
                return 200
            },
            () => {
                check("and its Stop Timeline stops it", !presentationTimeline.playing)
                clearAll()
                kept.count = written().cues.length
                kept.slideAt = centre(slideCell(3))
                kept.landAt = onTracks(1.6, 20)
                sweep(kept.slideAt, kept.landAt)
                return 400
            },
            () => {
                check("a slide swept up to the timeline in a click is not carried there", written().cues.length === kept.count && !slideDrag.Drag.active
                      && !named("timelineTrackOutline").visible, written().cues.length)
                testInput.mouse(0, kept.slideAt.x, kept.slideAt.y)
                return 250
            },
            () => {
                // Held a moment, and carried up
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.slideAt.x + (kept.landAt.x - kept.slideAt.x) * i / 10, kept.slideAt.y + (kept.landAt.y - kept.slideAt.y) * i / 10)
                return 200
            },
            () => {
                const carried = slideCarried
                check("held a moment and moved, the slide is carried, named as it will be on the timeline", slideDrag.Drag.active && carried !== null
                      && carried.id === document.slides[3].id && /^Cue \d+$/.test(carried.name), JSON.stringify(carried))
                const block = rgbAt(Qt.point(kept.landAt.x + 12 + 6, kept.landAt.y + 8 + 20))
                const fill = String(carried.fill).toLowerCase()
                check("drawn: what is carried, in the slide's group's colour", Math.abs(block.r - parseInt(fill.substring(1, 3), 16)) < 30
                      && Math.abs(block.g - parseInt(fill.substring(3, 5), 16)) < 30 && Math.abs(block.b - parseInt(fill.substring(5, 7), 16)) < 30,
                      block.text + " " + fill)
                check("the slides track is outlined, and where it would land is at the time under the pointer", named("timelineTrackOutline").visible
                      && named("timelineLanding").visible && Math.abs(named("timelineDrop").landing - 1.6) < 0.05, named("timelineDrop").landing)
                const lineAt = onTracks(named("timelineDrop").landing, 4)
                const line = rgbAt(Qt.point(lineAt.x + 1, lineAt.y))
                check("drawn: a white line there", line.r > 220 && line.g > 220 && line.b > 220, line.text)
                testInput.grab("timeline-carrying")
                testInput.mouse(2, kept.landAt.x, kept.landAt.y)
                return 700
            },
            () => {
                const added = written().cues.filter(cue => cue.track === "slide" && cue.slideId === document.slides[3].id)
                check("let go, it is written on the slides track at that time, named as ProPresenter names it", written().cues.length === kept.count + 1
                      && added.length === 1 && Math.abs(added[0].time - 1.6) < 0.05 && added[0].name === written().slides[document.slides[3].id].name,
                      JSON.stringify(added))
                check("drawn: its block on the slides track", cues().some(cue => cue.cue.track === "slide" && cue.cue.slideId === document.slides[3].id
                                                                         && Math.abs(cue.y - panel().rulerHeight) < 1))
                check("the slide carried was not taken live", !(Show.cueLive && Show.liveKey === documentKey && Show.liveIndex === 3), Show.liveIndex)
                check("as the log says", logged(/slide 4 of .* dropped on its timeline at 1\.6/).length === 1, logged(/dropped on its timeline/).slice(-1)[0])
                testInput.mouse(0, kept.slideAt.x, kept.slideAt.y)
                return 250
            },
            () => {
                // A press that shifts a few pixels before it is let go
                testInput.mouse(1, kept.slideAt.x + 5, kept.slideAt.y + 3)
                testInput.mouse(2, kept.slideAt.x + 5, kept.slideAt.y + 3)
                return 300
            },
            () => {
                check("a press that shifts a few pixels is a click: the slide goes live, and nothing is carried", Show.cueLive && Show.liveIndex === 3
                      && written().cues.length === kept.count + 1, Show.liveIndex)
                clearSlide()
                // A long timeline, and a slide held near the right end of what is in view
                setTimelineDuration(120)
                return 500
            },
            () => {
                const view = named("timelineView")
                kept.slideAt = centre(slideCell(0))
                kept.edgeAt = view.mapToItem(null, view.width - 6, panel().rulerHeight + 20)
                kept.contentX = view.contentX
                testInput.mouse(0, kept.slideAt.x, kept.slideAt.y)
                return 250
            },
            () => {
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.slideAt.x + (kept.edgeAt.x - kept.slideAt.x) * i / 10, kept.slideAt.y + (kept.edgeAt.y - kept.slideAt.y) * i / 10)
                return 600
            },
            () => {
                const view = named("timelineView")
                kept.landing = named("timelineDrop").landing
                check("held near the end of what is in view, the timeline scrolls on, and where it would land with it", view.contentX > kept.contentX + 50
                      && kept.landing > (view.contentX + view.width - 40) / panel().pps, kept.contentX + " " + view.contentX + " " + kept.landing)
                testInput.mouse(2, kept.edgeAt.x, kept.edgeAt.y)
                return 700
            },
            () => {
                const added = written().cues.filter(cue => cue.track === "slide" && cue.slideId === document.slides[0].id)
                check("let go there, it lands at the time that had been out of sight", added.some(cue => Math.abs(cue.time - kept.landing) < 0.1),
                      JSON.stringify(added.map(cue => cue.time)) + " " + kept.landing)
                check("and the view stays there, and does not go back to the playhead", named("timelineView").contentX > kept.contentX + 50,
                      named("timelineView").contentX)
                clearAll()
                named("timelineView").contentX = 0
                kept.count = written().cues.length
                kept.picture = mediaFiles.find(m => !m.missing && !m.video)
                kept.mediaAt = centre(binCell(m => m.id === kept.picture.id))
                // Out of the media bin and up to the timeline all at once: a click made in passing, let go off the picture
                sweep(kept.mediaAt, onTracks(2.5, 40 + 12))
                return 400
            },
            () => {
                check("a picture of the media bin swept up in a click is neither carried nor put on", written().cues.length === kept.count
                      && liveMedia === null, written().cues.length + " " + (liveMedia ? liveMedia.name : "none"))
                testInput.mouse(0, kept.mediaAt.x, kept.mediaAt.y)
                return 250
            },
            () => {
                kept.landAt = onTracks(2.5, 40 + 12)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.mediaAt.x + (kept.landAt.x - kept.mediaAt.x) * i / 10, kept.mediaAt.y + (kept.landAt.y - kept.mediaAt.y) * i / 10)
                return 200
            },
            () => {
                const outline = named("timelineTrackOutline")
                check("held a moment and dragged out of the media bin, a picture is carried, and the media track is outlined", mediaDragged !== null
                      && timelineCarried !== null && timelineCarried.track === "media" && outline.visible && Math.abs(outline.y - (panel().rulerHeight + 40)) < 1,
                      JSON.stringify(timelineCarried))
                const lineAt = onTracks(named("timelineDrop").landing, 40 + 12)
                const line = rgbAt(Qt.point(lineAt.x + 1, lineAt.y))
                check("drawn: where it would land, a white line down to the media track", named("timelineLanding").visible
                      && Math.abs(named("timelineDrop").landing - 2.5) < 0.05 && line.r > 220 && line.g > 220 && line.b > 220, line.text)
                testInput.mouse(2, kept.landAt.x, kept.landAt.y)
                return 700
            },
            () => {
                const added = written().cues.filter(cue => cue.track === "media")
                check("let go, the picture is written on the media track at that time, named by its file", written().cues.length === kept.count + 1
                      && added.length === 1 && Math.abs(added[0].time - 2.5) < 0.05 && added[0].name !== "", JSON.stringify(added.map(cue => [cue.time, cue.name])))
                check("drawn: its block on the media track", cues().some(cue => cue.cue.track === "media" && Math.abs(cue.y - (panel().rulerHeight + 40)) < 1))
                check("and it was not put on the media layer", liveMedia === null)
                check("as the log says", logged(/the picture .* dropped on the timeline of .* at 2\.5/).length === 1, logged(/dropped on the timeline/).slice(-1)[0])
                sidePanel.showControlTab = "audio"
                openAudioPlaylist(kept.audioList)
                return 600
            },
            () => {
                const list = named("audioList")
                let index = -1
                for (let i = 0; i < list.count && index < 0; ++i) {
                    if (list.model[i].name === "Walk In")
                        index = i
                }
                list.positionViewAtIndex(index, ListView.Contain)
                kept.audioAt = centre(list.itemAtIndex(index))
                testInput.mouse(0, kept.audioAt.x, kept.audioAt.y)
                return 250
            },
            () => {
                // Let go over the actions track: it goes on its own
                kept.landAt = onTracks(4, 88 + 12)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.audioAt.x + (kept.landAt.x - kept.audioAt.x) * i / 10, kept.audioAt.y + (kept.landAt.y - kept.audioAt.y) * i / 10)
                return 200
            },
            () => {
                const outline = named("timelineTrackOutline")
                check("a piece of the audio bin is carried, and the audio track is outlined, as long as the piece is", audioDragged !== null
                      && timelineCarried !== null && timelineCarried.track === "audio" && timelineCarried.length === 3
                      && outline.visible && Math.abs(outline.y - (panel().rulerHeight + 64)) < 1
                      && Math.abs(named("timelineLanding").width - panel().x0(3)) < 2, JSON.stringify(timelineCarried) + " " + named("timelineLanding").width)
                testInput.mouse(2, kept.landAt.x, kept.landAt.y)
                return 700
            },
            () => {
                const added = written().cues.filter(cue => cue.track === "audio" && Math.abs(cue.time - 4) < 0.05)
                check("let go over another track, it is written on the audio track at that time, by its file, as long as it is", added.length === 1
                      && added[0].name === "Walk In.wav" && added[0].length === 3, JSON.stringify(written().cues.map(cue => [cue.track, cue.time, cue.name])))
                check("drawn: its block on the audio track", cues().some(cue => cue.cue.track === "audio" && cue.cue.time === added[0]?.time
                                                                        && Math.abs(cue.y - (panel().rulerHeight + 64)) < 1))
                check("and it did not play", Show.liveMusic === null)
                clearAll()
                openClearGroups(ClearGroups.groups[0].id)
                return 600
            },
            () => {
                const presentationTick = named("clearTimeline:presentation"), announcementsTick = named("clearTimeline:announcements")
                check("the clear groups' settings have Stop Timeline, for the presentation and the announcements, ticked as the group has them",
                      presentationTick !== null && announcementsTick !== null && presentationTick.checked && announcementsTick.checked)
                click(centre(presentationTick))
                return 400
            },
            () => {
                check("taken off, it is written", ClearGroups.groups[0].timelines.indexOf("presentation") < 0
                      && !named("clearTimeline:presentation").checked, JSON.stringify(ClearGroups.groups[0].timelines))
                click(centre(named("clearTimeline:presentation")))
                return 400
            },
            () => {
                check("and put back", ClearGroups.groups[0].timelines.indexOf("presentation") >= 0)
                closeClearGroups()
                click(centre(named("timelineToggle")))
                return 400
            },
            () => {
                check("its button shuts it, and the grid has the room again", !timelineShown && !panel().visible && !named("timelineToggle").on)
                check("the log says what the timeline did", logged(/timeline\s/).length >= 4, logged(/timeline\s/).length)
                kept.slideAt = centre(slideCell(1))
                testInput.mouse(0, kept.slideAt.x, kept.slideAt.y)
                return 250
            },
            () => {
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, kept.slideAt.x, kept.slideAt.y - 8 * i)
                kept.carried = slideDrag.Drag.active
                testInput.mouse(2, kept.slideAt.x, kept.slideAt.y - 80)
                return 300
            },
            () => {
                check("with the timeline shut, a slide held and moved is not carried", !kept.carried && !slideDrag.Drag.active)
            }
        ]
        next()
    }
}
