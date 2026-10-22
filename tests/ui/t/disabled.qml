import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Disabled slides (ProPresenter's Enable/Disable: a cue's isEnabled; src/prodocument.h, show::State::stepTarget and
// takeAdvance), on the Demo workspace's "It Is Well", which has a slide ProPresenter disabled: that slide listed and
// dimmed, in the grid and in the editor's list; Next and Previous passing over it; a click showing it, and Next going
// on from it; Enable Slide and Disable Slide from a slide's menu, kept in the file; a Go To Next Timer passing over a
// disabled slide; and the next slide the operator is shown being the next that is not disabled. after_disabled in run.py
// reads the file.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function until(ready, ms, then) {
        return () => {
            if (kept.since === undefined)
                kept.since = testInput.now()
            if (!ready() && testInput.now() - kept.since < ms) {
                --t.at
                return 100
            }
            kept.since = undefined
            return then()
        }
    }

    function disabledPlaces() {
        return document.slides.map((s, i) => s.disabled === true ? i : -1).filter(i => i >= 0)
    }

    function frame(index) {
        slideCell(index)
        return named("slideFrame:" + index)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "Demo").path)
                openEntry(entry("It Is Well"))
                return 700
            },
            () => {
                kept.off = disabledPlaces()
                kept.count = document.slides.length
                check("the slide ProPresenter disabled is listed, and marked so", kept.off.length === 1 && kept.count === 18, kept.off.join() + " of " + kept.count)
                kept.k = kept.off[0]
                check("and drawn dimmed in the grid", frame(kept.k) !== null && frame(kept.k).opacity < 0.5 && frame(kept.k - 1).opacity === 1)
                goLive(kept.k - 1)
                return 500
            },
            () => {
                check("the next slide the operator is shown is the one after it", nextSlide !== null && nextSlide.id === document.slides[kept.k + 1].id)
                step(1)
                return 500
            },
            () => {
                check("Next passes over it", liveIndex === kept.k + 1, liveIndex)
                step(-1)
                return 500
            },
            () => {
                check("and so does Previous", liveIndex === kept.k - 1, liveIndex)
                clickSlide(kept.k)
                return 500
            },
            () => {
                check("a click on it shows it, on the output too", liveIndex === kept.k && liveSlide.id === document.slides[kept.k].id
                      && Lib.find(output.contentItem, i => i.slide !== undefined && i.slide !== null && i.slide.id === document.slides[kept.k].id && i.visible) !== null)
                step(1)
                return 500
            },
            () => {
                check("and Next goes on from it", liveIndex === kept.k + 1, liveIndex)
                testInput.grab("1-disabled")
                const at = centre(frame(kept.k))
                click(at, Qt.RightButton)
                return 400
            },
            () => {
                check("its menu has Enable Slide", labels().includes("Enable Slide") && !labels().includes("Disable Slide"), labels())
                click(menuRow("Enable Slide"))
                return 700
            },
            () => {
                check("enabled, it is no longer marked, nor dimmed", disabledPlaces().length === 0 && frame(kept.k).opacity === 1 && document.slides.length === kept.count)
                kept.d = 2
                click(centre(frame(kept.d)), Qt.RightButton)
                return 400
            },
            () => {
                check("another's menu has Disable Slide", labels().includes("Disable Slide"), labels())
                click(menuRow("Disable Slide"))
                return 700
            },
            () => {
                check("disabled, it is marked and dimmed", JSON.stringify(disabledPlaces()) === JSON.stringify([kept.d]) && frame(kept.d).opacity < 0.5)
                // A Go To Next Timer on the slide before it
                report(catalog.setSlideGoToNext(document.path, document.slides[kept.d - 1].id, true, 1, false))
                reloadDocument()
                return 500
            },
            () => {
                goLive(kept.d - 1)
                kept.started = testInput.now()
                return 300
            },
            until(() => liveIndex !== kept.d - 1, 4000, () => {
                check("a Go To Next Timer passes over it to the next", liveIndex === kept.d + 1 && testInput.now() - kept.started > 800, liveIndex)
                startEditing(currentEntry(), document.slides[kept.d].id)
                return 900
            }),
            () => {
                const rows = Lib.findAll(editScreen.contentItem ?? win.contentItem, i => i.slide !== undefined && i.slide !== null && i.slide.id !== undefined
                                         && i.current !== undefined)
                const row = rows.find(r => r.slide.id === document.slides[kept.d].id)
                check("the editor lists it, dimmed", row !== undefined && Lib.find(row, i => i.opacity !== undefined && i.opacity < 0.5) !== null, rows.length)
                stopEditing()
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 300
            },
        ]
        next()
    }
}
