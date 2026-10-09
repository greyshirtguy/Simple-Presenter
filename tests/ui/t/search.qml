import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Search: the window Ctrl+F and the toolbar's button bring up, finding presentations by
// their names and by their words, looking at the one picked, opening it, and adding it
// to the playlist that is open.
QtObject {
    id: t

//COMMON
    function rows() {
        return Lib.findAll(named("searchResults"), item => String(item.objectName) === "searchRow")
    }

    function found() {
        return rows().map(row => row.modelData.name)
    }

    function shown(name) {
        const item = named(name)
        if (!item)
            return false
        for (let at = item; at; at = at.parent) {
            if (!at.visible)
                return false
        }
        return true
    }

    function retype(text) {
        named("searchField").text = ""
        testInput.type(text)
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                return 2500
            },
            () => {
                check("the presentations of the libraries have been read, off the thread that draws", Search.count >= 10 && !Search.reading, Search.count)
                check("the search is not up until it is asked for", !searchOpen && !shown("searchPanel"))
                testInput.shortcut(Qt.Key_F, Qt.ControlModifier)
                return 400
            },
            () => {
                check("Ctrl+F brings it up, ready to be typed in", searchOpen && shown("searchPanel") && named("searchField").activeFocus && found().length === 0)
                testInput.type("move of")
                return 400
            },
            () => {
                check("a presentation is found by its name, as it is typed", found()[0] === "Move Of God" && rows()[0].modelData.byName === true, found().join(", "))
                kept.path = rows()[0].modelData.path
                const words = Search.wordsOf(kept.path)
                check("the one picked is shown as its words", words.length > 3 && named("searchWords").text.includes(words[1].split("\n")[0]), words.length)
                // A line from the middle of it, to search by
                kept.line = words[3].split("\n")[0]
                testInput.grab("1-by-name")
                named("searchAsSlides").clicked()
                return 900
            },
            () => {
                check("or as its slides", named("searchSlides").visible && named("searchSlides").count === catalog.open(kept.path).slides.length, named("searchSlides").count)
                testInput.grab("2-slides")
                named("searchAsText").clicked()
                retype(kept.line.toLowerCase().replace(/[^a-z ]/g, ""))
                return 500
            },
            () => {
                const row = rows().find(r => r.modelData.path === kept.path)
                check("it is found by its words too, whatever their capitals and punctuation, with the line that has them", row !== undefined && row.modelData.line === kept.line,
                      kept.line + " -> " + found().join(", "))
                retype("zzzz qqqq")
                return 300
            },
            () => {
                check("what nothing has finds nothing", found().length === 0)
                retype("o")
                return 400
            },
            () => {
                kept.many = found().length
                check("a letter finds many, those found by name first", kept.many > 5 && rows()[0].modelData.byName === true, kept.many)
                testInput.key(Qt.Key_Down)
                testInput.key(Qt.Key_Down)
                check("the arrow keys pick another", named("searchResults").currentIndex === 2)
                testInput.key(Qt.Key_Up)
                check("and back", named("searchResults").currentIndex === 1)
                // ---- adding to a playlist: only with one open
                check("with a library open there is no playlist to add to", !named("searchAdd").enabled && named("searchHint").text.includes("have the playlist open first"))
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                check("Esc puts it away", !searchOpen && !shown("searchPanel"))
                const playlist = catalog.playlists.find(p => !p.folder)
                openPlaylist(playlist.path)
                kept.playlist = playlist.path
                kept.before = catalog.playlistItems(playlist.path).length
                named("searchButton").clicked()
                return 400
            },
            () => {
                check("the toolbar's button brings it up as well, empty again", searchOpen && named("searchField").text === "" && named("searchField").activeFocus)
                testInput.type("build my")
                return 400
            },
            () => {
                check("with a playlist open, the one picked can be added to it", found()[0] === "Build My Life" && named("searchAdd").enabled
                      && named("searchHint").text.includes("Ctrl+Enter adds it"), found().join(", "))
                testInput.key(Qt.Key_Return, Qt.ControlModifier)
                return 500
            },
            () => {
                const items = catalog.playlistItems(kept.playlist)
                check("Ctrl+Enter adds it to the end of the playlist", items.length === kept.before + 1 && items[items.length - 1].name === "Build My Life", items.length)
                check("and the search stays, saying what it did", searchOpen && named("searchHint").text.includes("Added") && named("searchHint").text.includes("Build My Life"))
                retype("move of god")
                return 400
            },
            () => {
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                check("Enter opens the one picked in its library, and puts the search away", !searchOpen && playlistId === "" && libraryPath !== ""
                      && document !== null && document.name === "Move Of God", document ? document.name : "nothing")
                testInput.key(Qt.Key_Right)
                check("and the keys are the show's again", liveIndex === 0 && !cleared)
                return 200
            }
        ]
        next()
    }
}
