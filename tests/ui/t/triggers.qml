import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A timer's Color Triggers (src/colortriggers.h): ProPresenter's own, read from the stage layouts it wrote (a red one
// at nothing); in the editor's Linked Text, for a box linked to a timer, added with +, a time typed, a colour picked, one
// dragged to another place in the list, one taken away, the list kept as the default and put back from it; what is in
// the file afterwards, with the rest of the link as it was; and on the output, the time's words drawn in the colour of
// the trigger in force as the countdown comes down past it, in the box's own colour while it is above them all, and in
// the last one's through an overrun. What is looked at on the output is the colour drawn.
QtObject {
    id: t

//COMMON
    function timer(name) {
        return Timers.timers.find(x => x.name === name)
    }

    function inspector() {
        return editScreen.inspector
    }

    function sel() {
        return editScreen.canvas.selected
    }

    function triggers() {
        return (sel() ? sel().linkColorTriggers ?? [] : []).map(x => Timers.triggerTime(x.time) + " " + String(x.color))
    }

    function onDisk() {
        return catalog.open(kept.entry.file).slides[kept.index].elements.find(e => e.id === kept.element.id)
    }

    // The link as the file has it, but its triggers
    function linkOf(e) {
        return JSON.stringify([e.linkKind, e.linkTimerId, e.linkTimerName, e.linkTimerHours, e.linkTimerMinutes, e.linkTimerSeconds,
                               e.linkTimerHundredths, e.linkTimerPattern])
    }

    // How much of the box's part of the output is drawn red
    function redOnOutput() {
        const e = kept.element
        return testInput.red(false, e.x / 1920, e.y / 1080, e.width / 1920, e.height / 1080)
    }

    function liveText() {
        return Lib.findAll(output.contentItem, item => item.replacement !== undefined && typeof item.replacement === "string" && item.visible)[0]
    }

    function run() {
        steps = [
            () => {
                // ---- ProPresenter's own, in its stage layouts
                let linked = []
                for (const layout of StageLayouts.layouts)
                    linked = linked.concat(Array.from(layout.slide.elements).filter(e => e.linkKind === "timer"))
                const own = linked.filter(e => (e.linkColorTriggers ?? []).length > 0)
                check("the stage layouts ProPresenter wrote have a box linked to a timer with a Color Trigger: red, at nothing", own.length > 0
                      && own.every(e => e.linkColorTriggers.length === 1 && e.linkColorTriggers[0].time === 0 && String(e.linkColorTriggers[0].color) === "#ff2d21"),
                      JSON.stringify(own.map(e => e.linkColorTriggers)))
                // ---- A slide's box linked to a timer
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = catalog.open(d.file).slides
                        const at = slides.findIndex(s => s.elements.some(e => e.linkKind === "timer"))
                        if (at >= 0 && !kept.entry) {
                            kept.entry = d
                            kept.index = at
                            kept.element = slides[at].elements.find(e => e.linkKind === "timer")
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a slide with a box linked to a timer, with no triggers of its own)", kept.entry !== undefined
                      && (kept.element.linkColorTriggers ?? []).length === 0, kept.entry ? kept.entry.name : "none")
                kept.link = linkOf(kept.element)
                openEntry(kept.entry)
                startEditing(kept.entry, document.slides[kept.index].id)
                return 800
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                inspector().tab = "text"
                return 400
            },
            () => {
                check("the Linked Text has Color Triggers, with none yet", named("addTrigger") !== null && named("addTrigger").visible && triggers().length === 0)
                // (A button too small for its padding draws nothing of its sign: these were once so.)
                check("its + and ⋯ show their signs", ["addTrigger", "triggerDefaults"].every(n => Lib.find(named(n), item => item.text !== undefined && item.elide !== undefined
                      && item.paintedWidth > 4) !== null))
                click(centre(named("addTrigger")))
                return 400
            },
            () => {
                check("+ adds one: at nothing, in red", triggers().join(" | ") === "0:00.00 #ff2d21", triggers().join(" | "))
                click(centre(named("addTrigger")))
                return 400
            },
            () => {
                check("and another, half a minute before the latest, in yellow", triggers().join(" | ") === "0:00.00 #ff2d21 | 0:30.00 #ffcc00", triggers().join(" | "))
                const field = named("triggerTime:1")
                field.forceActiveFocus()
                field.selectAll()
                testInput.type("1:00")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("a time typed is kept, and written as ProPresenter writes it", triggers()[1] === "1:00.00 #ffcc00" && named("triggerTime:1").text === "1:00.00",
                      triggers().join(" | "))
                const field = named("triggerTime:1")
                field.forceActiveFocus()
                field.selectAll()
                testInput.type("soon")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("what is not a time is not kept", triggers()[1] === "1:00.00 #ffcc00" && named("triggerTime:1").text === "1:00.00", named("triggerTime:1").text)
                named("triggerColor:1").picked("#ff0000")
                return 400
            },
            () => {
                check("a colour picked is kept", triggers()[1] === "1:00.00 #ff0000", triggers().join(" | "))
                click(centre(named("addTrigger")))
                return 400
            },
            () => {
                check("(a third, at 1:30)", triggers().join(" | ") === "0:00.00 #ff2d21 | 1:00.00 #ff0000 | 1:30.00 #ffcc00", triggers().join(" | "))
                // (The inspector scrolled to bring the list into view, as it would be by hand: it is below the window's
                // foot as the inspector opens.)
                const flick = Lib.find(inspector(), item => item.contentY !== undefined && item.flicking !== undefined)
                const top = named("triggerGrip:0").mapToItem(flick.contentItem, 0, 0).y
                flick.contentY = Math.max(0, Math.min(top - flick.height / 3, flick.contentHeight - flick.height))
                return 300
            },
            () => {
                // Dragged by its handle from the bottom to the top
                const grip = named("triggerGrip:2")
                const from = grip.mapToItem(null, grip.width / 2, grip.height / 2)
                const to = named("triggerGrip:0").mapToItem(null, grip.width / 2, 2)
                testInput.mouse(4, from.x, from.y)
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 12; ++i)
                    testInput.mouse(1, from.x, from.y + (to.y - 6 - from.y) * i / 12)
                testInput.mouse(2, from.x, to.y - 6)
                return 500
            },
            () => {
                check("a trigger dragged by its handle goes to where it is let go: the list in another order", triggers().join(" | ")
                      === "1:30.00 #ffcc00 | 0:00.00 #ff2d21 | 1:00.00 #ff0000", triggers().join(" | "))
                const disk = onDisk()
                check("all of it is in the file, in that order, and the rest of the link as it was", disk !== undefined && linkOf(disk) === kept.link
                      && disk.linkColorTriggers.map(x => Timers.triggerTime(x.time) + " " + String(x.color)).join(" | ") === triggers().join(" | "),
                      disk ? JSON.stringify(disk.linkColorTriggers) : "none")
                testInput.grab("1-triggers")
                // ---- The default
                kept.save = named("triggerDefaults")
                click(centre(kept.save))
                return 400
            },
            () => {
                check("⋯ offers Save as Default and Recall Default, there being none kept yet", labels().startsWith("Save as Default, Recall Default(off)"), labels())
                click(menuRow("Save as Default"))
                return 400
            },
            () => {
                check("Save as Default keeps the list, in the app's own settings", colorTriggerDefault.map(x => Timers.triggerTime(x.time) + " " + x.color).join(" | ")
                      === triggers().join(" | ") && String(settings.value("colorTriggerDefault", "")).includes("#ff0000"), JSON.stringify(colorTriggerDefault))
                click(centre(named("removeTrigger:0")))
                return 400
            },
            () => {
                check("× takes one away", triggers().join(" | ") === "0:00.00 #ff2d21 | 1:00.00 #ff0000", triggers().join(" | "))
                check("(its × shows)", Lib.find(named("removeTrigger:0"), item => item.text === "×" && item.elide !== undefined && item.paintedWidth > 4) !== null)
                click(centre(named("removeTrigger:0")))
                return 400
            },
            () => {
                click(centre(named("removeTrigger:0")))
                return 400
            },
            () => {
                check("(all taken away)", triggers().length === 0 && onDisk().linkColorTriggers.length === 0)
                click(centre(kept.save))
                return 400
            },
            () => {
                click(menuRow("Recall Default"))
                return 400
            },
            () => {
                check("Recall Default puts the default list on the box", triggers().join(" | ") === "1:30.00 #ffcc00 | 0:00.00 #ff2d21 | 1:00.00 #ff0000"
                      && onDisk().linkColorTriggers.length === 3, triggers().join(" | "))
                stopEditing()
                return 600
            },
            () => {
                // ---- On the output. The slide sets the timer to a minute's countdown and starts it: 0:59 has come
                // down past 1:00, which is red.
                goLive(kept.index)
                return 1600
            },
            () => {
                const text = liveText()
                check("the slide's countdown is on the output", text !== undefined && /^5\d$/.test(text.replacement), text ? text.replacement : "none")
                check("drawn in the colour of the trigger it has come down past: red at 1:00", text !== undefined && String(text.replacementColor) === "#ff0000"
                      && redOnOutput() > 0.01, redOnOutput())
                testInput.grabOutput("2-red-under-a-minute")
                Timers.stop(timer("Timer").id)
                Timers.configure(timer("Timer").id, { duration: 150 })
                Timers.reset(timer("Timer").id)
                return 1200
            },
            () => {
                const text = liveText()
                check("above all the triggers (2:30), the box's own colour", text !== undefined && text.replacementColor === undefined && redOnOutput() < 0.002,
                      (text ? text.replacement + " " + text.replacementColor : "none") + " / " + redOnOutput())
                testInput.grabOutput("3-own-colour")
                Timers.configure(timer("Timer").id, { duration: 1, overrun: true })
                Timers.reset(timer("Timer").id)
                Timers.start(timer("Timer").id)
                return 2600
            },
            () => {
                const text = liveText()
                check("past nothing, in an overrun, the colour of the last: ProPresenter's red at 0:00", text !== undefined && text.replacement.startsWith("-")
                      && String(text.replacementColor) === "#ff2d21" && redOnOutput() > 0.01, (text ? text.replacement + " " + text.replacementColor : "none") + " / " + redOnOutput())
                testInput.grabOutput("4-overrun")
                Timers.stop(timer("Timer").id)
                Timers.configure(timer("Timer").id, { duration: 180 })
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
