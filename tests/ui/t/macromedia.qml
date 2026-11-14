import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Media of the media bin dragged onto the macros, on a bin run.py makes (prepare_macromedia): a playlist "Walk In" of a
// red video that goes round and a green picture. The video, held over the macros' tab, brings the tab forward, and
// dropped on the strip along the bottom of the macros is given a macro of its own, called what it is called, whose one
// action shows it; the macro's row draws that action's picture; run, the macro puts the video on the output, and in the
// operator's preview of it, from no playlist. The picture dropped on that macro is another of its actions, and the
// macro run again shows it last. The same video dropped on a slide is still that slide's media, and no action of it.
// What the files then hold is looked at when the test is over, by after_macromedia in run.py.
QtObject {
    id: t

//COMMON
    readonly property string walkIn: "0C0C0C0C-0000-0000-0000-000000000001"

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        testInput.hold(200)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    // On from where the pointer is, with the button still held
    function moveTo(to) {
        for (let i = 1; i <= 6; ++i)
            testInput.mouse(1, kept.to.x + (to.x - kept.to.x) * i / 6, kept.to.y + (to.y - kept.to.y) * i / 6)
        kept.to = to
    }

    function macrosTab() {
        return Lib.findAll(sidePanel, item => item.modelData !== undefined && item.modelData !== null && item.modelData.id === "macros" && item.chosen !== undefined)[0]
    }

    function allMacros() {
        let all = []
        for (const collection of Macros.collections)
            all = all.concat(Array.from(collection.macros))
        return all
    }

    // A macro's row in the list, brought into view
    function macroRow(id) {
        const list = named("macroList")
        const index = list.model.findIndex(row => row.id === id && row.heading !== true)
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    // The pictures of a macro's media actions in its row
    function mediaGlyphs(id) {
        return Lib.findAll(macroRow(id), item => item.kind === "media" && item.framed !== undefined)
    }

    function binRow(name) {
        return binCell(m => m.name === name)
    }

    // The colour at a point of the output, and of the operator's preview of it, as fractions of the screen
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

    // Whether a colour is near pure red or pure green
    function reddish(c) {
        return parseInt(c.substring(1, 3), 16) > 200 && parseInt(c.substring(3, 5), 16) < 60 && parseInt(c.substring(5, 7), 16) < 60
    }

    function greenish(c) {
        return parseInt(c.substring(1, 3), 16) < 60 && parseInt(c.substring(3, 5), 16) > 200 && parseInt(c.substring(5, 7), 16) < 60
    }

    function run() {
        steps = [
            () => {
                sidePanel.showControlTab = "timers"
                openMediaPlaylist(t.walkIn)
                return 800
            },
            // ---- A video, by way of the macros' tab, onto the strip: a macro of its own
            () => {
                check("the media bin shows Walk In: the video and the picture", mediaFiles.map(m => m.name).join() === "Red.mp4,Green.png", mediaFiles.map(m => m.name).join())
                kept.macros = allMacros().length
                dragTo(centre(binRow("Red.mp4")), centre(macrosTab()))
                check("the video is being carried", mediaDragged !== null && mediaDragged.name === "Red.mp4")
                check("held over the macros' tab, nothing yet", sidePanel.showControlTab === "timers")
                return 900
            },
            () => {
                check("after a moment the tab comes forward", sidePanel.showControlTab === "macros")
                check("and the video is still being carried", mediaDragged !== null && mediaDragged.name === "Red.mp4")
                // (The workspace has more macros than the list has room for: there is no bare part of it to drop on.)
                const strip = named("macroNewDrop")
                check("with the strip along the bottom of the macros to drop it on for a macro of its own", strip !== null && strip.visible === true
                      && strokes(Lib.find(strip, item => item.text !== undefined)).light > 3)
                moveTo(centre(strip))
                return 300
            },
            () => {
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const all = allMacros()
                const made = all.find(m => m.name === "Red")
                check("dropped there, the video is given a macro of its own, called what its file is called", all.length === kept.macros + 1 && made !== undefined,
                      all.map(m => m.name).join())
                check("and the strip has gone with the drag", named("macroNewDrop").visible === false && mediaDragged === null)
                const action = made !== undefined ? made.actions[0] : {}
                check("whose one action shows the video, as its row of the bin has it: going round", made !== undefined && made.actions.length === 1
                      && action.kind === "media" && action.done === true && action.media.name === "Red.mp4" && action.media.video === true
                      && action.media.loops === true && action.media.missing === false, JSON.stringify(action))
                check("said as what it does", action.title === "Show the video “Red”", action.title)
                check("with an id of its own, which its media goes by", /^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$/.test(action.id)
                      && action.media.id === action.id, action.id)
                check("the log says what was dropped where", logged(/drop\s+a video dropped on the macros, on none of them: a macro is made for it/).length === 1)
                kept.macro = made !== undefined ? made.id : ""
                return 300
            },
            () => {
                const glyphs = mediaGlyphs(kept.macro)
                check("its row shows the action's picture, a frame with a play mark in it, drawn", glyphs.length === 1 && strokes(glyphs[0]).light >= 3,
                      glyphs.length > 0 ? JSON.stringify(strokes(glyphs[0])) : "none")
                testInput.grab("1-a-macro-of-its-own")
                click(centre(macroRow(kept.macro)))
                return 1500
            },
            () => {
                check("run, the macro puts the video on the media layer, from no playlist", liveMedia !== null && liveMedia.name === "Red.mp4"
                      && Show.liveMediaPlaylistId === "", (liveMedia ? liveMedia.name : "-") + " " + Show.liveMediaPlaylistId)
                check("the output shows it", reddish(outputAt(0.5, 0.5)), outputAt(0.5, 0.5))
                check("and so does the operator's preview", reddish(previewAt(0.5, 0.5)), previewAt(0.5, 0.5))
                check("the log says what the action did", logged(/action\s+Show the video “Red”/).length === 1)
                testInput.grabOutput("2-red-by-the-macro")
                sidePanel.showControlTab = "timers"
                return 300
            },
            // ---- A picture onto the macro that is there
            () => {
                dragTo(centre(binRow("Green.png")), centre(macrosTab()))
                return 900
            },
            () => {
                check("the tab comes forward for a picture too", sidePanel.showControlTab === "macros" && mediaDragged !== null && mediaDragged.name === "Green.png")
                moveTo(centre(macroRow(kept.macro)))
                return 300
            },
            () => {
                check("the macro is marked as what it would be dropped on", macroRow(kept.macro).border.width === 2)
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 500
            },
            () => {
                const made = allMacros().find(m => m.id === kept.macro)
                const action = made.actions[1] ?? {}
                check("dropped on the macro, the picture is another of its actions, and no macro is made", made.actions.length === 2 && action.kind === "media"
                      && action.media.name === "Green.png" && action.media.video === false && allMacros().length === kept.macros + 1,
                      Array.from(made.actions).map(a => a.title).join(" | "))
                check("said as what it does", action.title === "Show the picture “Green”", action.title)
                check("its row shows the two actions' pictures", mediaGlyphs(kept.macro).length === 2)
                click(centre(macroRow(kept.macro)))
                return 1200
            },
            () => {
                check("run again, the macro shows the picture last, in place of the video", liveMedia !== null && liveMedia.name === "Green.png",
                      liveMedia ? liveMedia.name : "-")
                check("the output shows it", greenish(outputAt(0.5, 0.5)), outputAt(0.5, 0.5))
                check("and so does the operator's preview", greenish(previewAt(0.5, 0.5)), previewAt(0.5, 0.5))
                testInput.grab("3-two-actions")
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    if (entry("Macro Media"))
                        break
                }
                openEntry(entry("Macro Media"))
                return 800
            },
            // ---- The video onto a slide: the slide's media, as it always was
            () => {
                kept.actions = document.slides[0].actions.length
                const from = centre(binRow("Red.mp4"))
                const to = centre(slideCell(0))
                testInput.mouse(0, from.x, from.y)
                testInput.hold(200)
                for (let i = 1; i <= 12; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
                testInput.mouse(2, to.x, to.y)
                return 600
            },
            () => {
                const slide = document.slides[0]
                check("dropped on a slide, the video is the slide's media", slide.mediaName !== "" && slide.mediaVideo === true, slide.mediaName)
                check("and not an action of it", slide.actions.length === kept.actions && !slide.actions.some(a => a.kind === "media"),
                      slide.actions.map(a => a.kind).join())
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
