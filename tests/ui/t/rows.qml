import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The rows of a playlist selected several at a time, with Ctrl and Shift held, headings among them, with what is open
// staying open; the Delete key taking the selected rows out, and the menu's Remove for all of them; and the search's
// Ctrl+Enter adding under the row selected, the new row selected and lit, and named at the foot of the search; rows
// copied with Ctrl+C and pasted with Ctrl+V, under the row selected, into another playlist at its end, and from a row's
// menu, with Ctrl+V the media bin again once a slide has been clicked. What the playlists file then holds is looked at
// when the test is over, by after_rows in run.py.
QtObject {
    id: t

//COMMON
    // A row of the list of presentations, by its place, brought into view
    function rowAt(index) {
        const list = named("presentationList")
        list.positionViewAtIndex(index, ListView.Contain)
        return list.itemAtIndex(index)
    }

    function clickWith(p, modifiers) {
        testInput.mouse(0, p.x, p.y, modifiers)
        testInput.mouse(2, p.x, p.y, modifiers)
    }

    // Whether a row is drawn as selected: its own say, and its colour
    function drawnSelected(index) {
        const row = rowAt(index)
        return row !== null && row.selected === true && Qt.colorEqual(row.color, "#22ffffff")
    }

    function drawnPlain(index) {
        const row = rowAt(index)
        return row !== null && row.selected === false && !Qt.colorEqual(row.color, "#22ffffff")
    }

    // Whether a row is lit (the orange laid over it, see SidebarList.flash)
    function lit(index) {
        const row = rowAt(index)
        return row !== null && Lib.find(row, item => item.color !== undefined && Qt.colorEqual(item.color, "#ff8a1f") && item.opacity > 0.1) !== null
    }

    function names() {
        return documents.map(d => d.kind === "header" ? "[" + d.name + "]" : d.name)
    }

    function selectedNames() {
        return documents.filter(d => selectedRows.includes(d.path)).map(d => d.name)
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                openPlaylist(catalog.playlists.find(p => p.name === "Sunday Morning").path)
                return 600
            },
            () => {
                // Rows 0 and 3 are headings; 1, 2, 4, 5, 6, 7, 8 presentations; 9 a heading again
                kept.count = documents.length
                kept.names = names()
                check("a playlist of headings and presentations", kept.count >= 14 && documents[0].kind === "header" && documents[3].kind === "header" && documents[9].kind === "header"
                      && [1, 2, 4, 5, 6, 7, 8].every(i => openable(documents[i])), kept.names.join(", "))
                check("opening it opens its first presentation, which is the one row selected, and is drawn so, the others not", documentKey === documents[1].path
                      && selectedRows.length === 1 && selectedRows[0] === documentKey && drawnSelected(1) && drawnPlain(2) && drawnPlain(0), JSON.stringify(selectedRows))
                clickWith(centre(rowAt(4)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl+click on another row selects it as well, and opens nothing", selectedNames().join(", ") === documents[1].name + ", " + documents[4].name
                      && documentKey === documents[1].path && drawnSelected(4) && drawnSelected(1), selectedNames().join(", "))
                clickWith(centre(rowAt(1)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl+click on the row that is open lets it go, and it stays open", selectedNames().join(", ") === documents[4].name && documentKey === documents[1].path
                      && document.name === documents[1].name && drawnPlain(1), selectedNames().join(", "))
                clickWith(centre(rowAt(10)), Qt.ShiftModifier)
                return 300
            },
            () => {
                // (The anchor is the last row a click selected, and failing that the presentation open, as the slides' rule has it: the row
                // let go was the last clicked, so the range runs from the one open.)
                check("Shift+click selects every row from the presentation open to it, the headings between them too, and opens nothing",
                      selectedRows.length === 10 && [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].every(i => selectedRows.includes(documents[i].path)) && documentKey === documents[1].path
                      && drawnSelected(9) && drawnSelected(10) && drawnSelected(1) && drawnPlain(0) && drawnPlain(11), selectedNames().join(", "))
                testInput.grab("1-ten-selected")
                clickWith(centre(rowAt(6)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("(one let go)", selectedRows.length === 9 && !selectedRows.includes(documents[6].path) && drawnPlain(6))
                clickWith(centre(rowAt(8)), Qt.ShiftModifier)
                return 300
            },
            () => {
                check("Shift+click then runs from the row last let go, which was the last clicked, the rest staying selected", selectedRows.length === 10
                      && [1, 2, 3, 4, 5, 6, 7, 8, 9, 10].every(i => selectedRows.includes(documents[i].path)) && drawnSelected(6), selectedNames().join(", "))
                click(centre(rowAt(3)))
                return 300
            },
            () => {
                check("a heading clicked is selected, by itself, and opens nothing", selectedRows.length === 1 && selectedRows[0] === documents[3].path && documentKey === documents[1].path
                      && drawnSelected(3), selectedNames().join(", "))
                // ---- Search adds under the row selected: under the heading
                testInput.shortcut(Qt.Key_F, Qt.ControlModifier)
                return 400
            },
            () => {
                testInput.type("jesus have it all")
                return 400
            },
            () => {
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 500
            },
            () => {
                check("Ctrl+Enter in the search adds the presentation under the heading selected", documents.length === kept.count + 1 && documents[3].kind === "header"
                      && documents[4].name === "Jesus Have It All" && openable(documents[4]) && documents[5].name === kept.names[4], names().slice(2, 7).join(", "))
                check("the new row is the one selected, drawn so and lit; what is open is as it was", selectedRows.length === 1 && selectedRows[0] === documents[4].path
                      && drawnSelected(4) && lit(4) && documentKey === documents[1].path && document.name === documents[1].name, named("presentationList").litPath)
                check("and the foot of the search says so, and where", named("searchHint").text === "✓ Added “Jesus Have It All” to “Sunday Morning”, under “" + documents[3].name + "”.",
                      named("searchHint").text)
                testInput.grab("2-added-under-heading")
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 500
            },
            () => {
                check("added again, it goes under the one just added", documents.length === kept.count + 2 && documents[4].name === "Jesus Have It All" && documents[5].name === "Jesus Have It All"
                      && documents[6].name === kept.names[4] && selectedRows[0] === documents[5].path && lit(5)
                      && named("searchHint").text.includes("under “Jesus Have It All”"), names().slice(2, 8).join(", "))
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc puts the search away, and the keys are the show's", !searchOpen)
                // ---- Delete takes the selected rows out: the two added, one of them opened first
                click(centre(rowAt(4)))
                return 400
            },
            () => {
                check("a click opens a row, and it is then the one selected", documentKey === documents[4].path && document.name === "Jesus Have It All" && selectedRows.length === 1
                      && selectedRows[0] === documents[4].path)
                clickWith(centre(rowAt(5)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("(two selected)", selectedRows.length === 2 && selectedRows.includes(documents[5].path))
                click(centre(rowAt(5)), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on one of them offers to remove them all", menu.opened && labels().includes("Remove 2 Rows from Playlist"), labels())
                menu.close()
                return 300
            },
            () => {
                notice = ""
                testInput.key(Qt.Key_Delete)
                return 600
            },
            () => {
                check("Delete takes the selected rows out of the playlist, and the rows are as they were before them", documents.length === kept.count && names().join("|") === kept.names.join("|"),
                      names().slice(2, 7).join(", "))
                check("the row that took their place is opened, that being the one open, and is the one selected", documentKey === documents[4].path && document.name === kept.names[4]
                      && selectedRows.length === 1 && selectedRows[0] === documents[4].path && drawnSelected(4), document ? document.name : "nothing")
                check("and the operator is told", notice === "2 rows removed from “Sunday Morning”." && !noticeIsError, notice)
                testInput.grab("3-after-delete")
                // ---- Under a presentation: added under it, and left there for after_rows to see
                click(centre(rowAt(7)))
                return 400
            },
            () => {
                check("(a presentation opened and selected)", documentKey === documents[7].path && selectedRows[0] === documents[7].path)
                testInput.shortcut(Qt.Key_F, Qt.ControlModifier)
                return 400
            },
            () => {
                testInput.type("move of god")
                return 400
            },
            () => {
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 500
            },
            () => {
                check("added under the presentation open, which stays open", documents.length === kept.count + 1 && documents[8].name === "Move Of God" && documents[7].path === documentKey
                      && documents[9].name === kept.names[8] && selectedRows[0] === documents[8].path, names().slice(6, 11).join(", "))
                kept.added = documents[8].path
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                // ---- A heading deleted
                const heading = documents.findIndex(d => d.kind === "header" && d.name.includes("10:30am Service"))
                check("(the second service's heading)", heading > 9, heading)
                kept.heading = documents[heading].name
                kept.headingAt = heading
                click(centre(rowAt(heading)))
                return 300
            },
            () => {
                check("(selected)", selectedRows.length === 1 && selectedRows[0] === documents[kept.headingAt].path && documentKey === documents[7].path)
                notice = ""
                testInput.key(Qt.Key_Delete)
                return 600
            },
            () => {
                check("Delete takes a heading out too, and what is open stays open", documents.length === kept.count && !documents.some(d => d.name === kept.heading)
                      && documentKey === documents[7].path && notice === "“" + kept.heading + "” removed from “Sunday Morning”." && !noticeIsError, notice)
                check("what took its place is selected, and not opened", selectedRows.length === 1 && selectedRows[0] === documents[kept.headingAt].path && documentKey === documents[7].path,
                      selectedNames().join(", "))
                // ---- Rows copied and pasted
                click(centre(rowAt(1)))
                return 400
            },
            () => {
                clickWith(centre(rowAt(2)), Qt.ControlModifier)
                return 300
            },
            () => {
                check("(two presentations selected, the first of them open)", selectedRows.length === 2 && documentKey === documents[1].path
                      && documents[1].kind === "presentation" && documents[2].kind === "presentation", selectedNames().join(", "))
                kept.copied = [documents[1], documents[2]]
                notice = ""
                testInput.key(Qt.Key_C, Qt.ControlModifier)
                return 300
            },
            () => {
                check("Ctrl+C copies the selected rows, and says so", copiedRows.length === 2 && copiedRows[0] === documents[1].path && copiedRows[1] === documents[2].path
                      && notice === "2 rows copied." && !noticeIsError, notice)
                check("and the playlist is as it was", documents.length === kept.count)
                click(centre(rowAt(0)))
                return 300
            },
            () => {
                check("(the first heading selected)", selectedRows.length === 1 && documents[0].kind === "header" && selectedRows[0] === documents[0].path)
                notice = ""
                kept.bin = mediaBinVisible
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 700
            },
            () => {
                check("Ctrl+V pastes them under the heading selected, as they were, arrangements and all; what is open stays open, and the media bin as it was",
                      documents.length === kept.count + 2 && documents[1].name === kept.copied[0].name && documents[2].name === kept.copied[1].name
                      && documents[1].file === kept.copied[0].file && documents[1].arrangement === kept.copied[0].arrangement
                      && documents[2].arrangement === kept.copied[1].arrangement && documents[1].path !== kept.copied[0].path
                      && documentKey === documents[3].path && mediaBinVisible === kept.bin, names().slice(0, 5).join(", "))
                check("the pasted rows are the ones selected, drawn so, and the first lit", selectedRows.length === 2 && selectedRows[0] === documents[1].path
                      && selectedRows[1] === documents[2].path && drawnSelected(1) && drawnSelected(2) && lit(1), selectedNames().join(", "))
                check("and the operator is told", notice === "2 rows pasted into “Sunday Morning”." && !noticeIsError, notice)
                // A slide clicked: the rows are no longer what was last clicked
                click(centre(slideCell(0)))
                return 400
            },
            () => {
                check("(the rows are no longer what was last clicked)", !rowsFocused)
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 400
            },
            () => {
                check("after a click on a slide, Ctrl+V is the media bin again, and pastes nothing", mediaBinVisible === !kept.bin && documents.length === kept.count + 2)
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 400
            },
            () => {
                check("(and back)", mediaBinVisible === kept.bin)
                click(centre(rowAt(5)), Qt.RightButton)
                return 400
            },
            () => {
                check("a row's menu has Copy and Paste", menu.opened && labels().includes("Copy, Paste 2 Rows Under, Remove from Playlist"), labels())
                kept.under = documents[5].name
                click(menuRow("Paste 2 Rows Under"))
                return 700
            },
            () => {
                check("Paste puts them under that row", documents.length === kept.count + 4 && documents[6].name === kept.copied[0].name
                      && documents[7].name === kept.copied[1].name && documents[5].name === kept.under, names().slice(4, 9).join(", "))
                // Into another playlist, opened from the list: at its end
                openPlaylist(catalog.playlists.find(p => p.name === "Demo Playlist").path)
                return 500
            },
            () => {
                kept.other = documents.length
                testInput.key(Qt.Key_V, Qt.ControlModifier)
                return 700
            },
            () => {
                // (The playlist's first presentation was opened with it, and is the row selected: they go under it, which in
                // this playlist of one row is its end)
                check("in another playlist, opened from the list, Ctrl+V pastes them under the row selected there, the one opened with it", documents.length === kept.other + 2
                      && documents[kept.other].name === kept.copied[0].name && documents[kept.other + 1].name === kept.copied[1].name && selectedRows.length === 2,
                      names().slice(-3).join(", "))
                // ---- In a library, Delete does nothing
                openLibrary(catalog.libraries[0].path)
                return 500
            },
            () => {
                kept.library = documents.length
                check("in a library nothing is selected but the presentation open, which the list marks", selectedRows.length === 0 && documentKey !== "" && drawnSelected(documents.findIndex(d => d.path === documentKey)))
                notice = ""
                testInput.key(Qt.Key_Delete)
                return 400
            },
            () => {
                check("and Delete does nothing there", documents.length === kept.library && notice === "" && testInput.exists(documents[0].file), notice)
                check("the log says what was taken out, and what was added where",
                      logged(/  edit +2 rows removed from the playlist "Sunday Morning": "Jesus Have It All", "Jesus Have It All"/).length === 1
                      && logged(/  edit +1 row removed from the playlist "Sunday Morning": /).length === 1
                      && logged(/  edit +"Jesus Have It All" added to the playlist "Sunday Morning", under "9:00am Songs"/).length === 1
                      && logged(/  edit +"Move Of God" added to the playlist "Sunday Morning", under "/).length === 1, logged(/  edit /).slice(-6).join(" | "))
                check("and what was pasted where", logged(/  edit +2 rows pasted into the playlist "Sunday Morning", under "/).length === 2
                      && logged(/  edit +2 rows pasted into the playlist "Demo Playlist", under "All Hail King Jesus": /).length === 1, logged(/ pasted /).join(" | "))
                check("nothing went wrong on the way", logged(/PROBLEM|WARNING|ERROR/).filter(l => !l.includes("qt.qpa.theme")).length === 0, logged(/PROBLEM|WARNING|ERROR/).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
