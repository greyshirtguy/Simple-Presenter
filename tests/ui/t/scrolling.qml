import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Scrolling Text (src/textscroller.h, qml/ScrollingText.qml), on the Demo workspace's "It Is Well", which has a box
// ProPresenter made to scroll left, over and over, showing the slide's lyrics on one line (a link to its Lyrics box): the box's words going left on the output, by the speed its slider
// says, and in the operator's preview, and still in the slide's thumbnail; drawn moving (the output's pixels there
// change), and by the same in every frame of the output (by FrameClock, src/frameclock.h); then in the editor's Text tab, Scrolling ticked with ProPresenter's settings, and changed: going right, Start
// Off-Screen, a quicker Speed, the feathering unlocked and set at one end, Repeat off; shown again going right from off
// the box, and gone once past it; Scrolling switched off, and on again with the settings it had; and set going down, its
// words then laid out as wide as the box. after_scrolling in run.py reads the
// file.
QtObject {
    id: t

//COMMON
    function scrollers(root) {
        return Lib.findAll(root, i => i.settings !== undefined && i.travelled !== undefined && i.places !== undefined && i.visible)
    }

    // The scrolling box being tried, on the output (its slide has twelve others)
    function scroller() {
        return scrollers(output.contentItem).find(i => i.source.id === kept.element.id) ?? null
    }

    // The output's pixels along the middle of the box, where it is on the screen
    function along() {
        const e = kept.element
        const points = []
        for (let x = Math.max(e.x, 0) + 20; x < Math.min(e.x + e.width, 1920) - 20; x += 90)
            points.push(testInput.pixel(false, x / 1920, (e.y + e.height / 2) / 1080))
        return points
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries.find(l => l.name === "Demo").path)
                openEntry(entry("It Is Well"))
                return 700
            },
            () => {
                // (Every slide has the box, linked to the slide's lyrics; the first that has words)
                kept.index = document.slides.findIndex(s => s.elements.some(e => e.scroll !== undefined && e.hasText))
                const slide = document.slides[kept.index]
                kept.element = slide ? slide.elements.find(e => e.scroll !== undefined && e.hasText) : null
                check("a box ProPresenter made to scroll is read as one: left, over and over", kept.element !== null && kept.element.scroll.direction === "left"
                      && kept.element.scroll.repeat === true && Math.abs(kept.element.scroll.rate - 0.151) < 0.001, kept.element ? JSON.stringify(kept.element.scroll) : "none")
                goLive(kept.index)
                return 800
            },
            () => {
                const out = scroller()
                kept.first = out ? out.travelled : -1
                kept.pixels = along()
                kept.time = testInput.now()
                check("on the output its words go by ScrollingText: thirteen boxes of them on this slide, the one tried among them",
                      out !== null && scrollers(output.contentItem).length === 13 && out.places().length >= 1, scrollers(output.contentItem).length)
                check("each drawn by one shader, and the words by no item of their own", out !== null
                      && Lib.findAll(out, i => i.objectName === "scrollingWords" && i.visible).length === 1
                      && Lib.findAll(out, i => i.sourceItem !== undefined && i.hideSource === true && i.sourceItem && i.sourceItem.content !== undefined).length === 1)
                check("still in the slide's thumbnail", scrollers(named("slideGrid")).length === 0)
                check("and going in the operator's preview", scrollers(win.contentItem).length >= 13, scrollers(win.contentItem).length)
                return 1000
            },
            () => {
                const out = scroller()
                const seconds = (testInput.now() - kept.time) / 1000
                const went = out.travelled - kept.first
                const expected = TextScrolling.speed(kept.element.scroll.rate) * seconds
                check("they go at the speed the slider says", Math.abs(went - expected) < expected * 0.25 + 4, went.toFixed(1) + " for " + expected.toFixed(1))
                const pixels = along()
                check("and are drawn moving: the output's pixels along the box are not what they were", pixels.join() !== kept.pixels.join())
                check("over and over: the next going is " + "on its way", out.settings.repeat && out.places(), out.cycle.toFixed(0))
                testInput.grabOutput("1-scrolling")
                // How far they go in each frame of the output, for two seconds
                testInput.watchMotion(out, "travelled")
                return 2000
            },
            () => {
                const motion = testInput.motionSoFar()
                check("and go on by the same in every frame of the output, the screen's own frames being what they go by",
                      motion.frames > 90 && motion.uneven <= 2 && motion.stopped <= 1, JSON.stringify(motion))
                startEditing(currentEntry(), document.slides[kept.index].id)
                return 900
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "text"
                return 500
            },
            () => {
                check("the editor's Text tab has Scrolling ticked, and ProPresenter's settings", named("scrollingOn").checked && named("scrollDirection:left").chosen
                      && named("scrollStart").currentIndex === 0 && Math.abs(named("scrollSpeed").value - 0.151) < 0.002 && named("scrollRepeat").checked)
                check("and the words still there, in the editor", scrollers(editScreen.canvas).length === 0)
                testInput.grab("2-inspector")
                named("scrollDirection:right").children[1].clicked(null)
                return 400
            },
            () => {
                named("scrollStart").activated(1)
                return 300
            },
            () => {
                editScreen.inspector.setProperties({ scrollRate: 0.8 }, false)
                named("scrollFadeLock").children[1].clicked(null)
                return 300
            },
            () => {
                editScreen.inspector.setProperties({ scrollFadeLeft: 0.3 }, false)
                named("scrollRepeat").toggled(false)
                return 500
            },
            () => {
                const s = editScreen.canvas.selected.scroll
                check("changed: going right, from off the box, quicker, feathered at one end, once", s.direction === "right" && s.offScreen === true
                      && Math.abs(s.rate - 0.8) < 0.001 && s.fadeLeft === 0.3 && Math.abs(s.fadeRight - kept.element.scroll.fadeRight) < 0.001 && s.repeat === false,
                      JSON.stringify(s))
                check("(the feathering unlocked)", !Lib.find(win.contentItem, i => i.locked !== undefined && i.settings !== undefined).locked)
                stopEditing()
                return 800
            },
            // (Past the transition, as below)
            () => {
                goLive(kept.index)
                return 700
            },
            () => {
                const out = scroller()
                check("shown again it goes right, coming in from off the box at its left", out !== null && out.settings.direction === "right" && out.backwards
                      && out.places().length <= 1 && out.going && out.travelled < out.begin + 400 && out.begin === out.room)
                // (Most of the way: a line this long at this speed takes half a minute to cross)
                out.travelled = out.begin + out.length + out.bleed - 60
                return 700
            },
            () => {
                const out = scroller()
                kept.travelled = out.travelled
                check("once across, it is gone, and nothing more is drawn for it", !out.going && out.places().length === 0,
                      out.travelled.toFixed(0) + " of " + (out.begin + out.length).toFixed(0))
                return 300
            },
            () => {
                check("(and it has stopped)", scroller().travelled === kept.travelled)
                startEditing(currentEntry(), document.slides[kept.index].id)
                return 900
            },
            () => {
                editScreen.canvas.pick(kept.element.id)
                editScreen.inspector.tab = "text"
                named("scrollingOn").toggled(false)
                return 500
            },
            () => {
                check("Scrolling switched off, the box has none", editScreen.canvas.selected.scroll === undefined && !named("scrollingOn").checked)
                named("scrollingOn").toggled(true)
                return 500
            },
            () => {
                check("and on again, its settings are as they were", editScreen.canvas.selected.scroll !== undefined && editScreen.canvas.selected.scroll.direction === "right"
                      && Math.abs(editScreen.canvas.selected.scroll.rate - 0.8) < 0.001)
                named("scrollDirection:down").children[1].clicked(null)
                return 400
            },
            () => {
                check("set going down", editScreen.canvas.selected.scroll.direction === "down" && named("scrollDirection:down").chosen)
                stopEditing()
                return 800
            },
            // (Past the transition, when the slide as it was is gone from the output)
            () => {
                goLive(kept.index)
                return 700
            },
            () => {
                const out = scroller()
                const texture = Lib.find(out, i => i.sourceItem !== undefined && i.hideSource === true)
                check("shown again it goes down, from off the box at its top: its words laid out as wide as the box", out !== null && !out.across && out.backwards
                      && Math.abs(texture.textureSize.width - (kept.element.width + 2 * out.bleed) * out.unit) <= 1 && out.length < kept.element.width,
                      out ? texture.textureSize.width + " " + out.length.toFixed(0) : "none")
                testInput.grabOutput("2-down")
                return 300
            },
            () => {
                stopEditing()
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /qrc:|TypeError|ReferenceError/.test(l))
                check("nothing went wrong in the windows' scripts", wrong.length === 0, wrong.join(" | "))
                return 300
            },
        ]
        next()
    }
}
