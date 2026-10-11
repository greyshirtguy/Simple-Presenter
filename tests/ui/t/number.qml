import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// A slide asked for by its number, typed (src/slidenumber.h): the digits put a panel up over the slides and take nothing
// live; Enter takes the slide with that number live, as a click on it does; a second without a key and the panel goes by
// itself; and the keys that give a number up or mend it. Then a digit that is a group's hotkey, which stays one. What
// is looked at is what is drawn: the panel and how solid it is, the ring round the live slide in the grid, and the
// output's own picture, set against what a click on the same slide draws.
QtObject {
    id: t

//COMMON
    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function shown(item) {
        for (let at = item; at; at = at.parent) {
            if (!at.visible)
                return false
        }
        return item !== null
    }

    function digit(d) {
        testInput.key(Qt.Key_0 + Number(d), 0, String(d))
    }

    // What the output draws, as how much of each of twelve parts of it is lit: the same slide gives the same, and
    // slides with other words on them something else.
    function onOutput() {
        const parts = []
        for (let row = 0; row < 3; ++row) {
            for (let column = 0; column < 4; ++column)
                parts.push(testInput.lit(false, column / 4, row / 3, 1 / 4, 1 / 3).toFixed(3))
        }
        return parts.join(" ")
    }

    // The colour where the top of the ring is that the grid draws round the slide that is live
    function ringAt(index) {
        const cell = slideCell(index)
        const at = cell.mapToItem(null, cell.width / 2, 2.5)
        return String(testInput.windowPixel(at.x, at.y))
    }

    // The panel as it is drawn: whether it is there, the digits in it, and the lightest thing along the middle of the
    // digits, which is a digit's own white as the panel lets it through.
    function panel() {
        const sheet = named("typedNumber")
        if (sheet === null)
            return { up: false, digits: "", white: 0, solid: 0, said: "never made" }
        const digits = named("typedNumberDigits")
        const from = digits.mapToItem(null, 0, digits.height / 2)
        let white = 0
        for (let x = 0; x < digits.width && shown(sheet); x += 2) {
            const c = String(testInput.windowPixel(from.x + x, from.y))
            white = Math.max(white, parseInt(c.substring(1, 3), 16))
        }
        return { up: shown(sheet), digits: digits.text, white: white, solid: sheet.parent.opacity, sheet: sheet,
                 said: (shown(sheet) ? "up" : "not up") + ", \"" + digits.text + "\", white drawn as " + white + ", " + sheet.parent.opacity + " solid" }
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                return 600
            },
            () => {
                check("(the presentation has thirty slides to ask for)", document.slides.length === 30, document.slides.length)
                // ---- What a click draws, to set a number against
                click(centre(slideCell(11)))
                return 500
            },
            () => {
                kept.twelfth = onOutput()
                check("(a click takes the twelfth slide live, ringed in the grid)", liveIndex === 11 && ringAt(11) === "#ff8a1f", liveIndex + " " + ringAt(11))
                click(centre(slideCell(2)))
                return 500
            },
            () => {
                kept.third = onOutput()
                check("(and the third, which the output draws otherwise)", liveIndex === 2 && kept.third !== kept.twelfth && ringAt(2) === "#ff8a1f" && ringAt(11) !== "#ff8a1f",
                      kept.third + " | " + kept.twelfth)
                check("no number is up, and nothing has been made to show one", typedNumber === "" && named("typedNumber") === null)
                // ---- A number typed
                digit(1)
                return 300
            },
            () => {
                const drawn = panel()
                check("a digit typed takes nothing live: the third slide is still the one, on the output and in the grid", liveIndex === 2 && onOutput() === kept.third
                      && ringAt(2) === "#ff8a1f", liveIndex)
                check("it puts a panel up over the slides with the digit in it, drawn", typedNumber === "1" && drawn.up && drawn.digits === "1" && drawn.white > 150, drawn.said)
                const grid = named("slideGrid")
                const middle = centre(drawn.sheet)
                const inGrid = grid.mapFromItem(null, middle.x, middle.y)
                check("in the middle of the slides", Math.abs(inGrid.x - grid.width / 2) < 40 && Math.abs(inGrid.y - grid.height / 2) < 60, inGrid.x.toFixed(0) + "," + inGrid.y.toFixed(0)
                      + " of " + grid.width.toFixed(0) + "x" + grid.height.toFixed(0))
                // (A digit is white. Wholly solid it would be drawn as 255; three-quarters solid, over the panel's dark, as
                // 194 to 210, by what is behind.)
                check("about three-quarters solid: by what it says of itself, and by how white its digit is drawn", drawn.solid === 0.75 && drawn.white >= 186 && drawn.white <= 216,
                      drawn.solid + ", white drawn as " + drawn.white)
                testInput.grab("1-one-digit")
                digit(2)
                return 300
            },
            () => {
                const drawn = panel()
                check("a second digit goes on with the number, and still nothing is taken live", typedNumber === "12" && drawn.up && drawn.digits === "12" && liveIndex === 2
                      && onOutput() === kept.third, drawn.said)
                testInput.grab("2-two-digits")
                testInput.key(Qt.Key_Return)
                check("Enter takes the slide with that number live, at once", liveIndex === 11 && typedNumber === "", liveIndex)
                check("the panel does not vanish with it: it fades, with its digits still in it", panel().up && panel().digits === "12")
                return 500
            },
            () => {
                check("the output draws what a click on that slide drew", onOutput() === kept.twelfth, onOutput() + " | " + kept.twelfth)
                check("the grid has its ring round it, and round the third no more", ringAt(11) === "#ff8a1f" && ringAt(2) !== "#ff8a1f", ringAt(11) + " " + ringAt(2))
                check("and the panel has gone", !panel().up && named("typedNumber").parent.opacity === 0)
                check("the log has a line for it", logged(/  key +slide 12 of "The Joy" asked for by its number$/).length === 1)
                // ---- Left alone
                digit(3)
                return 600
            },
            () => {
                check("a number typed and left is still up after half a second", typedNumber === "3" && panel().up && panel().digits === "3")
                return 800
            },
            () => {
                check("and gone by itself after a second without a key, with nothing taken live", typedNumber === "" && !panel().up && liveIndex === 11 && onOutput() === kept.twelfth,
                      typedNumber + " " + liveIndex)
                // Each digit begins the second again.
                digit(2)
                return 700
            },
            () => {
                digit(4)
                return 700
            },
            () => {
                check("each digit begins the second again: 1.4 seconds after the first, and 0.7 after the second, the number is up", typedNumber === "24" && panel().up, typedNumber)
                return 700
            },
            () => {
                check("and it has gone when a second has passed since the last", typedNumber === "" && !panel().up && liveIndex === 11, typedNumber)
                // ---- A number no slide has
                digit(9)
                digit(9)
                testInput.key(Qt.Key_Enter)
                check("a number no slide has does nothing when Enter is pressed (the Enter of the number pad, here)", typedNumber === "" && liveIndex === 11)
                check("which the log says", logged(/  key +slide 99 asked for by its number: "The Joy" has 30$/).length === 1)
                digit(0)
                check("a nought typed first is shown", typedNumber === "0")
                testInput.key(Qt.Key_Return)
                check("and is no slide's number", liveIndex === 11 && typedNumber === "")
                // ---- Mending a number, and giving it up
                digit(0)
                digit(6)
                check("a nought that leads is dropped for the digit after it", typedNumber === "6")
                digit(5)
                testInput.key(Qt.Key_Backspace)
                check("Backspace takes the last digit off", typedNumber === "6", typedNumber)
                return 300
            },
            () => {
                check("which the panel shows", panel().up && panel().digits === "6")
                testInput.key(Qt.Key_Return)
                check("and Enter takes the number as it then is", liveIndex === 5, liveIndex)
                digit(8)
                testInput.key(Qt.Key_Backspace)
                check("Backspace on the only digit gives the number up", typedNumber === "" && liveIndex === 5)
                for (const d of [1, 2, 3, 4, 5])
                    digit(d)
                check("a number has four digits at most: a fifth is not taken", typedNumber === "1234", typedNumber)
                testInput.key(Qt.Key_Escape)
                check("Esc gives a number up, with nothing taken live", typedNumber === "" && liveIndex === 5)
                // A key held down is one digit.
                testInput.keyDown(Qt.Key_4, false, "4")
                testInput.keyDown(Qt.Key_4, true, "4")
                testInput.keyDown(Qt.Key_4, true, "4")
                testInput.keyUp(Qt.Key_4, false, "4")
                check("a digit held down is one digit, and not a row of them", typedNumber === "4", typedNumber)
                testInput.key(Qt.Key_Shift)
                check("a key that only goes with others leaves the number be", typedNumber === "4")
                testInput.key(Qt.Key_Right)
                check("any other key gives the number up and does what it does: Right goes to the next slide", typedNumber === "" && liveIndex === 6, typedNumber + " " + liveIndex)
                digit(2)
                testInput.key(Qt.Key_1, Qt.ControlModifier, "1")
                check("Ctrl with a digit is still the switch for the audience screens, and no digit of the number", typedNumber === "2" && !outputEnabled, typedNumber + " " + outputEnabled)
                testInput.key(Qt.Key_1, Qt.ControlModifier, "1")
                testInput.key(Qt.Key_Escape)
                check("(the screens are on again, and the number given up)", outputEnabled && typedNumber === "")
                // ---- Where the keys are not the show's
                digit(2)
                openSearch()
                return 400
            },
            () => {
                check("a number is given up when the keyboard goes elsewhere: to the search, here", searchOpen && typedNumber === "" && liveIndex === 6, typedNumber)
                searchPanel.closed()
                settingsOpen = true
                digit(5)
                check("with the settings up a digit begins no number", typedNumber === "")
                return 400
            },
            () => {
                // ---- A digit that is a group's hotkey
                const screen = Lib.find(win.contentItem, i => i.keyEdited !== undefined)
                screen.keyEdited(groups.findIndex(g => g.name === "Bridge"), "7")
                settingsOpen = false
                takeFocus()
                kept.bridge = document.slides.findIndex(s => s.groupStart && s.group === "Bridge")
                check("(the bridge has been given 7 for its hotkey)", hotkeys.some(h => h.key === "7") && kept.bridge > 0 && groupKeyAt[kept.bridge] === "7", JSON.stringify(hotkeys))
                digit(7)
                check("a digit that is a group's hotkey stays that: it goes to the group at once, and begins no number", liveIndex === kept.bridge && typedNumber === "",
                      liveIndex + " " + typedNumber)
                digit(1)
                digit(7)
                check("but once a number has been begun it is a digit of it", typedNumber === "17" && liveIndex === kept.bridge, typedNumber)
                testInput.key(Qt.Key_Return)
                check("so slide 17 can be asked for", liveIndex === 16 && liveIndex !== kept.bridge, liveIndex)
                testInput.key(Qt.Key_C, 0, "c")
                check("(and a letter's hotkey is what it was)", liveIndex === document.slides.findIndex(s => s.groupStart && s.group.startsWith("Chorus")), liveIndex)
                return 400
            },
            () => {
                check("at the end no number is up and nothing is drawn of one", typedNumber === "" && !panel().up)
            }
        ]
        next()
    }
}
