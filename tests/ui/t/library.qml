import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// The presentations of a library: the list narrowed by the box under it, in a library and in a playlist; one renamed
// in place (its file, the name inside the file, and the rows of playlists that are it), from a library and from a
// playlist; one deleted, which is its file moved to the Trash. What the files then hold is looked at when the test is
// over, by after_library in run.py.
QtObject {
    id: t

//COMMON
    // A row of the list of presentations, brought into view: by the name it shows
    function rowOf(name) {
        const list = named("presentationList")
        for (let i = 0; i < list.count; ++i) {
            if (list.model[i].name === name) {
                list.positionViewAtIndex(i, ListView.Contain)
                return list.itemAtIndex(i)
            }
        }
        return null
    }

    // The box a row is being renamed in, if it is
    function renameBox(row) {
        return row ? Lib.find(row, item => item.cursorPosition !== undefined && item.selectAll !== undefined) : null
    }

    function fileOf(name) {
        return libraryPath + "/" + name + ".pro"
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

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
                testInput.key(Qt.Key_Escape)
                return 300
            },
            // ---- Renaming
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Build My Life"))
                goLive(1)
                return 600
            },
            () => {
                kept.old = document.path
                kept.words = liveSlide.plainText
                check("(a presentation is open and on the output, and a playlist has it)", document.name === "Build My Life" && kept.old === fileOf("Build My Life")
                      && liveIndex === 1 && viewingLive && !cleared && testInput.lit(false, 0, 0.12, 1, 0.88) > 0.003
                      && catalog.playlists.some(p => catalog.playlistItems(p.path).some(row => row.file === kept.old)), testInput.lit(false, 0, 0.12, 1, 0.88))
                click(centre(rowOf("Build My Life")), Qt.RightButton)
                return 400
            },
            () => {
                check("a presentation's menu, in a library, has Rename and Delete", labels().includes("Edit, Rename, Rebuild Thumbnails, Export to PowerPoint…, Delete…"), labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                const box = renameBox(rowOf("Build My Life"))
                const edge = box ? box.mapToItem(null, 0.5, box.height / 2) : null
                const drawn = edge ? String(testInput.windowPixel(edge.x, edge.y)) : ""
                check("Rename puts a box in its row, with the name in it to be typed over", box !== null && box.visible && box.activeFocus && box.text === "Build My Life"
                      && box.selectedText === box.text, box ? box.text + " / " + box.selectedText : "no box")
                check("the box is drawn there, edged as the box that has the keyboard is", parseInt(drawn.substring(1, 3), 16) > 200 && parseInt(drawn.substring(5, 7), 16) < 90, drawn)
                testInput.grab("1-renaming")
                testInput.type("Build My Life Again")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                kept.renamed = fileOf("Build My Life Again")
                check("Enter renames the file", testInput.exists(kept.renamed) && !testInput.exists(kept.old))
                check("and the name inside it, with its slides as they were", catalog.open(kept.renamed).name === "Build My Life Again"
                      && catalog.open(kept.renamed).slides[1].plainText === kept.words)
                const title = named("gridTitle")
                const titled = strokes(title)
                const inList = listed("Build My Life Again")
                check("it is still the one being viewed, under its new name: over the slides", document !== null && document.path === kept.renamed
                      && documentKey === kept.renamed && title.text === "Build My Life Again" && titled.light >= 3 && titled.dark >= 3, title.text + " " + JSON.stringify(titled))
                check("and in the list, where its row is in view and picked out, and no row has the old name", inList.inView && inList.picked && inList.light >= 3
                      && inList.dark >= 3 && !documents.some(d => d.name === "Build My Life"), JSON.stringify(inList))
                check("the show goes on as it was: the same slide is live, and the output has it still", liveIndex === 1 && viewingLive && !cleared
                      && Show.liveKey === kept.renamed && liveDocument.path === kept.renamed && liveSlide.plainText === kept.words
                      && testInput.lit(false, 0, 0.12, 1, 0.88) > 0.003, Show.liveKey + " " + testInput.lit(false, 0, 0.12, 1, 0.88))
                check("the log has a line for it", logged(/  edit +the presentation "Build My Life.pro" renamed "Build My Life Again.pro"/).length === 1)
                testInput.grab("2-renamed")
                testInput.key(Qt.Key_Right)
                return 400
            },
            () => {
                check("and the arrow keys go on from where the show was", liveIndex === 2 && !cleared)
                // The playlist that has it
                const playlist = catalog.playlists.find(p => catalog.playlistItems(p.path).some(row => row.file === kept.renamed))
                check("the playlist that had it has it still", playlist !== undefined, catalog.playlists.map(p => p.name).join(", "))
                kept.playlist = playlist.path
                openPlaylist(playlist.path)
                return 600
            },
            () => {
                const row = documents.find(d => d.file === kept.renamed)
                const inList = listed("Build My Life Again")
                check("its row there has the new name, is not missing, and is drawn so", row !== undefined && row.name === "Build My Life Again" && !row.missing
                      && !documents.some(d => d.name === "Build My Life") && inList.light >= 3, row ? row.name + " " + JSON.stringify(inList) : "no row")
                click(centre(rowOf("Build My Life Again")))
                return 500
            },
            () => {
                check("and opens it", document !== null && document.path === kept.renamed && document.slides.length > 2)
                // Renamed from the playlist: another of its presentations
                kept.other = documents.find(d => openable(d) && d.file !== kept.renamed)
                click(centre(rowOf(kept.other.name)), Qt.RightButton)
                return 400
            },
            () => {
                check("in a playlist the menu has Rename, and no Delete: only the row is ever taken from there", labels().includes("Rename") && !labels().includes("Delete")
                      && labels().includes("Remove from Playlist"), labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                check("(its row has the box)", renameBox(rowOf(kept.other.name)) !== null && renameBox(rowOf(kept.other.name)).activeFocus)
                testInput.type(kept.other.name + " / Two")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                // A slash cannot be in a file's name: it is a dash there, and so in the name
                kept.otherName = kept.other.name + " - Two"
                const row = documents.find(d => d.path === kept.other.path)
                check("renamed from the playlist, the file is renamed, and the row with it", row !== undefined && row.name === kept.otherName
                      && row.file.endsWith("/" + kept.otherName + ".pro") && testInput.exists(row.file) && !testInput.exists(kept.other.file), row ? row.name + " " + row.file : "no row")
                kept.otherFile = row.file
                openLibrary(catalog.libraries[0].path)
                return 500
            },
            () => {
                check("and it is in its library under the new name", documents.some(d => d.name === kept.otherName) && !documents.some(d => d.name === kept.other.name))
                // A name the library has already
                click(centre(rowOf("Build My Life Again")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Rename"))
                return 400
            },
            () => {
                testInput.type("Abandoned")
                testInput.key(Qt.Key_Return)
                return 600
            },
            () => {
                const said = named("gridNotice")
                check("a name the library has already is refused, and it says so, in words that are drawn", notice.includes("already a presentation called") && noticeIsError
                      && said !== null && said.visible && strokes(said).light >= 3, notice)
                check("and nothing is renamed", testInput.exists(kept.renamed) && documents.some(d => d.name === "Build My Life Again")
                      && documents.filter(d => d.name === "Abandoned").length === 1)
                // Esc leaves the name as it was
                click(centre(rowOf("Build My Life Again")), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Rename"))
                return 400
            },
            () => {
                testInput.type("Something Else")
                testInput.key(Qt.Key_Escape)
                return 500
            },
            () => {
                check("Esc leaves the name as it was", testInput.exists(kept.renamed) && !testInput.exists(fileOf("Something Else")) && renameBox(rowOf("Build My Life Again")) === null)
                // ---- Deleting: one that a playlist has, while it is open and on the output
                const victim = catalog.playlistItems(catalog.playlists.find(p => p.name === "Demo Playlist").path).find(openable)
                kept.victim = victim.file
                kept.victimName = victim.name
                kept.after = documents[documents.findIndex(d => d.path === victim.file) + 1].name
                openEntry(documents.find(d => d.path === victim.file))
                // Its first slide that has words, and (with Alt, as it were) not the media the slide brings, so that
                // what is on the output is the slide's words and nothing else.
                kept.victimSlide = document.slides.findIndex(slide => slide.plainText.trim() !== "")
                goLive(kept.victimSlide, true)
                return 600
            },
            () => {
                check("(the one to be deleted is open and on the output)", document.path === kept.victim && viewingLive && !cleared && liveIndex === kept.victimSlide
                      && testInput.lit(false, 0, 0.12, 1, 0.88) > 0.003, testInput.lit(false, 0, 0.12, 1, 0.88))
                click(centre(rowOf(kept.victimName)), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Delete…"))
                return 400
            },
            () => {
                check("Delete asks first, saying where the file goes and what a playlist will say", menu.shown.some(i => (i.note ?? "").includes("moved to the Trash")
                      && i.note.includes(kept.victimName) && i.note.includes("missing")) && labels().includes("Move to Trash") && labels().includes("Cancel"), labels())
                testInput.grab("3-delete-asked")
                click(menuRow("Cancel"))
                return 400
            },
            () => {
                check("Cancel deletes nothing", testInput.exists(kept.victim) && documents.some(d => d.path === kept.victim))
                click(centre(rowOf(kept.victimName)), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Delete…"))
                return 400
            },
            () => {
                click(menuRow("Move to Trash"))
                return 800
            },
            () => {
                const moved = logged(/  trash +.* moved to the Trash, as /)
                kept.trash = moved.length === 1 ? moved[0].replace(/^.* moved to the Trash, as /, "") : ""
                check("Move to Trash takes the file out of the library, and it is in the Trash, not gone", !testInput.exists(kept.victim) && kept.trash !== ""
                      && testInput.exists(kept.trash) && kept.trash.includes("Trash"), kept.trash)
                const inList = listed(kept.after)
                const title = named("gridTitle")
                check("the list no longer has it, and the one that took its place is opened: named over the slides, and picked out in the list",
                      !documents.some(d => d.name === kept.victimName) && document !== null && document.name === kept.after && title.text === kept.after
                      && strokes(title).light >= 3 && inList.inView && inList.picked && inList.light >= 3, document ? document.name : "nothing open")
                check("its slide was taken off the output first, which is dark", cleared === true && testInput.lit(false, 0, 0.12, 1, 0.88) < 0.003, testInput.lit(false, 0, 0.12, 1, 0.88))
                check("the log has a line for it", logged(/  edit +the presentation ".*" moved to the Trash/).length === 1)
                testInput.grab("4-deleted")
                openPlaylist(catalog.playlists.find(p => p.name === "Demo Playlist").path)
                return 600
            },
            () => {
                const row = documents.find(d => d.name === kept.victimName)
                const item = rowOf(kept.victimName)
                const words = item ? Lib.find(item, i => i.font !== undefined && String(i.text).includes("(missing)")) : null
                check("the playlist that had it has its row still, which says it is missing", row !== undefined && row.missing && words !== null && words.visible
                      && strokes(words).light >= 3, row ? JSON.stringify(row.missing) : "no row")
                testInput.grab("5-missing-in-playlist")
            }
        ]
        next()
    }
}
