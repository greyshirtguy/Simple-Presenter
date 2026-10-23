import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Disable dates (src/disabledatefile.h, src/disabledates.h, qml/DisableDatePanel.qml), this app's own, on the Demo
// workspace's "Holy Holy Holy". The runner leaves a file of dates in the workspace before the app starts, with one long
// past: at open that slide is disabled and the date is gone from the file. Then from a slide's menu, Disable On…: the
// panel with the start of tomorrow to begin from; a time a few seconds on typed, said in words; saved, the slide marked
// in the grid, the menu's row saying the date, and the file having it; and at that time the slide disabled, its mark
// gone and the file without it. A date set on another and taken off again with Remove Date; and a time that has passed,
// which disables a third at once. after_disabledates in run.py reads the presentation and the file.
QtObject {
    id: t

//COMMON
    readonly property string datesFile: "@WORKSPACES@/Demo/SimplePresenter/Disable Dates.json"

    function frame(index) {
        slideCell(index)
        return named("slideFrame:" + index)
    }

    function badge(index) {
        const cell = slideCell(index)
        return cell ? Lib.find(cell, i => i.objectName === "disableDateBadge" && i.visible) : null
    }

    function panel() {
        return named("disableDatePanel")
    }

    function dates() {
        return JSON.parse(testInput.readText(datesFile) || "{}").dates ?? []
    }

    // Hours, minutes and seconds on the clock, as the panel reads a time
    function clockOf(at) {
        const d = new Date(at * 1000)
        return [d.getHours(), d.getMinutes(), d.getSeconds()].map(n => String(n).padStart(2, "0")).join(":")
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "Demo").path)
                openEntry(entry("Holy Holy Holy"))
                return 800
            },
            () => {
                const preset = testInput.readText("@WORKSPACES@/../disabledates-preset.txt").trim()
                kept.preset = document.slides.findIndex(s => s.id === preset)
                kept.count = document.slides.length
                check("a date that came while the app was not open: the slide disabled at open", kept.preset >= 0 && document.slides[kept.preset].disabled === true
                      && document.slides.filter(s => s.disabled === true).length === 1, kept.preset)
                check("and the date gone from the file, the rest of it kept", dates().length === 0 && JSON.parse(testInput.readText(datesFile)).kept === "as it is")
                check("the log says so", testInput.readText(Log.path).includes("disabled, its date"))
                kept.a = 4
                click(centre(frame(kept.a)), Qt.RightButton)
                return 400
            },
            () => {
                check("a slide's menu has Disable On…", labels().includes("Disable Slide, Disable On…"), labels())
                click(menuRow("Disable On…"))
                return 500
            },
            () => {
                const p = panel()
                const tomorrow = CalendarEvents.withTime(CalendarEvents.now() + 86400, "12:00 am")
                check("the panel, with the start of tomorrow to begin from", p !== null && p.visible && named("disableDateDay").text === CalendarEvents.dateText(tomorrow)
                      && named("disableDateTime").text === CalendarEvents.timeText(tomorrow) && !named("disableDateRemove").visible,
                      named("disableDateDay").text + " " + named("disableDateTime").text)
                kept.when = Math.floor(CalendarEvents.now()) + 8
                named("disableDateDay").text = CalendarEvents.dateText(kept.when)
                named("disableDateTime").text = clockOf(kept.when)
                return 300
            },
            () => {
                check("a time a few seconds on, said in words", named("disableDateSaid").text === "The slide is disabled on " + CalendarEvents.whenText(kept.when) + ".",
                      named("disableDateSaid").text)
                named("disableDateDay").text = "not a date"
                return 200
            },
            () => {
                check("what cannot be read is said to be", named("disableDateSaid").text === "That is no date and time that can be read." && named("disableDateSave").opacity < 1)
                named("disableDateDay").text = CalendarEvents.dateText(kept.when)
                return 200
            },
            () => {
                click(centre(named("disableDateSave")))
                return 600
            },
            () => {
                const id = document.slides[kept.a].id
                check("saved: the panel gone, the slide marked in the grid", !panel().visible && badge(kept.a) !== null && document.slides[kept.a].disabled !== true)
                check("and the file has it, by the presentation's uuid and the slide's", dates().length === 1 && dates()[0].slide === id && dates()[0].presentation === document.uuid
                      && dates()[0].path === "Libraries/Demo/Holy Holy Holy.pro" && Math.abs(new Date(dates()[0].at).getTime() / 1000 - kept.when) < 1, JSON.stringify(dates()))
                testInput.grab("1-marked")
                click(centre(frame(kept.a)), Qt.RightButton)
                return 400
            },
            () => {
                check("its menu says the date", labels().includes("Disable On " + CalendarEvents.whenText(kept.when) + "…"), labels())
                menu.close()
                return Math.max(200, (kept.when - CalendarEvents.now()) * 1000 + 1500)
            },
            () => {
                check("when it comes, the slide is disabled, and its mark gone", document.slides[kept.a].disabled === true && badge(kept.a) === null && frame(kept.a).opacity < 0.5)
                check("and the date gone from the file", dates().length === 0)
                kept.b = 6
                editDisableDate(kept.b)
                return 400
            },
            () => {
                named("disableDateTime").text = "9:00 am"
                click(centre(named("disableDateSave")))
                return 500
            },
            () => {
                check("another given a date tomorrow is marked, with the day", badge(kept.b) !== null && Lib.find(badge(kept.b), i => i.text === CalendarEvents.dateText(dates()[0]?.at ? new Date(dates()[0].at).getTime() / 1000 : 0)) !== null
                      && dates().length === 1)
                editDisableDate(kept.b)
                return 400
            },
            () => {
                check("its panel has Remove Date", named("disableDateRemove").visible)
                click(centre(named("disableDateRemove")))
                return 500
            },
            () => {
                check("taken off: no mark, nothing in the file, and the slide as it was", badge(kept.b) === null && dates().length === 0 && document.slides[kept.b].disabled !== true)
                kept.c = 7
                editDisableDate(kept.c)
                return 400
            },
            () => {
                named("disableDateDay").text = CalendarEvents.dateText(CalendarEvents.now() - 86400)
                return 200
            },
            () => {
                check("a time that has passed is said to have", named("disableDateSaid").text === "That has passed: saved, the slide is disabled now.", named("disableDateSaid").text)
                click(centre(named("disableDateSave")))
                return 800
            },
            () => {
                check("and saved, disables the slide at once", document.slides[kept.c].disabled === true && dates().length === 0 && document.slides.length === kept.count)
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 100
            },
        ]
        next()
    }
}
