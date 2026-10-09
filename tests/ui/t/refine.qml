import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// Props a collection at a time, put in order by dragging; the clear buttons dragged onto a slide and a macro; macros as
// coloured bars with their actions drawn on them, and their menu; a slide without its media, and its media without the
// slide; and when a background video is started again.
QtObject {
    id: t

//COMMON
    function propRows() {
        return Lib.findAll(named("propList"), i => i.modelData !== undefined && i.modelData.id !== undefined && i.on !== undefined)
    }

    function macroRows() {
        return Lib.findAll(named("macroList"), i => i.objectName === "macroRow")
    }

    function names(collection) {
        return collection.props.map(p => p.name).join("|")
    }

    function dragTo(from, to) {
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 12; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
        kept.to = to
    }

    function drop() {
        testInput.mouse(2, kept.to.x, kept.to.y)
    }

    function header(text) {
        return Lib.find(menu.contentItem, i => i.modelData !== undefined && i.modelData !== null && i.modelData.header === text)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                check("the app says which version it is", /^[0-9]+\.[0-9]+$/.test(Qt.application.version), Qt.application.version)
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                sidePanel.showControlTab = "props"
                return 600
            },
            () => {
                // ---- props, a collection at a time
                const picker = named("propCollection")
                const panel = named("propList").parent
                kept.panel = panel
                kept.first = Props.collections[0]
                check("the props tab shows one collection, chosen from a drop-down over the list", picker !== null && picker.visible && picker.model.length === Props.collections.length
                      && propRows().length === Math.min(kept.first.props.length, propRows().length) && panel.rows.length === kept.first.props.length
                      && panel.collectionId === kept.first.id, picker ? picker.model.join("|") : "none")
                panel.addCollection()
                return 400
            },
            () => {
                const made = Props.collections[Props.collections.length - 1]
                kept.extras = made.id
                check("a new collection is the one shown, with its name ready to be typed over", kept.panel.collectionId === made.id && kept.panel.renaming === made.id && kept.panel.rows.length === 0)
                kept.panel.rename({ heading: true, id: made.id, name: made.name }, "Extras")
                check("renamed", Props.collections[Props.collections.length - 1].name === "Extras" && named("propCollection").model.includes("Extras"))
                kept.panel.showAddMenu(named("showControlAdd"))
                return 300
            },
            () => {
                check("the + offers a prop or a collection", labels() === "New Prop, New Collection", labels())
                click(menuRow("New Prop"))
                return 700
            },
            () => {
                const extras = Props.collections.find(c => c.id === kept.extras)
                check("a new prop goes into the collection being shown, and opens in the editor", editing && extras.props.length === 1, extras.props.length)
                stopEditing()
                return 500
            },
            () => {
                kept.panel.showCollectionMenu(named("propCollectionMenu"))
                return 300
            },
            () => {
                check("the button beside the drop-down has what can be done with the collection", labels() === "[Extras], New Prop, Rename, One at a Time, Remove with its Props…, [Collections], New Collection", labels())
                testInput.key(Qt.Key_Escape)
                // back to the first collection, to move one of its props
                kept.panel.chosen = kept.first.id
                return 400
            },
            () => {
                kept.order = kept.first.props.map(p => p.id)
                check("the first collection has props to put in order", kept.order.length >= 3, kept.order.length)
                const rows = propRows()
                const from = rows[0].mapToItem(null, rows[0].width / 2, rows[0].height / 2)
                const to = rows[2].mapToItem(null, rows[2].width / 2, rows[2].height * 0.8)
                dragTo(from, to)
                return 250
            },
            () => {
                check("a prop dragged down the list shows where it would land", kept.panel.landing === (kept.order[3] ?? "end"), kept.panel.landing)
                testInput.grab("1-prop-drag")
                drop()
                return 500
            },
            () => {
                const now = Props.collections[0].props.map(p => p.id)
                check("and dropped is there: after the one it was dropped on the lower half of", now.join() === [kept.order[1], kept.order[2], kept.order[0]].concat(kept.order.slice(3)).join(), now.map(id => kept.order.indexOf(id)).join())
                check("it was not turned on by the drag", liveProps.length === 0)
                kept.moved = kept.order[1]
                kept.panel.showPropMenu(kept.panel.rows[0], propRows()[0], 20, 20)
                return 300
            },
            () => {
                check("a prop's menu has Move to, which leads to the other collections", labels().includes("Move to") && menuRow("Move to") !== null, labels())
                click(menuRow("Move to"))
                return 300
            },
            () => {
                check("in a menu of its own", labels().startsWith("[Move to], ") && labels().endsWith("Extras"), labels())
                click(menuRow("Extras"))
                return 500
            },
            () => {
                const extras = Props.collections.find(c => c.id === kept.extras)
                check("moved, the prop is in that collection, which is now the one shown", extras.props.some(p => p.id === kept.moved) && kept.panel.collectionId === kept.extras
                      && !Props.collections[0].props.some(p => p.id === kept.moved), extras.props.length)
                // ---- the clear buttons, dragged
                kept.slide = document.slides.findIndex(s => s.actions.length === 0 && s.mediaName === "")
                const button = named("clearProps")
                check("with nothing to clear, a click on a clear button does nothing", liveProps.length === 0 && !button.live)
                dragTo(centre(button), centre(slideCell(kept.slide)))
                return 250
            },
            () => {
                drop()
                return 500
            },
            () => {
                const a = document.slides[kept.slide].actions
                check("a clear button dragged onto a slide gives the slide the action that clears that layer", a.length === 1 && a[0].kind === "clear" && a[0].layer === 4, JSON.stringify(a.map(x => x.title)))
                // ---- a slide that clears itself: the slide is shown first, and then its actions are done
                commitAction({ slide: kept.slide }, { kind: "clear", layer: 5 }, "")
                goLive(kept.slide - 1)
                goLive(kept.slide)
                return 500
            },
            () => {
                check("a slide with an action that clears the slide is taken off by it: the slide first, then its actions", cleared && clearedByCue && liveSlide === null && liveIndex === kept.slide)
                check("it is still marked as the live slide", slideCell(kept.slide).live === true && cueLive)
                testInput.grab("1b-cleared-itself")
                testInput.key(Qt.Key_Right)
                check("and the arrow key goes on from it to the next", liveIndex === kept.slide + 1 && !cleared && !clearedByCue && slideCell(kept.slide + 1).live, liveIndex)
                clearSlide()
                check("a slide cleared by hand is not marked", cleared && !clearedByCue && !slideCell(kept.slide + 1).live)
                testInput.key(Qt.Key_Right)
                check("and the arrow key brings that one back", liveIndex === kept.slide + 1 && !cleared)
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says the slide was cleared by its own action", lines.some(l => l.includes("the slide, by its own action")))
                sidePanel.showControlTab = "macros"
                return 400
            },
            () => {
                // ---- macros
                const rows = macroRows()
                const first = rows[0]
                kept.macro = first.modelData
                const glyphs = Lib.findAll(Lib.find(first, i => i.objectName === "macroActions"), i => i.kind !== undefined && i.ink !== undefined)
                const block = Lib.find(first, i => i.objectName === "macroBlock")
                const picture = Lib.find(block, i => i.letter !== undefined && i.size !== undefined)
                check("a macro's row is tall enough for two lines, and plain; its colour is a rounded block round its picture", rows.length >= 4 && first.height === 46
                      && String(first.color) === "#2b2d31" && kept.macro.color === "#fffb00" && String(block.color) === "#fffb00" && block.radius > 4, first.color + " " + block.color)
                check("on a pale colour the picture is drawn dark", String(picture.ink) === "#15161a", picture.ink)
                const plain = rows.find(r => r.modelData.color === "")
                check("one with no colour of its own is royal blue", plain === undefined || String(plain.tint) === "#4169e1", plain ? plain.tint : "every macro here has a colour")
                check("with its picture, the M in brackets, and a small picture for each of its actions under its name", Lib.find(first, i => i.letter !== undefined && i.size !== undefined) !== null
                      && glyphs.length === kept.macro.actions.length && glyphs.map(g => g.kind).join() === kept.macro.actions.map(a => a.kind).join(), glyphs.map(g => g.kind).join())
                const tab = Lib.findAll(sidePanel, i => i.chosen !== undefined && i.modelData !== undefined && i.modelData.id === "macros")[0]
                check("the tab has the same picture", Lib.find(tab, i => i.letter !== undefined && i.size !== undefined && i.visible) !== null)
                testInput.grab("2-macros")
                const panel = named("macroList").parent
                kept.macros = panel
                panel.showMacroMenu(kept.macro, first, 20, 20)
                return 300
            },
            () => {
                const stage = kept.macro.actions.find(a => a.kind === "stage")
                kept.stage = stage
                check("its menu has everything: its actions to add, to remove, and each that can be changed; and the macro's own",
                      labels() === "Run, Add Action, Remove Action, [Actions], " + stage.title + ", [Macro], Rename, Colour, Duplicate, Remove…", labels())
                click(menuRow(stage.title))
                return 300
            },
            () => {
                check("an action that can be changed leads to Edit and Remove", labels() === "Edit…, Remove", labels())
                testInput.grab("3-macro-menu")
                click(menuRow("Edit…"))
                return 400
            },
            () => {
                const dialog = Lib.find(win.contentItem, i => i.openTimer !== undefined)
                check("Edit opens the panel for it", dialog.mode === "stage" && dialog.existing !== null && dialog.existing.id === kept.stage.id && dialog.target.macro === kept.macro.id)
                testInput.key(Qt.Key_Escape)
                kept.macros.showMacroMenu(kept.macro, macroRows()[0], 20, 20)
                return 300
            },
            () => {
                click(menuRow("Colour"))
                return 300
            },
            () => {
                check("a macro's colour is chosen from a few", labels() === "[Colour], Royal Blue, Red, Orange, Yellow, Green, Teal, Blue, Purple, Pink, Grey", labels())
                click(menuRow("Red"))
                return 500
            },
            () => {
                check("chosen, the macro is that colour, in the file and round its picture", Macros.find(kept.macro.id).color === "#d32f2f"
                      && String(Lib.find(macroRows()[0], i => i.objectName === "macroBlock").color) === "#d32f2f", Macros.find(kept.macro.id).color)
                report(Macros.setColor(kept.macro.id, ""))
                check("with its colour taken away it is royal blue", Macros.find(kept.macro.id).color === "" && String(macroRows()[0].tint) === "#4169e1", macroRows()[0].tint)
                const before = Macros.find(kept.macro.id).actions.length
                kept.before = before
                dragTo(centre(named("clearAll")), centre(macroRows()[0]))
                return 250
            },
            () => {
                drop()
                return 500
            },
            () => {
                const now = Macros.find(kept.macro.id).actions
                check("a clear button dragged onto a macro gives the macro that action", now.length === kept.before + 1 && now[now.length - 1].kind === "clear" && now[now.length - 1].layer === 0, now.map(a => a.title).join(" | "))
                kept.macros.showMacroMenu(Object.assign({}, kept.macro, { actions: now }), macroRows()[0], 20, 20)
                return 300
            },
            () => {
                click(menuRow("Remove Action"))
                return 300
            },
            () => {
                const now = Macros.find(kept.macro.id).actions
                check("Remove Action lists them all", labels() === "[Remove Action], " + now.map(a => a.title).join(", "), labels())
                click(menuRow("Clear everything"))
                return 500
            },
            () => {
                check("and removes the one chosen", Macros.find(kept.macro.id).actions.length === kept.before)
                // ---- a slide without its media, and its media without the slide
                openEntry(entry("When Wind Meets Fire"))
                return 600
            },
            () => {
                kept.video = document.slides.findIndex(s => s.mediaName !== "" && s.mediaVideo && s.media !== undefined)
                kept.plain = document.slides.findIndex(s => s.mediaName === "")
                check("a slide with a video that can be played", kept.video >= 0 && kept.plain >= 0, kept.video)
                clearAll()
                const cell = slideCell(kept.video)
                const picture = Lib.find(cell, i => i.source !== undefined && i.fillMode !== undefined && String(i.source).includes("thumbnail"))
                kept.picture = picture
                check("its thumbnail has its media behind it", picture !== null && picture.visible)
                testInput.keyDown(Qt.Key_Alt)
                check("with Alt held the thumbnails are drawn without their media", Cursors.altHeld && !picture.visible)
                const p = centre(cell)
                testInput.mouse(0, p.x, p.y, Qt.AltModifier)
                testInput.mouse(2, p.x, p.y, Qt.AltModifier)
                testInput.keyUp(Qt.Key_Alt)
                return 600
            },
            () => {
                check("and a click shows the slide without it", liveIndex === kept.video && !cleared && liveMedia === null && !Cursors.altHeld && kept.picture.visible, liveIndex + " " + (liveMedia === null))
                clearAll()
                showSlideMenu(kept.video, slideCell(kept.video), 30, 30)
                return 300
            },
            () => {
                const caption = header("Media")
                check("the Media caption of a slide with media is something to click", caption !== null && caption.pressable === true)
                testInput.grab("4-media-caption")
                click(centre(caption))
                return 2500
            },
            () => {
                check("which plays the slide's media and leaves the slide layer as it was", liveMedia !== null && liveMedia.path === document.slides[kept.video].media.path && cleared && player() !== null && player().position > 200,
                      (liveMedia !== null) + " " + cleared)
                showSlideMenu(kept.plain, slideCell(kept.plain), 30, 30)
                return 300
            },
            () => {
                check("a slide with no media has a caption that is only a caption", header("Media") !== null && header("Media").pressable === false)
                testInput.key(Qt.Key_Escape)
                // ---- when a background video is started again
                const media = document.slides[kept.video].media
                check("a looping background that is playing is left to play", media.playback === 1 && alreadyPlaying(media))
                report(catalog.setSlideMediaPlayback(document.path, document.slides[kept.video].id, 2, 3, 0))
                reloadDocument()
                check("set to play another way, it is started again when the slide is next shown", !alreadyPlaying(document.slides[kept.video].media))
                goLive(kept.video)
                return 1500
            },
            () => {
                check("which it was: it now plays as it was set", liveMedia.playback === 2 && player().loops === 3 && alreadyPlaying(document.slides[kept.video].media))
                report(catalog.setSlideMediaPlayback(document.path, document.slides[kept.video].id, 0, 0, 0))
                reloadDocument()
                goLive(kept.video)
                return 1500
            },
            () => {
                check("one that stops at its end is started again every time", liveMedia.playback === 0 && !alreadyPlaying(document.slides[kept.video].media))
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says a slide was shown without its media, and media without its slide", lines.some(l => l.includes("without its media, as asked")) && lines.some(l => l.includes("without the slide")))
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("was not found in the workspace")
                                                                    && !l.includes("eglCreateImage") && !l.includes("failed to get textures")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme")).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
