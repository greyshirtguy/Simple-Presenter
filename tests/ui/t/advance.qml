import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A show that goes on by itself: a slide's Go To Next Timer, made in the panel its
// menu opens, with and without Loop to Beginning, and taken off again; a presentation's
// Slide Show, from the button over the slides; a slide's own timer winning over it;
// the show and the announcements each going on by their own; and what stops it. It
// looks at which slide is live, and at what is drawn on the output, as time passes.
//
// The presentations are the test's own (prepare_advance in run.py): three slides that
// are each a block of colour, in "Plain" at the bottom left (yellow, cyan, white) and
// in "Notices" at the top right (red, green, blue); and "Timed", which comes with a
// timer on its first slide.
QtObject {
    id: t

//COMMON
    readonly property point atNotice: Qt.point(0.78, 0.25)
    readonly property point atPlain: Qt.point(0.22, 0.75)

    function out(p) {
        return String(testInput.pixel(false, p.x, p.y))
    }

    function is(colour, wanted) {
        const part = (c, i) => parseInt(String(c).slice(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(colour, i) - part(wanted, i)) <= 40)
    }

    function open(name) {
        openEntry(entry(name))
    }

    function panel() {
        return named("goToNextPanel")
    }

    function panelOpen() {
        const item = panel()
        return item !== null && item.parent.open === true
    }

    // The badge a slide's cell has for its timer, and the words in it
    function badge(index) {
        return Lib.find(slideCell(index), i => i.objectName === "goToNextBadge")
    }

    function badgeWords(index) {
        const found = badge(index)
        const words = found ? Lib.find(found, i => i.text !== undefined && i.font !== undefined) : null
        return words ? words.text : ""
    }

    function badgeKind(index) {
        const found = badge(index)
        const glyph = found ? Lib.find(found, i => i.kind !== undefined && i.ink !== undefined) : null
        return glyph ? glyph.kind : ""
    }

    function timerOf(index) {
        const timer = document.slides[index].goToNext
        return timer === undefined ? "none" : timer.seconds + (timer.loop ? " loop" : "")
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                open("Plain")
                return 500
            },
            () => {
                kept.plain = document.path
                kept.before = testInput.fileHash(kept.plain)
                check("a slide has no timer until it is given one, and its presentation no Slide Show", timerOf(0) === "none" && document.slideShow === 0
                      && badge(0) === null)
                showSlideMenu(0, slideCell(0), 20, 20)
                return 400
            },
            () => {
                check("a slide's menu has Go To Next Timer…, not ticked", labels().indexOf(", Go To Next Timer…,") >= 0, labels())
                click(menuRow("Go To Next Timer…"))
                return 500
            },
            () => {
                const seconds = named("goToNextSeconds")
                check("it puts up a panel with the seconds, which start at five, a tick box for Loop to Beginning, and nothing to take off",
                      panelOpen() && seconds.text === "5" && seconds.activeFocus && named("goToNextLoop").visible && !named("goToNextLoop").checked
                      && !named("goToNextRemove").visible, seconds.text)
                testInput.grab("1-go-to-next-panel")
                testInput.type("x")
                check("what is not a number cannot be saved", panel().parent.seconds < 0, seconds.text)
                testInput.key(Qt.Key_Return)
                return 300
            },
            () => {
                check("and Enter leaves the panel up", panelOpen() && timerOf(0) === "none")
                const seconds = named("goToNextSeconds")
                seconds.selectAll()
                testInput.type("1")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                check("a second, saved with Enter: the slide has the timer, which goes to the next", !panelOpen() && timerOf(0) === "1" && timerOf(1) === "none")
                check("its cell says so: an arrowhead against a bar, and the seconds", badge(0) !== null && badge(0).visible && badgeWords(0) === "1s"
                      && badgeKind(0) === "next" && badge(1) === null, badgeWords(0) + " " + badgeKind(0))
                testInput.snapshot(kept.plain, "advance-timer")
                check("nothing goes on until a slide is triggered", !Show.advanceWaiting)
                kept.at = testInput.now()
                click(centre(slideCell(0)))
                return 400
            },
            () => {
                check("triggered, the slide is live and its time is counting", Show.liveIndex === 0 && Show.advanceWaiting && Show.advanceSeconds === 1
                      && is(out(atPlain), "#ffff00"), out(atPlain))
                return 1100
            },
            () => {
                check("when the second is up the next slide is live, by itself, and on the output", Show.liveIndex === 1 && is(out(atPlain), "#00ffff")
                      && slideCell(1).live && !slideCell(0).live, Show.liveIndex + " " + out(atPlain))
                check("that slide has no timer, so the show stays there", !Show.advanceWaiting)
                // The last slide: half a second, and back to the first
                editGoToNext(2)
                return 400
            },
            () => {
                named("goToNextSeconds").selectAll()
                testInput.type("0.5")
                click(centre(named("goToNextLoop")))
                check("the tick box is ticked by a click", named("goToNextLoop").checked && panel().parent.seconds === 0.5)
                click(centre(named("goToNextSave")))
                return 600
            },
            () => {
                check("saved with the button: the last slide goes to the first after half a second", !panelOpen() && timerOf(2) === "0.5 loop" && timerOf(0) === "1")
                check("its cell has the arrow going round", badgeWords(2) === "0.5s" && badgeKind(2) === "loop", badgeWords(2) + " " + badgeKind(2))
                testInput.snapshot(kept.plain, "advance-loop")
                testInput.grab("2-timers-on-slides")
                check("the slide that is live is live still, and nothing was started by the edit", Show.liveIndex === 1 && !Show.advanceWaiting)
                click(centre(slideCell(2)))
                return 900
            },
            () => {
                check("from the last slide the show goes to the first", Show.liveIndex === 0 && is(out(atPlain), "#ffff00") && Show.advanceWaiting
                      && Show.advanceSeconds === 1, Show.liveIndex + " " + out(atPlain))
                return 1200
            },
            () => {
                check("and from there, by the first slide's timer, to the second, where it stays", Show.liveIndex === 1 && !Show.advanceWaiting, Show.liveIndex)
                // Cleared while it is counting
                click(centre(slideCell(0)))
                return 300
            },
            () => {
                check("counting again", Show.liveIndex === 0 && Show.advanceWaiting)
                testInput.key(Qt.Key_F2)
                return 1300
            },
            () => {
                check("clearing the slide stops it: the show is where it was a second and more later", cleared && Show.liveIndex === 0 && !Show.advanceWaiting,
                      Show.liveIndex)
                // The timers taken off again
                showSlideMenu(0, slideCell(0), 20, 20)
                return 400
            },
            () => {
                check("the menu has the timer ticked now", labels().indexOf(", *Go To Next Timer…,") >= 0, labels())
                click(menuRow("Go To Next Timer…"))
                return 500
            },
            () => {
                check("the panel has the slide's second, and a button that takes the timer off", panelOpen() && named("goToNextSeconds").text === "1"
                      && named("goToNextRemove").visible && !named("goToNextLoop").checked, named("goToNextSeconds").text)
                click(centre(named("goToNextRemove")))
                return 600
            },
            () => {
                check("taken off, the slide has none and its cell no badge", !panelOpen() && timerOf(0) === "none" && badge(0) === null && timerOf(2) === "0.5 loop")
                editGoToNext(2)
                return 400
            },
            () => {
                check("the other's panel has its half second and its tick", named("goToNextSeconds").text === "0.5" && named("goToNextLoop").checked)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc changes nothing", !panelOpen() && timerOf(2) === "0.5 loop")
                setGoToNext(2, -1, false)
                return 500
            },
            () => {
                check("with both timers off the file is what it was, to the byte", timerOf(2) === "none" && testInput.fileHash(kept.plain) === kept.before)
                // The Slide Show
                const button = named("slideShowButton")
                check("over the slides is the Slide Show button, unlit, at the left of the Arrangements button and of the destination's right",
                      button !== null && button.visible && !button.on && button.mapToItem(null, 0, 0).x > named("destinationButton").mapToItem(null, 0, 0).x
                      && button.mapToItem(null, 0, 0).x < named("arrangementToggle").mapToItem(null, 0, 0).x)
                click(centre(button))
                return 400
            },
            () => {
                check("its menu is ProPresenter's: None, a line, four times and Custom…", labels() === "[Slide Show], *None, --, 5 Seconds, 10 Seconds, 15 Seconds, 20 Seconds, Custom…",
                      labels())
                click(menuRow("10 Seconds"))
                return 600
            },
            () => {
                const button = named("slideShowButton")
                const words = Lib.find(button, i => i.text !== undefined && i.font !== undefined)
                check("ten seconds chosen: the presentation has it, and the button is lit and says so", document.slideShow === 10 && button.on && words.visible
                      && words.text === "10s", words.text)
                click(centre(button))
                return 400
            },
            () => {
                check("the menu has it ticked", labels().indexOf("*10 Seconds") >= 0 && labels().indexOf("*None") < 0, labels())
                click(menuRow("Custom…"))
                return 500
            },
            () => {
                check("Custom… puts up the panel with the seconds alone", panelOpen() && named("goToNextSeconds").text === "10" && !named("goToNextLoop").visible
                      && !named("goToNextRemove").visible && panel().parent.heading === "Slide Show")
                testInput.type("0")
                check("a Slide Show of no time cannot be saved", panel().parent.seconds < 0)
                named("goToNextSeconds").selectAll()
                testInput.type("0.6")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                check("a time of one's own is saved", document.slideShow === 0.6 && named("slideShowButton").on)
                testInput.snapshot(kept.plain, "advance-show")
                testInput.grab("3-slide-show")
                click(centre(named("slideShowButton")))
                return 400
            },
            () => {
                check("and the menu says what it is, ticked", labels().indexOf("*Custom… (0.6 s)") >= 0, labels())
                menu.close()
                check("a Slide Show starts nothing by being set", !Show.advanceWaiting && cleared)
                click(centre(slideCell(1)))
                return 900
            },
            () => {
                check("a slide triggered, every slide leads to the next", Show.liveIndex === 2 && is(out(atPlain), "#ffffff") && Show.advanceWaiting
                      && Show.advanceSeconds === 0.6, Show.liveIndex + " " + out(atPlain))
                return 700
            },
            () => {
                check("and the last to the first", Show.liveIndex === 0 && is(out(atPlain), "#ffff00") && Show.advanceWaiting, Show.liveIndex + " " + out(atPlain))
                testInput.key(Qt.Key_F2)
                // A slide's own timer wins over the Slide Show
                setGoToNext(1, 1.6, false)
                return 500
            },
            () => {
                check("a slide of it is given a longer time of its own", timerOf(1) === "1.6" && badgeWords(1) === "1.6s" && document.slideShow === 0.6)
                click(centre(slideCell(1)))
                return 1000
            },
            () => {
                check("which is the time it stays: after a second it is still there, where the Slide Show would have moved on", Show.liveIndex === 1
                      && Show.advanceWaiting && Show.advanceSeconds === 1.6, Show.liveIndex)
                return 900
            },
            () => {
                check("and then it goes on", Show.liveIndex === 2 || Show.liveIndex === 0, Show.liveIndex)
                // The announcements go on by their own time, while the show goes on by its
                testInput.key(Qt.Key_F2)
                setGoToNext(1, -1, false)
                check("the notices are sent to the announcements and given a Slide Show of their own", catalog.setDestination(entry("Notices").file, "announcements") === ""
                      && catalog.setSlideShow(entry("Notices").file, 1.5) === "")
                return 500
            },
            () => {
                open("Notices")
                return 500
            },
            () => {
                check("they have both", document.destination === "announcements" && document.slideShow === 1.5)
                click(centre(slideCell(0)))
                open("Plain")
                return 400
            },
            () => {
                click(centre(slideCell(0)))
                return 300
            },
            () => {
                check("a notice and a slide are live, each counting its own time", Show.announcementIndex === 0 && Show.liveIndex === 0 && Show.advanceWaiting
                      && Show.advanceSeconds === 0.6 && Show.announcementAdvanceWaiting && Show.announcementAdvanceSeconds === 1.5
                      && is(out(atNotice), "#ff0000") && is(out(atPlain), "#ffff00"), out(atNotice) + " " + out(atPlain))
                return 700
            },
            () => {
                check("the show has gone on and the announcements have not yet", Show.liveIndex === 1 && Show.announcementIndex === 0
                      && is(out(atNotice), "#ff0000") && is(out(atPlain), "#00ffff"), Show.liveIndex + " " + Show.announcementIndex)
                return 1000
            },
            () => {
                check("then the announcements have too, on their own layer", Show.announcementIndex === 1 && is(out(atNotice), "#00ff00"),
                      Show.announcementIndex + " " + out(atNotice))
                click(centre(named("clearAnnouncements")))
                kept.index = Show.liveIndex
                return 800
            },
            () => {
                check("clearing the announcements stops theirs and leaves the show's going", !Show.announcementAdvanceWaiting && Show.announcementIndex === 1
                      && Show.advanceWaiting && Show.liveIndex !== kept.index, Show.liveIndex + " from " + kept.index)
                // The Slide Show taken off while it is going
                click(centre(named("slideShowButton")))
                return 300
            },
            () => {
                click(menuRow("None"))
                return 500
            },
            () => {
                kept.index = Show.liveIndex
                check("None takes the Slide Show off, and the show stops going on at once", document.slideShow === 0 && !named("slideShowButton").on
                      && !Show.advanceWaiting && !cleared)
                check("the file is what it was to begin with, to the byte", testInput.fileHash(kept.plain) === kept.before)
                return 900
            },
            () => {
                check("and stays where it is", Show.liveIndex === kept.index, Show.liveIndex)
                // A timer that was in the file
                open("Timed")
                return 500
            },
            () => {
                check("a timer ProPresenter made is read: three seconds, to the next", timerOf(0) === "3" && badgeWords(0) === "3s" && timerOf(1) === "none")
                editGoToNext(0)
                return 400
            },
            () => {
                check("its panel has the three seconds and the button", named("goToNextSeconds").text === "3" && named("goToNextRemove").visible)
                click(centre(named("goToNextRemove")))
                return 600
            },
            () => {
                check("and it can be taken off", timerOf(0) === "none" && badge(0) === null)
                testInput.key(Qt.Key_F1)
                check("nothing was reported", notice === "", notice)
            },
        ]
        next()
    }
}
