import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// The order of a slide's elements, changed by dragging a row up or down the editor's list of them: where it lands,
// what shows where it would, what is seen on the slide where elements overlap, what the file holds after, and the
// ways out of it (let go where it was, off to one side, or the element gone while it is held).
QtObject {
    id: t

//COMMON
    readonly property var canvas: editScreen.canvas
    readonly property var editor: editScreen.editor
    readonly property var list: named("elementList")
    // The three rectangles the test adds overlap at this point of the slide; and here only the first two do
    readonly property point all: Qt.point(900, 600)
    readonly property point firstTwo: Qt.point(780, 480)

    function nameOf(element) {
        return element.name !== "" ? element.name : "?"
    }

    // The elements as the file has them, the back one first; and as the list shows them, down from its top
    function drawn() {
        return canvas.elements.map(nameOf).join()
    }

    // (A row is known by what it has, and not by a name: the rows are made again at every step of a change
    // that is dragged in the inspector, and are given nothing they can do without.)
    function rows() {
        return Lib.findAll(list, i => i.renaming !== undefined && i.selected !== undefined && i.modelData !== undefined).sort((a, b) => a.y - b.y)
    }

    // How much of a row's name is drawn light: none of it, while the row is held and fainter for it
    function bright(row) {
        const at = row.mapToItem(null, 44, 4)
        return testInput.windowLighter(at.x, at.y, 120, row.height - 8, 150)
    }

    // What is shown while a row is held is made for the holding, and gone after
    function gone(name) {
        const item = named(name)
        return item === null || !item.visible
    }

    function shown() {
        return rows().map(r => nameOf(r.modelData)).join()
    }

    function rowOf(name) {
        return rows().find(r => r.modelData.name === name)
    }

    // A point of the list, in the window
    function inList(x, y) {
        return list.mapToItem(null, x, y)
    }

    // The colour of the slide in the editor at a point of the slide
    function seen(at) {
        const p = canvas.mapToItem(null, canvas.originX + at.x * canvas.u, canvas.originY + at.y * canvas.u)
        return testInput.windowPixel(p.x, p.y)
    }

    function near(colour, wanted) {
        const part = (c, i) => parseInt(c.slice(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(colour, i) - part(wanted, i)) <= 40)
    }

    // Presses on a row and drags it to a height of the list, without letting go
    function hold(row, x, y) {
        const from = centre(row)
        const to = inList(x, y)
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 10; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
        kept.held = to
    }

    function letGo() {
        testInput.mouse(2, kept.held.x, kept.held.y)
    }

    function add(name, x, y, colour) {
        canvas.addShape("rectangle")
        canvas.setProperties({ name: name, x: x, y: y, width: 500, height: 300, fillOn: true, fillKind: "color", fillColor: colour }, false)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("The Joy"))
                startEditing(currentEntry(), document.slides[1].id)
                return 900
            },
            () => {
                // What ProPresenter's example slide has on it, by name ("?" for one with none), as the file has
                // them and as the list does, to be found again around the three that are added
                const own = canvas.elements.map(nameOf)
                kept.own = own.length
                kept.others = own.map(name => name + ",").join("")
                kept.tail = own.slice().reverse().map(name => "," + name).join("")
                kept.backName = own[0]
                add("Red", 700, 400, "#ff0000")
                add("Green", 760, 460, "#00ff00")
                add("Blue", 820, 520, "#0000ff")
                canvas.pick("")
                return 600
            },
            () => {
                check("three rectangles are added to ProPresenter's example slide, each drawn over what was there before it",
                      kept.own === 5 && drawn() === kept.others + "Red,Green,Blue", drawn())
                check("the list has the front one first", shown() === "Blue,Green,Red" + kept.tail && rows().length === kept.own + 3, shown())
                check("and where they overlap the front one is what is seen", near(seen(all), "#0000ff") && near(seen(firstTwo), "#00ff00"), seen(all) + " " + seen(firstTwo))
                kept.hash = testInput.fileHash(document.path)
                testInput.snapshot(document.path, "order-before")
                kept.blue = rowOf("Blue").modelData.id
                // The front one is dragged down to under the third row
                hold(rowOf("Blue"), list.width / 2, 3 * 32 - 5)
                return 400
            },
            () => {
                const line = named("elementLanding")
                const label = named("elementCarried")
                const at = line.mapToItem(null, 6, 1)
                check("a row dragged is held, and would land in the gap between rows that the pointer is nearest", editScreen.carriedId === kept.blue && editScreen.landing === 3,
                      editScreen.carriedId + " at " + editScreen.landing)
                check("a line in that gap says so, drawn in the orange of what is being done", line.visible && Math.abs(line.mapToItem(list, 0, 0).y - (3 * 32 - 1)) < 1.5
                      && near(testInput.windowPixel(at.x, at.y), "#ff8a1f"), line.mapToItem(list, 0, 0).y + " " + testInput.windowPixel(at.x, at.y))
                const said = Lib.findAll(label, i => i.text !== undefined).map(i => i.text).join()
                const held = named("elementHeld")
                check("its name goes with the pointer", label.visible && said === "Blue" && Math.abs(label.mapToItem(null, 0, 0).y - (kept.held.y + 8)) < 2, said)
                check("and its row is fainter where it was, the others as bright as they were", held !== null && held.visible
                      && Math.abs(held.mapToItem(null, 0, 0).y - rowOf("Blue").mapToItem(null, 0, 0).y) < 1 && bright(rowOf("Blue")) === 0 && bright(rowOf("Green")) > 0.01,
                      bright(rowOf("Blue")) + " and " + bright(rowOf("Green")))
                check("the line along the bottom says what letting go does", /Let go between two rows/.test(named("editorHintText").text), named("editorHintText").text)
                check("nothing is moved until it is let go", drawn() === kept.others + "Red,Green,Blue" && testInput.fileHash(document.path) === kept.hash)
                testInput.grab("1-held")
                letGo()
                return 500
            },
            () => {
                check("let go, it is under the two it was over: in the list, and in the order they are drawn in", shown() === "Green,Red,Blue" + kept.tail
                      && drawn() === kept.others + "Blue,Red,Green", shown() + " / " + drawn())
                check("and what is seen where they overlap is the one that is in front now", near(seen(all), "#00ff00") && near(seen(firstTwo), "#00ff00"), seen(all) + " " + seen(firstTwo))
                check("nothing is held any more: no line, no name, the row as bright as the others", editScreen.carriedId === "" && editScreen.landing === -1
                      && gone("elementLanding") && gone("elementCarried") && gone("elementHeld") && bright(rowOf("Blue")) > 0.01, bright(rowOf("Blue")))
                check("the one that was dragged is the one picked, and the change can be undone", canvas.selectedId === kept.blue && editor.canUndo
                      && testInput.fileHash(document.path) !== kept.hash)
                testInput.snapshot(document.path, "order-moved")
                kept.moved = testInput.fileHash(document.path)
                testInput.grab("2-moved")
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 500
            },
            () => {
                check("Undo puts it back in front, in the file as it was to the byte", drawn() === kept.others + "Red,Green,Blue" && near(seen(all), "#0000ff")
                      && testInput.fileHash(document.path) === kept.hash, drawn() + " " + seen(all))
                testInput.key(Qt.Key_Z, Qt.ControlModifier | Qt.ShiftModifier)
                return 500
            },
            () => {
                check("and Redo moves it again", drawn() === kept.others + "Blue,Red,Green" && testInput.fileHash(document.path) === kept.moved, drawn())
                // Let go just under itself: the front one, Green, held over its own lower half
                hold(rowOf("Green"), list.width / 2, 28)
                return 300
            },
            () => {
                check("a row held just under where it is would land there", editScreen.carriedId === rowOf("Green").modelData.id && editScreen.landing === 1 && !gone("elementLanding"),
                      editScreen.landing)
                letGo()
                return 400
            },
            () => {
                check("which is where it is: nothing is changed, and nothing written", drawn() === kept.others + "Blue,Red,Green" && testInput.fileHash(document.path) === kept.moved
                      && editScreen.notice === "", drawn() + " " + editScreen.notice)
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 500
            },
            () => {
                check("and no step to undo was left by it: Undo takes back the move before", drawn() === kept.others + "Red,Green,Blue" && testInput.fileHash(document.path) === kept.hash, drawn())
                // Off to one side: over the slide
                hold(rowOf("Red"), list.width + 60, 20)
                return 300
            },
            () => {
                check("held off to one side of the list it would land nowhere: its name is with the pointer still, and there is no line",
                      editScreen.carriedId === rowOf("Red").modelData.id && editScreen.landing === -1 && gone("elementLanding") && !gone("elementCarried"))
                letGo()
                return 400
            },
            () => {
                check("and let go there it stays where it was", drawn() === kept.others + "Red,Green,Blue" && testInput.fileHash(document.path) === kept.hash
                      && editScreen.carriedId === "", drawn())
                // The back one of all, one of ProPresenter's own, to the front: the last row to over the first
                kept.ids = canvas.elements.map(e => e.id)
                kept.back = kept.ids[0]
                list.positionViewAtEnd()
                return 300
            },
            () => {
                const last = rows()[rows().length - 1]
                check("the last row of the list is the element drawn first", last.modelData.id === kept.back && list.model[list.count - 1].id === kept.back)
                hold(last, list.width / 2, -14)
                return 900
            },
            () => {
                check("a row held above the list would land over the first row", editScreen.landing === 0, editScreen.landing)
                letGo()
                return 500
            },
            () => {
                const n = canvas.elements.length
                check("the back one let go over the first row is the front one, over the three rectangles", canvas.elements[n - 1].id === kept.back
                      && rows()[0].modelData.id === kept.back && drawn().endsWith("Red,Green,Blue," + kept.backName), drawn())
                check("and every other element is in the order it was", canvas.elements.slice(0, n - 1).map(e => e.id).join() === kept.ids.slice(1).join())
                list.positionViewAtBeginning()
                return 300
            },
            () => {
                // And back again: the first row to under the last
                hold(rows()[0], list.width / 2, list.height + 12)
                return 900
            },
            () => {
                check("a row held below the list would land under the last row, the list having gone on to it if it was out of sight",
                      editScreen.landing === list.count && list.contentY >= Math.max(0, list.contentHeight - list.height) - 1, editScreen.landing + " of " + list.count)
                letGo()
                return 500
            },
            () => {
                check("let go there it is the back one again, and the file is what it was before anything was dragged, to the byte",
                      canvas.elements[0].id === kept.back && drawn() === kept.others + "Red,Green,Blue" && testInput.fileHash(document.path) === kept.hash, drawn())
                list.positionViewAtBeginning()
                // A row is still a row: a click picks, a right click has the menu, a double click renames
                click(centre(rowOf("Green")))
                return 300
            },
            () => {
                check("a click on a row still picks its element, and holds nothing", canvas.selected !== null && canvas.selected.name === "Green" && editScreen.carriedId === "")
                click(centre(rowOf("Red")), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click still opens its menu, which has the order a step at a time", menu.opened && /Bring to Front, Bring Forward/.test(labels())
                      && canvas.selected.name === "Red", labels())
                click(menuRow("Bring Forward"))
                return 500
            },
            () => {
                check("Bring Forward moves it one place, as dragging it over the row above would", drawn() === kept.others + "Green,Red,Blue", drawn())
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 400
            },
            () => {
                // Held, and then deleted from the keyboard
                hold(rowOf("Blue"), list.width / 2, 2 * 32 + 4)
                return 300
            },
            () => {
                check("held again", editScreen.carriedId === kept.blue && editScreen.landing === 2)
                testInput.key(Qt.Key_Delete)
                return 400
            },
            () => {
                check("an element deleted while its row is held is gone, and nothing is held", canvas.elements.length === kept.own + 2 && editScreen.carriedId === ""
                      && gone("elementCarried") && gone("elementLanding"), canvas.elements.length + " " + editScreen.carriedId)
                letGo()
                return 400
            },
            () => {
                check("and letting go after that moves nothing", drawn() === kept.others + "Red,Green" && editScreen.notice === "", drawn() + " " + editScreen.notice)
                testInput.key(Qt.Key_Z, Qt.ControlModifier)
                return 400
            },
            () => {
                check("(Undo brings it back)", drawn() === kept.others + "Red,Green,Blue" && testInput.fileHash(document.path) === kept.hash, drawn())
                // More elements than the list has room for: a row held at the bottom edge scrolls the list
                for (let i = 0; i < 14; ++i)
                    canvas.addShape("ellipse")
                canvas.pick("")
                return 700
            },
            () => {
                list.positionViewAtBeginning()
                kept.count = canvas.elements.length
                kept.front = rows()[0].modelData.id
                check("with more rows than fit, the list scrolls", list.contentHeight > list.height + 60 && list.contentY === 0, list.contentHeight + " in " + list.height)
                hold(rows()[0], list.width / 2, list.height - 4)
                return 500
            },
            () => {
                kept.scrolled = list.contentY
                check("a row held at the bottom edge of the list sets the list going on under it", list.contentY > 20 && editScreen.carriedScroll > 0
                      && editScreen.landing === Math.round((list.contentY + list.height - 4) / 32), list.contentY + " " + editScreen.landing)
                return 2600
            },
            () => {
                const furthest = list.contentHeight - list.height
                check("until there is no further to go, where it stops, the landing being the gap under the pointer all the way", list.contentY > kept.scrolled
                      && Math.abs(list.contentY - furthest) < 1 && editScreen.carriedScroll === 0 && editScreen.landing === Math.round((list.contentY + list.height - 4) / 32),
                      list.contentY + " of " + furthest + ", landing " + editScreen.landing)
                kept.landing = editScreen.landing
                testInput.grab("3-scrolled")
                letGo()
                return 500
            },
            () => {
                // The front one, let go at that landing: it has that many rows over it, less the one it was itself
                const index = canvas.elements.findIndex(e => e.id === kept.front)
                check("let go there, the element is where the line was, counted in the rows that had scrolled out of sight as well",
                      index === kept.count - 1 - (kept.landing - 1) && list.model[kept.landing - 1].id === kept.front && kept.landing > 9, index + " of " + kept.count + ", landing " + kept.landing)
                const lines = testInput.readText(Log.path).split("\n")
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("not an image")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l)).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
