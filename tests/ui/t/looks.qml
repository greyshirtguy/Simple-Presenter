import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Looks: ProPresenter's own, read from the workspace; which layers each audience screen
// gets under the look that is live; a screen's slides dressed in the theme a look gives
// it, with the presentation left as it was; a look made live by hand and by an action;
// and the Looks section of the settings.
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

    function lookRows() {
        return Lib.findAll(named("looksSettings"), item => String(item.objectName) === "lookRow")
    }

    function lookLines() {
        return Lib.findAll(named("looksSettings"), item => String(item.objectName) === "lookLine")
    }

    function run() {
        steps = [
            () => {
                check("ProPresenter's looks are read from the workspace's file", Looks.looks.length === 7 && look("Lyrics L3rd") !== undefined
                      && look("Stream Clear") !== undefined, JSON.stringify(Looks.looks.map(l => l.name)))
                check("and the one that was live there is live here to start with", Looks.startsWith === look("Lyrics L3rd").id && Show.lookId === Looks.startsWith)
                check("its themes are found in the workspace, by their place under Themes", Themes.themes.length >= 10 && Themes.theme("New Life Chapel") !== null
                      && Themes.theme("Samples/Black Box") !== null && Themes.tree.some(e => e.kind === "folder" && e.name === "Samples"),
                      Themes.themes.length)
                kept.room = Screens.audience[0].id
                kept.stream = Screens.audience[1].id
                const gets = Looks.of(Show.lookId, kept.stream)
                check("a look gives each screen its layers and its theme", gets.slide === true && gets.media === false && gets.props === true
                      && gets.theme === "New Life Chapel" && Themes.slide(gets.theme, gets.themeSlide).name === "Lower 3rd", JSON.stringify(gets))
                kept.lower = Themes.slide(gets.theme, gets.themeSlide).slide
                const room = Looks.of(Show.lookId, kept.room)
                kept.upper = Themes.slide(room.theme, room.themeSlide).slide
                check("the room has another theme slide", room.media === true && Themes.slide(room.theme, room.themeSlide).name === "Upper 3rd")
                // The stream, which nothing is said about on this computer, is given a window
                Screens.setOutput(kept.stream, { output: "window" })
                return 800
            },
            () => {
                check("each screen's scene has the layers the look gives it", output.slideOn && output.mediaOn && output.propsOn
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
                testInput.grabOutput("1-room")
                // ---- another look
                Show.lookId = look("Stream Clear").id
                return 1800
            },
            () => {
                check("made live, another look gives the screens other layers", Show.lookId === look("Stream Clear").id && scenes[kept.stream].slideOn === false
                      && scenes[kept.stream].propsOn === true && output.slideOn === true && output.mediaOn === true)
                check("and, this one having no theme for the room, the room's slide is shown again as it is",
                      worded(shownOn(output))[0].y === worded(document.slides[kept.at])[0].y, worded(shownOn(output))[0].y)
                check("how long the layers take to come and go is the look's", lookFade === 1000 && output.lookFade === 1000)
                // ---- an action
                commitAction({ slide: kept.at + 1 }, { kind: "look", lookId: look("Lyrics L3rd").id, lookName: "Lyrics L3rd" }, "")
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
                check("shown, the slide's action makes that look live", Show.lookId === look("Lyrics L3rd").id && scenes[kept.stream].slideOn === true)
                check("and the slide is on each screen in that look's theme", worded(shownOn(output))[0].y === worded(kept.upper)[0].y
                      && worded(shownOn(scenes[kept.stream]))[0].y === worded(kept.lower)[0].y)
                const target = kept.lookMacro.actions.find(a => a.kind === "look")
                Show.lookId = ""
                check("with no look at all, every screen gets everything", scenes[kept.stream].slideOn && scenes[kept.stream].mediaOn && scenes[kept.stream].propsOn)
                runMacro(kept.lookMacro.id)
                check("a macro's action does the same", Show.lookId === look(target.lookName).id, target.lookName)
                // ---- the settings
                settingsOpen = true
                Lib.find(contentItem, item => item.section !== undefined && item.sections !== undefined).section = "looks"
                return 500
            },
            () => {
                check("the settings list the looks, the live one marked, with a line for each audience screen", lookRows().length === 7 && lookLines().length === 2
                      && named("looksSettings").chosen === Show.lookId)
                testInput.grab("2-settings")
                named("addLook").clicked()
                return 500
            },
            () => {
                const made = Looks.looks[7]
                check("a look is added, giving every screen everything, and is the one shown", Looks.looks.length === 8 && made.name === "Look"
                      && named("looksSettings").chosen === made.id && Looks.of(made.id, kept.stream).media === true && Looks.of(made.id, kept.stream).theme === "")
                kept.made = made.id
                Lib.find(lookLines()[1], item => String(item.objectName) === "look-media").toggled(false)
                return 400
            },
            () => {
                check("a switch takes a layer from a screen in that look, and no other", Looks.of(kept.made, kept.stream).media === false
                      && Looks.of(kept.made, kept.stream).slide === true && Looks.of(kept.made, kept.room).media === true)
                const choice = Lib.find(lookLines()[1], item => String(item.objectName) === "lookTheme")
                kept.choice = choice.model.findIndex(label => label === "Black Box — Two Lines")
                check("every slide of every theme can be chosen for a screen", kept.choice > 0 && choice.currentIndex === 0, choice.model.length)
                choice.activated(kept.choice)
                return 400
            },
            () => {
                const gets = Looks.of(kept.made, kept.stream)
                check("a screen is given a theme", gets.theme === "Samples/Black Box" && Themes.slide(gets.theme, gets.themeSlide).name === "Two Lines", JSON.stringify(gets))
                named("lookMakeLive").clicked()
                return 1500
            },
            () => {
                const two = Themes.slide("Samples/Black Box", Looks.of(kept.made, kept.stream).themeSlide).slide
                check("made live from the settings, it is", Show.lookId === kept.made && scenes[kept.stream].mediaOn === false && output.mediaOn === true)
                check("and the slide that is live is dressed in its theme at once", worded(shownOn(scenes[kept.stream]))[0].y === worded(two)[0].y
                      && worded(shownOn(output))[0].y === worded(document.slides[liveIndex])[0].y)
                check("a look is renamed", Looks.rename(kept.made, "Overflow") === "" && look("Overflow") !== undefined)
                named("lookRemove").clicked()
                return 500
            },
            () => {
                check("and removed; no longer there, it is no longer live", Looks.looks.length === 7 && Show.lookId === "" && look("Overflow") === undefined)
                settingsOpen = false
                return 200
            }
        ]
        next()
    }
}
