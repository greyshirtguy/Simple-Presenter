import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The Calendar (src/calendarfile.h, src/calendarevents.h, qml/CalendarWindow.qml), on the Act workspace, which has no
// calendar, and a playlist the test makes of two of its presentations. The toolbar's Calendar button opens the window
// of its own; an event added with +, named, given a time a few seconds on and a macro from the Action menu, the
// calendar enabled, and the macro run as the time comes; a second event given one presentation of the playlist from the
// menu, the playlist marked in the library, and the presentation live on its first slide at its time, on the output
// too; Test; the days of the week it repeats on, the list with each time and with each event once, and "ends on"; an
// event whose time passes while the calendar is off not run when it is switched on again; one deleted from its menu;
// the list's colours as drawn; the window closed by the button and by its own close, and opened again; and the app
// quitting with it open, which the runner sees (a run whose app does not quit fails). after_calendar in run.py reads
// the file.
QtObject {
    id: t

//COMMON
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

    // The Calendar's window, what it holds, and its menu
    function cal() {
        return calendarLoader.item
    }

    function inCal(name) {
        return Lib.find(cal().contentItem, item => item.objectName === name)
    }

    // A point of an item of the Calendar's window, in that window
    function calCentre(item) {
        return item.mapToItem(null, item.width / 2, item.height / 2)
    }

    function calClick(item, button) {
        const p = calCentre(item)
        testInput.mouseIn("calendarWindow", 0, p.x, p.y, 0, button ?? Qt.LeftButton)
        testInput.mouseIn("calendarWindow", 2, p.x, p.y, 0, button ?? Qt.LeftButton)
    }

    function calLabels() {
        return cal().popup.shown.map(i => i.header !== undefined ? "[" + i.header + "]" : i.divider === true ? "--"
                                     : (i.current ? "*" : "") + (i.label ?? "(note)") + (i.disabled ? "(off)" : "")).join(", ")
    }

    function calMenuRow(label) {
        return Lib.find(cal().popup.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === label)
    }

    function calHover(label) {
        const p = calCentre(calMenuRow(label))
        testInput.mouseIn("calendarWindow", 4, p.x, p.y)
    }

    function calPick(label) {
        calClick(calMenuRow(label))
    }

    function listRow(test) {
        return Lib.find(inCal("calendarList"), item => item.modelData !== undefined && item.modelData !== null && item.modelData.at !== undefined && test(item.modelData))
    }

    // A date and a time typed into the window's fields
    function typeIn(name, text) {
        const field = inCal(name)
        field.text = text
        field.editingFinished()
    }

    function startIn(seconds) {
        kept.due = Math.ceil(CalendarEvents.now()) + seconds
        typeIn("calendarStartDate", CalendarEvents.dateText(kept.due))
        typeIn("calendarStartTime", CalendarEvents.timeText(kept.due))
    }

    function run() {
        steps = [
            () => {
                check("the toolbar has Calendar, right of Bible", named("calendarButton") !== null
                      && named("calendarButton").mapToItem(null, 0, 0).x > named("bibleButton").mapToItem(null, 0, 0).x)
                check("(Act has no calendar)", !testInput.exists(catalog.workspacePath + "/Configuration/Calendar") && CalendarEvents.events.length === 0)
                // (A toolbar button is not clicked by the made-up mouse: the toolbar is what moves the window. Its signal is
                // what a click gives.)
                named("calendarButton").clicked()
                return 900
            },
            () => {
                check("which opens the Calendar in a window of its own, the button lit", cal() !== null && cal().visible && named("calendarButton").on
                      && cal().title === "Calendar")
                check("Calendar Enabled, not ticked in a workspace that has none, over an empty list", inCal("calendarEnabled") !== null
                      && !inCal("calendarEnabled").checked && inCal("calendarList").count === 0)
                // A playlist of two of the library's presentations, for an event to take live
                openLibrary(catalog.libraries[0].path)
                const two = documents.filter(d => openable(d) && catalog.open(d.file).slides.length > 1).slice(0, 2)
                const made = catalog.createPlaylist("Evening", "")
                kept.playlist = made.id
                for (const d of two)
                    catalog.addToPlaylist(made.id, d.file)
                kept.second = catalog.playlistItems(made.id)[1]
                check("(a playlist of two presentations)", made.error === "" && catalog.playlistItems(made.id).length === 2, made.error)
                calClick(inCal("calendarAdd"))
                return 600
            },
            () => {
                const e = cal().event
                check("+ adds an event, New Event, at the next whole hour, picked, its name being typed", cal().has && e.name === "New Event"
                      && e.start > CalendarEvents.now() && e.start <= CalendarEvents.now() + 3600 && e.start % 60 === 0 && inCal("calendarList").count === 1,
                      JSON.stringify(e))
                typeIn("calendarName", "Prelude")
                startIn(7)
                return 400
            },
            () => {
                const e = cal().event
                check("named, and given a time typed", e.name === "Prelude" && e.start === kept.due
                      && inCal("calendarStartTime").text === CalendarEvents.timeText(kept.due), e.start + " " + kept.due)
                calClick(inCal("calendarAction"))
                return 400
            },
            () => {
                check("Action has None, Playlists, Macros and Capture", calLabels() === "[Action], *None, Playlists, Macros, Capture(off)", calLabels())
                calHover("Macros")
                return 700
            },
            () => {
                check("Macros has the workspace's", calLabels().includes("Singing"), calLabels())
                calPick("Singing")
                return 500
            },
            () => {
                check("the macro is the event's action", cal().action.kind === "macro" && cal().action.macroName === "Singing"
                      && inCal("calendarActionWords").text === "Singing", JSON.stringify(cal().action))
                inCal("calendarEnabled").toggled(true)
                return 300
            },
            until(() => logged(/the calendar's event "Prelude", come due: the macro "Singing"/).length > 0, 12000, () => {
                const late = testInput.now() - kept.due * 1000
                check("enabled, the event comes due at its time and runs the macro", logged(/the calendar's event "Prelude", come due: the macro "Singing"/).length === 1
                      && late > -100 && late < 2500 && logged(/"Singing" run by hand/).length === 1, late + " ms")
                calClick(inCal("calendarAdd"))
                return 600
            }),
            // ---- A presentation of a playlist
            () => {
                typeIn("calendarName", "Evening Service")
                calClick(inCal("calendarAction"))
                return 400
            },
            () => {
                calHover("Playlists")
                return 700
            },
            () => {
                check("Playlists has the workspace's", calLabels().includes("Evening"), calLabels())
                calHover("Evening")
                return 700
            },
            () => {
                check("a playlist has the whole of it, and each presentation", calLabels() === "[Evening], The Whole Playlist, --, "
                      + catalog.playlistItems(kept.playlist).map(r => r.name).join(", "), calLabels())
                calPick(kept.second.name)
                return 500
            },
            () => {
                check("one presentation of it is the event's action", cal().action.kind === "playlist" && cal().action.playlistId === kept.playlist
                      && cal().action.itemId === kept.second.path && inCal("calendarActionWords").text === "Evening › " + kept.second.name,
                      inCal("calendarActionWords").text)
                const mark = Lib.find(named("playlistList"), i => i.objectName === "rowCalendar" && i.item !== null)
                check("and the playlist is marked with a calendar in the library", mark !== null)
                startIn(6)
                return 300
            },
            until(() => liveSlide !== null && liveDocument !== null && liveDocument.name === kept.second.name, 12000, () => {
                const late = testInput.now() - kept.due * 1000
                check("at its time, that presentation goes live on its first slide, the playlist opened at it", liveIndex === 0
                      && playlistId === kept.playlist && late > -100 && late < 2500, late + " ms")
                check("on the output too", output.contentItem !== null && testInput.lit(false, 0, 0, 1, 1) > 0.001)
                kept.lights = logged(/"Singing" run by hand/).length
                calClick(listRow(r => r.name === "Prelude" && r.section !== "upcoming"))
                return 400
            }),
            () => {
                check("(Prelude picked)", cal().event.name === "Prelude")
                calClick(inCal("calendarTest"))
                return 500
            },
            () => {
                check("Test runs its action now", logged(/the calendar's event "Prelude", tested: the macro "Singing"/).length === 1
                      && logged(/"Singing" run by hand/).length === kept.lights + 1)
                // (The day it starts on, ProPresenter's 1 to 7 from Sunday, and two days on: an event starting on a
                // day that is not one of its days has not come due on it)
                kept.days = [new Date(cal().event.start * 1000).getDay() + 1]
                kept.days.push((kept.days[0] + 1) % 7 + 1)
                kept.days.sort((a, b) => a - b)
                calClick(inCal("calendarDay:" + kept.days[0]))
                return 300
            },
            () => {
                calClick(inCal("calendarDay:" + kept.days[1]))
                return 400
            },
            () => {
                check("the days it repeats on, ProPresenter's Sun to Sat", JSON.stringify(cal().event.days) === JSON.stringify(kept.days) && cal().repeats,
                      JSON.stringify(cal().event.days))
                const rows = cal().rows
                check("the list has each time: the last it came due, under Current, and those to come, under Upcoming", cal().recurring
                      && rows.some(r => r.section === "current" && r.name === "Prelude") && rows.filter(r => r.section === "upcoming" && r.name === "Prelude").length > 50,
                      rows.length)
                calClick(inCal("calendarRecurring"))
                return 400
            },
            () => {
                check("and, with the arrows going round off, each event once", !cal().recurring && cal().rows.length === 2 && inCal("calendarList").count === 2,
                      cal().rows.length)
                calClick(inCal("calendarRecurring"))
                inCal("calendarEnds").toggled(true)
                return 400
            },
            () => {
                const start = new Date(cal().event.start * 1000)
                const end = new Date(cal().event.ends * 1000)
                check("ends on, ticked, is a year after it starts", end.getFullYear() === start.getFullYear() + 1 && end.getMonth() === start.getMonth()
                      && end.getDate() === start.getDate() && inCal("calendarEndDate").visible, cal().event.ends)
                inCal("calendarEnds").toggled(false)
                // An event due while the calendar is off
                calClick(inCal("calendarAdd"))
                return 400
            },
            () => {
                typeIn("calendarName", "Missed")
                CalendarEvents.change(cal().event.id, { action: { kind: "macro", macroId: Macros.find("", "Singing").id, macroName: "Singing" } })
                startIn(3)
                inCal("calendarEnabled").toggled(false)
                return 5000
            },
            () => {
                check("(the calendar off)", !CalendarEvents.enabled && cal().event.ends === 0 || cal().event.name === "Missed")
                inCal("calendarEnabled").toggled(true)
                return 2500
            },
            () => {
                check("one whose time passed while the calendar was off is not run when it is switched on again",
                      CalendarEvents.enabled && logged(/the calendar's event "Missed"/).length === 0, logged(/Missed/).join(" | "))
                calClick(listRow(r => r.name === "Missed"), Qt.RightButton)
                return 400
            },
            () => {
                check("an event's menu has Delete", calLabels() === "[Missed], Delete Event…", calLabels())
                calPick("Delete Event…")
                return 400
            },
            () => {
                calPick("Delete")
                return 500
            },
            () => {
                check("and deleted, it is gone", CalendarEvents.events.length === 2 && !CalendarEvents.events.some(e => e.name === "Missed") && !cal().has)
                calClick(listRow(r => r.name === "Evening Service"))
                return 500
            },
            () => {
                // The list as drawn: a row picked blue, one come due red
                const picked = listRow(r => r.name === "Evening Service")
                const past = listRow(r => r.past && r.name !== "Evening Service")
                const at = (row) => row.mapToItem(null, row.width - 8, 6)
                kept.blue = testInput.pixelIn("calendarWindow", at(picked).x, at(picked).y)
                kept.red = past ? testInput.pixelIn("calendarWindow", at(past).x, at(past).y) : ""
                check("the list as drawn: the event picked in blue, one that has come due in red", kept.blue === "#1f5fa8" && kept.red === "#6b2a2f",
                      kept.blue + " " + kept.red)
                testInput.grabIn("calendarWindow", "1-calendar")
                named("calendarButton").clicked()
                return 600
            },
            () => {
                check("the button again puts the window away", !calendarOpen && !cal().visible && !named("calendarButton").on)
                named("calendarButton").clicked()
                return 600
            },
            () => {
                check("(and opens it again)", calendarOpen && cal().visible)
                cal().close()
                return 600
            },
            () => {
                check("closed by its own close, it is put away, and the button says so", !calendarOpen && !cal().visible && !named("calendarButton").on)
                named("calendarButton").clicked()
                return 600
            },
            () => {
                check("and the button opens it again", calendarOpen && cal().visible)
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                // (Left open: the app is to quit with it so, which the runner sees)
                return 100
            },
        ]
        next()
    }
}
