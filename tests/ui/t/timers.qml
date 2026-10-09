import QtQuick
import QtMultimedia
import SimplePresenterApp
import "lib.js" as Lib

// The timers: the default one, adding, naming, the three kinds, running them, overrun,
// and what is written to the workspace's Configuration/Timers.
QtObject {
    id: t

//COMMON
    property int ticks: 0
    property Connections tickWatch: Connections {
        target: Timers
        function onTicked() { ++t.ticks }
    }

    function panel() {
        return Lib.find(win.contentItem, item => item.kinds !== undefined && item.kindNames !== undefined && item.settle !== undefined)
    }

    function showControl() {
        return Lib.find(win.contentItem, item => item.tabs !== undefined && item.tab !== undefined)
    }

    function rows() {
        return Lib.findAll(panel(), item => item.timer !== undefined && item.state !== undefined && item.open !== undefined)
    }

    function row(name) {
        const list = Lib.find(panel(), item => item.positionViewAtIndex !== undefined && item.count !== undefined)
        const index = Timers.timers.findIndex(x => x.name === name)
        if (index >= 0)
            list.positionViewAtIndex(index, ListView.Contain)
        return rows().find(r => r.timer.name === name)
    }

    // Whether an item's middle is inside the list, where a click can reach it
    function reachable(item) {
        const list = Lib.find(panel(), i => i.positionViewAtIndex !== undefined && i.count !== undefined)
        const p = item.mapToItem(list, item.width / 2, item.height / 2)
        return p.y >= 0 && p.y <= list.height
    }

    function timer(name) {
        return Timers.timers.find(x => x.name === name)
    }

    function state(name) {
        return Timers.state(timer(name).id)
    }

    function button(rowItem, kind) {
        return Lib.find(rowItem, item => item.kind === kind && item.available !== undefined)
    }

    // The boxes of a row for typing in, in the order they are laid out
    function fields(rowItem) {
        return Lib.findAll(rowItem, item => item.selectByMouse !== undefined && item.cursorPosition !== undefined && item.visible)
    }

    function tick(rowItem, text) {
        return Lib.find(rowItem, item => item.text === text && item.checked !== undefined && item.toggled !== undefined)
    }

    function typeInto(field, text) {
        click(centre(field))
        field.selectAll()
        testInput.type(text + "\n")
    }

    function addButton() {
        return Lib.find(showControl(), item => item.objectName === "showControlAdd")
    }

    function run() {
        steps = [
            () => {
                check("a workspace with no timers file has one countdown", Timers.timers.length === 1 && timer("Countdown") !== undefined
                      && timer("Countdown").kind === "countdown" && timer("Countdown").duration === 300 && !timer("Countdown").overrun, JSON.stringify(Timers.timers))
                check("which stands at its full length", state("Countdown").text === "0:05:00" && !state("Countdown").running, JSON.stringify(state("Countdown")))
                check("the show controls open on the timers", sidePanel.showControlTab === "timers" && panel() !== null && panel().visible && rows().length === 1)
                check("no file has been written for it", !testInput.exists(catalog.workspacePath + "/Configuration/Timers"))
                testInput.grab("1-default")
                // ---- the tabs
                const tabs = Lib.findAll(win.contentItem, item => item.chosen !== undefined && item.modelData !== undefined && item.modelData.id !== undefined)
                check("there are four tabs", tabs.length === 4 && tabs.map(x => x.modelData.id).join() === "timers,props,stage,macros" && tabs[0].chosen, tabs.map(x => x.modelData.id).join())
                check("the one showing is blue", Qt.colorEqual(tabs[0].color, "#1e88e5") && !Qt.colorEqual(tabs[1].color, "#1e88e5"))
                kept.tabs = tabs
                click(centre(tabs[1]))
                return 300
            },
            () => {
                check("Props is a tab, with its own + and no timers in it", sidePanel.showControlTab === "props" && kept.tabs[1].chosen && !kept.tabs[0].chosen && !panel().visible
                      && Qt.colorEqual(kept.tabs[1].color, "#1e88e5") && addButton().visible, String(kept.tabs[1].color))
                testInput.grab("2-props")
                click(centre(kept.tabs[2]))
                return 300
            },
            () => {
                check("and so is Stage", sidePanel.showControlTab === "stage" && kept.tabs[2].chosen)
                click(centre(kept.tabs[0]))
                return 300
            },
            () => {
                check("back to the timers", sidePanel.showControlTab === "timers" && panel().visible && addButton().visible)
                // ---- running the default countdown
                kept.ticks = ticks
                click(centre(button(row("Countdown"), "play")))
                return 2300
            },
            () => {
                const s = state("Countdown")
                check("start starts it", s.running && s.seconds < 298.2 && s.seconds > 297 && s.text === "0:04:58", JSON.stringify(s))
                check("it says so once a second, no more", ticks - kept.ticks >= 3 && ticks - kept.ticks <= 4, (ticks - kept.ticks) + " ticks in 2.3 s")
                check("the row shows it running", row("Countdown").state.running && button(row("Countdown"), "stop") !== null)
                testInput.grab("3-running")
                click(centre(button(row("Countdown"), "stop")))
                return 300
            },
            () => {
                kept.seconds = state("Countdown").seconds
                check("stop stops it", !state("Countdown").running && kept.seconds < 298.2)
                kept.ticks = ticks
                return 1500
            },
            () => {
                check("and it stays there, saying nothing", state("Countdown").seconds === kept.seconds && ticks === kept.ticks)
                click(centre(button(row("Countdown"), "play")))
                return 1200
            },
            () => {
                check("start carries on from where it stopped", state("Countdown").running && state("Countdown").seconds < kept.seconds - 0.9 && state("Countdown").seconds > kept.seconds - 1.6,
                      kept.seconds + " -> " + state("Countdown").seconds)
                click(centre(button(row("Countdown"), "restart")))
                return 300
            },
            () => {
                check("reset puts it back at its start, stopped", !state("Countdown").running && state("Countdown").seconds === 300 && state("Countdown").text === "0:05:00", JSON.stringify(state("Countdown")))
                // ---- setting it up: the row opens
                check("a row starts shut", !row("Countdown").open && fields(row("Countdown")).length === 0)
                kept.shut = row("Countdown").height
                click(row("Countdown").mapToItem(null, 30, 14))
                return 300
            },
            () => {
                const r = row("Countdown")
                check("a click on the row opens it", r.open && r.height > kept.shut + 60 && fields(r).length === 2, kept.shut + " -> " + r.height)
                kept.row = r
                testInput.grab("4-open")
                typeInto(fields(r)[1], "2")
                return 400
            },
            () => {
                check("typing a length sets it", timer("Countdown").duration === 2 && state("Countdown").text === "0:00:02", JSON.stringify(timer("Countdown")))
                check("and the file is written, with the default timer's id", testInput.exists(catalog.workspacePath + "/Configuration/Timers")
                      && timer("Countdown").id === "8E5C1B4A-6D2F-4B0E-9A37-1C5F0D2E7A64")
                check("the row was not made again for it", row("Countdown") === kept.row && row("Countdown").open && fields(row("Countdown"))[1].text === "0:00:02")
                check("the keyboard is back with the slides", !fields(row("Countdown"))[1].activeFocus)
                click(centre(button(row("Countdown"), "play")))
                return 3200
            },
            () => {
                const s = state("Countdown")
                check("a countdown stops at nothing", !s.running && s.seconds === 0 && s.text === "0:00:00", JSON.stringify(s))
                kept.ticks = ticks
                return 1200
            },
            () => {
                check("and then nothing runs", ticks === kept.ticks)
                click(centre(button(row("Countdown"), "play")))
                return 1300
            },
            () => {
                const s = state("Countdown")
                check("start on a finished countdown starts it again from its length", s.running && s.seconds < 1 && s.seconds > 0.4, JSON.stringify(s))
                click(centre(button(row("Countdown"), "restart")))
                // ---- a length typed, and Start pressed without Enter
                const field = fields(row("Countdown"))[1]
                click(centre(field))
                field.selectAll()
                testInput.type("0:07")
                check("the box has the keyboard while it is typed in", field.activeFocus && field.text === "0:07" && timer("Countdown").duration === 2)
                click(centre(button(row("Countdown"), "play")))
                return 1300
            },
            () => {
                const s = state("Countdown")
                check("Start takes what was typed, and starts that", timer("Countdown").duration === 7 && s.running && s.seconds < 6.2 && s.seconds > 5.2, JSON.stringify(s))
                check("and the keyboard is back with the slides", !fields(row("Countdown"))[1].activeFocus)
                click(centre(button(row("Countdown"), "restart")))
                typeInto(fields(row("Countdown"))[1], "2")
                return 400
            },
            () => {
                // ---- overrun
                click(centre(tick(row("Countdown"), "Overrun")))
                return 400
            },
            () => {
                check("overrun is set", timer("Countdown").overrun === true && !state("Countdown").running && state("Countdown").seconds === 2)
                click(centre(button(row("Countdown"), "play")))
                return 4300
            },
            () => {
                const s = state("Countdown")
                check("a countdown that may overrun runs on below nothing", s.running && s.seconds < -2 && s.seconds > -2.7 && s.text === "-0:00:02", JSON.stringify(s))
                testInput.grab("5-overrun")
                click(centre(button(row("Countdown"), "stop")))
                // ---- a name
                typeInto(fields(row("Countdown"))[0], "Service Start")
                return 400
            },
            () => {
                check("it can be renamed", timer("Service Start") !== undefined && timer("Countdown") === undefined && row("Service Start") === kept.row)
                check("renaming does not put it back at its start", state("Service Start").seconds < -2, JSON.stringify(state("Service Start")))
                typeInto(fields(row("Service Start"))[0], "   ")
                return 300
            },
            () => {
                check("an empty name is not taken", timer("Service Start") !== undefined && fields(row("Service Start"))[0].text === "Service Start", fields(row("Service Start"))[0].text)
                // ---- adding
                click(centre(addButton()))
                return 400
            },
            () => {
                check("+ adds a five-minute countdown", Timers.timers.length === 2 && timer("Timer") !== undefined && timer("Timer").kind === "countdown"
                      && timer("Timer").duration === 300 && timer("Timer").id.length === 36 && timer("Timer").id !== timer("Service Start").id, JSON.stringify(timer("Timer")))
                check("and opens its row, shutting the other", row("Timer") !== undefined && row("Timer").open && !row("Service Start").open)
                check("with the new row brought into view", reachable(fields(row("Timer"))[0]) && reachable(button(row("Timer"), "play")))
                click(centre(addButton()))
                return 400
            },
            () => {
                check("another gets another name", Timers.timers.length === 3 && timer("Timer 2") !== undefined, Timers.timers.map(x => x.name).join(", "))
                testInput.grab("6-three")
                // ---- the kinds: elapsed, chosen from the drop-down
                panel().opened = timer("Timer").id
                return 300
            },
            () => {
                const combo = Lib.find(row("Timer"), item => item.currentIndex !== undefined && item.popup !== undefined)
                check("the open row's drop-down shows its kind", combo !== null && combo.currentIndex === 0 && combo.model.length === 3)
                kept.height = row("Timer").height
                combo.activated(2)
                return 300
            },
            () => {
                check("choosing a kind there changes it", timer("Timer").kind === "elapsed" && state("Timer").text === "0:00:00")
                const r = row("Timer")
                check("an elapsed time's row offers an end", tick(r, "Ends at") !== null && tick(r, "Ends at").visible && r.height > kept.height + 20, kept.height + " -> " + r.height)
                click(centre(button(r, "play")))
                return 2400
            },
            () => {
                const s = state("Timer")
                check("it counts up", s.running && s.seconds > 2 && s.seconds < 2.9 && s.text === "0:00:02", JSON.stringify(s))
                // an end, at three seconds
                click(centre(tick(row("Timer"), "Ends at")))
                return 300
            },
            () => {
                check("setting it up differently puts it back at its start", !state("Timer").running && state("Timer").seconds === 0 && timer("Timer").hasEndTime)
                typeInto(fields(row("Timer"))[2], "3")
                return 300
            },
            () => {
                check("the end can be typed", timer("Timer").endTime === 3)
                click(centre(button(row("Timer"), "play")))
                return 4000
            },
            () => {
                const s = state("Timer")
                check("with an end, it stops there", !s.running && s.seconds === 3 && s.text === "0:00:03", JSON.stringify(s))
                Timers.configure(timer("Timer").id, { overrun: true })
                click(centre(button(row("Timer"), "play")))
                return 4300
            },
            () => {
                const s = state("Timer")
                check("unless it may overrun", s.running && s.seconds > 4 && s.text === "0:00:04", JSON.stringify(s))
                Timers.stop(timer("Timer").id)
                Timers.configure(timer("Timer").id, { startTime: 90, hasEndTime: false })
                return 300
            },
            () => {
                check("an elapsed time can start from a time", state("Timer").text === "0:01:30" && timer("Timer").startTime === 90)
                // ---- countdown to a time of day
                const now = new Date()
                kept.target = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds() + 125
                Timers.configure(timer("Timer 2").id, { kind: "countdownTo" })
                return 300
            },
            () => {
                check("a countdown to a time starts as ten in the morning", timer("Timer 2").kind === "countdownTo" && timer("Timer 2").timeOfDay === 36000)
                Timers.configure(timer("Timer 2").id, { timeOfDay: kept.target % 86400 })
                return 300
            },
            () => {
                const s = state("Timer 2")
                check("a countdown to a time runs of itself: it shows how long it is until then", s.running && s.seconds > 121 && s.seconds < 125 && s.text.startsWith("0:02:0"), JSON.stringify(s))
                check("its row shows it running", button(row("Timer 2"), "stop") !== null)
                return 2200
            },
            () => {
                const s = state("Timer 2")
                check("and runs down", s.running && s.seconds > 118.5 && s.seconds < 123, JSON.stringify(s))
                testInput.grab("7-three-kinds")
                click(centre(button(row("Timer 2"), "stop")))
                return 300
            },
            () => {
                kept.seconds = state("Timer 2").seconds
                check("it can be stopped", !state("Timer 2").running)
                return 1500
            },
            () => {
                check("stopped, it holds what it showed", !state("Timer 2").running && state("Timer 2").seconds === kept.seconds)
                click(centre(button(row("Timer 2"), "restart")))
                return 300
            },
            () => {
                check("and reset has it follow the clock again", state("Timer 2").running && state("Timer 2").seconds < kept.seconds - 1, JSON.stringify(state("Timer 2")))
                // A time that has just gone by, and one that went by long enough ago to be tomorrow's
                const now = new Date()
                kept.now = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds()
                Timers.configure(timer("Timer 2").id, { timeOfDay: (kept.now - 3600 + 86400) % 86400 })
                return 500
            },
            () => {
                const s = state("Timer 2")
                check("a time of day that went by an hour ago leaves it at nothing, stopped", !s.running && s.seconds === 0 && s.text === "0:00:00", JSON.stringify(s))
                Timers.configure(timer("Timer 2").id, { overrun: true })
                return 500
            },
            () => {
                const s = state("Timer 2")
                check("or, if it may overrun, an hour past", s.running && s.seconds < -3598 && s.seconds > -3606 && s.text.startsWith("-1:00:0"), JSON.stringify(s))
                Timers.configure(timer("Timer 2").id, { overrun: false, timeOfDay: (kept.now - 7 * 3600 + 2 * 86400) % 86400 })
                return 500
            },
            () => {
                const s = state("Timer 2")
                check("a time of day that went by seven hours ago is the next one, seventeen hours off", s.running && s.seconds > 17 * 3600 - 8 && s.seconds < 17 * 3600 + 2, JSON.stringify(s))
                Timers.configure(timer("Timer 2").id, { timeOfDay: (kept.now + 15 * 3600) % 86400 })
                return 500
            },
            () => {
                const s = state("Timer 2")
                check("and one fifteen hours ahead is fifteen hours off, whichever side of midnight", s.running && s.seconds > 15 * 3600 - 8 && s.seconds < 15 * 3600 + 2, JSON.stringify(s))
                panel().opened = timer("Timer 2").id
                return 300
            },
            () => {
                // the time of day typed in its box
                check("its box shows the time of day", /^\d\d:\d\d(:\d\d)?$/.test(fields(row("Timer 2"))[1].text), fields(row("Timer 2"))[1].text)
                typeInto(fields(row("Timer 2"))[1], "23:59")
                return 400
            },
            () => {
                check("a time of day can be typed", timer("Timer 2").timeOfDay === 23 * 3600 + 59 * 60, timer("Timer 2").timeOfDay)
                typeInto(fields(row("Timer 2"))[1], "nonsense")
                return 300
            },
            () => {
                check("nonsense is not taken", timer("Timer 2").timeOfDay === 23 * 3600 + 59 * 60 && fields(row("Timer 2"))[1].text === "23:59", fields(row("Timer 2"))[1].text)
                panel().opened = timer("Service Start").id
                return 300
            },
            () => {
                typeInto(fields(row("Service Start"))[1], "1:30")
                return 300
            },
            () => {
                check("a length can be typed as minutes and seconds", timer("Service Start").duration === 90 && fields(row("Service Start"))[1].text === "0:01:30")
                typeInto(fields(row("Service Start"))[1], "1:00:00")
                return 300
            },
            () => {
                check("or with hours", timer("Service Start").duration === 3600 && state("Service Start").text === "1:00:00")
                // ---- a name typed, and a slide clicked without Enter
                const field = fields(row("Service Start"))[0]
                click(centre(field))
                field.selectAll()
                testInput.type("Welcome")
                check("a name is being typed", field.activeFocus && field.text === "Welcome" && timer("Welcome") === undefined)
                click(centre(slideCell(1)))
                return 500
            },
            () => {
                check("a click on a slide takes what was typed, and shows the slide", timer("Welcome") !== undefined && liveIndex === 1 && !cleared,
                      Timers.timers.map(x => x.name).join(", ") + " / live " + liveIndex)
                check("and the keyboard is back with the show", !fields(row("Welcome"))[0].activeFocus)
                testInput.key(Qt.Key_Right)
                return 400
            },
            () => {
                check("so the arrow keys drive the slides", liveIndex === 2)
                clearAll()
                Timers.configure(timer("Welcome").id, { name: "Service Start" })
                return 300
            },
            () => {
                // ---- a click on the row shuts it
                const r = row("Service Start")
                click(r.mapToItem(null, 30, 14))
                return 300
            },
            () => {
                check("a click on an open row shuts it", !row("Service Start").open && panel().opened === "")
                // ---- removing
                click(row("Timer 2").mapToItem(null, 30, 14), Qt.RightButton)
                return 300
            },
            () => {
                check("a timer's menu", menu.opened && labels() === "Remove Timer", labels())
                click(menuRow("Remove Timer"))
                return 400
            },
            () => {
                check("removes it", Timers.timers.length === 2 && timer("Timer 2") === undefined && rows().length === 2)
                panel().opened = timer("Timer").id
                return 300
            },
            () => {
                const remove = Lib.find(row("Timer"), item => item.text === "Remove" && item.down !== undefined)
                check("an open row has a Remove button, in view", remove !== null && reachable(remove))
                click(centre(remove))
                return 400
            },
            () => {
                check("which removes that one", Timers.timers.length === 1 && timer("Timer") === undefined && timer("Service Start") !== undefined && rows().length === 1)
                testInput.grab("8-end")
                kept.ticks = ticks
                return 1500
            },
            () => {
                check("with nothing running, nothing ticks", ticks === kept.ticks && !Timers.timers.some(x => Timers.state(x.id).running))
                return 100
            }
        ]
        next()
    }
}
