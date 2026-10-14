import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// What the chord sheet took from the Mac and Windows chord editors on 2026-10-14 (chords.qml to chords4.qml have the
// rest of it): an empty box that is kept taking the chord off. Enter, a click on another place and a click on the bare
// sheet take the chord off, as one step to undo; Esc does not; and where there was no chord nothing is asked.
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
                stopEditing()
                return 600
            },
        ]
        next()
    }

    Component.onCompleted: run()
}
