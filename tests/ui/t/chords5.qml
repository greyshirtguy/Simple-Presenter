import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// What the chord sheet took from the Mac and Windows chord editors on 2026-10-14 (chords.qml to chords4.qml have the
// rest of it): an empty box that is kept taking the chord off, and the highlighted line; and the setting for the
// second. The empty box: Enter, a click on another place and a click on the bare sheet take the chord off, as one
// step to undo; Esc does not; and where there was no chord nothing is asked. The highlighted line: the words of the
// line the pointer is on are drawn in a colour of their own, and the rule of a line of chords alone in it and twice
// as thick; over a chord's bubble it is still that line; over the bare sheet between the slides, over the strip, it
// is none; the keys moving the spot make the spot's line it; the line a chord is being typed on is it wherever the
// pointer goes; a chord carried from the strip makes it the line it would land on, and none between slides. What is
// looked at is what is drawn, wherever the thing is one that is drawn.
QtObject {
    id: t

//COMMON
    function sheet() {
        return named("chordSheet")
    }

    function view() {
        return named("chordSheetView")
    }

    function lineItem(index) {
        return Lib.find(sheet(), item => item.isLine === true && item.index === index)
    }

    function lineOf(words) {
        return sheet().rows.findIndex(row => row.kind === "line" && row.text === words)
    }

    // Where a character of a line is on the window, in the words
    function over(index, at) {
        const item = lineItem(index)
        return item.mapToItem(null, item.xOf(at) + 3, item.wordsTop + 6)
    }

    // How much of a line's words is drawn red: they are, highlighted, once the highlighted line is given that colour
    function redOf(index) {
        const item = lineItem(index)
        const at = item.mapToItem(null, sheet().gutter, item.wordsTop)
        return testInput.windowRed(at.x, at.y, Math.min(160, item.width - 2 * sheet().gutter), 18)
    }

    // The rule that stands where a line of chords alone would have words
    function ruleOf(index) {
        return Lib.find(lineItem(index), item => item.visible && item.height <= 4 && item.width > 40 && item.border !== undefined)
    }

    function names(index) {
        return sheet().rows[index].chords.map(c => c.at + ":" + c.name).join(" ")
    }

    function cellOf(row) {
        return sheet().cells.find(cell => cell.row === row)
    }

    // A point of the sheet's page (where its slides are laid out), on the window
    function onWindow(x, y) {
        return view().contentItem.mapToItem(null, x, y)
    }

    // The chords the file has on the text box that has these words
    function chordsOnDisk(words) {
        for (const slide of catalog.open(kept.path).slides) {
            for (const element of slide.elements) {
                if (testInput.plain(element.text).split("\n").includes(words))
                    return (element.chords ?? []).map(c => c.at + ":" + c.name).join(" ")
            }
        }
        return "(no such words)"
    }

    // What a chord's bubble draws, and the box a chord is typed in
    function drawnAt(index, at) {
        const bubble = named("chord:" + index + ":" + at)
        const text = bubble ? Lib.find(bubble, item => item.text !== undefined && item.font !== undefined) : null
        return text ? text.text : "(no bubble)"
    }

    function field() {
        return named("chordField")
    }

    function button(number) {
        return named("keyChord" + number)
    }

    // A click as a hand makes one: the pointer moved there first, so that what it was over knows it has left
    function clickThere(item) {
        const p = centre(item)
        testInput.mouse(1, p.x - 2, p.y)
        testInput.mouse(1, p.x, p.y)
        click(p)
    }

    function sameSong() {
        return JSON.stringify(editScreen.editor.song()) === kept.song && testInput.fileHash(kept.path) === kept.hash
    }

    function run() {
        const herald = "Hark the herald angels sing"
        const glory = "Glory to the newborn King"
        const peace = "Peace on earth and mercy mild"
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                startEditing(entry("Hark2"))
                kept.path = entry("Hark2").path
                return 700
            },
            () => {
                click(centre(named("chordsMode")))
                return 900
            },
            () => {
                const s = sheet()
                kept.h = lineOf(herald)
                kept.g = lineOf(glory)
                kept.p = lineOf(peace)
                kept.song = JSON.stringify(editScreen.editor.song())
                kept.hash = testInput.fileHash(kept.path)
                check("the song is on the sheet", kept.h >= 0 && kept.g >= 0 && kept.p >= 0 && s.inKey.length === 7, kept.h + " " + kept.g + " " + kept.p)
                // The line's first chord, a word of it that has none ("herald"), and the bare sheet between two slides
                kept.chordAt = s.rows[kept.h].chords[0].at
                kept.chord = s.chordAt(kept.h, kept.chordAt)
                kept.place = 9
                check("(a line with a chord, and a word on it that has none)", kept.chord !== "" && s.chordAt(kept.h, kept.place) === "", names(kept.h))
                const cells = s.cells
                kept.between = onWindow(cells[0].x + cells[0].width + 6, cells[0].y + cells[0].height / 2)
                // ---- An empty box that is kept takes the chord off: by Enter
                click(over(kept.h, kept.chordAt))
                return 300
            },
            () => {
                check("(the box is open on the chord, all of it picked)", sheet().typing && field().text !== "" && field().selectedText === field().text, field().text)
                testInput.key(Qt.Key_Delete)
                return 200
            },
            () => {
                check("(and emptied)", sheet().typing && field().text === "")
                testInput.key(Qt.Key_Return)
                return 400
            },
            () => {
                check("an empty box kept by Enter takes off the chord that was there", !sheet().typing && sheet().chordAt(kept.h, kept.chordAt) === ""
                      && chordsOnDisk(herald).indexOf((sheet().rows[kept.h].start + kept.chordAt) + ":" + kept.chord) < 0, names(kept.h))
                check("as one step to undo", editScreen.editor.undo() === "" && sameSong())
                return 300
            },
            () => {
                // By a click on another place
                click(over(kept.h, kept.chordAt))
                return 300
            },
            () => {
                testInput.key(Qt.Key_Delete)
                return 200
            },
            () => {
                click(over(kept.g, 6))
                return 400
            },
            () => {
                check("and by a click on another place, the box opening there", sheet().typing && sheet().typingRow === kept.g && sheet().chordAt(kept.h, kept.chordAt) === "",
                      names(kept.h))
                testInput.key(Qt.Key_Escape)
                check("(undone)", editScreen.editor.undo() === "" && sameSong())
                return 300
            },
            () => {
                // By a click on the bare sheet
                click(over(kept.h, kept.chordAt))
                return 300
            },
            () => {
                testInput.key(Qt.Key_Delete)
                return 200
            },
            () => {
                click(kept.between)
                return 400
            },
            () => {
                check("and by a click on the bare sheet", !sheet().typing && sheet().chordAt(kept.h, kept.chordAt) === "", names(kept.h))
                check("(undone)", editScreen.editor.undo() === "" && sameSong())
                // Esc leaves it as it was
                click(over(kept.h, kept.chordAt))
                return 300
            },
            () => {
                testInput.key(Qt.Key_Delete)
                return 200
            },
            () => {
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc, with the box emptied, leaves the chord as it was and asks nothing of the song", !sheet().typing && sheet().chordAt(kept.h, kept.chordAt) === kept.chord
                      && sameSong(), names(kept.h))
                // Where there is no chord, an empty box kept asks nothing
                click(over(kept.h, kept.place))
                return 300
            },
            () => {
                check("(a box opened where there is no chord, empty)", sheet().typing && field().text === "" && sheet().chordAt(kept.h, kept.place) === "")
                testInput.key(Qt.Key_Return)
                return 300
            },
            () => {
                check("kept empty where there was no chord, nothing is asked of the song", !sheet().typing && sameSong())
                // ---- The highlighted line
                const s = sheet()
                check("as it comes, a chord's name is the sheet's orange, and so is the highlighted line",
                      String(s.chordColor) === "#ff8a1f" && String(s.lineColor) === "#ff8a1f" && chordLineColor === "", s.chordColor + " " + s.lineColor)
                // Off the sheet, to the strip over it: nobody is at the sheet. (The pointer is moved over the sheet first,
                // as a hand's always is before it clicks there: the clicks of this test are a press and a release where the
                // pointer is put, and the sheet is told the pointer is over it only by a move.)
                const q = over(kept.h, kept.place)
                testInput.mouse(1, q.x + 2, q.y)
                const b = centre(button(1))
                testInput.mouse(1, b.x, b.y - 4)
                testInput.mouse(1, b.x, b.y)
                return 300
            },
            () => {
                check("with nobody at the sheet, no line is highlighted", !sheet().attending && sheet().highlighted === -1, sheet().highlighted)
                // In a colour a picture can tell
                chordLineColor = "#ff0000"
                const p = over(kept.h, 9)
                testInput.mouse(1, p.x - 4, p.y)
                testInput.mouse(1, p.x, p.y)
                return 400
            },
            () => {
                const s = sheet()
                check("the pointer on a word makes its line the highlighted one", s.attending && s.highlighted === kept.h && s.spotRow === kept.h, s.highlighted)
                kept.lit = redOf(kept.h)
                kept.other = redOf(kept.g)
                check("and its words are drawn in the highlighted line's colour, and no other line's", kept.lit > 0.03 && kept.other < 0.005, kept.lit + " against " + kept.other)
                testInput.grab("1-highlighted-line")
                // Onto a chord's bubble of the same line
                const b = centre(named("chord:" + kept.h + ":" + kept.chordAt))
                testInput.mouse(1, b.x, b.y)
                return 300
            },
            () => {
                check("on a chord's bubble it is still that line", sheet().attending && sheet().highlighted === kept.h && redOf(kept.h) > 0.03, sheet().highlighted)
                // To the bare sheet between two slides
                testInput.mouse(1, kept.between.x, kept.between.y)
                return 300
            },
            () => {
                check("over the bare sheet between the slides, nobody is at the sheet and no line is highlighted", !sheet().attending && sheet().highlighted === -1
                      && redOf(kept.h) < 0.005, sheet().highlighted + " " + redOf(kept.h))
                // The keys move the spot: the person is at the sheet again
                testInput.key(Qt.Key_Right)
                return 300
            },
            () => {
                const s = sheet()
                check("a key that moves the spot makes its line the highlighted one, the pointer where it was", s.attending && s.highlighted === s.spotRow && s.spotRow >= 0
                      && redOf(s.spotRow) > 0.03, s.highlighted + " " + s.spotRow)
                // Back onto a line, and then out of the sheet, onto the strip over it
                const p = over(kept.g, 6)
                testInput.mouse(1, p.x, p.y)
                const b = centre(button(1))
                testInput.mouse(1, b.x, b.y - 4)
                testInput.mouse(1, b.x, b.y)
                return 300
            },
            () => {
                check("the pointer gone from the sheet to the strip over it, no line is highlighted", !sheet().attending && sheet().highlighted === -1
                      && redOf(kept.g) < 0.005, sheet().highlighted + " " + redOf(kept.g))
                // Typing a chord: its line is highlighted wherever the pointer goes
                click(over(kept.g, 6))
                return 300
            },
            () => {
                const p = over(kept.p, 6)
                testInput.mouse(1, p.x, p.y)
                return 300
            },
            () => {
                const s = sheet()
                check("while a chord is typed, its line is the highlighted one, though the pointer has gone on to another", s.typing && s.typingRow === kept.g
                      && s.spotRow === kept.p && s.highlighted === kept.g && redOf(kept.g) > 0.03 && redOf(kept.p) < 0.005, s.highlighted + " " + s.spotRow)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("(Esc: nothing changed)", !sheet().typing && sameSong())
                // A chord carried in from the strip: the line it would land on
                const from = centre(button(4))
                const to = over(kept.p, 6)
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 10; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
                return 300
            },
            () => {
                check("a chord carried from the strip over a word makes that line the highlighted one", sheet().carrying && sheet().carriedOver
                      && sheet().highlighted === kept.p && redOf(kept.p) > 0.03, sheet().highlighted)
                testInput.mouse(1, kept.between.x, kept.between.y)
                return 300
            },
            () => {
                check("and between two slides, where it would land nowhere, none", sheet().carrying && !sheet().carriedOver && sheet().highlighted === -1, sheet().highlighted)
                testInput.key(Qt.Key_Escape)
                testInput.mouse(2, kept.between.x, kept.between.y)
                return 300
            },
            () => {
                check("(given up: nothing changed)", sheet().dragged === null && sameSong())
                // A line of chords alone: its rule takes the colour, twice as thick
                kept.alone = sheet().rows.findIndex(row => row.kind === "line" && row.alone && !row.blank && row.chords.length > 0)
                const rule = ruleOf(kept.alone)
                kept.thin = rule ? rule.height : -1
                check("(the song has a line of chords alone, with a thin rule where its words would be)", kept.alone >= 0 && kept.thin === 1, kept.alone + " " + kept.thin)
                const cell = cellOf(sheet().rows[kept.alone].row)
                view().contentY = Math.max(0, cell.y - 40)
                return 300
            },
            () => {
                const item = lineItem(kept.alone)
                const p = item.mapToItem(null, item.width - 30, item.wordsTop + 6)
                testInput.mouse(1, p.x - 3, p.y)
                testInput.mouse(1, p.x, p.y)
                return 300
            },
            () => {
                const rule = ruleOf(kept.alone)
                const at = rule ? rule.mapToItem(null, 0, 0) : { x: 0, y: 0 }
                const red = rule ? testInput.windowRed(at.x + 4, at.y, Math.min(100, rule.width - 8), rule.height) : 0
                check("highlighted, a line of chords alone has its rule in the colour, twice as thick", sheet().highlighted === kept.alone && rule !== null
                      && rule.height === 2 * kept.thin && red > 0.5, sheet().highlighted + " " + (rule ? rule.height : "-") + " " + red)
                testInput.grab("2-highlighted-chords-alone")
                chordLineColor = ""
                view().contentY = 0
                return 300
            },
            () => {
                check("the highlighted line's colour put back is the sheet's orange", String(sheet().lineColor) === "#ff8a1f")
                // ---- The settings: the highlighted line's colour
                stopEditing()
                return 600
            },
            () => {
                settingsOpen = true
                kept.settings = Lib.find(win.contentItem, item => item.chordSheetColorEdited !== undefined)
                kept.settings.section = "chords"
                return 500
            },
            () => {
                const swatch = named("chordColour:The highlighted line")
                const usual = named("usualColour:The highlighted line")
                check("the settings for chords have the highlighted line's colour, the sheet's orange as it comes, with nothing to go back to", swatch !== null && swatch.visible
                      && String(swatch.value) === "#ff8a1f" && !usual.enabled && String(named("chordColour:On the chord sheet").value) === "#ff8a1f",
                      swatch ? String(swatch.value) : "none")
                swatch.picked("#ff0000")
                return 300
            },
            () => {
                check("a colour picked for the line is kept as the app's own, and the sheet is given it", chordLineColor === "#ff0000" && named("usualColour:The highlighted line").enabled,
                      chordLineColor)
                const sample = named("chordSample:The highlighted line")
                const at = sample.mapToItem(null, 0, sample.height / 2)
                const red = testInput.windowRed(at.x, at.y, sample.width, sample.height / 2)
                check("and its sample's second line is drawn in it", red > 0.02, red)
                testInput.grab("5-settings")
                click(centre(named("usualColour:The highlighted line")))
                return 300
            },
            () => {
                check("Usual Colour puts the line's colour back", chordLineColor === "" && !named("usualColour:The highlighted line").enabled, chordLineColor)
                // ---- Done
                kept.settings.section = "groups"
                settingsOpen = false
                return 300
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
