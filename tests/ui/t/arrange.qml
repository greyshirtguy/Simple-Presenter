import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// The arrangement editor over the slides: the button that opens it, Master's row and the arrangement's, a new
// arrangement, a group dragged into one, moved along it and taken out of it, and the window that lists a
// presentation's arrangements, where one is copied, renamed and removed. What the slides then are, what is drawn,
// and (after, with protoc) what the song's file holds.
QtObject {
    id: t

//COMMON
    readonly property var bar: named("arrangementBar")

    function topOf(item) {
        return item.mapToItem(null, 0, 0).y
    }

    // How far below the top of the slides' part of the window the slides themselves begin
    function slidesFrom() {
        return topOf(named("slideGrid")) - grid.mapToItem(null, 0, 0).y
    }

    function nameOf(group) {
        return group === null ? "(missing group)" : group.name !== "" ? group.name : "(no name)"
    }

    // The labels of Master's row, and of the arrangement's, from the left
    function masterLabels() {
        return Lib.findAll(named("arrangementMaster"), i => i.objectName === "arrangementGroup").sort((a, b) => a.x - b.x)
    }

    function runLabels() {
        return Lib.findAll(named("arrangementRun"), i => i.objectName === "arrangementPlace").sort((a, b) => a.x - b.x)
    }

    function said(labels) {
        return labels.map(label => label.name).join(",")
    }

    function near(colour, wanted) {
        const part = (c, i) => parseInt(String(c).slice(1 + 2 * i, 3 + 2 * i), 16)
        return [0, 1, 2].every(i => Math.abs(part(colour, i) - part(wanted, i)) <= 30)
    }

    // The colour a label is drawn in, near its left end where its name is not
    function drawn(label) {
        const at = label.mapToItem(null, 4, label.height - 4)
        return testInput.windowPixel(at.x, at.y)
    }

    // Presses on a label and drags it to a point of the window, without letting go
    function hold(label, to) {
        const from = centre(label)
        testInput.mouse(0, from.x, from.y)
        for (let i = 1; i <= 10; ++i)
            testInput.mouse(1, from.x + (to.x - from.x) * i / 10, from.y + (to.y - from.y) * i / 10)
        kept.held = to
    }

    function letGo() {
        testInput.mouse(2, kept.held.x, kept.held.y)
    }

    function gone(name) {
        const item = named(name)
        return item === null || !item.visible
    }

    // The arrangement being viewed, and one by its name, as the presentation's list has them
    function viewed() {
        return document.arrangementList.find(a => a.name === document.arrangement) ?? null
    }

    function arrangement(name) {
        return document.arrangementList.find(a => a.name === name) ?? null
    }

    function names() {
        return document.arrangementList.map(a => a.name).join("|")
    }

    function rows() {
        return Lib.findAll(named("arrangementsList"), i => i.objectName === "arrangementRow").sort((a, b) => a.y - b.y)
    }

    function row(name) {
        return rows().find(r => r.modelData.name === name)
    }

    function logged(what) {
        return testInput.readText(Log.path).split("\n").filter(line => line.includes("  edit  ") && line.includes(what)).length
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                arrangementsShown = false
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Abandoned"))
                return 600
            },
            () => {
                const toggle = named("arrangementToggle")
                kept.master = document.groups.map(g => g.id)
                kept.masterNames = document.groups.map(nameOf)
                kept.from = slidesFrom()
                check("the song has its groups, as Master has them, and the one arrangement ProPresenter gave it, which it is viewed in",
                      kept.master.length === 7 && document.arrangement === "Default" && names() === "Default" && viewed().groups.length === 11, kept.masterNames.join(","))
                check("the line over the slides has the Arrangements button at its right, with ProPresenter's picture, and no drop-down any more",
                      toggle !== null && toggle.visible && toggle.on === false && named("arrangementBox") === null
                      && Lib.findAll(toggle, i => i.name === "Arrangements" && i.inkName !== undefined).length === 1
                      && Math.abs(toggle.mapToItem(grid, toggle.width, 0).x - (grid.width - 16)) < 1.5)
                const title = named("gridTitle")
                const viewedIn = named("gridArrangement")
                check("the song's name is in the middle of the line", title.text === "Abandoned"
                      && Math.abs(title.mapToItem(grid, title.width / 2, 0).x - grid.width / 2) < 1.5, title.mapToItem(grid, title.width / 2, 0).x + " of " + grid.width)
                check("which arrangement it is viewed in is said beside the button, on its left and set against it", viewedIn.visible && viewedIn.text === "Default"
                      && Math.abs(viewedIn.mapToItem(null, viewedIn.width, 0).x - (toggle.mapToItem(null, 0, 0).x - 8)) < 1.5
                      && viewedIn.mapToItem(null, 0, 0).x > title.mapToItem(null, title.width, 0).x)
                kept.toggleAt = toggle.mapToItem(null, 0, 0).x
                check("nothing of the editor is made until it is asked for, and the line is no higher than it was", named("arrangementBar") === null && kept.from <= 34, kept.from)
                kept.hash = testInput.fileHash(document.path)
                testInput.snapshot(document.path, "arrange-before")
                click(centre(toggle))
                return 500
            },
            () => {
                const toggle = named("arrangementToggle")
                check("the button opens the editor under the line: two thin rows, and the slides begin that much lower", bar !== null && bar.visible && toggle.on === true
                      && arrangementsShown === true && bar.height <= 60 && Math.abs(slidesFrom() - (kept.from + bar.height)) < 1.5, slidesFrom() + " from " + kept.from)
                const master = masterLabels()
                check("the top row is Master: every group of the song, in its order, by name", said(master) === kept.masterNames.join(",") && master.length === 7, said(master))
                // (The colour of a group in the grid is the app's own for a group of that name, where it has one, before
                // the colour the song's file gives it: so a label is to be the colour of its group's slides, and in this
                // song the two differ for most groups.)
                const frames = document.groups.map(g => g.name === "" ? "#5a5d66" : String(groupColor(document.slides.find(s => s.group === g.name))))
                check("each in the colour its group's slides have in the grid below, the one with no name grey: as the labels are set, and as they are drawn",
                      master.every((label, i) => near(label.color, frames[i])) && master.every(label => near(drawn(label), String(label.color)))
                      && frames.filter((colour, i) => document.groups[i].color !== "" && !near(colour, document.groups[i].color)).length >= 3,
                      master.map(drawn).join(" ") + " for " + frames.join(" "))
                const cell = slideCell(document.slides.findIndex(s => s.group === "Chorus 1"))
                check("which is the colour the frame of such a slide is", cell !== null && near(String(cell.frame), String(masterLabels()[2].color))
                      && masterLabels()[2].name === "Chorus 1", cell ? String(cell.frame) : "no slide")
                const places = runLabels()
                check("the row under it has the arrangement's picker, which says which, and beside it what that arrangement goes through, in its order",
                      Lib.findAll(named("arrangementPicker"), i => i.text === "Default").length === 1 && places.length === 11
                      && said(places) === viewed().groups.map(id => nameOf(document.groups.find(g => g.id === id))).join(","), said(places))
                check("both rows of labels begin at the same place", Math.abs(master[0].mapToItem(null, 0, 0).x - places[0].mapToItem(null, 0, 0).x) < 3)
                testInput.grab("1-open")
                click(centre(named("arrangementPicker")))
                return 400
            },
            () => {
                check("the picker's menu has Master and the song's arrangements, the one viewed ticked, then a new one and the list of them",
                      menu.opened && labels() === "[Arrangement], Master, *Default, --, New Arrangement, Edit Arrangements…", labels())
                // (Under a caption a menu's rows are set in, and the tick is in that room: it was on the name's first letter.)
                const ticked = Lib.find(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === "Default")
                const tick = Lib.findAll(ticked, i => i.text === "✓" && i.visible)
                const words = Lib.findAll(ticked, i => i.text === "Default" && i.visible)
                check("the tick of the one viewed stands before its name, clear of it", tick.length === 1 && words.length === 1
                      && tick[0].mapToItem(null, tick[0].implicitWidth, 0).x <= words[0].mapToItem(null, 0, 0).x + words[0].leftPadding,
                      tick.length ? tick[0].mapToItem(null, tick[0].implicitWidth, 0).x + " then " + words[0].mapToItem(null, 0, 0).x : "no tick")
                click(menuRow("New Arrangement"))
                return 800
            },
            () => {
                const made = arrangement("New Arrangement")
                check("New Arrangement makes one, last of the song's, that begins as a copy of Master: every group once, in Master's order",
                      names() === "Default|New Arrangement" && made !== null && made.groups.join() === kept.master.join(), names())
                check("and it is the one viewed: in the picker, after the song's name, and in the row of the library, which lists both for its menu",
                      document.arrangement === "New Arrangement" && named("gridArrangement").text === "New Arrangement"
                      && Lib.findAll(named("arrangementPicker"), i => i.text === "New Arrangement").length === 1
                      && currentEntry().arrangement === "New Arrangement" && currentEntry().arrangements.join("|") === "Default|New Arrangement")
                check("its row has Master's labels, and the slides are in Master's order", said(runLabels()) === kept.masterNames.join(",")
                      && document.slides.map(s => s.id).join() === catalog.openArranged(document.path, "").slides.map(s => s.id).join())
                check("the log says what was made", logged("an arrangement made for \"Abandoned\": \"New Arrangement\"") === 1)
                testInput.snapshot(document.path, "arrange-new")
                // The pointer over the second of its groups
                const second = centre(runLabels()[1])
                testInput.mouse(4, second.x, second.y)
                return 300
            },
            () => {
                const out = runLabels().map(label => Lib.find(label, i => i.objectName === "arrangementTakeOut"))
                check("the pointer over a group of the arrangement brings up an ✕ on it, and on no other", out[1].visible && out.filter(x => x.visible).length === 1)
                check("Master's groups have none", masterLabels().every(label => Lib.find(label, i => i.objectName === "arrangementTakeOut") === null))
                kept.taken = runLabels()[1].name
                testInput.grab("2-take-out")
                click(centre(out[1]))
                return 700
            },
            () => {
                const left = kept.masterNames.filter((name, i) => i !== 1)
                check("the ✕ takes that group out of the arrangement", said(runLabels()) === left.join(",") && viewed().groups.join() === kept.master.filter((id, i) => i !== 1).join(),
                      said(runLabels()))
                check("and its slides are no longer among those shown, though the song still has the group", !document.slides.some(s => s.group === kept.taken)
                      && document.groups.length === 7 && document.groups[1].name === kept.taken && kept.taken === "Verse 1", kept.taken)
                check("nothing was picked up by that click", bar.carried === null && gone("arrangementCarried"))
                check("the log says which group was taken out of which arrangement",
                      logged("the arrangement \"New Arrangement\" of \"Abandoned\": the group \"Verse 1\" taken out of place 2") === 1)
                kept.hash1 = testInput.fileHash(document.path)
                // That group dragged down from Master to the end of the arrangement
                const last = runLabels()[5]
                hold(masterLabels()[1], last.mapToItem(null, last.width + 14, last.height / 2))
                return 400
            },
            () => {
                const carried = named("arrangementCarried")
                const line = named("arrangementLanding")
                const last = runLabels()[5]
                const at = line.mapToItem(null, 1, line.height / 2)
                check("a group dragged down from Master is held, and would land in the gap of the arrangement that the pointer is nearest", bar.carried !== null
                      && bar.carried.from === "master" && bar.carried.group.name === "Verse 1" && bar.landing === 6, bar.landing)
                const middle = carried.mapToItem(null, carried.width / 2, carried.height / 2)
                check("its label is under the pointer, held by the point it was picked up by: the middle, here", carried !== null && carried.visible && carried.name === "Verse 1"
                      && Math.abs(middle.x - kept.held.x) < 2 && Math.abs(middle.y - kept.held.y) < 2 && near(carried.color, String(masterLabels()[1].color)),
                      middle.x + "," + middle.y + " for " + kept.held.x + "," + kept.held.y)
                check("and a line says where it would land: after the last group, drawn in orange over the label that is held", line.visible
                      && line.mapToItem(null, 0, 0).x > last.mapToItem(null, last.width, 0).x - 1 && near(testInput.windowPixel(at.x, at.y), "#ff8a1f")
                      && at.x > carried.mapToItem(null, 0, 0).x && at.x < carried.mapToItem(null, carried.width, 0).x,
                      testInput.windowPixel(at.x, at.y))
                check("nothing is changed until it is let go", said(runLabels()).split(",").length === 6 && testInput.fileHash(document.path) === kept.hash1)
                testInput.grab("3-held")
                letGo()
                return 700
            },
            () => {
                const places = runLabels()
                check("let go, the group is in the arrangement there", places.length === 7 && places[6].name === "Verse 1" && viewed().groups[6] === kept.master[1], said(places))
                check("and its slides are shown last", document.slides[document.slides.length - 1].group === "Verse 1")
                check("nothing is held any more", bar.carried === null && bar.landing === -1 && gone("arrangementCarried") && gone("arrangementLanding"))
                check("the log says where it was put", logged("the group \"Verse 1\" put in at place 7") === 1)
                // The last of the arrangement dragged to before its first
                const first = places[0]
                hold(places[6], first.mapToItem(null, 2, first.height / 2))
                return 400
            },
            () => {
                check("a group of the arrangement dragged along it is held, fainter where it was, and would land before the first", bar.carried !== null && bar.carried.from === "run"
                      && bar.carried.place === 6 && bar.landing === 0 && runLabels()[6].opacity < 0.5 && runLabels()[5].opacity === 1 && !gone("arrangementLanding"), bar.landing)
                letGo()
                return 700
            },
            () => {
                const places = runLabels()
                kept.edited = [kept.master[1], kept.master[0]].concat(kept.master.slice(2))
                check("let go there it is the first, and the others are in the order they were", places[0].name === "Verse 1" && viewed().groups.join() === kept.edited.join(), said(places))
                check("the slides begin with that group now, and the label is drawn first, in the colour those slides have", document.slides[0].group === "Verse 1"
                      && document.slides[0].groupStart === true && near(drawn(places[0]), String(groupColor(document.slides[0]))), drawn(places[0]))
                check("the log says from where to where", logged("the group \"Verse 1\" moved from place 7 to place 1") === 1)
                kept.hash2 = testInput.fileHash(document.path)
                testInput.grab("4-arranged")
                // Let go just after where it is
                const third = places[2]
                hold(third, third.mapToItem(null, third.width - 2, third.height / 2 + 9))
                return 400
            },
            () => {
                check("a group held just after where it is would land there", bar.carried !== null && bar.carried.place === 2 && bar.landing === 3, bar.landing)
                letGo()
                return 500
            },
            () => {
                check("which is where it is: nothing is changed, and nothing written", viewed().groups.join() === kept.edited.join() && testInput.fileHash(document.path) === kept.hash2
                      && notice === "", notice)
                // Let go off the row
                const second = runLabels()[1]
                hold(second, second.mapToItem(null, 40, 90))
                return 400
            },
            () => {
                check("held away from the arrangement's row it would land nowhere: its label is with the pointer still, and there is no line",
                      bar.carried !== null && bar.landing === -1 && gone("arrangementLanding") && !gone("arrangementCarried"))
                letGo()
                return 500
            },
            () => {
                check("and let go there it stays where it was", viewed().groups.join() === kept.edited.join() && testInput.fileHash(document.path) === kept.hash2 && bar.carried === null)
                testInput.snapshot(document.path, "arrange-edited")
                click(centre(named("arrangementPicker")))
                return 400
            },
            () => {
                check("the menu has the new arrangement, ticked", labels() === "[Arrangement], Master, Default, *New Arrangement, --, New Arrangement, Edit Arrangements…", labels())
                // (The first row of that name is the arrangement; the one under the line makes another.)
                click(menuRow("Master"))
                return 700
            },
            () => {
                check("Master picked: the slides are in Master's order, and nothing is said beside the button, which is where it was", document.arrangement === ""
                      && document.slides.map(s => s.id).join() === catalog.openArranged(document.path, "").slides.map(s => s.id).join() && !named("gridArrangement").visible
                      && named("arrangementToggle").mapToItem(null, 0, 0).x === kept.toggleAt)
                check("the picker says Master, and there is nothing beside it: Master is the row above", Lib.findAll(named("arrangementPicker"), i => i.text === "Master").length === 1
                      && runLabels().length === 0 && masterLabels().length === 7)
                kept.hash3 = testInput.fileHash(document.path)
                const master = masterLabels()[2]
                hold(master, master.mapToItem(null, 30, bar.rowHeight))
                return 400
            },
            () => {
                const hint = Lib.findAll(bar, i => i.text !== undefined && String(i.text).startsWith("Master cannot be changed here") && i.visible)
                check("a group dragged down while Master is viewed has nowhere to land, and the row says why", bar.carried !== null && bar.landing === -1 && hint.length === 1
                      && gone("arrangementLanding"))
                letGo()
                return 500
            },
            () => {
                check("let go, nothing is changed", testInput.fileHash(document.path) === kept.hash3 && names() === "Default|New Arrangement" && bar.carried === null)
                click(centre(named("arrangementPicker")))
                return 400
            },
            () => {
                click(menuRow("Edit Arrangements…"))
                return 600
            },
            () => {
                const panel = named("arrangementsPanel")
                check("Edit Arrangements… brings up a window with the song's arrangements, Master not among them", arrangementsOpen && panel !== null && panel.visible
                      && rows().map(r => r.modelData.name).join("|") === "Default|New Arrangement", rows().map(r => r.modelData.name).join("|"))
                const marks = rows().map(r => Lib.findAll(r, i => i.letter !== undefined && i.ground !== undefined).sort((a, b) => a.x - b.x))
                check("each row has what its arrangement goes through as narrow marks, one for each place, with the first letter of the group's name",
                      marks.every((m, i) => m.length === rows()[i].modelData.groups.length && m.every(mark => mark.width <= 14))
                      && marks[1].map(m => m.letter).join("") === "V" + "CVTBE" && marks[0].length === 11, marks.map(m => m.map(x => x.letter).join("")).join(" | "))
                check("in the colours the labels of the editor have, and all of an arrangement of eleven in sight",
                      marks[1].every(m => near(String(m.color), String(masterLabels().find(label => label.group.id === m.modelData).color)))
                      && marks[0][10].mapToItem(null, 13, 0).x <= named("arrangementsPanel").mapToItem(null, named("arrangementsPanel").width, 0).x - 12)
                check("with Master viewed, no row says it is the one viewed, and the first is picked", rows().every(r => !r.viewed) && rows()[0].chosen && !rows()[1].chosen)
                testInput.grab("5-window")
                click(row("New Arrangement").mapToItem(null, 30, 16))
                return 300
            },
            () => {
                check("a click picks a row", row("New Arrangement").chosen && !row("Default").chosen)
                click(centre(named("arrangementsDuplicate")))
                return 700
            },
            () => {
                const copy = arrangement("New Arrangement 2")
                check("Duplicate makes a copy straight after the one picked, named after it with a number, going through what it does",
                      names() === "Default|New Arrangement|New Arrangement 2" && copy !== null && copy.groups.join() === kept.edited.join(), names())
                check("the copy is the one picked, and what is viewed is as it was", row("New Arrangement 2").chosen && document.arrangement === "")
                check("the log says what was copied as what", logged("the arrangement \"New Arrangement\" of \"Abandoned\" copied as \"New Arrangement 2\"") === 1)
                click(row("New Arrangement 2").mapToItem(null, 30, 16), Qt.RightButton)
                return 400
            },
            () => {
                check("a right click on a row has the same to do", labels() === "[New Arrangement 2], Rename, Duplicate, Delete…", labels())
                click(menuRow("Rename"))
                return 400
            },
            () => {
                const box = Lib.findAll(named("arrangementsList"), i => i.objectName === "arrangementNameBox" && i.visible)
                check("Rename puts the name there to be typed over", box.length === 1 && box[0].activeFocus && box[0].selectedText === "New Arrangement 2")
                testInput.type("Evening")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                check("and the arrangement has the name typed", names() === "Default|New Arrangement|Evening" && row("Evening").chosen
                      && logged("the arrangement \"New Arrangement 2\" of \"Abandoned\" renamed \"Evening\"") === 1, names())
                click(row("Evening").mapToItem(null, 30, 16), Qt.RightButton)
                return 400
            },
            () => {
                click(menuRow("Rename"))
                return 400
            },
            () => {
                testInput.type("Default")
                testInput.key(Qt.Key_Return)
                return 700
            },
            () => {
                check("a name another arrangement of the song has is not taken, and the window says why", names() === "Default|New Arrangement|Evening"
                      && notice.includes("has an arrangement named Default already"), notice)
                click(centre(named("arrangementsDelete")))
                return 400
            },
            () => {
                check("Delete asks first, by its button", menu.opened && labels() === "[Delete “Evening”?], Delete Arrangement", labels())
                click(menuRow("Delete Arrangement"))
                return 700
            },
            () => {
                check("and then the arrangement is gone, and the one before it in the list is picked", names() === "Default|New Arrangement" && row("New Arrangement").chosen
                      && logged("the arrangement \"Evening\" of \"Abandoned\" removed") === 1, names())
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts the window away", !arrangementsOpen && gone("arrangementsPanel"))
                click(centre(named("arrangementPicker")))
                return 400
            },
            () => {
                // (The arrangement of that name, which is the first row of that name: the other makes a new one.)
                const made = Lib.findAll(menu.contentItem, item => item.modelData !== undefined && item.modelData !== null && item.modelData.label === "New Arrangement")
                                .sort((a, b) => a.mapToItem(null, 0, 0).y - b.mapToItem(null, 0, 0).y)
                click(centre(made[0]))
                return 700
            },
            () => {
                check("the arrangement that was made is viewed again, as it was left", document.arrangement === "New Arrangement" && viewed().groups.join() === kept.edited.join()
                      && runLabels()[0].name === "Verse 1" && names() === "Default|New Arrangement", names())
                openArrangements()
                return 500
            },
            () => {
                check("the window comes up on the arrangement that is viewed, and its row says so", row("New Arrangement").chosen && row("New Arrangement").viewed && !row("Default").viewed)
                testInput.key(Qt.Key_Delete)
                return 400
            },
            () => {
                check("the Delete key asks the same", menu.opened && labels() === "[Delete “New Arrangement”?], Delete Arrangement", labels())
                click(menuRow("Delete Arrangement"))
                return 800
            },
            () => {
                check("the arrangement that was viewed removed, the song is viewed as Master", names() === "Default" && document.arrangement === ""
                      && !named("gridArrangement").visible && runLabels().length === 0 && currentEntry().arrangement === ""
                      && document.slides.map(s => s.id).join() === catalog.openArranged(document.path, "").slides.map(s => s.id).join(), names())
                testInput.snapshot(document.path, "arrange-end")
                click(centre(named("arrangementsDone")))
                return 400
            },
            () => {
                check("Done puts the window away", !arrangementsOpen)
                // ---- A playlist's row has an arrangement of its own, which the playlist names by id and by name:
                // one that has been renamed since is still the row's.
                setArrangement(currentEntry(), "Default")
                kept.file = document.path
                kept.playlist = catalog.createPlaylist("Arranged", "").id
                catalog.addToPlaylist(kept.playlist, kept.file)
                return 700
            },
            () => {
                const item = catalog.playlistItems(kept.playlist).find(r => r.file === kept.file)
                check("put in a playlist while an arrangement is the one selected, the song's row there has that arrangement", item !== undefined && item.arrangement === "Default",
                      item ? item.arrangement : "no row")
                renameArrangement(arrangement("Default").id, "Morning")
                return 800
            },
            () => {
                const item = catalog.playlistItems(kept.playlist).find(r => r.file === kept.file)
                check("that arrangement renamed, in the library it is still the one viewed, by its new name", document.arrangement === "Morning" && names() === "Morning"
                      && named("gridArrangement").text === "Morning")
                check("and the playlist's row still has it, by its new name, though the playlist has not been written again", item.arrangement === "Morning"
                      && item.arrangements.join() === "Morning" && item.detail === "[Morning]", item.arrangement + " " + item.detail)
                openPlaylist(kept.playlist)
                return 700
            },
            () => {
                check("opened from that row, the song is in that arrangement", document !== null && document.path !== undefined && document.arrangement === "Morning"
                      && playlistId === kept.playlist, document ? document.arrangement : "none")
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("Abandoned"))
                return 600
            },
            () => {
                click(centre(named("arrangementToggle")))
                return 500
            },
            () => {
                check("the button shuts the editor again, and the slides are back where they began", arrangementsShown === false && named("arrangementToggle").on === false
                      && bar.visible === false && Math.abs(slidesFrom() - kept.from) < 1.5, slidesFrom())
                openEntry(documents.find(d => openable(d) && load(d).arrangements.length === 0))
                return 600
            },
            () => {
                check("a presentation with no arrangements has the button all the same: it is where one is made", document.arrangements.length === 0
                      && named("arrangementToggle").visible && !named("gridArrangement").visible)
                click(centre(named("arrangementToggle")))
                return 500
            },
            () => {
                check("opened, it has Master's row and the picker, which says Master", bar.visible && masterLabels().length === document.groups.length && runLabels().length === 0
                      && Lib.findAll(named("arrangementPicker"), i => i.text === "Master").length === 1, masterLabels().length)
                click(centre(named("arrangementPicker")))
                return 400
            },
            () => {
                check("and its menu has Master, a new one and the list", labels() === "[Arrangement], *Master, --, New Arrangement, Edit Arrangements…", labels())
                testInput.key(Qt.Key_Escape)
                return 300
            },
            () => {
                // Simple View is for running a show: the editor and its button are not in it
                kept.open = slidesFrom()
                setSimpleView(true, "the test")
                return 600
            },
            () => {
                check("in Simple View there is neither the editor nor its button, and the slides have the room", simple && !named("arrangementToggle").visible && !bar.visible
                      && slidesFrom() <= 34 && arrangementsShown === true, slidesFrom())
                check("and the name is at the left of its line there, clear of that view's own button in the middle", named("gridTitle").mapToItem(grid, 0, 0).x <= 17,
                      named("gridTitle").mapToItem(grid, 0, 0).x)
                setSimpleView(false, "the test")
                return 600
            },
            () => {
                check("and out of it they are back as they were left, the name in the middle again", named("arrangementToggle").visible && bar.visible
                      && Math.abs(slidesFrom() - kept.open) < 1.5 && Math.abs(named("gridTitle").mapToItem(grid, named("gridTitle").width / 2, 0).x - grid.width / 2) < 1.5, slidesFrom())
            },
            () => {
                click(centre(named("arrangementToggle")))
                return 300
            },
            () => {
                // (But for the one thing that was meant to be refused: the name another arrangement had.)
                const wrong = testInput.readText(Log.path).split("\n").filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme")
                                                                                 && !l.includes("not an image") && !l.includes("has an arrangement named Default already"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.length + " lines")
            }
        ]
        next()
    }
}
