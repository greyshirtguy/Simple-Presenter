import QtQuick
import SimplePresenterApp
import "lib.js" as Lib

// Changing an action that is there for another of its kind (the maintainer, 2026-11-11: "editing a macro gives a popup
// sub menu to browse all macros to select a different one. Same with Looks, props, stage and clear actions"): from a
// slide's menu, Edit Action, which lists its actions, each leading to what there is to pick from with the one it is
// ticked; from the menu of the action's own icon; and for a macro's actions from the macro's menu. The action is the same
// action afterwards, by its id, naming the other. A macro ProPresenter wrote has its look and its stage layout changed
// and put back, and its file is then as ProPresenter wrote it. What the files hold is looked at in run.py
// (after_editactions).
QtObject {
    id: t

//COMMON
    function actionsOf(kind) {
        return document.slides[kept.s].actions.filter(action => action.kind === kind)
    }

    function slideMenu() {
        click(centre(slideCell(kept.s)), Qt.RightButton)
    }

    function macroNamed(name) {
        for (const collection of Macros.collections) {
            const macro = collection.macros.find(candidate => candidate.name === name)
            if (macro)
                return macro
        }
        return undefined
    }

    function logged(pattern) {
        return testInput.readText(Log.path).split("\n").filter(line => pattern.test(line))
    }

    // The looks as an action's menu lists them, the one that is `id` ticked
    function lookRows(id) {
        return "[Audience Look], " + Looks.looks.map(look => (look.id === id ? "*" : "") + look.name).join(", ")
    }

    function run() {
        steps = [
            () => {
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                return 800
            },
            () => {
                kept.s = document.slides.findIndex(slide => slide.plainText.trim() !== "" && slide.actions.length === 0)
                kept.one = Looks.looks[0]
                kept.two = Looks.looks[1]
                check("(a slide with no actions, and two looks)", kept.s >= 0 && kept.one !== undefined && kept.two !== undefined && kept.one.id !== kept.two.id)
                slideMenu()
                return 300
            },
            () => {
                check("a slide with no actions has Edit Action greyed, between Add Action and Remove Action", menu.opened
                      && labels().startsWith("Edit, Quick Edit, Add Action, Edit Action(off), Remove Action(off), "), labels())
                testInput.key(Qt.Key_Escape)
                commitAction({ slide: kept.s }, { kind: "look", lookId: kept.one.id, lookName: kept.one.name }, "")
                return 600
            },
            () => {
                kept.look = actionsOf("look")[0]
                check("(the slide goes over to the first look)", kept.look !== undefined && kept.look.lookId === kept.one.id && document.slides[kept.s].actions.length === 1)
                slideMenu()
                return 300
            },
            () => {
                check("with an action, Edit Action is there to be used", labels().startsWith("Edit, Quick Edit, Add Action, Edit Action, Remove Action, "), labels())
                click(menuRow("Edit Action"))
                return 300
            },
            () => {
                check("it lists the slide's actions, by name", labels() === "[Edit Action], " + kept.one.name, labels())
                click(menuRow(kept.one.name))
                return 300
            },
            () => {
                check("a look action leads to the looks, the one it goes over to ticked", labels() === lookRows(kept.one.id), labels())
                testInput.grab("1-edit-look")
                click(menuRow(kept.two.name))
                return 600
            },
            () => {
                const now = actionsOf("look")
                check("picking another makes it that look's: the same action, by its id, naming the other", now.length === 1 && document.slides[kept.s].actions.length === 1
                      && now[0].id === kept.look.id && now[0].lookId === kept.two.id && now[0].lookName === kept.two.name && now[0].label === kept.two.name, JSON.stringify(now))
                check("the log says an action was changed", logged(new RegExp("edit +an action changed on slide " + (kept.s + 1) + " of .*: look")).length === 1)
                // ---- From the menu of the action's own icon
                showActionMenu({ slide: kept.s }, now[0], slideCell(kept.s), 30, 30)
                return 300
            },
            () => {
                check("the menu of the action's icon has the same, as one row", labels() === "(note), Audience Look, Remove", labels())
                click(menuRow("Audience Look"))
                return 300
            },
            () => {
                check("with the look it now is ticked", labels() === lookRows(kept.two.id), labels())
                click(menuRow(kept.one.name))
                return 600
            },
            () => {
                check("and picking the first puts it back", actionsOf("look").length === 1 && actionsOf("look")[0].id === kept.look.id && actionsOf("look")[0].lookId === kept.one.id)
                // ---- A clear
                commitAction({ slide: kept.s }, { kind: "clear", layer: 5 }, "")
                return 600
            },
            () => {
                kept.clear = actionsOf("clear")[0]
                check("(the slide clears the slide too)", kept.clear !== undefined && kept.clear.layer === 5 && document.slides[kept.s].actions.length === 2)
                slideMenu()
                return 300
            },
            () => {
                click(menuRow("Edit Action"))
                return 300
            },
            () => {
                check("Edit Action lists both, each by its name", labels() === "[Edit Action], " + kept.one.name + ", " + actionLabel(kept.clear), labels())
                click(menuRow(actionLabel(kept.clear)))
                return 300
            },
            () => {
                check("a clear leads to what can be cleared, what it clears ticked",
                      labels() === "[Clear], Everything, *The Slide, The Media, The Props, The Messages, The Announcements, The Video Input", labels())
                click(menuRow("The Announcements"))
                return 600
            },
            () => {
                const now = actionsOf("clear")[0]
                check("changed to the announcements, it clears those", now.id === kept.clear.id && now.announcements === true && now.picture === "ClearAnnouncements", JSON.stringify(now))
                showActionMenu({ slide: kept.s }, now, slideCell(kept.s), 30, 30)
                return 300
            },
            () => {
                check("(its icon's menu)", labels() === "(note), Clear, Remove", labels())
                click(menuRow("Clear"))
                return 300
            },
            () => {
                check("the announcements ticked now", labels() === "[Clear], Everything, The Slide, The Media, The Props, The Messages, *The Announcements, The Video Input", labels())
                click(menuRow("The Media"))
                return 600
            },
            () => {
                const now = actionsOf("clear")[0]
                check("and changed to the media, it clears the media, of the show and not of the announcements", now.id === kept.clear.id && now.layer === 2
                      && now.announcements !== true && now.picture === "ClearMedia", JSON.stringify(now))
                // ---- A macro
                kept.macros = Macros.collections.reduce((all, collection) => all.concat(collection.macros.map(macro => ({ macro: macro, collection: collection }))), [])
                check("(two macros to pick between)", kept.macros.length >= 2, kept.macros.length)
                commitAction({ slide: kept.s }, macroAction(kept.macros[0].macro, kept.macros[0].collection), "")
                return 600
            },
            () => {
                kept.macro = actionsOf("macro")[0]
                check("(the slide runs the first macro)", kept.macro !== undefined && kept.macro.macroId === kept.macros[0].macro.id)
                slideMenu()
                return 300
            },
            () => {
                click(menuRow("Edit Action"))
                return 300
            },
            () => {
                click(menuRow(kept.macros[0].macro.name))
                return 300
            },
            () => {
                check("a macro action leads to the macros' collections", labels() === "[Macro], " + Macros.collections.filter(collection => collection.macros.length > 0)
                      .map(collection => collection.name).join(", "), labels())
                click(menuRow(kept.macros[1].collection.name))
                return 300
            },
            () => {
                const expected = "[" + kept.macros[1].collection.name + "], " + kept.macros[1].collection.macros.map(macro => (macro.id === kept.macros[0].macro.id ? "*" : "") + macro.name).join(", ")
                check("and a collection to its macros, the one it runs ticked", labels() === expected, labels() + " / " + expected)
                click(menuRow(kept.macros[1].macro.name))
                return 600
            },
            () => {
                const now = actionsOf("macro")[0]
                check("picking another makes it run that one", now.id === kept.macro.id && now.macroId === kept.macros[1].macro.id && now.macroName === kept.macros[1].macro.name, JSON.stringify(now))
                // ---- A prop: which, and whether it is put on or taken off
                kept.props = Props.collections.reduce((all, collection) => all.concat(collection.props.map(prop => ({ prop: prop, collection: collection }))), [])
                check("(a prop)", kept.props.length >= 1, kept.props.length)
                const first = kept.props[0]
                commitAction({ slide: kept.s }, { kind: "prop", propId: first.prop.id, propName: first.prop.name, collectionId: first.collection.id,
                                                  collectionName: first.collection.id === "" ? "" : first.collection.name, clear: false }, "")
                return 600
            },
            () => {
                kept.prop = actionsOf("prop")[0]
                check("(the slide puts the prop on)", kept.prop !== undefined && kept.prop.propId === kept.props[0].prop.id && kept.prop.clear === false)
                showActionMenu({ slide: kept.s }, kept.prop, slideCell(kept.s), 30, 30)
                return 300
            },
            () => {
                check("a prop's icon has the props, and whether it is put on or taken off, which it is ticked", labels() === "(note), Prop, --, *Trigger, Clear, Remove", labels())
                click(menuRow("Clear"))
                return 600
            },
            () => {
                const now = actionsOf("prop")[0]
                check("Clear makes it take the prop off", now.id === kept.prop.id && now.clear === true && now.propId === kept.props[0].prop.id && now.label.endsWith("(Clear)"), JSON.stringify(now))
                slideMenu()
                return 300
            },
            () => {
                click(menuRow("Edit Action"))
                return 300
            },
            () => {
                check("Edit Action has all four", labels().split(", ").length === 5, labels())
                click(menuRow(actionLabel(actionsOf("prop")[0])))
                return 300
            },
            () => {
                check("a prop action leads to the props' collections, and to Trigger and Clear", labels() === "[Prop], " + Props.collections.filter(collection => collection.props.length > 0)
                      .map(collection => collection.name).join(", ") + ", --, Trigger, *Clear", labels())
                click(menuRow(kept.props[kept.props.length - 1].collection.name))
                return 300
            },
            () => {
                const last = kept.props[kept.props.length - 1]
                check("a collection to its props, the one it is ticked", labels() === "[" + last.collection.name + "], "
                      + last.collection.props.map(prop => (prop.id === kept.props[0].prop.id ? "*" : "") + prop.name).join(", "), labels())
                click(menuRow(last.prop.name))
                return 600
            },
            () => {
                const now = actionsOf("prop")[0]
                const last = kept.props[kept.props.length - 1]
                check("picking a prop makes it that prop's, taken off as it was", now.id === kept.prop.id && now.propId === last.prop.id && now.clear === true, JSON.stringify(now))
                // ---- A macro ProPresenter wrote: its look and its stage layout, changed from the macro's menu and put back
                kept.file = catalog.workspacePath + "/Configuration/Macros"
                kept.hash = testInput.fileHash(kept.file)
                kept.singing = macroNamed("Singing")
                kept.goes = kept.singing.actions.find(action => action.kind === "look")
                kept.stage = kept.singing.actions.find(action => action.kind === "stage")
                check("(ProPresenter's macro Singing: a look and a stage layout among its actions)", kept.goes !== undefined && kept.stage !== undefined
                      && kept.goes.lookName === "Lyrics L3rd", kept.singing.actions.map(action => action.kind).join(" "))
                sidePanel.showControlTab = "macros"
                return 400
            },
            () => {
                kept.panel = named("macroList").parent
                kept.panel.showMacroMenu(kept.singing, named("macroList"), 20, 20)
                return 300
            },
            () => {
                check("a macro's menu lists its actions that can be changed: the stage layout, the look and its clears now among them",
                      labels().startsWith("Run, Add Action, Remove Action, [Actions], ") && labels().includes(", " + kept.stage.label + ", ")
                      && labels().includes(", Lyrics L3rd, ") && kept.singing.actions.filter(action => action.kind === "clear").every(action => labels().includes(", " + actionLabel(action) + ", ")),
                      labels())
                click(menuRow("Lyrics L3rd"))
                return 300
            },
            () => {
                check("its look action leads to the looks, with Remove after them", labels() === lookRows(Looks.looks.find(look => look.name === "Lyrics L3rd").id) + ", Remove", labels())
                testInput.grab("2-macro-look")
                click(menuRow("Stream Clear"))
                return 600
            },
            () => {
                const now = macroNamed("Singing").actions.find(action => action.id === kept.goes.id)
                check("picking another changes the macro's action", now !== undefined && now.lookName === "Stream Clear" && macroNamed("Singing").actions.length === kept.singing.actions.length,
                      JSON.stringify(now))
                check("in its file", testInput.fileHash(kept.file) !== kept.hash)
                kept.panel.showMacroMenu(macroNamed("Singing"), named("macroList"), 20, 20)
                return 300
            },
            () => {
                click(menuRow("Stream Clear"))
                return 300
            },
            () => {
                click(menuRow("Lyrics L3rd"))
                return 600
            },
            () => {
                check("put back, the macros' file is as ProPresenter wrote it, byte for byte", testInput.fileHash(kept.file) === kept.hash)
                kept.panel.showMacroMenu(macroNamed("Singing"), named("macroList"), 20, 20)
                return 300
            },
            () => {
                click(menuRow(kept.stage.label))
                return 300
            },
            () => {
                // (This workspace has more than one stage screen: each has its own row of layouts)
                check("its stage action leads to the stage screens, then its panel and Remove", Screens.stage.length > 1
                      && labels() === "[Stage], " + Screens.stage.map(screen => screen.name).join(", ") + ", Edit…, Remove", labels())
                kept.screen = Screens.stage.find(screen => Show.stageLayoutsOf(kept.stage)[screen.id] !== undefined)
                kept.layout = Show.stageLayoutsOf(kept.stage)[kept.screen.id]
                click(menuRow(kept.screen.name))
                return 300
            },
            () => {
                check("a screen to the stage layouts, the one the action gives it ticked, and No Change", labels() === "[" + kept.screen.name + "], No Change, "
                      + StageLayouts.layouts.map(layout => (layout.id === kept.layout.id ? "*" : "") + layout.name).join(", "), labels())
                kept.other = StageLayouts.layouts.find(layout => layout.id !== kept.layout.id)
                click(menuRow(kept.other.name))
                return 600
            },
            () => {
                const now = macroNamed("Singing").actions.find(action => action.id === kept.stage.id)
                const given = Show.stageLayoutsOf(now)
                check("picking another gives that screen that layout, and the other screens what they had", given[kept.screen.id].id === kept.other.id
                      && Screens.stage.every(screen => screen.id === kept.screen.id
                                             || (given[screen.id] ? given[screen.id].id : "") === (Show.stageLayoutsOf(kept.stage)[screen.id] ? Show.stageLayoutsOf(kept.stage)[screen.id].id : "")),
                      JSON.stringify(now.assignments))
                kept.panel.showMacroMenu(macroNamed("Singing"), named("macroList"), 20, 20)
                return 300
            },
            () => {
                click(menuRow(macroNamed("Singing").actions.find(action => action.id === kept.stage.id).label))
                return 300
            },
            () => {
                click(menuRow(kept.screen.name))
                return 300
            },
            () => {
                check("(the other layout ticked now)", labels().includes("*" + kept.other.name), labels())
                click(menuRow(kept.layout.name))
                return 600
            },
            () => {
                check("put back, the file is as ProPresenter wrote it again", testInput.fileHash(kept.file) === kept.hash)
                const wrong = logged(/PROBLEM|WARNING|ERROR/).filter(line => !line.includes("qt.qpa.theme"))
                check("nothing went wrong on the way", wrong.length === 0, wrong.join(" | "))
            }
        ]
        next()
    }
}
