import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The Text button's popover (TextFormatPanel, textformat.h) and the slides it goes to (SlideSelection), on slides
// ProPresenter wrote. Ctrl and a click selects a slide, with a blue ring drawn round it and nothing taken live, and
// lets it go again; Shift and a click selects every slide from the last one selected, or from the first; the live
// slide counts as selected; Esc, a click alone and another presentation let them go. With none selected, a size
// typed goes to every run of every slide; with two selected, the size's arrow, a colour, the two alignments, the
// outline, the shadow, the capitals, the style and the family go to theirs alone, with everything else in the file
// as it was; the red is drawn on the output, in the operator's preview and in the selected slide's thumbnail, and not
// in another's; the size box shows what all have, and is empty where they differ; Revert puts the file back byte for
// byte. And the Text button, drawn as Aa between Search and Themes, lit while its popover is open, doing nothing while
// editing.
QtObject {
    id: t

//COMMON
    function worded(slide) {
        return slide.elements.filter(e => testInput.plain(e.text).trim() !== "")
    }

    function slidesOf(entry) {
        return catalog.open(entry.file).slides
    }

    // What `pick` gives of each run of a slide's text boxes with words
    function runsOn(slide, pick) {
        return worded(slide).reduce((all, e) => all.concat(testInput.runsOf(e.text).map(pick)), [])
    }

    // An element as the file has it, but for its text, what is worked out from it, and the keys that start with any
    // of `without`
    function restOf(e, without) {
        const rest = {}
        for (const key of Object.keys(e).sort()) {
            if (key !== "text" && key !== "displayText" && key !== "visible" && !(without ?? []).some(w => key.startsWith(w)))
                rest[key] = e[key]
        }
        return JSON.stringify(rest) + testInput.plain(e.text)
    }

    function looks(e) {
        return testInput.runsOf(e.text).map(r => [r.family, r.size, r.bold, r.italic, String(r.color), r.capitalization, r.strokeWidth,
                                                  String(r.strokeColor)].join(" "))
    }

    // A slide as a whole: what it does, its background, and each element, its text's look and its alignment with it
    function whole(slide) {
        return JSON.stringify([slide.actions, String(slide.backgroundColor), slide.drawsBackground])
               + slide.elements.map(e => restOf(e) + looks(e).join("|") + testInput.alignmentsOf(e.text).join(",")).join("||")
    }

    function clickWith(p, modifiers) {
        testInput.mouse(0, p.x, p.y, modifiers)
        testInput.mouse(2, p.x, p.y, modifiers)
    }

    function selectedAre(places) {
        return JSON.stringify(selectedSlideIds.slice().sort()) === JSON.stringify(places.map(i => document.slides[i].id).sort())
    }

    function ring(index) {
        const cell = slideCell(index)
        return cell ? Lib.find(cell, item => item.objectName === "selectedRing") : null
    }

    // The first eight slides that have a blue ring
    function rings() {
        const out = []
        for (let i = 0; i < Math.min(8, document.slides.length); ++i) {
            if (ring(i))
                out.push(i)
        }
        return JSON.stringify(out)
    }

    // Whether a slide's blue ring is drawn in the operator window: the middle of its left side
    function blueAt(index) {
        const r = ring(index)
        if (!r)
            return false
        const p = r.mapToItem(null, 1.5, r.height / 2)
        const c = String(testInput.windowPixel(p.x, p.y))
        const part = i => parseInt(c.substring(1 + 2 * i, 3 + 2 * i), 16)
        return part(2) > 180 && part(0) < 120
    }

    // How much red the operator sees in a slide's thumbnail: inside its frame, which is the colour of its group, and
    // above its label. (Words drawn this small are a few red points: any more than before is what is looked for.)
    function redIn(index) {
        const cell = slideCell(index)
        const frame = Lib.find(cell, item => item.objectName === "slideFrame:" + index)
        const at = frame.mapToItem(null, 0, 0)
        return testInput.windowRed(at.x + frame.width * 0.05, at.y + frame.height * 0.05, frame.width * 0.9, frame.height * 0.65)
    }

    function previewRed() {
        const preview = named("previewSlide")
        const at = preview.mapToItem(null, 0, 0)
        return testInput.windowRed(at.x, at.y, preview.width, preview.height)
    }

    function reds() {
        return { output: testInput.red(false, 0, 0, 1, 1), preview: previewRed(), selected: redIn(3), other: redIn(2) }
    }

    function typeInto(name, text) {
        const field = named(name)
        field.forceActiveFocus()
        field.selectAll()
        testInput.type(text)
        testInput.key(Qt.Key_Return)
    }

    // The On or the Off of the outline's or the shadow's switch
    function switchOf(name, on) {
        return centre(Lib.find(named(name), item => item.modelData === on && item.width === 36))
    }

    // Whether the slides at `places` have what `test` asks of each of their text boxes with words, and every other
    // slide is as it was before
    function onlyOn(after, places, test) {
        return places.every(i => worded(after[i]).length > 0 && worded(after[i]).every(test))
               && after.every((s, i) => places.includes(i) || whole(s) === whole(kept.before[i]))
    }

    function run() {
        steps = [
            () => {
                // ---- A presentation whose first six slides have words, and nothing else to do, each shown once
                for (const lib of catalog.libraries) {
                    openLibrary(lib.path)
                    for (const d of documents.filter(openable)) {
                        const slides = slidesOf(d)
                        const ids = slides.map(s => s.id)
                        if (slides.length >= 6 && new Set(ids).size === ids.length
                            && slides.slice(0, 6).every(s => worded(s).length > 0 && s.media === undefined && s.actions.length === 0)) {
                            kept.entry = d
                            break
                        }
                    }
                    if (kept.entry)
                        break
                }
                check("(a presentation whose first six slides have words and nothing else to do)", kept.entry !== undefined,
                      kept.entry ? kept.entry.name : "none")
                // (Under a look that gives the output no theme: a theme's boxes are drawn in place of the slide's own.)
                const plain = Looks.looks.find(look => Looks.of(look.id, Screens.audience[0].id).theme === "")
                if (plain)
                    Show.lookId = plain.id
                openEntry(kept.entry)
                return 800
            },
            () => {
                kept.count = document.slides.length
                const button = named("textButton")
                const letters = button ? Lib.find(button, item => item.text === "Aa" && item.visible) : null
                const drawn = letters ? strokes(letters) : { light: 0, dark: 0 }
                const x = item => item.mapToItem(null, 0, 0).x
                check("the toolbar has a Text button between Search and Themes, with Aa drawn on it", letters !== null && drawn.light > 0 && drawn.dark > 0
                      && x(named("searchButton")) < x(button) && x(button) < x(named("themesButton")), JSON.stringify(drawn))
                check("(nothing is live)", activeSlideIndex === -1 && !cueLive)
                // ---- Selecting
                clickWith(centre(slideCell(1)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl and a click selects a slide, and takes nothing live", selectedAre([1]) && !cueLive, JSON.stringify(selectedPlaces()))
                check("a selected slide has a blue ring round it in the operator window, and no other slide has one", blueAt(1) && rings() === "[1]", rings())
                clickWith(centre(slideCell(1)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl and a click again lets it go, and its ring", selectedSlideIds.length === 0 && rings() === "[]", rings())
                clickWith(centre(slideCell(1)), Qt.ControlModifier)
                clickWith(centre(slideCell(3)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl and a click on another adds it", selectedAre([1, 3]) && rings() === "[1,3]" && blueAt(3), JSON.stringify(selectedPlaces()))
                clickWith(centre(slideCell(5)), Qt.ShiftModifier)
                return 300
            },
            () => {
                check("Shift and a click selects every slide from the last one selected", selectedAre([1, 3, 4, 5]) && rings() === "[1,3,4,5]",
                      JSON.stringify(selectedPlaces()))
                clickWith(centre(slideCell(4)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("(one of them let go of with Ctrl)", selectedAre([1, 3, 5]), JSON.stringify(selectedPlaces()))
                clickWith(centre(slideCell(2)), Qt.ShiftModifier)
                return 300
            },
            () => {
                // (The last one clicked having been let go of, and nothing being live)
                check("Shift and a click then selects from the first slide", selectedAre([0, 1, 2, 3, 5]) && !cueLive, JSON.stringify(selectedPlaces()))
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc lets them all go", selectedSlideIds.length === 0 && rings() === "[]", rings())
                click(centre(slideCell(2)))
                return 800
            },
            () => {
                check("(a click alone takes a slide live)", cueLive && liveIndex === 2 && activeSlideIndex === 2)
                clickWith(centre(slideCell(4)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("the live slide counts as selected: Ctrl and a click on another selects the two", selectedAre([2, 4]), JSON.stringify(selectedPlaces()))
                check("the live one keeping its own ring, and the other having the blue", rings() === "[4]" && blueAt(4)
                      && Lib.find(slideCell(2), item => item.objectName === "liveRing") !== null, rings())
                click(centre(slideCell(1)))
                return 800
            },
            () => {
                check("a click alone takes its slide live and lets the others go", cueLive && liveIndex === 1 && selectedSlideIds.length === 0 && rings() === "[]",
                      JSON.stringify(selectedPlaces()))
                // ---- None selected: a size typed goes to every slide
                kept.before = slidesOf(kept.entry)
                named("textButton").clicked()
                return 500
            },
            () => {
                const loader = named("textFormat")
                const panel = loader ? loader.item : null
                const at = panel ? panel.mapToItem(null, 0, 0) : { x: 0, y: 0 }
                const button = named("textButton")
                check("the Text button opens its popover, under it, and is lit while it is open", textFormatOpen && panel !== null && button.on
                      && at.y >= toolbar.height && Math.abs(at.x - button.mapToItem(null, 0, 0).x) < 40
                      && String(testInput.windowPixel(at.x + 3, at.y + panel.height / 2)) === "#25272b",
                      panel ? at.x + "," + at.y + " " + testInput.windowPixel(at.x + 3, at.y + panel.height / 2) : "none")
                const goesTo = named("textFormat:goesTo")
                const ids = new Set(document.slides.map(s => s.id))
                check("with no slide selected, it says what it sets goes to all the slides", goesTo !== null && goesTo.text.includes("all " + ids.size + " slides"),
                      goesTo ? goesTo.text : "none")
                typeInto("textFormat:size", "50")
                return 1200
            },
            () => {
                const after = slidesOf(kept.entry)
                check("a size typed goes to every run of every slide's words", after.every(s => runsOn(s, r => r.size).every(size => size === 50)),
                      JSON.stringify(after.map(s => runsOn(s, r => r.size))))
                check("and nothing else changes: the words, the boxes, the other elements, what the slides do", after.length === kept.before.length
                      && after.every((s, i) => s.elements.length === kept.before[i].elements.length
                                     && s.elements.every((e, j) => restOf(e) === restOf(kept.before[i].elements[j]))
                                     && JSON.stringify(s.actions) === JSON.stringify(kept.before[i].actions)))
                check("the size box shows the size they all have now", named("textFormat:size").text === "50", named("textFormat:size").text)
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts the popover away", !textFormatOpen && named("textFormat").item === null && !named("textButton").on)
                clickWith(centre(slideCell(3)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("(the live slide and another selected)", selectedAre([1, 3]), JSON.stringify(selectedPlaces()))
                kept.before = slidesOf(kept.entry)
                kept.reds = reds()
                named("textButton").clicked()
                return 500
            },
            () => {
                const goesTo = named("textFormat:goesTo")
                check("with two selected, what is set goes to them", goesTo.text.includes("the 2 selected slides"), goesTo.text)
                testInput.grab("2-popover")
                click(centre(named("textFormat:sizeUp")))
                return 1200
            },
            () => {
                const after = slidesOf(kept.entry)
                check("the size's up arrow makes every run of the selected slides' words a point larger, and changes no other slide",
                      onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => r.size === 51)), JSON.stringify([1, 3].map(i => runsOn(after[i], r => r.size))))
                check("the size box shows their size", named("textFormat:size").text === "51", named("textFormat:size").text)
                named("textFormat:color").picked("#ff0000")
                return 1200
            },
            () => {
                const after = slidesOf(kept.entry)
                check("a colour goes to the selected slides' words", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => String(r.color) === "#ff0000")),
                      JSON.stringify([1, 3].map(i => runsOn(after[i], r => String(r.color)))))
                // (The popover is over some of the slides)
                testInput.key(Qt.Key_Escape)
                return 800
            },
            () => {
                const now = reds()
                testInput.grabOutput("1-red")
                testInput.grab("1-red-operator")
                check("the red is drawn on the output (the live slide is one of them), in the operator's preview and in the other selected slide's thumbnail, and not in another's",
                      now.output > kept.reds.output && now.preview > kept.reds.preview && now.selected > kept.reds.selected
                      && now.other <= kept.reds.other, JSON.stringify([kept.reds, now]))
                check("the popover put away leaves the slides selected", selectedAre([1, 3]) && rings() === "[3]", rings())
                kept.before = slidesOf(kept.entry)
                named("textButton").clicked()
                return 500
            },
            () => {
                click(centre(named("textFormat:alignRight")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Align Right goes to every paragraph of the selected slides", onlyOn(after, [1, 3], e => testInput.alignmentsOf(e.text).every(a => (a & 15) === 2)),
                      JSON.stringify(worded(after[1]).map(e => testInput.alignmentsOf(e.text))))
                check("and is lit", named("textFormat:alignRight").on && !named("textFormat:alignLeft").on)
                click(centre(named("textFormat:alignBottom")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Align Bottom goes to their text boxes", onlyOn(after, [1, 3], e => e.verticalAlignment === Qt.AlignBottom),
                      JSON.stringify(worded(after[1]).map(e => e.verticalAlignment)))
                click(switchOf("textFormat:stroke", true))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Stroke On outlines every run of their words", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => r.strokeWidth > 0)),
                      JSON.stringify(runsOn(after[1], r => r.strokeWidth)))
                typeInto("textFormat:strokeWidth", "4")
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                // (As near as the file keeps it: a share of the size)
                check("and a width typed is their outline's width", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => Math.abs(r.strokeWidth - 4) < 0.01)),
                      JSON.stringify(runsOn(after[1], r => r.strokeWidth)))
                named("textFormat:strokeColor").picked("#00ff00")
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("and a colour, its colour", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => String(r.strokeColor) === "#00ff00")),
                      JSON.stringify(runsOn(after[1], r => String(r.strokeColor))))
                click(switchOf("textFormat:shadow", true))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Shadow On gives their text boxes a shadow", onlyOn(after, [1, 3], e => e.textShadowEnabled === true),
                      JSON.stringify(worded(after[1]).map(e => e.textShadowEnabled)))
                typeInto("textFormat:shadowBlur", "12")
                return 1000
            },
            () => {
                typeInto("textFormat:shadowOpacity", "50")
                return 1000
            },
            () => {
                named("textFormat:shadowColor").picked("#0000ff")
                return 1000
            },
            () => {
                typeInto("textFormat:shadowAngle", "90")
                return 1000
            },
            () => {
                typeInto("textFormat:shadowOffset", "7")
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                const shadow = e => [e.textShadowRadius, Number(e.textShadowColor.a).toFixed(2), String(Qt.rgba(e.textShadowColor.r, e.textShadowColor.g, e.textShadowColor.b, 1)),
                                     e.textShadowAngle, e.textShadowOffset].join(" ")
                check("its blur, opacity, colour, angle and offset go to them", onlyOn(after, [1, 3], e => shadow(e) === "12 0.50 #0000ff 90 7"),
                      JSON.stringify(worded(after[1]).map(shadow)))
                check("the shadow's angle is shown on its dial", Math.round(named("textFormat:angleDial").value) === 90, named("textFormat:angleDial").value)
                click(centre(named("textFormat:shadowBlurUp")))
                return 1000
            },
            () => {
                click(centre(named("textFormat:shadowOpacityDown")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                const shadow = e => [e.textShadowRadius, Number(e.textShadowColor.a).toFixed(2)].join(" ")
                check("the arrows beside the blur and the opacity step them, a point and a hundredth", onlyOn(after, [1, 3], e => shadow(e) === "13 0.49"),
                      JSON.stringify(worded(after[1]).map(shadow)))
                check("and the boxes show the new numbers", named("textFormat:shadowBlur").text === "13" && named("textFormat:shadowOpacity").text === "49",
                      named("textFormat:shadowBlur").text + " " + named("textFormat:shadowOpacity").text)
                // (Whatever capitals the words have now: a song's are often in capitals already)
                kept.capitals = [1, 3].every(i => runsOn(after[i], r => r.capitalization).every(c => c === 1))
                check("(AA is lit if their words are all in capitals)", named("textFormat:capitals1").on === kept.capitals)
                click(centre(named("textFormat:capitals3")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Aa puts their words in Title Case, and is lit, and AA not", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => r.capitalization === 3))
                      && named("textFormat:capitals3").on && !named("textFormat:capitals1").on, JSON.stringify(runsOn(after[1], r => r.capitalization)))
                click(centre(named("textFormat:capitals1")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("AA puts them in capitals, and is lit", [1, 3].every(i => runsOn(after[i], r => r.capitalization).every(c => c === 1))
                      && named("textFormat:capitals1").on && !named("textFormat:capitals3").on, JSON.stringify(runsOn(after[1], r => r.capitalization)))
                click(centre(named("textFormat:capitals1")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("and clicked again, as they were typed", [1, 3].every(i => runsOn(after[i], r => r.capitalization).every(c => c === 0)) && !named("textFormat:capitals1").on,
                      JSON.stringify(runsOn(after[1], r => r.capitalization)))
                named("textFormat:style").activated(2)
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("a style, Bold, goes to their words", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => r.bold && !r.italic))
                      && named("textFormat:style").displayText === "Bold", JSON.stringify(runsOn(after[1], r => [r.bold, r.italic])))
                named("textFormat:family").picked("DejaVu Sans")
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("and a family", onlyOn(after, [1, 3], e => testInput.runsOf(e.text).every(r => r.family === "DejaVu Sans")),
                      JSON.stringify(runsOn(after[1], r => r.family)))
                check("their words, their boxes' places and everything else of their slides are as they were",
                      [1, 3].every(i => after[i].elements.length === kept.before[i].elements.length
                                   && after[i].elements.every((e, j) => restOf(e, ["verticalAlignment", "textShadow"]) === restOf(kept.before[i].elements[j], ["verticalAlignment", "textShadow"]))
                                   && JSON.stringify(after[i].actions) === JSON.stringify(kept.before[i].actions)))
                // ---- What differs is shown empty: every slide again, two of them a point larger
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("(a second Esc lets the slides go)", selectedSlideIds.length === 0)
                named("textButton").clicked()
                return 500
            },
            () => {
                check("where the slides' sizes differ, the size box is empty, and so are the other things that differ",
                      named("textFormat:size").text === "" && named("textFormat:style").displayText === "" && !named("textFormat:alignRight").on,
                      named("textFormat:size").text + " / " + named("textFormat:style").displayText)
                check("(Revert has nothing to take back yet)", !named("textFormat:revert").enabled)
                kept.hash = testInput.fileHash(kept.entry.file)
                kept.before = slidesOf(kept.entry)
                // The empty box gone into and left, with nothing typed
                named("textFormat:size").forceActiveFocus()
                return 300
            },
            () => {
                takeFocus()
                return 600
            },
            () => {
                check("an empty box gone into and left with nothing typed changes nothing", testInput.fileHash(kept.entry.file) === kept.hash
                      && named("textFormat:size").text === "", named("textFormat:size").text)
                // Up, in the empty box: each run's own size a point larger
                named("textFormat:size").forceActiveFocus()
                testInput.key(Qt.Key_Up)
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("Up in the empty size box makes each run's own size a point larger, as the arrow does",
                      after.every((s, i) => JSON.stringify(runsOn(s, r => r.size)) === JSON.stringify(runsOn(kept.before[i], r => r.size + 1))),
                      JSON.stringify([runsOn(after[1], r => r.size), runsOn(after[2], r => r.size)]))
                takeFocus()
                click(centre(named("textFormat:sizeDown")))
                return 1000
            },
            () => {
                const after = slidesOf(kept.entry)
                check("and the down arrow a point smaller again", after.every((s, i) => JSON.stringify(runsOn(s, r => r.size)) === JSON.stringify(runsOn(kept.before[i], r => r.size))),
                      JSON.stringify([runsOn(after[1], r => r.size), runsOn(after[2], r => r.size)]))
                check("(the file is not as it was, which Revert can take back)", testInput.fileHash(kept.entry.file) !== kept.hash && named("textFormat:revert").enabled)
                click(centre(named("textFormat:revert")))
                return 1000
            },
            () => {
                check("Revert puts the file back byte for byte, and has nothing more to take back", testInput.fileHash(kept.entry.file) === kept.hash
                      && !named("textFormat:revert").enabled)
                // A click outside the popover puts it away, and does nothing else
                const slides = named("slideGrid")
                click(slides.mapToItem(null, slides.width - 30, slides.height - 30))
                return 500
            },
            () => {
                check("a click outside the popover puts it away, and does nothing else", !textFormatOpen && cueLive && liveIndex === 1)
                // ---- The settings put it away
                named("textButton").clicked()
                settingsOpen = true
                return 500
            },
            () => {
                check("the settings opened put the popover away", settingsOpen && !textFormatOpen && named("textFormat").item === null)
                settingsOpen = false
                return 500
            },
            () => {
                // ---- Another presentation shown lets the slides go, and puts the popover away
                clickWith(centre(slideCell(3)), Qt.ControlModifier)
                kept.other = documents.filter(openable).find(d => d.file !== kept.entry.file)
                return 300
            },
            () => {
                check("(slides selected)", selectedAre([1, 3]))
                named("textButton").clicked()
                check("(the popover open)", textFormatOpen)
                openEntry(kept.other)
                return 600
            },
            () => {
                check("another presentation shown lets them go, and puts the popover away", selectedSlideIds.length === 0 && !textFormatOpen)
                openEntry(kept.entry)
                return 600
            },
            () => {
                // ---- In the editor (the popover open, which going into it puts away)
                named("textButton").clicked()
                check("(the popover open)", textFormatOpen)
                startEditing(kept.entry, document.slides[2].id)
                return 800
            },
            () => {
                check("going into the editor puts the popover away", editing && !textFormatOpen && named("textFormat").item === null)
                named("textButton").clicked()
                return 300
            },
            () => {
                check("the Text button does nothing while editing", !textFormatOpen)
                stopEditing()
                return 800
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
