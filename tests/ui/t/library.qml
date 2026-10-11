import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The presentations of a library: the list narrowed by the box under it, in a library and in a playlist.
QtObject {
    id: t

//COMMON
    // The names the list of presentations is showing, in its order (headings in brackets)
    function shownNames() {
        const list = named("presentationList")
        const names = []
        for (let i = 0; i < list.count; ++i)
            names.push(list.model[i].kind === "header" ? "[" + list.model[i].name + "]" : list.model[i].name)
        return names
    }

    function run() {
        steps = [
            // ---- The box under the list, which narrows it
            () => {
                transitionDuration = 0
                openLibrary(catalog.libraries[0].path)
                return 500
            },
            () => {
                const box = named("presentationFilter")
                const list = named("presentationList")
                kept.all = documents.length
                check("under the list of presentations is a box to narrow it by, empty to begin with, and the list ends above it", box !== null && box.visible
                      && box.text === "" && box.placeholderText === "Filter" && list.count === kept.all && kept.all > 20
                      && list.mapToItem(null, 0, list.height).y <= box.mapToItem(null, 0, 0).y + 0.5, list.count + " rows")
                click(centre(box))
                return 300
            },
            () => {
                const box = named("presentationFilter")
                check("a click puts the keyboard in it", box.activeFocus)
                testInput.type("ALL")
                return 400
            },
            () => {
                const names = shownNames()
                const count = named("presentationFilterCount")
                check("typing narrows the list to the names that have it, whatever the capitals, in the order they were in", documentFilter === "ALL"
                      && names.length > 1 && names.length < kept.all && names.every(name => name.toLowerCase().includes("all"))
                      && names.join("|") === documents.filter(d => d.name.toLowerCase().includes("all")).map(d => d.name).join("|"), names.join(", "))
                const first = listed(names[0])
                check("what is left is drawn from the top of the list, and the box says how many of how many", first.inView && first.light >= 3
                      && first.at.startsWith("0 ") && count.visible && count.text === names.length + " of " + kept.all && strokes(count).light >= 2,
                      JSON.stringify(first) + " " + count.text)
                check("the library itself is as it was: only the list is narrowed", documents.length === kept.all)
                testInput.grab("0-narrowed")
                testInput.type(" king")
                return 400
            },
            () => {
                const names = shownNames()
                check("every word typed has to be in a name", documentFilter === "ALL king" && names.length >= 1 && names.length < 4
                      && names.every(name => name.toLowerCase().includes("all") && name.toLowerCase().includes("king")), names.join(", "))
                kept.first = names[0]
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                check("Enter opens the first of what is left, and gives the keys back to the show", document !== null && document.name === kept.first
                      && !named("presentationFilter").activeFocus && documentFilter === "ALL king", document ? document.name : "nothing open")
                click(centre(named("presentationFilterClear")))
                return 400
            },
            () => {
                const open = listed(kept.first)
                check("the cross empties the box, and the whole list is back, with the one that is open in view and picked out", documentFilter === ""
                      && named("presentationFilter").text === "" && named("presentationList").count === kept.all && open.inView && open.picked && open.light >= 3
                      && !named("presentationFilterCount").visible, JSON.stringify(open))
                click(centre(named("presentationFilter")))
                testInput.type("zzzz")
                return 400
            },
            () => {
                const nothing = named("presentationFilterNothing")
                check("what no name has leaves nothing, and the list says so in words that are drawn", named("presentationList").count === 0 && nothing.visible
                      && strokes(nothing).light >= 3 && named("presentationFilterCount").text === "0 of " + kept.all, named("presentationFilterCount").text)
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc empties the box and gives the keys back", documentFilter === "" && !named("presentationFilter").activeFocus
                      && named("presentationList").count === kept.all && !named("presentationFilterNothing").visible)
                click(centre(named("presentationFilter")))
                testInput.type("build")
                return 300
            },
            () => {
                check("(narrowed again)", shownNames().length >= 1 && shownNames().length < kept.all, shownNames().join(", "))
                openPlaylist(catalog.playlists.find(p => p.name === "Sunday Morning").path)
                return 600
            },
            () => {
                kept.rows = documents.filter(d => d.kind !== "header").length
                check("opening a playlist empties the box: what is opened is seen whole", documentFilter === "" && named("presentationFilter").text === ""
                      && named("presentationList").count === documents.length && documents.some(d => d.kind === "header"), shownNames().join(", "))
                click(centre(named("presentationFilter")))
                testInput.type("build")
                return 400
            },
            () => {
                const names = shownNames()
                const rows = names.filter(name => !name.startsWith("["))
                check("a playlist is narrowed in the same way, to the rows that have what was typed", rows.length >= 1 && rows.every(name => name.toLowerCase().includes("build"))
                      && named("presentationFilterCount").text === rows.length + " of " + kept.rows, names.join(", "))
                check("and a heading stays only for the rows under it that are left", names.filter(name => name.startsWith("[")).length < documents.filter(d => d.kind === "header").length
                      && names.every((name, i) => !name.startsWith("[") || (i + 1 < names.length && !names[i + 1].startsWith("["))), names.join(", "))
                testInput.grab("0-playlist-narrowed")
            }
        ]
        next()
    }
}
