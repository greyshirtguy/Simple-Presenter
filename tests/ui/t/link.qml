import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// A text box linked to a timer: read from a ProPresenter file, shown live on the output
// and in the thumbnails, and set up in the editor.
QtObject {
    id: t

//COMMON
    function timer(name) {
        return Timers.timers.find(x => x.name === name)
    }

    // The text items of a window or view that are drawing words from a live link
    function liveTexts(root) {
        return Lib.findAll(root, item => item.replacement !== undefined && typeof item.replacement === "string")
    }

    function inspector() {
        return editScreen.inspector
    }

    function choice(test) {
        return Lib.findAll(inspector(), item => item.choice !== undefined && item.model !== undefined && item.chosen !== undefined).find(c => c.visible && test(c))
    }

    function showsChoice() {
        return choice(c => c.model.length > 0 && c.model[0] === "its own text")
    }

    // The drop-downs for the four parts of a timer's time, in the order they are laid out
    function partChoices() {
        return Lib.findAll(inspector(), item => item.choice !== undefined && item.model !== undefined && item.chosen !== undefined)
                  .filter(c => c.visible && c.model.length === 5 && c.model[0] === "Hidden")
                  .sort((a, b) => a.mapToItem(null, 0, 0).y - b.mapToItem(null, 0, 0).y)
    }

    // What the output's and the grid's linked texts were seen to show, while watching
    property var seenOnOutput: ({})
    property var seenInGrid: ({})
    property Timer watch: Timer {
        interval: 8
        repeat: true
        onTriggered: {
            for (const x of t.liveTexts(output.contentItem))
                if (x.visible)
                    t.seenOnOutput[x.replacement] = true
            for (const x of t.liveTexts(t.slideCell(t.kept.liveIndex)))
                t.seenInGrid[x.replacement] = true
        }
    }

    function onDisk() {
        return catalog.open(editScreen.editor.path).slides.find(s => s.id === editScreen.canvas.slide.id).elements.find(e => e.id === kept.elementId)
    }

    function run() {
        const canvas = () => editScreen.canvas
        const sel = () => editScreen.canvas.selected
        steps = [
            () => {
                check("the workspace's timers are read from ProPresenter's file", Timers.timers.length === 4
                      && Timers.timers.map(x => x.name).join("|") === "Timer|Service Countdown|NLC Live|Show Mini 👍", Timers.timers.map(x => x.name).join("|"))
                const a = timer("Timer"), b = timer("Service Countdown"), c = timer("NLC Live"), d = timer("Show Mini 👍")
                check("a countdown that may overrun", a.kind === "countdown" && a.duration === 180 && a.overrun && a.id === "9212EC2F-00DD-4A94-A017-D89A1521D6AA", JSON.stringify(a))
                check("a countdown to half past ten", b.kind === "countdownTo" && b.timeOfDay === 37800 && !b.overrun, JSON.stringify(b))
                check("an elapsed time from five minutes", c.kind === "elapsed" && c.startTime === 300 && c.overrun && !c.hasEndTime && Timers.state(c.id).text === "0:05:00", JSON.stringify(c))
                check("an elapsed time from nothing", d.kind === "elapsed" && d.startTime === 0 && d.overrun, JSON.stringify(d))
                // ---- the presentation with a text box linked to a timer
                let found = undefined
                for (const library of catalog.libraries) {
                    openLibrary(library.path)
                    found = entry("Media After Songs - Sun AM")
                    if (found)
                        break
                }
                check("found the presentation", found !== undefined)
                openEntry(found)
                kept.entry = found
                let where = -1, which = null
                document.slides.forEach((slide, i) => {
                    const linked = slide.elements.find(e => e.linkKind === "timer")
                    if (linked && where < 0) {
                        where = i
                        which = linked
                    }
                })
                kept.index = where
                kept.element = which
                check("it has a text box linked to a timer", where >= 0, "slide " + (where + 1) + " of " + document.slides.length)
                check("the link is read: which timer, and how its time is written", which.linkTimerName === "Timer" && which.linkTimerId.length === 36
                      && which.linkTimerHours === 0 && which.linkTimerMinutes === 0 && which.linkTimerSeconds === 1 && which.linkTimerPattern === "${timer}",
                      JSON.stringify([which.linkTimerName, which.linkTimerId, which.linkTimerHours, which.linkTimerMinutes, which.linkTimerSeconds, which.linkTimerPattern]))
                check("this file's link carries an id that is no timer's here, so it is found by its name", which.linkTimerId !== timer("Timer").id
                      && Timers.linkedTimer(which.linkTimerId, which.linkTimerName) === timer("Timer").id)
                check("it counts as having text, and shows", which.hasText && which.visible)
                const slide = document.slides[where]
                check("the timer is not among the slide's words", !/\b0\b/.test(slide.plainText) && !slide.plainText.includes("180"), JSON.stringify(slide.plainText))
                // ---- what the slide's cue does to the timer
                const actions = slide.actions.filter(a => a.kind === "timer")
                check("the slide's cue has an action for the timer: put it back and start it, as a minute's countdown", actions.length === 1 && actions[0].action === 3
                      && actions[0].timerName === "Timer" && actions[0].configuration !== undefined, JSON.stringify(actions))
                check("no other slide of it does anything to a timer", document.slides.filter(x => x.actions.some(a => a.kind === "timer")).length === 1)
                check("before the slide is triggered the timer is as the workspace has it", timer("Timer").duration === 180 && timer("Timer").overrun && !Timers.state(timer("Timer").id).running)
                kept.file = testInput.fileHash(catalog.workspacePath + "/Configuration/Timers")
                goLive(where)
                return 1500
            },
            () => {
                const a = timer("Timer")
                check("triggering the slide sets the timer up as its action says, and starts it", a.kind === "countdown" && a.duration === 60 && !a.overrun
                      && Timers.state(a.id).running, JSON.stringify(a))
                const texts = liveTexts(output.contentItem).filter(x => x.visible)
                check("on the output the box shows the timer's time, in seconds as the link says", texts.some(x => x.replacement === "59"), texts.map(x => x.replacement).join("|"))
                const inGrid = liveTexts(slideCell(kept.index))
                check("and so does the slide's thumbnail", inGrid.length === 1 && inGrid[0].replacement === "59", inGrid.map(x => x.replacement).join("|"))
                check("nothing was written for it", testInput.fileHash(catalog.workspacePath + "/Configuration/Timers") === kept.file && kept.file !== "")
                testInput.grabOutput("1-output-timer-59")
                testInput.grab("1-operator")
                return 2000
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible)
                check("the output follows the timer", texts.some(x => x.replacement === "57"), texts.map(x => x.replacement).join("|"))
                check("and the thumbnail", liveTexts(slideCell(kept.index))[0].replacement === "57")
                testInput.grabOutput("2-output-timer-57")
                goLive(kept.index)
                return 1400
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible)
                check("triggering the slide again starts the countdown again", texts.some(x => x.replacement === "59"), texts.map(x => x.replacement).join("|"))
                // Set by hand: what the slide set and what is changed now both go into the file
                Timers.stop(timer("Timer").id)
                Timers.configure(timer("Timer").id, { overrun: true })
                return 400
            },
            () => {
                const a = timer("Timer")
                check("a change made by hand keeps what the slide had set, and is written", a.duration === 60 && a.overrun
                      && testInput.fileHash(catalog.workspacePath + "/Configuration/Timers") !== kept.file, JSON.stringify(a))
                Timers.configure(a.id, { duration: 2 })
                Timers.start(a.id)
                return 4400
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible)
                check("past its end, an overrunning countdown shows a negative time on the output", texts.some(x => x.replacement === "-2"), texts.map(x => x.replacement).join("|"))
                testInput.grabOutput("3-output-timer-overrun")
                Timers.stop(timer("Timer").id)
                Timers.configure(timer("Timer").id, { duration: 180 })
                return 400
            },
            () => {
                // ---- the editor
                clearAll()
                startEditing(kept.entry, document.slides[kept.index].id)
                return 800
            },
            () => {
                check("the editor is up on that slide", editing && canvas().slide !== null && canvas().slide.id === document.slides[kept.index].id)
                canvas().pick(kept.element.id)
                inspector().tab = "text"
                return 400
            },
            () => {
                kept.elementId = canvas().selectedId
                check("the linked box is picked", sel() !== null && sel().linkKind === "timer")
                const shows = showsChoice()
                check("the inspector says which timer it shows", shows !== undefined && shows.model[shows.choice] === "the timer “Timer”", shows ? shows.model.join(" | ") + " -> " + shows.choice : "none")
                check("and lists the slide's other element, though it has no name", shows.model.includes("the text of an unnamed element") || canvas().elements.length !== 2, shows.model.join(" | "))
                check("and offers every timer of the workspace", shows.model.filter(m => m.startsWith("the timer")).length === 4)
                const parts = partChoices()
                check("and has a drop-down for each of hours, minutes, seconds and hundredths", parts.length === 4
                      && parts.every(c => c.model.join("|") === "Hidden|Two digits|One digit|Two digits, hidden at 0|One digit, hidden at 0"),
                      parts.length + ": " + (parts[0] ? parts[0].model.join("|") : ""))
                check("which say how this link is written: seconds alone, as one digit", parts.map(c => c.choice).join() === "0,0,2,0", parts.map(c => c.choice).join())
                check("the canvas shows the time too", liveTexts(canvas()).some(x => x.replacement === "180"), liveTexts(canvas()).map(x => x.replacement).join("|"))
                testInput.grab("4-editor-linked")
                canvas().editText(kept.elementId)
                return 300
            },
            () => {
                check("its text cannot be typed over, and the editor says why", canvas().editingId === "" && editScreen.notice.includes("shows the timer “Timer”"), editScreen.notice)
                // Another timer
                const shows = showsChoice()
                shows.activated(shows.model.indexOf("the timer “Service Countdown”"))
                return 400
            },
            () => {
                check("choosing another timer links to that one", sel().linkKind === "timer" && sel().linkTimerName === "Service Countdown" && sel().linkTimerId === timer("Service Countdown").id
                      && sel().linkTimerSeconds === 1 && sel().linkTimerMinutes === 0, JSON.stringify([sel().linkTimerName, sel().linkTimerSeconds]))
                const disk = onDisk()
                check("and that is in the file", disk.linkKind === "timer" && disk.linkTimerName === "Service Countdown" && disk.linkTimerId === timer("Service Countdown").id && disk.linkTimerPattern === "${timer}")
                // How it is written: the hundredths, as two digits
                partChoices()[3].activated(1)
                return 400
            },
            () => {
                check("choosing two digits for the hundredths sets that part's style and no other", sel().linkTimerHundredths === 2 && sel().linkTimerSeconds === 1
                      && sel().linkTimerMinutes === 0 && sel().linkTimerHours === 0 && onDisk().linkTimerHundredths === 2,
                      JSON.stringify([sel().linkTimerHours, sel().linkTimerMinutes, sel().linkTimerSeconds, sel().linkTimerHundredths]))
                const shown = liveTexts(canvas()).map(x => x.replacement)
                check("and the canvas shows hundredths", shown.length === 1 && /^-?\d+\.\d\d$/.test(shown[0]), shown.join("|"))
                // hours when there are any, minutes and seconds as two digits, no hundredths
                const parts = partChoices()
                parts[0].activated(4)
                return 300
            },
            () => {
                partChoices()[1].activated(1)
                return 300
            },
            () => {
                partChoices()[2].activated(1)
                return 300
            },
            () => {
                partChoices()[3].activated(0)
                return 300
            },
            () => {
                check("each drop-down sets its own part", sel().linkTimerHours === 3 && sel().linkTimerMinutes === 2 && sel().linkTimerSeconds === 2 && sel().linkTimerHundredths === 0,
                      JSON.stringify([sel().linkTimerHours, sel().linkTimerMinutes, sel().linkTimerSeconds, sel().linkTimerHundredths]))
                check("and shows what it is set to", partChoices().map(c => c.choice).join() === "4,1,1,0", partChoices().map(c => c.choice).join())
                const shown = liveTexts(canvas()).map(x => x.replacement)
                check("and the canvas shows the time that way", shown.length === 1 && /^-?(\d+:)?\d\d:\d\d$/.test(shown[0]), shown.join("|"))
                testInput.grab("5-editor-written")
                // Its own text
                showsChoice().activated(0)
                return 400
            },
            () => {
                check("choosing its own text takes the link away", sel().linkKind === "none" && onDisk().linkKind === "none" && liveTexts(canvas()).length === 0)
                check("with nothing left to say how a timer is written", partChoices().length === 0)
                editScreen.editor.undo()
                return 400
            },
            () => {
                check("undo brings the link back", sel() !== null && sel().linkKind === "timer" && sel().linkTimerName === "Service Countdown" && sel().linkTimerHours === 3,
                      sel() ? sel().linkKind : "nothing picked")
                // Linked to another element instead
                const shows = showsChoice()
                const other = shows.model.findIndex(m => m.startsWith("the text of"))
                kept.hasOther = other >= 0
                if (other >= 0)
                    shows.activated(other)
                return 400
            },
            () => {
                if (kept.hasOther)
                    check("linking to another element replaces the timer link", sel().linkKind === "element" && onDisk().linkKind === "element" && liveTexts(canvas()).length === 0)
                // A new text box, linked to a timer from nothing
                canvas().addText()
                return 500
            },
            () => {
                kept.elementId = canvas().selectedId
                check("a new text box is picked, and is being typed in", sel() !== null && sel().linkKind === "none" && canvas().editingId === kept.elementId)
                const shows = showsChoice()
                shows.activated(shows.model.indexOf("the timer “NLC Live”"))
                return 500
            },
            () => {
                check("linking it ends the typing", canvas().editingId === "")
                check("a new box linked to a timer is written the usual way: hours when there are any, minutes, seconds", sel().linkKind === "timer"
                      && sel().linkTimerName === "NLC Live" && sel().linkTimerId === timer("NLC Live").id
                      && sel().linkTimerHours === 3 && sel().linkTimerMinutes === 2 && sel().linkTimerSeconds === 2 && sel().linkTimerHundredths === 0,
                      JSON.stringify([sel().linkTimerHours, sel().linkTimerMinutes, sel().linkTimerSeconds, sel().linkTimerHundredths]))
                const shown = liveTexts(canvas()).map(x => x.replacement)
                check("and shows the timer's time, in its own style", shown.includes("05:00"), shown.join("|"))
                check("the file has the link", onDisk().linkKind === "timer" && onDisk().linkTimerPattern === "${timer}" && onDisk().hasText)
                testInput.grab("6-editor-new-box")
                kept.path = editScreen.editor.path
                kept.slideId = canvas().slide.id
                stopEditing()
                return 800
            },
            () => {
                check("the editor is down", !editing)
                const slide = document.slides.find(s => s.id === kept.slideId)
                const element = slide.elements.find(e => e.id === kept.elementId)
                check("the main window has the new link", element !== undefined && element.linkKind === "timer" && element.linkTimerName === "NLC Live")
                goLive(document.slides.indexOf(slide))
                Timers.start(timer("NLC Live").id)
                return 2400
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible).map(x => x.replacement)
                check("and on the output the new box counts up with its timer", texts.includes("05:02"), texts.join("|"))
                testInput.grabOutput("7-output-new-box")
                Timers.stop(timer("NLC Live").id)
                // The timer renamed: a link made here finds it still, by its id
                Timers.configure(timer("NLC Live").id, { name: "Live Stream" })
                return 500
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible).map(x => x.replacement)
                check("a renamed timer is still found by a link made here, by its id", timer("Live Stream") !== undefined && timer("NLC Live") === undefined && texts.includes("05:02"), texts.join("|"))
                // ---- hundredths, live: the box set to show them, in the editor, and then watched on the output
                Timers.configure(timer("Live Stream").id, { name: "NLC Live" })
                startEditing(kept.entry, kept.slideId)
                return 800
            },
            () => {
                editScreen.canvas.pick(kept.elementId)
                inspector().tab = "text"
                return 400
            },
            () => {
                partChoices()[3].activated(1)
                return 400
            },
            () => {
                check("the new box is set to show hundredths", sel().linkTimerHundredths === 2 && liveTexts(canvas()).some(x => /^05:0\d\.\d\d$/.test(x.replacement)),
                      liveTexts(canvas()).map(x => x.replacement).join("|"))
                stopEditing()
                return 600
            },
            () => {
                kept.liveIndex = document.slides.findIndex(x => x.id === kept.slideId)
                goLive(kept.liveIndex)
                return 1200
            },
            () => {
                // Stopped: nothing changes, and nothing is beating
                kept.beat = Timers.beat
                seenOnOutput = ({})
                seenInGrid = ({})
                watch.start()
                return 1000
            },
            () => {
                watch.stop()
                const onOutput = Object.keys(seenOnOutput).filter(x => x.includes("."))
                check("with the timer stopped, a box that shows hundredths stands still, and nothing beats", onOutput.length === 1 && Timers.beat === kept.beat, onOutput.join(" ") + " beat " + kept.beat + " -> " + Timers.beat)
                Timers.start(timer("NLC Live").id)
                return 300
            },
            () => {
                kept.beat = Timers.beat
                kept.slowBeat = Timers.slowBeat
                seenOnOutput = ({})
                seenInGrid = ({})
                watch.start()
                return 2000
            },
            () => {
                watch.stop()
                const onOutput = Object.keys(seenOnOutput).filter(x => /^05:\d\d\.\d\d$/.test(x))
                const inGrid = Object.keys(seenInGrid).filter(x => /^05:\d\d\.\d\d$/.test(x))
                check("running, the output shows its hundredths about thirty times a second", onOutput.length >= 45 && onOutput.length <= 72 && Timers.beat - kept.beat >= 50,
                      onOutput.length + " different times in 2 s, " + (Timers.beat - kept.beat) + " beats")
                check("and the thumbnail far less often: five times a second, and when any timer's second changes", inGrid.length >= 8 && inGrid.length <= 20 && Timers.slowBeat - kept.slowBeat <= 11,
                      inGrid.length + " different times in 2 s, " + (Timers.slowBeat - kept.slowBeat) + " slow beats")
                testInput.grabOutput("8-output-hundredths")
                Timers.stop(timer("NLC Live").id)
                return 900
            },
            () => {
                kept.beat = Timers.beat
                return 700
            },
            () => {
                check("stopped again, the beat stops", Timers.beat === kept.beat, kept.beat + " -> " + Timers.beat)
                Timers.configure(timer("NLC Live").id, { name: "Live Stream" })
                return 300
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible).map(x => x.replacement)
                check("(the timer renamed again for what follows)", timer("Live Stream") !== undefined, texts.join("|"))
                // A timer that is not in the workspace
                Timers.remove(timer("Live Stream").id)
                return 500
            },
            () => {
                const texts = liveTexts(output.contentItem).filter(x => x.visible).map(x => x.replacement)
                check("a box linked to a timer that is gone shows the time at nothing", texts.includes("00:00.00"), texts.join("|"))
                startEditing(kept.entry, kept.slideId)
                return 800
            },
            () => {
                editScreen.canvas.pick(kept.elementId)
                inspector().tab = "text"
                return 400
            },
            () => {
                const shows = showsChoice()
                check("and the editor says the timer is not here, keeping the link", shows !== undefined && shows.model[shows.choice] === "the timer “NLC Live” (not here)"
                      && sel().linkKind === "timer", shows ? shows.model.join(" | ") + " -> " + shows.choice : "none")
                testInput.grab("8-editor-timer-gone")
                stopEditing()
                return 500
            }
        ]
        next()
    }
}
