import QtQuick
import QtQuick.Window
import SimplePresenterApp
import "lib.js" as Lib

// Actions on slides and in macros (timers, clearing, the stage, props, macros), the macros tab, the icons for a slide's
// media and how it plays, their menus, and dragging the show controls onto slides.
QtObject {
    id: t

//COMMON
    function slideWith(test) {
        return document.slides.findIndex(test)
    }

    function kinds(index) {
        return document.slides[index].actions.map(a => a.kind).join(",")
    }

    function badges(index, name) {
        return Lib.findAll(slideCell(index), i => i.objectName === name)
    }

    function near(a, b) {
        return Math.abs(a - b) <= 1.5
    }

    function inFile(index) {
        return catalog.open(document.path).slides[index].actions
    }

    function panelOf(name) {
        return Lib.find(win.contentItem, i => i.objectName === name)
    }

    function run() {
        steps = [
            () => {
                transitionDuration = 0
                const names = Macros.collections[0].macros.map(m => m.name)
                check("the workspace's macros are read, in their collection", Macros.collections.length === 1 && Macros.collections[0].name === "Default Collection"
                      && names.slice(0, 2).join("|") === "Singing|Stream & Stage Notes", names.join("|"))
                const singing = Macros.find("", "Singing")
                check("a macro is its actions, each said in words, and whether it is one done here", singing.actions.map(a => a.kind + (a.done ? "" : "-")).join(" ") === "stage other- clear- clear clear-"
                      && singing.actions[0].title === "Give the stage the layout “Singing”" && singing.actions[3].title === "Clear the props"
                      && singing.actions[1].title === "Audience look: Lyrics L3rd", singing.actions.map(a => a.title).join(" | "))
                check("ProPresenter's stage screens are known by name", Screens.stage.length === 2 && stageScreen().id === "E3A36D8C-E70D-45D8-A127-E7DAC6577CDC", JSON.stringify(StageLayouts.screens))
                openLibrary(catalog.libraries[0].path)
                openEntry(entry("All Hail King Jesus"))
                return 600
            },
            () => {
                kept.macroSlide = slideWith(s => s.actions.some(a => a.kind === "macro" && a.macroName === "Singing"))
                check("a slide's own actions come with it", kept.macroSlide >= 0 && kinds(kept.macroSlide).includes("macro"), kept.macroSlide + ": " + (kept.macroSlide >= 0 ? kinds(kept.macroSlide) : ""))
                const icons = badges(kept.macroSlide, "actionBadge")
                const glyph = icons.length > 0 ? Lib.find(icons[0], i => i.kind !== undefined) : null
                check("and each has an icon in the slide's corner, of its kind", icons.length === document.slides[kept.macroSlide].actions.length && glyph !== null && glyph.kind === "macro"
                      && icons[0].width === 22 && Math.abs(icons[0].opacity - 0.8) < 0.01, icons.length + " icons")
                // ---- a slide's macro is run when the slide goes live
                const prop = Props.collections[0].props[0]
                toggleProp(prop.id)
                Show.stageLayoutId = ""
                kept.prop = prop
                check("a prop is on and the stage has the plain view, to begin with", liveProps.length === 1 && stageLayout === null)
                goLive(kept.macroSlide)
                return 500
            },
            () => {
                check("going live, the slide ran its macro: the stage has the layout the macro gives it", stageLayout !== null && stageLayout.name === "Singing", stageLayout ? stageLayout.name : "none")
                check("and the props are cleared, as the macro also says", liveProps.length === 0)
                check("the slide itself is on the output", liveIndex === kept.macroSlide && !cleared)
                const lines = testInput.readText(Log.path).split("\n")
                check("the log says what was run, and what of it is not done here", lines.some(l => l.includes("Run the macro “Singing”, which has 5 actions"))
                      && lines.some(l => l.includes("Audience look: Lyrics L3rd: not done here")) && lines.some(l => l.includes("Clear the audio: not done here")))
                // ---- adding actions: the menu
                kept.target = slideWith(s => s.actions.length === 0 && s.mediaName === "")
                showSlideMenu(kept.target, slideCell(kept.target), 30, 30)
                return 300
            },
            () => {
                check("a slide's menu offers to add an action, and to remove one, under Edit", labels().startsWith("Edit, Add Action, Remove Action(off), [Media]"), labels())
                // The pointer resting on the row opens its menu beside it
                const row = menuRow("Add Action")
                testInput.mouse(4, row.x, row.y)
                return 500
            },
            () => {
                const panes = Lib.findAll(menu.contentItem, i => i.px !== undefined && i.rows !== undefined)
                kept.panes = panes
                check("resting the pointer on Add Action opens its menu, of five kinds", labels() === "[Add Action], Timer…, Clear, Stage…, Prop, Macro", labels())
                check("beside the slide's menu, which stays where it is", panes.length === 2 && menu.opened && near(panes[1].x, panes[0].x + panes[0].width - 4)
                      && panes[1].y >= panes[0].y && panes[1].x + panes[1].width <= win.width, panes.map(p => p.x + "," + p.y).join(" "))
                const row = menuRow("Clear")
                testInput.mouse(4, row.x, row.y)
                return 500
            },
            () => {
                const panes = Lib.findAll(menu.contentItem, i => i.px !== undefined && i.rows !== undefined)
                check("clearing: the layers there are here, in a third menu", labels() === "[Clear], Everything, The Slide, The Media, The Props" && panes.length === 3, labels())
                check("every menu is inside the window", panes.every(p => p.x >= 0 && p.y >= 0 && p.x + p.width <= win.width && p.y + p.height <= win.height))
                testInput.grab("0-menus")
                // Back on another row of the second menu, the third shuts
                const other = menuRow("Stage…")
                testInput.mouse(4, other.x, other.y)
                return 500
            },
            () => {
                check("resting on another row shuts the menu that was open from the first", Lib.findAll(menu.contentItem, i => i.px !== undefined && i.rows !== undefined).length === 2
                      && labels().startsWith("[Add Action]"), labels())
                click(menuRow("Clear"))
                return 300
            },
            () => {
                click(menuRow("The Props"))
                return 500
            },
            () => {
                const a = document.slides[kept.target].actions
                check("the slide has the action, and its icon", a.length === 1 && a[0].kind === "clear" && a[0].layer === 4 && a[0].title === "Clear the props"
                      && badges(kept.target, "actionBadge").length === 1, JSON.stringify(a.map(x => x.title)))
                check("and it is in the file", inFile(kept.target).length === 1 && inFile(kept.target)[0].id === a[0].id)
                // ---- a timer action, in its panel
                showAddActionMenu({ slide: kept.target }, slideCell(kept.target), 30, 30)
                return 300
            },
            () => {
                click(menuRow("Timer…"))
                return 400
            },
            () => {
                const dialog = Lib.find(win.contentItem, i => i.openTimer !== undefined)
                kept.dialog = dialog
                check("a timer action is set up in a small panel, which starts on the first timer and Start", dialog.visible && dialog.mode === "timer" && dialog.timerName === Timers.timers[0].name
                      && dialog.timerAction === 0 && !dialog.timerSet && named("actionTimer").model.length === Timers.timers.length, dialog.timerName)
                testInput.grab("1-timer-dialog")
                const second = Timers.timers[1]
                dialog.timerId = second.id
                dialog.timerName = second.name
                dialog.timerAction = 3
                dialog.timerSet = true
                dialog.setKind = "countdown"
                dialog.setDuration = 90
                kept.timer = second
                return 200
            },
            () => {
                testInput.grab("2-timer-dialog-set")
                named("actionAccept").clicked()
                return 500
            },
            () => {
                const a = document.slides[kept.target].actions
                const made = a[1]
                kept.timerAction = made
                check("accepted, the slide has a second action: that timer, reset and started, set to a countdown of a minute and a half",
                      !kept.dialog.visible && a.length === 2 && made.kind === "timer" && made.timerId === kept.timer.id && made.timerName === kept.timer.name && made.action === 3
                      && made.set && made.setKind === "countdown" && made.setDuration === 90, JSON.stringify(made))
                check("in words", made.title === "Reset and start the timer “" + kept.timer.name + "”, set anew", made.title)
                Timers.reset(kept.timer.id)
                goLive(kept.target)
                return 700
            },
            () => {
                const state = Timers.state(kept.timer.id)
                check("going live, the timer is set to that and running", state.running && state.seconds > 85 && state.seconds <= 90, state.seconds)
                // ---- changing it
                const icon = badges(kept.target, "actionBadge")[1]
                kept.icon = icon
                // A right click on the icon is the icon's
                click(centre(icon), Qt.RightButton)
                return 300
            },
            () => {
                check("a right click on an action's icon is for that action", menu.opened && labels() === "(note), Change…, Remove", labels())
                click(menuRow("Change…"))
                return 400
            },
            () => {
                check("Change opens the panel as the action is", kept.dialog.mode === "timer" && kept.dialog.timerName === kept.timer.name && kept.dialog.timerAction === 3 && kept.dialog.timerSet
                      && kept.dialog.setDuration === 90 && kept.dialog.existing !== null)
                kept.dialog.timerAction = 1
                kept.dialog.timerSet = false
                kept.dialog.accept()
                return 500
            },
            () => {
                const made = document.slides[kept.target].actions[1]
                check("and saves it as changed, the same action", made.id === kept.timerAction.id && made.action === 1 && !made.set && made.title === "Stop the timer “" + kept.timer.name + "”", made.title)
                goLive(0)
                goLive(kept.target)
                check("which now stops the timer", !Timers.state(kept.timer.id).running)
                // ---- a stage action
                Show.stageLayoutId = ""
                kept.dialog.openStage({ slide: kept.target }, null)
                return 400
            },
            () => {
                const choice = named("actionStageLayout")
                check("a stage action's panel has the stage screen, and No Change or a layout for it", kept.dialog.mode === "stage" && choice.model[0] === "-- No Change --"
                      && choice.model.length === StageLayouts.layouts.length + 1 && choice.currentIndex === 0, choice.model.join("|"))
                testInput.grab("3-stage-dialog")
                kept.layout = StageLayouts.layouts[1]
                kept.dialog.layoutId = kept.layout.id
                kept.dialog.accept()
                return 500
            },
            () => {
                const made = document.slides[kept.target].actions[2]
                check("the action names every stage screen ProPresenter has, the first with the layout and the other left as it is", made.kind === "stage" && made.assignments.length === 2
                      && made.assignments[0].screenId === stageScreen().id && made.assignments[0].layoutId === kept.layout.id && made.assignments[0].layoutName === kept.layout.name
                      && made.assignments[1].layoutId === "" && made.assignments[1].layoutName === "", JSON.stringify(made.assignments))
                goLive(0)
                goLive(kept.target)
                check("and going live gives the stage that layout", stageLayoutId === kept.layout.id)
                // ---- a prop action: put on, and taken off
                const collection = Props.collections[0]
                showPropActionMenu({ slide: kept.target }, kept.prop, collection, slideCell(kept.target), 30, 30)
                return 300
            },
            () => {
                check("a prop is triggered by an action, or cleared", labels() === "[" + kept.prop.name + "], Trigger, Clear", labels())
                click(menuRow("Trigger"))
                return 500
            },
            () => {
                const made = document.slides[kept.target].actions[3]
                check("the action says which prop, and of which collection", made.kind === "prop" && made.propId === kept.prop.id && !made.clear && made.collectionName === Props.collections[0].name
                      && made.title === "Show the prop “" + kept.prop.name + "”", made.title)
                // The slide clears the props first, and then puts this one on
                toggleProp(Props.collections[0].props[1].id)
                goLive(0)
                goLive(kept.target)
                check("going live does the actions in order: the props cleared, then this one on", liveProps.length === 1 && liveProps[0] === kept.prop.id, JSON.stringify(liveProps))
                check("four actions, four icons", badges(kept.target, "actionBadge").length === 4)
                slideCell(kept.target)
                return 300
            },
            () => {
                testInput.grab("4-icons")
                // ---- taking one away
                showActionMenu({ slide: kept.target }, document.slides[kept.target].actions[0], slideCell(kept.target), 30, 30)
                return 300
            },
            () => {
                check("a clear action has nothing to change, only to remove", labels() === "(note), Remove", labels())
                click(menuRow("Remove"))
                return 500
            },
            () => {
                check("removed, the others are as they were", kinds(kept.target) === "timer,stage,prop" && inFile(kept.target).length === 3, kinds(kept.target))
                showSlideMenu(kept.target, slideCell(kept.target), 30, 30)
                return 300
            },
            () => {
                click(menuRow("Remove Action"))
                return 300
            },
            () => {
                const titles = document.slides[kept.target].actions.map(a => a.title)
                check("the slide's menu lists its actions to remove", labels() === "[Remove Action], " + titles.join(", "), labels())
                testInput.key(Qt.Key_Escape)
                check("Esc shuts the menus", !menu.opened)
                // ---- the macros tab
                sidePanel.showControlTab = "macros"
                return 400
            },
            () => {
                const list = named("macroList")
                const rows = Lib.findAll(list, i => i.objectName === "macroRow")
                check("the show controls have a tab of macros, by collection", list !== null && list.visible && rows.length >= 4 && list.model[0].heading && list.model[0].name === "Default Collection", rows.length)
                testInput.grab("5-macros")
                // a new macro
                const panel = list.parent
                kept.panel = panel
                panel.add("")
                return 400
            },
            () => {
                const made = Macros.collections[0].macros[Macros.collections[0].macros.length - 1]
                kept.macro = made
check("a new macro is added to the collection, with its name ready to be typed", made.name === "Macro" && made.actions.length === 0 && kept.panel.renaming === made.id, made.name)
                kept.panel.rename({ id: made.id, name: made.name }, "House Lights")
                check("and is renamed", Macros.find(made.id).name === "House Lights")
                commitAction({ macro: made.id }, { kind: "clear", layer: 4 }, "")
                commitAction({ macro: made.id }, { kind: "stage", assignments: stageAssignments(null, StageLayouts.layouts[0]) }, "")
                const now = Macros.find(made.id)
                check("it is given actions as a slide is", now.actions.map(a => a.title).join(" | ") === "Clear the props | Give the stage the layout “" + StageLayouts.layouts[0].name + "”", now.actions.map(a => a.title).join(" | "))
                check("which are drawn on it", kept.panel.rows.find(r => r.id === made.id).actions.length === 2)
                setProp(kept.prop.id, true)
                Show.stageLayoutId = ""
                const before = liveProps.length
                kept.panel.run(made.id)
                check("a click runs it: the props cleared, the stage given the layout", before >= 1 && liveProps.length === 0 && stageLayoutId === StageLayouts.layouts[0].id)
                return 300
            },
            () => {
                testInput.grab("6-macro-open")
                // a macro that runs itself, by way of another, does not go round for ever
                const other = Macros.add("")
                commitAction({ macro: other.id }, { kind: "macro", macroId: kept.macro.id, macroName: "House Lights" }, "")
                commitAction({ macro: kept.macro.id }, { kind: "macro", macroId: other.id, macroName: Macros.find(other.id).name }, "")
                runMacro(kept.macro.id)
                const lines = testInput.readText(Log.path).split("\n")
                check("macros that run each other are stopped, and it is said", lines.some(l => l.includes("macros that run each other have gone round eight times")))
                win.report(Macros.remove(other.id))
                const left = Macros.find(kept.macro.id)
                win.report(Macros.removeAction(kept.macro.id, left.actions[left.actions.length - 1].id))
                check("an action is taken out of a macro, and a macro removed", Macros.find(other.id).id === undefined && Macros.find(kept.macro.id).actions.length === 2)
                named("macroList").positionViewAtEnd()
                return 400
            },
            () => {
                // ---- dragging out of the show controls onto a slide
                kept.drop = slideWith((s, i) => i > kept.target && s.actions.length === 0)
                const row = Lib.findAll(named("macroList"), i => i.objectName === "macroRow").find(r => r.modelData.id === kept.macro.id)
                const from = row.mapToItem(null, row.width / 2, row.height / 2)
                const to = centre(slideCell(kept.drop))
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 12; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
                kept.to = to
                // (What is dragged says where it is on the next turn of the event loop.)
                return 200
            },
            () => {
                kept.overDrop = Lib.find(slideCell(kept.drop), i => i.zone !== undefined && i.containsDrag !== undefined).onto
                testInput.grab("6b-dragging")
                testInput.mouse(2, kept.to.x, kept.to.y)
                return 600
            },
            () => {
                const a = document.slides[kept.drop].actions
                check("a macro dragged onto a slide is marked on the slide as it comes over it", kept.overDrop === true)
                check("and dropped gives the slide the action that runs it, without running it", a.length === 1 && a[0].kind === "macro" && a[0].macroId === kept.macro.id && a[0].macroName === "House Lights"
                      && a[0].collectionName === "Default Collection" && liveIndex === kept.target, JSON.stringify(a.map(x => x.title)))
                sidePanel.showControlTab = "timers"
                return 400
            },
            () => {
                // a timer dragged: the panel comes up for it
                const rows = Lib.findAll(sidePanel, i => i.timer !== undefined && i.state !== undefined && i.timer.id === kept.timer.id)
                const from = rows[0].mapToItem(null, 60, 14)
                const to = centre(slideCell(kept.drop))
                testInput.mouse(0, from.x, from.y)
                for (let i = 1; i <= 12; ++i)
                    testInput.mouse(1, from.x + (to.x - from.x) * i / 12, from.y + (to.y - from.y) * i / 12)
                testInput.mouse(2, to.x, to.y)
                return 500
            },
            () => {
                check("a timer dragged onto a slide brings up the panel, for that timer", kept.dialog.mode === "timer" && kept.dialog.timerId === kept.timer.id && kept.dialog.target.slide === kept.drop, kept.dialog.mode + " " + kept.dialog.timerName)
                testInput.key(Qt.Key_Escape)
                check("and Esc leaves the slide as it was", kept.dialog.mode === "" && document.slides[kept.drop].actions.length === 1)
                // ---- the media of a slide: its two icons and their menus
                openEntry(entry("When Wind Meets Fire"))
                return 600
            },
            () => {
                kept.video = slideWith(s => s.mediaName !== "" && s.mediaVideo)
                const s = document.slides[kept.video]
                check("a slide with a video says how it plays on from its end", kept.video >= 0 && s.mediaPlayback === 1 && !s.mediaForeground, kept.video + " " + (s ? s.mediaPlayback : ""))
                const playback = badges(kept.video, "playbackBadge")
                const media = badges(kept.video, "mediaBadge")
                check("with an icon for it under the one for its behaviour", playback.length === 1 && media.length === 1 && Lib.find(playback[0], i => i.kind !== undefined).kind === "loop"
                      && near(playback[0].mapToItem(null, 0, 0).x, media[0].mapToItem(null, 0, 0).x) && near(playback[0].mapToItem(null, 0, 0).y, media[0].mapToItem(null, 0, 0).y + 21), playback.length + " " + media.length)
                click(centre(playback[0]), Qt.RightButton)
                return 300
            },
            () => {
                check("a right click on that icon offers the ways of playing, the one it has ticked", labels() === "[At Its End], Stop, *Loop, Loop for Play Count, Loop for Time", labels())
                click(menuRow("Loop for Play Count"))
                return 300
            },
            () => {
                check("a play count is chosen from a few", labels() === "[Play Count], 2 times, 3 times, 4 times, 5 times, 10 times, 20 times", labels())
                click(menuRow("3 times"))
                return 600
            },
            () => {
                const s = document.slides[kept.video]
                const icon = badges(kept.video, "playbackBadge")[0]
                check("chosen, the slide's video plays three times", s.mediaPlayback === 2 && s.mediaLoopCount === 3 && s.media.loops && s.media.playback === 2 && s.media.loopCount === 3, s.mediaPlayback + " " + s.mediaLoopCount)
                check("and the icon says so", icon.words === "×3" && icon.width > 18, icon.words)
                check("in the file too", catalog.open(document.path).slides[kept.video].mediaLoopCount === 3)
                goLive(kept.video)
                return 2500
            },
            () => {
                check("on the output it is played that many times", player() !== null && player().loops === 3, player() ? player().loops : "no player")
                click(centre(badges(kept.video, "mediaBadge")[0]), Qt.RightButton)
                return 300
            },
            () => {
                check("a right click on the other icon is the media's behaviour", labels() === "[Media], *Background, Foreground", labels())
                click(menuRow("Foreground"))
                return 600
            },
            () => {
                const s = document.slides[kept.video]
                check("made a foreground, it is one, and plays on from its end as it did", s.mediaForeground && s.mediaPlayback === 2 && s.mediaLoopCount === 3)
                showMediaPlaybackMenu(kept.video, badges(kept.video, "playbackBadge")[0])
                return 300
            },
            () => {
                check("the count it has is in its menu", labels().includes("*Loop for Play Count: 3"), labels())
                click(menuRow("Loop for Time"))
                return 300
            },
            () => {
                click(menuRow("30 s"))
                return 600
            },
            () => {
                const s = document.slides[kept.video]
                check("going round for a length of time", s.mediaPlayback === 3 && s.mediaLoopSeconds === 30 && badges(kept.video, "playbackBadge")[0].words === "30 s", s.mediaPlayback + " " + s.mediaLoopSeconds)
                showMediaPlaybackMenu(kept.video, badges(kept.video, "playbackBadge")[0])
                return 300
            },
            () => {
                click(menuRow("Stop"))
                return 600
            },
            () => {
                const s = document.slides[kept.video]
                const icon = badges(kept.video, "playbackBadge")[0]
                check("and stopping at its end", s.mediaPlayback === 0 && !s.media.loops && Lib.find(icon, i => i.kind !== undefined).kind === "stop" && icon.words === "")
                // a click with the other button on an icon is still the slide's
                goLive(0)
                click(centre(icon))
                check("a plain click on an icon still shows the slide", liveIndex === kept.video)
                slideCell(kept.video)
                return 600
            },
            () => {
                testInput.grab("7-media-icons")
                const lines = testInput.readText(Log.path).split("\n")
                check("nothing went wrong on the way", lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme") && !l.includes("was not found in the workspace")
                                                                    && !l.includes("gone round eight times") && !l.includes("eglCreateImage") && !l.includes("failed to get textures")).length === 0,
                      lines.filter(l => /PROBLEM|WARNING|ERROR/.test(l) && !l.includes("qt.qpa.theme")).slice(0, 3).join(" | "))
            }
        ]
        next()
    }
}
