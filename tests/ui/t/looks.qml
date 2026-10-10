import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Looks: ProPresenter's own, read from the workspace; the live look, which is a look of
// its own and is what each audience screen goes by; a screen's slides dressed in the
// theme the live look gives it, with the presentation left as it was; a saved look made
// live from the toolbar, by an action and from the Looks window; the live look changed
// by itself, and saved as the look it came from; and the Looks window, laid out as
// ProPresenter's is.
QtObject {
    id: t

//COMMON
    function look(name) {
        return Looks.looks.find(l => l.name === name)
    }

    // The slide an audience scene's slide layer holds, as it was handed it
    function shownOn(scene) {
        const holders = Lib.findAll(scene.contentItem ?? scene, item => item.content !== undefined && item.content !== null && item.content.id !== undefined
                                    && item.content.elements !== undefined)
        return holders.length > 0 ? holders[holders.length - 1].content : null
    }

    function worded(slide) {
        return slide.elements.filter(e => e.text !== undefined && testInput.plain(e.text).trim() !== "")
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

    // The tick box of a layer for a screen, in the Looks window
    function box(layer, screen) {
        return Lib.find(named("lookCell:" + layer + ":" + screen), item => item.toggled !== undefined && item.visible === true)
    }

    function savedRows() {
        return Lib.findAll(named("looksList"), item => String(item.objectName).startsWith("lookRow:") && item.isLive === false)
    }

    function run() {
        steps = [
            () => {
                check("ProPresenter's looks are read from the workspace's file", Looks.looks.length === 7 && look("Lyrics L3rd") !== undefined
                      && look("Stream Clear") !== undefined, JSON.stringify(Looks.looks.map(l => l.name)))
                kept.lyrics = look("Lyrics L3rd").id
                check("the live look is a look of its own, named for the saved look it was made from, which it knows by id",
                      Looks.live.name === "Lyrics L3rd" && Looks.live.origin === kept.lyrics && Looks.live.id !== "" && Looks.live.id !== kept.lyrics
                      && !Looks.looks.some(l => l.id === Looks.live.id), JSON.stringify({ id: Looks.live.id, name: Looks.live.name, origin: Looks.live.origin }))
                check("opening the workspace says which saved look that was, and makes nothing live", Show.lookId === kept.lyrics)
                check("the toolbar's Looks button says which look is live", named("looksButton") !== null && named("looksButton").label === "Lyrics L3rd"
                      && lookName === "Lyrics L3rd", named("looksButton") ? named("looksButton").label : "no button")
                check("its themes are found in the workspace, by their place under Themes", Themes.themes.length >= 10 && Themes.theme("New Life Chapel") !== null
                      && Themes.theme("Samples/Black Box") !== null && Themes.tree.some(e => e.kind === "folder" && e.name === "Samples"),
                      Themes.themes.length)
                kept.room = Screens.audience[0].id
                kept.stream = Screens.audience[1].id
                const gets = Looks.liveOf(kept.stream)
                check("the live look gives each screen its layers and its theme", gets.slide === true && gets.media === false && gets.props === true
                      && gets.theme === "New Life Chapel" && Themes.slide(gets.theme, gets.themeSlide).name === "Lower 3rd", JSON.stringify(gets))
                kept.lower = Themes.slide(gets.theme, gets.themeSlide).slide
                const room = Looks.liveOf(kept.room)
                kept.upper = Themes.slide(room.theme, room.themeSlide).slide
                check("the room has another theme slide", room.media === true && Themes.slide(room.theme, room.themeSlide).name === "Upper 3rd")
                check("what it says of ProPresenter's other layers is read too", room.messages === true && room.announcements === true
                      && typeof room.videoInput === "boolean" && typeof room.mask === "string", JSON.stringify(room))
                kept.fileBefore = testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace")
                // The stream, which nothing is said about on this computer, is given a window
                Screens.setOutput(kept.stream, { output: "window" })
                return 800
            },
            () => {
                check("each screen's scene has the layers the live look gives it", output.slideOn && output.mediaOn && output.propsOn
                      && scenes[kept.stream].slideOn === true && scenes[kept.stream].mediaOn === false && scenes[kept.stream].propsOn === true)
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                kept.at = document.slides.findIndex(s => worded(s).length === 1)
                goLive(kept.at)
                return 1500
            },
            () => {
                const own = document.slides[kept.at]
                const inRoom = shownOn(output)
                const onStream = shownOn(scenes[kept.stream])
                const words = testInput.plain(worded(own)[0].text)
                check("a slide is shown on each screen dressed in that screen's theme: the theme's box", inRoom !== null && onStream !== null
                      && worded(inRoom).length === 1 && worded(onStream).length === 1
                      && worded(inRoom)[0].y === worded(kept.upper)[0].y && worded(inRoom)[0].height === worded(kept.upper)[0].height
                      && worded(onStream)[0].y === worded(kept.lower)[0].y && worded(onStream)[0].height === worded(kept.lower)[0].height
                      && worded(kept.lower)[0].y !== worded(kept.upper)[0].y,
                      (inRoom ? worded(inRoom)[0].y : "nothing") + " and " + (onStream ? worded(onStream)[0].y : "nothing"))
                check("the slide's own words", testInput.plain(worded(inRoom)[0].text) === words && testInput.plain(worded(onStream)[0].text) === words, words)
                const format = testInput.runsOf(worded(onStream)[0].text)[0]
                const themed = testInput.runsOf(worded(kept.lower)[0].text)[0]
                check("and the theme's font and size", format.size === themed.size && format.family === themed.family && format.color === themed.color,
                      format.family + " " + format.size + " " + format.color)
                const onDisk = catalog.open(document.path).slides.find(s => s.id === own.id)
                check("the presentation itself is as it was", onDisk !== undefined && worded(own)[0].y === worded(onDisk)[0].y
                      && worded(own)[0].height === worded(onDisk)[0].height && worded(own)[0].y !== worded(onStream)[0].y,
                      worded(own)[0].y + " in the file, " + worded(onStream)[0].y + " on the stream")
                check("and none of this has written anything to the workspace's file", testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace") === kept.fileBefore)
                testInput.grabOutput("1-room")
                // ---- another look, from the toolbar
                // (The toolbar's buttons are worked by their signal: a press on the toolbar is also the start of moving the window.)
                named("looksButton").clicked()
                return 500
            },
            () => {
                check("the Looks button gives the saved looks, the one the live look came from ticked, and the way to the window",
                      menu.opened && labels().startsWith("[Look], *Lyrics L3rd, ") && labels().includes("Stream Clear") && labels().endsWith("[], Edit Looks…"), labels())
                click(menuRow("Stream Clear"))
                return 1800
            },
            () => {
                kept.clear = look("Stream Clear").id
                check("picked there, a saved look is made live: the live look is now a copy of it, under the id it had",
                      Looks.live.name === "Stream Clear" && Looks.live.origin === kept.clear && Show.lookId === kept.clear && Looks.live.changed === false
                      && JSON.stringify(Looks.live.screens) === JSON.stringify(look("Stream Clear").screens), Looks.live.name + " from " + Looks.live.origin)
                check("which is in ProPresenter's file, where it keeps the live look", testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace") !== kept.fileBefore)
                check("the screens get other layers", scenes[kept.stream].slideOn === false && scenes[kept.stream].propsOn === true && output.slideOn === true
                      && output.mediaOn === true)
                check("the button says so", named("looksButton").label === "Stream Clear")
                check("and, this look having no theme for the room, the room's slide is shown again as it is",
                      worded(shownOn(output))[0].y === worded(document.slides[kept.at])[0].y, worded(shownOn(output))[0].y)
                check("how long the layers take to come and go is the look's", lookFade === 1000 && output.lookFade === 1000)
                // ---- an action
                commitAction({ slide: kept.at + 1 }, { kind: "look", lookId: kept.lyrics, lookName: "Lyrics L3rd" }, "")
                const action = document.slides[kept.at + 1].actions.find(a => a.kind === "look")
                check("a slide can be given an action that goes over to a look", action !== undefined && action.done === true && action.lookName === "Lyrics L3rd"
                      && action.title.includes("Lyrics L3rd"), JSON.stringify(action))
                sidePanel.showControlTab = "macros"
                const macros = []
                for (const collection of Macros.collections)
                    for (const macro of collection.macros)
                        macros.push(macro)
                kept.lookMacro = macros.find(m => m.actions.some(a => a.kind === "look"))
                check("ProPresenter's macros that go over to a look are understood", kept.lookMacro !== undefined
                      && kept.lookMacro.actions.find(a => a.kind === "look").done === true, macros.map(m => m.actions.map(a => a.kind).join("+")).join(" "))
                goLive(kept.at + 1)
                return 1500
            },
            () => {
                check("shown, the slide's action makes that look live", Looks.live.origin === kept.lyrics && Looks.live.name === "Lyrics L3rd" && Show.lookId === kept.lyrics
                      && scenes[kept.stream].slideOn === true, Looks.live.name)
                check("and the slide is on each screen in that look's theme", worded(shownOn(output))[0].y === worded(kept.upper)[0].y
                      && worded(shownOn(scenes[kept.stream]))[0].y === worded(kept.lower)[0].y)
                const target = kept.lookMacro.actions.find(a => a.kind === "look")
                runMacro(kept.lookMacro.id)
                check("a macro's action does the same", Looks.live.origin === look(target.lookName).id && Show.lookId === Looks.live.origin, target.lookName)
                Show.lookId = kept.lyrics
                // ---- the Looks window
                // (The toolbar's buttons are worked by their signal: a press on the toolbar is also the start of moving the window.)
                named("looksButton").clicked()
                return 500
            },
            () => {
                click(menuRow("Edit Looks…"))
                return 600
            },
            () => {
                check("Edit Looks brings the Looks window up, over the operator window and not in the settings", looksOpen && shown("looksPanel") && !settingsOpen
                      && !settingsScreen.sections.some(section => section.path === "looks"), settingsScreen.sections.map(section => section.name).join(", "))
                check("the live look is first in its list, and is the one picked, with the saved looks under it", looksPanel.onLive && named("lookRow:live").chosen
                      && named("lookRow:live").name === "Lyrics L3rd" && savedRows().length === 7, savedRows().map(row => row.name).join(", "))
                check("the saved look it came from is marked, and no other", savedRows().filter(row => row.origin).map(row => row.name).join() === "Lyrics L3rd")
                check("the live look is live already: Make Live is for a saved one, and there is nothing to save while it is unchanged",
                      named("looksMakeLive").visible === false && named("looksSave").visible === false && named("looksClose").visible === true)
                const layers = Lib.findAll(named("looksPanel"), item => String(item.objectName).startsWith("lookLayer:")).map(item => item.modelData.key)
                check("the layers are down the side in the order they lie on a screen, the top one first",
                      layers.join() === "mask,messages,props,announcements,theme,slide,media,videoInput", layers.join())
                check("with the screens across the top: a tick box for each layer this app has",
                      box("props", kept.room).available && box("slide", kept.stream).available && box("media", kept.room).checked === true
                      && box("media", kept.stream).checked === false && box("slide", kept.stream).checked === true)
                check("and ProPresenter's other layers shown as the look has them, greyed, not to be changed", box("messages", kept.room).available === false
                      && box("announcements", kept.stream).available === false && box("videoInput", kept.room).available === false
                      && box("messages", kept.room).checked === Looks.liveOf(kept.room).messages)
                testInput.grab("2-looks-window")
                kept.before = JSON.stringify(look("Lyrics L3rd").screens)
                click(centre(box("props", kept.room)))
                return 900
            },
            () => {
                check("a layer switched off in the live look is off the screen at once", Looks.liveOf(kept.room).props === false && output.propsOn === false
                      && scenes[kept.stream].propsOn === true)
                check("the saved look it came from is as it was, and the list says the live look has been changed",
                      JSON.stringify(look("Lyrics L3rd").screens) === kept.before && Looks.live.changed === true && named("lookRow:live").changed === true
                      && Looks.live.origin === kept.lyrics)
                click(centre(box("messages", kept.room)))
                check("a greyed layer does not answer to a click", Looks.liveOf(kept.room).messages === true)
                // The slides taken off the room's screen
                kept.litWithSlide = testInput.lit(false, 0, 0.12, 1, 0.88)
                click(centre(box("slide", kept.room)))
                return 900
            },
            () => {
                const lit = testInput.lit(false, 0, 0.12, 1, 0.88)
                check("the slide layer switched off in the live look: the room's screen draws no slide", Looks.liveOf(kept.room).slide === false
                      && output.slideOn === false && kept.litWithSlide > 0.01 && lit < 0.001, kept.litWithSlide.toFixed(4) + " then " + lit.toFixed(4))
                check("while the stream still has its own", scenes[kept.stream].slideOn === true)
                const logged = testInput.readText(Log.path).split("\n").filter(line => line.includes("  looks  "))
                check("what was changed, of which look and for which screen, is in the log",
                      logged.some(line => line.includes("the live look: the screen \"" + Screens.audience[0].name + "\" no longer gets the slides")),
                      logged.slice(-2).join(" | "))
                check("Save is there now that the live look is changed, where Make Live is for a saved look",
                      named("looksSave").visible === true && named("looksMakeLive").visible === false)
                testInput.grab("2b-looks-changed")
                click(centre(box("slide", kept.room)))
                return 700
            },
            () => {
                check("switched on again, the slide is back on the screen", output.slideOn === true && testInput.lit(false, 0, 0.12, 1, 0.88) > 0.01)
                // Save: the live look, as it has been changed (no props for the room), kept as the look it came from
                click(centre(named("looksSave")))
                return 700
            },
            () => {
                check("Save keeps the live look as the saved look it was made from", look("Lyrics L3rd").screens[kept.room].props === false
                      && JSON.stringify(look("Lyrics L3rd").screens) === JSON.stringify(Looks.live.screens) && Looks.live.origin === kept.lyrics
                      && Looks.live.name === "Lyrics L3rd" && Looks.looks.length === 7)
                check("after which the live look is not changed, and there is nothing to save", Looks.live.changed === false && named("looksSave").visible === false
                      && named("lookRow:live").changed === false)
                // (Put back as it was, for what follows: the props for the room again, in the live look and saved.)
                click(centre(box("props", kept.room)))
                return 700
            },
            () => {
                click(centre(named("looksSave")))
                return 700
            },
            () => {
                check("and saved again as it was", JSON.stringify(look("Lyrics L3rd").screens) === kept.before && Looks.live.changed === false && output.propsOn === true)
                click(centre(box("props", kept.room)))
                return 700
            },
            () => {
                // A saved look is picked, to be looked at
                click(centre(named("lookRow:" + kept.clear)))
                return 500
            },
            () => {
                check("picking a saved look shows what it says and makes nothing live", looksPanel.picked === kept.clear && named("looksTitle").text === "Stream Clear"
                      && Looks.live.name === "Lyrics L3rd" && output.propsOn === false && named("looksMakeLive").visible === true
                      && named("looksSave").visible === false && box("slide", kept.stream).checked === false, named("looksTitle").text)
                click(centre(box("media", kept.room)))
                return 600
            },
            () => {
                check("a saved look is changed without the screens changing", look("Stream Clear").screens[kept.room].media === false && output.mediaOn === true
                      && Looks.live.name === "Lyrics L3rd")
                click(centre(named("looksMakeLive")))
                return 1500
            },
            () => {
                check("Make Live makes it live, as it now is", Looks.live.name === "Stream Clear" && Looks.live.origin === kept.clear && Looks.live.changed === false
                      && output.mediaOn === false && output.propsOn === true && savedRows().filter(row => row.origin).map(row => row.name).join() === "Stream Clear")
                check("and it stays the one picked", looksPanel.picked === kept.clear)
                click(centre(box("media", kept.room)))
                return 600
            },
            () => {
                check("the saved look put back", look("Stream Clear").screens[kept.room].media === true && output.mediaOn === false && Looks.live.changed === true)
                // The live look, as it is, kept as a new saved look
                click(centre(named("lookRow:live")))
                click(centre(named("looksAdd")))
                return 600
            },
            () => {
                const made = Looks.looks[7]
                check("the + keeps the look that is picked as a new saved look, here the live look as it has been changed",
                      Looks.looks.length === 8 && made !== undefined && made.name === "Look" && made.screens[kept.room].media === false
                      && JSON.stringify(made.screens) === JSON.stringify(Looks.live.screens) && looksPanel.picked === made.id, made ? made.name : "none")
                check("and its name is there to be typed over", looksPanel.renaming === made.id)
                kept.made = made.id
                testInput.key(Qt.Key_A, Qt.ControlModifier)
                testInput.type("Overflow")
                testInput.key(Qt.Key_Return)
                return 500
            },
            () => {
                check("typed, it is the look's name", look("Overflow") !== undefined && look("Overflow").id === kept.made && looksPanel.renaming === "" && looksOpen)
                // A theme for the stream, in that look
                const cell = named("lookCell:theme:" + kept.stream)
                click(centre(Lib.find(cell, item => item.radius === 5 && item.visible === true)))
                return 500
            },
            () => {
                check("Presentation is the theme a screen's slides are dressed in: a click gives none, and the themes as a slide's own menu has them",
                      menu.opened && labels().startsWith("[Presentation], ") && labels().includes("No Theme") && labels().includes("Samples")
                      && labels().includes("New Life Chapel"), labels())
                menu.close()
                const two = Themes.theme("Samples/Black Box").slides.find(s => s.name === "Two Lines")
                looksPanel.set(kept.stream, { theme: "Samples/Black Box", themeSlide: two.id })
                return 500
            },
            () => {
                const gets = Looks.of(kept.made, kept.stream)
                check("a screen is given a theme in a saved look", gets.theme === "Samples/Black Box" && Themes.slide(gets.theme, gets.themeSlide).name === "Two Lines"
                      && looksPanel.themeName(gets) === "Two Lines", JSON.stringify(gets))
                testInput.grab("3-looks-saved")
                click(centre(named("looksMakeLive")))
                return 1500
            },
            () => {
                const two = Themes.slide("Samples/Black Box", Looks.of(kept.made, kept.stream).themeSlide).slide
                check("made live, the slide that is live is dressed in it at once", Looks.live.origin === kept.made && Show.lookId === kept.made
                      && worded(shownOn(scenes[kept.stream]))[0].y === worded(two)[0].y, Looks.live.name)
                // Its menu
                const row = named("lookRow:" + kept.made)
                const p = centre(row)
                testInput.mouse(0, p.x, p.y, 0, Qt.RightButton)
                testInput.mouse(2, p.x, p.y, 0, Qt.RightButton)
                return 500
            },
            () => {
                check("a saved look's menu has what can be done with it", labels() === "[Overflow], Make Live, Rename, Duplicate, Delete…", labels())
                menu.close()
                looksPanel.duplicate(kept.made)
                return 400
            },
            () => {
                const copy = look("Overflow Copy")
                check("a look is copied", copy !== undefined && JSON.stringify(copy.screens) === JSON.stringify(look("Overflow").screens) && looksPanel.picked === copy.id)
                looksPanel.remove(copy.id)
                looksPanel.remove(kept.made)
                return 500
            },
            () => {
                check("and removed. The live look is not: it is still what the screens show, and now names no saved look",
                      Looks.looks.length === 7 && look("Overflow") === undefined && Looks.live.name === "Overflow" && Looks.live.origin === "" && Show.lookId === ""
                      && looksPanel.onLive && Looks.liveOf(kept.stream).theme === "Samples/Black Box", Looks.live.name + " / " + Show.lookId)
                check("nothing here has changed what the looks say of ProPresenter's other layers", Looks.liveOf(kept.room).messages === true
                      && look("Lyrics L3rd").screens[kept.room].announcements === true)
                testInput.key(Qt.Key_Escape)
                return 400
            },
            () => {
                check("Esc puts the window away", !looksOpen && !shown("looksPanel"))
                openLooks()
                return 400
            },
            () => {
                check("it comes up again", looksOpen && shown("looksPanel"))
                click(centre(named("looksClose")))
                return 400
            },
            () => {
                check("and its own close button puts it away too", !looksOpen && !shown("looksPanel"))
                // A saved look made live and then changed by hand, as an operator might for one service
                Show.lookId = kept.lyrics
                return 800
            },
            () => {
                check("set", Looks.setScreen(Looks.live.id, kept.room, { props: false }) === "" && Looks.live.changed === true && output.propsOn === false)
                named("looksButton").clicked()
                return 500
            },
            () => {
                check("the menu of looks ticks the look the live one came from, and says nothing of the live look having been changed: that is the Looks window's to show",
                      menu.opened && Looks.live.changed === true && labels().startsWith("[Look], *Lyrics L3rd, ") && !menu.shown.some(row => row.note !== undefined), labels())
                menu.close()
                // An action that goes over to that same look puts the live look back as the saved look has it, and the log says so
                Show.lookId = kept.lyrics
                return 800
            },
            () => {
                const logged = testInput.readText(Log.path).split("\n").filter(line => line.includes("  looks  "))
                check("made live again, the saved look is what the screens get: the change made by hand is gone, which the log says",
                      Looks.live.changed === false && output.propsOn === true
                      && logged.some(line => line.includes("the live look had been changed: it is put back as the saved look \"Lyrics L3rd\" has it")),
                      logged.slice(-1).join())
                Show.lookId = kept.lyrics
                return 800
            },
            () => {
                check("a saved look made live again puts the screens back as it has them", Looks.live.name === "Lyrics L3rd" && Looks.live.changed === false
                      && output.propsOn === true && output.mediaOn === true && scenes[kept.stream].mediaOn === false)
                // Going over to a look is on the path of a click on a slide, so it is kept cheap.
                kept.file = testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace")
                Show.lookId = kept.lyrics
                return 800
            },
            () => {
                check("a look that is live already, and has not been changed, is not made live again: nothing is written",
                      testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace") === kept.file && Looks.live.name === "Lyrics L3rd")
                Show.lookId = kept.clear
                check("another look is on the screens at once, from what is in memory, before anything is written",
                      Looks.live.name === "Stream Clear" && Looks.live.origin === kept.clear && scenes[kept.stream].slideOn === false
                      && testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace") === kept.file, Looks.live.name)
                return 900
            },
            () => {
                check("and in ProPresenter's file a moment later", testInput.fileHash(catalog.workspacePath + "/Configuration/Workspace") !== kept.file
                      && Looks.live.name === "Stream Clear" && Looks.live.changed === false)
                // One more, made live as the app is closed: it is written on the way out (the runner looks at the file).
                Show.lookId = look("Notes L3rd").id
                return 20
            }
        ]
        next()
    }
}
