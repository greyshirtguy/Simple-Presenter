import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A look and what is on a layer changing together (the maintainer, 2026-11-11). With a look live that keeps the slides
// from the stream, a slide whose action goes over to a look that gives them back comes onto the stream by itself, from
// nothing, and the slide that was live before it is not seen there, not for a frame: it was, dissolving away, and for a
// moment even with a cut, the look's change being made at once and the slide's a frame or more later. And the other way
// about: a slide whose action goes over to a look that takes the slides from the stream by a fade is not seen there as
// they go. What the stream draws is watched from moment to moment, at a grid of points of its window, and what its
// slide layer holds in sight.
QtObject {
    id: t

//COMMON
    // The points of the stream's window that are watched: all of it under its title, a grid close enough to find its words
    readonly property var points: {
        const all = []
        for (let row = 0; row < 86; ++row) {
            for (let column = 0; column < 60; ++column)
                all.push([0.008 + column * 0.0164, 0.125 + row * 0.01])
        }
        return all
    }
    property var samples: []
    property Timer sampler: Timer {
        interval: 25
        repeat: true
        onTriggered: {
            const layer = t.streamLayer()
            t.samples.push({ at: Date.now(), opacity: layer ? layer.opacity : -1, drawn: t.drawnThere(), greys: t.streamNow() })
        }
    }

    function streamLayer() {
        const content = testInput.windowContent("output-" + kept.stream)
        return content ? Lib.find(content, item => item.name === "slide layer") : null
    }

    // How light the stream's window is at each of the points, now
    function streamNow() {
        return testInput.greysIn("output-" + kept.stream, points)
    }

    // The two holders of the stream's slide layer
    function holders() {
        const layer = streamLayer()
        const found = []
        if (!layer)
            return found
        for (let i = 0; i < layer.children.length; ++i) {
            const child = layer.children[i]
            if (child.sourceComponent !== undefined && child.item)
                found.push(child)
        }
        return found
    }

    // The slides the stream's slide layer is drawing, by id: what its holders that are in sight hold, while the layer
    // itself is in sight
    function drawnThere() {
        const layer = streamLayer()
        if (!layer || !layer.visible || layer.opacity <= 0)
            return []
        return holders().filter(holder => holder.visible && holder.item.opacity > 0 && holder.item.content).map(holder => holder.item.content.id)
    }

    function watch() {
        samples = []
        sampler.start()
    }

    function mean(values) {
        return values.length === 0 ? 0 : values.reduce((sum, value) => sum + value, 0) / values.length
    }

    // The points one picture of the stream lights and another leaves dark
    function litByAlone(one, other) {
        const found = []
        for (let i = 0; i < one.length; ++i) {
            if (one[i] > 120 && other[i] < 50)
                found.push(i)
        }
        return found
    }

    // The most that any of some points was lighter, at any of the looks at the stream, than `picture` has it
    function lighterThan(picture, places) {
        let most = 0
        for (const sample of samples) {
            for (const i of places)
                most = Math.max(most, sample.greys[i] - picture[i])
        }
        return most
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                kept.room = Screens.audience[0].id
                kept.stream = Screens.audience[1].id
                // A look that gives the stream its slides (the quickest there is: the maintainer's are at once), and one that
                // takes them away by a fade
                const gives = Looks.looks.filter(l => Looks.of(l.id, kept.stream).slide === true).sort((a, b) => a.transition - b.transition)
                const takes = Looks.looks.filter(l => Looks.of(l.id, kept.stream).slide === false).sort((a, b) => b.transition - a.transition)
                check("(a look that gives the stream its slides, and one that takes them from it by a fade)", gives.length > 0 && takes.length > 0 && takes[0].transition > 0,
                      JSON.stringify(Looks.looks.map(l => [l.name, l.transition, Looks.of(l.id, kept.stream).slide])))
                kept.gives = gives[0]
                kept.takes = takes.find(l => l.name === "Stream Clear") ?? takes[0]
                check("(the look that gives them is made at once, as the maintainer's are: run.py's prepare_lookreveal has it so)", kept.gives.transition === 0, kept.gives.name + " " + kept.gives.transition)
                testInput.say("  the look that gives: " + kept.gives.name + ", over " + kept.gives.transition + " s; the look that takes: " + kept.takes.name + ", over " + kept.takes.transition + " s")
                // The two dress the stream alike to begin with, so that the look's change is of the stream's layers alone (a
                // look that gives the stream another theme as well has the live slide shown again there: that is at the end)
                const own = Looks.of(kept.gives.id, kept.stream), other = Looks.of(kept.takes.id, kept.stream)
                kept.ownTheme = { theme: own.theme, themeSlide: own.themeSlide }
                Looks.setScreen(kept.gives.id, kept.stream, { theme: other.theme, themeSlide: other.themeSlide })
                check("(the two looks dress the stream alike)", Looks.of(kept.gives.id, kept.stream).theme === other.theme
                      && Looks.of(kept.gives.id, kept.stream).themeSlide === other.themeSlide, JSON.stringify([own.theme, other.theme]))
                // The stream, which nothing is said about on this computer, is given a window
                Screens.setOutput(kept.stream, { output: "window" })
                selectTransition("Dissolve")
                transitionDuration = 0.8
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                return 1000
            },
            () => {
                const worded = document.slides.map((slide, place) => ({ i: place, id: slide.id, words: slide.plainText.trim(), actions: slide.actions.length }))
                                             .filter(slide => slide.words !== "" && slide.actions === 0)
                kept.a = worded[0]
                kept.b = worded.find(slide => slide.words !== kept.a.words)
                kept.c = worded.find(slide => slide.words !== kept.a.words && slide.words !== kept.b.words)
                // (And one far from them, whose words are not painted ahead as the next slides' are: its change takes longer)
                kept.d = worded.slice().reverse().find(slide => slide.words !== kept.a.words && slide.words !== kept.b.words && slide.words !== kept.c.words)
                check("(four slides of different words, with no actions of their own)", kept.a !== undefined && kept.b !== undefined && kept.c !== undefined
                      && kept.d !== undefined && kept.d.i > kept.c.i + 4, worded.length)
                check("the stream has a window of its own, with its slide layer", streamLayer() !== null && holders().length === 2, holders().length)
                // The second brings the look that gives the stream its slides
                commitAction({ slide: kept.b.i }, { kind: "look", lookId: kept.gives.id, lookName: kept.gives.name }, "")
                return 600
            },
            () => {
                // And the third the look that takes them away
                commitAction({ slide: kept.c.i }, { kind: "look", lookId: kept.takes.id, lookName: kept.takes.name }, "")
                return 600
            },
            () => {
                // The far one gives them back too
                commitAction({ slide: kept.d.i }, { kind: "look", lookId: kept.gives.id, lookName: kept.gives.name }, "")
                return 600
            },
            () => {
                check("the two slides have their look actions", document.slides[kept.b.i].actions.some(action => action.kind === "look" && action.lookId === kept.gives.id)
                      && document.slides[kept.c.i].actions.some(action => action.kind === "look" && action.lookId === kept.takes.id)
                      && document.slides[kept.a.i].id === kept.a.id && document.slides[kept.b.i].id === kept.b.id && document.slides[kept.c.i].id === kept.c.id)
                // The stream's own picture of the first slide, to know its words by
                Show.lookId = kept.gives.id
                goLive(kept.a.i)
                return 2200
            },
            () => {
                kept.pictureA = streamNow()
                check("(the stream shows the first slide's words)", kept.pictureA.filter(grey => grey > 120).length >= 20 && drawnThere().join() === kept.a.id,
                      kept.pictureA.filter(grey => grey > 120).length + " points lit")
                testInput.grabIn("output-" + kept.stream, "1-stream-first-slide")
                // By hand: the look that keeps the slides from the stream
                Show.lookId = kept.takes.id
                return kept.takes.transition * 1000 + 700
            },
            () => {
                const dark = streamNow()
                check("with the look live that keeps the slides from the stream, the stream shows none", dark.every(grey => grey < 40) && streamLayer().opacity === 0,
                      "lightest " + Math.max.apply(null, dark) + ", layer " + streamLayer().opacity)
                check("(the room shows the slide still)", testInput.lit(false, 0, 0, 1, 1) > 0.001, testInput.lit(false, 0, 0, 1, 1))
                // ---- The slide whose action gives the stream its slides back
                watch()
                goLive(kept.b.i)
                return 2400
            },
            () => {
                sampler.stop()
                kept.pictureB = streamNow()
                testInput.grabIn("output-" + kept.stream, "2-stream-second-slide")
                const onlyA = litByAlone(kept.pictureA, kept.pictureB)
                const onlyB = litByAlone(kept.pictureB, dark())
                check("(the two slides' words are told apart on the stream)", onlyA.length >= 5 && onlyB.length >= 20, onlyA.length + " points lit by the first alone, " + onlyB.length + " by the second")
                check("its look is live, and the stream has its slides", Looks.live.origin === kept.gives.id && streamLayer().opacity === 1 && drawnThere().join() === kept.b.id,
                      Looks.live.name + " " + streamLayer().opacity + " " + drawnThere().join())
                // (The first slide's own points are never lighter than the second slide leaves them: its words would light them)
                const worst = lighterThan(kept.pictureB, onlyA)
                check("the slide before it is never seen on the stream as the look gives it its slides back: not for a frame", samples.length >= 20 && worst <= 30,
                      samples.length + " looks at the stream, the most one of the first slide's own points was lighter than the second slide has it " + worst)
                check("nor does the stream's slide layer draw it at any moment that it is in sight", samples.every(sample => !sample.drawn.includes(kept.a.id)),
                      samples.filter(sample => sample.drawn.includes(kept.a.id)).length + " of " + samples.length)
                const rise = samples.map(sample => mean(onlyB.map(i => sample.greys[i])))
                const full = mean(onlyB.map(i => kept.pictureB[i]))
                check("the new slide comes onto the stream from nothing, by the transition: through values between, and never back",
                      rise.some(value => value > full * 0.15 && value < full * 0.85) && rise.every((value, i) => i === 0 || value >= rise[i - 1] - 12)
                      && rise[0] < full * 0.3 && rise[rise.length - 1] > full * 0.9, rise.map(value => Math.round(value)).join(" ") + " to " + Math.round(full))
                check("the room's slide changed by the transition, as it always has", logged(/transition +slide layer: Dissolve over 0\.80 s/).length >= 2)
                // ---- The other way about: the slide whose action takes the stream's slides away, by the look's fade
                watch()
                goLive(kept.c.i)
                return kept.takes.transition * 1000 + 1300
            },
            () => {
                sampler.stop()
                check("its look is live, and the stream's slides have gone", Looks.live.origin === kept.takes.id && streamLayer().opacity === 0, Looks.live.name + " " + streamLayer().opacity)
                // (Fading from the second slide, no point is lighter than that slide had it: nothing of the third lights one)
                let over = 0
                for (const sample of samples) {
                    for (let i = 0; i < sample.greys.length; ++i)
                        over = Math.max(over, sample.greys[i] - kept.pictureB[i])
                }
                const onlyB = litByAlone(kept.pictureB, dark())
                const fall = samples.map(sample => mean(onlyB.map(i => sample.greys[i])))
                const full = mean(onlyB.map(i => kept.pictureB[i]))
                check("the slide that came with that look is not seen on the stream as its slides go: the stream fades from what it showed", samples.length >= 20 && over <= 45,
                      samples.length + " looks at the stream, the most a point was lighter than the second slide had it " + over)
                check("by the look's fade: through values between, and never back", fall.some(value => value > full * 0.15 && value < full * 0.85)
                      && fall.every((value, i) => i === 0 || value <= fall[i - 1] + 12) && fall[fall.length - 1] < full * 0.1, fall.map(value => Math.round(value)).join(" "))
                check("nor does the stream's slide layer draw it while it is in sight", samples.every(sample => !sample.drawn.includes(kept.c.id)),
                      samples.filter(sample => sample.drawn.includes(kept.c.id)).length + " of " + samples.length)
                // (Its holders are out of sight with it: what they hold is looked at, whether or not they are)
                const held = holders().filter(holder => holder.item.content).map(holder => holder.item.content.id)
                check("out of sight, the layer has that slide, for when a look gives the slides back", held.join() === kept.c.id, held.join())
                // ---- A look that gives the slides back by itself shows what the layer has
                Show.lookId = kept.gives.id
                return kept.gives.transition * 1000 + 900
            },
            () => {
                kept.pictureC = streamNow()
                check("a look that gives the stream its slides back by itself shows the slide that is live", streamLayer().opacity === 1 && drawnThere().join() === kept.c.id
                      && kept.pictureC.filter(grey => grey > 120).length >= 20, drawnThere().join() + " " + kept.pictureC.filter(grey => grey > 120).length)
                Show.lookId = kept.takes.id
                return kept.takes.transition * 1000 + 700
            },
            () => {
                // ---- With a cut, which showed the slide before for a moment; and with the look dressing the stream in its
                // own theme, where it has one, which has the slide shown again there as the theme changes
                check("(the stream's slides are away again)", streamLayer().opacity === 0)
                Looks.setScreen(kept.gives.id, kept.stream, kept.ownTheme)
                transitionIndex = 0
                watch()
                goLive(kept.d.i)
                return 1500
            },
            () => {
                sampler.stop()
                // (No point of the stream is ever lighter than it ends: nothing is shown there but what the slide comes to,
                // whatever the theme makes of it)
                const ends = streamNow()
                const worst = lighterThan(ends, points.map((point, i) => i))
                check("with a cut too, nothing is seen on the stream as the look gives it its slides back but the slide that brought the look",
                      samples.length >= 20 && worst <= 30 && samples.every(sample => !sample.drawn.includes(kept.c.id)),
                      samples.length + " looks at the stream, the most a point was lighter than it ends " + worst
                      + "; the slide before drawn in " + samples.filter(sample => sample.drawn.includes(kept.c.id)).length)
                check("and that slide is there", streamLayer().opacity === 1 && drawnThere().join() === kept.d.id
                      && litByAlone(ends, dark()).length >= 20, drawnThere().join())
                const wrong = logged(/PROBLEM|WARNING|ERROR/).filter(line => !line.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
            }
        ]
        next()
    }

    // A picture of the stream with nothing lit
    function dark() {
        return points.map(() => 0)
    }
}
