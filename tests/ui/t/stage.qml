import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The stage screen and its layouts, on ProPresenter's own Stage file: choosing a layout,
// what its linked boxes show as slides go live, a rule about a timer, a new layout and
// the editor on it (the yellow outline, what a box is linked to, the inspector's list),
// renaming, copying and removing layouts. Then ProPresenter's own props.
QtObject {
    id: t

//COMMON
    function tabs() {
        return Lib.findAll(sidePanel, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
    }

    function layout(name) {
        return StageLayouts.layouts.find(l => l.name === name)
    }

    function timer(name) {
        return Timers.timers.find(x => x.name === name)
    }

    // The elements the stage preview is drawing: the same layout as the stage window's
    function drawn() {
        const preview = Lib.findAll(sidePanel, item => item.slide !== undefined && item.slideWidth !== undefined && item.showsNow !== undefined)
                           .find(item => item.visible && stageLayout !== null && item.slide !== null && item.slide.label === stageLayout.name)
        return preview ? Lib.findAll(preview, item => item.source !== undefined && item.textBleed !== undefined && item.boxX !== undefined) : []
    }

    function box(test) {
        return drawn().find(item => test(item.source))
    }

    function words(element) {
        return testInput.plain(element.text).trim()
    }

    function choice(test) {
        return Lib.findAll(editScreen.inspector, item => item.choice !== undefined && item.model !== undefined && item.chosen !== undefined).find(c => c.visible && test(c))
    }

    function showsChoice() {
        return choice(c => c.model.length > 0 && c.model[0] === "its own text")
    }

    function marks() {
        return Lib.findAll(editScreen.canvas, item => item.objectName === "linkMark" && item.visible)
    }

    function captions() {
        return marks().map(m => m.caption).join(" | ")
    }

    function doneButton() {
        return named("showButton")
    }

    function editorRows() {
        return Lib.findAll(editScreen, item => item.slide !== undefined && item.current !== undefined && item.renaming !== undefined && item.index !== undefined)
                  .sort((a, b) => a.index - b.index)
    }

    function propRows() {
        return Lib.findAll(named("propList"), item => item.modelData !== undefined && item.modelData !== null && item.naming !== undefined)
    }

    // How much of the stage window under its bar is lit
    function stageLit() {
        return testInput.lit(true, 0, 22 / 189, 1, 167 / 189)
    }

    function run() {
        const lyrics = slide => slide.elements.filter(e => e.name === "Lyrics" && e.visible).map(e => testInput.plain(e.displayText).trim()).filter(x => x !== "").join("\n")
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "MainHall").path)
                openEntry(entry("Build My Life"))
                check("ProPresenter's stage layouts are read", StageLayouts.layouts.map(l => l.name).join("|") === "Singing|Slide Notes|Live Slides|Triple Preview",
                      StageLayouts.layouts.map(l => l.name).join("|"))
                check("each is a slide, with a size and things on it", StageLayouts.layouts.every(l => l.slide.width > 0 && l.slide.height > 0 && l.slide.elements.length > 0),
                      StageLayouts.layouts.map(l => l.slide.width + "x" + l.slide.height + ":" + l.slide.elements.length).join(" "))
                check("the stage starts with the plain view", stageLayoutId === "" && stageLayout === null)
                kept.stageHash = testInput.fileHash(StageLayouts.path)
                kept.propsHash = testInput.fileHash(Props.path)
                click(centre(tabs()[2]))
                return 400
            },
            () => {
                const combo = named("stageLayoutChoice")
                check("the Stage tab lists the one stage screen", sidePanel.showControlTab === "stage" && named("stageScreenRow") !== null && named("stageScreenRow").visible)
                check("with a drop-down of the plain view and the layouts", combo !== null && combo.model.join("|") === "Words, now and next|Singing|Slide Notes|Live Slides|Triple Preview"
                      && combo.currentIndex === 0, combo ? combo.model.join("|") : "none")
                check("and a way to edit them", named("stageLayoutEdit").enabled)
                testInput.grab("1-stage-tab")
                goLive(0)
                return 1500
            },
            () => {
                kept.plain = stageLit()
                check("the plain view shows the words", kept.plain > 0.02, kept.plain)
                testInput.grabStage("1-stage-plain")
                click(centre(named("stageLayoutChoice")))
                return 500
            },
            () => {
                const combo = named("stageLayoutChoice")
                const row = Lib.find(combo.popup.contentItem, item => item.modelData === "Singing" && item.highlighted !== undefined)
                check("the drop-down opens", combo.popup.opened && row !== null)
                click(centre(row))
                return 1200
            },
            () => {
                // ---- ProPresenter's "Singing" layout
                check("choosing a layout gives it to the stage", stageLayout !== null && stageLayout.name === "Singing" && stageLayoutId === layout("Singing").id
                      && named("stageLayoutChoice").currentIndex === 1)
                const current = box(e => e.linkKind === "slideText" && !e.linkSlideNext)
                const next = box(e => e.linkKind === "slideText" && e.linkSlideNext)
                check("it has a box for the live slide's words and one for the next's", current !== undefined && next !== undefined
                      && current.source.linkSlideSource === Show.ElementNamed && current.source.linkSlideName === "Lyrics", current ? JSON.stringify([current.source.linkSlideSource, current.source.linkSlideName]) : "none")
                check("the first shows the words of the slide that is live", current.liveLinkText === lyrics(liveSlide) && current.liveLinkText.length > 10 && current.visible, JSON.stringify(current.liveLinkText))
                check("the second those of the slide after it", next.liveLinkText === lyrics(nextSlide) && next.liveLinkText.length > 10 && next.liveLinkText !== current.liveLinkText, JSON.stringify(next.liveLinkText))
                check("which are plain words, whatever the slide does to them", !current.liveLinkText.includes("\\") && current.liveLinkText === Show.slideText(false, Show.ElementNamed, "lyrics", 0))
                const clock = box(e => e.name === "Clock")
                check("a box linked to something the app does not follow shows nothing, not its sample", clock !== undefined && clock.source.linkKind === "other"
                      && clock.source.linkLabel === "Clock" && !clock.source.hasText && words(clock.source) === "1:23 PM", clock ? JSON.stringify([clock.source.linkKind, clock.source.hasText, words(clock.source)]) : "none")
                const time = box(e => e.name === "Timer")
                check("a box linked to a timer shows the timer", time !== undefined && time.source.linkKind === "timer" && time.liveLinkText === "3:00", time ? time.liveLinkText : "none")
                const live = box(e => words(e).replace(/\s+/g, " ") === "NLC LIVE")
                check("a box that shows while a timer runs does not show while it does not", live !== undefined && live.source.visibilityTimed && !live.visible, live ? live.visible : "none")
                kept.stage = stageLit()
                check("the stage window draws the layout", kept.stage > 0.01 && kept.stage !== kept.plain, kept.stage)
                testInput.grabStage("2-stage-singing")
                testInput.grab("2-singing")
                Timers.start(timer("NLC Live").id)
                Timers.start(timer("Timer").id)
                return 1400
            },
            () => {
                const live = box(e => words(e).replace(/\s+/g, " ") === "NLC LIVE")
                check("and shows once it runs", live.visible)
                const time = box(e => e.name === "Timer")
                check("the timer's box counts with the timer", time.liveLinkText === "2:59" || time.liveLinkText === "2:58", time.liveLinkText)
                testInput.grabStage("3-stage-timer-running")
                Timers.stop(timer("NLC Live").id)
                Timers.reset(timer("NLC Live").id)
                Timers.stop(timer("Timer").id)
                Timers.reset(timer("Timer").id)
                goLive(1)
                return 1200
            },
            () => {
                const live = box(e => words(e).replace(/\s+/g, " ") === "NLC LIVE")
                check("and goes when it stops", !live.visible)
                const current = box(e => e.linkKind === "slideText" && !e.linkSlideNext)
                const next = box(e => e.linkKind === "slideText" && e.linkSlideNext)
                check("the words follow the show: the next slide's are now the live one's", current.liveLinkText === lyrics(document.slides[1]) && next.liveLinkText === lyrics(document.slides[2]),
                      JSON.stringify([current.liveLinkText, next.liveLinkText]))
                clearSlide()
                return 900
            },
            () => {
                const current = box(e => e.linkKind === "slideText" && !e.linkSlideNext)
                check("with the slide cleared there are no live words", current.liveLinkText === "", JSON.stringify(current.liveLinkText))
                check("the files are as ProPresenter left them", testInput.fileHash(StageLayouts.path) === kept.stageHash && testInput.fileHash(Props.path) === kept.propsHash)
                // ---- a layout of the app's own
                click(centre(named("showControlAdd")))
                return 1000
            },
            () => {
                check("the + makes a layout, gives it to the stage and opens it in the editor", StageLayouts.layouts.length === 5 && StageLayouts.layouts[4].name === "Layout"
                      && stageLayoutId === StageLayouts.layouts[4].id && editing && editScreen.editor.kind === "stage" && editScreen.editor.count === 5 && editScreen.canvas.row === 4,
                      StageLayouts.layouts.map(l => l.name).join("|") + " row " + editScreen.canvas.row)
                check("the editor says what is being edited", editScreen.subject === "Stage Layouts", editScreen.subject)
                const elements = editScreen.canvas.elements
                check("it starts as the words of the live slide over those of the next", elements.length === 2 && elements[0].linkKind === "slideText" && !elements[0].linkSlideNext
                      && elements[0].linkSlideSource === Show.Words && elements[1].linkKind === "slideText" && elements[1].linkSlideNext && elements[0].name === "Current Slide"
                      && elements[1].name === "Next Slide" && editScreen.canvas.slide.drawsBackground, JSON.stringify(elements.map(e => [e.name, e.linkKind, e.linkSlideNext, e.linkSlideSource])))
                check("each linked box has a yellow outline, and says what it is linked to", marks().length === 2 && captions() === "Current Slide: Text | Next Slide: Text"
                      && marks().every(m => Qt.colorEqual(m.border.color, "#ffd400")), captions())
                const drawnHere = Lib.findAll(editScreen.canvas, item => item.source !== undefined && item.liveLinkText !== undefined && item.sampled !== undefined)
                check("with no slide live, a box shows its own text to set its look by", drawnHere.length === 2 && drawnHere[0].sampled && testInput.plain(drawnHere[0].textOverride) === "Current Slide",
                      drawnHere.map(d => d.sampled).join())
                check("and the next slide's words where there are some", !drawnHere[1].sampled && drawnHere[1].liveLinkText === nextSlide.plainText && drawnHere[1].liveLinkText.length > 5, JSON.stringify(drawnHere[1].liveLinkText))
                testInput.grab("4-new-layout")
                editScreen.canvas.pick(elements[0].id)
                return 400
            },
            () => {
                const shows = showsChoice()
                editScreen.inspector.tab = "text"
                kept.model = shows ? shows.model : []
                return 300
            },
            () => {
                const shows = showsChoice()
                check("the inspector lists the two slides among what a box can show", shows !== undefined && shows.model.includes("the current slide's text") && shows.model.includes("the next slide's text")
                      && shows.model.includes("the timer “Timer”") && shows.model.includes("the text of “Next Slide”"), shows ? shows.model.join(" | ") : "none")
                check("and has this one as the current slide's", shows.model[shows.choice] === "the current slide's text", shows.model[shows.choice])
                testInput.grab("5-inspector")
                shows.chosen(shows.model.indexOf("the next slide's text"))
                return 400
            },
            () => {
                const e = editScreen.canvas.selected
                check("choosing the next slide links the box to that", e.linkKind === "slideText" && e.linkSlideNext && e.linkSlideSource === Show.Words && captions().startsWith("Next Slide: Text"), captions())
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("the timer “Timer”"))
                return 400
            },
            () => {
                const e = editScreen.canvas.selected
                check("choosing a timer links it to the timer instead", e.linkKind === "timer" && e.linkTimerName === "Timer" && captions().startsWith("Timer: Timer"), captions())
                const onDisk = StageLayouts.reload() === "" ? StageLayouts.layouts[4].slide.elements[0] : null
                check("which is what is in the file", onDisk !== null && onDisk.linkKind === "timer" && onDisk.linkTimerId === timer("Timer").id, onDisk ? onDisk.linkKind : "none")
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("the current slide's text"))
                return 400
            },
            () => {
                const e = editScreen.canvas.selected
                check("and back to the current slide's words", e.linkKind === "slideText" && !e.linkSlideNext && captions() === "Current Slide: Text | Next Slide: Text", captions())
                check("each of those is a step that can be undone", editScreen.editor.canUndo)
                // a box of its own text has no mark
                editScreen.canvas.addText()
                testInput.type("Plain")
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("a box with text of its own has no outline", editScreen.canvas.elements.length === 3 && marks().length === 2)
                editScreen.canvas.removeSelected()
                // ---- ProPresenter's layout, in the editor
                editScreen.showRow(0)
                return 500
            },
            () => {
                check("ProPresenter's own layout is there to edit too, each linked box marked with what it is", editScreen.canvas.slide.label === "Singing" && marks().length >= 5
                      && captions().includes("Current Slide: “Lyrics”") && captions().includes("Next Slide: “Lyrics”") && captions().includes("Timer: Timer")
                      && captions().includes("Clock") && captions().includes("Video countdown"), captions())
                const clock = Lib.findAll(editScreen.canvas, item => item.source !== undefined && item.sampled !== undefined).find(d => d.source.name === "Clock")
                check("and the clock's box shows its sample here", clock !== undefined && clock.sampled && testInput.plain(clock.textOverride).trim() === "1:23 PM")
                editScreen.canvas.pick(editScreen.canvas.elements.find(e => e.linkKind === "slideText" && !e.linkSlideNext).id)
                testInput.grab("6-singing-in-editor")
                return 400
            },
            () => {
                const shows = showsChoice()
                check("a box linked to a slide's elements of a name is listed as that", shows !== undefined && shows.model[shows.choice] === "the current slide's “Lyrics” text", shows ? shows.model[shows.choice] : "none")
                check("nothing was changed by looking", testInput.fileHash(StageLayouts.path) !== kept.stageHash && StageLayouts.reload() === "" && StageLayouts.layouts[0].slide.elements.length === layout("Singing").slide.elements.length)
                // ---- renaming, copying and removing, from the editor's list
                const rows = editorRows()
                check("the editor lists the five layouts by name", rows.length >= 1 && editScreen.editor.count === 5 && editScreen.editor.slideAt(4).label === "Layout")
                editScreen.showRow(4)
                return 400
            },
            () => {
                const row = editorRows().find(r => r.index === 4)
                const p = row.mapToItem(null, row.width / 2, row.height / 2)
                click(p, Qt.RightButton)
                return 400
            },
            () => {
                check("a layout's menu in the editor", menu.opened && labels() === "[Layout], Rename, Duplicate, Remove…", labels())
                click(menuRow("Rename"))
                return 500
            },
            () => {
                check("Rename puts a box where the name is", editScreen.renamingRow === StageLayouts.layouts[4].id)
                testInput.type("Words and Timer\n")
                return 600
            },
            () => {
                check("and the name typed is the layout's", StageLayouts.layouts[4].name === "Words and Timer" && editScreen.editor.slideAt(4).label === "Words and Timer"
                      && editScreen.canvas.row === 4 && editScreen.renamingRow === "", StageLayouts.layouts.map(l => l.name).join("|"))
                check("still the stage's layout", stageLayout !== null && stageLayout.name === "Words and Timer")
                const row = editorRows().find(r => r.index === 4)
                click(row.mapToItem(null, row.width / 2, row.height / 2), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Duplicate"))
                return 600
            },
            () => {
                check("Duplicate makes a copy after it and shows that", StageLayouts.layouts.length === 6 && StageLayouts.layouts[5].name === "Words and Timer 2" && editScreen.editor.count === 6
                      && editScreen.canvas.row === 5 && StageLayouts.layouts[5].id !== StageLayouts.layouts[4].id && StageLayouts.layouts[5].slide.elements.length === 2,
                      StageLayouts.layouts.map(l => l.name).join("|") + " row " + editScreen.canvas.row)
                const row = editorRows().find(r => r.index === 5)
                editScreen.showRow(5)
                click(row.mapToItem(null, row.width / 2, row.height / 2), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Remove…"))
                return 500
            },
            () => {
                check("removing asks first", menu.opened && menu.items[0].note !== undefined && menu.items[0].note.includes("Words and Timer 2"), JSON.stringify(menu.items.map(i => i.note ?? i.label)))
                click(menuRow("Remove"))
                return 600
            },
            () => {
                check("and then removes it, showing the one before", StageLayouts.layouts.length === 5 && editScreen.editor.count === 5 && editScreen.canvas.row === 4
                      && editScreen.canvas.slide.label === "Words and Timer", StageLayouts.layouts.map(l => l.name).join("|") + " row " + editScreen.canvas.row)
                click(centre(named("editorAddRow")))
                return 600
            },
            () => {
                check("the editor's own + adds a layout", StageLayouts.layouts.length === 6 && StageLayouts.layouts[5].name === "Layout" && editScreen.canvas.row === 5)
                check("without giving it to the stage", stageLayout.name === "Words and Timer")
                StageLayouts.remove(StageLayouts.layouts[5].id)
                editScreen.openStage(StageLayouts.path, catalog.workspacePath, stageLayoutId)
                // the second box of the stage's layout becomes a timer, to see it on the stage
                editScreen.canvas.pick(editScreen.canvas.elements[1].id)
                return 400
            },
            () => {
                const shows = showsChoice()
                shows.chosen(shows.model.indexOf("the timer “Timer”"))
                return 400
            },
            () => {
                doneButton().clicked()
                return 500
            },
            () => {
                check("Done goes back to the show", !editing && keys.activeFocus && named("gridTitle").text === "Build My Life")
                goLive(3)
                return 1300
            },
            () => {
                const current = box(e => e.linkKind === "slideText")
                const time = box(e => e.linkKind === "timer")
                check("the stage shows the layout as it was left: the live words", current !== undefined && current.liveLinkText === liveSlide.plainText && current.liveLinkText.length > 5, current ? JSON.stringify(current.liveLinkText) : "none")
                check("and the timer, written the way a newly linked box writes one", time !== undefined && time.liveLinkText === "03:00", time ? time.liveLinkText : "none")
                check("the stage window is lit by it", stageLit() > 0.01, stageLit())
                testInput.grabStage("7-stage-own-layout")
                testInput.grab("7-own-layout")
                testInput.snapshot(StageLayouts.path, "Stage-after")
                // ---- ProPresenter's props
                click(centre(tabs()[1]))
                return 500
            },
            () => {
                const rows = propRows()
                check("ProPresenter's props are listed by collection, the first shown", rows.map(r => r.modelData.name).join("|") === "Service Countdown|Service Countdown BIG|Web Effects"
                      && named("propCollection").model.join("|") === "Default Collection  (one at a time)|New Collection", rows.map(r => r.modelData.name).join("|") + " in " + named("propCollection").model.join("|"))
                check("the first of which shows one at a time, as ProPresenter has it set", Props.collections[0].single === true && Props.collections[1].single === false)
                check("they dissolve over the second ProPresenter gives them", Props.transitionDuration === 1, Props.transitionDuration)
                testInput.grab("8-props")
                click(centre(rows[0]))
                return 1800
            },
            () => {
                const id = Props.collections[0].props[0].id
                check("one turned on is on the output, over the slide", liveProps.join() === id && shownProps.length === 1)
                const linked = shownProps[0].slide.elements.filter(e => e.linkKind === "timer")
                check("its box is linked to ProPresenter's timer, found here", linked.length === 1 && Timers.linkedTimer(linked[0].linkTimerId, linked[0].linkTimerName) === timer("Service Countdown").id,
                      JSON.stringify(linked.map(e => [e.linkTimerName, e.linkTimerId])))
                testInput.grabOutput("9-output-prop")
                click(centre(propRows()[2]))
                return 1800
            },
            () => {
                check("another of the same collection takes its place", liveProps.join() === Props.collections[0].props[2].id, liveProps.join())
                testInput.grabOutput("10-output-other-prop")
                check("turning props on and off changes nothing in the file", testInput.fileHash(Props.path) === kept.propsHash)
                testInput.key(Qt.Key_F4)
                return 300
            },
            () => {
                check("F4 clears them", liveProps.length === 0)
                return 100
            }
        ]
        next()
    }
}
