import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Bible mode with a theme of ProPresenter's own (the Act workspace's New Life Chapel, its Bible Main slide), whose box
// for the verse is set to Single Line and has a box of its own for the reference: the two boxes guessed by their names;
// the box's Single Line kept on Bible slides, as the maintainer wants it, With Verse's reference running on after the
// words and every slide's words fitting its box as they are drawn, joined; each run of the words keeping its own look,
// a verse's number alone raised; the words drawn at the theme's
// size on a slide that begins with a verse's number as on one that does not (found 2026-10-15: such a slide was drawn
// small throughout), looked at in the operator's grid (the output here is dressed by the workspace's live look, which
// has a theme of its own); the reference in its own box when that is chosen; and the boxes chosen kept for each theme
// slide. The verses are the test runner's made-up Bible (tests/ui/run.py).
QtObject {
    id: t

//COMMON
    function panel() {
        return named("biblePanel").item
    }

    function slides() {
        return win.document ? win.document.slides : []
    }

    function find(passage) {
        const field = named("biblePassage")
        field.forceActiveFocus()
        field.selectAll()
        testInput.type(passage)
        testInput.key(Qt.Key_Return)
    }

    function box(slide, name) {
        return (slide.elements ?? []).find(e => e.textBox && e.name === name)
    }

    // The lines of words drawn in a slide's thumbnail in the grid, inside one of its boxes,
    // as [top, bottom] in the window's points from the box's top: where the rows that have
    // anything light in them begin and end
    function inkLines(index, element) {
        const cell = slideCell(index)
        const frame = named("slideGrid").frameWidth
        const width = cell.width - 12 - 2 * frame
        const corner = cell.mapToItem(null, 6 + frame, 6 + frame)
        const scale = width / 1920
        const left = corner.x + element.x * scale
        const top = corner.y + element.y * scale
        const lines = []
        let start = -1
        for (let row = 0; row <= Math.ceil(element.height * scale); ++row) {
            const on = row < element.height * scale && testInput.windowLighter(left, top + row, element.width * scale, 1, 110) > 0.004
            if (on && start < 0)
                start = row
            if (!on && start >= 0) {
                lines.push([start, row])
                start = -1
            }
        }
        return lines
    }

    function run() {
        steps = [
            () => {
                named("bibleButton").clicked()
                return 600
            },
            () => {
                Bibles.importFile("file://" + catalog.workspacePath + "/Made Up Bible.xml")
                return 1500
            },
            () => {
                const theme = Themes.themes.find(th => th.name === "New Life Chapel")
                kept.place = theme ? theme.place : ""
                kept.main = theme ? theme.slides.find(s => s.name === "Bible Main") : null
                kept.lower = theme ? theme.slides.find(s => s.name === "L3rd Bible") : null
                check("(the theme, with its Bible Main and L3rd Bible slides)", kept.main !== null && kept.lower !== null && kept.main !== undefined && kept.lower !== undefined)
                kept.themeVerse = kept.main && kept.main.slide ? (kept.main.slide.elements ?? []).find(e => e.name === "Verse") : null
                check("(whose verse box is Single Line)", kept.themeVerse !== undefined && kept.themeVerse !== null && kept.themeVerse.textTransform === 1,
                      kept.themeVerse ? kept.themeVerse.textTransform : "none")
                panel().edit({ theme: kept.place, themeSlide: kept.main.id, references: "verse", verseNumbers: true, flow: "even", breakOnNewVerse: false })
                return 800
            },
            () => {
                check("its boxes for the verse and for the reference, guessed by their names", panel().verseBoxName === "Verse" && panel().boxName(panel().referenceBox) === "Reference",
                      panel().verseBoxName + " / " + panel().referenceBox)
                // With Verse: the reference after the words, in their box
                panel().editBoxes({ reference: "" })
                find("John 3:16-20")
                return 1800
            },
            () => {
                check("(slides made)", slides().length >= 2, slides().length)
                const verses = slides().map(s => box(s, "Verse"))
                check("the copies of the verse box keep the theme's Single Line", verses.every(v => v && v.textTransform === 1),
                      verses.map(v => v ? v.textTransform : "none").join(" "))
                const shown = verses.map(v => testInput.plain(v.displayText))
                check("With Verse in a Single Line box: each slide's reference runs on after its words, on the same line (Verse References)",
                      shown.every((text, i) => !text.includes("\n") && text.endsWith(" " + slides()[i].label + " MUV")), shown.map(s => s.slice(-24)).join(" | "))
                const over = verses.filter(v => testInput.laidOutHeight(v.displayText, v.width - v.marginLeft - v.marginRight) > v.height - v.marginTop - v.marginBottom + 0.5)
                check("and every slide's words fit its box as they are drawn, joined", over.length === 0, over.length)
                const runs = testInput.runsOf(verses[0].displayText)
                check("each run keeps its own look: only a verse's number is raised, and the words are at the theme's size",
                      runs.length > 1 && runs[0].superscript === 1 && /^\d+/.test(runs[0].text)
                      && runs.filter(r => r.superscript !== 0).every(r => /^\d+\s?$/.test(r.text))
                      && runs.filter(r => r.superscript === 0).every(r => Math.round(r.size) === Math.round(runs[0].size)),
                      JSON.stringify(runs.map(r => [r.text.slice(0, 8), r.superscript, Math.round(r.size)])))
                testInput.grab("1-theme")
                // The first slide, which begins with a verse's number; and then the same without verse numbers
                kept.numbered = inkLines(0, verses[0])
                panel().edit({ verseNumbers: false })
                return 1500
            },
            () => {
                const a = kept.numbered
                const b = inkLines(0, box(slides()[0], "Verse"))
                // (How tall the tallest line's letters are drawn. Drawn raised and small throughout, as it was, the
                // lines were as far apart as ever, Qt keeping a raised font's line height, but their letters were about
                // six tenths as tall: 6 points against 10, here.)
                const tallest = lines => Math.max(0, ...lines.map(l => l[1] - l[0]))
                check("in the grid, a slide that begins with a verse's number has its words drawn as tall as one that does not: the same size",
                      a.length >= 2 && b.length >= 2 && Math.abs(tallest(a) - tallest(b)) <= 1, tallest(a) + " against " + tallest(b) + ": " + JSON.stringify(a) + " / " + JSON.stringify(b))
                // ---- The reference in its own box, and the choice kept for this theme slide
                panel().edit({ verseNumbers: true })
                panel().editBoxes({ reference: panel().boxes.find(b => b.name === "Reference").id })
                return 1500
            },
            () => {
                const s = slides()[0]
                check("the reference in the Reference box, and the words alone in the Verse box", testInput.plain(box(s, "Reference").displayText) === s.label + " MUV"
                      && testInput.plain(box(s, "Verse").displayText).split("\n").length === 1, testInput.plain(box(s, "Reference").displayText))
                panel().edit({ themeSlide: kept.lower.id })
                return 800
            },
            () => {
                kept.lowerChoice = panel().referenceBox
                panel().edit({ themeSlide: kept.main.id })
                return 800
            },
            () => {
                check("the boxes chosen are kept for each theme slide: back at Bible Main, the Reference box is still chosen",
                      panel().boxName(panel().referenceBox) === "Reference" && panel().verseBoxName === "Verse", panel().referenceBox)
                panel().editBoxes({ reference: "" })
                panel().edit({ themeSlide: kept.lower.id })
                return 800
            },
            () => {
                check("and another theme slide's own choice is its own", panel().referenceBox === kept.lowerChoice, panel().referenceBox + " / " + kept.lowerChoice)
                named("showButton").clicked()
                return 400
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
